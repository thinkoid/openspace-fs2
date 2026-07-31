#include "fs2.hh"
#include "version.hh"

#include <stdlib.h>
#include <string.h>

#include <asteroid/asteroid.hh>
#include <bmpman/bmpman.hh>
#include <cfile/cfile.hh>
#include <cmeasure/cmeasure.hh>
#include <controlconfig/controlsconfig.hh>
#include <debris/debris.hh>
#include <fireball/fireballs.hh>
#include <freespace2/freespace.hh>
#include <gamesequence/gamesequence.hh>
#include <gamesnd/eventmusic.hh>
#include <gamesnd/gamesnd.hh>
#include <globalincs/linklist.hh>
#include <graphics/font.hh>
#include <graphics/grinternal.hh>
#include <hud/hudmessage.hh>
#include <hud/hud.hh>
#include <hud/hudtarget.hh>
#include <io/timer.hh>
#include <lighting/lighting.hh>
#include <localization/localize.hh>
#include <mission/missionbriefcommon.hh>
#include <mission/missiongoals.hh>
#include <mission/missionhotkey.hh>
#include <mission/missionlog.hh>
#include <mission/missionmessage.hh>
#include <mission/missionparse.hh>
#include <mission/missiontraining.hh>
#include <object/objcollide.hh>
#include <object/object.hh>
#include <object/objectsnd.hh>
#include <observer/observer.hh>
#include <particle/particle.hh>
#include <playerman/player.hh>
#include <radar/radar.hh>
#include <ship/afterburner.hh>
#include <ship/ai.hh>
#include <ship/awacs.hh>
#include <ship/ship.hh>
#include <ship/shipfx.hh>
#include <ship/shiphit.hh>
#include <sound/sound.hh>
#include <starfield/supernova.hh>
#include <stats/medals.hh>
#include <stats/scoring.hh>
#include <weapon/beam.hh>
#include <weapon/emp.hh>
#include <weapon/flak.hh>
#include <weapon/muzzleflash.hh>
#include <weapon/shockwave.hh>
#include <weapon/trails.hh>
#include <weapon/weapon.hh>

// missiontraining.cc file-scope state the lesson exposure reads: retail
// never exported these because only its own display consumed them; the
// boundary is a second display
extern char Training_text[];
extern int Training_msg_timestamp;
extern int Training_obj_lines[];
extern int Training_obj_num_lines;
void message_training_que_check();
#define TRAINING_OBJ_LINES_KEY (1 << 30)   // missiontraining.cc:62

// controlsconfigcommon.cc's key-text resolver (sexp_key_pressed's own)
int translate_key_to_index(char *key);

// the page-in residents levelpaging.cc consumes by local extern (retail
// never put them in headers): the sim needs their MODEL halves
void weapons_page_in();
void debris_page_in();

// The sound recorder (the sounds-as-events seam): with sound disabled,
// snd_play/snd_play_3d forward what WOULD have played through
// Snd_capture; events() drains this. Bounded -- a saturated frame drops
// the excess rather than growing without limit.
struct sound_req_t {
    char name[32];
    bool has_pos;
    vector pos;
};

static std::vector<sound_req_t> captured_sounds;

static void
capture_sound(const char *name, const vector *pos)
{
    if (captured_sounds.size() >= 64)
        return;

    sound_req_t req;
    memset(&req, 0, sizeof(req));
    strncpy(req.name, name, sizeof(req.name) - 1);
    if (pos) {
        req.has_pos = true;
        req.pos = *pos;
    }
    captured_sounds.push_back(req);
}

// The chatter recorder (missionmessage.cc's Msg_capture seam): sender,
// translated text and voice wave at the moment a message commits to the
// player's screen. Same bound-and-drop discipline as the sound ring.
struct message_req_t {
    char who[32];
    char text[512];
    char wave[32];
};

static std::vector<message_req_t> captured_messages;

