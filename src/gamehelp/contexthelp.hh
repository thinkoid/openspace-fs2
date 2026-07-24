/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

// ContextHelp.h
//
//

#ifndef __CONTEXTHELP_H__
#define __CONTEXTHELP_H__

// Help overlays
//
// Adding a help overlay:    1) Add a #define that uniquely identifies your help overlay
//                                                                        2) Increment MAX_HELP_OVERLAYS
//                                                                        3) Add the filename for the help overlay to Help_overlays[] array in ContextHelp.cpp
//
// Must be kept current
#define MAX_HELP_OVERLAYS 16

// ship selection help
#define SS_OVERLAY 0
// weapons loadout help
#define WL_OVERLAY 1
// briefing help
#define BR_OVERLAY 2
// main hall help
#define MH_OVERLAY 3
// barracks help
#define BARRACKS_OVERLAY 4
// control config help
#define CONTROL_CONFIG_OVERLAY 5
// debriefing help
#define DEBRIEFING_OVERLAY 6
// multi create game help
#define MULTI_CREATE_OVERLAY 7
// multi start game help overlay
#define MULTI_START_OVERLAY 8
// join game help overlay
#define MULTI_JOIN_OVERLAY 9
// main hall 2 help overlay
#define MH2_OVERLAY 10
// hotkey assignment help overlay
#define HOTKEY_OVERLAY 11
// campaign room help overlay
#define CAMPAIGN_ROOM_OVERLAY 12
// sim room help overlay
#define SIM_ROOM_OVERLAY 13
// tech room (general) help overlay
#define TECH_ROOM_OVERLAY 14
// command briefing help overlay
#define CMD_BRIEF_OVERLAY 15

// other help overlay constants
// max number of screen elements per element type per overlay
#define HELP_MAX_ITEM 50
// 
#define HELP_PADDING 1
// max string length for text overlay element
#define HELP_MAX_STRING_LENGTH 128
// good for 20 segments, can prolly reduce this (FIXME)
#define HELP_MAX_PLINE_VERTICES 21
#define HELP_PLINE_THICKNESS 2
#define HELP_OVERLAY_FILENAME "help.tbl"

// help overlay calls
int help_overlay_active(int overlay_id);
void help_overlay_set_state(int overlay_id, int state);
void help_overlay_load(int overlay_id);
void help_overlay_unload(int overlay_id);
void help_overlay_maybe_blit(int overlay_id);

void context_help_init(); // called once at game startup
void
context_help_grey_screen(); // call to grey out a screen (normally when applying a help overlay)

void launch_context_help();

#endif /* __CONTEXTHELP_H__ */
