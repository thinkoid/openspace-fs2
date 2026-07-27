/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef _MISSIONBRIEF_H
#define _MISSIONBRIEF_H

#include <ui/ui.hh>

// #defines to identify which screen we are on
#define ON_BRIEFING_SELECT 1
#define ON_SHIP_SELECT 2
#define ON_WEAPON_SELECT 3

// briefing buttons
#define BRIEF_BUTTON_LAST_STAGE 0
#define BRIEF_BUTTON_NEXT_STAGE 1
#define BRIEF_BUTTON_PREV_STAGE 2
#define BRIEF_BUTTON_FIRST_STAGE 3
#define BRIEF_BUTTON_SCROLL_UP 4
#define BRIEF_BUTTON_SCROLL_DOWN 5
#define BRIEF_BUTTON_SKIP_TRAINING 6
#define BRIEF_BUTTON_PAUSE 7
#define BRIEF_BUTTON_EXIT_LOOP 8

#define NUM_BREIFING_REGIONS (NUM_COMMON_REGIONS + 8)

extern int Brief_background_bitmap;

// Sounds
#define BRIEFING_MUSIC_DELAY 2500 // 650 ms delay before breifing music starts
extern int Briefing_music_handle;
extern int Briefing_music_begin_timestamp;

void brief_init();
void brief_close();
void brief_do_frame(float frametime);
void brief_unhide_buttons();
uintptr_t brief_get_closeup_icon(); // carries a brief_icon* (was uint on 32-bit)
void brief_turn_off_closeup_icon();

void briefing_stop_music();
void briefing_start_music();
void briefing_load_music(char *fname);
void brief_stop_voices();

int brief_only_allow_briefing();

#endif // don't add anything past this line
