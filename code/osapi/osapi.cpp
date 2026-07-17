/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell
 * or otherwise commercially exploit the source or things you created based on the
 * source.
 *
*/

#include <SDL.h>

#include <stdio.h>
#include <stdarg.h>

#include "pstypes.h"
#include "osapi.h"
#include "key.h"
#include "joy.h"
#include "mouse.h"
#include "outwnd.h"
#include "2d.h"
#include "gamesequence.h"
#include "osregistry.h"
#include "cmdline.h"

// ----------------------------------------------------------------------------------------------------
// OSAPI DEFINES/VARS
//

// os-wide globals
static SDL_Window	*sdl_window = NULL;
static int			fAppActive = 0;
static int			main_window_inited = 0;
static char			szWinTitle[128];
static char			szWinClass[128];
static int			Os_inited = 0;

int Os_debugger_running = 0;

// ----------------------------------------------------------------------------------------------------
// OSAPI FORWARD DECLARATIONS
//

// called at shutdown
void os_deinit();

// input hooks, defined in key.cpp / mouse.cpp.  Deliberately not in their
// public headers - only this event pump uses them.
extern void key_mark_sdl_scancode( int sdl_scancode, int state );
extern void mouse_mark_motion( int x, int y );


// ----------------------------------------------------------------------------------------------------
// OSAPI FUNCTIONS
//

// initialization/shutdown functions -----------------------------------------------

// If app_name is NULL or ommited, then TITLE is used
// for the app name, which is where registry keys are stored.
void os_init(char * wclass, char * title, char *app_name, char *version_string )
{
	os_init_registry_stuff(Osreg_company_name, title, version_string);

	strcpy( szWinTitle, title );
	strcpy( szWinClass, wclass );

	// TIMER drives the audio-stream service callbacks (audiostr.cpp)
	if ( SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0 )	{
		mprintf(( "SDL_Init failed: %s\n", SDL_GetError() ));
	}

	// initialized.  The window itself is created later, when the graphics
	// code calls os_create_window().
	Os_inited = 1;

	atexit(os_deinit);
}

// set the main window title
void os_set_title( char * title )
{
	strcpy( szWinTitle, title );
	if ( sdl_window )	{
		SDL_SetWindowTitle( sdl_window, szWinTitle );
	}
}

// call at program end
void os_cleanup()
{
	// window and SDL are torn down in os_deinit(), via atexit

	#ifndef NDEBUG
		outwnd_close();
	#endif
}


// window management -----------------------------------------------------------------

// Returns 1 if app is not the foreground app.
int os_foreground()
{
	return fAppActive;
}

// Returns the handle to the main window
uint os_get_window()
{
	return 0;
}

// Returns the main SDL window, or NULL until os_create_window() succeeds.
SDL_Window *os_get_sdl_window()
{
	return sdl_window;
}

// Create (or resize) and show the main window.  Returns 0 on success.
int os_create_window(int w, int h)
{
	if ( !Os_inited )	{
		return -1;
	}

	if ( sdl_window )	{
		SDL_SetWindowSize( sdl_window, w, h );
		SDL_ShowWindow( sdl_window );
		return 0;
	}

	// no SDL_WINDOW_OPENGL: the software renderer blits via the window
	// surface, and the GL flag breaks headless (dummy-driver) runs
	sdl_window = SDL_CreateWindow( szWinTitle,
								SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
								w, h, 0 );
	if ( !sdl_window )	{
		mprintf(( "SDL_CreateWindow failed: %s\n", SDL_GetError() ));
		return -1;
	}

	// Hack!! Turn off the OS cursor, same as the retail window did.
	SDL_ShowCursor(SDL_DISABLE);

	main_window_inited = 1;
	#ifndef NDEBUG
		outwnd_init(1);
	#endif

	return 0;
}


// process management -----------------------------------------------------------------

// Sleeps for n milliseconds or until app becomes active.
void os_sleep(int ms)
{
	SDL_Delay(ms);
}

// Used to stop message processing
void os_suspend()
{
	// single threaded now - nothing to suspend
}

// resume message processing
void os_resume()
{
	// single threaded now - nothing to resume
}

// the SDL replacement for the retail win32_message_handler / win32_process2
// message pump: drain pending SDL events and route them to key/mouse, the
// focus handling, and the quit path.
void os_poll()
{
	SDL_Event e;

	while ( SDL_PollEvent(&e) )	{
		switch (e.type)	{

		case SDL_KEYDOWN:
			key_mark_sdl_scancode( e.key.keysym.scancode, 1 );
			break;

		case SDL_KEYUP:
			key_mark_sdl_scancode( e.key.keysym.scancode, 0 );
			break;

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:	{
				int state = (e.type == SDL_MOUSEBUTTONDOWN) ? 1 : 0;

				switch (e.button.button)	{
				case SDL_BUTTON_LEFT:
					mouse_mark_button( MOUSE_LEFT_BUTTON, state );
					break;
				case SDL_BUTTON_RIGHT:
					mouse_mark_button( MOUSE_RIGHT_BUTTON, state );
					break;
				case SDL_BUTTON_MIDDLE:
					mouse_mark_button( MOUSE_MIDDLE_BUTTON, state );
					break;
				}
			}
			break;

		case SDL_MOUSEMOTION:
			mouse_mark_motion( e.motion.x, e.motion.y );
			break;

		case SDL_WINDOWEVENT:
			switch (e.window.event)	{

			case SDL_WINDOWEVENT_FOCUS_GAINED:
				// the retail WM_SETFOCUS / WM_ACTIVATE path
				if ( !fAppActive )	{
					fAppActive = 1;
					key_got_focus();
					gr_activate(1);
				}
				break;

			case SDL_WINDOWEVENT_FOCUS_LOST:
				// the retail WM_KILLFOCUS / WM_ACTIVATE path
				if ( fAppActive )	{
					fAppActive = 0;
					key_lost_focus();
					if (Mouse_hidden)	{
						Mouse_hidden = 0;
					}
					gr_activate(0);
				}
				break;

			case SDL_WINDOWEVENT_CLOSE:
				// same mechanism the retail WM_CLOSE handler used
				gameseq_post_event(GS_EVENT_QUIT_GAME);
				break;
			}
			break;

		case SDL_QUIT:
			// same mechanism the retail WM_CLOSE handler used
			gameseq_post_event(GS_EVENT_QUIT_GAME);
			break;
		}
	}

	// retail polled the joystick from a winmm thread; we pump it here
	joy_process();
}

// called at shutdown
void os_deinit()
{
	if ( sdl_window )	{
		SDL_DestroyWindow( sdl_window );
		sdl_window = NULL;
	}

	SDL_Quit();
}
