// GENERATED trap stubs for game symbols the foundation references but
// whose subsystems are not ported yet.  Calling one aborts with its name;
// data symbols read as zeros.  Entries disappear as subsystems port.
// Regenerate: link without this file, feed the demangled undefined-symbol
// list to gen_gamestubs.py (see notes.txt).

#include <stdio.h>
#include <stdlib.h>

#include "pstypes.h"

struct UI_WINDOW;
struct ai_info;
struct beam_info;
struct game_snd;
struct multi_local_options;
struct multi_server_options;
struct net_player;
struct object;
struct ship;
struct ship_subsys;
struct sound_env;
struct wing;

static void oracle_trap(const char *sym)
{
	fprintf(stderr, "oracle strayed into unported code: %s\n", sym);
	abort();
}

long audiostream_close()
{
	return -1;
}

long audiostream_close_all(int)
{
	return -1;
}

long audiostream_close_file(int, int)
{
	return -1;
}

long audiostream_done_reading(int)
{
	return -1;
}

long audiostream_get_bytes_committed(int)
{
	return -1;
}

long audiostream_is_inited()
{
	return -1;
}

long audiostream_is_paused(int)
{
	return -1;
}

long audiostream_is_playing(int)
{
	return -1;
}

long audiostream_open(char*, int)
{
	return -1;
}

long audiostream_pause_all()
{
	return -1;
}

long audiostream_pause(int)
{
	return -1;
}

long audiostream_play(int, float, int)
{
	return -1;
}

long audiostream_set_byte_cutoff(int, unsigned int)
{
	return -1;
}

long audiostream_set_volume_all(float, int)
{
	return -1;
}

long audiostream_stop(int, int, int)
{
	return -1;
}

long audiostream_unpause_all()
{
	return -1;
}

long audiostream_unpause(int)
{
	return -1;
}

void d3d_flush()
{
	oracle_trap("d3d_flush");
}

void d3d_zbias(int)
{
	oracle_trap("d3d_zbias");
}

void debug_console(void (*)())
{
}

long demo_close()
{
	return -1;	// demo recorder is gone
}

long demo_do_frame_end()
{
	return -1;	// demo recorder is gone
}

long demo_do_frame_start()
{
	return -1;	// demo recorder is gone
}

long demo_POST_builtin_message(int, ship*, int, int)
{
	return -1;	// demo recorder is gone
}

long demo_POST_departed(int, int)
{
	return -1;	// demo recorder is gone
}

long demo_POST_obj_create(char*, int)
{
	return -1;	// demo recorder is gone
}

long demo_POST_primary_fired(object*, int, int)
{
	return -1;	// demo recorder is gone
}

long demo_POST_ship_kill(object*)
{
	return -1;	// demo recorder is gone
}

long demo_POST_unique_message(char*, char*, int, int)
{
	return -1;	// demo recorder is gone
}

long demo_POST_warpin(int, int)
{
	return -1;	// demo recorder is gone
}

long demo_POST_warpout(int, int)
{
	return -1;	// demo recorder is gone
}

long demo_should_sim(object*)
{
	return -1;	// demo recorder is gone
}

long demo_start_playback(char*)
{
	return -1;	// demo recorder is gone
}

long demo_start_record(char*)
{
	return -1;	// demo recorder is gone
}

long ds3d_update_buffer(int, float, float, vector*, vector*)
{
	return -1;
}

long ds_get_channel(int)
{
	return -1;
}

long ds_get_play_position(int)
{
	return -1;
}

long ds_using_ds3d()
{
	return -1;
}

void find_player_id(short)
{
	oracle_trap("find_player_id");
}

void gr_d3d_activate(int)
{
	oracle_trap("gr_d3d_activate");
}

void gr_d3d_bitmap_ex(int, int, int, int, int, int)
{
	oracle_trap("gr_d3d_bitmap_ex");
}

void gr_d3d_bitmap(int, int)
{
	oracle_trap("gr_d3d_bitmap");
}

void gr_d3d_cleanup()
{
	oracle_trap("gr_d3d_cleanup");
}

void gr_d3d_init()
{
	oracle_trap("gr_d3d_init");
}

void gr_d3d_preload_init()
{
	oracle_trap("gr_d3d_preload_init");
}

void gr_d3d_preload(int, int)
{
	oracle_trap("gr_d3d_preload");
}

void gr_dd_activate(int)
{
	oracle_trap("gr_dd_activate");
}

void gr_directdraw_cleanup()
{
	oracle_trap("gr_directdraw_cleanup");
}

