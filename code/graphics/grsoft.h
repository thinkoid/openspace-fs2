/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#ifndef _GRSOFT_H
#define _GRSOFT_H

void gr_soft_init();
void gr_soft_cleanup();


// Functions/variables common between grsoft and grdirectdraw
extern int Grx_mouse_saved;
void grx_save_mouse_area(int x, int y, int w, int h );
void grx_restore_mouse_area();
void grx_print_screen(char * filename);
int gr8_save_screen();
void gr8_restore_screen(int id);
void gr8_free_screen(int id);
void gr8_dump_frame_start(int first_frame, int frames_between_dumps);
void gr8_dump_frame();
void gr8_dump_frame_stop();
void gr8_set_gamma(float gamma);

// bitmap functions
void grx_bitmap(int x, int y);
void grx_bitmap_ex(int x, int y, int w, int h, int sx, int sy);

#endif
