/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

// (windows.h removed)
// (windowsx.h removed)

#include <graphics/2d.hh>
#include <graphics/grinternal.hh>
#include <graphics/gradient.hh>
#include <math/floating.hh>
#include <graphics/line.hh>
#include <palman/palman.hh>

void
gr8_gradient(int x1, int y1, int x2, int y2)
{
#ifndef HARDWARE_ONLY
    int i;
    int xstep, ystep;
    int clipped = 0, swapped = 0;
    ubyte *xlat_table;

    int l = 0, dl = 0;

    if (!Current_alphacolor) {
        gr_line(x1, y1, x2, y2);
        return;
    }

    INT_CLIPLINE(x1, y1, x2, y2, gr_screen.clip_left, gr_screen.clip_top,
                 gr_screen.clip_right, gr_screen.clip_bottom, return, clipped = 1,
                 swapped = 1);

    int dy = y2 - y1;
    int dx = x2 - x1;
    int error_term = 0;

    if (dx == 0 && dy == 0) {
        return;
    }

    gr_lock();

    ubyte *dptr = GR_SCREEN_PTR(ubyte, x1, y1);

    xlat_table = (ubyte *)&Current_alphacolor->table.lookup[0][0];

    if (dy < 0) {
        dy = -dy;
        ystep = -gr_screen.rowsize / gr_screen.bytes_per_pixel;
    }
    else {
        ystep = gr_screen.rowsize / gr_screen.bytes_per_pixel;
    }

    if (dx < 0) {
        dx = -dx;
        xstep = -1;
    }
    else {
        xstep = 1;
    }

    if (dx > dy) {
        if (!swapped) {
            l = 14 << 8;
            dl = (-14 << 8) / dx;
        }
        else {
            l = 0;
            dl = (14 << 8) / dx;
        }

        for (i = 0; i < dx; i++) {
            *dptr = xlat_table[(l & 0xF00) | *dptr];
            l += dl;
            dptr += xstep;
            error_term += dy;

            if (error_term > dx) {
                error_term -= dx;
                dptr += ystep;
            }
        }
        *dptr = xlat_table[(l & 0xF00) | *dptr];
    }
    else {
        if (!swapped) {
            l = 14 << 8;
            dl = (-14 << 8) / dy;
        }
        else {
            l = 0;
            dl = (14 << 8) / dy;
        }

        for (i = 0; i < dy; i++) {
            *dptr = xlat_table[(l & 0xF00) | *dptr];
            l += dl;
            dptr += ystep;
            error_term += dx;
            if (error_term > 0) {
                error_term -= dy;
                dptr += xstep;
            }
        }
        *dptr = xlat_table[(l & 0xF00) | *dptr];
    }
    gr_unlock();
#else
    Int3();
#endif
}