static void
capture_message(const char *who, const char *text, const char *wave)
{
    if (captured_messages.size() >= 16)
        return;

    message_req_t req;
    memset(&req, 0, sizeof(req));
    strncpy(req.who, who, sizeof(req.who) - 1);
    strncpy(req.text, text, sizeof(req.text) - 1);
    strncpy(req.wave, wave, sizeof(req.wave) - 1);
    captured_messages.push_back(req);
}

// radar.cc's blip-list reset -- file-scope in retail, but its callers are
// split: radar_frame_init (the caller game_frame uses) needs fonts, while
// the reset alone is the sim half (ship_process_post PLOTS blips every
// frame; without the reset the lists overflow -- retail's own "else
// buffers can overflow" warning)
void radar_null_nblips();

const char *
fs2_t::version() const
{
    return LIBFS2_VERSION;
}

// ----------------------------------------------------------------------
// the mission world (slice 2)

// gr_screen color stores, the mission2tres recipe: table parsing pokes
// two function pointers; everything else graphical stays untouched
// deviceless, but not lossless: the rgb payload is sim-visible truth
// (weapon_get_laser_color cycles a laser's color through here, and the
// bolt art crosses the boundary) -- record it exactly as grx_init_color
// would; only the device bookkeeping stays dead
static void
null_init_color(color *clr, int r, int g, int b)
{
    clr->red = (ubyte)r;
    clr->green = (ubyte)g;
    clr->blue = (ubyte)b;
    clr->alpha = 255;
}

static void
null_init_alphacolor(color *clr, int r, int g, int b, int alpha, int)
{
    clr->red = (ubyte)r;
    clr->green = (ubyte)g;
    clr->blue = (ubyte)b;
    clr->alpha = (ubyte)alpha;
}

static void
null_zbuffer_clear(int)
{
}

