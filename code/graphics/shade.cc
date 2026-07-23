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
#include <math/floating.hh>
#include <graphics/line.hh>
#include <palman/palman.hh>

void
grx_create_shader(shader *shade, float r, float g, float b, float c)
{
    int i;
    float Sr, Sg, Sb;
    float Dr, Dg, Db;
    int ri, gi, bi;

    shade->screen_sig = gr_screen.signature;
    shade->r = r;
    shade->g = g;
    shade->b = b;
    shade->c = c;

    for (i = 0; i < 256; i++) {
        Sr = i2fl(gr_palette[i * 3 + 0]);
        Sg = i2fl(gr_palette[i * 3 + 1]);
        Sb = i2fl(gr_palette[i * 3 + 2]);
        Dr = Sr * r + Sg * r + Sb * r + c * 256.0f;
        Dg = Sr * g + Sg * g + Sb * g + c * 256.0f;
        Db = Sr * b + Sg * b + Sb * b + c * 256.0f;
        ri = fl2i(Dr);
        if (ri < 0)
            ri = 0;
        else if (ri > 255)
            ri = 255;
        gi = fl2i(Dg);
        if (gi < 0)
            gi = 0;
        else if (gi > 255)
            gi = 255;
        bi = fl2i(Db);
        if (bi < 0)
            bi = 0;
        else if (bi > 255)
            bi = 255;
        shade->lookup[i] = (unsigned char)(palette_find(ri, gi, bi));
    }
}

void
grx_set_shader(shader *shade)
{
    if (shade) {
        if (shade->screen_sig != gr_screen.signature) {
            gr_create_shader(shade, shade->r, shade->g, shade->b, shade->c);
        }
        gr_screen.current_shader = *shade;
    }
    else {
        gr_create_shader(&gr_screen.current_shader, 0.0f, 0.0f, 0.0f, 0.0f);
    }
}

void
gr8_shade(int x, int y, int w, int h)
{
    int x1, y1, x2, y2;
    ubyte *xlat_table;

    x1 = x;
    if (x1 < gr_screen.clip_left)
        x1 = gr_screen.clip_left;
    if (x1 > gr_screen.clip_right)
        x1 = gr_screen.clip_right;

    x2 = x + w - 1;
    if (x2 < gr_screen.clip_left)
        x2 = gr_screen.clip_left;
    if (x2 > gr_screen.clip_right)
        x2 = gr_screen.clip_right;

    y1 = y;
    if (y1 < gr_screen.clip_top)
        y1 = gr_screen.clip_top;
    if (y1 > gr_screen.clip_bottom)
        y1 = gr_screen.clip_bottom;

    y2 = y + h - 1;
    if (y2 < gr_screen.clip_top)
        y2 = gr_screen.clip_top;
    if (y2 > gr_screen.clip_bottom)
        y2 = gr_screen.clip_bottom;

    w = x2 - x1 + 1;
    if (w < 1)
        return;

    h = y2 - y1 + 1;
    if (h < 1)
        return;

    int i;
    xlat_table = gr_screen.current_shader.lookup;

    gr_lock();

    for (i = 0; i < h; i++) {
        ubyte *dp = GR_SCREEN_PTR(ubyte, x1, y1 + i);
        // (retail also had a dword-aligned, 4-pixels-at-a-time asm unroll of this loop)
        for (int j = 0; j < w; j++) {
            *dp = xlat_table[*dp];
            dp++;
        }
    }

    gr_unlock();
}
