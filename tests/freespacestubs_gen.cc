// GENERATED trap stubs for game symbols the foundation references but
// whose subsystems are not ported yet.  Calling one aborts with its name;
// data symbols read as zeros.  Entries disappear as subsystems port.
// Regenerate: link without this file, feed the demangled undefined-symbol
// list to gen_gamestubs.py (see notes.txt).

#include <stdio.h>
#include <stdlib.h>

#include <globalincs/pstypes.hh>

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

void game_do_state_common(int)
{
	oracle_trap("game_do_state_common");
}

void game_do_state(int)
{
	oracle_trap("game_do_state");
}

void game_enter_state(int, int)
{
	oracle_trap("game_enter_state");
}

void game_flash(float, float, float)
{
	oracle_trap("game_flash");
}

void game_flush()
{
	oracle_trap("game_flush");
}

void game_get_default_skill_level()
{
	oracle_trap("game_get_default_skill_level");
}

void game_increase_skill_level()
{
	oracle_trap("game_increase_skill_level");
}

void game_leave_state(int, int)
{
	oracle_trap("game_leave_state");
}

void game_poll()
{
	oracle_trap("game_poll");
}

void game_process_event(int, int)
{
	oracle_trap("game_process_event");
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

void game_tst_mark(object*, ship*)
{
	oracle_trap("game_tst_mark");
}

void game_whack_apply(float, float)
{
	oracle_trap("game_whack_apply");
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
unsigned char Game_current_mission_filename[1 << 20];
unsigned char Game_ships_tbl_valid[1 << 20];
unsigned char Game_skill_level[1 << 20];
unsigned char Game_subspace_effect[1 << 20];
unsigned char Game_time_compression[1 << 20];
unsigned char Game_weapons_tbl_valid[1 << 20];
int Pofview_running = 0;
unsigned char Show_target_debug_info[1 << 20];
unsigned char Show_target_weapons[1 << 20];
unsigned char Sun_drew[1 << 20];
unsigned char Test_begin[1 << 20];
unsigned char tst[1 << 20];
unsigned char Viewer_zoom[1 << 20];
unsigned char Warpout_forced[1 << 20];
unsigned char Warpout_time[1 << 20];