// Boot: cfile at the install root + the table chain, once per process --
// the proven tool recipe (mission2tres/shiptbl2tres), game order
// (sounds.tbl before weapon_init so sound indices resolve).
static bool
boot(const char *game_root)
{
    static bool booted = false;
    static bool boot_ok = false;

    if (booted)
        return boot_ok;
    booted = true;

    char exe_path[CF_MAX_PATHNAME_LENGTH];
    snprintf(exe_path, sizeof(exe_path), "%s/x", game_root);
    if (cfile_init(exe_path))
        return false;

    // retail Int3 is a continuable breakpoint; t=0 bulk ship creation
    // trips the ship_make_create_time_unique diagnostic exactly as FRED's
    // bulk paths do (see debug.cc)
    setenv("FS2_INT3_CONTINUE", "1", 1);

    // no audio device: every snd_ call no-ops behind retail's own switch
    // -- and the no-op path reports to the recorder
    Sound_enabled = 0;
    Snd_capture = capture_sound;
    Msg_capture = capture_message;

    gr_screen.gf_init_color = null_init_color;
    gr_screen.gf_init_alphacolor = null_init_alphacolor;
    gr_screen.gf_zbuffer_clear = null_zbuffer_clear;
    gr_screen.gf_set_font = grx_set_font;   // retail's own; fonts are DATA

    // a described 640x480 canvas nothing ever draws to: the sim's own
    // paths read it -- fireball_get_lod projects the explosion through g3
    // to pick a detail level (ship_dying_frame, render machinery inside
    // the death sequence)
    gr_screen.max_w = 640;
    gr_screen.max_h = 480;
    gr_screen.aspect = 1.0f;
    gr_screen.clip_left = 0;
    gr_screen.clip_top = 0;
    gr_screen.clip_right = 639;
    gr_screen.clip_bottom = 479;
    gr_screen.clip_width = 640;
    gr_screen.clip_height = 480;

    // the 1555 ARGB color guns, grsoft.cc:858's own aux-path block: the
    // game-path table parse locks weapon bitmaps at 16bpp
    // (bm_set_components divides by the gun scales), so a headless boot
    // must describe the pixel format even though nothing ever renders
    Gr_red.bits = 5;
    Gr_red.shift = 10;
    Gr_red.scale = 256 / 32;
    Gr_red.mask = 0x7C00;
    Gr_green.bits = 5;
    Gr_green.shift = 5;
    Gr_green.scale = 256 / 32;
    Gr_green.mask = 0x03e0;
    Gr_blue.bits = 5;
    Gr_blue.shift = 0;
    Gr_blue.scale = 256 / 32;
    Gr_blue.mask = 0x1F;
    Gr_alpha.bits = 1;
    Gr_alpha.shift = 15;
    Gr_alpha.scale = 255;
    Gr_alpha.mask = 0x8000;
    Gr_t_red = Gr_red;
    Gr_t_green = Gr_green;
    Gr_t_blue = Gr_blue;
    Gr_t_alpha = Gr_alpha;
    Gr_current_red = &Gr_red;
    Gr_current_green = &Gr_green;
    Gr_current_blue = &Gr_blue;
    Gr_current_alpha = &Gr_alpha;

    timer_init();
    lcl_init(LCL_ENGLISH);
    // the .vf fonts load through cfile -- pure data, and every
    // font-metric path (message wrapping, HUD text sizing) becomes real
    gr_font_init();
    gamesnd_parse_soundstbl();
    parse_medal_tbl();     // mission parse resolves medal names (the loop
                           // missions' SOC promotions) against Medals[]
    asteroid_init();       // asteroid.tbl; asteroid_create_all loads the
                           // field missions' POFs from Asteroid_info
    mflash_game_init();    // mflash.tbl BEFORE weapon_init: the weapon
                           // parse looks its $Muzzleflash: up here, and a
                           // failed lookup drops WIF_MFLASH -- which flak
                           // turret fire asserts (flak.cc:219)
    ai_init();
    weapon_init();
    ship_init();

    // the pilot: enough of Players[0] for the sim chain (scoring, ci,
    // control mode); pilot files and ship select stay out of the library
    Player_num = 0;
    Player = &Players[0];
    memset(Player, 0, sizeof(player));
    strcpy(Player->callsign, "libfs2");
    Player->control_mode = PCM_NORMAL;

    // the zeroed player carries dead list heads; retail's own data-side
    // initializer (HUD_init calls it) wires keyed_targets + the free list
    hud_keyed_targets_clear();

    // the retail sequencer, entered once: the stubs' game_process_event
    // sends every event to GS_STATE_GAME_PLAY -- the only state this
    // library is ever in; sim code (message_queue_process) reads it
    gameseq_init();
    gameseq_post_event(GS_EVENT_ENTER_GAME);
    gameseq_process_events();

    boot_ok = true;
    return true;
}