void gr_directdraw_force_windowed()
{
	oracle_trap("gr_directdraw_force_windowed");
}

void gr_directdraw_init()
{
	oracle_trap("gr_directdraw_init");
}

void gr_glide_activate(int)
{
	oracle_trap("gr_glide_activate");
}

void gr_glide_bitmap_ex(int, int, int, int, int, int)
{
	oracle_trap("gr_glide_bitmap_ex");
}

void gr_glide_bitmap(int, int)
{
	oracle_trap("gr_glide_bitmap");
}

void gr_glide_cleanup()
{
	oracle_trap("gr_glide_cleanup");
}

void gr_glide_force_windowed()
{
	oracle_trap("gr_glide_force_windowed");
}

void gr_glide_init()
{
	oracle_trap("gr_glide_init");
}

void gr_glide_string_hack(int, int, char*)
{
	oracle_trap("gr_glide_string_hack");
}

void gr_opengl_bitmap_ex(int, int, int, int, int, int)
{
	oracle_trap("gr_opengl_bitmap_ex");
}

void gr_opengl_bitmap(int, int)
{
	oracle_trap("gr_opengl_bitmap");
}

void gr_opengl_cleanup()
{
	oracle_trap("gr_opengl_cleanup");
}

void gr_opengl_init()
{
	oracle_trap("gr_opengl_init");
}

long multi_assign_network_signature(int)
{
	return -1;
}

long multi_campaign_eval_debrief()
{
	return -1;
}

long multi_can_message(net_player*)
{
	return -1;
}

long multi_common_voice_display_status()
{
	return -1;
}

long multi_create_game_close()
{
	return -1;
}

long multi_create_game_do()
{
	return -1;
}

long multi_create_game_init()
{
	return -1;
}

long multi_debrief_accept_hit()
{
	return -1;
}

long multi_debrief_close()
{
	return -1;
}

long multi_debrief_do_frame()
{
	return -1;
}

long multi_debrief_esc_hit()
{
	return -1;
}

long multi_debrief_init()
{
	return -1;
}

long multi_debrief_replay_hit()
{
	return -1;
}

long multi_debrief_stats_accept_code()
{
	return -1;
}

long multi_df_debrief_close()
{
	return -1;
}

long multi_df_debrief_do()
{
	return -1;
}

long multi_df_debrief_init()
{
	return -1;
}

long multi_df_eval_kill(net_player*, object*)
{
	return -1;
}

long multi_display_netinfo()
{
	return -1;
}

long multi_do_client_warp(float)
{
	return -1;
}

long multi_do_frame()
{
	return -1;
}

long multi_endgame_ending()
{
	return -1;
}

long multi_find_player_by_callsign(char*)
{
	return -1;
}

long multi_find_player_by_object(object*)
{
	return -1;
}

long multi_find_player_by_signature(int)
{
	return -1;
}

long multi_game_client_setup_close()
{
	return -1;
}

long multi_game_client_setup_do_frame()
{
	return -1;
}

long multi_game_client_setup_init()
{
	return -1;
}

long multi_get_next_network_signature(int)
{
	return -1;
}

long multi_get_player_ship(int)
{
	return -1;
}

long multi_handle_end_mission_request()
{
	return -1;
}

long multi_host_options_close()
{
	return -1;
}

long multi_host_options_do()
{
	return -1;
}

long multi_host_options_init()
{
	return -1;
}

long multi_ignore_controls(int)
{
	return -1;
}

long multi_ingame_select_close()
{
	return -1;
}

long multi_ingame_select_do()
{
	return -1;
}

long multi_ingame_select_init()
{
	return -1;
}

long multi_init()
{
	return -1;
}

long multi_join_clear_game_list()
{
	return -1;
}

long multi_join_game_close()
{
	return -1;
}

long multi_join_game_do_frame()
{
	return -1;
}

long multi_join_game_init()
{
	return -1;
}

long multi_kick_player(int, int, int)
{
	return -1;
}

long multi_log_close()
{
	return -1;
}

long multi_log_process()
{
	return -1;
}

long multi_maybe_send_repair_info(object*, object*, int)
{
	return -1;
}

long multi_maybe_send_ship_status()
{
	return -1;
}

long multi_message_should_broadcast(int)
{
	return -1;
}

long multi_msg_eval_ship_squadmsg(int, int, ai_info*, int)
{
	return -1;
}

long multi_msg_eval_wing_squadmsg(int, int, ai_info*, int)
{
	return -1;
}

long multi_msg_key_down(int)
{
	return -1;
}

