/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell
 * or otherwise commercially exploit the source or things you created based on the
 * source.
 *
*/

#include <SDL.h>

#include <io/mouse.hh>
#include <graphics/2d.hh>
#include <osapi/osapi.hh>

LOCAL int mouse_inited = 0;
LOCAL int Mouse_x;
LOCAL int Mouse_y;

int mouse_flags;
int mouse_left_pressed = 0;
int mouse_right_pressed = 0;
int mouse_middle_pressed = 0;
int mouse_left_up = 0;
int mouse_right_up = 0;
int mouse_middle_up = 0;
int Mouse_dx = 0;
int Mouse_dy = 0;
int Mouse_dz = 0;

int Mouse_sensitivity = 4;
int Use_mouse_to_fly = 0;
int Mouse_hidden = 0;
int Keep_mouse_centered = 0;

void mouse_force_pos(int x, int y);

int mouse_is_visible()
{
	return !Mouse_hidden;
}

void mouse_close()
{
	if (!mouse_inited)
		return;

	mouse_inited = 0;
}

void mouse_init()
{
	// Initialize queue
	if ( mouse_inited ) return;
	mouse_inited = 1;

	mouse_flags = 0;
	Mouse_x = gr_screen.max_w / 2;
	Mouse_y = gr_screen.max_h / 2;

	atexit( mouse_close );
}


// ----------------------------------------------------------------------------
// mouse_mark_button() is called asynchronously by the OS when a mouse button
// goes up or down.  The mouse button that is affected is passed via the
// flags parameter.
//
// parameters:   flags ==> mouse button pressed/released
//               set   ==> 1 - button is pressed
//                         0 - button is released

void mouse_mark_button( uint flags, int set)
{
	if ( !mouse_inited ) return;

	if ( !(mouse_flags & MOUSE_LEFT_BUTTON) )	{

		if ( (flags & MOUSE_LEFT_BUTTON) && (set == 1) ) {
			mouse_left_pressed++;

////////////////////////////
/// SOMETHING TERRIBLE IS ABOUT TO HAPPEN.  I FEEL THIS IS NECESSARY FOR THE DEMO, SINCE
/// I DON'T WANT TO CALL CRITICAL SECTION CODE EACH FRAME TO CHECK THE LEFT MOUSE BUTTON.
/// PLEASE SEE ALAN FOR MORE INFORMATION.
////////////////////////////
#ifdef FS2_DEMO
					{
					extern void demo_reset_trailer_timer();
					demo_reset_trailer_timer();
					}
#endif
////////////////////////////
/// IT'S OVER.  SEE, IT WASN'T SO BAD RIGHT?  IT'S IS VERY UGLY LOOKING, I KNOW.
////////////////////////////

		}
	}
	else {
		if ( (flags & MOUSE_LEFT_BUTTON) && (set == 0) ){
			mouse_left_up++;
		}
	}

	if ( !(mouse_flags & MOUSE_RIGHT_BUTTON) )	{

		if ( (flags & MOUSE_RIGHT_BUTTON) && (set == 1) ){
			mouse_right_pressed++;
		}
	}
	else {
		if ( (flags & MOUSE_RIGHT_BUTTON) && (set == 0) ){
			mouse_right_up++;
		}
	}

	if ( !(mouse_flags & MOUSE_MIDDLE_BUTTON) )	{

		if ( (flags & MOUSE_MIDDLE_BUTTON) && (set == 1) ){
			mouse_middle_pressed++;
		}
	}
	else {
		if ( (flags & MOUSE_MIDDLE_BUTTON) && (set == 0) ){
			mouse_middle_up++;
		}
	}

	if ( set ){
		mouse_flags |= flags;
	} else {
		mouse_flags &= ~flags;
	}
}

// mouse_mark_motion() is called from the os_poll() event pump in osapi.cpp
// on SDL_MOUSEMOTION - it replaces the Win32 GetCursorPos() polling as the
// source of the tracked cursor position.
void mouse_mark_motion( int x, int y )
{
	if ( !mouse_inited ) return;

	Mouse_x = x;
	Mouse_y = y;
}

void mouse_flush()
{
	if (!mouse_inited)
		return;

	mouse_eval_deltas();
	Mouse_dx = Mouse_dy = Mouse_dz = 0;
	mouse_left_pressed = 0;
	mouse_right_pressed = 0;
	mouse_middle_pressed = 0;
	mouse_flags = 0;
}