bool
fs2_t::load(const char *game_root, const char *mission, int seed)
{
    if (!boot(game_root))
        return false;

    // --- game_level_init (freespace.cc:872), the sim subset, same order.
    // Presentation-only entries stay out: game_flash_reset, shield_hit
    // decals, radar blip art. Everything else is world state.
    //
    // GM_NORMAL before the chain, retail's own precondition: the game
    // carries it from startup, and level-load code branches on it --
    // asteroid_create reads pos and angs ONLY inside `if (GM_NORMAL)`
    // and builds the orientation from stack garbage otherwise (the
    // multiplayer arm; the server used to overwrite it). A whole field
    // of garbage-oriented rocks flakes the physics assert.
    Game_mode = GM_NORMAL;
    srand(seed);
    Framecount = 0;

    obj_init();
    model_free_all();
    mission_brief_common_init();
    weapon_level_init();
    ai_level_init();
    ship_level_init();
    player_level_init();
    shipfx_flash_init();
    particle_init();
    fireball_init();
    debris_init();
    cmeasure_init();
    radar_mission_init();
    mission_init_goals();
    mission_log_init();
    messages_init();
    // the scrollback store (HUD_init calls it in retail's chain): the
    // training path writes every message into it and asserts it exists
    hud_init_msg_window();
    obj_snd_level_init();
    shockwave_level_init();
    afterburner_level_init();
    scoring_level_init(&Player->stats);
    asteroid_level_init();
    control_config_clear_used_status();
    Missiontime = 0;
    m_pre_entry = true;
    Entry_delay_time = 0;
    observer_init();
    flak_level_init();
    awacs_level_init();
    beam_level_init();
    mflash_level_init();
    supernova_level_init();
    shipfx_engine_wash_level_init();

    // --- mission_load (missionload.cc:99), minus pilot-file bookkeeping
    strncpy(Game_current_mission_filename, mission, MAX_FILENAME_LEN - 1);
    if (parse_main(const_cast<char *>(mission)) != 0)
        return false;

    // --- game_post_level_init (freespace.cc:991), the sim subset --
    // HUD_init/stars/neb2 are presentation
    model_level_post_init();
    mission_hotkey_set_defaults();
    training_mission_init();
    asteroid_create_all();

    // --- freespace_mission_load_stuff: the sim-essential calls --
    // created-list merge + player-ship AI fixup, and the weapon MODELS
    // (level_page_in's cargo we actually need: an AI missile launch
    // reaches model_get_radius on the missile's POF)
    mission_parse_fixup_players();
    weapons_page_in();
    debris_page_in();      // debris01/02.pof -- subsystem hits shed
                           // debris_create'd pieces of it

    Game_mode |= GM_IN_MISSION;

    m_world_live = true;
    m_burn_held = false;
    m_log_drained = 0;
    m_known.clear();

    return true;
}

// one object's snapshot record
static object_state_t
record_of(object *objp)
{
    object_state_t rec;
    memset(&rec, 0, sizeof(rec));

    rec.signature = objp->signature;
    rec.objnum = OBJ_INDEX(objp);
    rec.type = objp->type;
    rec.radius = objp->radius;
    rec.pos = objp->pos;
    rec.orient = objp->orient;
    rec.vel = objp->phys_info.vel;

    if (objp->type == OBJ_WEAPON) {
        weapon_info *wip =
            &Weapon_info[Weapons[objp->instance].weapon_info_index];
        strncpy(rec.class_name, wip->name, sizeof(rec.class_name) - 1);

        // the bolt's art: lasers cross their tbl size and the current
        // cycle color; a POF-rendered weapon (missiles) crosses its model
        // instead and the presenter loads it like any hull
        rec.laser_length = wip->laser_length;
        rec.laser_head_radius = wip->laser_head_radius;
        if (wip->render_type == WRT_POF) {
            strncpy(rec.pof, wip->pofbitmap_name, sizeof(rec.pof) - 1);
        }
        else {
            color c;
            memset(&c, 0, sizeof(c));
            weapon_get_laser_color(&c, objp);
            rec.laser_rgb[0] = c.red;
            rec.laser_rgb[1] = c.green;
            rec.laser_rgb[2] = c.blue;

            // the bolt's art by name -- bmpman still knows what the tbl
            // loaded (@Laser Bitmap / @Laser Glow)
            if (wip->laser_bitmap >= 0)
                strncpy(rec.laser_bitmap, bm_get_filename(wip->laser_bitmap),
                        sizeof(rec.laser_bitmap) - 1);
            if (wip->laser_glow_bitmap >= 0)
                strncpy(rec.laser_glow,
                        bm_get_filename(wip->laser_glow_bitmap),
                        sizeof(rec.laser_glow) - 1);
        }
        return rec;
    }
    if (objp->type == OBJ_SHOCKWAVE) {
        // the blast front: the object's radius is pinned at the outer
        // ceiling from creation; the expanding front the presenter scales
        // to is the shockwave's own
        strncpy(rec.class_name, "shockwave", sizeof(rec.class_name) - 1);
        rec.radius = Shockwaves[objp->instance].radius;
        return rec;
    }
    if (objp->type == OBJ_FIREBALL) {
        // the one distinction presentation needs: an arrival's warp
        // effect is not an explosion (retail's own accessor) -- and the
        // pof slot carries the type's ani stem, the flipbook to play
        strncpy(rec.class_name, fireball_is_warp(objp) ? "warp" : "explosion",
                sizeof(rec.class_name) - 1);
        strncpy(rec.pof, fireball_art_name(objp), sizeof(rec.pof) - 1);
        return rec;
    }
    if (objp->type == OBJ_DEBRIS)
        return rec;

    ship *shipp = &Ships[objp->instance];
    ship_info *sip = &Ship_info[shipp->ship_info_index];

    strncpy(rec.name, shipp->ship_name, sizeof(rec.name) - 1);
    strncpy(rec.class_name, sip->name, sizeof(rec.class_name) - 1);
    strncpy(rec.pof, sip->pof_file, sizeof(rec.pof) - 1);
    rec.team = shipp->team;
    // a wing member's placement follows the WING's arrival location
    // (mission_set_wing_arrival_location repositions the wave); the
    // ship's own field stays at its default for those
    rec.arrival_location = shipp->wingnum >= 0
                               ? Wings[shipp->wingnum].arrival_location
                               : shipp->arrival_location;
    rec.player = (objp->flags & OF_PLAYER_SHIP) != 0;
    rec.dying = (shipp->flags & SF_DYING) != 0;
    rec.afterburner = (objp->phys_info.flags & PF_AFTERBURNER_ON) != 0;
    rec.hull = objp->hull_strength;
    rec.hull_max = sip->initial_hull_strength;
    rec.max_speed = objp->phys_info.max_vel.z;

    for (int i = 0; i < MAX_SHIELD_SECTIONS; i++)
        rec.shield[i] = objp->shields[i];
    rec.shield_max = sip->shields;

    rec.weapon_energy = shipp->weapon_energy;
    rec.weapon_energy_max = sip->max_weapon_reserve;
    rec.burner_fuel = shipp->afterburner_fuel;
    rec.burner_fuel_max = sip->afterburner_fuel_capacity;

    return rec;
}