long multi_msg_message_text(char*)
{
	return -1;
}

long multi_num_players()
{
	return -1;
}

long multi_obs_zoom_to_target()
{
	return -1;
}

long multi_oo_gameplay_init()
{
	return -1;
}

long multi_oo_interp(object*)
{
	return -1;
}

long multi_oo_is_interp_object(object*)
{
	return -1;
}

long multi_oo_rate_init_all()
{
	return -1;
}

long multi_options_local_load(multi_local_options*, net_player*)
{
	return -1;
}

long multi_options_set_local_defaults(multi_local_options*)
{
	return -1;
}

long multi_options_set_netgame_defaults(multi_server_options*)
{
	return -1;
}

long multi_options_update_local()
{
	return -1;
}

long multi_pause_close()
{
	return -1;
}

long multi_pause_do()
{
	return -1;
}

long multi_pause_do_frame()
{
	return -1;
}

long multi_pause_init(UI_WINDOW*)
{
	return -1;
}

long multi_pause_request(int)
{
	return -1;
}

long multi_pinfo_popup(net_player*)
{
	return -1;
}

long multi_ping_reset_players()
{
	return -1;
}

long multi_query_lag_status()
{
	return -1;
}

long multi_quit_game(int, int, int, int)
{
	return -1;
}

long multi_rate_display(int, int, int)
{
	return -1;
}

long multi_reset_timestamps()
{
	return -1;
}

long multi_respawn_build_points()
{
	return -1;
}

long multi_respawn_check(object*)
{
	return -1;
}

long multi_respawn_normal()
{
	return -1;
}

long multi_respawn_observer()
{
	return -1;
}

long multi_server_update_player_weapons(net_player*, ship*)
{
	return -1;
}

long multi_set_network_signature(unsigned short, int)
{
	return -1;
}

long multi_show_ingame_ping()
{
	return -1;
}

long multi_standalone_postgame_close()
{
	return -1;
}

long multi_standalone_postgame_do()
{
	return -1;
}

long multi_standalone_postgame_init()
{
	return -1;
}

long multi_standalone_wait_close()
{
	return -1;
}

long multi_standalone_wait_do()
{
	return -1;
}

long multi_standalone_wait_init()
{
	return -1;
}

long multi_start_game_close()
{
	return -1;
}

long multi_start_game_do()
{
	return -1;
}

long multi_start_game_init()
{
	return -1;
}

long multi_sync_close()
{
	return -1;
}

long multi_sync_do()
{
	return -1;
}

long multi_sync_init()
{
	return -1;
}

long multi_team_maybe_add_score(int, int)
{
	return -1;
}

long multi_ts_close()
{
	return -1;
}

long multi_ts_commit_pressed()
{
	return -1;
}

long multi_ts_common_init()
{
	return -1;
}

long multi_ts_disabled_high_slot(int, int)
{
	return -1;
}

long multi_ts_disabled_slot(int, int)
{
	return -1;
}

long multi_ts_do()
{
	return -1;
}

long multi_ts_get_team_and_slot(char*, int*, int*)
{
	return -1;
}

long multi_ts_init()
{
	return -1;
}

long multi_ts_is_locked()
{
	return -1;
}

long multi_ts_lock_pressed()
{
	return -1;
}

long multi_unload_common_icons()
{
	return -1;
}

long multi_voice_close()
{
	return -1;
}

long multi_voice_init()
{
	return -1;
}

long multi_voice_set_prefs(int)
{
	return -1;
}

long multi_voice_status()
{
	return -1;
}

long multi_voice_test_get_playback_buffer()
{
	return -1;
}

long multi_voice_test_packet_tossed()
{
	return -1;
}

long multi_voice_test_process()
{
	return -1;
}

long multi_voice_test_recording()
{
	return -1;
}

long multi_voice_test_record_start()
{
	return -1;
}

long multi_voice_test_record_stop()
{
	return -1;
}

long oo_display()
{
	return -1;	// multiplayer machinery, silent
}

long psnet_close()
{
	return -1;
}

long psnet_get_network_status()
{
	return -1;
}

long psnet_init(int, int)
{
	return -1;
}

long psnet_is_valid_ip_string(char*, int)
{
	return -1;
}

long psnet_use_protocol(int)
{
	return -1;
}

long rtvoice_play_uncompressed(int, unsigned char*, int)
{
	return -1;
}

long rtvoice_set_qos(int)
{
	return -1;
}

long rtvoice_stop_playback_all()
{
	return -1;
}

long rtvoice_uncompress(unsigned char*, int, double, unsigned char*, int)
{
	return -1;
}

