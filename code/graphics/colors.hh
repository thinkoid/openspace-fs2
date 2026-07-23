/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef _COLORS_H
#define _COLORS_H

struct alphacolor_old;

void grx_init_alphacolors();
void grx_init_color(color *clr, int r, int g, int b);
void grx_init_alphacolor(color *clr, int r, int g, int b, int alpha, int type);
void grx_set_color(int r, int g, int b);
void grx_set_color_fast(color *clr);
void grx_get_color(int *r, int *g, int *b);

void calc_alphacolor_old(alphacolor_old *ac);

#endif