void
fs2_t::step(float dt, const flight_controls_t &controls)
{
    if (!m_world_live)
        return;

    // --- the clock, virtual: dt is an input (the plan's rule), the
    // timestamp ticker and Missiontime advance from it and nothing else
    Frametime = fl2f(dt);
    flFrametime = dt;
    timestamp_inc(dt);
    Missiontime += Frametime;
    Framecount++;

    // --- game_frame's pre-simulation strip (freespace.cc:3290).
    // radar_frame_init wants fonts for blip glyph metrics; its sim half
    // is the blip-list reset alone (ship_process_post plots blips every
    // frame -- a render-free frame still fills the lists).
    // shield_frame_init stays out entirely: the shield-hit buffer fills
    // only from the damage/render side, and its reset is bitmap-adjacent.
    if (Missiontime > Entry_delay_time)
        m_pre_entry = false;

    radar_null_nblips();

    // the light pool resets per frame (game_frame does it just before the
    // simulation, deliberately outside the Pre_player_entry guard) --
    // weapon fire ADDS lights sim-side, and the pool overflows without it
    light_reset();

    // the virtual stick: retail's control_info filled from the boundary
    // in place of read_keyboard_controls; the rest of
    // read_player_controls' PCM_NORMAL path follows verbatim
    // (playercontrol.cc:824)
    if (!m_pre_entry && Player_obj->type == OBJ_SHIP) {
        memset(&Player->ci, 0, sizeof(control_info));
        Player->ci.pitch = controls.pitch;
        Player->ci.heading = controls.heading;
        Player->ci.bank = controls.bank;
        Player->ci.forward = controls.forward;

        // the triggers: obj_player_fire_stuff (called from obj_move_all's
        // player branch) fires primaries/secondaries/countermeasures off
        // these counts, trigger-down flag and stream cadence included
        Player->ci.fire_primary_count = controls.fire_primary ? 1 : 0;
        Player->ci.fire_secondary_count = controls.fire_secondary ? 1 : 0;
        Player->ci.fire_countermeasure_count =
            controls.fire_countermeasure ? 1 : 0;

        // afterburner engages through retail's own fuel accounting. The
        // stop MUST say key_released: afterburners_start latches
        // PF_AFTERBURNER_WAIT on every engage and only a key-released
        // stop clears it (afterburner.cc:154/301) -- without it the
        // burner lights exactly once per mission (field-reported).
        // KNOWN LEAK: the 1300 ms relight lockout inside
        // afterburners_start reads timer_get_milliseconds() -- WALL
        // time; a future burn-replaying gate will see nondeterminism
        // across differently-paced runs.
        if (controls.afterburner && !m_burn_held)
            afterburners_start(Player_obj);
        else if (!controls.afterburner && m_burn_held)
            afterburners_stop(Player_obj, 1);
        m_burn_held = controls.afterburner;

        object *objp = Player_obj;
        objp->phys_info.max_vel.z = Ships[objp->instance].current_max_speed;
        if (!(Ships[objp->instance].flags & SF_DYING)) {
            if (Ships[objp->instance].wash_intensity > 0) {
                vector wash_rot;
                float intensity =
                    0.3f * min(Ships[objp->instance].wash_intensity, 1.0f);
                vm_vec_copy_scale(&wash_rot,
                                  &Ships[objp->instance].wash_rot_axis,
                                  intensity);
                physics_read_flying_controls(&objp->orient, &objp->phys_info,
                                             &Player->ci, flFrametime,
                                             &wash_rot);
            }
            else {
                physics_read_flying_controls(&objp->orient, &objp->phys_info,
                                             &Player->ci, flFrametime);
            }
        }
    }

    // --- game_simulation_frame (freespace.cc:3028), the sim subset --
    // out: viewer/padlock camera state, HUD updates, object sounds,
    // listener placement (presentation); in: everything that moves the
    // world
    awacs_process();

    Player->damage_this_burst -= (flFrametime * MAX_BURST_DAMAGE /
                                  (0.001f * BURST_DURATION));
    Player->damage_this_burst = max(Player->damage_this_burst, 0.0f);

    supernova_process();
    if (supernova_active() >= 5)
        return;

    ship_process_targeting_lasers();

    // the target-next action, retail's own cycler (T in keycontrol)
    if (controls.target_next && !m_target_held && !m_pre_entry)
        hud_target_next();
    m_target_held = controls.target_next;

    mission_parse_eval_stuff();        // arrivals and departures, live
    obj_move_all(flFrametime);
    mission_eval_goals();
    training_check_objectives();
    // the training sexps' contexts: `speed` (hold a speed band),
    // fly-path waypoint progress, `targeted`'s held-for timestamp --
    // without this, Training-1's "fly at max speed" event never fires
    game_do_training_checks();
    // the training-message queue promotes on the DISPLAY path in retail
    // (message_training_display calls it); a headless frame promotes here
    message_training_que_check();

    if (!m_pre_entry)
        message_queue_process();
    message_maybe_distort();

    // player_repair_frame stays behind: it lives in freespace.cc (not the
    // foundation) and only matters once the player can call a support
    // ship -- a later slice replicates it beside the rearm flow
    player_process_pending_praise();

    emp_process_local();

    particle_move_all(flFrametime);
    trail_move_all(flFrametime);
    mflash_process_all();
    shipfx_flash_do_frame(flFrametime);
    shockwave_move_all(flFrametime);
}

