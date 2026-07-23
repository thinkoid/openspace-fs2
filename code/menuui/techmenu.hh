/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef _TECHMENU_H
#define _TECHMENU_H

#define MAX_INTEL_ENTRIES 10
#define TECH_INTEL_DESC_LEN 5120

typedef struct
{
    char name[32];
    char desc[TECH_INTEL_DESC_LEN];
    char anim_filename[32];
    int in_tech_db; // determines if visible in tech db or not
} intel_data;

extern intel_data Intel_info[MAX_INTEL_ENTRIES];
extern int Intel_info_size;

// function prototypes
void techroom_init();
void techroom_close();
void techroom_do_frame(float frametime);
int techroom_on_ships_tab();
void
techroom_intel_init(); // called on startup so campaigns can manipulate tech room visibility

#endif
