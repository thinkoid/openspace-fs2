/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#include "ui.h"
#include "uidefs.h"
#include "gamesnd.h"
#include "gamesequence.h"
#include "missionscreencommon.h"
#include "key.h"
#include "bmpman.h"
#include "font.h"
#include "missionbriefcommon.h"
#include "missioncmdbrief.h"
#include "redalert.h"
#include "audiostr.h"
#include "timer.h"
#include "eventmusic.h"
#include "player.h"
#include "contexthelp.h"
#include "alphacolors.h"
#include "animplay.h"

char *Cmd_brief_fname[GR_NUM_RESOLUTIONS] = {
	"CommandBrief",
	"2_CommandBrief"
};

char *Cmd_brief_mask[GR_NUM_RESOLUTIONS] = {
	"CommandBrief-m",
	"2_Commandbrief-m"
};

// lookups for coordinates
#define CMD_X_COORD 0
#define CMD_Y_COORD 1
#define CMD_W_COORD 2
#define CMD_H_COORD 3

int Cmd_text_wnd_coords[GR_NUM_RESOLUTIONS][4] = {
	{
		17, 109, 606, 108			// GR_640
	},
	{
		28, 174, 969, 174			// GR_1024
	}
};


int Cmd_stage_y[GR_NUM_RESOLUTIONS] = {
	90,		// GR_640
	145		// GR_1024
};

int Cmd_image_wnd_coords[GR_NUM_RESOLUTIONS][4] = {
	{
		26, 258, 441, 204				// GR_640
	},
	{
		155, 475, 706, 327		// GR_1024
	}
};

#define NUM_BUTTONS	8

#define FIRST_STAGE_BUTTON	0
#define PREV_STAGE_BUTTON	1
#define PAUSE_BUTTON			2
#define NEXT_STAGE_BUTTON	3
#define LAST_STAGE_BUTTON	4
#define HELP_BUTTON			5
#define OPTIONS_BUTTON		6
#define ACCEPT_BUTTON		7

// buttons
ui_button_info Cmd_brief_buttons[GR_NUM_RESOLUTIONS][NUM_BUTTONS] = {
	{ // GR_640
		ui_button_info("CBB_00",	504,	221,	-1,	-1,	0),
		ui_button_info("CBB_01",	527,	221,	-1,	-1,	1),
		ui_button_info("CBB_02",	555,	221,	-1,	-1,	2),
		ui_button_info("CBB_03",	583,	221,	-1,	-1,	3),
		ui_button_info("CBB_04",	607,	221,	-1,	-1,	4),
		ui_button_info("CBB_05",	539,	431,	-1,	-1,	5),
		ui_button_info("CBB_06",	538,	455,	-1,	-1,	6),
		ui_button_info("CBB_07",	575,	432,	-1,	-1,	7),
	},
	{ // GR_1024
		ui_button_info("2_CBB_00",	806,	354,	-1,	-1,	0),
		ui_button_info("2_CBB_01",	844,	354,	-1,	-1,	1),
		ui_button_info("2_CBB_02",	888,	354,	-1,	-1,	2),
		ui_button_info("2_CBB_03",	933,	354,	-1,	-1,	3),
		ui_button_info("2_CBB_04",	971,	354,	-1,	-1,	4),
		ui_button_info("2_CBB_05",	863,	690,	-1,	-1,	5),
		ui_button_info("2_CBB_06",	861,	728,	-1,	-1,	6),
		ui_button_info("2_CBB_07",	920,	692,	-1,	-1,	7),
	}
};

// text
#define CMD_BRIEF_NUM_TEXT		3
UI_XSTR Cmd_brief_text[GR_NUM_RESOLUTIONS][CMD_BRIEF_NUM_TEXT] = {
	{ // GR_640
		{ "Help",		928,	500,	440,	UI_XSTR_COLOR_GREEN,	-1,	&Cmd_brief_buttons[0][HELP_BUTTON].button },
		{ "Options",	1036,	479,	464,	UI_XSTR_COLOR_GREEN,	-1,	&Cmd_brief_buttons[0][OPTIONS_BUTTON].button },
		{ "Continue",	1069,	564,	413,	UI_XSTR_COLOR_PINK,	-1,	&Cmd_brief_buttons[0][ACCEPT_BUTTON].button },
	},
	{ // GR_1024
		{ "Help",		928,	800,	704,	UI_XSTR_COLOR_GREEN,	-1,	&Cmd_brief_buttons[1][HELP_BUTTON].button },
		{ "Options",	1036,	797,	743,	UI_XSTR_COLOR_GREEN,	-1,	&Cmd_brief_buttons[1][OPTIONS_BUTTON].button },
		{ "Continue",	1069,	917,	661,	UI_XSTR_COLOR_PINK,	-1,	&Cmd_brief_buttons[1][ACCEPT_BUTTON].button },
	}
};