int mouse_down_count(int n, int reset_count)
{
	int tmp = 0;
	if ( !mouse_inited ) return 0;

	if ( (n < LOWEST_MOUSE_BUTTON) || (n > HIGHEST_MOUSE_BUTTON)) return 0;

	switch (n) {
		case MOUSE_LEFT_BUTTON:
			tmp = mouse_left_pressed;
			if ( reset_count ) {
				mouse_left_pressed = 0;
			}
			break;

		case MOUSE_RIGHT_BUTTON:
			tmp = mouse_right_pressed;
			if ( reset_count ) {
				mouse_right_pressed = 0;
			}
			break;

		case MOUSE_MIDDLE_BUTTON:
			tmp = mouse_middle_pressed;
			if ( reset_count ) {
				mouse_middle_pressed = 0;
			}
			break;
	} // end switch

	return tmp;
}

// mouse_up_count() returns the number of times button n has gone from down to up
// since the last call
//
// parameters:  n - button of mouse (see #define's in mouse.h)
//
int mouse_up_count(int n)
{
	int tmp = 0;
	if ( !mouse_inited ) return 0;

	if ( (n < LOWEST_MOUSE_BUTTON) || (n > HIGHEST_MOUSE_BUTTON)) return 0;

	switch (n) {
		case MOUSE_LEFT_BUTTON:
			tmp = mouse_left_up;
			mouse_left_up = 0;
			break;

		case MOUSE_RIGHT_BUTTON:
			tmp = mouse_right_up;
			mouse_right_up = 0;
			break;

		case MOUSE_MIDDLE_BUTTON:
			tmp = mouse_middle_up;
			mouse_middle_up = 0;
			break;

		default:
			Assert(0);	// can't happen
			break;
	} // end switch

	return tmp;
}

// returns 1 if mouse button btn is down, 0 otherwise

int mouse_down(int btn)
{
	int tmp;
	if ( !mouse_inited ) return 0;

	if ( (btn < LOWEST_MOUSE_BUTTON) || (btn > HIGHEST_MOUSE_BUTTON)) return 0;

	if ( mouse_flags & btn )
		tmp = 1;
	else
		tmp = 0;

	return tmp;
}

// returns the fraction of time btn has been down since last call
// (currently returns 1 if buttons is down, 0 otherwise)
//
float mouse_down_time(int btn)
{
	float tmp;
	if ( !mouse_inited ) return 0.0f;

	if ( (btn < LOWEST_MOUSE_BUTTON) || (btn > HIGHEST_MOUSE_BUTTON)) return 0.0f;

	if ( mouse_flags & btn )
		tmp = 1.0f;
	else
		tmp = 0.0f;

	return tmp;
}

void mouse_get_delta(int *dx, int *dy, int *dz)
{
	if (dx)
		*dx = Mouse_dx;
	if (dy)
		*dy = Mouse_dy;
	if (dz)
		*dz = Mouse_dz;
}

// Forces the actual cursor to be at (x,y).  This may be independent of our tracked (x,y) mouse pos.
void mouse_force_pos(int x, int y)
{
	if (os_foreground()) {  // only mess with the cursor if we are in control of it
		SDL_Window *win = os_get_sdl_window();

		if (win) {
			SDL_WarpMouseInWindow(win, x, y);
		}

		Mouse_x = x;
		Mouse_y = y;
	}
}

// change in mouse position since last call
void mouse_eval_deltas()
{
	static int old_x = 0;
	static int old_y = 0;
	int tmp_x, tmp_y, cx, cy;

	Mouse_dx = Mouse_dy = Mouse_dz = 0;
	if (!mouse_inited)
		return;

	cx = gr_screen.max_w / 2;
	cy = gr_screen.max_h / 2;

	tmp_x = Mouse_x;
	tmp_y = Mouse_y;

	Mouse_dx = tmp_x - old_x;
	Mouse_dy = tmp_y - old_y;
	Mouse_dz = 0;

	if (Keep_mouse_centered && Mouse_hidden) {
		if (Mouse_dx || Mouse_dy)
			mouse_force_pos(cx, cy);

		old_x = cx;
		old_y = cy;

	} else {
		old_x = tmp_x;
		old_y = tmp_y;
	}
}

int mouse_get_pos(int *xpos, int *ypos)
{
	int flags;

	if (!mouse_inited) {
		*xpos = *ypos = 0;
		return 0;
	}

	flags = mouse_flags;

	if (Mouse_x < 0){
		Mouse_x = 0;
	}

	if (Mouse_y < 0){
		Mouse_y = 0;
	}

	if (Mouse_x >= gr_screen.max_w){
		Mouse_x = gr_screen.max_w - 1;
	}

	if (Mouse_y >= gr_screen.max_h){
		Mouse_y = gr_screen.max_h - 1;
	}

	if (xpos){
		*xpos = Mouse_x;
	}

	if (ypos){
		*ypos = Mouse_y;
	}

	return flags;
}

void mouse_get_real_pos(int *mx, int *my)
{
	*mx = Mouse_x;
	*my = Mouse_y;
}

void mouse_set_pos(int xpos, int ypos)
{
	if ((xpos != Mouse_x) || (ypos != Mouse_y)){
		mouse_force_pos(xpos, ypos);
	}
}
