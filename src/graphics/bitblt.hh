/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef _BITBLT_H
#define _BITBLT_H

void grx_bitmap(int x, int y);
void grx_bitmap_ex(int x, int y, int w, int h, int sx, int sy);
void grx_aabitmap(int x, int y);
void grx_aabitmap_ex(int x, int y, int w, int h, int sx, int sy);

#endif //_BITBLT_H
