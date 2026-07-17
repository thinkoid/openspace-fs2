// GENERATED trap stubs for game symbols the foundation references but
// whose subsystems are not ported yet.  Calling one aborts with its name;
// data symbols read as zeros.  Entries disappear as subsystems port.
// Regenerate: link without this file, feed the demangled undefined-symbol
// list to gen_gamestubs.py (see notes.txt).

#include <stdio.h>
#include <stdlib.h>

struct CFILE;
struct ai_info;
struct angles;
struct anim;
struct anim_instance;
struct beam_fire_info;
struct matrix;
struct object;
struct ship;
struct ship_subsys;
struct vector;
struct vertex;

static void oracle_trap(const char *sym)
{
	fprintf(stderr, "oracle strayed into unported code: %s\n", sym);
	abort();
}

void ai_add_ship_goal_sexp(int, int, ai_info*)
{
	oracle_trap("ai_add_ship_goal_sexp");
}

void ai_add_wing_goal_sexp(int, int, int)
{
	oracle_trap("ai_add_wing_goal_sexp");
}

void ai_clear_ship_goals(ai_info*)
{
	oracle_trap("ai_clear_ship_goals");
}

void ai_clear_wing_goals(int)
{
	oracle_trap("ai_clear_wing_goals");
}

void ai_find_docked_object(object*)
{
	oracle_trap("ai_find_docked_object");
}

void ai_get_subsystem_type(char*)
{
	oracle_trap("ai_get_subsystem_type");
}

void ai_good_secondary_time(int, int, int, char*)
{
	oracle_trap("ai_good_secondary_time");
}

void ai_query_goal_valid(int, int)
{
	oracle_trap("ai_query_goal_valid");
}

void ai_set_rearm_status(int, int)
{
	oracle_trap("ai_set_rearm_status");
}

void anim_free(anim*)
{
	oracle_trap("anim_free");
}

void anim_instance_get_byte(anim_instance*, int)
{
	oracle_trap("anim_instance_get_byte");
}

void anim_instance_is_streamed(anim_instance*)
{
	oracle_trap("anim_instance_is_streamed");
}

void anim_load(char*, int)
{
	oracle_trap("anim_load");
}

void anim_read_header(anim*, CFILE*)
{
	oracle_trap("anim_read_header");
}

void apply_damage_to_shield(object*, int, float)
{
	oracle_trap("apply_damage_to_shield");
}

void awacs_get_level(object*, ship*, int)
{
	oracle_trap("awacs_get_level");
}

void beam_fire(beam_fire_info*)
{
	oracle_trap("beam_fire");
}

void d3d_zbias(int)
{
	oracle_trap("d3d_zbias");
}

void do_subobj_destroyed_stuff(ship*, ship_subsys*, vector*)
{
	oracle_trap("do_subobj_destroyed_stuff");
}

void g3_check_normal_facing(vector*, vector*)
{
	oracle_trap("g3_check_normal_facing");
}

void g3_code_vertex(vertex*)
{
	oracle_trap("g3_code_vertex");
}

void g3_done_instance()
{
	oracle_trap("g3_done_instance");
}

void g3_draw_bitmap(vertex*, int, float, unsigned int)
{
	oracle_trap("g3_draw_bitmap");
}

void g3_draw_line(vertex*, vertex*)
{
	oracle_trap("g3_draw_line");
}

void g3_draw_poly(int, vertex**, unsigned int)
{
	oracle_trap("g3_draw_poly");
}

void g3_draw_sphere_ez(vector*, float)
{
	oracle_trap("g3_draw_sphere_ez");
}

void g3_draw_sphere(vertex*, float)
{
	oracle_trap("g3_draw_sphere");
}

void g3_project_vertex(vertex*)
{
	oracle_trap("g3_project_vertex");
}

void g3_rotate_vertex(vertex*, vector*)
{
	oracle_trap("g3_rotate_vertex");
}

void g3_start_instance_angles(vector*, angles*)
{
	oracle_trap("g3_start_instance_angles");
}

void g3_start_instance_matrix(vector*, matrix*)
{
	oracle_trap("g3_start_instance_matrix");
}

void get_shield_strength(object*)
{
	oracle_trap("get_shield_strength");
}

void gr_d3d_preload_init()
{
	oracle_trap("gr_d3d_preload_init");
}

void gr_d3d_preload(int, int)
{
	oracle_trap("gr_d3d_preload");
}

void gr_get_string_size(int*, int*, char*, int)
{
	oracle_trap("gr_get_string_size");
}

void gr_set_palette(char*, unsigned char*, int)
{
	oracle_trap("gr_set_palette");
}

void hud_add_ship_to_escort(int, int)
{
	oracle_trap("hud_add_ship_to_escort");
}