long send_ai_info_update_packet(object*, char)
{
	return -1;	// multiplayer machinery, silent
}

long send_asteroid_create(object*, object*, int, vector*)
{
	return -1;	// multiplayer machinery, silent
}

long send_asteroid_hit(object*, object*, vector*, float)
{
	return -1;	// multiplayer machinery, silent
}

long send_asteroid_throw(object*)
{
	return -1;	// multiplayer machinery, silent
}

long send_beam_fired_packet(object*, ship_subsys*, object*, int, beam_info*)
{
	return -1;	// multiplayer machinery, silent
}

long send_cargo_revealed_packet(ship*)
{
	return -1;	// multiplayer machinery, silent
}

long send_change_iff_packet(unsigned short, int)
{
	return -1;	// multiplayer machinery, silent
}

long send_countermeasure_success_packet(int)
{
	return -1;	// multiplayer machinery, silent
}

long send_debrief_info(int*, int**)
{
	return -1;	// multiplayer machinery, silent
}

long send_debris_update_packet(object*, int)
{
	return -1;	// multiplayer machinery, silent
}

long send_emp_effect(unsigned short, float, float)
{
	return -1;	// multiplayer machinery, silent
}

long send_event_update_packet(int)
{
	return -1;	// multiplayer machinery, silent
}

long send_flak_fired_packet(int, int, int, float)
{
	return -1;	// multiplayer machinery, silent
}

long send_game_chat_packet(net_player*, char*, int, net_player*, char*, int)
{
	return -1;	// multiplayer machinery, silent
}

long send_homing_weapon_info(int)
{
	return -1;	// multiplayer machinery, silent
}

long send_lightning_packet(int, vector*, vector*)
{
	return -1;	// multiplayer machinery, silent
}

long send_mission_goal_info_packet(int, int, int)
{
	return -1;	// multiplayer machinery, silent
}

long send_mission_log_packet(int)
{
	return -1;	// multiplayer machinery, silent
}

long send_mission_message_packet(int, char*, int, int, int, int, int, int)
{
	return -1;	// multiplayer machinery, silent
}

long send_mission_sync_packet(int, int)
{
	return -1;	// multiplayer machinery, silent
}

long send_netplayer_update_packet(net_player*)
{
	return -1;	// multiplayer machinery, silent
}

long send_NEW_countermeasure_fired_packet(object*, int, int)
{
	return -1;	// multiplayer machinery, silent
}

long send_NEW_primary_fired_packet(ship*, int)
{
	return -1;	// multiplayer machinery, silent
}

long send_player_order_packet(int, int, int)
{
	return -1;	// multiplayer machinery, silent
}

long send_player_pain_packet(net_player*, int, float, vector*, vector*)
{
	return -1;	// multiplayer machinery, silent
}

long send_player_stats_block_packet(net_player*, int, net_player*)
{
	return -1;	// multiplayer machinery, silent
}

long send_reinforcement_avail(int)
{
	return -1;	// multiplayer machinery, silent
}

long send_secondary_fired_packet(ship*, unsigned short, int, int, int)
{
	return -1;	// multiplayer machinery, silent
}

long send_self_destruct_packet()
{
	return -1;	// multiplayer machinery, silent
}

long send_ship_create_packet(object*, int)
{
	return -1;	// multiplayer machinery, silent
}

long send_ship_depart_packet(object*)
{
	return -1;	// multiplayer machinery, silent
}

long send_ship_kill_packet(object*, object*, float, int)
{
	return -1;	// multiplayer machinery, silent
}

long send_subsystem_cargo_revealed_packet(ship*, int)
{
	return -1;	// multiplayer machinery, silent
}

long send_subsystem_destroyed_packet(ship*, int, vector)
{
	return -1;	// multiplayer machinery, silent
}

long send_turret_fired_packet(int, int, int)
{
	return -1;	// multiplayer machinery, silent
}

long send_weapon_detonate_packet(object*)
{
	return -1;	// multiplayer machinery, silent
}

long send_wing_create_packet(wing*, int, int)
{
	return -1;	// multiplayer machinery, silent
}

long send_wss_request_packet(short, int, int, int, int, int, int, int, net_player*)
{
	return -1;	// multiplayer machinery, silent
}

long send_wss_update_packet(int, unsigned char*, int)
{
	return -1;	// multiplayer machinery, silent
}

long snd_chg_loop_status(int, int)
{
	return -1;
}

long snd_close()
{
	return -1;
}

long snd_do_frame()
{
	return -1;
}