std::vector<object_state_t>
fs2_t::snapshot() const
{
    std::vector<object_state_t> out;

    if (!m_world_live)
        return out;

    for (object *objp = GET_FIRST(&obj_used_list);
         objp != END_OF_LIST(&obj_used_list); objp = GET_NEXT(objp)) {
        if (objp->type != OBJ_SHIP && objp->type != OBJ_START &&
            objp->type != OBJ_WEAPON && objp->type != OBJ_FIREBALL &&
            objp->type != OBJ_DEBRIS && objp->type != OBJ_SHOCKWAVE)
            continue;
        out.push_back(record_of(objp));
    }

    return out;
}

void
fs2_t::key_mark(const char *key_text)
{
    int z = translate_key_to_index(const_cast<char *>(key_text));
    if (z >= 0)
        control_used(z);
}

hud_state_t
fs2_t::hud_state() const
{
    hud_state_t out;
    out.training_text[0] = '\0';
    out.training_voice[0] = '\0';
    out.primary_speed = 0.0f;
    out.target_signature = -1;

    if (!m_world_live)
        return out;

    // the selected primary's muzzle speed -- with the target's motion
    // (both already in the snapshot) it completes the lead solution
    if (Player_obj && Player_obj->type == OBJ_SHIP) {
        const ship_weapon &w = Ships[Player_obj->instance].weapons;
        if (w.current_primary_bank >= 0 &&
            w.current_primary_bank < w.num_primary_banks) {
            int wi = w.primary_bank_weapons[w.current_primary_bank];
            if (wi >= 0)
                out.primary_speed = Weapon_info[wi].max_speed;
        }
    }

    // the player's target, as retail's own targeting state has it --
    // signature, not objnum, so the presenter keys into its snapshot map;
    // a freed slot (target died, world moved on) reads as no target
    if (Player_ai && Player_ai->target_objnum >= 0) {
        const object *t = &Objects[Player_ai->target_objnum];
        if (t->type != OBJ_NONE)
            out.target_signature = t->signature;
    }

    // the training message, gated exactly as the display gates it
    // (missiontraining.cc:886): inside its timing window and non-empty
    if (!timestamp_elapsed(Training_msg_timestamp) &&
        strlen(Training_text) > 0) {
        message_translate_tokens(out.training_text, Training_text);

        // the wave name: setup keeps only the text, so find the message
        // back the way the queue found it -- by its own content
        for (int m = 0; m < Num_messages; m++) {
            if (strcmp(Training_text, Messages[m].message) != 0)
                continue;
            int w = Messages[m].wave_info.index;
            if (w >= 0)
                strncpy(out.training_voice, Message_waves[w].name,
                        sizeof(out.training_voice) - 1);
            break;
        }
    }

    // the directives, decoded as training_obj_display draws them
    for (int i = 0; i < Training_obj_num_lines; i++) {
        int z = Training_obj_lines[i] & 0xffff;

        directive_t d;
        memset(&d, 0, sizeof(d));

        if (Training_obj_lines[i] & TRAINING_OBJ_LINES_KEY) {
            d.key_line = true;
            message_translate_tokens(d.text,
                                     Mission_events[z].objective_key_text);
        }
        else {
            strncpy(d.text, Mission_events[z].objective_text,
                    sizeof(d.text) - 8);
            if (Mission_events[z].count)
                sprintf(d.text + strlen(d.text), NOX(" [%d]"),
                        Mission_events[z].count);
            d.state = mission_get_event_status(z);
        }

        out.directives.push_back(d);
    }

    return out;
}

