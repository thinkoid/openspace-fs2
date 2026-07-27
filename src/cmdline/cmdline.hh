/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#ifndef FS_CMDLINE_HEADER_FILE
#define FS_CMDLINE_HEADER_FILE

int parse_cmdline(int argc, char **argv);

// COMMAND LINE SETTINGS
// This section is for reference by all the *_init() functions. For example, the sound init function
// could check to see if (int Cmdline_freespace_no_sound) has been set by the command line parser.
//
// Add any extern definitions here and put the actual variables inside of cmdline.cpp for ease of use
// Also, check to make sure anything you add doesn't break Fred or TestCode

extern int Cmdline_freespace_no_sound;
extern int Cmdline_freespace_no_music;
extern int Cmdline_gimme_all_medals;
extern int Cmdline_use_last_pilot;
extern int Cmdline_cd_check;
extern int Cmdline_spew_pof_info;
extern int Cmdline_mouse_coords;

extern int Cmdline_window;
extern int Cmdline_opengl;

#endif
