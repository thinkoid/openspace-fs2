// GENERATED trap stubs for game symbols the foundation references but
// whose subsystems are not ported yet.  Calling one aborts with its name;
// data symbols read as zeros.  Entries disappear as subsystems port.
// Regenerate: link without this file, feed the demangled undefined-symbol
// list to gen_gamestubs.py (see notes.txt).

#include <stdio.h>
#include <stdlib.h>

#include <gamesequence/gamesequence.hh>
#include <globalincs/pstypes.hh>
#include <hud/hud.hh>
#include <object/object.hh>
#include <playerman/player.hh>
#include <ship/shipfx.hh>

struct object;
struct ship;

static void oracle_trap(const char *sym)
{
   fprintf(stderr, "oracle strayed into unported code: %s\n", sym);
   abort();
}

void find_freespace_cd(char*)
{
   oracle_trap("find_freespace_cd");
}

void game_check_key()
{
   oracle_trap("game_check_key");
}

void game_do_cd_mission_check(char*)
{
   oracle_trap("game_do_cd_mission_check");
}

// The gamesequence quartet: NO-OPS (enter/leave/do are presentation
// hooks) and a one-transition state machine -- libfs2 boots the retail
// sequencer (gameseq_init + post + process) and every event lands in
// GS_STATE_GAME_PLAY, which is the only state the simulation library is
// ever in. sim-side code (message_queue_process) reads the state.
void game_do_state_common(int)
{
}

void game_do_state(int)
{
}

void game_enter_state(int, int)
{
}

struct fs_builtin_mission;

fs_builtin_mission *game_find_builtin_mission(char*)
{
   oracle_trap("game_find_builtin_mission");
   return nullptr;
}

// NO-OP: the big-damage screen flash (shiphit's path) is presentation
void game_flash(float, float, float)
{
}

void game_flush()
{
   oracle_trap("game_flush");
}

void game_format_time(fix, char*)
{
   oracle_trap("game_format_time");
}

// real behavior, not a trap: init_new_pilot (the boundary's Commander
// Jameson bootstrap) asks for it; retail returns DEFAULT_SKILL_LEVEL
// (freespace.cc:174), and the wrong-signature void trap was UB anyway
int game_get_default_skill_level()
{
   return 1;   // DEFAULT_SKILL_LEVEL, freespace.cc:174
}

void game_increase_skill_level()
{
   oracle_trap("game_increase_skill_level");
}

void game_leave_state(int, int)
{
}

void game_poll()
{
   oracle_trap("game_poll");
}

// The player-warpout event arc is real behavior, not a trap: freespace.cc
// (the unported game half this file stands in for) owns these handlers
// (freespace.cc:4127..4215), and the warpout stages travel retail's own
// event queue -- read_player_controls posts DONE_STAGE1, shipfx's
// warpout frame posts DONE_STAGE2/DONE, libfs2's step() drains the
// queue into here. Sim substance only: camera moves, viewer modes and
// the warpout-sound handle are presentation and stay out (the whoosh
// itself crosses the boundary through the Snd_capture seam when the
// boundary posts the start event). Everything else still lands in
// GS_STATE_GAME_PLAY, the only state the simulation library is ever in.
void game_process_event(int, int event)
{
    switch (event) {
    case GS_EVENT_PLAYER_WARPOUT_START:
        if (Player->control_mode == PCM_NORMAL) {
            Player->control_mode = PCM_WARPOUT_STAGE1;
            Warpout_time = 0.0f;
            Warpout_forced = 0;
        }
        break;

    case GS_EVENT_PLAYER_WARPOUT_STOP:
        if (Player->control_mode != PCM_NORMAL && !Warpout_forced) {
            Player->control_mode = PCM_NORMAL;
            hud_subspace_notify_abort();
        }
        break;

    case GS_EVENT_PLAYER_WARPOUT_DONE_STAGE1:   // up to warp speed
        if (Player->control_mode != PCM_WARPOUT_STAGE1) {
            gameseq_post_event(GS_EVENT_PLAYER_WARPOUT_STOP);
        }
        else {
            shipfx_warpout_start(Player_obj);
            Player->control_mode = PCM_WARPOUT_STAGE2;
        }
        break;

    case GS_EVENT_PLAYER_WARPOUT_DONE_STAGE2:   // reached the effect
        if (Player->control_mode != PCM_WARPOUT_STAGE2)
            gameseq_post_event(GS_EVENT_PLAYER_WARPOUT_STOP);
        else
            Player->control_mode = PCM_WARPOUT_STAGE3;
        break;

    case GS_EVENT_PLAYER_WARPOUT_DONE:          // through it -- departed
        Player->control_mode = PCM_NORMAL;
        break;
    }

    gameseq_set_state(GS_STATE_GAME_PLAY, 1);
}

void game_set_frametime(int)
{
   oracle_trap("game_set_frametime");
}

void game_set_view_clip()
{
   oracle_trap("game_set_view_clip");
}

void game_shudder_apply(int, float)
{
   oracle_trap("game_shudder_apply");
}

void game_start_time()
{
   oracle_trap("game_start_time");
}

void game_stop_looped_sounds()
{
   oracle_trap("game_stop_looped_sounds");
}

void game_stop_time()
{
   oracle_trap("game_stop_time");
}

// NO-OP, not a trap: retail's "hehe" easter-egg marker sits on
// ship_hit_kill's path (shiphit.cc:1374), so every simulated kill crosses
// it -- pure presentation (a screen flash), safely nothing here
void game_tst_mark(object*, ship*)
{
}

bool game_using_low_mem()
{
   oracle_trap("game_using_low_mem");
   return false;
}

// NO-OP: the getting-hit HUD shake is presentation (a live sim path --
// the player takes hits in campaign combat)
void game_whack_apply(float, float)
{
}

void get_version_string(char*)
{
   oracle_trap("get_version_string");
}

void set_cdrom_path(int)
{
   oracle_trap("set_cdrom_path");
}

// data symbols, zero-backed
unsigned char Camera_pos[1 << 20];
unsigned char Dead_player_last_vel[1 << 20];
unsigned char Debug_octant[1 << 20];
unsigned char flFrametime[1 << 20];
unsigned char Framerate_delay[1 << 20];
int Fred_running = 0;
unsigned char Freespace_gamma[1 << 20];
unsigned char Game_current_mission_filename[1 << 20];
unsigned char Game_ships_tbl_valid[1 << 20];
unsigned char Game_skill_level[1 << 20];
unsigned char Game_subspace_effect[1 << 20];
unsigned char Game_time_compression[1 << 20];
unsigned char Game_weapons_tbl_valid[1 << 20];
unsigned char game_single_step[1 << 20];
unsigned char last_single_step[1 << 20];
int Pofview_running = 0;
unsigned char Show_target_debug_info[1 << 20];
unsigned char Show_target_weapons[1 << 20];
unsigned char Sun_drew[1 << 20];
unsigned char Test_begin[1 << 20];
unsigned char tst[1 << 20];
unsigned char Viewer_zoom[1 << 20];

// live warpout state, typed (player.hh externs them; freespace.cc
// declared them) -- read_player_controls' stage ramp and the event arc
// above share them
int Warpout_forced = 0;
float Warpout_time = 0.0f;