static UI_WINDOW Ui_window;
static int Background_bitmap;					// bitmap for the background of the cmd_briefing
static int Cur_stage;
static int Scroll_offset;
static int Cmd_brief_inited = 0;
// static int Cmd_brief_ask_for_cd;
static int Voice_good_to_go = 0;
static int Voice_started_time = 0;
static int Voice_ended_time;
static int Anim_playing_id = -1;
static anim_instance *Cur_anim_instance = NULL;
static int Last_anim_frame_num;

static int Cmd_brief_last_voice;
static int Palette_bmp = -1;
static ubyte Palette[768];
static char Palette_name[128];

void cmd_brief_init_voice()
{
	int i;

	Assert(Cur_cmd_brief);
	for (i=0; i<Cur_cmd_brief->num_stages; i++) {
		Cur_cmd_brief->stage[i].wave = -1;
		if (stricmp(Cur_cmd_brief->stage[i].wave_filename, NOX("none")) && Cur_cmd_brief->stage[i].wave_filename[0]) {
			Cur_cmd_brief->stage[i].wave = audiostream_open(Cur_cmd_brief->stage[i].wave_filename, ASF_VOICE);
			if (Cur_cmd_brief->stage[i].wave < 0) {
				nprintf(("General", "Failed to load \"%s\"", Cur_cmd_brief->stage[i].wave_filename));
			}
		}
	}

	Cmd_brief_last_voice = -1;
}

int cmd_brief_check_stage_done()
{
	if (!Voice_good_to_go)
		return 0;

	if (Voice_ended_time && (timer_get_milliseconds() - Voice_ended_time >= 1000))
		return 1;

	if (Briefing_voice_enabled && (Cmd_brief_last_voice >= 0)) {
		if (audiostream_is_playing(Cmd_brief_last_voice)){
			return 0;
		}

		if (!Voice_ended_time){
			Voice_ended_time = timer_get_milliseconds();
		}

		return 0;
	}

	// if we get here, there is no voice, so we simulate the time it would take instead
	if (!Voice_ended_time)
		Voice_ended_time = Voice_started_time + max(5000, Num_brief_text_lines[0] * 3500);

	return 0;
}

// start playback of the voice for a particular briefing stage
void cmd_brief_voice_play(int stage_num)
{
	int voice = -1;

	if (!Voice_good_to_go) {
		Voice_started_time = 0;
		return;
	}

	if (!Voice_started_time) {
		Voice_started_time = timer_get_milliseconds();
		Voice_ended_time = 0;
	}

	if (!Briefing_voice_enabled){
		return;
	}

	if (Cur_stage >= 0 && Cur_stage < Cur_cmd_brief->num_stages){
		voice = Cur_cmd_brief->stage[stage_num].wave;
	}

	// are we still on same voice that is currently playing/played?
	if (Cmd_brief_last_voice == voice){
		return;  // no changes, nothing to do.
	}

	// if previous wave is still playing, stop it first.
	if (Cmd_brief_last_voice >= 0) {
		audiostream_stop(Cmd_brief_last_voice);  // stream is automatically rewound
		Cmd_brief_last_voice = -1;
	}

	// ok, new wave needs playing, so we can start playing it now (and it becomes the current wave)
	Cmd_brief_last_voice = voice;
	if (voice >= 0){
		audiostream_play(voice, Master_voice_volume, 0);
	}
}

// called to leave the command briefing screen
void cmd_brief_exit()
{
	gameseq_post_event(GS_EVENT_START_BRIEFING);
}

void cmd_brief_stop_anim(int id)
{
	if (Cur_anim_instance && (id != Anim_playing_id)) {
		anim_stop_playing(Cur_anim_instance);
		Cur_anim_instance = NULL;
	}

	Voice_good_to_go = 0;
	if (Cmd_brief_last_voice >= 0) {
		audiostream_stop(Cmd_brief_last_voice);  // stream is automatically rewound
		Cmd_brief_last_voice = -1;
	}
}

