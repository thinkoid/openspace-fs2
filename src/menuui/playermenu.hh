/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef _PLAYER_SELECT_MENU_HEADER_FILE
#define _PLAYER_SELECT_MENU_HEADER_FILE

// general defines
// looking through single player pilots
#define PLAYER_SELECT_MODE_SINGLE 0

// flag indicating if this is the absolute first pilot created and selected. Used to determine
// if the main hall should display the help overlay screen
extern int Player_select_very_first_pilot;

// functions for selecting pilots at the very beginning of Freespace
void player_select_init();
void player_select_do();
void player_select_close();

// function to check whether we found a "last pilot". loads this pilot in if possible and returns true, or false otherwise
int player_select_get_last_pilot();

// tooltips
void player_tips_init();
void player_tips_popup();

#endif
