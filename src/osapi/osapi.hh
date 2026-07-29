/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef _OSAPI_H
#define _OSAPI_H

#include <globalincs/pstypes.hh>

// --------------------------------------------------------------------------------------------------
// OSAPI FUNCTIONS
//

// initialization/shutdown functions -----------------------------------------------

// If app_name is NULL or ommited, then TITLE is used
// for the app name, which is where registry keys are stored.
void os_init(char *wclass, char *title, char *app_name = NULL,
             char *version_string = NULL);

// set the main window title
void os_set_title(char *title);

// call at program end
void os_cleanup();

// window management ---------------------------------------------------------------

// Returns 1 if app is not the foreground app.
int os_foreground();

// SDL2 window management.  osapi owns the SDL window; the graphics backend
// renders into it.  Forward declared so this header doesn't drag SDL in.
struct SDL_Window;

// Returns the main SDL window, or NULL until os_create_window() succeeds.
SDL_Window *os_get_sdl_window();

// Create (or resize) and show the main window.  Returns 0 on success.
// use_opengl asks for a GL-capable window (the GL backend creates its
// context on it); the software renderer blits via the window surface.
int os_create_window(int w, int h, int use_opengl = 0);

// process management --------------------------------------------------------------

// drain pending SDL events and route them to key/mouse/focus/quit handling
void os_poll();

// Sleeps for n milliseconds or until app becomes active.
void os_sleep(int ms);

#endif