std::vector<event_t>
fs2_t::events()
{
    std::vector<event_t> out;

    if (!m_world_live)
        return out;

    // the frame's sound requests, oldest first
    for (const sound_req_t &req : captured_sounds) {
        event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.kind = event_t::sound;
        strncpy(ev.name, req.name, sizeof(ev.name) - 1);
        ev.has_pos = req.has_pos;
        ev.pos = req.pos;
        out.push_back(ev);
    }
    captured_sounds.clear();

    // the frame's chatter, oldest first
    for (const message_req_t &req : captured_messages) {
        event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.kind = event_t::message;
        strncpy(ev.pname, req.who, sizeof(ev.pname) - 1);
        strncpy(ev.text, req.text, sizeof(ev.text) - 1);
        strncpy(ev.name, req.wave, sizeof(ev.name) - 1);
        out.push_back(ev);
    }
    captured_messages.clear();

    // new mission-log entries since the last drain -- retail's own record
    for (; m_log_drained < last_entry; m_log_drained++) {
        const log_entry &e = log_entries[m_log_drained];

        event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.kind = event_t::log;
        ev.log_type = e.type;
        strncpy(ev.pname, e.pname, sizeof(ev.pname) - 1);
        strncpy(ev.sname, e.sname, sizeof(ev.sname) - 1);
        ev.time = e.timestamp;
        out.push_back(ev);
    }

    // world membership diff against the last drain
    std::vector<object_state_t> now = snapshot();

    for (const object_state_t &obj : now) {
        bool known = false;
        for (const object_state_t &old : m_known)
            if (old.signature == obj.signature)
                known = true;
        if (!known) {
            event_t ev;
            memset(&ev, 0, sizeof(ev));
            ev.kind = event_t::created;
            ev.signature = obj.signature;
            strncpy(ev.name, obj.name, sizeof(ev.name) - 1);
            out.push_back(ev);
        }
    }

    for (const object_state_t &old : m_known) {
        bool alive = false;
        for (const object_state_t &obj : now)
            if (obj.signature == old.signature)
                alive = true;
        if (!alive) {
            event_t ev;
            memset(&ev, 0, sizeof(ev));
            ev.kind = event_t::destroyed;
            ev.signature = old.signature;
            strncpy(ev.name, old.name, sizeof(ev.name) - 1);
            out.push_back(ev);
        }
    }

    m_known = now;
    return out;
}

