/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#include <missionui/missionpause.hh>
#include <ui/ui.hh>
#include <popup/popup.hh>
#include <graphics/2d.hh>
#include <bmpman/bmpman.hh>
#include <io/key.hh>
#include <sound/audiostr.hh>
#include <gamesequence/gamesequence.hh>
#include <freespace2/freespace.hh>
#include <hud/hud.hh>
#include <object/object.hh>
#include <graphics/font.hh>
#include <globalincs/alphacolors.hh>
#include <weapon/beam.hh>

// ----------------------------------------------------------------------------------------------------------------
// PAUSE DEFINES/VARS
//

// pause bitmap name
char *Pause_bmp_name[GR_NUM_RESOLUTIONS] = {
	"PleaseWait",
	"2_PleaseWait"
};

// pause bitmap display stuff
int Please_wait_coords[GR_NUM_RESOLUTIONS][4] = {
	{ // GR_640
		152, 217, 316, 26
	},
	{ // GR_1024
		247, 346, 510, 36
	}	
};

// pause window objects
UI_WINDOW Pause_win;
UI_CHECKBOX Pause_single_step;
UI_CHECKBOX Pause_physics;
UI_CHECKBOX Pause_ai;
UI_CHECKBOX Pause_ai_render;
UI_CHECKBOX Pause_firing;
UI_CHECKBOX Pause_external_view_mode_check;
UI_BUTTON Pause_continue;

// if we're already paused
int Paused = 0;

// background screen
int Pause_background_bitmap = -1;

// saved background screen
int Pause_saved_screen = -1;

// if we're in external vie wmode
int Pause_external_view_mode = 0;

// externs
extern int Player_attacking_enabled;
extern int Ai_render_debug_flag;
extern int Ai_firing_enabled;
extern int physics_paused;
extern int ai_paused;
extern int last_single_step;
extern int game_single_step;

// ----------------------------------------------------------------------------------------------------------------
// PAUSE FUNCTIONS
//

// initialize the pause screen
void pause_init()
{
	// if we're already paused. do nothing
	if ( Paused ) {
		return;
	}	

	// pause all beam weapon sounds
	beam_pause_sounds();

	Pause_saved_screen = gr_save_screen();

	// pause all game music
	audiostream_pause_all();

	//JAS: REMOVED CALL TO SET INTERFACE PALETTE TO GET RID OF SCREEN CLEAR WHEN PAUSING
	//common_set_interface_palette();  // set the interface palette
	Pause_win.create(0, 0, gr_screen.max_w, gr_screen.max_h, 0);

	Pause_background_bitmap = bm_load(Pause_bmp_name[gr_screen.res]);

	Paused = 1;
}

// pause do frame
void pause_do()
{
	int k;
	char *pause_str = XSTR("Paused", 767);
	int str_w, str_h;

	//	RENDER A GAME FRAME HERE AS THE BACKGROUND
	gr_restore_screen(Pause_saved_screen);
	if (Pause_background_bitmap >= 0) {
		gr_set_bitmap(Pause_background_bitmap);

		// draw the bitmap
		gr_bitmap(Please_wait_coords[gr_screen.res][0], Please_wait_coords[gr_screen.res][1]);

		// draw "Paused" on it
		gr_set_color_fast(&Color_normal);
		gr_set_font(FONT2);
		gr_get_string_size(&str_w, &str_h, pause_str);
		gr_string((gr_screen.max_w - str_w) / 2, (gr_screen.max_h - str_h) / 2, pause_str);
		gr_set_font(FONT1);
	}

	// process the ui window here
	k = Pause_win.process() & ~KEY_DEBUGGED;
	switch (k) {
	case KEY_ESC:
	case KEY_PAUSE:
		gameseq_post_event(GS_EVENT_PREVIOUS_STATE);
		break;
	}	// end switch

	// draw the background window
	Pause_win.draw();

	// a very unique case where we shouldn't be doing the page flip because we're inside of popup code
	if(!popup_active()){
		gr_flip();
	}
}

// close the pause screen
void pause_close()
{
	// if we're not paused - do nothing
	if ( !Paused ) {
		return;
	}

	// unpause all beam weapon sounds
	beam_unpause_sounds();

	// deinit stuff
	gr_free_screen(Pause_saved_screen);

	if (Pause_background_bitmap){
		bm_unload(Pause_background_bitmap);
	}

	Pause_win.destroy();
	game_flush();

	// unpause all the music
	audiostream_unpause_all();

	Paused = 0;
}

// debug pause init
void pause_debug_init()
{
	Pause_win.create( 100,100,400,300, WIN_DIALOG );

	Pause_physics.create( &Pause_win, NOX("Physics Pause <P>"), 200, 150, physics_paused );
	Pause_ai.create( &Pause_win, NOX("AI Pause <A>"), 200, 175, ai_paused );
	#ifndef NDEBUG
	Pause_ai_render.create( &Pause_win, NOX("AI Render Stuff <R>"), 200, 200, Ai_render_debug_flag);
	#endif
	Pause_firing.create( &Pause_win, NOX("AI firing <F>"), 200, 225, Ai_firing_enabled);
	Pause_external_view_mode_check.create( &Pause_win, NOX("External View <E>"), 200, 250, Pause_external_view_mode);
	Pause_single_step.create( &Pause_win, NOX("Single Step <S>"), 200, 290, game_single_step );
	Pause_continue.create( &Pause_win, NOX("Leave Pause"), 200, 350, 200, 40 );

	Pause_single_step.set_hotkey( KEY_S );
	Pause_physics.set_hotkey( KEY_P );
	Pause_ai.set_hotkey( KEY_A );
	Pause_ai_render.set_hotkey( KEY_R );
	Pause_firing.set_hotkey( KEY_F );
	Pause_external_view_mode_check.set_hotkey( KEY_E );
	Pause_continue.set_hotkey( KEY_ESC );

	Pause_continue.set_focus();
}

// debug pause do frame
void pause_debug_do()
{
	int key;

	key = Pause_win.process();
	if ( Pause_single_step.changed())	{
		game_single_step = Pause_single_step.checked();
	}

	if ( Pause_physics.changed())	{
		physics_paused = Pause_physics.checked();
	}

	if ( Pause_ai.changed())	{
		ai_paused = Pause_ai.checked();
		if (ai_paused){
			obj_init_all_ships_physics();
		}
	}

	if ( Pause_ai_render.changed())	{
		Ai_render_debug_flag = Pause_ai_render.checked();
	}

	if ( Pause_firing.changed())	{
		Ai_firing_enabled = Pause_firing.checked();
	}

	if ( Pause_external_view_mode_check.changed())	{
		Pause_external_view_mode = Pause_external_view_mode_check.checked();
		if (Pause_external_view_mode){
			HUD_sourced_printf(HUD_SOURCE_HIDDEN, XSTR( "External view of player ship.", 182));
		} else {
			HUD_sourced_printf(HUD_SOURCE_HIDDEN, XSTR( "View from inside player ship.", 183));
		}
	}

	if ( Pause_continue.pressed() || (key == KEY_PAUSE) )	{	//	Changed, MK, 11/9/97, only Pause break pause.
		gameseq_post_event(GS_EVENT_PREVIOUS_STATE);
	}

	gr_clear();
	Pause_win.draw();

	gr_flip();
}

// debug pause close
void pause_debug_close()
{
	last_single_step = 0;	// Make so single step waits a frame before stepping
	Pause_win.destroy();
	game_flush();
}