long snd_get_3d_vol_and_pan(game_snd*, vector*, float*, float*, float)
{
	return -1;
}

long snd_get_duration(int)
{
	return -1;
}

long snd_get_format(int, int*, int*)
{
	return -1;
}

long snd_get_pitch(int)
{
	return -1;
}

long snd_init(int, int)
{
	return -1;
}

long snd_is_inited()
{
	return -1;
}

long snd_is_playing(int)
{
	return -1;
}

long snd_load(game_snd*, int)
{
	return -1;
}

long snd_num_playing()
{
	return -1;
}

long snd_play_3d(game_snd*, vector*, vector*, float, vector*, int, float, int, vector*, float, int)
{
	return -1;
}

long snd_play(game_snd*, float, float, int, bool)
{
	return -1;
}

long snd_play_looping(game_snd*, float, int, int, float, int, int)
{
	return -1;
}

long snd_play_raw(int, float, float, int)
{
	return -1;
}

long snd_set_pan(int, float)
{
	return -1;
}

long snd_set_pitch(int, int)
{
	return -1;
}

long snd_set_pos(int, game_snd*, float, int)
{
	return -1;
}

long snd_set_volume(int, float)
{
	return -1;
}

long snd_spew_debug_info()
{
	return -1;
}

long snd_stop_all()
{
	return -1;
}

long snd_stop(int)
{
	return -1;
}

long snd_time_remaining(int, int, int)
{
	return -1;
}

long snd_unload(int)
{
	return -1;
}

long snd_update_3d_pos(int, game_snd*, vector*)
{
	return -1;
}

long snd_update_listener(vector*, vector*, matrix*)
{
	return -1;
}

long sound_env_disable()
{
	return -1;
}

long sound_env_set(sound_env*)
{
	return -1;
}

long standalone_main_close()
{
	return -1;
}

long standalone_main_do()
{
	return -1;
}

long standalone_main_init()
{
	return -1;
}

long std_init_standalone()
{
	return -1;
}

long std_multi_set_standalone_missiontime(float)
{
	return -1;
}

long std_multi_update_goals()
{
	return -1;
}

long std_set_standalone_fps(float)
{
	return -1;
}

void windebug_memwatch_init()
{
}

// data symbols, zero-backed
unsigned char D3D_32bit[1 << 20];
unsigned char D3D_fog_mode[1 << 20];
unsigned char D3D_inited[1 << 20];
unsigned char D3d_rendition_uvs[1 << 20];
unsigned char D3D_textures_in[1 << 20];
unsigned char D3D_textures_in_frame[1 << 20];
unsigned char D3D_zbias[1 << 20];
unsigned char Demo_error[1 << 20];
unsigned char Demo_make[1 << 20];
unsigned char Glide_explosion_vram[1 << 20];
unsigned char Glide_textures_in[1 << 20];
unsigned char Glide_textures_in_frame[1 << 20];
unsigned char Glide_voodoo3[1 << 20];
unsigned char Ipx_active[1 << 20];
unsigned char Master_sound_volume[1 << 20];
unsigned char Master_voice_volume[1 << 20];
unsigned char Multi_button_info_ok[1 << 20];
unsigned char Multi_chat_stream[1 << 20];
unsigned char Multi_common_icons[1 << 20];
unsigned char Multi_connection_speed[1 << 20];
unsigned char Multi_display_netinfo[1 << 20];
unsigned char Multi_options_g[1 << 20];
unsigned char Multi_pause_status[1 << 20];
unsigned char Multi_ship_status_bi[1 << 20];
unsigned char Multi_sync_mode[1 << 20];
unsigned char Multi_team0_score[1 << 20];
unsigned char Multi_team1_score[1 << 20];
unsigned char Multi_tracker_id[1 << 20];
unsigned char Multi_tracker_login[1 << 20];
unsigned char Multi_tracker_passwd[1 << 20];
unsigned char Multi_tracker_squad_name[1 << 20];
unsigned char Multi_update_fireup_launcher_on_exit[1 << 20];
unsigned char Multi_voice_can_record[1 << 20];
unsigned char Multi_voice_local_prefs[1 << 20];
unsigned char Netgame[1 << 20];
unsigned char oo_arrive_time_count[1 << 20];
unsigned char oo_interp_count[1 << 20];
unsigned char OO_update_index[1 << 20];
unsigned char Snd_hram[1 << 20];
unsigned char Snd_sram[1 << 20];
unsigned char Sound_enabled[1 << 20];
unsigned char Tcp_active[1 << 20];
unsigned char TotalRam[1 << 20];
