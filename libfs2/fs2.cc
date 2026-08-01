#include "fs2.hh"
#include "version.hh"

#include <stdlib.h>
#include <string.h>

#include <filesystem>

#include <anim/animplay.hh>
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
#include <hud/hudescort.hh>
#include <hud/hudmessage.hh>
#include <hud/hudshield.hh>
#include <hud/hud.hh>
#include <hud/hudtarget.hh>
#include <io/timer.hh>
#include <lighting/lighting.hh>
#include <localization/localize.hh>
#include <mission/missionbriefcommon.hh>
#include <mission/missioncampaign.hh>
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
#include <osapi/osregistry.hh>
#include <parse/sexp.hh>
#include <particle/particle.hh>
#include <playerman/managepilot.hh>
#include <playerman/player.hh>
#include <radar/radar.hh>
#include <ship/afterburner.hh>
#include <ship/ai.hh>
#include <ship/awacs.hh>
#include <ship/ship.hh>
#include <ship/shipfx.hh>
#include <ship/shiphit.hh>
#include <sound/sound.hh>
#include <starfield/starfield.hh>
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

// hudshield.cc's icon table (ships.tbl $Shield_icon entries) has no
// header declaration of its own
#define MAX_SHIELD_ICONS 40
extern char Hud_shield_filenames[MAX_SHIELD_ICONS][MAX_FILENAME_LEN];
extern int Hud_shield_filename_count;
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

    // Persistence is a Linux affair: cfile root 0 -- the write root, and
    // the head of the read search -- is the XDG data home
    // ($XDG_DATA_HOME/fs2, default ~/.local/share/fs2), where pilots and
    // campaign saves live. The retail tree rides behind it in the
    // engine's old CD-ROM slot, so reads fall through to retail data and
    // anything dropped in the data home shadows it. Both buffers are
    // static: cfile keeps the cdrom pointer (cfile_refresh rereads it).
    static char data_home[CF_MAX_PATHNAME_LENGTH];
    static char retail_root[CF_MAX_PATHNAME_LENGTH];

    const char *xdg = getenv("XDG_DATA_HOME");
    if (xdg && xdg[0])
        snprintf(data_home, sizeof(data_home), "%s/fs2", xdg);
    else
        snprintf(data_home, sizeof(data_home), "%s/.local/share/fs2",
                 getenv("HOME") ? getenv("HOME") : ".");

    // every cfile path is root + type dir + filename (+ localization)
    // inside MAX_PATH_LEN, built by strcpy/strcat with no bounds -- keep
    // enough headroom that no combination can reach the ceiling
    if (strlen(data_home) > MAX_PATH_LEN - 96) {
        fprintf(stderr, "fs2: data home path too long: %s\n", data_home);
        return false;
    }

    // the subtree the savefile writers expect; cfile never mkdirs
    std::error_code ec;
    std::filesystem::create_directories(
        std::string(data_home) + "/data/players/single", ec);
    if (ec)
        return false;

    char exe_path[CF_MAX_PATHNAME_LENGTH + 4];
    snprintf(exe_path, sizeof(exe_path), "%s/x", data_home);
    snprintf(retail_root, sizeof(retail_root), "%s/", game_root);
    if (cfile_init(exe_path, retail_root))
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
    stars_init();          // stars.tbl: the sun/backdrop CATALOG -- the
                           // mission's #Background instances resolve
                           // against it (sun light RGBI, blend mode)
    hud_shield_game_init(); // hud.tbl: the shield-icon catalog, BEFORE
                           // ship_init -- $Shield_icon resolves against
                           // it or stays 255 (and the record crosses "")
    mflash_game_init();    // mflash.tbl BEFORE weapon_init: the weapon
                           // parse looks its $Muzzleflash: up here, and a
                           // failed lookup drops WIF_MFLASH -- which flak
                           // turret fire asserts (flak.cc:219)
    ai_init();
    weapon_init();
    ship_init();

    // the default key bindings, live: message_translate_tokens turns a
    // training message's "$t$" into the CURRENT binding's text
    // (translate_key reads Control_config[].key_id), and without this
    // key_id is zero -- which textifies to an empty string and eats the
    // key out of "Press $t$ to..." (field-reported)
    control_config_reset_defaults();

    // the anim render-instance free list (retail wires it in game_init):
    // the pilot's default HUD config turns every gauge on, and an active
    // talking-head gauge sends message_play_anim through anim_play --
    // which walks these lists headless
    anim_init();

    // the config shim behind os_config_read_*: without this
    // init_new_pilot's detail/skill reads return 0, not their defaults
    os_init_registry_stuff(NULL, NULL, NULL);

    // The Pilot. His name is Commander Jameson (there is exactly one,
    // and Elite named him first): resume him from the data home if he
    // has flown before, induct him fresh otherwise. Induction writes
    // nothing -- the .plr appears at the first campaign save.
    Player_num = 0;
    Player = &Players[0];
    memset(Player, 0, sizeof(player));

    // the zeroed player carries dead list heads; retail's own data-side
    // initializer (HUD_init calls it) wires keyed_targets + the free list
    hud_keyed_targets_clear();

    strcpy(Player->callsign, "Commander Jameson");
    if (read_pilot_file(Player->callsign, 1, Player) != 0)
        init_new_pilot(Player);
    Player->control_mode = PCM_NORMAL;

    // retail's startup mode, carried from game_init: campaign savefile
    // code asserts it before any mission load (missioncampaign.cc:755)
    Game_mode = GM_NORMAL;

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
    Game_mode = GM_NORMAL | (m_campaign ? GM_CAMPAIGN_MODE : 0);
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
    if (objp->type == OBJ_DEBRIS) {
        // the piece's identity: its source model and WHICH submodel --
        // the presenter shows exactly that chunk (hull debris wears the
        // ship's own textures that way)
        const debris *db = &Debris[objp->instance];
        if (db->model_num >= 0) {
            polymodel *pm = model_get(db->model_num);
            if (pm) {
                strncpy(rec.pof, pm->filename, sizeof(rec.pof) - 1);
                if (db->submodel_num >= 0 && db->submodel_num < pm->n_models)
                    strncpy(rec.piece, pm->submodel[db->submodel_num].name,
                            sizeof(rec.piece) - 1);
            }
        }
        return rec;
    }

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
    rec.species = sip->species;
    if (sip->shield_icon_index != 255 &&
        sip->shield_icon_index < Hud_shield_filename_count)
        strncpy(rec.shield_icon,
                Hud_shield_filenames[sip->shield_icon_index],
                sizeof(rec.shield_icon) - 1);

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

    // the H binding, keycontrol.cc:1986's own sequence: mark the
    // control used (sexp key-pressed reads it) and take the next
    // closest hostile, sensors permitting
    if (controls.target_hostile && !m_hostile_held && !m_pre_entry) {
        control_used(TARGET_NEXT_CLOSEST_HOSTILE);
        if (hud_sensors_ok(Player_ship))
            hud_target_next_list();
    }
    m_hostile_held = controls.target_hostile;

    // the E binding, same retail sequence: next ship on the escort list
    // (empty list = retail's own no-op)
    if (controls.target_escort && !m_escort_held && !m_pre_entry) {
        control_used(TARGET_NEXT_ESCORT_SHIP);
        hud_escort_target_next();
    }
    m_escort_held = controls.target_escort;

    // the S binding: next subsystem on the current target
    if (controls.target_subsys && !m_subsys_held && !m_pre_entry) {
        control_used(TARGET_NEXT_SUBOBJECT);
        if (hud_sensors_ok(Player_ship))
            hud_target_next_subobject();
    }
    m_subsys_held = controls.target_subsys;

    // the "." binding (keycontrol.cc's CYCLE_NEXT_PRIMARY body): bank 1
    // -> bank 2 -> linked, ship_select_next_primary's own cycle, with
    // retail's quarter-second ready-again delay on the new bank
    if (controls.cycle_primary && !m_cycle_p_held && !m_pre_entry &&
        Player_obj && Player_ship) {
        control_used(CYCLE_NEXT_PRIMARY);
        if (ship_select_next_primary(Player_obj, CYCLE_PRIMARY_NEXT))
            Player_ship->weapons.next_primary_fire_stamp
                [Player_ship->weapons.current_primary_bank] = timestamp(250);
    }
    m_cycle_p_held = controls.cycle_primary;

    // the "/" binding (CYCLE_SECONDARY's body): the next mounted
    // missile bank, same ready-again delay
    if (controls.cycle_secondary && !m_cycle_s_held && !m_pre_entry &&
        Player_obj) {
        control_used(CYCLE_SECONDARY);
        if (ship_select_next_secondary(Player_obj)) {
            ship_weapon &w = Ships[Player_obj->instance].weapons;
            if (timestamp_elapsed(
                    w.next_secondary_fire_stamp[w.current_secondary_bank]))
                w.next_secondary_fire_stamp[w.current_secondary_bank] =
                    timestamp(250);
        }
    }
    m_cycle_s_held = controls.cycle_secondary;

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