fs2_t::fs2_t()
{
    physics_init(&m_pi);
    m_pi.flags = PF_ACCELERATES;
}

void
fs2_t::fly_reset(const flight_params_t &params)
{
    physics_init(&m_pi);

    // the physics_dump recipe, verbatim -- PF_ACCELERATES only, the flag
    // pair the oracle pins; PF_AFTERBURNER_ON comes and goes per step
    m_pi.flags = PF_ACCELERATES;

    m_pi.mass = params.mass;
    m_pi.max_vel = params.max_vel;
    m_pi.max_rear_vel = params.max_rear_vel;
    m_pi.max_rotvel = params.max_rotvel;

    m_pi.forward_accel_time_const = params.forward_accel_time_const;
    m_pi.forward_decel_time_const = params.forward_decel_time_const;
    m_pi.slide_accel_time_const = 0.0f;
    m_pi.slide_decel_time_const = 0.0f;
    m_pi.side_slip_time_const = params.side_slip_time_const;
    m_pi.rotdamp = params.rotdamp;

    m_pi.afterburner_max_vel = params.afterburner_max_vel;
    m_pi.afterburner_forward_accel_time_const =
        params.afterburner_forward_accel_time_const;

    m_pos = vmd_zero_vector;
    m_orient = vmd_identity_matrix;

    m_has_afterburner = params.afterburner_max_vel.z > 0.0f;
}

void
fs2_t::fly_step(float dt, const flight_controls_t &controls)
{
    control_info ci;
    memset(&ci, 0, sizeof(ci));

    ci.pitch = controls.pitch;
    ci.heading = controls.heading;
    ci.bank = controls.bank;
    ci.forward = controls.forward;

    // the engage policy lives here; the burner's flight behavior (stick
    // floor, goal swap, accel-const swap) is retail's own physics.cc,
    // reading the flag
    if (controls.afterburner && m_has_afterburner)
        m_pi.flags |= PF_AFTERBURNER_ON;
    else
        m_pi.flags &= ~PF_AFTERBURNER_ON;

    physics_read_flying_controls(&m_orient, &m_pi, &ci, dt, NULL);
    physics_sim(&m_pos, &m_orient, &m_pi, dt);
}

flight_state_t
fs2_t::fly_state() const
{
    flight_state_t state;

    state.pos = m_pos;
    state.orient = m_orient;
    state.vel = m_pi.vel;
    state.rotvel = m_pi.rotvel;
    state.speed = m_pi.speed;
    state.fspeed = m_pi.fspeed;

    return state;
}