void cmd_brief_new_stage(int stage)
{
	int i;
	anim_play_struct aps;

	if (stage < 0) {
		cmd_brief_stop_anim(-1);
		Cur_stage = -1;
		Anim_playing_id = -1;
	}

	Cur_stage = stage;
	brief_color_text_init(Cur_cmd_brief->stage[stage].text, Cmd_text_wnd_coords[gr_screen.res][CMD_W_COORD]);

	i = Cur_cmd_brief->stage[Cur_stage].anim_ref;
	if (i < 0)
		i = Cur_stage;

	cmd_brief_stop_anim(i);

	if (i != Anim_playing_id) {
		if (Cur_cmd_brief->stage[i].anim) {
			anim_play_init(&aps, Cur_cmd_brief->stage[i].anim,Cmd_image_wnd_coords[gr_screen.res][CMD_X_COORD], Cmd_image_wnd_coords[gr_screen.res][CMD_Y_COORD]);
			aps.looped = 1;
			Cur_anim_instance = anim_play(&aps);
			Last_anim_frame_num = 0;
		}

		Anim_playing_id = i;
	}

	if (Cur_cmd_brief->stage[i].anim) {
		memcpy(Palette, Cur_cmd_brief->stage[i].anim->palette, 384);
		gr_set_palette(Cur_cmd_brief->stage[i].ani_filename, Palette, 1);
	}
}

void cmd_brief_hold()
{
	cmd_brief_stop_anim(-1);
	Anim_playing_id = -1;
}

void cmd_brief_unhold()
{
	cmd_brief_new_stage(Cur_stage);
}

void cmd_brief_button_pressed(int n)
{
	switch (n) {
		case HELP_BUTTON:
			launch_context_help();
			gamesnd_play_iface(SND_HELP_PRESSED);
			break;

		case OPTIONS_BUTTON:
			gamesnd_play_iface(SND_SWITCH_SCREENS);
			gameseq_post_event(GS_EVENT_OPTIONS_MENU);
			break;

		case FIRST_STAGE_BUTTON:
			if (Cur_stage) {
				cmd_brief_new_stage(0);
				gamesnd_play_iface(SND_BRIEF_STAGE_CHG);
			} else {
				gamesnd_play_iface(SND_GENERAL_FAIL);
			}

			break;

		case PREV_STAGE_BUTTON:
			if (Cur_stage) {
				cmd_brief_new_stage(Cur_stage - 1);
				gamesnd_play_iface(SND_BRIEF_STAGE_CHG);
			} else {
				gamesnd_play_iface(SND_GENERAL_FAIL);
			}

			break;

		case NEXT_STAGE_BUTTON:
			if (Cur_stage < Cur_cmd_brief->num_stages - 1) {
				cmd_brief_new_stage(Cur_stage + 1);
				gamesnd_play_iface(SND_BRIEF_STAGE_CHG);
			} else {
				gamesnd_play_iface(SND_GENERAL_FAIL);
			}

			break;

		case LAST_STAGE_BUTTON:
			if (Cur_stage < Cur_cmd_brief->num_stages - 1) {
				cmd_brief_new_stage(Cur_cmd_brief->num_stages - 1);
				gamesnd_play_iface(SND_BRIEF_STAGE_CHG);
			} else {
				gamesnd_play_iface(SND_GENERAL_FAIL);
			}
			break;

		case ACCEPT_BUTTON:
			cmd_brief_exit();
			gamesnd_play_iface(SND_COMMIT_PRESSED);
			break;

		case PAUSE_BUTTON:
			gamesnd_play_iface(SND_USER_SELECT);
			Player->auto_advance ^= 1;
			break;
	}
}

void cmd_brief_ani_wave_init(int index)
{
	char *name;
	int i;

	// first, search and see if anim is already used in another stage
	for (i=0; i<index; i++) {
		if (!stricmp(Cur_cmd_brief->stage[i].ani_filename, Cur_cmd_brief->stage[index].ani_filename)) {
			if (Cur_cmd_brief->stage[i].anim_ref >= 0)
				Cur_cmd_brief->stage[index].anim_ref = Cur_cmd_brief->stage[i].anim_ref;
			else
				Cur_cmd_brief->stage[index].anim_ref = i;

			return;
		}
	}

	// this is the first instance of the given anim filename
	Cur_cmd_brief->stage[index].anim_ref = -1;
	name = Cur_cmd_brief->stage[index].ani_filename;
	if (!name[0] || !stricmp(name, NOX("<default>")) || !stricmp(name, NOX("none.ani"))) {
		name = NOX("CB_default");
		strcpy(Cur_cmd_brief->stage[index].ani_filename, name);
	}

	int load_attempts = 0;
	while (1) {

		if ( load_attempts++ > 5 ) {
			break;
		}

		Cur_cmd_brief->stage[index].anim = anim_load(name, 1);
		if ( Cur_cmd_brief->stage[index].anim ) {
			break;
		}

		// couldn't load animation, ask user to insert CD (if necessary)
		// if ( Cmd_brief_ask_for_cd ) {
			// if ( game_do_cd_check() == 0 ) {
				// Cmd_brief_ask_for_cd = 0;
				// break;
			// }
		// }
	}

	// check to see if cb anim loaded, if not, try the default one
	if ( !Cur_cmd_brief->stage[index].anim ) {
		Cur_cmd_brief->stage[index].anim = anim_load(NOX("CB_default"), 1);
	}
}