// the hot crossing, packed: the same object walk as snapshot(), only
// the fields that change frame to frame -- no strings, no per-record
// structures. The presenter joins rows to birth identity by sig; the
// created event carries the full record once.
frame_t
fs2_t::frame() const
{
    frame_t out;

    if (!m_world_live)
        return out;

    for (object *objp = GET_FIRST(&obj_used_list);
         objp != END_OF_LIST(&obj_used_list); objp = GET_NEXT(objp)) {
        if (objp->type != OBJ_SHIP && objp->type != OBJ_START &&
            objp->type != OBJ_WEAPON && objp->type != OBJ_FIREBALL &&
            objp->type != OBJ_DEBRIS && objp->type != OBJ_SHOCKWAVE)
            continue;

        out.sig.push_back(objp->signature);
        out.pos.push_back(objp->pos);
        out.rvec.push_back(objp->orient.rvec);
        out.uvec.push_back(objp->orient.uvec);
        out.fvec.push_back(objp->orient.fvec);
        out.vel.push_back(objp->phys_info.vel);
        out.hull.push_back(objp->hull_strength);

        // the live radius: a shockwave's is the expanding blast front
        out.radius.push_back(objp->type == OBJ_SHOCKWAVE
                                 ? Shockwaves[objp->instance].radius
                                 : objp->radius);

        int flags = 0;
        float sh[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        unsigned char rgb[3] = { 0, 0, 0 };

        if (objp->type == OBJ_SHIP || objp->type == OBJ_START) {
            if (Ships[objp->instance].flags & SF_DYING)
                flags |= 1;
            if (objp->phys_info.flags & PF_AFTERBURNER_ON)
                flags |= 2;
            if (objp->flags & OF_PLAYER_SHIP)
                flags |= 4;
            for (int i = 0; i < MAX_SHIELD_SECTIONS; i++)
                sh[i] = objp->shields[i];
        }
        else if (objp->type == OBJ_WEAPON) {
            weapon_info *wip =
                &Weapon_info[Weapons[objp->instance].weapon_info_index];
            if (wip->render_type != WRT_POF) {
                color c;
                memset(&c, 0, sizeof(c));
                weapon_get_laser_color(&c, objp);
                rgb[0] = c.red;
                rgb[1] = c.green;
                rgb[2] = c.blue;
            }
        }

        out.flags.push_back(flags);
        out.shield.insert(out.shield.end(), sh, sh + 4);
        out.rgb.insert(out.rgb.end(), rgb, rgb + 3);
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

// ----------------------------------------------------------------------
// the campaign (slice 3): retail's own missioncampaign machine behind
// the boundary. The .fc2 parse, the .csg resume, the branch formulas and
// the savefile writes are all missioncampaign.cc's -- the boundary only
// sequences them the way freespace.cc's debrief path does, and carries
// the verdict across as values.

bool
fs2_t::load_campaign(const char *game_root, const char *name)
{
    if (!boot(game_root))
        return false;

    // .fc2 parse + the pilot's .csg resume (savefile_load is inside);
    // nonzero = the campaign file is missing or unreadable
    if (mission_campaign_load(name))
        return false;

    // the readyroom epilogue: campaign mode on (load() re-applies it to
    // Game_mode each mission), the pilot bookmarked, and the resume
    // point promoted to the current mission (-1 = already complete)
    m_campaign = true;
    m_campaign_over = false;
    strcpy(Player->current_campaign, Campaign.filename);
    if (mission_campaign_next_mission() != 0)
        m_campaign_over = true;

    return true;
}

const char *
fs2_t::current_mission() const
{
    if (!m_campaign || m_campaign_over || Campaign.current_mission < 0)
        return "";

    return Campaign.missions[Campaign.current_mission].name;
}

debrief_t
fs2_t::debrief()
{
    debrief_t out;
    out.next_mission[0] = '\0';
    out.loop_offer = false;

    // retail's debrief entry, in retail's order (freespace.cc:4527 +
    // debrief_init's campaign block): fail what never completed, store
    // the goal/event record, THEN evaluate the branch -- the campaign
    // formulas read the stored record
    mission_goal_fail_incomplete();

    if (m_campaign) {
        mission_campaign_store_goals_and_events();
        mission_campaign_eval_next_mission();
    }

    // accept the level stats into the pilot (debrief_init's own call --
    // rank/medal progression), before the stage formulas can read them
    scoring_level_close();

    for (int i = 0; i < Num_goals; ++i) {
        goal_state_t g;
        memset(&g, 0, sizeof(g));

        strncpy(g.name, Mission_goals[i].name, sizeof(g.name) - 1);
        strncpy(g.text, Mission_goals[i].message, sizeof(g.text) - 1);
        g.type = Mission_goals[i].type & GOAL_TYPE_MASK;
        g.status = Mission_goals[i].satisfied;
        g.invalid = (Mission_goals[i].type & INVALID_GOAL) != 0;

        out.goals.push_back(g);
    }

    // the stage selection -- debrief_set_stages' formula loop, minus the
    // presentation-fed stages (promotion, badge, the traitor variant).
    // debrief_init's own team-0 aim: the parse fills Debriefings[0] and
    // leaves the pointer NULL until the debrief screen wants it
    Debriefing = &Debriefings[0];

    for (int i = 0; i < Debriefing->num_stages; ++i) {
        debrief_stage &st = Debriefing->stages[i];

        if (eval_sexp(st.formula) != 1)
            continue;

        debrief_stage_t s;
        s.text = st.new_text ? st.new_text : "";
        s.recommendation =
            st.new_recommendation_text ? st.new_recommendation_text : "";
        memset(s.voice, 0, sizeof(s.voice));
        strncpy(s.voice, st.voice, sizeof(s.voice) - 1);

        out.stages.push_back(s);
    }

    // the campaign verdict: the branch's pick, and the optional-loop
    // solicitation exactly as debrief_accept offers it (a repeat of the
    // same mission suppresses the offer)
    if (m_campaign) {
        int cur = Campaign.current_mission;

        if (Campaign.next_mission >= 0)
            strncpy(out.next_mission,
                    Campaign.missions[Campaign.next_mission].name,
                    sizeof(out.next_mission) - 1);

        out.loop_offer = Campaign.missions[cur].has_mission_loop &&
                         Campaign.loop_mission != -1 &&
                         Campaign.next_mission != cur;
        if (out.loop_offer && Campaign.missions[cur].mission_loop_desc)
            out.loop_desc = Campaign.missions[cur].mission_loop_desc;
    }

    return out;
}

void
fs2_t::accept(bool take_loop)
{
    if (!m_campaign)
        return;

    // the loop brief's YES (retail's popup, missiondebrief.cc:960):
    // steer the branch into the side loop before committing
    if (take_loop && Campaign.loop_mission != -1 &&
        Campaign.missions[Campaign.current_mission].has_mission_loop) {
        Campaign.loop_enabled = 1;
        Campaign.next_mission = Campaign.loop_mission;
    }

    // grants, the completion mark, the .csg save, next -> current
    mission_campaign_mission_over();

    // debrief_accept's post-check: reentry closes the loop
    if (Campaign.next_mission == Campaign.loop_reentry)
        Campaign.loop_enabled = 0;

    if (Campaign.next_mission == -1)
        m_campaign_over = true;

    // the pilot's bookmark -- stats and current campaign to the .plr
    write_pilot_file(Player);
}

hud_state_t
fs2_t::hud_state() const
{
    hud_state_t out;
    out.training_text[0] = '\0';
    out.training_voice[0] = '\0';
    out.primary_speed = 0.0f;
    out.target_signature = -1;
    out.target_subsys[0] = '\0';
    out.target_subsys_pos = vmd_zero_vector;
    out.weapon_energy = 0.0f;
    out.weapon_energy_max = 0.0f;
    out.burner_fuel = 0.0f;
    out.burner_fuel_max = 0.0f;

    if (!m_world_live)
        return out;

    // the selected primary's muzzle speed -- with the target's motion
    // (both already in the snapshot) it completes the lead solution --
    // and the energy/fuel gauges (HUD freight here, so the packed
    // frame() stays uniform across object kinds)
    if (Player_obj && Player_obj->type == OBJ_SHIP) {
        const ship &sp = Ships[Player_obj->instance];
        const ship_info &si = Ship_info[sp.ship_info_index];
        const ship_weapon &w = sp.weapons;
        if (w.current_primary_bank >= 0 &&
            w.current_primary_bank < w.num_primary_banks) {
            int wi = w.primary_bank_weapons[w.current_primary_bank];
            if (wi >= 0)
                out.primary_speed = Weapon_info[wi].max_speed;
        }
        out.weapon_energy = sp.weapon_energy;
        out.weapon_energy_max = si.max_weapon_reserve;
        out.burner_fuel = sp.afterburner_fuel;
        out.burner_fuel_max = si.afterburner_fuel_capacity;
    }

    // the player's target, as retail's own targeting state has it --
    // signature, not objnum, so the presenter keys into its snapshot map;
    // a freed slot (target died, world moved on) reads as no target
    if (Player_ai && Player_ai->target_objnum >= 0) {
        const object *t = &Objects[Player_ai->target_objnum];
        if (t->type != OBJ_NONE)
            out.target_signature = t->signature;

        if (Player_ai->targeted_subsys) {
            strncpy(out.target_subsys,
                    Player_ai->targeted_subsys->system_info->name,
                    sizeof(out.target_subsys) - 1);
            get_subsystem_world_pos(const_cast<object *>(t),
                                    Player_ai->targeted_subsys,
                                    &out.target_subsys_pos);
        }
    }

    // the weapon gauge, one line per MOUNTED bank (an authored-empty
    // bank reads -1 and stays off the gauge): primaries arm the
    // selected bank or all of them when linked, secondaries arm the
    // selected bank -- doubled shots under dual fire
    if (Player_obj && Player_obj->type == OBJ_SHIP) {
        const ship &sp = Ships[Player_obj->instance];
        const ship_weapon &w = sp.weapons;

        for (int i = 0; i < w.num_primary_banks; i++) {
            if (w.primary_bank_weapons[i] < 0)
                continue;

            weapon_bank_t b;
            memset(&b, 0, sizeof(b));
            strncpy(b.name, Weapon_info[w.primary_bank_weapons[i]].name,
                    sizeof(b.name) - 1);
            b.armed = (sp.flags & SF_PRIMARY_LINKED) ||
                      i == w.current_primary_bank;
            b.shots = 1;
            out.primary_banks.push_back(b);
        }

        for (int i = 0; i < w.num_secondary_banks; i++) {
            if (w.secondary_bank_weapons[i] < 0)
                continue;

            weapon_bank_t b;
            memset(&b, 0, sizeof(b));
            strncpy(b.name, Weapon_info[w.secondary_bank_weapons[i]].name,
                    sizeof(b.name) - 1);
            b.armed = i == w.current_secondary_bank;
            b.shots = (b.armed && (sp.flags & SF_SECONDARY_DUAL_FIRE)) ? 2
                                                                       : 1;
            out.secondary_banks.push_back(b);
        }
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

// the mission's authored sky: suns first, then the backdrop patches --
// instances from the mission's #Background bitmaps, catalog facts (sun
// RGBI + glow, blend mode) from stars.tbl via the same lookup the
// renderer uses
std::vector<backdrop_t>
fs2_t::backdrop() const
{
    std::vector<backdrop_t> out;

    if (!m_world_live)
        return out;

    for (int n = 0; n < Num_suns; n++) {
        backdrop_t d;
        memset(&d, 0, sizeof(d));
        d.sun = true;
        strncpy(d.name, Suns[n].filename, sizeof(d.name) - 1);
        vm_angles_2_matrix(&d.orient, &Suns[n].ang);
        d.scale_x = Suns[n].scale_x;
        d.scale_y = Suns[n].scale_y;
        d.div_x = Suns[n].div_x;
        d.div_y = Suns[n].div_y;

        int k = stars_find_sun(Suns[n].filename);
        starfield_bitmap *bm = k >= 0 ? &Sun_bitmaps[k] : NULL;
        if (bm) {
            strncpy(d.glow, bm->glow_filename, sizeof(d.glow) - 1);
            d.xparent = bm->xparent != 0;
            d.r = bm->r;
            d.g = bm->g;
            d.b = bm->b;
            d.i = bm->i;
        }
        out.push_back(d);
    }

    for (int n = 0; n < Num_starfield_bitmaps; n++) {
        starfield_bitmap_instance *inst = &Starfield_bitmap_instance[n];
        if (!inst->filename[0])
            continue;

        backdrop_t d;
        memset(&d, 0, sizeof(d));
        d.sun = false;
        strncpy(d.name, inst->filename, sizeof(d.name) - 1);
        vm_angles_2_matrix(&d.orient, &inst->ang);
        d.scale_x = inst->scale_x;
        d.scale_y = inst->scale_y;
        d.div_x = inst->div_x;
        d.div_y = inst->div_y;

        for (int k = 0; k < MAX_STARFIELD_BITMAPS; k++) {
            if (!stricmp(Starfield_bitmaps[k].filename, inst->filename)) {
                d.xparent = Starfield_bitmaps[k].xparent != 0;
                break;
            }
        }
        out.push_back(d);
    }

    return out;
}

int
fs2_t::num_stars() const
{
    return m_world_live ? Num_stars : 0;
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
            ev.birth = obj;   // the identity record crosses ONCE, here;
                              // frame() rows carry only what changes
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