void hud_find_target_distance(object*, object*)
{
	oracle_trap("hud_find_target_distance");
}

void hud_gauge_start_flash(int)
{
	oracle_trap("hud_gauge_start_flash");
}

void hud_remove_ship_from_escort(int)
{
	oracle_trap("hud_remove_ship_from_escort");
}

void hud_shield_quadrant_hit(object*, int)
{
	oracle_trap("hud_shield_quadrant_hit");
}

void key_getch()
{
	oracle_trap("key_getch");
}

void light_apply_rgb(unsigned char*, unsigned char*, unsigned char*, vector*, vector*, float)
{
	oracle_trap("light_apply_rgb");
}

void light_apply(vector*, vector*, float)
{
	oracle_trap("light_apply");
}

void light_filter_pop()
{
	oracle_trap("light_filter_pop");
}

void light_filter_push_box(vector*, vector*)
{
	oracle_trap("light_filter_push_box");
}

void light_filter_push(int, vector*, float)
{
	oracle_trap("light_filter_push");
}

void light_rotate_all()
{
	oracle_trap("light_rotate_all");
}

void message_send_unique_to_player(char*, void*, int, int, int, int)
{
	oracle_trap("message_send_unique_to_player");
}

void message_training_que(char*, int, int)
{
	oracle_trap("message_training_que");
}

void mission_campaign_find_mission(char*)
{
	oracle_trap("mission_campaign_find_mission");
}

void mission_campaign_save_persistent(int, int)
{
	oracle_trap("mission_campaign_save_persistent");
}

void mission_goal_mark_invalid(char*)
{
	oracle_trap("mission_goal_mark_invalid");
}

void mission_goal_mark_valid(char*)
{
	oracle_trap("mission_goal_mark_valid");
}

void mission_log_get_time_indexed(int, char*, char*, int, long*)
{
	oracle_trap("mission_log_get_time_indexed");
}

void mission_log_get_time(int, char*, char*, long*)
{
	oracle_trap("mission_log_get_time");
}

void mission_parse_get_arrival_ship(char*)
{
	oracle_trap("mission_parse_get_arrival_ship");
}

void mission_parse_ship_arrived(char*)
{
	oracle_trap("mission_parse_ship_arrived");
}

void multi_find_player_by_object(object*)
{
	oracle_trap("multi_find_player_by_object");
}

void neb2_get_lod_scale(int)
{
	oracle_trap("neb2_get_lod_scale");
}

void os_config_read_string(char*, char*, char*)
{
	oracle_trap("os_config_read_string");
}

void read_mission_goal_list(int)
{
	oracle_trap("read_mission_goal_list");
}

void red_alert_start_mission()
{
	oracle_trap("red_alert_start_mission");
}

void send_change_iff_packet(unsigned short, int)
{
	oracle_trap("send_change_iff_packet");
}

void ship_docking_valid(int, int)
{
	oracle_trap("ship_docking_valid");
}

void ship_find_exited_ship_by_name(char*)
{
	oracle_trap("ship_find_exited_ship_by_name");
}

void ship_get_indexed_subsys(ship*, int, vector*)
{
	oracle_trap("ship_get_indexed_subsys");
}

void ship_get_length(ship*)
{
	oracle_trap("ship_get_length");
}

void ship_get_random_ship_in_wing(int, int, float, int)
{
	oracle_trap("ship_get_random_ship_in_wing");
}

void ship_get_subsys_index(ship*, char*, int)
{
	oracle_trap("ship_get_subsys_index");
}

void ship_get_subsys(ship*, char*)
{
	oracle_trap("ship_get_subsys");
}

void ship_get_subsystem_strength(ship*, int)
{
	oracle_trap("ship_get_subsystem_strength");
}

void ship_get_texture(int)
{
	oracle_trap("ship_get_texture");
}

void ship_info_lookup(char*)
{
	oracle_trap("ship_info_lookup");
}

void ship_is_visible_by_team(int, int)
{
	oracle_trap("ship_is_visible_by_team");
}

void ship_jettison_cargo(ship*)
{
	oracle_trap("ship_jettison_cargo");
}

void ship_name_lookup(char*, int)
{
	oracle_trap("ship_name_lookup");
}

void ship_query_state(char*)
{
	oracle_trap("ship_query_state");
}

void ship_recalc_subsys_strength(ship*)
{
	oracle_trap("ship_recalc_subsys_strength");
}

void ship_self_destruct(object*)
{
	oracle_trap("ship_self_destruct");
}

void ship_type_name_lookup(char*)
{
	oracle_trap("ship_type_name_lookup");
}

void ship_vanished(int)
{
	oracle_trap("ship_vanished");
}

void Skill_level_names(int, int)
{
	oracle_trap("Skill_level_names");
}

void supernova_start(int)
{
	oracle_trap("supernova_start");
}