void cmd_brief_init(int team)
{
	common_music_init(SCORE_BRIEFING);

//#ifndef FS2_DEMO

	int i;
	ui_button_info *b;

	Cmd_brief_inited = 0;
	Cur_cmd_brief = &Cmd_briefs[team];

	if ( red_alert_mission() ) {
		gameseq_post_event(GS_EVENT_RED_ALERT);
		return;
	}

	if (Cur_cmd_brief->num_stages <= 0)
		return;

	gr_reset_clip();
	gr_clear();
	Mouse_hidden++;
	gr_flip();
	Mouse_hidden--;

	/*
	Palette_bmp = bm_load("BarracksPalette");	//CommandBriefPalette");
	Assert(Palette_bmp);
	bm_get_palette(Palette_bmp, Palette, Palette_name);  // get the palette for this bitmap
	gr_set_palette(Palette_name, Palette, 1);
	*/

	Ui_window.create(0, 0, gr_screen.max_w, gr_screen.max_h, 0);
	Ui_window.set_mask_bmap(Cmd_brief_mask[gr_screen.res]);

	// Cmd_brief_ask_for_cd = 1;

	for (i=0; i<NUM_BUTTONS; i++) {
		b = &Cmd_brief_buttons[gr_screen.res][i];

		b->button.create(&Ui_window, "", b->x, b->y, 60, 30, 0, 1);
		// set up callback for when a mouse first goes over a button
		b->button.set_highlight_action(common_play_highlight_sound);
		b->button.set_bmaps(b->filename);
		b->button.link_hotspot(b->hotspot);
	}

	// add text
	for(i=0; i<CMD_BRIEF_NUM_TEXT; i++){
		Ui_window.add_XSTR(&Cmd_brief_text[gr_screen.res][i]);
	}

	// set up readyrooms for buttons so we draw the correct animation frame when a key is pressed
	Cmd_brief_buttons[gr_screen.res][FIRST_STAGE_BUTTON].button.set_hotkey(KEY_SHIFTED | KEY_LEFT);
	Cmd_brief_buttons[gr_screen.res][LAST_STAGE_BUTTON].button.set_hotkey(KEY_SHIFTED | KEY_RIGHT);
	Cmd_brief_buttons[gr_screen.res][PREV_STAGE_BUTTON].button.set_hotkey(KEY_LEFT);
	Cmd_brief_buttons[gr_screen.res][NEXT_STAGE_BUTTON].button.set_hotkey(KEY_RIGHT);
	Cmd_brief_buttons[gr_screen.res][ACCEPT_BUTTON].button.set_hotkey(KEY_CTRLED | KEY_ENTER);
	Cmd_brief_buttons[gr_screen.res][HELP_BUTTON].button.set_hotkey(KEY_F1);
	Cmd_brief_buttons[gr_screen.res][OPTIONS_BUTTON].button.set_hotkey(KEY_F2);

	// load in help overlay bitmap	
	help_overlay_load(CMD_BRIEF_OVERLAY);
	help_overlay_set_state(CMD_BRIEF_OVERLAY,0);

	Background_bitmap = bm_load(Cmd_brief_fname[gr_screen.res]);

	// BarracksPalette/CommandBriefPalette (the load commented out above) never
	// shipped; without a base palette the static Palette[] is all zeros and
	// cmd_brief_new_stage overlays the ani's 128 entries onto black -- the
	// background art then remaps to a blue wash.  As with the pilot-select
	// screen, the background PCX carries the palette this screen needs.
	if (Background_bitmap >= 0) {
		bm_get_palette(Background_bitmap, Palette, Palette_name);
		gr_set_palette(Palette_name, Palette, 1);
	}

	for (i=0; i<Cur_cmd_brief->num_stages; i++)
		cmd_brief_ani_wave_init(i);

	cmd_brief_init_voice();
	Scroll_offset = 0;
	Cur_anim_instance = NULL;
	cmd_brief_new_stage(0);
	Cmd_brief_inited = 1;

//#endif
}

