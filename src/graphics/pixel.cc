/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#include <graphics/2d.hh>
#include <graphics/grinternal.hh>
#include <graphics/pixel.hh>
#include <palman/palman.hh>

void
gr8_pixel(int x, int y)
{
    ubyte *dptr;

    if (x < gr_screen.clip_left)
        return;
    if (x > gr_screen.clip_right)
        return;
    if (y < gr_screen.clip_top)
        return;
    if (y > gr_screen.clip_bottom)
        return;

    gr_lock();

    dptr = GR_SCREEN_PTR(ubyte, x, y);
    if (Current_alphacolor) {
        // *dptr = Current_alphacolor->table.lookup[14][*dptr];
    }
    else {
        *dptr = gr_screen.current_color.raw8;
    }

    gr_unlock();
}
