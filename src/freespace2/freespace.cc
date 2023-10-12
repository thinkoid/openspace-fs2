// -*- mode: c++; -*-

#include <unistd.h>
#include <sys/stat.h>
#include <SDL.h>
#include <SDL_main.h>
#include <cinttypes>
#include <stdexcept>

#include <defs.hh>
#include <anim/animplay.hh>
#include <asteroid/asteroid.hh>
#include <autopilot/autopilot.hh>
#include <bmpman/bmpman.hh>
#include <cfile/cfile.hh>
#include <cmdline/cmdline.hh>
#include <cmeasure/cmeasure.hh>
#include <cutscene/cutscenes.hh>
#include <cutscene/movie.hh>
#include <cutscene/player.hh>
#include <debris/debris.hh>
#include <debugconsole/console.hh>
#include <decals/decals.hh>
#include <events/events.hh>
#include <fireball/fireballs.hh>
#include "freespace2/freespace.hh"
#include "freespace2/SDLGraphicsOperations.hh"
#include "freespaceresource.hh"
#include <gamehelp/contexthelp.hh>
#include <gamehelp/gameplayhelp.hh>
#include <gamesequence/gamesequence.hh>
#include <gamesnd/eventmusic.hh>
#include <gamesnd/gamesnd.hh>
#include <graphics/font.hh>
#include <graphics/light.hh>
#include <graphics/matrix.hh>
#include <graphics/shadows.hh>
#include <hud/hud.hh>
#include <hud/hudconfig.hh>
#include <hud/hudescort.hh>
#include <hud/hudlock.hh>
#include <hud/hudmessage.hh>
#include <hud/hudparse.hh>
#include <hud/hudshield.hh>
#include <hud/hudsquadmsg.hh>
#include <hud/hudtargetbox.hh>
#include <iff_defs/iff_defs.hh>
#include <io/cursor.hh>
#include <io/joy.hh>
#include <io/joy_ff.hh>
#include <io/key.hh>
#include <io/mouse.hh>
#include <io/timer.hh>
#include <jumpnode/jumpnode.hh>
#include <lab/lab.hh>
#include <lab/wmcgui.hh> //So that GUI_System can be initialized
#include "levelpaging.hh"
#include "libs/ffmpeg/FFmpeg.hh"
#include <lighting/lighting.hh>
#include <localization/localize.hh>
#include <math/fix.hh>
#include <math/prng.hh>
#include <menuui/barracks.hh>
#include <menuui/credits.hh>
#include <menuui/mainhallmenu.hh>
#include <menuui/optionsmenu.hh>
#include <menuui/playermenu.hh>
#include <menuui/readyroom.hh>
#include <menuui/snazzyui.hh>
#include <menuui/techmenu.hh>
#include <menuui/trainingmenu.hh>
#include <mission/missionbriefcommon.hh>
#include <mission/missioncampaign.hh>
#include <mission/missiongoals.hh>
#include <mission/missionhotkey.hh>
#include <mission/missionload.hh>
#include <mission/missionlog.hh>
#include <mission/missionmessage.hh>
#include <mission/missionparse.hh>
#include <mission/missiontraining.hh>
#include <missionui/fictionviewer.hh>
#include <missionui/missionbrief.hh>
#include <missionui/missioncmdbrief.hh>
#include <missionui/missiondebrief.hh>
#include <missionui/missionloopbrief.hh>
#include <missionui/missionpause.hh>
#include <missionui/missionscreencommon.hh>
#include <missionui/missionshipchoice.hh>
#include <missionui/missionweaponchoice.hh>
#include <missionui/redalert.hh>
#include <mod_table/mod_table.hh>
#include <nebula/neb.hh>
#include <nebula/neblightning.hh>
#include <object/objcollide.hh>
#include <object/objectsnd.hh>
#include <object/waypoint.hh>
#include <observer/observer.hh>
#include <osapi/osapi.hh>
#include <osapi/osregistry.hh>
#include <parse/encrypt.hh>
#include <parse/parselo.hh>
#include <parse/sexp.hh>
#include <particle/ParticleManager.hh>
#include <particle/particle.hh>
#include <pilotfile/pilotfile.hh>
#include <playerman/managepilot.hh>
#include <playerman/player.hh>
#include <popup/popup.hh>
#include <popup/popupdead.hh>
#include <radar/radar.hh>
#include <radar/radarsetup.hh>
#include "render/3d.hh"
#include <render/batching.hh>
#include <shared/alphacolors.hh>
#include <shared/version.hh>
#include <ship/afterburner.hh>
#include <ship/awacs.hh>
#include <ship/ship.hh>
#include <ship/shipcontrails.hh>
#include <ship/shipfx.hh>
#include <ship/shiphit.hh>
#include <sound/audiostr.hh>
#include <sound/ds.hh>
#include <sound/sound.hh>
#include <starfield/starfield.hh>
#include <starfield/supernova.hh>
#include <stats/medals.hh>
#include <stats/stats.hh>
#include <tracing/Monitor.hh>
#include <tracing/tracing.hh>
#include <weapon/beam.hh>
#include <weapon/emp.hh>
#include <weapon/flak.hh>
#include <weapon/muzzleflash.hh>
#include <weapon/shockwave.hh>
#include <weapon/weapon.hh>
#include <assert/assert.hh>
#include <log/log.hh>

extern int Om_tracker_flag; // needed for FS2OpenPXO config

#ifdef NDEBUG
#ifdef FRED
#error macro FRED is defined when trying to build release FreeSpace.  Please undefine FRED macro in build settings
#endif
#endif

extern bool frame_rate_display;

void game_reset_view_clip ();
void game_reset_shade_frame ();
void game_post_level_init ();
void game_do_frame ();
void game_update_missiontime (); // called from game_do_frame() and
                                 // navmap_do_frame()
void game_reset_time ();
void game_show_framerate (); // draws framerate in lower right corner

int Game_no_clear = 0;

typedef struct big_expl_flash {
    float max_flash_intensity; // max intensity
    float cur_flash_intensity; // cur intensity
    int flash_start;           // start time
} big_expl_flash;

#define FRAME_FILTER 16

#define DEFAULT_SKILL_LEVEL 1
int Game_skill_level = DEFAULT_SKILL_LEVEL;

#define EXE_FNAME ("fs2.exe")

#define LAUNCHER_FNAME ("Launcher.exe")

// JAS: Code for warphole camera.
// Needs to be cleaned up.
float Warpout_time = 0.0f;
int Warpout_forced =
    0; // Set if this is a forced warpout that cannot be cancelled.
sound_handle Warpout_sound = sound_handle::invalid ();
int Use_joy_mouse = 0;
int Use_palette_flash = 1;
#ifndef NDEBUG
int Use_fullscreen_at_startup = 0;
#endif
int Show_area_effect = 0;
object* Last_view_target = NULL;

int frame_int = -1;
float frametimes[FRAME_FILTER];
float frametotal = 0.0f;
float flRealframetime;
float flFrametime;
fix FrametimeOverall = 0;

#ifndef NDEBUG
int Show_framerate = 1;
#else
int Show_framerate = 0;
#endif

int Framerate_cap = 120;

// to determine if networking should be disabled, needs to be done first thing
int Networking_disabled = 0;

// for the model page in system
extern void model_page_in_start ();

int Show_cpu = 0;
int Show_target_debug_info = 0;
int Show_target_weapons = 0;
int Game_font = -1;
#ifndef NDEBUG
static int Show_player_pos =
    0; // debug console command to show player world pos on HUD
#endif

int Debug_octant = -1;

fix Game_time_compression = F1_0;
fix Desired_time_compression = Game_time_compression;
fix Time_compression_change_rate = 0;
bool Time_compression_locked =
    false; // Can the user change time with shift- controls?

// auto-lang stuff
int detect_lang ();

// table checksums that will be used for pilot files
uint Weapon_tbl_checksum = 0;
uint Ships_tbl_checksum = 0;

// if the ships.tbl the player has is valid
int Game_ships_tbl_valid = 0;

// if the weapons.tbl the player has is valid
int Game_weapons_tbl_valid = 0;

int Test_begin = 0;
extern int Player_attacking_enabled;
int Show_net_stats;

int Pre_player_entry;

int Fred_running = 0;
bool running_unittests = false;

// required for hudtarget... kinda dumb, but meh
char Fred_alt_names[MAX_SHIPS][NAME_LENGTH + 1];
char Fred_callsigns[MAX_SHIPS][NAME_LENGTH + 1];

char Game_current_mission_filename[MAX_FILENAME_LEN];
int game_single_step = 0;
int last_single_step = 0;

int game_zbuffer = 1;
static int Game_paused;

#define EXPIRE_BAD_CHECKSUM 1
#define EXPIRE_BAD_TIME 2

extern void ssm_init ();
extern void ssm_level_init ();
extern void ssm_process ();

// defines and variables used for dumping frame for making trailers.
#ifndef NDEBUG
int Debug_dump_frames = 0; // Set to 0 to not dump frames, else equal hz to
                           // dump. (15 or 30 probably)
int Debug_dump_trigger = 0;
int Debug_dump_frame_count;
int Debug_dump_frame_num = 0;
#define DUMP_BUFFER_NUM_FRAMES 1 // store every 15 frames
#endif

// amount of time to wait after the player has died before we display the death
// died popup
#define PLAYER_DIED_POPUP_WAIT 2500
int Player_died_popup_wait = -1;

int Multi_ping_timestamp = -1;