void cmd_brief_close()
{
	int i;

	if (Cmd_brief_inited) {
		cmd_brief_stop_anim(-1);
		Anim_playing_id = -1;
		for (i=0; i<Cur_cmd_brief->num_stages; i++) {
			if (Cur_cmd_brief->stage[i].wave >= 0)
				audiostream_close_file(Cur_cmd_brief->stage[i].wave, 0);

			if (Cur_cmd_brief->stage[i].anim_ref < 0)
				if (Cur_cmd_brief->stage[i].anim)
					anim_free(Cur_cmd_brief->stage[i].anim);
		}

		if (Background_bitmap >= 0)
			bm_unload(Background_bitmap);

		// unload the overlay bitmap
		help_overlay_unload(CMD_BRIEF_OVERLAY);

		Ui_window.destroy();
		/*
		if (Palette_bmp){
			bm_unload(Palette_bmp);
		}
		*/

		game_flush();
		Cmd_brief_inited = 0;
	}
}

void cmd_brief_do_frame(float frametime)
{
	char buf[40];
	int i, k, w;		

	// if no command briefing exists, skip this screen.
	if (!Cmd_brief_inited) {
		cmd_brief_exit();
		return;
	}

	if ( help_overlay_active(CMD_BRIEF_OVERLAY) ) {
		Cmd_brief_buttons[gr_screen.res][HELP_BUTTON].button.reset_status();
		Ui_window.set_ignore_gadgets(1);
	}

	k = Ui_window.process() & ~KEY_DEBUGGED;

	if ( (k > 0) || B1_JUST_RELEASED ) {
		if ( help_overlay_active(CMD_BRIEF_OVERLAY) ) {
			help_overlay_set_state(CMD_BRIEF_OVERLAY, 0);
			Ui_window.set_ignore_gadgets(0);
			k = 0;
		}
	}

	if ( !help_overlay_active(CMD_BRIEF_OVERLAY) ) {
		Ui_window.set_ignore_gadgets(0);
	}

	switch (k) {
	case KEY_ESC:
		common_music_close();
		gameseq_post_event(GS_EVENT_MAIN_MENU);
		break;
	}	// end switch

	for (i=0; i<NUM_BUTTONS; i++){
		if (Cmd_brief_buttons[gr_screen.res][i].button.pressed()){
			cmd_brief_button_pressed(i);
		}
	}

	cmd_brief_voice_play(Cur_stage);
	common_music_do();

	if (cmd_brief_check_stage_done() && Player->auto_advance && (Cur_stage < Cur_cmd_brief->num_stages - 1)){
//		if (!Cur_anim_instance || (Cur_anim_instance->frame_num < Last_anim_frame_num))
		if (!Cur_anim_instance || Cur_anim_instance->loop_count){
			cmd_brief_new_stage(Cur_stage + 1);
		}
	}

	if (Cur_anim_instance){
		Last_anim_frame_num = Cur_anim_instance->frame_num;
	}

	GR_MAYBE_CLEAR_RES(Background_bitmap);
	if (Background_bitmap >= 0) {
		gr_set_bitmap(Background_bitmap);
		gr_bitmap(0, 0);
	} 

	{
		// JAS: This code is hacked to allow the animation to use all 256 colors
		extern int Palman_allow_any_color;
		Palman_allow_any_color = 1;
		anim_render_all(0, frametime);
		Palman_allow_any_color = 0;
	}
	Ui_window.draw();

	if (!Player->auto_advance){
		Cmd_brief_buttons[gr_screen.res][PAUSE_BUTTON].button.draw_forced(2);
	}

	gr_set_font(FONT1);
	gr_set_color_fast(&Color_text_heading);

	sprintf(buf, XSTR( "Stage %d of %d", 464), Cur_stage + 1, Cur_cmd_brief->num_stages);
	gr_get_string_size(&w, NULL, buf);
	gr_string(Cmd_text_wnd_coords[gr_screen.res][CMD_X_COORD] + Cmd_text_wnd_coords[gr_screen.res][CMD_W_COORD] - w, Cmd_stage_y[gr_screen.res], buf);

	if (brief_render_text(Scroll_offset, Cmd_text_wnd_coords[gr_screen.res][CMD_X_COORD], Cmd_text_wnd_coords[gr_screen.res][CMD_Y_COORD], Cmd_text_wnd_coords[gr_screen.res][CMD_H_COORD], frametime, 0, 1)){
		Voice_good_to_go = 1;
	}

	// blit help overlay if active
	help_overlay_maybe_blit(CMD_BRIEF_OVERLAY);

	gr_flip();
}