void timestamp()
{
	oracle_trap("timestamp");
}

void timestamp_has_time_elapsed(int, int)
{
	oracle_trap("timestamp_has_time_elapsed");
}

void timestamp(int)
{
	oracle_trap("timestamp");
}

void translate_key_to_index(char*)
{
	oracle_trap("translate_key_to_index");
}

void weapon_info_lookup(char*)
{
	oracle_trap("weapon_info_lookup");
}

void wing_name_lookup(char*, int)
{
	oracle_trap("wing_name_lookup");
}

// data symbols, zero-backed
unsigned char Ai_info[1 << 20];
unsigned char Briefings[1 << 20];
unsigned char Campaign[1 << 20];
unsigned char Campaign_ended_in_mission[1 << 20];
unsigned char Canv_h2[1 << 20];
unsigned char Canv_w2[1 << 20];
unsigned char Cargo_names[1 << 20];
unsigned char Cargo_names_buf[1 << 20];
unsigned char Control_config[1 << 20];
unsigned char D3D_32bit[1 << 20];
unsigned char Debriefings[1 << 20];
unsigned char Energy_levels[1 << 20];
unsigned char Event_index[1 << 20];
unsigned char Eye_position[1 << 20];
unsigned char flFrametime[1 << 20];
unsigned char G3_count[1 << 20];
unsigned char Game_skill_level[1 << 20];
unsigned char Gr_alpha[1 << 20];
unsigned char Gr_bitmap_poly[1 << 20];
unsigned char Gr_blue[1 << 20];
unsigned char Gr_current_alpha[1 << 20];
unsigned char Gr_current_blue[1 << 20];
unsigned char Gr_current_green[1 << 20];
unsigned char Gr_current_red[1 << 20];
unsigned char Gr_gamma[1 << 20];
unsigned char Gr_gamma_int[1 << 20];
unsigned char Gr_green[1 << 20];
unsigned char Gr_red[1 << 20];
unsigned char Gr_scaler_zbuffering[1 << 20];
unsigned char gr_screen[1 << 20];
unsigned char Gr_ta_alpha[1 << 20];
unsigned char Gr_ta_blue[1 << 20];
unsigned char Gr_ta_green[1 << 20];
unsigned char Gr_t_alpha[1 << 20];
unsigned char Gr_ta_red[1 << 20];
unsigned char Gr_t_blue[1 << 20];
unsigned char Gr_t_green[1 << 20];
unsigned char Gr_t_red[1 << 20];
unsigned char gr_zbuffering_mode[1 << 20];
unsigned char Jump_nodes[1 << 20];
unsigned char keyd_pressed[1 << 20];
unsigned char Matrix_scale[1 << 20];
unsigned char Medals[1 << 20];
unsigned char Messages[1 << 20];
unsigned char Mission_events[1 << 20];
unsigned char Mission_filename[1 << 20];
unsigned char Mission_goals[1 << 20];
unsigned char Multi_team0_score[1 << 20];
unsigned char Multi_team1_score[1 << 20];
unsigned char Neb2_render_mode[1 << 20];
unsigned char Netgame[1 << 20];
unsigned char Net_player[1 << 20];
unsigned char Net_players[1 << 20];
unsigned char Num_cargo[1 << 20];
unsigned char Num_goals[1 << 20];
unsigned char Num_jump_nodes[1 << 20];
unsigned char Num_messages[1 << 20];
unsigned char Num_mission_events[1 << 20];
unsigned char Num_ship_types[1 << 20];
unsigned char Num_team_names[1 << 20];
unsigned char Num_teams[1 << 20];
unsigned char Num_waypoint_lists[1 << 20];
unsigned char Num_weapon_types[1 << 20];
unsigned char Objects[1 << 20];
unsigned char obj_used_list[1 << 20];
unsigned char physics_paused[1 << 20];
unsigned char Player[1 << 20];
unsigned char Player_ai[1 << 20];
unsigned char Player_obj[1 << 20];
unsigned char Player_ship[1 << 20];
unsigned char Ship_counts[1 << 20];
unsigned char Ship_info[1 << 20];
unsigned char Ship_obj_list[1 << 20];
unsigned char Ships[1 << 20];
unsigned char Ships_exited[1 << 20];
unsigned char Ship_type_flags[1 << 20];
unsigned char Ship_type_names[1 << 20];
unsigned char Team_names[1 << 20];
unsigned char The_mission[1 << 20];
unsigned char Total_goal_ship_names[1 << 20];
unsigned char TotalRam[1 << 20];
unsigned char Training_failure[1 << 20];
unsigned char View_position[1 << 20];
unsigned char Waypoint_lists[1 << 20];
unsigned char Weapon_info[1 << 20];
unsigned char Weapons[1 << 20];
unsigned char Wings[1 << 20];