// builtin mission list stuff
int Game_builtin_mission_count = 92;
fs_builtin_mission Game_builtin_mission_list[MAX_BUILTIN_MISSIONS] = {
    // single player campaign
    { "freespace2.fc2", (FSB_FROM_VOLITION | FSB_CAMPAIGN_FILE), "" },

    // act 1
    { "sm1-01.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_2 },
    { "sm1-02.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_2 },
    { "sm1-03.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_2 },
    { "sm1-04.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_2 },
    { "sm1-05.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_2 },
    { "sm1-06.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_2 },
    { "sm1-07.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_2 },
    { "sm1-08.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_2 },
    { "sm1-09.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_2 },
    { "sm1-10.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_2 },
    { "loop1-1.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_2 },
    { "loop1-2.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_2 },
    { "loop1-3.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_2 },
    { "training-1.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN),
      FS_CDROM_VOLUME_2 },
    { "training-2.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN),
      FS_CDROM_VOLUME_2 },
    { "training-3.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN),
      FS_CDROM_VOLUME_2 },
    { "tsm-104.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_2 },
    { "tsm-105.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_2 },
    { "tsm-106.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_2 },

    // act 2
    { "sm2-01.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_3 },
    { "sm2-02.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_3 },
    { "sm2-03.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_3 },
    { "sm2-04.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_3 },
    { "sm2-05.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_3 },
    { "sm2-06.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_3 },
    { "sm2-07.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_3 },
    { "sm2-08.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_3 },
    { "sm2-09.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_3 },
    { "sm2-10.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_3 },

    // act 3
    { "sm3-01.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_3 },
    { "sm3-02.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_3 },
    { "sm3-03.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_3 },
    { "sm3-04.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_3 },
    { "sm3-05.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_3 },
    { "sm3-06.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_3 },
    { "sm3-07.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_3 },
    { "sm3-08.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_3 },
    { "sm3-09.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_3 },
    { "sm3-10.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_3 },
    { "loop2-1.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_3 },
    { "loop2-2.fs2", (FSB_FROM_VOLITION | FSB_CAMPAIGN), FS_CDROM_VOLUME_3 },

    // multiplayer missions

    // gauntlet
    { "g-shi.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "g-ter.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "g-vas.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },

    // coop
    { "m-01.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "m-02.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "m-03.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "m-04.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },

    // dogfight
    { "mdh-01.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mdh-02.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mdh-03.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mdh-04.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mdh-05.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mdh-06.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mdh-07.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mdh-08.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mdh-09.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mdl-01.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mdl-02.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mdl-03.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mdl-04.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mdl-05.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mdl-06.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mdl-07.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mdl-08.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mdl-09.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mdm-01.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mdm-02.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mdm-03.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mdm-04.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mdm-05.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mdm-06.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mdm-07.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mdm-08.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mdm-09.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "osdog.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },

    // TvT
    { "mt-01.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mt-02.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mt-03.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mt-04.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mt-05.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mt-06.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mt-07.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mt-08.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mt-09.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },
    { "mt-10.fs2", (FSB_FROM_VOLITION | FSB_MULTI), "" },

    // campaign
    { "templar.fc2", (FSB_FROM_VOLITION | FSB_MULTI | FSB_CAMPAIGN_FILE), "" },
    { "templar-01.fs2", (FSB_FROM_VOLITION | FSB_MULTI | FSB_CAMPAIGN), "" },
    { "templar-02.fs2", (FSB_FROM_VOLITION | FSB_MULTI | FSB_CAMPAIGN), "" },
    { "templar-03.fs2", (FSB_FROM_VOLITION | FSB_MULTI | FSB_CAMPAIGN), "" },
    { "templar-04.fs2", (FSB_FROM_VOLITION | FSB_MULTI | FSB_CAMPAIGN), "" },
};

// Internal function prototypes
void game_do_training_checks ();
void game_shutdown (void);
void game_show_event_debug (float frametime);
void game_event_debug_init ();
void game_frame (bool paused = false);
void game_start_subspace_ambient_sound ();
void game_stop_subspace_ambient_sound ();
void verify_ships_tbl ();
void verify_weapons_tbl ();
void game_title_screen_display ();
void game_title_screen_close ();

// loading background filenames
static const char* Game_loading_bground_fname[GR_NUM_RESOLUTIONS] = {
    "LoadingBG",  // GR_640
    "2_LoadingBG" // GR_1024
};

static const char* Game_loading_ani_fname[GR_NUM_RESOLUTIONS] = {
    "Loading",  // GR_640
    "2_Loading" // GR_1024
};

static const char* Game_title_screen_fname[GR_NUM_RESOLUTIONS] = {
    "PreLoad", "2_PreLoad"
};

static const char* Game_logo_screen_fname[GR_NUM_RESOLUTIONS] = {
    "PreLoadLogo", "2_PreLoadLogo"
};

// for title screens
static int Game_title_bitmap = -1;
static int Game_title_logo = -1;

// How much RAM is on this machine. Set in WinMain
uint64_t FreeSpace_total_ram = 0;

// game flash stuff
float Game_flash_red = 0.0f;
float Game_flash_green = 0.0f;
float Game_flash_blue = 0.0f;
float Sun_spot = 0.0f;
big_expl_flash Big_expl_flash = { 0.0f, 0.0f, 0 };

// game shudder stuff (in ms)
int Game_shudder_time = -1;
int Game_shudder_total = 0;
float Game_shudder_intensity = 0.0f; // should be between 0.0 and 100.0

// EAX stuff
sound_env Game_sound_env;
sound_env Game_default_sound_env = { EAX_ENVIRONMENT_BATHROOM, 0.2f, 0.2f,
                                     1.0f };
int Game_sound_env_update_timestamp;

fs_builtin_mission* game_find_builtin_mission (char* filename) {
    int idx;

    // look through all existing builtin missions
    for (idx = 0; idx < Game_builtin_mission_count; idx++) {
        if (!strcasecmp (Game_builtin_mission_list[idx].filename, filename)) {
            return &Game_builtin_mission_list[idx];
        }
    }

    // didn't find it
    return NULL;
}

int game_get_default_skill_level () { return DEFAULT_SKILL_LEVEL; }

// Resets the flash
void game_flash_reset () {
    Game_flash_red = 0.0f;
    Game_flash_green = 0.0f;
    Game_flash_blue = 0.0f;
    Sun_spot = 0.0f;
    Big_expl_flash.max_flash_intensity = 0.0f;
    Big_expl_flash.cur_flash_intensity = 0.0f;
    Big_expl_flash.flash_start = 0;
}

float Gf_critical =
    -1.0f; // framerate we should be above on the average for this mission
float Gf_critical_time =
    0.0f; // how much time we've been at the critical framerate

void game_framerate_check_init () {
    // zero critical time
    Gf_critical_time = 0.0f;

    // nebula missions
    if (The_mission.flags[Mission::Mission_Flags::Fullneb]) {
        Gf_critical = 15.0f;
    }
    else {
        Gf_critical = 25.0f;
    }
}

extern float Framerate;
void game_framerate_check () {
    int y_start = gr_screen.center_offset_y + 100;

    // if the current framerate is above the critical level, add frametime
    if (Framerate >= Gf_critical) { Gf_critical_time += flFrametime; }

    if (!Show_framerate) { return; }

    // display if we're above the critical framerate
    if (Framerate < Gf_critical) {
        gr_set_color_fast (&Color_bright_red);
        gr_string (
            gr_screen.center_offset_x + 200, y_start, "Framerate warning",
            GR_RESIZE_NONE);

        y_start += 10;
    }

    // display our current pct of good frametime
    if (f2fl (Missiontime) >= 0.0f) {
        float pct = (Gf_critical_time / f2fl (Missiontime)) * 100.0f;

        if (pct >= 85.0f) { gr_set_color_fast (&Color_bright_green); }
        else {
            gr_set_color_fast (&Color_bright_red);
        }

        gr_printf_no_resize (
            gr_screen.center_offset_x + 200, y_start, "%d%%", (int)pct);

        y_start += 10;
    }
}

/**
 * Adds a flash effect.
 *
 * These can be positive or negative. The range will get capped at around -1 to
 * 1, so stick with a range like that.
 *
 * @param r red colour value
 * @param g green colour value
 * @param b blue colour value
 */
void game_flash (float r, float g, float b) {
    Game_flash_red += r;
    Game_flash_green += g;
    Game_flash_blue += b;

    if (Game_flash_red < -1.0f) { Game_flash_red = -1.0f; }
    else if (Game_flash_red > 1.0f) {
        Game_flash_red = 1.0f;
    }

    if (Game_flash_green < -1.0f) { Game_flash_green = -1.0f; }
    else if (Game_flash_green > 1.0f) {
        Game_flash_green = 1.0f;
    }

    if (Game_flash_blue < -1.0f) { Game_flash_blue = -1.0f; }
    else if (Game_flash_blue > 1.0f) {
        Game_flash_blue = 1.0f;
    }
}

/**
 * Adds a flash for Big Ship explosions
 * @param flash flash intensity. Range capped from 0 to 1.
 */
void big_explosion_flash (float flash) {
    CLAMP (flash, 0.0f, 1.0f);

    Big_expl_flash.flash_start = timestamp (1);
    Big_expl_flash.max_flash_intensity = flash;
    Big_expl_flash.cur_flash_intensity = 0.0f;
}

// Amount to diminish palette towards normal, per second.
#define DIMINISH_RATE 0.75f
#define SUN_DIMINISH_RATE 6.00f

int Sun_drew = 0;

float sn_glare_scale = 1.7f;
DCF (sn_glare, "Sets the sun glare scale (Default is 1.7)") {
    dc_stuff_float (&sn_glare_scale);
}

float Supernova_last_glare = 0.0f;
bool stars_sun_has_glare (int index);
extern bool ls_on;
extern bool ls_force_off;
void game_sunspot_process (float frametime) {
    TRACE_SCOPE (tracing::SunspotProcess);
    int n_lights, idx;
    int sn_stage;
    float Sun_spot_goal = 0.0f;

    // supernova
    sn_stage = supernova_active ();
    if (sn_stage) {
        // sunspot differently based on supernova stage
        switch (sn_stage) {
        // approaching. player still in control
        case 1:
            float pct;
            pct = (1.0f - (supernova_time_left () / SUPERNOVA_CUT_TIME));

            vec3d light_dir;
            light_get_global_dir (&light_dir, 0);
            float dot;
            dot = vm_vec_dot (&light_dir, &Eye_matrix.vec.fvec);

            if (dot >= 0.0f) {
                // scale it some more
                dot = dot * (0.5f + (pct * 0.5f));
                dot += 0.05f;

                Sun_spot_goal += (dot * sn_glare_scale);
            }

            // draw the sun glow
            if (!shipfx_eye_in_shadow (&Eye_position, Viewer_obj, 0)) {
                // draw the glow for this sun
                stars_draw_sun_glow (0);
            }

            Supernova_last_glare = Sun_spot_goal;
            break;

        // camera cut. player not in control. note : at this point camera
        // starts out facing the sun. so we can go nice and bright
        case 2:
        case 3:
            Sun_spot_goal = 0.9f;
            Sun_spot_goal +=
                (1.0f - (supernova_time_left () / SUPERNOVA_CUT_TIME)) * 0.1f;

            if (Sun_spot_goal > 1.0f) { Sun_spot_goal = 1.0f; }

            Sun_spot_goal *= sn_glare_scale;
            Supernova_last_glare = Sun_spot_goal;
            break;

        // fade to white. display dead popup
        case 4:
        case 5:
            Supernova_last_glare += (2.0f * flFrametime);
            if (Supernova_last_glare > 2.0f) { Supernova_last_glare = 2.0f; }

            Sun_spot_goal = Supernova_last_glare;
            break;
        }

        Sun_drew = 0;
    }
    else {
        Sun_spot_goal = 0.0f;
        if (Sun_drew) {
            // check sunspots for all suns
            n_lights = light_get_global_count ();

            // check
            for (idx = 0; idx < n_lights; idx++) {
                bool in_shadow =
                    shipfx_eye_in_shadow (&Eye_position, Viewer_obj, idx);

                if ((ls_on && !ls_force_off) || !in_shadow) {
                    vec3d light_dir;
                    light_get_global_dir (&light_dir, idx);

                    // only do sunglare stuff if this sun has one
                    if (stars_sun_has_glare (idx)) {
                        float dot =
                            vm_vec_dot (&light_dir, &Eye_matrix.vec.fvec) *
                                0.5f +
                            0.5f;
                        Sun_spot_goal += (float)pow (dot, 85.0f);
                    }
                }
                if (!in_shadow) {
                    // draw the glow for this sun
                    stars_draw_sun_glow (idx);
                }
            }

            Sun_drew = 0;
        }
    }

    float dec_amount = frametime * SUN_DIMINISH_RATE;

    if (Sun_spot < Sun_spot_goal) {
        Sun_spot += dec_amount;
        if (Sun_spot > Sun_spot_goal) { Sun_spot = Sun_spot_goal; }
    }
    else if (Sun_spot > Sun_spot_goal) {
        Sun_spot -= dec_amount;
        if (Sun_spot < Sun_spot_goal) { Sun_spot = Sun_spot_goal; }
    }
}

/**
 * Call once a frame to diminish the flash effect to 0.
 * @param frametime Period over which to dimish at ::DIMINISH_RATE
 */
void game_flash_diminish (float frametime) {
    float dec_amount = frametime * DIMINISH_RATE;

    if (Game_flash_red > 0.0f) {
        Game_flash_red -= dec_amount;
        if (Game_flash_red < 0.0f) Game_flash_red = 0.0f;
    }
    else {
        Game_flash_red += dec_amount;
        if (Game_flash_red > 0.0f) Game_flash_red = 0.0f;
    }

    if (Game_flash_green > 0.0f) {
        Game_flash_green -= dec_amount;
        if (Game_flash_green < 0.0f) Game_flash_green = 0.0f;
    }
    else {
        Game_flash_green += dec_amount;
        if (Game_flash_green > 0.0f) Game_flash_green = 0.0f;
    }

    if (Game_flash_blue > 0.0f) {
        Game_flash_blue -= dec_amount;
        if (Game_flash_blue < 0.0f) Game_flash_blue = 0.0f;
    }
    else {
        Game_flash_blue += dec_amount;
        if (Game_flash_blue > 0.0f) Game_flash_blue = 0.0f;
    }

    // update big_explosion_cur_flash
#define TIME_UP 1500
#define TIME_DOWN 2500
    int duration = TIME_UP + TIME_DOWN;
    int time = timestamp_until (Big_expl_flash.flash_start);
    if (time > -duration) {
        time = -time;
        if (time < TIME_UP) {
            Big_expl_flash.cur_flash_intensity =
                Big_expl_flash.max_flash_intensity * time / (float)TIME_UP;
        }
        else {
            time -= TIME_UP;
            Big_expl_flash.cur_flash_intensity =
                Big_expl_flash.max_flash_intensity *
                ((float)TIME_DOWN - time) / (float)TIME_DOWN;
        }
    }

    if (Use_palette_flash) {
        int r, g, b;

        // Change the 200 to change the color range of colors.
        r = int (Game_flash_red * 128.0f);
        g = int (Game_flash_green * 128.0f);
        b = int (Game_flash_blue * 128.0f);

        if (Sun_spot > 0.0f && (!ls_on || ls_force_off)) {
            r += int (Sun_spot * 128.0f);
            g += int (Sun_spot * 128.0f);
            b += int (Sun_spot * 128.0f);
        }

        if (Big_expl_flash.cur_flash_intensity > 0.0f) {
            r += int (Big_expl_flash.cur_flash_intensity * 128.0f);
            g += int (Big_expl_flash.cur_flash_intensity * 128.0f);
            b += int (Big_expl_flash.cur_flash_intensity * 128.0f);
        }

        if (r < 0)
            r = 0;
        else if (r > 255)
            r = 255;
        if (g < 0)
            g = 0;
        else if (g > 255)
            g = 255;
        if (b < 0)
            b = 0;
        else if (b > 255)
            b = 255;

        if ((r != 0) || (g != 0) || (b != 0)) { gr_flash (r, g, b); }
    }
}

void game_level_close () {
    // save player-persistent variables
    mission_campaign_save_on_close_variables (); // Goober5000

    // De-Initialize the game subsystems
    obj_delete_all ();
    sexp_music_close (); // Goober5000
    event_music_level_close ();
    game_stop_looped_sounds ();
    snd_stop_all ();
    obj_snd_level_close (); // uninit object-linked persistant sounds
    gamesnd_unload_gameplay_sounds (); // unload gameplay sounds from
    // memory
    anim_level_close ();         // stop and clean up any anim instances
    message_mission_shutdown (); // called after anim_level_close() to make
    // sure instances are clear
    shockwave_level_close ();
    fireball_close ();
    shield_hit_close ();
    mission_event_shutdown ();
    asteroid_level_close ();
    jumpnode_level_close ();
    waypoint_level_close ();
    flak_level_close (); // unload flak stuff
    neb2_level_close (); // shutdown gaseous nebula stuff
    ct_level_close ();
    beam_level_close ();
    mflash_level_close ();
    mission_brief_common_reset (); // close out parsed briefing/mission
    // stuff
    cam_close ();
    subtitles_close ();
    particle::ParticleManager::get ()->clearSources ();
    particle::close ();
    trail_level_close ();
    ship_clear_cockpit_displays ();
    hud_level_close ();
    model_instance_free_all ();
    batch_render_close ();

    // be sure to not only reset the time but the lock as well
    set_time_compression (1.0f, 0.0f);
    lock_time_compression (false);

    audiostream_unpause_all ();
    Game_paused = 0;

    gr_set_ambient_light (120, 120, 120);

    stars_level_close ();

    Pilot.save_savefile ();
}

uint load_gl_init;
uint load_mission_load;
uint load_post_level_init;

/**
 * Intializes game stuff.
 *
 * @return 0 on failure, 1 on success
 */
void game_level_init () {
    game_busy (NOX ("** starting game_level_init() **"));
    load_gl_init = (uint)time (NULL);

    Framecount = 0;
    game_reset_view_clip ();
    game_reset_shade_frame ();

    Key_normal_game = (Game_mode & GM_NORMAL);
    Cheats_enabled = 0;

    Game_shudder_time = -1;

    Perspective_locked = false;

    // reset the geometry map and distortion map batcher, this should to be
    // done pretty soon in this mission load process (though it's not required)
    batch_reset ();

    // Initialize the game subsystems
    game_reset_time (); // resets time, and resets saved time too

    Multi_ping_timestamp = -1;

    obj_init (); // Must be inited before the other systems

    if (!(Game_mode & GM_STANDALONE_SERVER)) {
        model_page_in_start (); // mark any existing models as unused but don't
                                // unload them yet
        WARNINGF (LOCATION, "Beginning level bitmap paging...");
        bm_page_in_start ();
    }
    else {
        model_free_all (); // Free all existing models if standalone server
    }

    mission_brief_common_init (); // Free all existing briefing/debriefing text
    weapon_level_init ();

    NavSystem_Init (); // zero out the nav system

    ai_level_init (); // Call this before ship_init() because it reads
                      // ai.tbl.
    ship_level_init ();
    player_level_init ();
    shipfx_flash_init (); // Init the ship gun flash system.
    game_flash_reset ();  // Reset the flash effect
    particle::init ();    // Reset the particle system
    fireball_init ();
    debris_init ();
    shield_hit_init (); // Initialize system for showing shield hits

    mission_init_goals ();
    mission_log_init ();
    messages_init ();
    obj_snd_level_init (); // init object-linked persistant sounds
    anim_level_init ();
    shockwave_level_init ();
    afterburner_level_init ();
    scoring_level_init (&Player->stats);
    key_level_init ();
    asteroid_level_init ();
    control_config_clear_used_status ();
    collide_ship_ship_sounds_init ();
    Missiontime = 0;
    Skybox_timestamp = game_get_overall_frametime ();
    Pre_player_entry = 1; // Means the player has not yet entered.
    Entry_delay_time = 0; // Could get overwritten in mission read.
    observer_init ();
    flak_level_init ();  // initialize flak - bitmaps, etc
    ct_level_init ();    // initialize ships contrails, etc
    awacs_level_init (); // initialize AWACS
    beam_level_init ();  // initialize beam weapons
    mflash_level_init ();
    ssm_level_init ();
    supernova_level_init ();
    cam_init ();
    snd_aav_init ();

    shipfx_engine_wash_level_init ();

    stars_pre_level_init ();
    neb2_level_init ();
    nebl_level_init ();

    Last_view_target = NULL;
    Game_paused = 0;

    Game_no_clear = 0;

    // campaign wasn't ended
    Campaign_ending_via_supernova = 0;

    load_gl_init = (uint) (time (NULL) - load_gl_init);
}

/**
 * Called when a mission is over -- does server specific stuff.
 */
void freespace_stop_mission () {
    game_level_close ();
    Game_mode &= ~GM_IN_MISSION;
}

// An estimate as to how high the count passed to game_loading_callback will
// go. This is just a guess, it seems to always be about the same.   The count
// is proportional to the code being executed, not the time, so this works good
// for a bar, assuming the code does about the same thing each time you
// load a level.   You can find this value by looking at the return value
// of game_busy_callback(NULL), which I conveniently print out to the
// debug output window with the '=== ENDING LOAD ==' stuff.
#define COUNT_ESTIMATE 425

int Game_loading_callback_inited = 0;
int Game_loading_background = -1;
generic_anim Game_loading_ani;

static int Game_loading_ani_coords[GR_NUM_RESOLUTIONS][2] = {
    {
        63, 316 // GR_640
    },
    {
        101, 505 // GR_1024
    }
};

#ifndef NDEBUG
extern char Processing_filename[MAX_PATH_LEN];
static int busy_shader_created = 0;
shader busy_shader;
#endif
static int framenum;

/**
 * This gets called 10x per second and count is the number of times
 * ::game_busy() has been called since the current callback function was set.
 */
void game_loading_callback (int count) {
    ASSERT (Game_loading_callback_inited == 1);
    ASSERTX (
        Game_loading_ani.num_frames > 0,
        "Load Screen animation %s not found, or corrupted. Needs to be an "
        "animation with at least 1 frame.",
        Game_loading_ani.filename);

    int do_flip = 0;

    int new_framenum = bm_get_anim_frame (
        Game_loading_ani.first_frame, static_cast< float > (count),
        static_cast< float > (COUNT_ESTIMATE));
    // retail incremented the frame number by one, essentially skipping the 1st
    // frame except for single-frame anims
    if (Game_loading_ani.num_frames > 1 &&
        new_framenum < Game_loading_ani.num_frames - 1) {
        new_framenum++;
    }

    // make sure we always run forwards - graphical hack
    if (new_framenum > framenum) framenum = new_framenum;

    if (Game_loading_ani.num_frames > 0) {
        GR_MAYBE_CLEAR_RES (Game_loading_background);
        if (Game_loading_background > -1) {
            gr_set_bitmap (Game_loading_background);
            gr_bitmap (0, 0, GR_RESIZE_MENU);
        }

        gr_set_bitmap (Game_loading_ani.first_frame + framenum);
        gr_bitmap (
            Game_loading_ani_coords[gr_screen.res][0],
            Game_loading_ani_coords[gr_screen.res][1], GR_RESIZE_MENU);

        do_flip = 1;
    }

#ifndef NDEBUG
    // print the current filename being processed by game_busy(), the shader
    // here is a quick hack since the background isn't always drawn so we can't
    // clear the text away from the previous filename. the shader is completely
    // opaque to hide the old text. must easier and faster than redrawing the
    // entire screen every flip - taylor
    if (!busy_shader_created) {
        gr_create_shader (&busy_shader, 5, 5, 5, 255);
        busy_shader_created = 1;
    }

    if (Processing_filename[0] != '\0') {
        gr_set_shader (&busy_shader);
        gr_shade (
            0, 0, gr_screen.max_w_unscaled, 17,
            GR_RESIZE_MENU); // make sure it goes across the entire width

        gr_set_color_fast (&Color_white);
        gr_string (5, 5, Processing_filename, GR_RESIZE_MENU);

        do_flip = 1;
        memset (Processing_filename, 0, MAX_PATH_LEN);
    }
#endif

    auto progress =
        static_cast< float > (count) / static_cast< float > (COUNT_ESTIMATE);
    CLAMP (progress, 0.0f, 1.0f);

    fs2::os::events::buffer_all ();

    if (do_flip) gr_flip ();
}

void game_loading_callback_init () {
    ASSERT (Game_loading_callback_inited == 0);

    Game_loading_background =
        bm_load (The_mission.loading_screen[gr_screen.res]);

    if (Game_loading_background < 0)
        Game_loading_background =
            bm_load (Game_loading_bground_fname[gr_screen.res]);

    generic_anim_init (
        &Game_loading_ani, Game_loading_ani_fname[gr_screen.res]);
    generic_anim_load (&Game_loading_ani);
    ASSERTX (
        Game_loading_ani.num_frames > 0,
        "Load Screen animation %s not found, or corrupted. Needs to be an "
        "animation with at least 1 frame.",
        Game_loading_ani.filename);

    Game_loading_callback_inited = 1;
    io::mouse::CursorManager::get ()->showCursor (false);
    framenum = 0;
    game_busy_callback (
        game_loading_callback,
        (COUNT_ESTIMATE / Game_loading_ani.num_frames) + 1);
}

void game_loading_callback_close () {
    ASSERT (Game_loading_callback_inited == 1);

    // Make sure bar shows all the way over.
    game_loading_callback (COUNT_ESTIMATE);

    int real_count = game_busy_callback (NULL);
    io::mouse::CursorManager::get ()->showCursor (true);

    Game_loading_callback_inited = 0;

#ifndef NDEBUG
    WARNINGF (LOCATION, "=================== ENDING LOAD ================");
    WARNINGF (LOCATION, "Real count = %d, Estimated count = %d", real_count,COUNT_ESTIMATE);
    WARNINGF (LOCATION, "================================================");
#else
    // to remove warnings in release build
    real_count = 0;
#endif

    generic_anim_unload (&Game_loading_ani);

    bm_release (Game_loading_background);
    common_free_interface_palette (); // restore game palette
    Game_loading_background = -1;

    font::set_font (font::FONT1);
}

/**
 * Update the sound environment (ie change EAX settings based on proximity to
 * large ships)
 */
void game_maybe_update_sound_environment () {
    // do nothing for now
}

/**
 * Assign the sound environment for the game, based on the current mission
 */
void game_assign_sound_environment () {
    if (The_mission.sound_environment.id >= 0) {
        Game_sound_env = The_mission.sound_environment;
    }
    else if (SND_ENV_DEFAULT > 0) {
        sound_env_get (&Game_sound_env, SND_ENV_DEFAULT);
    }
    else {
        Game_sound_env = Game_default_sound_env;
    }

    Game_sound_env_update_timestamp = timestamp (1);
}

/**
 * Function which gets called before actually entering the mission.
 */
void freespace_mission_load_stuff () {
    // called if we're not on a freespace dedicated (non rendering, no pilot)
    // server IE : we _don't_ want to load any sounds or bitmap/texture info on
    // this machine.
    if (!(Game_mode & GM_STANDALONE_SERVER)) {
        WARNINGF (LOCATION,"=================== STARTING LEVEL DATA LOAD ==================");

        game_busy (NOX ("** setting up event music **"));
        event_music_level_init (
            -1); // preloads the first 2 seconds for each event music track

        game_busy (NOX ("** unloading interface sounds **"));
        gamesnd_unload_interface_sounds (); // unload interface sounds from
                                            // memory

        {
            TRACE_SCOPE (tracing::PreloadMissionSounds);

            game_busy (NOX ("** preloading common game sounds **"));
            gamesnd_preload_common_sounds (); // load in sounds that are
                                              // expected to play

            game_busy (NOX ("** preloading gameplay sounds **"));
            gamesnd_load_gameplay_sounds (); // preload in gameplay sounds if
                                             // wanted

            game_busy (NOX ("** assigning sound environment for mission **"));
            ship_assign_sound_all ();         // assign engine sounds to ships
            game_assign_sound_environment (); // assign the sound environment
                                              // for this mission
        }

        obj_merge_created_list ();

        if (!(Game_mode & GM_MULTIPLAYER)) {
            // call function in missionparse.cpp to fixup player/ai stuff.
            game_busy (NOX ("** fixing up player/ai stuff **"));
            mission_parse_fixup_players ();
        }

        // Load in all the bitmaps for this level
        level_page_in ();

        game_busy (NOX ("** finished with level_page_in() **"));

        if (Game_loading_callback_inited) { game_loading_callback_close (); }
    }
    // the only thing we need to call on the standalone for now.
    else {
        obj_merge_created_list ();

        // Load in all the bitmaps for this level
        level_page_in ();
    }
}

/**
 * Called after mission is loaded.
 *
 * Because player isn't created until after mission loads, some things must get
 * initted after the level loads
 */
void game_post_level_init () {
    TRACE_SCOPE (tracing::LoadPostMissionLoad);

    HUD_init ();
    hud_setup_escort_list ();
    mission_hotkey_set_defaults (); // set up the default hotkeys (from mission
                                    // file)

    stars_post_level_init ();

    // While trying to track down the nebula bug I encountered a cool effect -
    // comment this out to fly a mission in a void. Maybe we should develop
    // this into a full effect or something, because it is seriously cool.
    neb2_post_level_init ();

    // Initialize decal system
    decals::initializeMission ();

#ifndef NDEBUG
    game_event_debug_init ();
#endif

    training_mission_init ();
    asteroid_create_all ();

    // set ambient light for level
    gr_set_ambient_light (
        The_mission.ambient_light_level & 0xff,
        (The_mission.ambient_light_level >> 8) & 0xff,
        (The_mission.ambient_light_level >> 16) & 0xff);

    game_framerate_check_init ();

    // If this is a red alert mission in campaign mode, bash wingman status
    if ((Game_mode & GM_CAMPAIGN_MODE) && red_alert_mission ()) {
        red_alert_bash_wingman_status ();
    }

    freespace_mission_load_stuff ();
}

/**
 * Tells the server to load the mission and initialize structures
 */
int game_start_mission () {
    WARNINGF (LOCATION,"=================== STARTING LEVEL LOAD ==================");

    int s1 = timer_get_milliseconds ();

    // clear post processing settings
    gr_post_process_set_defaults ();

    get_mission_info (Game_current_mission_filename, &The_mission, false);

    if (!(Game_mode & GM_STANDALONE_SERVER)) game_loading_callback_init ();

    game_level_init ();

    game_busy (NOX ("** starting mission_load() **"));
    load_mission_load = (uint)time (NULL);
    if (mission_load (Game_current_mission_filename)) {
        popup (
            PF_BODY_BIG | PF_USE_AFFIRMATIVE_ICON, 1, POPUP_OK,
            XSTR ("Attempt to load the mission failed", 169));

        gameseq_post_event (GS_EVENT_MAIN_MENU);
        game_loading_callback_close ();
        game_level_close ();

        return 0;
    }

    load_mission_load = (uint) (time (NULL) - load_mission_load);

    // free up memory from parsing the mission
    extern void stop_parse ();
    stop_parse ();

    game_busy (NOX ("** starting game_post_level_init() **"));
    load_post_level_init = (uint)time (NULL);
    game_post_level_init ();
    load_post_level_init = (uint) (time (NULL) - load_post_level_init);

#ifndef NDEBUG
    {
        void Do_model_timings_test ();
        Do_model_timings_test ();
    }
#endif

    bm_print_bitmaps ();

    int e1 = timer_get_milliseconds ();

    WARNINGF (LOCATION, "Level load took %f seconds.", (e1 - s1) / 1000.0f);
    return 1;
}

int Interface_framerate = 0;
#ifndef NDEBUG

DCF_BOOL (mouse_control, Use_mouse_to_fly)
DCF_BOOL (show_framerate, Show_framerate)
DCF_BOOL (show_target_debug_info, Show_target_debug_info)
DCF_BOOL (show_target_weapons, Show_target_weapons)
DCF_BOOL (lead_target_cheat, Players[Player_num].lead_target_cheat)
DCF_BOOL (sound, Sound_enabled)
DCF_BOOL (zbuffer, game_zbuffer)
DCF_BOOL (show_shield_mesh, Show_shield_mesh)
DCF_BOOL (player_attacking, Player_attacking_enabled)
DCF_BOOL (show_waypoints, Show_waypoints)
DCF_BOOL (show_area_effect, Show_area_effect)
DCF_BOOL (show_net_stats, Show_net_stats)
extern int Training_message_method;
DCF_BOOL (training_msg_method, Training_message_method)
DCF_BOOL (show_player_pos, Show_player_pos)
DCF_BOOL (i_framerate, Interface_framerate)
DCF_BOOL (show_bmpman_usage, Cmdline_bmpman_usage)

DCF (warp, "Tests warpin effect") {
    if (dc_optional_string_either ("help", "--help")) {
        dc_printf (
            "Params: bool warpin, string Target = "
            "\n  Warps in if true, out if false. Player is target unless "
            "specific ship is specified\n");
        return;
    } // Else, process command

    // TODO: Provide status flag

    bool warpin;
    char target[MAX_NAME_LEN];
    int idx = -1;

    dc_stuff_boolean (&warpin);
    if (dc_maybe_stuff_string_white (target, MAX_NAME_LEN)) {
        idx = ship_name_lookup (target);
    } // Else, default target to player

    if (idx < 0) {
        // Player is target
        if (Player_ai->target_objnum > -1) {
            if (warpin) {
                shipfx_warpin_start (&Objects[Player_ai->target_objnum]);
            }
            else {
                shipfx_warpout_start (&Objects[Player_ai->target_objnum]);
            }
        }
    }
    else {
        // Non-player is targer
        if (warpin) { shipfx_warpin_start (&Objects[Ships[idx].objnum]); }
        else {
            shipfx_warpout_start (&Objects[Ships[idx].objnum]);
        }
    }
}

DCF (show_cpu, "Toggles showing cpu usage") {
    bool process = true;

    if (dc_optional_string_either ("help", "--help")) {
        dc_printf (
            "Usage: (optional) bool Show_cpu\n If true, Show_cpu is set and "
            "Show_mem is cleared.  If false, then Show_cpu is cleared.  If "
            "nothing passed, then toggle.\n");
        process = false;
    }

    if (dc_optional_string_either ("status", "--status") ||
        dc_optional_string_either ("?", "--?")) {
        dc_printf ("Show_cpu is %s\n", (Show_cpu ? "TRUE" : "FALSE"));
        process = false;
    }

    if (!process) {
        // Help and/or status was given, so don't process the command
        return;
    } // Else, process the command

    if (!dc_maybe_stuff_boolean (&Show_cpu)) {
        // Nothing passed, so toggle
        Show_cpu = !Show_cpu;
    } // Else, value was set/cleared by user
}

#endif

DCF (use_joy_mouse, "Makes joystick move mouse cursor") {
    bool process = true;

    if (dc_optional_string_either ("help", "--help")) {
        dc_printf (
            "Usage: use_joy_mouse [bool]\nSets use_joy_mouse to true or "
            "false.  If nothing passed, then toggles it.\n");
        process = false;
    }

    if (dc_optional_string_either ("status", "--status") ||
        dc_optional_string_either ("?", "--?")) {
        dc_printf (
            "use_joy_mouse is %s\n", (Use_joy_mouse ? "TRUE" : "FALSE"));
        process = false;
    }

    if (!process) { return; }

    if (!dc_maybe_stuff_boolean (&Use_joy_mouse)) {
        // Nothing passed, so toggle
        Use_joy_mouse = !Use_joy_mouse;
    } // Else, value was set/cleared by user

    fs2::registry::write ("Default.JoystickMovesCursor", Use_joy_mouse);
}

DCF_BOOL (palette_flash, Use_palette_flash);

int Use_low_mem = 0;

DCF (low_mem, "Uses low memory settings regardless of RAM") {
    bool process = true;

    if (dc_optional_string_either ("help", "--help")) {
        dc_printf (
            "Usage: low_mem [bool]\nSets low_mem to true or false.  If "
            "nothing passed, then toggles it.\n");
        process = false;
    }

    if (dc_optional_string_either ("status", "--status") ||
        dc_optional_string_either ("?", "--?")) {
        dc_printf ("low_mem is %s\n", (Use_low_mem ? "TRUE" : "FALSE"));
        process = false;
    }

    if (!process) { return; }

    if (!dc_maybe_stuff_boolean (&Use_low_mem)) {
        // Nothing passed, so toggle
        Use_low_mem = !Use_low_mem;
    } // Else, value was set/cleared by user

    fs2::registry::write ("Default.LowMem", Use_low_mem);
}

#ifndef NDEBUG

DCF (force_fullscreen, "Forces game to startup in fullscreen mode") {
    bool process = true;
    if (dc_optional_string_either ("help", "--help")) {
        dc_printf (
            "Usage: low_mem [bool]\nSets low_mem to true or false.  If "
            "nothing passed, then toggles it.\n");
        process = false;
    }

    if (dc_optional_string_either ("status", "--status") ||
        dc_optional_string_either ("?", "--?")) {
        dc_printf (
            "low_mem is %s\n", (Use_fullscreen_at_startup ? "TRUE" : "FALSE"));
        process = false;
    }

    if (!process) { return; }

    if (dc_maybe_stuff_boolean (&Use_fullscreen_at_startup)) {
        // Nothing passed, so toggle
        Use_fullscreen_at_startup = !Use_fullscreen_at_startup;
    } // Else, value was set/cleared by user

    fs2::registry::write ("Default.ForceFullscreen", Use_fullscreen_at_startup);
}
#endif

int Framerate_delay = 0;

float FreeSpace_gamma = 1.0f;

DCF (gamma, "Sets and saves Gamma Factor") {
    if (dc_optional_string_either ("help", "--help")) {
        dc_printf ("Usage: gamma <float>\n");
        dc_printf (
            "Sets gamma in range 1-3, no argument resets to default 1.2\n");
        return;
    }

    if (dc_optional_string_either ("status", "--status") ||
        dc_optional_string_either ("?", "--?")) {
        dc_printf ("Gamma = %.2f\n", FreeSpace_gamma);
        return;
    }

    if (!dc_maybe_stuff_float (&FreeSpace_gamma)) {
        dc_printf ("Gamma reset to 1.0f\n");
        FreeSpace_gamma = 1.0f;
    }
    if (FreeSpace_gamma < 0.1f) { FreeSpace_gamma = 0.1f; }
    else if (FreeSpace_gamma > 5.0f) {
        FreeSpace_gamma = 5.0f;
    }

    gr_set_gamma (FreeSpace_gamma);

    fs2::registry::write ("Default.Gamma", FreeSpace_gamma);
}

/**
 * Game initialisation
 */
void game_init () {
    int s1, e1;
    char whee[MAX_PATH_LEN];

    Game_current_mission_filename[0] = 0;

    // Moved from rand32, if we're gonna break, break immediately.
    ASSERT (RAND_MAX == 0x7fff || RAND_MAX >= 0x7ffffffd);

    Framerate_delay = 0;

    // encrypt stuff
    encrypt_init ();

    // Initialize the timer before the os
    timer_init ();

    //
    // Set window and quit handlers:
    //
    fs2::os::init ();

    II << "FreeSpace 2 v" << FS_VERSION_FULL;

    memset (whee, 0, sizeof (whee));

    getcwd (whee, MAX_PATH_LEN - 1);

    strcat (whee, "/");
    strcat (whee, EXE_FNAME);

    // Initialize the libraries
    s1 = timer_get_milliseconds ();

    if (cfile_init (
            whee, NULL)) { // initialize before calling any cfopen stuff!!!
        exit (1);
    }

    e1 = timer_get_milliseconds ();

    mod_table_init (); // load in all the mod dependent settings

    // initialize localization module. Make sure this is done AFTER initialzing
    // OS.
    lcl_init (detect_lang ());
    lcl_xstr_init ();

    // verify that he has a valid ships.tbl (will Game_ships_tbl_valid if so)
    verify_ships_tbl ();

    // verify that he has a valid weapons.tbl
    verify_weapons_tbl ();

    Use_joy_mouse = 0;
    Use_low_mem = fs2::registry::read ("Default.LowMem", 0);

#ifndef NDEBUG
    Use_fullscreen_at_startup =
        fs2::registry::read ("Default.ForceFullscreen", 1);
#endif

    // change FPS cap if told to do so (for those who can't use vsync or where
    // vsync isn't enough)
    uint max_fps = 0;
    if ((max_fps = fs2::registry::read ("Default.MaxFPS", 0)) != 0) {
        if ((max_fps > 15) && (max_fps < 120)) {
            Framerate_cap = (int)max_fps;
        }
    }

    Asteroids_enabled = 1;

    snd_init ();

    auto sdlGraphicsOperations = std::make_unique< SDLGraphicsOperations > ();

    if (gr_init (std::move (sdlGraphicsOperations)) == false) {
        EE << "error intializing graphics!";
        exit (1);
    }

    // This needs to happen after graphics initialization
    tracing::init ();

    // D3D's gamma system now works differently. 1.0 is the default value
    FreeSpace_gamma = fs2::registry::read ("Default.GammaD3D", 1.f);

    font::init (); // loads up all fonts

    gr_set_gamma (FreeSpace_gamma);
    game_title_screen_display ();

    // If less than 48MB of RAM, use low memory model.
    bm_set_low_mem (0); // Use all frames of bitmaps

    // WMC - Initialize my new GUI system
    // This may seem scary, but it should take up 0 processing time and very
    // little memory as long as it's not being used. Otherwise, it just keeps
    // the parsed interface.tbl in memory.
    GUI_system.ParseClassInfo ("interface.tbl");

    particle::ParticleManager::init ();

    iff_init (); // Goober5000 - this must be done even before species_defs :p
    species_init (); // Load up the species defs - this needs to be done FIRST
                     // -- Kazan

    brief_parse_icon_tbl ();

    hud_init_comm_orders (); // Goober5000

    control_config_common_init (); // sets up localization stuff in the control
                                   // config

    parse_rank_tbl ();
    parse_medal_tbl ();

    cutscene_init ();
    key_init ();
    mouse_init ();
    gamesnd_parse_soundstbl ();

    gameseq_init ();

    II << "Freespace2 Open Source Mission Log";

    io::joystick::init ();

    player_controls_init ();
    model_init ();

    event_music_init ();

    // initialize alpha colors
    // CommanderDJ: try with colors.tbl first, then use the old way if that
    // doesn't work
    alpha_colors_init ();

    decals::initialize ();

    obj_init ();
    mflash_game_init ();
    armor_init ();
    ai_init ();
    ai_profiles_init (); // Goober5000
    weapon_init ();
    glowpoint_init ();
    ship_init (); // read in ships.tbl

    player_init ();
    mission_campaign_init (); // load in the default campaign
    anim_init ();
    context_help_init ();
    techroom_intel_init (); // parse species.tbl, load intel info
    hud_positions_init ();  // Setup hud positions

    asteroid_init ();
    mission_brief_common_init (); // Mark all the briefing structures as empty.

    neb2_init (); // fullneb stuff
    nebl_init ();
    stars_init ();
    ssm_init ();
    player_tips_init (); // helpful tips
    beam_init ();

    // load the list of pilot pic filenames (for barracks and pilot select
    // popup quick reference)
    pilot_load_pic_list ();
    pilot_load_squad_pic_list ();

    // Load the default cursor and enable it
    io::mouse::Cursor* cursor =
        io::mouse::CursorManager::get ()->loadCursor ("cursor", true);
    if (cursor) {
        io::mouse::CursorManager::get ()->setCurrentCursor (cursor);
    }

    if (!Cmdline_reparse_mainhall) { main_hall_table_init (); }

    // This needs to be done after the dynamic SEXP init so that our
    // documentation contains the dynamic sexps
    if (Cmdline_output_sexp_info) { output_sexps ("sexps.html"); }

    Viewer_mode = 0;
    Game_paused = 0;

    game_title_screen_close ();

    libs::ffmpeg::initialize ();

    WARNINGF (LOCATION, "Ships.tbl is : %s",Game_ships_tbl_valid ? "VALID" : "INVALID!!!!");
    WARNINGF (LOCATION, "Weapons.tbl is : %s",Game_weapons_tbl_valid ? "VALID" : "INVALID!!!!");

    WARNINGF (LOCATION, "cfile_init() took %d", e1 - s1);

    // if we are done initializing, start showing the cursor
    io::mouse::CursorManager::get ()->showCursor (true);

    mouse_set_pos (gr_screen.max_w / 2, gr_screen.max_h / 2);
}

char transfer_text[128];

float Start_time = 0.0f;

float Framerate = 0.0f;

#ifndef NDEBUG
float Timing_total = 0.0f;
float Timing_render2 = 0.0f;
float Timing_render3 = 0.0f;
float Timing_flip = 0.0f;
float Timing_clear = 0.0f;
#endif

MONITOR (NumPolysDrawn)
MONITOR (NumPolys)
MONITOR (NumVerts)
MONITOR (BmpUsed)
MONITOR (BmpNew)

void game_get_framerate () {
    if (frame_int == -1) {
        for (int i = 0; i < FRAME_FILTER; i++) frametimes[i] = 0.0f;

        frametotal = 0.0f;
        frame_int = 0;
    }

    frametotal -= frametimes[frame_int];
    frametotal += flRealframetime;
    frametimes[frame_int] = flRealframetime;
    frame_int = (frame_int + 1) % FRAME_FILTER;

    if (frametotal != 0.0f) {
        if (Framecount >= FRAME_FILTER)
            Framerate = FRAME_FILTER / frametotal;
        else
            Framerate = Framecount / frametotal;
    }

    Framecount++;
}

/**
 * Show FPS within game
 */
void game_show_framerate () {
    float cur_time;
    int line_height = gr_get_font_height () + 1;

    cur_time = f2fl (timer_get_approx_seconds ());
    if (cur_time - Start_time > 30.0f) {
        WARNINGF (LOCATION,"%i frames executed in %7.3f seconds, %7.3f frames per second.",Framecount, cur_time - Start_time,Framecount / (cur_time - Start_time));
        Start_time += 1000.0f;
    }

#ifdef WMC
    // WMC - this code spits out the target of all turrets
    if ((Player_ai->target_objnum != -1) &&
        (Objects[Player_ai->target_objnum].type == OBJ_SHIP)) {
        // Debug crap
        int t = 0;
        ship_subsys* pss;

        gr_set_color_fast (&HUD_color_debug);

        object* objp = &Objects[Player_ai->target_objnum];
        for (pss = GET_FIRST (&shipp->subsys_list);
             pss != END_OF_LIST (&shipp->subsys_list); pss = GET_NEXT (pss)) {
            if (pss->system_info->type == SUBSYSTEM_TURRET) {
                if (pss->turret_enemy_objnum == -1)
                    gr_printf_no_resize (
                        gr_screen.center_offset_x + 10,
                        gr_screen.center_offset_y + (t * line_height),
                        "Turret %d: <None>", t);
                else if (Objects[pss->turret_enemy_objnum].type == OBJ_SHIP)
                    gr_printf_no_resize (
                        gr_screen.center_offset_x + 10,
                        gr_screen.center_offset_y + (t * line_height),
                        "Turret %d: %s", t,
                        Ships[Objects[pss->turret_enemy_objnum].instance]
                            .ship_name);
                else
                    gr_printf_no_resize (
                        gr_screen.center_offset_x + 10,
                        gr_screen.center_offset_y + (t * line_height),
                        "Turret %d: <Object %d>", t, pss->turret_enemy_objnum);

                t++;
            }
        }
    }
#endif

    if (Show_framerate || Cmdline_frame_profile || Cmdline_bmpman_usage) {
        gr_set_color_fast (&HUD_color_debug);

        if (Cmdline_frame_profile) {
            gr_string (
                gr_screen.center_offset_x + 20,
                gr_screen.center_offset_y + 100 + line_height,
                tracing::get_frame_profile_output ().c_str (), GR_RESIZE_NONE);
        }

        if (Show_framerate) {
            if (frametotal != 0.0f)
                gr_printf_no_resize (
                    gr_screen.center_offset_x + 20,
                    gr_screen.center_offset_y + 100, "FPS: %0.1f", Framerate);
            else
                gr_string (
                    gr_screen.center_offset_x + 20,
                    gr_screen.center_offset_y + 100, "FPS: ?", GR_RESIZE_NONE);
        }

        if (Cmdline_bmpman_usage) {
            gr_printf_no_resize (
                gr_screen.center_offset_x + 20,
                gr_screen.center_offset_y + 100 - line_height, "BMPMAN: %d/%d",
                bmpman_count_bitmaps (), bmpman_count_available_slots ());
        }
    }

#ifndef NDEBUG
    if (Debug_dump_frames) return;
#endif

    // possibly show control checking info
    control_check_indicate ();

#ifndef NDEBUG
    if (Show_cpu == 1) {
        int sx, sy;
        sx = gr_screen.center_offset_x + gr_screen.center_w - 154;
        sy = gr_screen.center_offset_y + 15;

        gr_set_color_fast (&HUD_color_debug);

        gr_printf_no_resize (sx, sy, NOX ("DMA: %s"), transfer_text);
        sy += line_height;
        gr_printf_no_resize (sx, sy, NOX ("POLYP: %d"), modelstats_num_polys);
        sy += line_height;
        gr_printf_no_resize (
            sx, sy, NOX ("POLYD: %d"), modelstats_num_polys_drawn);
        sy += line_height;
        gr_printf_no_resize (sx, sy, NOX ("VERTS: %d"), modelstats_num_verts);
        sy += line_height;

        {
            extern int Num_pairs; // Number of object pairs that were checked.
            gr_printf_no_resize (sx, sy, NOX ("PAIRS: %d"), Num_pairs);
            sy += line_height;

            extern int Num_pairs_checked; // What percent of object pairs were
                                          // checked.
            gr_printf_no_resize (sx, sy, NOX ("FVI: %d"), Num_pairs_checked);
            sy += line_height;
            Num_pairs_checked = 0;
        }

        gr_printf_no_resize (sx, sy, NOX ("Snds: %d"), snd_num_playing ());
        sy += line_height;

        if (Timing_total > 0.01f) {
            gr_printf_no_resize (
                sx, sy, NOX ("CLEAR: %.0f%%"),
                Timing_clear * 100.0f / Timing_total);
            sy += line_height;
            gr_printf_no_resize (
                sx, sy, NOX ("REND2D: %.0f%%"),
                Timing_render2 * 100.0f / Timing_total);
            sy += line_height;
            gr_printf_no_resize (
                sx, sy, NOX ("REND3D: %.0f%%"),
                Timing_render3 * 100.0f / Timing_total);
            sy += line_height;
            gr_printf_no_resize (
                sx, sy, NOX ("FLIP: %.0f%%"),
                Timing_flip * 100.0f / Timing_total);
            sy += line_height;
            gr_printf_no_resize (
                sx, sy, NOX ("GAME: %.0f%%"),
                (Timing_total - (Timing_render2 + Timing_render3 +
                                 Timing_flip + Timing_clear)) *
                    100.0f / Timing_total);
            sy += line_height;
        }
    }

    if (Show_player_pos) {
        int sx, sy;
        sx = gr_screen.center_offset_x + 320;
        sy = gr_screen.center_offset_y + 100;
        gr_printf_no_resize (
            sx, sy, NOX ("Player Pos: (%d,%d,%d)"),
            int (Player_obj->pos.xyz.x), int (Player_obj->pos.xyz.y),
            int (Player_obj->pos.xyz.z));
    }

    MONITOR_INC (NumPolys, modelstats_num_polys);
    MONITOR_INC (NumPolysDrawn, modelstats_num_polys_drawn);
    MONITOR_INC (NumVerts, modelstats_num_verts);

    modelstats_num_polys = 0;
    modelstats_num_polys_drawn = 0;
    modelstats_num_verts = 0;
    modelstats_num_sortnorms = 0;
#endif
}

void game_show_eye_pos (camid cid) {
    if (!Cmdline_show_pos) return;

    if (!cid.isValid ()) return;

    camera* cam = cid.getCamera ();
    vec3d cam_pos = vmd_zero_vector;
    matrix cam_orient = vmd_identity_matrix;
    cam->get_info (&cam_pos, &cam_orient);

    // Do stuff
    int font_height = 2 * gr_get_font_height ();
    angles_t rot_angles;

    gr_set_color_fast (&HUD_color_debug);

    // Position
    gr_printf_no_resize (
        gr_screen.center_offset_x + 20,
        gr_screen.center_offset_y + 100 - font_height, "X:%f Y:%f Z:%f",
        cam_pos.xyz.x, cam_pos.xyz.y, cam_pos.xyz.z);
    font_height -= font_height / 2;

    // Orientation
    vm_extract_angles_matrix (&rot_angles, &cam_orient);
    rot_angles.p *= (180 / PI);
    rot_angles.b *= (180 / PI);
    rot_angles.h *= (180 / PI);
    gr_printf_no_resize (
        gr_screen.center_offset_x + 20,
        gr_screen.center_offset_y + 100 - font_height, "Xr:%f Yr:%f Zr:%f",
        rot_angles.p, rot_angles.b, rot_angles.h);
}

/**
 * Show the time remaining in a mission.  Used only when the end-mission
 * sexpression is used
 *
 * mission_end_time is a global from missionparse.cpp that contains the mission
 * time at which the mission should end (in fixed seconds).  There is code in
 * missionparse.cpp which actually handles checking how much time is left.
 */
void game_show_time_left () {
    int diff;

    if (Mission_end_time == -1) return;

    diff = f2i (Mission_end_time - Missiontime);
    // be sure to bash to 0.  diff could be negative on frame that we quit
    // mission
    if (diff < 0) diff = 0;

    hud_set_default_color ();
    gr_printf_no_resize (
        gr_screen.center_offset_x + 5, gr_screen.center_offset_y + 40,
        XSTR ("Mission time remaining: %d seconds", 179), diff);
}

//========================================================================================
//=================== NEW DEBUG CONSOLE COMMANDS TO REPLACE OLD DEBUG PAUSE
// MENU =========
//========================================================================================

#ifndef NDEBUG

DCF (ai_pause, "Pauses ai") {
    bool process = true;

    if (dc_optional_string_either ("help", "--help")) {
        dc_printf (
            "Usage: ai_paused [bool]\nSets ai_paused to true or false.  If "
            "nothing passed, then toggles it.\n");
        process = false;
    }

    if (dc_optional_string_either ("status", "--status") ||
        dc_optional_string_either ("?", "--?")) {
        dc_printf ("ai_paused is %s\n", (ai_paused ? "TRUE" : "FALSE"));
        process = false;
    }

    if (!process) { return; }

    if (!dc_maybe_stuff_boolean (&ai_paused)) { ai_paused = !ai_paused; }

    if (ai_paused) { obj_init_all_ships_physics (); }
}

DCF (single_step, "Enables single step mode.") {
    bool process = true;

    if (dc_optional_string_either ("help", "--help")) {
        dc_printf (
            "Usage: game_single_step [bool]\nEnables or disables single-step "
            "mode.  If nothing passed, then toggles it.\nSingle-step mode "
            "will freeze the game, and will advance frame by frame with each "
            "key press\n");
        process = false;
    }

    if (dc_optional_string_either ("status", "--status") ||
        dc_optional_string_either ("?", "--?")) {
        dc_printf ("ai_paused is %s\n", (game_single_step ? "TRUE" : "FALSE"));
        process = false;
    }

    if (!process) { return; }

    if (!dc_maybe_stuff_boolean (&game_single_step)) {
        game_single_step = !game_single_step;
    }

    last_single_step = 0; // Make so single step waits a frame before stepping
}

DCF_BOOL (physics_pause, physics_paused)
DCF_BOOL (ai_rendering, Ai_render_debug_flag)
DCF_BOOL (ai_firing, Ai_firing_enabled)

// Create some simple aliases to these commands...
debug_command dc_s ("s", "shortcut for single_step", dcf_single_step);
debug_command dc_p ("p", "shortcut for physics_pause", dcf_physics_pause);
debug_command dc_r ("r", "shortcut for ai_rendering", dcf_ai_rendering);
debug_command dc_f ("f", "shortcut for ai_firing", dcf_ai_firing);
debug_command dc_a ("a", "shortcut for ai_pause", dcf_ai_pause);
#endif

//========================================================================================
//========================================================================================

void game_training_pause_do () {
    int key;

    key = game_check_key ();
    if (key > 0) { gameseq_post_event (GS_EVENT_PREVIOUS_STATE); }

    gr_flip ();
}

void game_increase_skill_level () {
    Game_skill_level++;
    if (Game_skill_level >= NUM_SKILL_LEVELS) { Game_skill_level = 0; }
}

int Player_died_time;

int View_percent = 100;

DCF (view, "Sets the percent of the 3d view to render.") {
    bool process = true;
    int value;

    if (dc_optional_string_either ("help", "--help")) {
        dc_printf (
            "Usage: view [n]\nwhere n is percent of view to show (5-100).\n");
        process = false;
    }

    if (dc_optional_string_either ("status", "--status") ||
        dc_optional_string_either ("?", "--?")) {
        dc_printf ("View is set to %d%%\n", View_percent);
        process = false;
    }

    if (!process) { return; }

    dc_stuff_int (&value);
    if ((value >= 5) && (value <= 100)) { View_percent = value; }
    else {
        dc_printf ("Error: Outside legal range [5 - 100]");
    }
}

/**
 * Set the clip region for the 3d rendering window
 */
void game_reset_view_clip () {
    Cutscene_bar_flags = CUB_NONE;
    Cutscene_delta_time = 1.0f;
    Cutscene_bars_progress = 1.0f;
}

void game_set_view_clip (float /*frametime*/) {
    if ((Game_mode & GM_DEAD) || (supernova_active () >= 2)) {
        // Set the clip region for the letterbox "dead view"
        int yborder = gr_screen.max_h / 4;

        if (g3_in_frame () == 0) {
            // Ensure that the bars are black
            gr_set_color (0, 0, 0);
            gr_set_bitmap (0); // Valathil - Don't ask me why this has to be
                               // here but otherwise the black bars don't draw
            gr_rect (0, 0, gr_screen.max_w, yborder, GR_RESIZE_NONE);
            gr_rect (
                0, gr_screen.max_h - yborder, gr_screen.max_w, yborder,
                GR_RESIZE_NONE);
        }
        else {
            // Numeric constants encouraged by J "pig farmer" S, who shall
            // remain semi-anonymous.
            // J.S. I've changed my ways!! See the new "no constants" code!!!
            gr_set_clip (
                0, yborder, gr_screen.max_w, gr_screen.max_h - yborder * 2,
                GR_RESIZE_NONE);
        }
    }
    else {
        // Set the clip region for normal view
        if (View_percent >= 100) { gr_reset_clip (); }
        else {
            int xborder, yborder;

            if (View_percent < 5) { View_percent = 5; }

            float fp = float (View_percent) / 100.0f;
            int fi = int (sqrtf (fp) * 100.0f);
            if (fi > 100) fi = 100;

            xborder = (gr_screen.max_w * (100 - fi)) / 200;
            yborder = (gr_screen.max_h * (100 - fi)) / 200;

            gr_set_clip (
                xborder, yborder, gr_screen.max_w - xborder * 2,
                gr_screen.max_h - yborder * 2, GR_RESIZE_NONE);
        }
    }
}

void show_debug_stuff () {
    int i;
    int laser_count = 0, missile_count = 0;

    for (i = 0; i < MAX_OBJECTS; i++) {
        if (Objects[i].type == OBJ_WEAPON) {
            if (Weapon_info[Weapons[Objects[i].instance].weapon_info_index]
                    .subtype == WP_LASER) {
                laser_count++;
            }
            else if (
                Weapon_info[Weapons[Objects[i].instance].weapon_info_index]
                    .subtype == WP_MISSILE) {
                missile_count++;
            }
        }
    }

    WARNINGF (LOCATION, "Frame: %i Lasers: %4i, Missiles: %4i", Framecount,laser_count, missile_count);
}

extern int Tool_enabled;
int tst = 0;
int tst_time = 0;
int tst_big = 0;
vec3d tst_pos;
int tst_bitmap = -1;
float tst_x, tst_y;
float tst_offset, tst_offset_total;
int tst_mode;
int tst_stamp;
void game_tst_frame_pre () {
    // start tst
    if (tst == 3) {
        tst = 0;

        // screen position
        vertex v;
        g3_rotate_vertex (&v, &tst_pos);
        g3_project_vertex (&v);

        // offscreen
        if (!((v.screen.xyw.x >= 0) && (v.screen.xyw.x <= gr_screen.max_w) &&
              (v.screen.xyw.y >= 0) && (v.screen.xyw.y <= gr_screen.max_h))) {
            return;
        }

        // big ship? always tst
        if (tst_big) {
            // within 3000 meters
            if (vm_vec_dist_quick (&tst_pos, &Eye_position) <= 3000.0f) {
                tst = 2;
            }
        }
        else {
            // within 300 meters
            if ((vm_vec_dist_quick (&tst_pos, &Eye_position) <= 300.0f) &&
                ((tst_time == 0) || ((time (NULL) - tst_time) >= 10))) {
                tst = 2;
            }
        }
    }
}
void game_tst_frame () {
    int left = 0;

    if (!Tool_enabled) { return; }

    // setup tst
    if (tst == 2) {
        tst_time = (int)time (NULL);

        // load the tst bitmap
        switch ((int)fs2::prng::randf (0, 0.0f, 3.0)) {
        case 0:
            tst_bitmap = bm_load ("ig_jim");
            left = 1;
            WARNINGF (LOCATION, "TST 0");
            break;

        case 1:
            tst_bitmap = bm_load ("ig_kan");
            left = 0;
            WARNINGF (LOCATION, "TST 1");
            break;

        case 2:
            tst_bitmap = bm_load ("ig_jim");
            left = 1;
            WARNINGF (LOCATION, "TST 2");
            break;

        default:
            tst_bitmap = bm_load ("ig_kan");
            left = 0;
            WARNINGF (LOCATION, "TST 3");
            break;
        }

        if (tst_bitmap < 0) {
            tst = 0;
            return;
        }

        // get the tst bitmap dimensions
        int w, h;
        bm_get_info (tst_bitmap, &w, &h);

        // tst y
        tst_y = fs2::prng::randf (0, 0.0f, (float)gr_screen.max_h - h);

        snd_play (gamesnd_get_interface_sound (InterfaceSounds::VASUDAN_BUP));

        // tst x and direction
        tst_mode = 0;
        if (left) {
            tst_x = (float)-w;
            tst_offset_total = (float)w;
            tst_offset = (float)w;
        }
        else {
            tst_x = (float)gr_screen.max_w;
            tst_offset_total = (float)-w;
            tst_offset = (float)w;
        }

        tst = 1;
    }

    // run tst
    if (tst == 1) {
        float diff = (tst_offset_total / 0.5f) * flFrametime;

        // move the bitmap
        if (tst_mode == 0) {
            tst_x += diff;

            tst_offset -= fabsf (diff);
        }
        else if (tst_mode == 2) {
            tst_x -= diff;

            tst_offset -= fabsf (diff);
        }

        // draw the bitmap
        gr_set_bitmap (tst_bitmap);
        gr_bitmap ((int)tst_x, (int)tst_y, GR_RESIZE_NONE);

        if (tst_mode == 1) {
            if (timestamp_elapsed_safe (tst_stamp, 1100)) { tst_mode = 2; }
        }
        else {
            // if we passed the switch point
            if (tst_offset <= 0.0f) {
                // switch modes
                switch (tst_mode) {
                case 0:
                    tst_mode = 1;
                    tst_stamp = timestamp (1000);
                    tst_offset = fabsf (tst_offset_total);
                    break;

                case 2: tst = 0; return;
                }
            }
        }
    }
}
void game_tst_mark (object* objp, ship* shipp) {
    ship_info* sip;

    if (!Tool_enabled) { return; }

    // bogus
    if ((objp == NULL) || (shipp == NULL) || (shipp->ship_info_index < 0) ||
        (shipp->ship_info_index >= static_cast< int > (Ship_info.size ()))) {
        return;
    }
    sip = &Ship_info[shipp->ship_info_index];

    // already tst
    if (tst) { return; }

    tst_pos = objp->pos;
    if (sip->is_big_or_huge ()) { tst_big = 1; }
    tst = 3;
}

extern void render_shields ();

void player_repair_frame (float frametime) {
    if (Player_obj  &&  Player_obj->type == OBJ_SHIP &&
        Player_ship && !Player_ship->flags[Ship::Ship_Flags::Dying]) {
        ai_do_repair_frame (
            Player_obj, &Ai_info[Ships[Player_obj->instance].ai_index],
            frametime);
    }
}

#define NUM_FRAMES_TEST 300
#define NUM_MIXED_SOUNDS 16

void do_timing_test (float frame_time) {
    static int framecount = 0;
    static int test_running = 0;
    static float test_time = 0.0f;

    static sound_handle snds[NUM_MIXED_SOUNDS];
    int i;

    if (test_running) {
        framecount++;
        test_time += frame_time;
        if (framecount >= NUM_FRAMES_TEST) {
            test_running = 0;
            WARNINGF (LOCATION, "%d frames took %.3f seconds", NUM_FRAMES_TEST,test_time);
            for (i = 0; i < NUM_MIXED_SOUNDS; i++) snd_stop (snds[i]);
        }
    }

    if (Test_begin == 1) {
        framecount = 0;
        test_running = 1;
        test_time = 0.0f;
        Test_begin = 0;

        for (i = 0; i < NUM_MIXED_SOUNDS; i++)
            snds[i] = sound_handle::invalid ();

        // start looping digital sounds
        for (i = 0; i < NUM_MIXED_SOUNDS; i++)
            snds[i] = snd_play_looping (
                gamesnd_get_game_sound (gamesnd_id (i)), 0.0f, -1, -1);
    }
}

DCF (dcf_fov, "Change the field of view of the main camera") {
    camera* cam = Main_camera.getCamera ();
    bool process = true;
    float value;

    if (dc_optional_string_either ("help", "--help")) {
        dc_printf (
            "Usage: fov [factor]\nFactor is the zoom factor btwn .25 and "
            "1.25\nNo parameter resets it to default.\n");
        process = false;
    }

    if (dc_optional_string_either ("status", "--status") ||
        dc_optional_string_either ("?", "--?")) {
        if (cam == NULL) { dc_printf ("Camera unavailable."); }
        else {
            dc_printf (
                "Zoom factor set to %6.3f (original = 0.5, John = 0.75)\n",
                cam->get_fov ());
        }

        process = false;
    }

    if ((cam == NULL) || (!process)) { return; }

    if (!dc_maybe_stuff_float (&value)) {
        // No value passed, use default
        cam->set_fov (VIEWER_ZOOM_DEFAULT);
    }
    else {
        // Value passed, Clamp it to valid values
        if (value < 0.25f) {
            value = 0.25f;
            dc_printf ("Zoom factor clamped to 0.25\n");
        }
        else if (value > 1.25f) {
            value = 1.25f;
            dc_printf ("Zoom factor clamped to 1.25\n");
        }
        else {
            dc_printf ("Zoom factor set to %6.3f\n", value);
        }

        cam->set_fov (value);
    }
}

DCF (framerate_cap, "Sets the framerate cap") {
    bool process = true;

    if (dc_optional_string_either ("help", "--help")) {
        dc_printf (
            "Usage: framerate_cap [n]\nwhere n is the frames per second to "
            "cap framerate at.\n");
        dc_printf (
            "If n is 0 or omitted, then the framerate cap is removed\n");
        dc_printf ("[n] must be from 1 to 120.\n");
        process = false;
    }

    if (dc_optional_string_either ("status", "--status") ||
        dc_optional_string_either ("?", "--?")) {
        if (Framerate_cap) {
            dc_printf ("Framerate cap is set to %d fps\n", Framerate_cap);
        }
        else {
            dc_printf ("There is no framerate cap currently active.\n");
        }

        process = false;
    }

    if (!process) { return; }

    if (!dc_maybe_stuff_int (&Framerate_cap)) { Framerate_cap = 0; }

    if ((Framerate_cap < 0) || (Framerate_cap > 120)) {
        dc_printf ("Illegal value for framerate cap. (Must be from 1-120) \n");
        Framerate_cap = 0;
    }

    if (Framerate_cap == 0) { dc_printf ("Framerate cap disabled"); }
    else {
        dc_printf ("Framerate cap is set to %d fps\n", Framerate_cap);
    }
}

#define MIN_DIST_TO_DEAD_CAMERA 50.0f
int Show_viewing_from_self = 0;

void say_view_target () {
    object* view_target;

    if ((Viewer_mode & VM_OTHER_SHIP) && (Player_ai->target_objnum != -1))
        view_target = &Objects[Player_ai->target_objnum];
    else
        view_target = Player_obj;

    if (Game_mode & GM_DEAD) {
        if (Player_ai->target_objnum != -1)
            view_target = &Objects[Player_ai->target_objnum];
    }

    if (!(Game_mode & GM_DEAD_DIED) &&
        ((Game_mode & (GM_DEAD_BLEW_UP)) ||
         ((Last_view_target != NULL) && (Last_view_target != view_target)))) {
        if (view_target != Player_obj) {
            char view_target_name[128] = "";
            switch (Objects[Player_ai->target_objnum].type) {
            case OBJ_SHIP:
                if (Ships[Objects[Player_ai->target_objnum].instance]
                        .flags[Ship::Ship_Flags::Hide_ship_name]) {
                    strcpy (view_target_name, "targeted ship");
                }
                else {
                    strcpy (
                        view_target_name,
                        Ships[Objects[Player_ai->target_objnum].instance]
                            .get_display_string ());
                }
                break;
            case OBJ_WEAPON:
                strcpy (
                    view_target_name,
                    Weapon_info
                        [Weapons[Objects[Player_ai->target_objnum].instance]
                             .weapon_info_index]
                            .get_display_string ());
                Viewer_mode &= ~VM_OTHER_SHIP;
                break;
            case OBJ_JUMP_NODE: {
                strcpy (view_target_name, XSTR ("jump node", 184));
                Viewer_mode &= ~VM_OTHER_SHIP;
                break;
            }
            case OBJ_DEBRIS: {
                strcpy (view_target_name, "Debris");
                Viewer_mode &= ~VM_OTHER_SHIP;
                break;
            }

            default: ASSERT (0); break;
            }

            end_string_at_first_hash_symbol (view_target_name);
            if (strlen (view_target_name)) {
                hud_set_iff_color (&Objects[Player_ai->target_objnum], 1);
                HUD_fixed_printf (
                    0.0f, gr_screen.current_color,
                    XSTR ("Viewing %s%s\n", 185),
                    (Viewer_mode & VM_OTHER_SHIP) ? XSTR ("from ", 186) : "",
                    view_target_name);
                Show_viewing_from_self = 1;
            }
        }
        else {
            if (Show_viewing_from_self) {
                color col;
                gr_init_color (&col, 0, 255, 0);

                HUD_fixed_printf (
                    2.0f, col, "%s", XSTR ("Viewing from self\n", 188));
            }
        }
    }

    Last_view_target = view_target;
}

float Game_hit_x = 0.0f;
float Game_hit_y = 0.0f;

// Reset at the beginning of each frame
void game_whack_reset () {
    Game_hit_x = 0.0f;
    Game_hit_y = 0.0f;
}

// Apply a 2d whack to the player
void game_whack_apply (float x, float y) {
    // Do some force feedback
    joy_ff_play_dir_effect (x * 80.0f, y * 80.0f);

    // Move the eye
    Game_hit_x += x;
    Game_hit_y += y;

    // mprintf(( "WHACK = %.1f, %.1f\n", Game_hit_x, Game_hit_y ));
}

// call to apply a "shudder"
void game_shudder_apply (int time, float intensity) {
    Game_shudder_time = timestamp (time);
    Game_shudder_total = time;
    Game_shudder_intensity = intensity;
}

float get_shake (float intensity, int decay_time, int max_decay_time) {
    int r = myrand ();

    float shake = intensity * (float)(r - RAND_MAX_2) * RAND_MAX_1f;

    if (decay_time >= 0) {
        ASSERT (max_decay_time > 0);
        shake *=
            (0.5f - fabsf (0.5f - (float)decay_time / (float)max_decay_time));
    }

    return shake;
}

#define FF_SCALE 10000
extern int Wash_on;
extern float sn_shudder;
void apply_view_shake (matrix* eye_orient) {
    angles_t tangles;
    tangles.p = 0.0f;
    tangles.h = 0.0f;
    tangles.b = 0.0f;

    // do shakes that only affect the HUD
    if (Viewer_obj == Player_obj) {
        physics_info* pi = &Player_obj->phys_info;

        // Make eye shake due to afterburner
        if (!timestamp_elapsed (pi->afterburner_decay)) {
            tangles.p += get_shake (
                0.07f, timestamp_until (pi->afterburner_decay),
                ABURN_DECAY_TIME);
            tangles.h += get_shake (
                0.07f, timestamp_until (pi->afterburner_decay),
                ABURN_DECAY_TIME);
        }

        // Make eye shake due to engine wash
        if (Player_obj->type == OBJ_SHIP &&
            (Ships[Player_obj->instance].wash_intensity > 0) && Wash_on) {
            float wash_intensity = Ships[Player_obj->instance].wash_intensity;

            tangles.p += get_shake (0.07f * wash_intensity, -1, 0);
            tangles.h += get_shake (0.07f * wash_intensity, -1, 0);

            // play the force feedback effect
            vec3d rand_vec;
            vm_vec_rand_vec_quick (&rand_vec);
            joy_ff_play_dir_effect (
                FF_SCALE * wash_intensity * rand_vec.xyz.x,
                FF_SCALE * wash_intensity * rand_vec.xyz.y);
        }

        // Make eye shake due to shuddering
        if (Game_shudder_time != -1) {
            if (timestamp_elapsed (Game_shudder_time)) {
                Game_shudder_time = -1;
            }
            else {
                tangles.p += get_shake (
                    Game_shudder_intensity * 0.005f,
                    timestamp_until (Game_shudder_time), Game_shudder_total);
                tangles.h += get_shake (
                    Game_shudder_intensity * 0.005f,
                    timestamp_until (Game_shudder_time), Game_shudder_total);
            }
        }
    }
    // do shakes that affect external cameras
    else {
        // Make eye shake due to supernova
        if (supernova_camera_cut ()) {
            float cut_pct =
                1.0f - (supernova_time_left () / SUPERNOVA_CUT_TIME);
            tangles.p += get_shake (0.07f * cut_pct * sn_shudder, -1, 0);
            tangles.h += get_shake (0.07f * cut_pct * sn_shudder, -1, 0);
        }
    }

    // maybe bail
    if (tangles.p == 0.0f && tangles.h == 0.0f && tangles.b == 0.0f) return;

    matrix tm, tm2;
    vm_angles_2_matrix (&tm, &tangles);
    ASSERT (vm_vec_mag (&tm.vec.fvec) > 0.0f);
    ASSERT (vm_vec_mag (&tm.vec.rvec) > 0.0f);
    ASSERT (vm_vec_mag (&tm.vec.uvec) > 0.0f);
    vm_matrix_x_matrix (&tm2, eye_orient, &tm);
    *eye_orient = tm2;
}

// Player's velocity just before he blew up.  Used to keep camera target
// moving.
vec3d Dead_player_last_vel = { { { 1.0f, 1.0f, 1.0f } } };

int Scripting_didnt_draw_hud = 1;

camid chase_get_camera () {
    static camid chase_camera;
    if (!chase_camera.isValid ()) {
        chase_camera = cam_create ("Chase camera");
    }

    return chase_camera;
}

extern vec3d Dead_camera_pos;

// Set eye_pos and eye_orient based on view mode.
camid game_render_frame_setup () {
    bool fov_changed;

    if (!Main_camera.isValid ()) { Main_camera = cam_create ("Main camera"); }
    camera* main_cam = Main_camera.getCamera ();
    if (main_cam == NULL) {
        ASSERTX (0, "Unable to generate main camera");
        return camid ();
    }

    vec3d eye_pos;
    matrix eye_orient = vmd_identity_matrix;
    vec3d tmp_dir;

    static int last_Viewer_mode = 0;
    static int last_Game_mode = 0;
    static int last_Viewer_objnum = -1;
    static float last_FOV = Sexp_fov;

    fov_changed = ((last_FOV != Sexp_fov) && (Sexp_fov > 0.0f));

    // First, make sure we take into account 2D Missions.
    // These replace the normal player in-cockpit view with a topdown view.
    if (The_mission.flags[Mission::Mission_Flags::Mission_2d]) {
        if (!Viewer_mode) { Viewer_mode = VM_TOPDOWN; }
    }

    // This code is supposed to detect camera "cuts"... like going between
    // different views.

    // determine if we need to regenerate the nebula
    if ((!(last_Viewer_mode & VM_EXTERNAL) &&
         (Viewer_mode & VM_EXTERNAL)) || // internal to external
        ((last_Viewer_mode & VM_EXTERNAL) &&
         !(Viewer_mode & VM_EXTERNAL)) || // external to internal
        (!(last_Viewer_mode & VM_DEAD_VIEW) &&
         (Viewer_mode & VM_DEAD_VIEW)) || // non dead-view to dead-view
        ((last_Viewer_mode & VM_DEAD_VIEW) &&
         !(Viewer_mode & VM_DEAD_VIEW)) || // dead-view to non dead-view
        (!(last_Viewer_mode & VM_WARP_CHASE) &&
         (Viewer_mode & VM_WARP_CHASE)) || // non warp-chase to warp-chase
        ((last_Viewer_mode & VM_WARP_CHASE) &&
         !(Viewer_mode & VM_WARP_CHASE)) || // warp-chase to non warp-chase
        (!(last_Viewer_mode & VM_OTHER_SHIP) &&
         (Viewer_mode & VM_OTHER_SHIP)) || // non other-ship to other-ship
        ((last_Viewer_mode & VM_OTHER_SHIP) &&
         !(Viewer_mode & VM_OTHER_SHIP)) || // other-ship to non-other ship
        (!(last_Viewer_mode & VM_FREECAMERA) &&
         (Viewer_mode & VM_FREECAMERA)) ||
        ((last_Viewer_mode & VM_FREECAMERA) &&
         !(Viewer_mode & VM_FREECAMERA)) ||
        (!(last_Viewer_mode & VM_TOPDOWN) && (Viewer_mode & VM_TOPDOWN)) ||
        ((last_Viewer_mode & VM_TOPDOWN) && !(Viewer_mode & VM_TOPDOWN)) ||
        (fov_changed) ||
        ((Viewer_mode & VM_OTHER_SHIP) &&
         (last_Viewer_objnum !=
          Player_ai->target_objnum)) // other ship mode, but targets changes
    ) {
        // regenerate the nebula
        neb2_eye_changed ();
    }

    if ((last_Viewer_mode != Viewer_mode) || (last_Game_mode != Game_mode) ||
        (fov_changed) || (Viewer_mode & VM_FREECAMERA)) {
        // mprintf(( "************** Camera cut! ************\n" ));
        last_Viewer_mode = Viewer_mode;
        last_Game_mode = Game_mode;
        last_FOV = main_cam->get_fov ();

        // Camera moved.  Tell stars & debris to not do blurring.
        stars_camera_cut ();
    }

    say_view_target ();

    if (Viewer_mode & VM_PADLOCK_ANY) { player_display_padlock_view (); }

    if (Game_mode & GM_DEAD) {
        vec3d vec_to_deader, view_pos;
        float dist;

        Viewer_mode |= VM_DEAD_VIEW;

        if (Player_ai->target_objnum != -1) {
            int view_from_player = 1;

            if (Viewer_mode & VM_OTHER_SHIP) {
                // View from target.
                Viewer_obj = &Objects[Player_ai->target_objnum];

                last_Viewer_objnum = Player_ai->target_objnum;

                if (Viewer_obj->type == OBJ_SHIP) {
                    ship_get_eye (&eye_pos, &eye_orient, Viewer_obj);
                    view_from_player = 0;
                }
            }
            else {
                last_Viewer_objnum = -1;
            }

            if (view_from_player) {
                // View target from player ship.
                Viewer_obj = NULL;
                eye_pos = Player_obj->pos;
                vm_vec_normalized_dir (
                    &tmp_dir, &Objects[Player_ai->target_objnum].pos,
                    &eye_pos);
                vm_vector_2_matrix (&eye_orient, &tmp_dir, NULL, NULL);
                // rtn_cid = ship_get_followtarget_eye( Player_obj );
            }
        }
        else {
            dist = vm_vec_normalized_dir (
                &vec_to_deader, &Player_obj->pos, &Dead_camera_pos);

            if (dist < MIN_DIST_TO_DEAD_CAMERA) dist += flFrametime * 16.0f;

            vm_vec_scale (&vec_to_deader, -dist);
            vm_vec_add (&Dead_camera_pos, &Player_obj->pos, &vec_to_deader);

            view_pos = Player_obj->pos;

            if (!(Game_mode & GM_DEAD_BLEW_UP)) {
                Viewer_mode &= ~(VM_EXTERNAL | VM_CHASE);
                vm_vec_scale_add2 (
                    &Dead_camera_pos, &Original_vec_to_deader,
                    25.0f * flFrametime);
                Dead_player_last_vel = Player_obj->phys_info.vel;
            }
            else if (Player_ai->target_objnum != -1) {
                view_pos = Objects[Player_ai->target_objnum].pos;
            }
            else {
                // Make camera follow explosion, but gradually slow down.
                vm_vec_scale_add2 (
                    &Player_obj->pos, &Dead_player_last_vel, flFrametime);
                view_pos = Player_obj->pos;
                vm_vec_scale (&Dead_player_last_vel, 0.99f);
                vm_vec_scale_add2 (
                    &Dead_camera_pos, &Original_vec_to_deader,
                    MIN (25.0f, vm_vec_mag_quick (&Dead_player_last_vel)) *
                        flFrametime);
            }

            eye_pos = Dead_camera_pos;

            vm_vec_normalized_dir (&tmp_dir, &Player_obj->pos, &eye_pos);

            vm_vector_2_matrix (&eye_orient, &tmp_dir, NULL, NULL);
            Viewer_obj = NULL;
        }
    }

    // if supernova shockwave
    if (supernova_camera_cut ()) {
        // no viewer obj
        Viewer_obj = NULL;

        // call it dead view
        Viewer_mode |= VM_DEAD_VIEW;

        // set eye pos and orient
        // rtn_cid = supernova_set_view();
        supernova_get_eye (&eye_pos, &eye_orient);
    }
    else {
        // If already blown up, these other modes can override.
        if (!(Game_mode & (GM_DEAD | GM_DEAD_BLEW_UP))) {
            Viewer_mode &= ~VM_DEAD_VIEW;

            if (!(Viewer_mode & VM_FREECAMERA)) Viewer_obj = Player_obj;

            if (Viewer_mode & VM_OTHER_SHIP) {
                if (Player_ai->target_objnum != -1) {
                    Viewer_obj = &Objects[Player_ai->target_objnum];
                    last_Viewer_objnum = Player_ai->target_objnum;
                }
                else {
                    Viewer_mode &= ~VM_OTHER_SHIP;
                    last_Viewer_objnum = -1;
                }
            }
            else {
                last_Viewer_objnum = -1;
            }

            if (Viewer_mode & VM_FREECAMERA) {
                Viewer_obj = NULL;
                return cam_get_current ();
            }
            else if (Viewer_mode & VM_EXTERNAL) {
                matrix tm, tm2;

                vm_angles_2_matrix (&tm2, &Viewer_external_info.angles);
                vm_matrix_x_matrix (&tm, &Viewer_obj->orient, &tm2);

                vm_vec_scale_add (
                    &eye_pos, &Viewer_obj->pos, &tm.vec.fvec,
                    2.0f * Viewer_obj->radius + Viewer_external_info.distance);

                vm_vec_sub (&tmp_dir, &Viewer_obj->pos, &eye_pos);
                vm_vec_normalize (&tmp_dir);
                vm_vector_2_matrix (
                    &eye_orient, &tmp_dir, &Viewer_obj->orient.vec.uvec, NULL);
                Viewer_obj = NULL;

                // Modify the orientation based on head orientation.
                compute_slew_matrix (&eye_orient, &Viewer_slew_angles);
            }
            else if (Viewer_mode & VM_CHASE) {
                vec3d move_dir;
                vec3d aim_pt;

                if (Viewer_obj->phys_info.speed < 62.5f)
                    move_dir = Viewer_obj->phys_info.vel;
                else {
                    move_dir = Viewer_obj->phys_info.vel;
                    vm_vec_scale (
                        &move_dir, (62.5f / Viewer_obj->phys_info.speed));
                }

                vec3d tmp_up;
                matrix eyemat;
                ship_get_eye (&tmp_up, &eyemat, Viewer_obj, false, false);

                // create a better 3rd person view if this is the player ship
                if (Viewer_obj == Player_obj) {
                    // get a point 1000m forward of ship
                    vm_vec_copy_scale (
                        &aim_pt, &Viewer_obj->orient.vec.fvec, 1000.0f);
                    vm_vec_add2 (&aim_pt, &Viewer_obj->pos);

                    vm_vec_scale_add (
                        &eye_pos, &Viewer_obj->pos, &move_dir,
                        -0.02f * Viewer_obj->radius);
                    vm_vec_scale_add2 (
                        &eye_pos, &eyemat.vec.fvec,
                        -2.125f * Viewer_obj->radius -
                            Viewer_chase_info.distance);
                    vm_vec_scale_add2 (
                        &eye_pos, &eyemat.vec.uvec,
                        0.625f * Viewer_obj->radius +
                            0.35f * Viewer_chase_info.distance);
                    vm_vec_sub (&tmp_dir, &aim_pt, &eye_pos);
                    vm_vec_normalize (&tmp_dir);
                }
                else {
                    vm_vec_scale_add (
                        &eye_pos, &Viewer_obj->pos, &move_dir,
                        -0.02f * Viewer_obj->radius);
                    vm_vec_scale_add2 (
                        &eye_pos, &eyemat.vec.fvec,
                        -2.5f * Viewer_obj->radius -
                            Viewer_chase_info.distance);
                    vm_vec_scale_add2 (
                        &eye_pos, &eyemat.vec.uvec,
                        0.75f * Viewer_obj->radius +
                            0.35f * Viewer_chase_info.distance);
                    vm_vec_sub (&tmp_dir, &Viewer_obj->pos, &eye_pos);
                    vm_vec_normalize (&tmp_dir);
                }

                // JAS: I added the following code because if you slew up using
                // Descent-style physics, tmp_dir and
                // Viewer_obj->orient.vec.uvec are equal, which causes a
                // zero-length vector in the vm_vector_2_matrix call because
                // the up and the forward vector are the same.   I fixed it by
                // adding in a fraction of the right vector all the time to the
                // up vector.
                tmp_up = eyemat.vec.uvec;
                vm_vec_scale_add2 (&tmp_up, &eyemat.vec.rvec, 0.00001f);

                vm_vector_2_matrix (&eye_orient, &tmp_dir, &tmp_up, NULL);
                Viewer_obj = NULL;

                // Modify the orientation based on head orientation.
                compute_slew_matrix (&eye_orient, &Viewer_slew_angles);
            }
            else if (Viewer_mode & VM_WARP_CHASE) {
                Warp_camera.get_info (&eye_pos, NULL);

                ship* shipp = &Ships[Player_obj->instance];

                vec3d warp_pos = Player_obj->pos;
                shipp->warpout_effect->getWarpPosition (&warp_pos);
                vm_vec_sub (&tmp_dir, &warp_pos, &eye_pos);
                vm_vec_normalize (&tmp_dir);
                vm_vector_2_matrix (
                    &eye_orient, &tmp_dir, &Player_obj->orient.vec.uvec, NULL);
                Viewer_obj = NULL;
            }
            else if (Viewer_mode & VM_TOPDOWN) {
                angles_t rot_angles = { PI_2, 0.0f, 0.0f };
                bool position_override = false;
                if (Viewer_obj->type == OBJ_SHIP) {
                    ship_info* sip = &Ship_info[Ships[Viewer_obj->instance]
                                                    .ship_info_index];
                    if (sip->topdown_offset_def) {
                        eye_pos.xyz.x =
                            Viewer_obj->pos.xyz.x + sip->topdown_offset.xyz.x;
                        eye_pos.xyz.y =
                            Viewer_obj->pos.xyz.y + sip->topdown_offset.xyz.y;
                        eye_pos.xyz.z =
                            Viewer_obj->pos.xyz.z + sip->topdown_offset.xyz.z;
                        position_override = true;
                    }
                }
                if (!position_override) {
                    eye_pos.xyz.x = Viewer_obj->pos.xyz.x;
                    eye_pos.xyz.y =
                        Viewer_obj->pos.xyz.y + Viewer_obj->radius * 25.0f;
                    eye_pos.xyz.z = Viewer_obj->pos.xyz.z;
                }
                vm_angles_2_matrix (&eye_orient, &rot_angles);
                Viewer_obj = NULL;
            }
            else {
                // get an eye position based upon the correct type of object
                switch (Viewer_obj->type) {
                case OBJ_SHIP:
                    // make a call to get the eye point for the player object
                    ship_get_eye (&eye_pos, &eye_orient, Viewer_obj);
                    break;
                case OBJ_OBSERVER:
                    // make a call to get the eye point for the player object
                    observer_get_eye (&eye_pos, &eye_orient, Viewer_obj);
                    break;
                default:
                    WARNINGF (LOCATION,"Invalid Value for Viewer_obj->type. Expected values are OBJ_SHIP (1) and OBJ_OBSERVER (12), we encountered %d. Please tell a coder.",Viewer_obj->type);
                    ASSERT (0);
                }
            }
        }
    }

    main_cam->set_position (&eye_pos);
    main_cam->set_rotation (&eye_orient);

    // setup neb2 rendering
    neb2_render_setup (Main_camera);

    return Main_camera;
}

#ifndef NDEBUG
extern void ai_debug_render_stuff ();
#endif

int Game_subspace_effect = 0;
DCF_BOOL (subspace, Game_subspace_effect)

void clip_frame_view ();

// Does everything needed to render a frame
extern std::vector< object* > effect_ships;
extern std::vector< object* > transparent_objects;
void game_render_frame (camid cid) {
    GR_DEBUG_SCOPE ("Main Frame");
    TRACE_SCOPE (tracing::RenderMainFrame);

    g3_start_frame (game_zbuffer);

    camera* cam = cid.getCamera ();
    matrix eye_no_jitter = vmd_identity_matrix;
    if (cam != NULL) {
        vec3d eye_pos;
        matrix eye_orient;

        // Get current camera info
        cam->get_info (&eye_pos, &eye_orient);

        // Handle jitter if not cutscene camera
        eye_no_jitter = eye_orient;
        if (!(Viewer_mode & VM_FREECAMERA)) {
            apply_view_shake (&eye_orient);
            cam->set_rotation (&eye_orient);
        }

        // Maybe override FOV from SEXP
        if (Sexp_fov <= 0.0f)
            g3_set_view_matrix (&eye_pos, &eye_orient, cam->get_fov ());
        else
            g3_set_view_matrix (&eye_pos, &eye_orient, Sexp_fov);
    }
    else {
        g3_set_view_matrix (
            &vmd_zero_vector, &vmd_identity_matrix, VIEWER_ZOOM_DEFAULT);
    }

    // maybe offset the HUD (jitter stuff) and measure the 2D displacement
    // between the player's view and ship vector
    HUD_set_offsets (Viewer_obj, true, &eye_no_jitter);

    // this needs to happen after g3_start_frame() and before the primary
    // projection and view matrix is setup
    if (Cmdline_env)
        stars_setup_environment_mapping (cid);

    gr_zbuffer_clear (TRUE);
    gr_scene_texture_begin ();

    neb2_render_setup (cid);

#ifndef DYN_CLIP_DIST
    gr_set_proj_matrix (
        Proj_fov, gr_screen.clip_aspect, Min_draw_distance, Max_draw_distance);
    gr_set_view_matrix (&Eye_position, &Eye_matrix);
#endif

    if (Game_subspace_effect) {
        stars_draw (0, 0, 0, 1, 0);
    }
    else {
        stars_draw (1, 1, 1, 0, 0);
    }

    shadows_render_all (Proj_fov, &Eye_matrix, &Eye_position);
    obj_render_queue_all ();

    render_shields ();

    if (!Trail_render_override)
        trail_render_all (); // render missilie trails after everything else.
    particle::render_all (); // render particles after everything else.

#ifdef DYN_CLIP_DIST
    gr_end_proj_matrix ();
    gr_end_view_matrix ();
    gr_set_proj_matrix (
        Proj_fov, gr_screen.clip_aspect, Min_draw_distance, Max_draw_distance);
    gr_set_view_matrix (&Eye_position, &Eye_matrix);
#endif

    beam_render_all (); // render all beam weapons

    // render nebula lightning
    nebl_render_all ();

    // render local player nebula
    neb2_render_player ();

    batching_render_all (false);

    gr_copy_effect_texture ();

    // render all ships with shader effects on them
    std::vector< object* >::iterator obji = effect_ships.begin ();
    for (; obji != effect_ships.end (); ++obji) { obj_render (*obji); }
    effect_ships.clear ();

    // render distortions after the effect framebuffer is copied.
    batching_render_all (true);

    Shadow_override = true;
    // Draw the viewer 'cause we didn't before.
    // This is so we can change the minimum clipping distance without messing
    // everything up.
    if (Viewer_obj && (Viewer_obj->type == OBJ_SHIP) &&
        (Ship_info[Ships[Viewer_obj->instance].ship_info_index]
             .flags[Ship::Info_Flags::Show_ship_model]) &&
        (!Viewer_mode || (Viewer_mode & VM_PADLOCK_ANY) ||
         (Viewer_mode & VM_OTHER_SHIP) || (Viewer_mode & VM_TRACK) ||
         !(Viewer_mode & VM_EXTERNAL))) {
        gr_post_process_save_zbuffer ();
        ship_render_show_ship_cockpit (Viewer_obj);
        gr_post_process_restore_zbuffer ();
    }

#ifndef NDEBUG
    ai_debug_render_stuff ();
    extern void snd_spew_debug_info ();
    snd_spew_debug_info ();
#endif

    gr_end_proj_matrix ();
    gr_end_view_matrix ();

    // Draw viewer cockpit
    if (Viewer_obj != NULL && Viewer_mode != VM_TOPDOWN &&
        Ship_info[Ships[Viewer_obj->instance].ship_info_index]
                .cockpit_model_num > 0) {
        GR_DEBUG_SCOPE ("Render Cockpit");

        gr_post_process_save_zbuffer ();
        ship_render_cockpit (Viewer_obj);
        gr_post_process_restore_zbuffer ();
    }

    gr_set_proj_matrix (
        Proj_fov, gr_screen.clip_aspect, Min_draw_distance, Max_draw_distance);
    gr_set_view_matrix (&Eye_position, &Eye_matrix);

    // Do the sunspot
    game_sunspot_process (flFrametime);

    gr_end_proj_matrix ();
    gr_end_view_matrix ();

    Shadow_override = false;
    //================ END OF 3D RENDERING STUFF ====================

    gr_scene_texture_end ();

    game_tst_frame_pre ();

#ifndef NDEBUG
    do_timing_test (flFrametime);
#endif

    g3_end_frame ();
}

extern int Player_dead_state;

// Flip the page and time how long it took.
void game_flip_page_and_time_it () {
    tracing::async::end (tracing::MainFrame, tracing::MainFrameScope);
    tracing::async::begin (tracing::MainFrame, tracing::MainFrameScope);

    fix t1, t2, d;
    int t;
    t1 = timer_get_fixed_seconds ();
    gr_flip ();
    t2 = timer_get_fixed_seconds ();
    d = t2 - t1;
    t = (gr_screen.max_w * gr_screen.max_h * gr_screen.bytes_per_pixel) / 1024;
    sprintf (transfer_text, NOX ("%.2f MB/s"), double (t) * 65 / d);
}

void game_simulation_frame () {
    TRACE_SCOPE (tracing::Simulation);

    // Do camera stuff
    // This is for the warpout cam
    if (Player->control_mode != PCM_NORMAL) Warp_camera.do_frame (flFrametime);

    // Do ingame cutscenes stuff
    if (!Time_compression_locked) { cam_do_frame (flFrametime); }
    else {
        cam_do_frame (flRealframetime);
    }

    // process AWACS stuff - do this first thing
    awacs_process ();

    // Do autopilot stuff
    NavSystem_Do ();

    // single player, set Player hits_this_frame to 0
    if (!(Game_mode & GM_MULTIPLAYER) && Player) {
        Player->damage_this_burst -=
            (flFrametime * MAX_BURST_DAMAGE / (0.001f * BURST_DURATION));
        Player->damage_this_burst = MAX (Player->damage_this_burst, 0.0f);
    }

    // supernova
    supernova_process ();
    if (supernova_active () >= 5) { return; }

    // fire targeting lasers now so that
    // 1 - created this frame
    // 2 - collide this frame
    // 3 - render this frame
    // 4 - ignored and deleted next frame
    // the basic idea being that because it con be confusing to deal with them
    // on a multi-frame basis, they are only valid for frame
    ship_process_targeting_lasers ();

    // do this here so that it works for multiplayer
    if (Viewer_obj) {
        // get viewer direction
        int viewer_direction = PHYSICS_VIEWER_REAR;

        if (Viewer_mode == 0) { viewer_direction = PHYSICS_VIEWER_FRONT; }
        if (Viewer_mode & VM_PADLOCK_UP) {
            viewer_direction = PHYSICS_VIEWER_UP;
        }
        else if (Viewer_mode & VM_PADLOCK_REAR) {
            viewer_direction = PHYSICS_VIEWER_REAR;
        }
        else if (Viewer_mode & VM_PADLOCK_LEFT) {
            viewer_direction = PHYSICS_VIEWER_LEFT;
        }
        else if (Viewer_mode & VM_PADLOCK_RIGHT) {
            viewer_direction = PHYSICS_VIEWER_RIGHT;
        }

        physics_set_viewer (&Viewer_obj->phys_info, viewer_direction);
    }
    else {
        physics_set_viewer (
            &Objects[Player->objnum].phys_info, PHYSICS_VIEWER_FRONT);
    }

    // evaluate mission departures and arrivals before we process all objects.
    mission_parse_eval_stuff ();

    // move all the objects now
    obj_move_all (flFrametime);

    game_do_training_checks ();

    mission_eval_goals ();

    // always check training objectives, even in multiplayer missions. we need
    // to do this so that the directives gauge works properly on clients
    training_check_objectives ();

    // only process the message queue when the player is "in" the game
    if (!Pre_player_entry) {
        // process any messages send to the player
        message_queue_process ();
    }

    // maybe distort incoming message if comms damaged
    message_maybe_distort ();

    // AI objects get repaired in ai_process, called from move code...deal with
    // player.
    player_repair_frame (flFrametime);

    // maybe send off a delayed praise message to the player
    player_process_pending_praise ();

    // maybe tell the player he is all alone
    player_maybe_play_all_alone_msg ();

    // process some stuff every frame (before frame is rendered)
    emp_process_local ();

    hud_update_frame (flFrametime); // update hud systems

    if (!physics_paused) {
        // Move particle system
        particle::move_all (flFrametime);
        particle::ParticleManager::get ()->doFrame (flFrametime);

        // Move missile trails
        trail_move_all (flFrametime);

        // Flash the gun flashes
        shipfx_flash_do_frame (flFrametime);

        shockwave_move_all (flFrametime); // update all the shockwaves
    }

    // subspace missile strikes
    ssm_process ();

    // update the object-linked persistant sounds
    obj_snd_do_frame ();

    game_maybe_update_sound_environment ();

    snd_update_listener (
        &Eye_position, &Player_obj->phys_info.vel, &Eye_matrix);

#ifndef NDEBUG
    // AL: debug code used for testing ambient subspace sound (ie when enabling
    // subspace through debug console)
    if (Game_subspace_effect) { game_start_subspace_ambient_sound (); }
#endif
}

// Maybe render and process the dead-popup
void game_maybe_do_dead_popup (float frametime) {
    if (popupdead_is_active ()) {
        int leave_popup = 1;
        int choice = popupdead_do_frame (frametime);

        switch (choice) {
        case 0: gameseq_post_event (GS_EVENT_ENTER_GAME); break;

        case 1: gameseq_post_event (GS_EVENT_END_GAME); break;

        case 2: gameseq_post_event (GS_EVENT_START_GAME); break;

            // this should only happen during a red alert mission
        case 3:
            if (The_mission.flags[Mission::Mission_Flags::Red_alert]) {
                // choose the previous mission
                mission_campaign_previous_mission ();
            }
            else {
                // bogus?
                ASSERT (0);
            }

            gameseq_post_event (GS_EVENT_START_GAME);
            break;

        default: leave_popup = 0; break;
        }

        if (leave_popup) { popupdead_close (); }
    }
}

// returns true if player is actually in a game_play stats
int game_actually_playing () {
    int state;

    state = gameseq_get_state ();
    if ((state != GS_STATE_GAME_PLAY) && (state != GS_STATE_DEATH_DIED) &&
        (state != GS_STATE_DEATH_BLEW_UP))
        return 0;
    else
        return 1;
}

void game_render_hud (camid cid) {
    gr_reset_clip ();

    if (cid.isValid ()) {
        g3_start_frame (0); // 0 = turn zbuffering off
        g3_set_view (cid.getCamera ());

        hud_render_preprocess (flFrametime);

        g3_end_frame ();
    }

    // main HUD rendering function
    hud_render_all ();

    // Diminish the palette effect
    game_flash_diminish (flFrametime);
}

// 100% blackness
void game_reset_shade_frame () {
    Fade_type = FI_NONE;
    gr_create_shader (&Viewer_shader, 0, 0, 0, 0);
}

void game_shade_frame (float /*frametime*/) {
    // only do frame shade if we are actually in a game play state
    if (!game_actually_playing ()) { return; }

    GR_DEBUG_SCOPE ("Shade frame");

    if (Fade_type != FI_NONE) {
        ASSERT (Fade_start_timestamp > 0);
        ASSERT (Fade_end_timestamp > 0);
        ASSERT (Fade_end_timestamp > Fade_start_timestamp);

        if (timestamp () >= Fade_start_timestamp) {
            int startAlpha = 0;
            int endAlpha = 0;

            if (Fade_type == FI_FADEOUT) { endAlpha = 255; }
            else if (Fade_type == FI_FADEIN) {
                startAlpha = 255;
            }

            int alpha = 0;

            if (timestamp () < Fade_end_timestamp) {
                int duration = (Fade_end_timestamp - Fade_start_timestamp);
                int elapsed = (timestamp () - Fade_start_timestamp);

                alpha = int (
                    (float)startAlpha +
                    (((float)endAlpha - (float)startAlpha) / (float)duration) *
                        (float)elapsed);
            }
            else {
                // Fade finished
                Fade_type = FI_NONE;
                Fade_start_timestamp = 0;
                Fade_end_timestamp = 0;

                alpha = endAlpha;
            }

            Viewer_shader.c = (ubyte)alpha;
        }
    }

    gr_flash_alpha (
        Viewer_shader.r, Viewer_shader.g, Viewer_shader.b, Viewer_shader.c);
}

const static int CUTSCENE_BAR_DIVISOR = 8;
void bars_do_frame (float frametime) {
    if ((Cutscene_bar_flags & CUB_GRADUAL) && Cutscene_bars_progress < 1.0f) {
        // Determine how far along we are
        ASSERT (Cutscene_delta_time > 0.0f);

        Cutscene_bars_progress += frametime / Cutscene_delta_time;
        if (Cutscene_bars_progress >= 1.0f) {
            // Reset this stuff
            Cutscene_delta_time = 1.0f;
            Cutscene_bars_progress = 1.0f;
        }

        // Figure out where the bars should be
        int yborder;
        if (Cutscene_bar_flags & CUB_CUTSCENE)
            yborder = int (
                Cutscene_bars_progress *
                (gr_screen.max_h / CUTSCENE_BAR_DIVISOR));
        else
            yborder = gr_screen.max_h / CUTSCENE_BAR_DIVISOR -
                      int (
                          Cutscene_bars_progress *
                          (gr_screen.max_h / CUTSCENE_BAR_DIVISOR));

        if (g3_in_frame () == 0) {
            // Set rectangles
            gr_set_color (0, 0, 0);
            gr_set_bitmap (0); // Valathil - Don't ask me why this has to be
                               // here but otherwise the black bars don't draw
            gr_rect (0, 0, gr_screen.max_w, yborder, GR_RESIZE_NONE);
            gr_rect (
                0, gr_screen.max_h - yborder, gr_screen.max_w, yborder,
                GR_RESIZE_NONE);
        }
        else {
            // Set clipping
            gr_reset_clip ();
            gr_set_clip (
                0, yborder, gr_screen.max_w, gr_screen.max_h - yborder * 2,
                GR_RESIZE_NONE);
        }
    }
    else if (Cutscene_bar_flags & CUB_CUTSCENE) {
        int yborder = gr_screen.max_h / CUTSCENE_BAR_DIVISOR;

        if (g3_in_frame () == 0) {
            gr_set_color (0, 0, 0);
            gr_set_bitmap (0); // Valathil - Don't ask me why this has to be
                               // here but otherwise the black bars don't draw
            gr_rect (0, 0, gr_screen.max_w, yborder, GR_RESIZE_NONE);
            gr_rect (
                0, gr_screen.max_h - yborder, gr_screen.max_w, yborder,
                GR_RESIZE_NONE);
        }
        else {
            gr_reset_clip ();
            gr_set_clip (
                0, yborder, gr_screen.max_w, gr_screen.max_h - (yborder * 2),
                GR_RESIZE_NONE);
        }
    }
}

void clip_frame_view () {
    if (!Time_compression_locked) {
        // Player is dead
        game_set_view_clip (flFrametime);

        // Cutscene bars
        bars_do_frame (flRealframetime);
    }
    else {
        // Player is dead
        game_set_view_clip (flRealframetime);

        // Cutscene bars
        bars_do_frame (flRealframetime);
    }
}

// WMC - This does stuff like fading in and out and subtitles. Special FX?
// Basically stuff you need rendered after everything else (including HUD)
void game_render_post_frame () {
    float frametime = flFrametime;
    if (Time_compression_locked) { frametime = flRealframetime; }

    subtitles_do_frame (frametime);
    game_shade_frame (frametime);
    subtitles_do_frame_post_shaded (frametime);
}

#ifndef NDEBUG
#define DEBUG_GET_TIME(x) \
    { x = timer_get_fixed_seconds (); }
#else
#define DEBUG_GET_TIME(x)
#endif

void game_frame (bool paused) {
#ifndef NDEBUG
    fix total_time1, total_time2;
    fix render2_time1 = 0, render2_time2 = 0;
    fix render3_time1 = 0, render3_time2 = 0;
    fix flip_time1 = 0, flip_time2 = 0;
    fix clear_time1 = 0, clear_time2 = 0;
#endif
    int actually_playing;

#ifndef NDEBUG
    if (Framerate_delay) {
        int start_time = timer_get_milliseconds ();
        while (timer_get_milliseconds () < start_time + Framerate_delay)
            ;
    }
#endif
    // start timing frame
    TRACE_SCOPE (tracing::MainFrame);

    DEBUG_GET_TIME (total_time1)

    if (paused) {
        // Reset the lights here or they just keep on increasing
        light_reset ();
    }
    else {
        // var to hold which state we are in
        actually_playing = game_actually_playing ();

        ASSERT (OBJ_INDEX (Player_obj) >= 0);

        if (Missiontime > Entry_delay_time) { Pre_player_entry = 0; }

        // Note: These are done even before the player enters, else buffers
        // can overflow.
        if (!(Game_mode & GM_STANDALONE_SERVER)) { radar_frame_init (); }

        shield_frame_init ();

        if (!Pre_player_entry && actually_playing) {
            if ((!popup_running_state ()) && (!popupdead_is_active ())) {
                game_process_keys ();
                read_player_controls (Player_obj, flFrametime);
            }
        }

        // Reset the whack stuff
        game_whack_reset ();

        // These two lines must be outside of Pre_player_entry code,
        // otherwise too many lights are added.
        light_reset ();

        game_simulation_frame ();

        // if not actually in a game play state, then return.  This condition
        // could only be true in a multiplayer game.
        if (!actually_playing) {
            ASSERT (Game_mode & GM_MULTIPLAYER);
            return;
        }
    }

    if (!Pre_player_entry) {
        DEBUG_GET_TIME (clear_time1);
        // clear the screen to black
        gr_reset_clip ();
        if ((Game_detail_flags & DETAIL_FLAG_CLEAR)) { gr_clear (); }

        DEBUG_GET_TIME (clear_time2);
        DEBUG_GET_TIME (render3_time1);

        camid cid = game_render_frame_setup ();

        game_render_frame (cid);

        // Cutscene bars
        clip_frame_view ();

        game_render_hud (cid);
        HUD_reset_clip ();

        if (Game_detail_flags & DETAIL_FLAG_HUD) {
            anim_render_all (0, flFrametime);
        }

        if (!(Viewer_mode & (VM_EXTERNAL | VM_DEAD_VIEW | VM_WARP_CHASE |
                             VM_PADLOCK_ANY))) {
            TRACE_SCOPE (tracing::RenderHUDHook);
        }

        // check to see if we should display the death died popup
        if (Game_mode & GM_DEAD_BLEW_UP) {
            if ((Player_died_popup_wait != -1) &&
                (timestamp_elapsed (Player_died_popup_wait))) {
                Player_died_popup_wait = -1;
                popupdead_start ();
            }
        }

        // Goober5000 - check if we should red-alert
        // (this is approximately where the red_alert_check_status()
        // function tree began in the pre-HUD-overhaul code)
        red_alert_maybe_move_to_next_mission ();

        DEBUG_GET_TIME (render3_time2);
        DEBUG_GET_TIME (render2_time1);

        gr_reset_clip ();
        game_get_framerate ();
        game_show_framerate ();
        game_show_eye_pos (cid);

        game_show_time_left ();

        gr_reset_clip ();
        game_render_post_frame ();

        game_tst_frame ();

        DEBUG_GET_TIME (render2_time2);;

        // maybe render and process the dead popup
        game_maybe_do_dead_popup (flFrametime);

        // If a regular popup is active, don't flip (popup code flips)
        if (!popup_running_state ()) {
            DEBUG_GET_TIME (flip_time1);
            game_flip_page_and_time_it ();
            DEBUG_GET_TIME (flip_time2);
        }
    }

    asteroid_frame ();

    // process lightning (nebula only)
    nebl_process ();

    if (Cmdline_frame_profile) { tracing::frame_profile_process_frame (); }

    DEBUG_GET_TIME (total_time2)

#ifndef NDEBUG
    // Got some timing numbers
    Timing_total = f2fl (total_time2 - total_time1) * 1000.0f;
    Timing_clear = f2fl (clear_time2 - clear_time1) * 1000.0f;
    Timing_render2 = f2fl (render2_time2 - render2_time1) * 1000.0f;
    Timing_render3 = f2fl (render3_time2 - render3_time1) * 1000.0f;
    Timing_flip = f2fl (flip_time2 - flip_time1) * 1000.0f;
#endif
}

#define MAX_FRAMETIME \
    (F1_0 /           \
     4) // Frametime gets saturated at this.  Changed by MK on 11/1/97.
        // Some bug was causing Frametime to always get saturated
        // at 2.0 seconds after the player       died.  This resulted in
        // screwed up death sequences.

fix Last_time = 0; // The absolute time of game at end of last frame (beginning
                   // of this frame)
fix Last_delta_time = 0; // While game is paused, this keeps track of how much
                         // elapsed in the frame before paused.
static int timer_paused = 0;
#if defined(TIMER_TEST) && !defined(NDEBUG)
static int stop_count, start_count;
static int time_stopped, time_started;
#endif
int saved_timestamp_ticker = -1;

void game_reset_time () {
    game_start_time ();
    timestamp_reset ();
    game_stop_time ();
}

void game_stop_time () {
    if (timer_paused == 0) {
        fix time;
        time = timer_get_fixed_seconds ();
        // Save how much time progressed so far in the frame so we can
        // use it when we unpause.
        Last_delta_time = time - Last_time;

        // mprintf(("Last_time in game_stop_time = %7.3f\n",
        // f2fl(Last_delta_time)));
        if (Last_delta_time < 0) {
#if defined(TIMER_TEST) && !defined(NDEBUG)
            ASSERT (0);
#endif
            Last_delta_time = 0;
        }
#if defined(TIMER_TEST) && !defined(NDEBUG)
        time_stopped = time;
#endif

        // Stop the timer_tick stuff...
        saved_timestamp_ticker = timestamp ();
    }
    timer_paused++;

#if defined(TIMER_TEST) && !defined(NDEBUG)
    stop_count++;
#endif
}

void game_start_time () {
    timer_paused--;
    ASSERT (timer_paused >= 0);
    if (timer_paused == 0) {
        fix time;
        time = timer_get_fixed_seconds ();
#if defined(TIMER_TEST) && !defined(NDEBUG)
        if (Last_time < 0) ASSERT (0);
    }
#endif
    // Take current time, and set it backwards to account for time
    // that the frame already executed, so that timer_get_fixed_seconds() -
    // Last_time will be correct when it goes to calculate the frametime next
    // frame.
    Last_time = time - Last_delta_time;
#if defined(TIMER_TEST) && !defined(NDEBUG)
    time_started = time;
#endif

    // Restore the timer_tick stuff...
    // Normally, you should never access 'timestamp_ticker', consider this a
    // low-level routine
    ASSERT (saved_timestamp_ticker > -1); // Called out of order, get JAS
    timestamp_set_value (saved_timestamp_ticker);
    saved_timestamp_ticker = -1;
}

#if defined(TIMER_TEST) && !defined(NDEBUG)
start_count++;
#endif
}

void lock_time_compression (bool is_locked) {
    Time_compression_locked = is_locked;
}

void change_time_compression (float multiplier) {
    fix modified = fl2f (f2fl (Game_time_compression) * multiplier);

    Desired_time_compression = Game_time_compression = modified;
    Time_compression_change_rate = 0;
}

void set_time_compression (float multiplier, float change_time) {
    if (change_time <= 0.0f) {
        Game_time_compression = Desired_time_compression = fl2f (multiplier);
        Time_compression_change_rate = 0;
        return;
    }

    Desired_time_compression = fl2f (multiplier);
    Time_compression_change_rate = fl2f (
        f2fl (Desired_time_compression - Game_time_compression) / change_time);
}

void game_set_frametime (int state) {
    fix thistime = timer_get_fixed_seconds ();

    if (Last_time == 0)
        Frametime = F1_0 / 30;
    else
        Frametime = thistime - Last_time;

        // Frametime = F1_0 / 30;

#ifndef NDEBUG
    fix debug_frametime = Frametime; // Just used to display frametime.
#endif

    // If player hasn't entered mission yet, make frame take 1/4 second.
    if ((Pre_player_entry) && (state == GS_STATE_GAME_PLAY))
        Frametime = F1_0 / 4;
#ifndef NDEBUG
    else if ((Debug_dump_frames) && (state == GS_STATE_GAME_PLAY)) {
         // note link to above if!!!!!
        fix frame_speed = F1_0 / Debug_dump_frames;

        if (Frametime > frame_speed) {
            WARNINGF (LOCATION, "slow frame: %x", (int)Frametime);
        }
        else {
            do {
                thistime = timer_get_fixed_seconds ();
                Frametime = thistime - Last_time;
            } while (Frametime < frame_speed);
        }
        Frametime = frame_speed;
    }
#endif

    ASSERTX (
        Framerate_cap > 0,
        "Framerate cap %d is too low. Needs to be a positive, non-zero number",
        Framerate_cap);

    // Cap the framerate so it doesn't get too high.
    if (!Cmdline_NoFPSCap) {
        fix cap;

        cap = F1_0 / Framerate_cap;
        if (Frametime < cap) {
            thistime = cap - Frametime;
            // mprintf(("Sleeping for %6.3f seconds.\n",
            // f2fl(thistime)));
            fs2::os::sleep (unsigned (f2fl (thistime) * 1000.0f));
            Frametime = cap;
            thistime = timer_get_fixed_seconds ();
        }
    }

    // If framerate is too low, cap it.
    if (Frametime > MAX_FRAMETIME) {
        II
            << "frame " << Framecount << " took too long: "
            << f2fl (Frametime) << "/" << f2fl (debug_frametime);
        Frametime = MAX_FRAMETIME;
    }

    flRealframetime = f2fl (Frametime);

    // Handle changes in time compression
    if (Game_time_compression != Desired_time_compression) {
        bool ascending = Desired_time_compression > Game_time_compression;
        if (Time_compression_change_rate)
            Game_time_compression +=
                fixmul (Time_compression_change_rate, Frametime);
        if ((ascending && Game_time_compression > Desired_time_compression) ||
            (!ascending && Game_time_compression < Desired_time_compression))
            Game_time_compression = Desired_time_compression;
    }

    Frametime = fixmul (Frametime, Game_time_compression);

    if (Frametime <= 0) {
        // If the Frametime is zero or below due to Game_time_compression, set
        // the Frametime to 1 (1/65536 of a second).
        Frametime = 1;
    }

    Last_time = thistime;
    // mprintf(("Frame %i, Last_time = %7.3f\n", Framecount, f2fl(Last_time)));
    Last_frame_timestamp = timestamp ();

    flFrametime = f2fl (Frametime);

    timestamp_inc (Frametime);

    // wrap overall frametime if needed
    if (FrametimeOverall > (INT_MAX - F1_0)) FrametimeOverall = 0;

    FrametimeOverall += Frametime;

    II << "frametime : " << std::hex << Frametime;
}

fix game_get_overall_frametime () { return FrametimeOverall; }

// This is called from game_do_frame(), and from navmap_do_frame()
void game_update_missiontime () {
    // TODO JAS: Put in if and move this into game_set_frametime,
    // fix navmap to call game_stop/start_time
    // if ( !timer_paused )
    Missiontime += Frametime;
}

void game_do_frame () {
    game_set_frametime (GS_STATE_GAME_PLAY);
    game_update_missiontime ();

    // if (Player_ship->flags[Ship::Ship_Flags::Dying])
    // flFrametime /= 15.0;

    if (game_single_step && (last_single_step == game_single_step)) {
        fs2::os::title ("SINGLE STEP MODE (Pause exits, any key steps)");
        while (key_checkch () == 0) fs2::os::sleep (10);
        fs2::os::title ("FreeSpace 2");
        Last_time = timer_get_fixed_seconds ();
    }

    last_single_step = game_single_step;

    game_frame ();
}

int Joymouse_button_status = 0;

// Flush all input devices
void game_flush () {
    key_flush ();
    mouse_flush ();
    snazzy_flush ();

    Joymouse_button_status = 0;

    // mprintf(("Game flush!\n" ));
}

// Call this whenever in a loop, or when you need to check for a keystroke.
int game_check_key () {
    int k = game_poll ();

    // convert keypad enter to normal enter
    if ((k & KEY_MASK) == KEY_PADENTER) k = (k & ~KEY_MASK) | KEY_ENTER;

    return k;
}

// same as game_check_key(), except this is used while actually in the game.
// Since there generally are differences between game control keys and general
// UI keys, makes sense to have seperate functions for each case.  If you are
// not checking a game control while in a mission, you should probably be using
// game_check_key() instead.
int game_poll () {
    int k, state;

    if (!fs2::os::is_foreground ()) {

        game_stop_time ();
        fs2::os::sleep (1);
        game_start_time ();

        if ((gameseq_get_state () == GS_STATE_GAME_PLAY) &&
            (!popup_active ()) && (!popupdead_is_active ())) {
            game_process_pause_key ();
        }
    }

    k = key_inkey ();

    // Move the mouse cursor with the joystick.
    if (fs2::os::is_foreground () &&
        !io::mouse::CursorManager::get ()->isCursorShown () &&
        (Use_joy_mouse)) {
        // Move the mouse cursor with the joystick
        int mx, my, dx, dy;
        int jx, jy;

        int raw_axis[2];

        joystick_read_raw_axis (2, raw_axis);

        jx = joy_get_scaled_reading (raw_axis[0]);
        jy = joy_get_scaled_reading (raw_axis[1]);

        dx = int (f2fl (jx) * flFrametime * 500.0f);
        dy = int (f2fl (jy) * flFrametime * 500.0f);

        if (dx || dy) {
            mouse_get_real_pos (&mx, &my);
            mouse_set_pos (mx + dx, my + dy);
        }

        int j, m;
        j = joy_down (0);
        m = mouse_down (MOUSE_LEFT_BUTTON);

        if (j != Joymouse_button_status) {
            // mprintf(( "Joy went from %d to %d, mouse is %d\n",
            // Joymouse_button_status, j, m ));
            Joymouse_button_status = j;
            if (j && (!m)) { mouse_mark_button (MOUSE_LEFT_BUTTON, 1); }
            else if ((!j) && (m)) {
                mouse_mark_button (MOUSE_LEFT_BUTTON, 0);
            }
        }
    }

    state = gameseq_get_state ();

    // If a popup is running, don't process all the Fn keys
    if (popup_active ()) {
        if (state != GS_STATE_DEATH_BLEW_UP) { return k; }
    }

    switch (k) {
    case KEY_F1:
        launch_context_help ();
        k = 0;
        break;

    case KEY_F2:
        switch (state) {
        case GS_STATE_INITIAL_PLAYER_SELECT:
        case GS_STATE_OPTIONS_MENU:
        case GS_STATE_HUD_CONFIG:
        case GS_STATE_CONTROL_CONFIG:
        case GS_STATE_DEATH_DIED:
            // case GS_STATE_DEATH_BLEW_UP:    //      DEATH_BLEW_UP
            //might be okay but do not comment out DEATH_DIED as otherwise no
            // clean
            // up is performed on the dead ship
        case GS_STATE_VIEW_MEDALS: break;

        default:
            gameseq_post_event (GS_EVENT_OPTIONS_MENU);
            k = 0;
            break;
        }

        break;

        // hotkey selection screen -- only valid from briefing and beyond.
    case KEY_F3:
        if ((state == GS_STATE_TEAM_SELECT) || (state == GS_STATE_BRIEFING) ||
            (state == GS_STATE_SHIP_SELECT) ||
            (state == GS_STATE_WEAPON_SELECT) ||
            (state == GS_STATE_GAME_PLAY) || (state == GS_STATE_GAME_PAUSED)) {
            gameseq_post_event (GS_EVENT_HOTKEY_SCREEN);
            k = 0;
        }
        break;

    case KEY_DEBUGGED + KEY_F3:
        gameseq_post_event (GS_EVENT_TOGGLE_FULLSCREEN);
        break;

    case KEY_F4:
        if ((state == GS_STATE_GAME_PLAY) ||
            (state == GS_STATE_DEATH_DIED) ||
            (state == GS_STATE_DEATH_BLEW_UP) ||
            (state == GS_STATE_GAME_PAUSED)) {
            gameseq_post_event (GS_EVENT_MISSION_LOG_SCROLLBACK);
            k = 0;
        }
        break;

    case KEY_ESC | KEY_SHIFTED:
        gameseq_post_event (GS_EVENT_QUIT_GAME);
        k = 0;
        break;

    case KEY_DEBUGGED + KEY_P: break;

    case KEY_PRINT_SCRN: {
        static int counter = fs2::registry::read ("Default.ScreenshotNum", 0);
        char tmp_name[MAX_FILENAME_LEN];

        game_stop_time ();

        // we could probably go with .3 here for 1,000 shots but people really
        // need to clean out their directories better than that so it's 100 for
        // now.
        sprintf (tmp_name, NOX ("screen%.4i"), counter);
        counter++;

        // we've got two character precision so we can only have 100 shots at a
        // time, reset if needed
        // Now we have four digit precision :) -WMC
        if (counter > 9999) {
            // This should pop up a dialogue or something ingame.
            WARNINGF (LOCATION,"Screenshot count has reached max of 9999. Resetting to 0.");
            counter = 0;
        }

        WARNINGF (LOCATION, "Dumping screen to '%s'", tmp_name);
        gr_print_screen (tmp_name);

        game_start_time ();
        fs2::registry::write ("Default.ScreenshotNum", counter);
    }

        k = 0;
        break;

    case KEY_SHIFTED | KEY_ENTER: {
#if !defined(NDEBUG)
        game_stop_time ();
        debug_console ();
        game_flush ();
        game_start_time ();
#endif
        break;
    }
    }

    return k;
}

void os_close () { gameseq_post_event (GS_EVENT_QUIT_GAME); }

// All code to process events.   This is the only place
// that you should change the state of the game.
void game_process_event (int current_state, int event) {
    WARNINGF (LOCATION, "Got event %s (%d) in state %s (%d)", GS_event_text[event],event, GS_state_text[current_state], current_state);

    switch (event) {
    case GS_EVENT_SIMULATOR_ROOM:
        gameseq_set_state (GS_STATE_SIMULATOR_ROOM);
        break;

    case GS_EVENT_MAIN_MENU: gameseq_set_state (GS_STATE_MAIN_MENU); break;

    case GS_EVENT_OPTIONS_MENU:
        gameseq_push_state (GS_STATE_OPTIONS_MENU);
        break;

    case GS_EVENT_BARRACKS_MENU:
        gameseq_set_state (GS_STATE_BARRACKS_MENU);
        break;

    case GS_EVENT_TECH_MENU: gameseq_set_state (GS_STATE_TECH_MENU); break;
    case GS_EVENT_LAB: gameseq_push_state (GS_STATE_LAB); break;
    case GS_EVENT_TRAINING_MENU:
        gameseq_set_state (GS_STATE_TRAINING_MENU);
        break;

    case GS_EVENT_START_GAME:
        Select_default_ship = 0;
        gameseq_set_state (GS_STATE_START_GAME);
        break;

    case GS_EVENT_START_GAME_QUICK:
        Select_default_ship = 1;
        gameseq_post_event (GS_EVENT_ENTER_GAME);
        break;

    case GS_EVENT_CMD_BRIEF: gameseq_set_state (GS_STATE_CMD_BRIEF); break;

    case GS_EVENT_RED_ALERT: gameseq_set_state (GS_STATE_RED_ALERT); break;

    case GS_EVENT_START_BRIEFING: gameseq_set_state (GS_STATE_BRIEFING); break;

    case GS_EVENT_DEBRIEF:
        // did we end the campaign in the main freespace 2 single player
        // campaign? (specifically, did we successfully jump out when the
        // supernova was in progress and the campaign was ending?)
        if (Campaign_ending_via_supernova &&
            (Game_mode &
             GM_CAMPAIGN_MODE) /* && !strcasecmp(Campaign.filename, "freespace2")*/) {
            gameseq_post_event (GS_EVENT_END_CAMPAIGN);
        }
        else {
            gameseq_set_state (GS_STATE_DEBRIEF);
        }
        break;

    case GS_EVENT_SHIP_SELECTION:
        gameseq_set_state (GS_STATE_SHIP_SELECT);
        break;

    case GS_EVENT_WEAPON_SELECTION:
        gameseq_set_state (GS_STATE_WEAPON_SELECT);
        break;

    case GS_EVENT_ENTER_GAME:
        if (Game_mode & GM_MULTIPLAYER) {
            // if we're respawning, make sure we change the view mode so that
            // the hud shows up
            if (current_state == GS_STATE_DEATH_BLEW_UP) { Viewer_mode = 0; }

            gameseq_set_state (GS_STATE_GAME_PLAY);
        }
        else {
            gameseq_set_state (GS_STATE_GAME_PLAY, 1);
        }

        Start_time = f2fl (timer_get_approx_seconds ());
        WARNINGF (LOCATION, "Entering game at time = %7.3f", Start_time);

        break;

    case GS_EVENT_END_GAME:
        if ((current_state == GS_STATE_GAME_PLAY) ||
            (current_state == GS_STATE_DEATH_DIED) ||
            (current_state == GS_STATE_DEATH_BLEW_UP) ||
            (current_state == GS_STATE_DEBRIEF)) {
            gameseq_set_state (GS_STATE_MAIN_MENU);
        }
        else
            ASSERT (0);

        break;

    case GS_EVENT_QUIT_GAME:
        main_hall_stop_music (true);
        main_hall_stop_ambient ();
        gameseq_set_state (GS_STATE_QUIT_GAME);
        break;

    case GS_EVENT_GAMEPLAY_HELP:
        gameseq_push_state (GS_STATE_GAMEPLAY_HELP);
        break;

    case GS_EVENT_PAUSE_GAME: gameseq_push_state (GS_STATE_GAME_PAUSED); break;

    case GS_EVENT_DEBUG_PAUSE_GAME:
        gameseq_push_state (GS_STATE_DEBUG_PAUSED);
        break;

    case GS_EVENT_TRAINING_PAUSE:
        gameseq_push_state (GS_STATE_TRAINING_PAUSED);
        break;

    case GS_EVENT_PREVIOUS_STATE: gameseq_pop_state (); break;

    case GS_EVENT_LOAD_MISSION_MENU:
        gameseq_set_state (GS_STATE_LOAD_MISSION_MENU);
        break;

    case GS_EVENT_MISSION_LOG_SCROLLBACK:
        gameseq_push_state (GS_STATE_MISSION_LOG_SCROLLBACK);
        break;

    case GS_EVENT_HUD_CONFIG: gameseq_push_state (GS_STATE_HUD_CONFIG); break;

    case GS_EVENT_CONTROL_CONFIG:
        gameseq_push_state (GS_STATE_CONTROL_CONFIG);
        break;

    case GS_EVENT_DEATH_DIED: gameseq_set_state (GS_STATE_DEATH_DIED); break;

    case GS_EVENT_DEATH_BLEW_UP:
        if (current_state == GS_STATE_DEATH_DIED) {
            gameseq_set_state (GS_STATE_DEATH_BLEW_UP);
            event_music_player_death ();
        }
        else {
            WARNINGF (LOCATION,"Ignoring GS_EVENT_DEATH_BLEW_UP because we're in state %d",current_state);
        }
        break;

    case GS_EVENT_NEW_CAMPAIGN:
        if (!mission_load_up_campaign ()) { readyroom_continue_campaign (); }
        break;

    case GS_EVENT_CAMPAIGN_CHEAT:
        if (!mission_load_up_campaign ()) { readyroom_continue_campaign (); }
        break;

    case GS_EVENT_CAMPAIGN_ROOM:
        gameseq_set_state (GS_STATE_CAMPAIGN_ROOM);
        break;

    case GS_EVENT_CREDITS: gameseq_set_state (GS_STATE_CREDITS); break;

    case GS_EVENT_VIEW_MEDALS:
        gameseq_push_state (GS_STATE_VIEW_MEDALS);
        break;

    case GS_EVENT_SHOW_GOALS:
        gameseq_push_state (
            GS_STATE_SHOW_GOALS); // use push_state() since we might get to
                                  // this screen through a variety of states
        break;

    case GS_EVENT_HOTKEY_SCREEN:
        gameseq_push_state (
            GS_STATE_HOTKEY_SCREEN); // use push_state() since we might get to
                                     // this screen through a variety of states
        break;

    case GS_EVENT_GOTO_VIEW_CUTSCENES_SCREEN:
        gameseq_set_state (GS_STATE_VIEW_CUTSCENES);
        break;

    case GS_EVENT_INGAME_PRE_JOIN:
        gameseq_set_state (GS_STATE_INGAME_PRE_JOIN);
        break;

    case GS_EVENT_EVENT_DEBUG:
        gameseq_push_state (GS_STATE_EVENT_DEBUG);
        break;

    // Start a warpout where player automatically goes 70 no matter what
    // and can't cancel out of it.
    case GS_EVENT_PLAYER_WARPOUT_START_FORCED:
        Warpout_forced =
            1; // If non-zero, bash the player to speed and go through effect

        // Same code as in GS_EVENT_PLAYER_WARPOUT_START only ignores current
        // mode
        Player->saved_viewer_mode = Viewer_mode;
        Player->control_mode = PCM_WARPOUT_STAGE1;
        Warpout_sound =
            snd_play (gamesnd_get_game_sound (GameSounds::PLAYER_WARP_OUT));
        Warpout_time = 0.0f; // Start timer!
        break;

    case GS_EVENT_PLAYER_WARPOUT_START:
        if (Player->control_mode != PCM_NORMAL) {
            WARNINGF (LOCATION, "Player isn't in normal mode; cannot warp out.");
        }
        else {
            Player->saved_viewer_mode = Viewer_mode;
            Player->control_mode = PCM_WARPOUT_STAGE1;
            Warpout_sound = snd_play (
                gamesnd_get_game_sound (GameSounds::PLAYER_WARP_OUT));
            Warpout_time = 0.0f; // Start timer!
            Warpout_forced = 0; // If non-zero, bash the player to speed and go
                                // through effect
        }
        break;

    case GS_EVENT_PLAYER_WARPOUT_STOP:
        if (Player->control_mode != PCM_NORMAL) {
            if (!Warpout_forced) { // cannot cancel forced warpout
                Player->control_mode = PCM_NORMAL;
                Viewer_mode = Player->saved_viewer_mode;
                hud_subspace_notify_abort ();
                WARNINGF (LOCATION, "Player put back to normal mode.");
                if (Warpout_sound.isValid ()) {
                    snd_stop (Warpout_sound);
                    Warpout_sound = sound_handle::invalid ();
                }
            }
        }
        break;

    case GS_EVENT_PLAYER_WARPOUT_DONE_STAGE1: // player ship got up to speed
        if (Player->control_mode != PCM_WARPOUT_STAGE1) {
            gameseq_post_event (GS_EVENT_PLAYER_WARPOUT_STOP);
            WARNINGF (LOCATION,"Player put back to normal mode, because of invalid sequence in stage1.");
        }
        else {
            WARNINGF (LOCATION,"Hit target speed.  Starting warp effect and moving to stage 2!");
            shipfx_warpout_start (Player_obj);
            Player->control_mode = PCM_WARPOUT_STAGE2;

            if (!(The_mission.ai_profile
                      ->flags[AI::Profile_Flags::No_warp_camera])) {
                Player->saved_viewer_mode = Viewer_mode;
                Viewer_mode |= VM_WARP_CHASE;
                Warp_camera = warp_camera (Player_obj);
            }
        }
        break;

    case GS_EVENT_PLAYER_WARPOUT_DONE_STAGE2: // player ship got into the warp
                                              // effect
        if (Player->control_mode != PCM_WARPOUT_STAGE2) {
            gameseq_post_event (GS_EVENT_PLAYER_WARPOUT_STOP);
            WARNINGF (LOCATION,"Player put back to normal mode, because of invalid sequence in stage2.");
        }
        else {
            WARNINGF (LOCATION, "Hit warp effect.  Moving to stage 3!");
            Player->control_mode = PCM_WARPOUT_STAGE3;
        }
        break;

    case GS_EVENT_PLAYER_WARPOUT_DONE: // player ship got through the warp
                                       // effect
        WARNINGF (LOCATION, "Player warped out.  Going to debriefing!");
        Player->control_mode = PCM_NORMAL;
        Viewer_mode = Player->saved_viewer_mode;
        Warpout_sound = sound_handle::invalid ();

        gameseq_post_event (GS_EVENT_DEBRIEF);
        break;

    case GS_EVENT_INITIAL_PLAYER_SELECT:
        gameseq_set_state (GS_STATE_INITIAL_PLAYER_SELECT);
        break;

    case GS_EVENT_GAME_INIT:
        // see if the command line option has been set to use the last pilot,
        // and act acoordingly
        if (player_select_get_last_pilot ()) {
            // always enter the main menu -- do the automatic network startup
            // stuff elsewhere so that we still have valid checks for
            // networking modes, etc.
            gameseq_set_state (GS_STATE_MAIN_MENU);
        }
        else {
            gameseq_set_state (GS_STATE_INITIAL_PLAYER_SELECT);
        }
        break;

    case GS_EVENT_END_CAMPAIGN:
        gameseq_set_state (GS_STATE_END_OF_CAMPAIGN);
        break;

    case GS_EVENT_LOOP_BRIEF: gameseq_set_state (GS_STATE_LOOP_BRIEF); break;

    case GS_EVENT_FICTION_VIEWER:
        gameseq_set_state (GS_STATE_FICTION_VIEWER);
        break;

    default: ASSERT (0); break;
    }
}

// Called when a state is being left.
// The current state is still at old_state, but as soon as
// this function leaves, then the current state will become
// new state.     You should never try to change the state
// in here... if you think you need to, you probably really
// need to post an event, not change the state.
void game_leave_state (int old_state, int new_state) {
    int end_mission = 1;

    switch (new_state) {
    case GS_STATE_GAME_PAUSED:
    case GS_STATE_DEBUG_PAUSED:
    case GS_STATE_OPTIONS_MENU:
    case GS_STATE_CONTROL_CONFIG:
    case GS_STATE_MISSION_LOG_SCROLLBACK:
    case GS_STATE_DEATH_DIED:
    case GS_STATE_SHOW_GOALS:
    case GS_STATE_HOTKEY_SCREEN:
    case GS_STATE_TRAINING_PAUSED:
    case GS_STATE_EVENT_DEBUG:
    case GS_STATE_GAMEPLAY_HELP:
    case GS_STATE_LAB:
        end_mission = 0; // these events shouldn't end a mission
        break;
    }

    switch (old_state) {
    case GS_STATE_BRIEFING:
        brief_stop_voices ();
        if ((new_state != GS_STATE_OPTIONS_MENU) &&
            (new_state != GS_STATE_WEAPON_SELECT) &&
            (new_state != GS_STATE_SHIP_SELECT) &&
            (new_state != GS_STATE_HOTKEY_SCREEN) &&
            (new_state != GS_STATE_TEAM_SELECT)) {
            common_select_close ();
            if (new_state == GS_STATE_MAIN_MENU) { freespace_stop_mission (); }
        }
        break;

    case GS_STATE_DEBRIEF:
        if ((new_state != GS_STATE_VIEW_MEDALS) &&
            (new_state != GS_STATE_OPTIONS_MENU)) {
            debrief_close ();
        }
        break;

    case GS_STATE_LOAD_MISSION_MENU: mission_load_menu_close (); break;

    case GS_STATE_SIMULATOR_ROOM: sim_room_close (); break;

    case GS_STATE_CAMPAIGN_ROOM: campaign_room_close (); break;

    case GS_STATE_CMD_BRIEF:
        if (new_state == GS_STATE_OPTIONS_MENU) { cmd_brief_hold (); }
        else {
            cmd_brief_close ();
            common_select_close ();
            if (new_state == GS_STATE_MAIN_MENU) { freespace_stop_mission (); }
        }
        break;

    case GS_STATE_RED_ALERT:
        red_alert_close ();
        common_select_close ();
        if (new_state == GS_STATE_MAIN_MENU) { freespace_stop_mission (); }
        break;

    case GS_STATE_SHIP_SELECT:
        if (new_state != GS_STATE_OPTIONS_MENU &&
            new_state != GS_STATE_WEAPON_SELECT &&
            new_state != GS_STATE_HOTKEY_SCREEN &&
            new_state != GS_STATE_BRIEFING &&
            new_state != GS_STATE_TEAM_SELECT) {
            common_select_close ();
            if (new_state == GS_STATE_MAIN_MENU) { freespace_stop_mission (); }
        }
        break;

    case GS_STATE_WEAPON_SELECT:
        if (new_state != GS_STATE_OPTIONS_MENU &&
            new_state != GS_STATE_SHIP_SELECT &&
            new_state != GS_STATE_HOTKEY_SCREEN &&
            new_state != GS_STATE_BRIEFING &&
            new_state != GS_STATE_TEAM_SELECT) {
            common_select_close ();
            if (new_state == GS_STATE_MAIN_MENU) { freespace_stop_mission (); }
        }
        break;

    case GS_STATE_TEAM_SELECT:
        if (new_state != GS_STATE_OPTIONS_MENU &&
            new_state != GS_STATE_SHIP_SELECT &&
            new_state != GS_STATE_HOTKEY_SCREEN &&
            new_state != GS_STATE_BRIEFING &&
            new_state != GS_STATE_WEAPON_SELECT) {
            common_select_close ();
            if (new_state == GS_STATE_MAIN_MENU) { freespace_stop_mission (); }
        }
        break;

    case GS_STATE_MAIN_MENU: main_hall_close (); break;

    case GS_STATE_OPTIONS_MENU:
        options_menu_close ();
        break;

    case GS_STATE_BARRACKS_MENU:
        if (new_state != GS_STATE_VIEW_MEDALS) { barracks_close (); }
        break;

    case GS_STATE_MISSION_LOG_SCROLLBACK: hud_scrollback_close (); break;

    case GS_STATE_TRAINING_MENU: training_menu_close (); break;

    case GS_STATE_GAME_PLAY:
        tracing::async::end (tracing::MainFrame, tracing::MainFrameScope);

        if (!(Game_mode & GM_STANDALONE_SERVER)) {
            player_save_target_and_weapon_link_prefs ();
            game_stop_looped_sounds ();
        }

        sound_env_disable ();
        joy_ff_stop_effects ();

        // stop game time under certain conditions
        if (end_mission) {
            game_stop_time ();
        }

        if (end_mission) {
            snd_aav_init ();
            freespace_stop_mission ();

            if (Cmdline_benchmark_mode) {
                gameseq_post_event (GS_EVENT_QUIT_GAME);
            }
        }
        io::mouse::CursorManager::get ()->showCursor (true);
        break;

    case GS_STATE_TECH_MENU: techroom_close (); break;

    case GS_STATE_TRAINING_PAUSED: Training_num_lines = 0;
    case GS_STATE_GAME_PAUSED:
        game_start_time ();
        if (end_mission) { pause_close (); }
        break;

    case GS_STATE_DEBUG_PAUSED:
#ifndef NDEBUG
        game_start_time ();
        pause_debug_close ();
#endif
        break;

    case GS_STATE_HUD_CONFIG: hud_config_close (); break;

    case GS_STATE_CONTROL_CONFIG: control_config_close (); break;

    case GS_STATE_DEATH_DIED:
        Game_mode &= ~GM_DEAD_DIED;

        if (end_mission && (new_state == GS_STATE_DEBRIEF)) {
            freespace_stop_mission ();
        }
        break;

    case GS_STATE_DEATH_BLEW_UP:
        Game_mode &= ~GM_DEAD_BLEW_UP;

        if (end_mission) { freespace_stop_mission (); }

        break;

    case GS_STATE_CREDITS:
        credits_close ();
        main_hall_start_music ();
        break;

    case GS_STATE_VIEW_MEDALS: medal_main_close (); break;

    case GS_STATE_SHOW_GOALS: mission_show_goals_close (); break;

    case GS_STATE_HOTKEY_SCREEN:
        if (new_state != GS_STATE_OPTIONS_MENU) { mission_hotkey_close (); }
        break;

    case GS_STATE_VIEW_CUTSCENES: cutscenes_screen_close (); break;
    case GS_STATE_INITIAL_PLAYER_SELECT: player_select_close (); break;
    case GS_STATE_END_OF_CAMPAIGN: mission_campaign_end_close (); break;
    case GS_STATE_LOOP_BRIEF: loop_brief_close (); break;

    case GS_STATE_FICTION_VIEWER:
        fiction_viewer_close ();
        common_select_close ();
        if (new_state == GS_STATE_MAIN_MENU) { freespace_stop_mission (); }
        break;

    case GS_STATE_LAB: lab_close (); break;
    }
}

// Called when a state is being entered.
// The current state is set to the state we're entering at
// this point, and old_state is set to the state we're coming
// from.    You should never try to change the state
// in here... if you think you need to, you probably really
// need to post an event, not change the state.

void game_enter_state (int old_state, int new_state) {
    switch (new_state) {
    case GS_STATE_MAIN_MENU:
        // set the game_mode based on the type of player
        ASSERT (Player);
        Game_mode = GM_NORMAL;

        // determine which ship this guy is currently based on
        mission_load_up_campaign (Player);

        // if we're coming from the end of a campaign, we want to load the
        // first mainhall of the campaign otherwise load the mainhall for the
        // mission the player's up to
        if (Campaign.next_mission == -1) {
            main_hall_init (Campaign.missions[0].main_hall);
        }
        else {
            main_hall_init (
                Campaign.missions[Campaign.next_mission].main_hall);
        }

        if (Cmdline_start_mission) {
            strcpy (Game_current_mission_filename, Cmdline_start_mission);
            WARNINGF (LOCATION, "Straight to mission '%s'",Game_current_mission_filename);
            gameseq_post_event (GS_EVENT_START_GAME);
            // This stops the mission from loading again when you go back to
            // the hall
            Cmdline_start_mission = NULL;
        }
        break;

    case GS_STATE_START_GAME:
        main_hall_stop_music (true);
        main_hall_stop_ambient ();

        if (Game_mode & GM_NORMAL) {
            // this should put us into a new state on failure!
            if (!game_start_mission ()) break;
        }

        // maybe play a movie before the mission
        mission_campaign_maybe_play_movie (CAMPAIGN_MOVIE_PRE_MISSION);

        // determine where to go next
        if (mission_has_fiction ()) {
            gameseq_post_event (GS_EVENT_FICTION_VIEWER);
        }
        else if (mission_has_cmd_brief ()) {
            gameseq_post_event (GS_EVENT_CMD_BRIEF);
        }
        else if (red_alert_mission ()) {
            gameseq_post_event (GS_EVENT_RED_ALERT);
        }
        else {
            gameseq_post_event (GS_EVENT_START_BRIEFING);
        }
        break;

    case GS_STATE_FICTION_VIEWER:
        common_maybe_play_cutscene (MOVIE_PRE_FICTION);
        fiction_viewer_init ();
        break;

    case GS_STATE_CMD_BRIEF: {
        if (old_state == GS_STATE_OPTIONS_MENU) { cmd_brief_unhold (); }
        else {
            common_maybe_play_cutscene (MOVIE_PRE_CMD_BRIEF);
            int team_num =
                0; // team number used as index for which cmd brief to use.
            cmd_brief_init (team_num);
        }
        break;
    }

    case GS_STATE_RED_ALERT:
        common_maybe_play_cutscene (MOVIE_PRE_BRIEF);
        red_alert_init ();
        break;

    case GS_STATE_BRIEFING:
        common_maybe_play_cutscene (MOVIE_PRE_BRIEF);
        brief_init ();
        break;

    case GS_STATE_DEBRIEF:
        game_stop_looped_sounds ();
        mission_goal_fail_incomplete (); // fail all incomplete goals before
                                         // entering debriefing
        if ((old_state != GS_STATE_VIEW_MEDALS) &&
            (old_state != GS_STATE_OPTIONS_MENU)) {
            common_maybe_play_cutscene (MOVIE_PRE_DEBRIEF);
            debrief_init ();
        }
        break;

    case GS_STATE_LOAD_MISSION_MENU: mission_load_menu_init (); break;

    case GS_STATE_SIMULATOR_ROOM: sim_room_init (); break;

    case GS_STATE_CAMPAIGN_ROOM: campaign_room_init (); break;

    case GS_STATE_SHIP_SELECT: ship_select_init (); break;

    case GS_STATE_WEAPON_SELECT: weapon_select_init (); break;

    case GS_STATE_GAME_PAUSED:
        game_stop_time ();
        pause_init ();
        break;

    case GS_STATE_DEBUG_PAUSED:
        // game_stop_time();
        // fs2::os::title("FreeSpace 2 - PAUSED");
        // break;
        //
    case GS_STATE_TRAINING_PAUSED:
#ifndef NDEBUG
        game_stop_time ();
        pause_debug_init ();
#endif
        break;

    case GS_STATE_OPTIONS_MENU:
        // game_stop_time();
        options_menu_init ();
        break;

    case GS_STATE_GAME_PLAY:
        // maybe play a cutscene
        if ((old_state == GS_STATE_BRIEFING) ||
            (old_state == GS_STATE_SHIP_SELECT) ||
            (old_state == GS_STATE_WEAPON_SELECT) ||
            (old_state == GS_STATE_RED_ALERT)) {
            common_maybe_play_cutscene (MOVIE_PRE_GAME);
        }
        // reset time compression to default level so it's right at the
        // beginning of a mission - taylor
        if (old_state != GS_STATE_GAME_PAUSED) {
            // Game_time_compression = F1_0;
        }

        /* game could be comming from a restart (rather than first start)
        so make sure that we zero the hud gauge overrides (Sexp_hud_*)
        \sa sexp_hud_display_gauge*/
        if ((old_state == GS_STATE_GAME_PLAY) ||
            (old_state == GS_STATE_BRIEFING) ||
            (old_state == GS_STATE_DEBRIEF) ||
            (old_state == GS_STATE_SHIP_SELECT) ||
            (old_state == GS_STATE_WEAPON_SELECT) ||
            (old_state == GS_STATE_RED_ALERT)) {
            Sexp_hud_display_warpout = 0;
        }

        // Goober5000 - people may not have realized that pausing causes this
        // state to be re-entered
        if ((old_state != GS_STATE_GAME_PAUSED) &&
            (old_state != GS_STATE_MAIN_MENU)) {
            radar_mission_init ();

            // Set the current hud
            set_current_hud ();

            ship_init_cockpit_displays (Player_ship);
        }

        // coming from the gameplay state or the main menu, we might need to
        // load the mission
        if ((Game_mode & GM_NORMAL) &&
            ((old_state == GS_STATE_MAIN_MENU) ||
             (old_state == GS_STATE_GAME_PLAY) ||
             (old_state == GS_STATE_DEATH_BLEW_UP))) {
            if (!game_start_mission ()) // this should put us into a new state.
                // Failed!!!
                break;
        }

        // if we are coming from the briefing, ship select, weapons loadout, or
        // main menu (in the case of quick start), then do bitmap loads, etc
        // Don't do any of the loading stuff if we are in multiplayer -- this
        // stuff is all handled in the multi-wait section
        if (old_state == GS_STATE_BRIEFING       ||
            old_state == GS_STATE_SHIP_SELECT    ||
            old_state == GS_STATE_WEAPON_SELECT  ||
            old_state == GS_STATE_MAIN_MENU      ||
            old_state == GS_STATE_SIMULATOR_ROOM) {
            // JAS: Used to do all paging here.

#ifndef NDEBUG
            // XSTR:OFF
            HUD_printf (
                "Skill level is set to ** %s **",
                Skill_level_names (Game_skill_level));
// XSTR:ON
#endif

            main_hall_stop_music (true);
            main_hall_stop_ambient ();
            event_music_first_pattern (); // start the first pattern
        }

        // special code that restores player ship selection and weapons loadout
        // when doing a quick start
        if (!(Game_mode & GM_MULTIPLAYER) &&
            ((old_state == GS_STATE_MAIN_MENU) ||
             (old_state == GS_STATE_DEATH_BLEW_UP) ||
             (old_state == GS_STATE_GAME_PLAY))) {
            if (!strcasecmp (
                    Player_loadout.filename, Game_current_mission_filename)) {
                wss_direct_restore_loadout ();
            }
        }

        // single-player, quick-start after just died... we need to set weapon
        // linking and kick off the event music
        if (!(Game_mode & GM_MULTIPLAYER) &&
            (old_state == GS_STATE_DEATH_BLEW_UP)) {
            event_music_first_pattern (); // start the first pattern
        }

        if (old_state != GS_STATE_GAME_PAUSED) {
            event_music_first_pattern (); // start the first pattern
        }

        player_restore_target_and_weapon_link_prefs ();

        Game_mode |= GM_IN_MISSION;

#ifndef NDEBUG
        // required to truely make mouse deltas zeroed in debug mouse code
        void mouse_force_pos (int x, int y);
        mouse_force_pos (gr_screen.max_w / 2, gr_screen.max_h / 2);
#endif

        game_flush ();

        // only start time if in single player, or coming from multi wait state
        if (old_state != GS_STATE_VIEW_CUTSCENES)
            game_start_time ();

        Game_subspace_effect = 0;

        if (The_mission.flags[Mission::Mission_Flags::Subspace]) {
            Game_subspace_effect = 1;
            if (!(Game_mode & GM_STANDALONE_SERVER)) {
                game_start_subspace_ambient_sound ();
            }
        }

        sound_env_set (&Game_sound_env);

        joy_ff_mission_init (
            Ship_info[Player_ship->ship_info_index].rotation_time);

        io::mouse::CursorManager::get ()->showCursor (false, true);

        tracing::async::begin (tracing::MainFrame, tracing::MainFrameScope);
        break;

    case GS_STATE_HUD_CONFIG: hud_config_init (); break;

    case GS_STATE_CONTROL_CONFIG: control_config_init (); break;

    case GS_STATE_TECH_MENU: techroom_init (); break;

    case GS_STATE_BARRACKS_MENU:
        if (old_state != GS_STATE_VIEW_MEDALS) { barracks_init (); }
        break;

    case GS_STATE_MISSION_LOG_SCROLLBACK: hud_scrollback_init (); break;

    case GS_STATE_DEATH_DIED:
        Player_died_time = timestamp (10);

        if (!(Game_mode & GM_MULTIPLAYER)) { player_show_death_message (); }
        Game_mode |= GM_DEAD_DIED;
        break;

    case GS_STATE_DEATH_BLEW_UP:
        if (!popupdead_is_active ()) { Player_ai->target_objnum = -1; }

        // stop any local EMP effect
        emp_stop_local ();

        Players[Player_num].flags &=
            ~PLAYER_FLAGS_AUTO_TARGETING; // Prevent immediate switch to a
                                          // hostile ship.
        Game_mode |= GM_DEAD_BLEW_UP;
        Show_viewing_from_self = 0;

        // timestamp how long we should wait before displaying the died popup
        if (!popupdead_is_active ()) {
            Player_died_popup_wait = timestamp (PLAYER_DIED_POPUP_WAIT);
        }
        break;

    case GS_STATE_GAMEPLAY_HELP: gameplay_help_init (); break;

    case GS_STATE_CREDITS:
        main_hall_stop_music (true);
        main_hall_stop_ambient ();
        credits_init ();
        break;

    case GS_STATE_VIEW_MEDALS: medal_main_init (Player); break;
    case GS_STATE_SHOW_GOALS: mission_show_goals_init (); break;
    case GS_STATE_HOTKEY_SCREEN: mission_hotkey_init (); break;
    case GS_STATE_VIEW_CUTSCENES: cutscenes_screen_init (); break;
    case GS_STATE_INITIAL_PLAYER_SELECT: player_select_init (); break;
    case GS_STATE_END_OF_CAMPAIGN: mission_campaign_end_init (); break;
    case GS_STATE_LOOP_BRIEF: loop_brief_init (); break;
    case GS_STATE_LAB: lab_init (); break;
    default:
        break;
    } // end switch
}

// do stuff that may need to be done regardless of state
void game_do_state_common (int /* state */) {
    // determine if to draw the mouse this frame
    io::mouse::CursorManager::doFrame ();

    // update sound systems
    snd_do_frame ();

     // music needs to play across many states
    event_music_do_frame ();
}

// Called once a frame.
// You should never try to change the state
// in here... if you think you need to, you probably really
// need to post an event, not change the state.
int Game_do_state_should_skip = 0;

void game_do_state (int state) {
    // always lets the do_state_common() function determine if the state should
    // be skipped
    Game_do_state_should_skip = 0;

    //
    // Legal to set the should skip state anywhere in this function; do stuff
    // that may need to be done regardless of state.
    //
    game_do_state_common (state);

    if (Game_do_state_should_skip) { return; }

    switch (state) {
    case GS_STATE_MAIN_MENU:
        game_set_frametime (GS_STATE_MAIN_MENU);
        main_hall_do (flFrametime);
        break;

    case GS_STATE_OPTIONS_MENU:
        game_set_frametime (GS_STATE_OPTIONS_MENU);
        options_menu_do_frame (flFrametime);
        break;

    case GS_STATE_BARRACKS_MENU:
        game_set_frametime (GS_STATE_BARRACKS_MENU);
        barracks_do_frame (flFrametime);
        break;

    case GS_STATE_TRAINING_MENU:
        game_set_frametime (GS_STATE_TRAINING_MENU);
        training_menu_do_frame (flFrametime);
        break;

    case GS_STATE_TECH_MENU:
        game_set_frametime (GS_STATE_TECH_MENU);
        techroom_do_frame (flFrametime);
        break;

    case GS_STATE_GAMEPLAY_HELP:
        game_set_frametime (GS_STATE_GAMEPLAY_HELP);
        gameplay_help_do_frame (flFrametime);
        break;

    case GS_STATE_GAME_PLAY: // do stuff that should be done during gameplay
        game_do_frame ();
        break;

    case GS_STATE_GAME_PAUSED:

        if (pause_get_type () == PAUSE_TYPE_VIEWER) {
            read_player_controls (Player_obj, flFrametime);
            // game_process_keys();
            game_frame (true);
        }

        pause_do ();
        break;

    case GS_STATE_DEBUG_PAUSED:
#ifndef NDEBUG
        game_set_frametime (GS_STATE_DEBUG_PAUSED);
        pause_debug_do ();
#endif
        break;

    case GS_STATE_TRAINING_PAUSED: game_training_pause_do (); break;

    case GS_STATE_LOAD_MISSION_MENU:
        game_set_frametime (GS_STATE_LOAD_MISSION_MENU);
        mission_load_menu_do ();
        break;

    case GS_STATE_BRIEFING:
        game_set_frametime (GS_STATE_BRIEFING);
        brief_do_frame (flFrametime);
        break;

    case GS_STATE_DEBRIEF:
        game_set_frametime (GS_STATE_DEBRIEF);
        debrief_do_frame (flFrametime);
        break;

    case GS_STATE_SHIP_SELECT:
        game_set_frametime (GS_STATE_SHIP_SELECT);
        ship_select_do (flFrametime);
        break;

    case GS_STATE_WEAPON_SELECT:
        game_set_frametime (GS_STATE_WEAPON_SELECT);
        weapon_select_do (flFrametime);
        break;

    case GS_STATE_MISSION_LOG_SCROLLBACK:
        game_set_frametime (GS_STATE_MISSION_LOG_SCROLLBACK);
        hud_scrollback_do_frame (flFrametime);
        break;

    case GS_STATE_HUD_CONFIG:
        game_set_frametime (GS_STATE_HUD_CONFIG);
        hud_config_do_frame (flFrametime);
        break;

    case GS_STATE_CONTROL_CONFIG:
        game_set_frametime (GS_STATE_CONTROL_CONFIG);
        control_config_do_frame (flFrametime);
        break;

    case GS_STATE_DEATH_DIED: game_do_frame (); break;

    case GS_STATE_DEATH_BLEW_UP: game_do_frame (); break;

    case GS_STATE_SIMULATOR_ROOM:
        game_set_frametime (GS_STATE_SIMULATOR_ROOM);
        sim_room_do_frame (flFrametime);
        break;

    case GS_STATE_CAMPAIGN_ROOM:
        game_set_frametime (GS_STATE_CAMPAIGN_ROOM);
        campaign_room_do_frame (flFrametime);
        break;

    case GS_STATE_RED_ALERT:
        game_set_frametime (GS_STATE_RED_ALERT);
        red_alert_do_frame (flFrametime);
        break;

    case GS_STATE_CMD_BRIEF:
        game_set_frametime (GS_STATE_CMD_BRIEF);
        cmd_brief_do_frame (flFrametime);
        break;

    case GS_STATE_CREDITS:
        game_set_frametime (GS_STATE_CREDITS);
        credits_do_frame (flFrametime);
        break;

    case GS_STATE_VIEW_MEDALS:
        game_set_frametime (GS_STATE_VIEW_MEDALS);
        medal_main_do ();
        break;

    case GS_STATE_SHOW_GOALS:
        game_set_frametime (GS_STATE_SHOW_GOALS);
        mission_show_goals_do_frame (flFrametime);
        break;

    case GS_STATE_HOTKEY_SCREEN:
        game_set_frametime (GS_STATE_HOTKEY_SCREEN);
        mission_hotkey_do_frame (flFrametime);
        break;

    case GS_STATE_VIEW_CUTSCENES:
        game_set_frametime (GS_STATE_VIEW_CUTSCENES);
        cutscenes_screen_do_frame ();
        break;

    case GS_STATE_EVENT_DEBUG:
#ifndef NDEBUG
        game_set_frametime (GS_STATE_EVENT_DEBUG);
        game_show_event_debug (flFrametime);
#endif
        break;

    case GS_STATE_INITIAL_PLAYER_SELECT:
        game_set_frametime (GS_STATE_INITIAL_PLAYER_SELECT);
        player_select_do ();
        break;

    case GS_STATE_END_OF_CAMPAIGN: mission_campaign_end_do (); break;

    case GS_STATE_LOOP_BRIEF:
        game_set_frametime (GS_STATE_LOOP_BRIEF);
        loop_brief_do (flFrametime);
        break;

    case GS_STATE_FICTION_VIEWER:
        game_set_frametime (GS_STATE_FICTION_VIEWER);
        fiction_viewer_do_frame (flFrametime);
        break;

    case GS_STATE_LAB:
        game_set_frametime (GS_STATE_LAB);
        lab_do_frame (flFrametime);
        break;
    } // end switch(gs_current_state)
}

void game_spew_pof_info_sub (
    int model_num, polymodel* pm, int sm, CFILE* out, int* out_total,
    int* out_destroyed_total) {
    int i;
    int sub_total = 0;
    int sub_total_destroyed = 0;
    int total = 0;
    char str[255] = "";

    // get the total for all his children
    for (i = pm->submodel[sm].first_child; i >= 0;
         i = pm->submodel[i].next_sibling) {
        game_spew_pof_info_sub (
            model_num, pm, i, out, &sub_total, &sub_total_destroyed);
    }

    // find the # of faces for this _individual_ object
    total = submodel_get_num_polys (model_num, sm);
    if (strstr (pm->submodel[sm].name, "-destroyed")) {
        sub_total_destroyed = total;
    }

    // write out total
    sprintf (
        str, "Submodel %s total : %d faces\n", pm->submodel[sm].name, total);
    cfputs (str, out);

    *out_total += total + sub_total;
    *out_destroyed_total += sub_total_destroyed;
}

#define BAIL()                                     \
    do {                                           \
        int _idx;                                  \
        for (_idx = 0; _idx < num_files; _idx++) { \
            if (pof_list[_idx] != NULL) {          \
                free (pof_list[_idx]);          \
                pof_list[_idx] = NULL;             \
            }                                      \
        }                                          \
        return;                                    \
    } while (0);
void game_spew_pof_info () {
    char* pof_list[1000];
    int num_files;
    CFILE* out;
    int idx, model_num, i, j;
    polymodel* pm;
    int total, root_total, model_total, destroyed_total, counted;
    char str[255] = "";

    // get file list
    num_files = cf_get_file_list (1000, pof_list, CF_TYPE_MODELS, "*.pof");

    // spew info on all the pofs
    if (!num_files) { return; }

    // go
    out = cfopen ("pofspew.txt", "wt", CFILE_NORMAL, CF_TYPE_DATA);
    if (out == NULL) { BAIL (); }
    counted = 0;
    for (idx = 0; idx < num_files; idx++, counted++) {
        sprintf (str, "%s.pof", pof_list[idx]);
        model_num = model_load (str, 0, NULL);
        if (model_num >= 0) {
            pm = model_get (model_num);

            // if we have a real model
            if (pm != NULL) {
                cfputs (str, out);
                cfputs ("\n", out);

                // go through and print all raw submodels
                cfputs ("RAW\n", out);
                total = 0;
                model_total = 0;
                for (i = 0; i < pm->n_models; i++) {
                    total = submodel_get_num_polys (model_num, i);

                    model_total += total;
                    sprintf (
                        str, "Submodel %s total : %d faces\n",
                        pm->submodel[i].name, total);
                    cfputs (str, out);
                }
                sprintf (str, "Model total %d\n", model_total);
                cfputs (str, out);

                // now go through and do it by LOD
                cfputs ("BY LOD\n\n", out);
                for (i = 0; i < pm->n_detail_levels; i++) {
                    sprintf (str, "LOD %d\n", i);
                    cfputs (str, out);

                    // submodels
                    root_total =
                        submodel_get_num_polys (model_num, pm->detail[i]);
                    total = 0;
                    destroyed_total = 0;
                    for (j = pm->submodel[pm->detail[i]].first_child; j >= 0;
                         j = pm->submodel[j].next_sibling) {
                        game_spew_pof_info_sub (
                            model_num, pm, j, out, &total, &destroyed_total);
                    }

                    sprintf (
                        str, "Submodel %s total : %d faces\n",
                        pm->submodel[pm->detail[i]].name, root_total);
                    cfputs (str, out);

                    sprintf (str, "TOTAL: %d\n", total + root_total);
                    cfputs (str, out);
                    sprintf (
                        str, "TOTAL not counting destroyed faces %d\n",
                        (total + root_total) - destroyed_total);
                    cfputs (str, out);
                    sprintf (
                        str, "TOTAL destroyed faces %d\n\n", destroyed_total);
                    cfputs (str, out);
                }
                cfputs (
                    "---------------------------------------------------------"
                    "---------------\n\n",
                    out);
            }
            // Free memory of this model again
            model_unload (model_num);
        }
    }

    cfclose (out);
    model_free_all ();
    BAIL ();
}

DCF (pofspew, "Spews POF info without shutting down the game") {
    game_spew_pof_info ();
}

/**
 * Does some preliminary checks and then enters main event loop.
 *
 * @returns 0 on a clean exit, or
 * @returns 1 on an error
 */
int game_main (int argc, char* argv[]) {
    int state;

    // check if networking should be disabled, this could probably be done
    // later but the sooner the better
    // TODO: remove this when multi is fixed to handle more than
    // MAX_SHIP_CLASSES_MULTI
    if (Ship_info.size () > MAX_SHIP_CLASSES_MULTI) {
        Networking_disabled = 1;
    }

    if (!parse_cmdline (argc, argv)) { return 1; }

    game_init ();

    game_stop_time ();

    // maybe spew pof stuff
    if (Cmdline_spew_pof_info) {
        game_spew_pof_info ();
        game_shutdown ();
        return 0;
    }

    // maybe spew VP CRCs, and exit
    if (Cmdline_verify_vps) {
        extern void cfile_spew_pack_file_crcs ();
        cfile_spew_pack_file_crcs ();
        game_shutdown ();
        return 0;
    }

    movie::play ("intro.mve");

    gameseq_post_event (GS_EVENT_GAME_INIT);

    while (1) {
        // only important for non THREADED mode
        fs2::os::events::process_all ();

        state = gameseq_process_events ();
        if (state == GS_STATE_QUIT_GAME)
            break;

        // Since tracing is always active this needs to happen in the main loop
        tracing::process_events ();
    }

    game_shutdown ();

    return 0;
}

void game_shutdown () {
    // don't ever flip a page on the standalone!
    if (!(Game_mode & GM_STANDALONE_SERVER)) {
        gr_reset_clip ();
        gr_clear ();
        gr_flip ();
    }

    // if the player has left the "player select" screen and quit the game
    // without actually choosing a player, Player will be NULL, in which case
    // we shouldn't write the player file out!
    if (Player) {
        Pilot.save_player ();
        Pilot.save_savefile ();
    }

    particle::ParticleManager::shutdown ();
    batching_shutdown ();

    // load up common multiplayer icons
    hud_close ();
    fireball_close ();  // free fireball system
    particle::close (); // close out the particle system
    weapon_close ();    // free any memory that was allocated for the weapons
    ship_close ();      // free any memory that was allocated for the ships
    hud_free_scrollback_list (); // free space allocated to store hud messages
                                 // in hud scrollback

    decals::shutdown ();

    io::mouse::CursorManager::shutdown ();

    mission_campaign_clear (); // clear out the campaign stuff
    message_mission_close ();  // clear loaded table data from message.tbl
    mission_parse_close (); // clear out any extra memory that may be in use by
                            // mission parsing

    sexp_shutdown ();

    if (Cmdline_old_collision_sys) {
        obj_pairs_close (); // free memory from object collision pairs
    }
    else {
        obj_reset_colliders ();
    }
    stars_close (); // clean out anything used by stars code

    // the menu close functions will unload the bitmaps if they were displayed
    // during the game
    main_hall_close ();
    training_menu_close ();

    // free left over memory from table parsing
    player_tips_close ();

    control_config_common_close ();
    io::joystick::shutdown ();

    audiostream_close ();
    snd_close ();
    event_music_close ();
    gamesnd_close (); // close out gamesnd, needs to happen *after* other
                      // sounds are closed

    obj_shutdown ();

    model_free_all ();
    bm_unload_all (); // unload/free bitmaps, has to be called *after*
                      // model_free_all()!

    tracing::shutdown ();

    gr_close ();
    fs2::os::fini ();
    cfile_close ();

    // although the comment in cmdline.cpp said this isn't needed,
    // Valgrind disagrees (quite possibly incorrectly), but this is just
    // cleaner
    if (Cmdline_mod != NULL) {
        delete[] Cmdline_mod;
        Cmdline_mod = NULL;
    }

    lcl_xstr_close ();

#if 0 // don't have an updater for fs2_open
      // HACKITY HACK HACK
    // if this flag is set, we should be firing up the launcher when exiting freespace
        extern int Multi_update_fireup_launcher_on_exit;
        if(Multi_update_fireup_launcher_on_exit){
                game_launch_launcher_on_exit();
        }
#endif
}

// game_stop_looped_sounds()
//
// This function will call the appropriate stop looped sound functions for
// those modules which use looping sounds.  It is not enough just to stop a
// looping sound at the DirectSound level, the game is keeping track of looping
// sounds, and this function is used to inform the game that looping sounds are
// being halted.
//
void game_stop_looped_sounds () {
    hud_stop_looped_locking_sounds ();
    hud_stop_looped_engine_sounds ();
    afterburner_stop_sounds ();
    player_stop_looped_sounds ();
    obj_snd_stop_all (); // stop all object-linked persistant sounds
    game_stop_subspace_ambient_sound ();
    snd_stop (Radar_static_looping);
    Radar_static_looping = sound_handle::invalid ();
    snd_stop (Target_static_looping);
    shipfx_stop_engine_wash_sound ();
    Target_static_looping = sound_handle::invalid ();
}

void game_do_training_checks () {
    int i, s;
    float d;
    waypoint_list* wplp;

    if (Training_context & TRAINING_CONTEXT_SPEED) {
        s = (int)Player_obj->phys_info.fspeed;
        if ((s >= Training_context_speed_min) &&
            (s <= Training_context_speed_max)) {
            if (!Training_context_speed_set) {
                Training_context_speed_set = 1;
                Training_context_speed_timestamp = timestamp ();
            }
        }
        else
            Training_context_speed_set = 0;
    }

    if (Training_context & TRAINING_CONTEXT_FLY_PATH) {
        wplp = Training_context_path;
        if (wplp->get_waypoints ().size () >
            (uint)Training_context_goal_waypoint) {
            i = Training_context_goal_waypoint;
            do {
                waypoint* wpt = find_waypoint_at_index (wplp, i);
                ASSERT (wpt != NULL);
                d = vm_vec_dist (wpt->get_pos (), &Player_obj->pos);
                if (d <= Training_context_distance) {
                    Training_context_at_waypoint = i;
                    if (Training_context_goal_waypoint == i) {
                        Training_context_goal_waypoint++;
                        snd_play (
                            gamesnd_get_game_sound (GameSounds::CARGO_REVEAL),
                            0.0f);
                    }

                    break;
                }

                i++;
                if ((uint)i == wplp->get_waypoints ().size ()) i = 0;

            } while (i != Training_context_goal_waypoint);
        }
    }

    if ((Players_target == UNINITIALIZED) ||
        (Player_ai->target_objnum != Players_target) ||
        (Player_ai->targeted_subsys != Players_targeted_subsys)) {
        Players_target = Player_ai->target_objnum;
        Players_targeted_subsys = Player_ai->targeted_subsys;
        Players_target_timestamp = timestamp ();
    }
    // following added by Sesquipedalian for is_missile_locked
    if ((Players_mlocked == UNINITIALIZED) ||
        (Player_ai->current_target_is_locked != Players_mlocked)) {
        Players_mlocked = Player_ai->current_target_is_locked;
        Players_mlocked_timestamp = timestamp ();
    }
}

/////////// Following is for event debug view screen

#ifndef NDEBUG

#define EVENT_DEBUG_MAX 5000
#define EVENT_DEBUG_EVENT 0x8000

int Event_debug_index[EVENT_DEBUG_MAX];
int ED_count;

void game_add_event_debug_index (int n, int indent) {
    if (ED_count < EVENT_DEBUG_MAX)
        Event_debug_index[ED_count++] = n | (indent << 16);
}

void game_add_event_debug_sexp (int n, int indent) {
    if (n < 0) return;

    if (Sexp_nodes[n].first >= 0) {
        game_add_event_debug_sexp (Sexp_nodes[n].first, indent);
        game_add_event_debug_sexp (Sexp_nodes[n].rest, indent);
        return;
    }

    game_add_event_debug_index (n, indent);
    if (Sexp_nodes[n].subtype == SEXP_ATOM_OPERATOR)
        game_add_event_debug_sexp (Sexp_nodes[n].rest, indent + 1);
    else
        game_add_event_debug_sexp (Sexp_nodes[n].rest, indent);
}

void game_event_debug_init () {
    int e;

    ED_count = 0;
    for (e = 0; e < Num_mission_events; e++) {
        game_add_event_debug_index (e | EVENT_DEBUG_EVENT, 0);
        game_add_event_debug_sexp (Mission_events[e].formula, 1);
    }
}

void game_show_event_debug (float /*frametime*/) {
    char buf[256];
    int i, k, z;
    int font_height, font_width;
    int y_index, y_max;
    static int scroll_offset = 0;

    k = game_check_key ();
    if (k) switch (k) {
        case KEY_UP:
        case KEY_PAD8:
            scroll_offset--;
            if (scroll_offset < 0) scroll_offset = 0;
            break;

        case KEY_DOWN:
        case KEY_PAD2: scroll_offset++; break;

        case KEY_PAGEUP:
            scroll_offset -= 20;
            if (scroll_offset < 0) scroll_offset = 0;
            break;

        case KEY_PAGEDOWN:
            scroll_offset += 20; // not font-independent, hard-coded since I
                                 // counted the lines!
            break;

        default:
            gameseq_post_event (GS_EVENT_PREVIOUS_STATE);
            key_flush ();
            break;
        } // end switch

    gr_clear ();
    gr_set_color_fast (&Color_bright);
    font::set_font (font::FONT1);
    gr_get_string_size (&font_width, NULL, NOX ("EVENT DEBUG VIEW"));

    gr_string (
        (gr_screen.clip_width_unscaled - font_width) / 2,
        gr_screen.center_offset_y + 15, NOX ("EVENT DEBUG VIEW"));

    gr_set_color_fast (&Color_normal);
    font::set_font (font::FONT1);
    gr_get_string_size (&font_width, &font_height, NOX ("test"));
    y_max = gr_screen.center_offset_y + gr_screen.center_h - font_height - 5;
    y_index = gr_screen.center_offset_y + 45;

    k = scroll_offset;
    while (k < ED_count) {
        if (y_index > y_max) break;

        z = Event_debug_index[k];
        if (z & EVENT_DEBUG_EVENT) {
            z &= 0x7fff;
            sprintf (
                buf, NOX ("%s%s (%s) %s%d %d"),
                (Mission_events[z].flags & MEF_CURRENT) ? NOX ("* ") : "",
                Mission_events[z].name,
                Mission_events[z].result ? NOX ("True") : NOX ("False"),
                (Mission_events[z].chain_delay < 0) ? "" : NOX ("x "),
                Mission_events[z].repeat_count, Mission_events[z].interval);
        }
        else {
            i = (z >> 16) * 3;
            buf[i] = 0;
            while (i--) buf[i] = ' ';

            strcat (buf, Sexp_nodes[z & 0x7fff].text);
            switch (Sexp_nodes[z & 0x7fff].value) {
            case SEXP_TRUE: strcat (buf, NOX (" (True)")); break;

            case SEXP_FALSE: strcat (buf, NOX (" (False)")); break;

            case SEXP_KNOWN_TRUE:
                strcat (buf, NOX (" (Always true)"));
                break;

            case SEXP_KNOWN_FALSE:
                strcat (buf, NOX (" (Always false)"));
                break;

            case SEXP_CANT_EVAL: strcat (buf, NOX (" (Can't eval)")); break;

            case SEXP_NAN:
            case SEXP_NAN_FOREVER:
                strcat (buf, NOX (" (Not a number)"));
                break;
            }
        }

        gr_printf_no_resize (
            gr_screen.center_offset_x + 10, y_index, "%s", buf);
        y_index += font_height;
        k++;
    }

    gr_flip ();
}

#endif // NDEBUG

#ifndef NDEBUG
FILE* Time_fp;
FILE* Texture_fp;

int Tmap_npixels = 0;
int Tmap_num_too_big = 0;
int Num_models_needing_splitting = 0;

void Time_model (int modelnum) {
    // mprintf(( "Timing ship '%s'\n", si->name ));

    vec3d eye_pos, model_pos;
    matrix eye_orient, model_orient;

    polymodel* pm = model_get (modelnum);

    size_t l = strlen (pm->filename);
    while ((l > 0)) {
        if ((l == '/') || (l == '\\') || (l == ':')) {
            l++;
            break;
        }
        l--;
    }
    char* pof_file = &pm->filename[l];

    int model_needs_splitting = 0;

    // fprintf( Texture_fp, "Model: %s\n", pof_file );
    int i;
    for (i = 0; i < pm->n_textures; i++) {
        char filename[1024];
        ubyte pal[768];
        texture_map* tmap = &pm->maps[i];

        for (int j = 0; j < TM_NUM_TYPES; j++) {
            int bmp_num = tmap->textures[j].GetOriginalTexture ();
            if (bmp_num > -1) {
                bm_get_palette (bmp_num, pal, filename);
                int w, h;
                bm_get_info (bmp_num, &w, &h);

                if ((w > 512) || (h > 512)) {
                    fprintf (
                        Texture_fp, "%s\t%s\t%d\t%d\n", pof_file, filename, w,
                        h);
                    Tmap_num_too_big++;
                    model_needs_splitting++;
                }
            }
            else {
                // fprintf( Texture_fp, "\tTexture %d is bogus\n", i );
            }
        }
    }

    if (model_needs_splitting) { Num_models_needing_splitting++; }
    eye_orient = model_orient = vmd_identity_matrix;
    eye_pos = model_pos = vmd_zero_vector;

    eye_pos.xyz.z = -pm->rad * 2.0f;

    vec3d eye_to_model;

    vm_vec_sub (&eye_to_model, &model_pos, &eye_pos);
    vm_vector_2_matrix (&eye_orient, &eye_to_model, NULL, NULL);

    fix t1 = timer_get_fixed_seconds ();

    angles_t ta;
    ta.p = ta.b = ta.h = 0.0f;
    int framecount = 0;

    Tmap_npixels = 0;

    int bitmaps_used_this_frame, bitmaps_new_this_frame;

    bm_get_frame_usage (&bitmaps_used_this_frame, &bitmaps_new_this_frame);

    modelstats_num_polys = modelstats_num_verts = 0;

    while (ta.h < PI2) {
        matrix m1;
        vm_angles_2_matrix (&m1, &ta);
        vm_matrix_x_matrix (&model_orient, &vmd_identity_matrix, &m1);

        gr_reset_clip ();
        // gr_clear();

        g3_start_frame (1);

        // WMC - I think I can set this to VIEWER_ZOOM_DEFAULT.
        // If it's not appropriate, use cam_get_current()
        g3_set_view_matrix (&eye_pos, &eye_orient, VIEWER_ZOOM_DEFAULT);

        model_clear_instance (modelnum);

        model_render_params render_info;
        render_info.set_detail_level_lock (0);
        model_set_detail_level (0); // use highest detail level
        model_render_immediate (
            &render_info, modelnum, &model_orient,
            &model_pos); //|MR_NO_POLYS );

        g3_end_frame ();
        // gr_flip();

        framecount++;
        ta.h += 0.1f;

        int k = key_inkey ();
        if (k == KEY_ESC) { exit (1); }
    }

    fix t2 = timer_get_fixed_seconds ();

    bm_get_frame_usage (&bitmaps_used_this_frame, &bitmaps_new_this_frame);

    modelstats_num_polys /= framecount;
    modelstats_num_verts /= framecount;

    Tmap_npixels /= framecount;

    WARNINGF (LOCATION, "'%s' is %.2f FPS", pof_file,float (framecount) / f2fl (t2 - t1));
    fprintf (
        Time_fp, "\"%s\"\t%.0f\t%d\t%d\t%d\t%d\n", pof_file,
        float (framecount) / f2fl (t2 - t1), bitmaps_used_this_frame,
        modelstats_num_polys, modelstats_num_verts, Tmap_npixels);
}

int Time_models = 0;
DCF_BOOL (time_models, Time_models)

void Do_model_timings_test () {
    if (!Time_models) return;

    WARNINGF (LOCATION, "Timing models!");

    int i;

    ubyte model_used[MAX_POLYGON_MODELS];
    int model_id[MAX_POLYGON_MODELS];
    for (i = 0; i < MAX_POLYGON_MODELS; i++) { model_used[i] = 0; }

    // Load them all
    for (auto sip = Ship_info.begin (); sip != Ship_info.end (); ++sip) {
        sip->model_num = model_load (sip->pof_file, 0, NULL);

        model_used[sip->model_num % MAX_POLYGON_MODELS]++;
        model_id[sip->model_num % MAX_POLYGON_MODELS] = sip->model_num;
    }

    Texture_fp = fopen (NOX ("ShipTextures.txt"), "wt");
    if (!Texture_fp) return;

    Time_fp = fopen (NOX ("ShipTimings.txt"), "wt");
    if (!Time_fp) return;

    fprintf (Time_fp, "Name\tFPS\tTRAM\tPolys\tVerts\tPixels\n");
    // fprintf( Time_fp, "FPS\tTRAM\tPolys\tVerts\tPixels\n" );

    for (i = 0; i < MAX_POLYGON_MODELS; i++) {
        if (model_used[i]) { Time_model (model_id[i]); }
    }

    fprintf (Texture_fp, "Number too big: %d\n", Tmap_num_too_big);
    fprintf (
        Texture_fp, "Number of models needing splitting: %d\n",
        Num_models_needing_splitting);

    fclose (Time_fp);
    fclose (Texture_fp);

    exit (1);
}
#endif

// Call this function when you want to inform the player that a feature is
// disabled in this build
void game_feature_disabled_popup () {
    popup (
        PF_USE_AFFIRMATIVE_ICON | PF_BODY_BIG, 1, POPUP_OK,
        XSTR (
            "Sorry, the requested feature is currently disabled in this build",
            1621));
}

// format the specified time (fixed point) into a nice string
void game_format_time (fix m_time, char* time_str) {
    float mtime;
    int hours, minutes, seconds;
    char tmp[10];

    mtime = f2fl (m_time);

    // get the hours, minutes and seconds
    hours = (int)(mtime / 3600.0f);
    if (hours > 0) { mtime -= (3600.0f * (float)hours); }
    seconds = (int)mtime % 60;
    minutes = (int)mtime / 60;

    // print the hour if necessary
    if (hours > 0) {
        sprintf (time_str, XSTR ("%d:", 201), hours);
        // if there are less than 10 minutes, print a leading 0
        if (minutes < 10) {
            strcpy (tmp, NOX ("0"));
            strcat (time_str, tmp);
        }
    }

    // print the minutes
    if (hours) {
        sprintf (tmp, XSTR ("%d:", 201), minutes);
        strcat (time_str, tmp);
    }
    else {
        sprintf (time_str, XSTR ("%d:", 201), minutes);
    }

    // print the seconds
    if (seconds < 10) {
        strcpy (tmp, NOX ("0"));
        strcat (time_str, tmp);
    }
    sprintf (tmp, "%d", seconds);
    strcat (time_str, tmp);
}

// Stuff version string in *str.
void get_version_string (char* str, int max_size) {
    // XSTR:OFF
    ASSERT (max_size > 6);

    sprintf (str, "FreeSpace 2 Open v%s", FS_VERSION_FULL);

#ifndef NDEBUG
    strcat (str, " Debug");
#endif

    // Lets get some more info in here
    switch (gr_screen.mode) {
    case GR_OPENGL: strcat (str, " OpenGL"); break;
    }
}

// ----------------------------------------------------------------
//
// Subspace Ambient Sound START
//
// ----------------------------------------------------------------

static sound_handle Subspace_ambient_left_channel = sound_handle::invalid ();
static sound_handle Subspace_ambient_right_channel = sound_handle::invalid ();

//
void game_start_subspace_ambient_sound () {
    if (!Subspace_ambient_left_channel.isValid ()) {
        Subspace_ambient_left_channel = snd_play_looping (
            gamesnd_get_game_sound (GameSounds::SUBSPACE_LEFT_CHANNEL), -1.0f);
    }

    if (!Subspace_ambient_right_channel.isValid ()) {
        Subspace_ambient_right_channel = snd_play_looping (
            gamesnd_get_game_sound (GameSounds::SUBSPACE_RIGHT_CHANNEL), 1.0f);
    }
}

void game_stop_subspace_ambient_sound () {
    if (Subspace_ambient_left_channel.isValid ()) {
        snd_stop (Subspace_ambient_left_channel);
        Subspace_ambient_left_channel = sound_handle::invalid ();
    }

    if (Subspace_ambient_right_channel.isValid ()) {
        snd_stop (Subspace_ambient_right_channel);
        Subspace_ambient_right_channel = sound_handle::invalid ();
    }
}

// ----------------------------------------------------------------
//
// Subspace Ambient Sound END
//
// ----------------------------------------------------------------

// ----------------------------------------------------------------
// Language autodetection stuff
//

// default setting is "-1" to use registry setting with English as fall back
// DO NOT change that default setting here or something uncouth might happen
// in the localization code
int detect_lang () {
    uint file_checksum;
    int idx;
    std::string first_font;

    // if the reg is set then let lcl_init() figure out what to do
    if (fs2::registry::read ("Default.Language", std::string ()).empty ())
        return -1;

    // try and open the file to verify
    font::stuff_first (first_font);
    CFILE* detect = cfopen (const_cast< char* > (first_font.c_str ()), "rb");

    // will use default setting if something went wrong
    if (!detect) return -1;

    // get the long checksum of the file
    file_checksum = 0;
    cfseek (detect, 0, SEEK_SET);
    cf_chksum_long (detect, &file_checksum);
    cfclose (detect);
    detect = NULL;

    // now compare the checksum/filesize against known #'s
    for (idx = 0; idx < NUM_BUILTIN_LANGUAGES; idx++) {
        if (Lcl_builtin_languages[idx].checksum == (int)file_checksum) {
            WARNINGF (LOCATION, "AutoLang: Language auto-detection successful...");
            return idx;
        }
    }

    // notify if a match was not found, include detected checksum
    ERRORF (
        LOCATION, "ERROR: Unknown Language Checksum: %i\n",
        (int)file_checksum);
    WARNINGF (LOCATION, "Using default language settings...");

    return -1;
}

//
// Eng Auto Lang stuff
// ----------------------------------------------------------------

// ----------------------------------------------------------------
// SHIPS TBL VERIFICATION STUFF
//

// checksums, just keep a list of all valid ones, if it matches any of them,
// keep it
#define NUM_SHIPS_TBL_CHECKSUMS 1

int Game_ships_tbl_checksums[NUM_SHIPS_TBL_CHECKSUMS] = {
    // -1022810006,                                    // 1.0 FULL
    -1254285366 // 1.2 FULL (German)
};

void verify_ships_tbl () {
    /*
#ifdef NDEBUG
    Game_ships_tbl_valid = 1;
#else
    */
    uint file_checksum;
    int idx;

    // detect if the packfile exists
    CFILE* detect = cfopen ("ships.tbl", "rb");
    Game_ships_tbl_valid = 0;

    // not mission-disk
    if (!detect) {
        Game_ships_tbl_valid = 0;
        return;
    }

    // get the long checksum of the file
    file_checksum = 0;
    cfseek (detect, 0, SEEK_SET);
    cf_chksum_long (detect, &file_checksum);
    cfclose (detect);
    detect = NULL;

    // now compare the checksum/filesize against known #'s
    for (idx = 0; idx < NUM_SHIPS_TBL_CHECKSUMS; idx++) {
        if (Game_ships_tbl_checksums[idx] == (int)file_checksum) {
            Game_ships_tbl_valid = 1;
            return;
        }
    }
    // #endif
}

DCF (shipspew, "display the checksum for the current ships.tbl") {
    uint file_checksum;
    CFILE* detect = cfopen ("ships.tbl", "rb");
    // get the long checksum of the file
    file_checksum = 0;
    cfseek (detect, 0, SEEK_SET);
    cf_chksum_long (detect, &file_checksum);
    cfclose (detect);

    dc_printf ("%d", file_checksum);
}

// ----------------------------------------------------------------
// WEAPONS TBL VERIFICATION STUFF
//

// checksums, just keep a list of all valid ones, if it matches any of them,
// keep it
#define NUM_WEAPONS_TBL_CHECKSUMS 1

int Game_weapons_tbl_checksums[NUM_WEAPONS_TBL_CHECKSUMS] = {
    // 399297860,                              // 1.0 FULL
    -553984927 // 1.2 FULL (german)
};

void verify_weapons_tbl () {
    /*
#ifdef NDEBUG
    Game_weapons_tbl_valid = 1;
#else
    */
    uint file_checksum;
    int idx;

    // detect if the packfile exists
    CFILE* detect = cfopen ("weapons.tbl", "rb");
    Game_weapons_tbl_valid = 0;

    // not mission-disk
    if (!detect) {
        Game_weapons_tbl_valid = 0;
        return;
    }

    // get the long checksum of the file
    file_checksum = 0;
    cfseek (detect, 0, SEEK_SET);
    cf_chksum_long (detect, &file_checksum);
    cfclose (detect);
    detect = NULL;

    // now compare the checksum/filesize against known #'s
    for (idx = 0; idx < NUM_WEAPONS_TBL_CHECKSUMS; idx++) {
        if (Game_weapons_tbl_checksums[idx] == (int)file_checksum) {
            Game_weapons_tbl_valid = 1;
            return;
        }
    }
    // #endif
}

DCF (wepspew, "display the checksum for the current weapons.tbl") {
    uint file_checksum;
    CFILE* detect = cfopen ("weapons.tbl", "rb");
    // get the long checksum of the file
    file_checksum = 0;
    cfseek (detect, 0, SEEK_SET);
    cf_chksum_long (detect, &file_checksum);
    cfclose (detect);

    dc_printf ("%d", file_checksum);
}

void game_title_screen_display () {
    Game_title_logo = bm_load (Game_logo_screen_fname[gr_screen.res]);
    Game_title_bitmap = bm_load (Game_title_screen_fname[gr_screen.res]);

    if (Game_title_bitmap != -1) {
        // set
        gr_set_bitmap (Game_title_bitmap);

        // get bitmap's width and height
        int width, height;
        bm_get_info (Game_title_bitmap, &width, &height);

        // set the screen scale to the bitmap's dimensions
        gr_set_screen_scale (width, height);

        // draw it in the center of the screen
        gr_bitmap (
            (gr_screen.max_w_unscaled - width) / 2,
            (gr_screen.max_h_unscaled - height) / 2, GR_RESIZE_MENU);

        if (Game_title_logo != -1) {
            gr_set_bitmap (Game_title_logo);

            gr_bitmap (0, 0, GR_RESIZE_MENU);
        }

        gr_reset_screen_scale ();
    }

    gr_flip ();
}

void game_title_screen_close () {
    if (Game_title_bitmap != -1) {
        bm_release (Game_title_bitmap);
        Game_title_bitmap = -1;
    }

    if (Game_title_logo != -1) {
        bm_release (Game_title_logo);
        Game_title_bitmap = -1;
    }
}

// return true if the game is running with "low memory", which is less than
// 48MB
bool game_using_low_mem () {
    if (Use_low_mem == 0) { return false; }
    else {
        return true;
    }
}

// place calls here that need to take effect immediately when the game is
// minimized.  Called from osapi.cpp
void game_pause () {
    // Protection against flipping out -- Kazan
    if (!GameState_Stack_Valid ()) return;

    if (!(Game_mode & GM_MULTIPLAYER)) {
        switch (gameseq_get_state ()) {
        case GS_STATE_MAIN_MENU:
            main_hall_pause (); // not an instant shutoff of misc anims and
                                // sounds
            break;

        case GS_STATE_BRIEFING: brief_pause (); break;

        case GS_STATE_DEBRIEF: debrief_pause (); break;

        case GS_STATE_CMD_BRIEF: cmd_brief_pause (); break;

        case GS_STATE_RED_ALERT: red_alert_voice_pause (); break;

        // anything that would leave the ambient mainhall sound going
        case GS_STATE_TECH_MENU:
        case GS_STATE_BARRACKS_MENU:
            main_hall_stop_ambient ();
            main_hall_stop_music (true); // not an instant shutoff
            break;

        // things that would get music except if they are called while
        // in-mission
        case GS_STATE_OPTIONS_MENU:
        case GS_STATE_HUD_CONFIG:
            if (!(Game_mode & GM_IN_MISSION)) {
                main_hall_stop_ambient ();
                main_hall_stop_music (true); // not an instant shutoff
            }
            break;

        // only has the ambient sound, no music
        case GS_STATE_INITIAL_PLAYER_SELECT: main_hall_stop_ambient (); break;

        // pause_init is a special case and we don't unpause it ourselves
        case GS_STATE_GAME_PLAY:
            if ((!popup_active ()) && (!popupdead_is_active ())) pause_init ();
            break;

        case GS_STATE_FICTION_VIEWER: fiction_viewer_pause (); break;

        default: audiostream_pause_all ();
        }
    }
}

// calls to be executed when the game is restored from minimized or inactive
// state
void game_unpause () {
    if (!GameState_Stack_Valid ()) return;

    // automatically recover from everything but an in-mission pause
    if (!(Game_mode & GM_MULTIPLAYER)) {
        switch (gameseq_get_state ()) {
        case GS_STATE_MAIN_MENU: main_hall_unpause (); break;

        case GS_STATE_BRIEFING: brief_unpause (); break;

        case GS_STATE_DEBRIEF: debrief_unpause (); break;

        case GS_STATE_CMD_BRIEF: cmd_brief_unpause (); break;

        case GS_STATE_RED_ALERT: red_alert_voice_unpause (); break;

        // anything that would leave the ambient mainhall sound going
        case GS_STATE_TECH_MENU:
        case GS_STATE_BARRACKS_MENU:
            main_hall_start_ambient ();
            main_hall_start_music (); // not an instant shutoff
            break;

        // things that would get music except if they are called while
        // in-mission
        case GS_STATE_OPTIONS_MENU:
        case GS_STATE_HUD_CONFIG:
            if (!(Game_mode & GM_IN_MISSION)) {
                main_hall_start_ambient ();
                main_hall_start_music (); // not an instant shutoff
            }
            break;

        // only has the ambient sound, no music
        case GS_STATE_INITIAL_PLAYER_SELECT: main_hall_start_ambient (); break;

        // if in a game then do nothing, pause_init() should have been called
        // and will get cleaned up elsewhere
        case GS_STATE_GAME_PLAY: break;

        // ditto for if we explicitly paused the game and then minimized it
        case GS_STATE_GAME_PAUSED: break;

        case GS_STATE_FICTION_VIEWER: fiction_viewer_unpause (); break;

        default: audiostream_unpause_all ();
        }
    }
}

int
main (int argc, char** argv) {
    srand (time (0));

    try {
        return game_main (argc, argv);
    }
    catch (const std::exception& x) {
        EE << x.what ();
        return 1;
    }
    catch (...) {
        EE << "unknown exception in main";
        return 1;
    }
}
