/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <graphics/2d.hh>
#include <graphics/grinternal.hh>
#include <render/3d.hh>
#include <graphics/tmapper.hh>
#include <bmpman/bmpman.hh>
#include <graphics/tmapscanline.hh>
#include <io/key.hh>
#include <math/floating.hh>
#include <palman/palman.hh>

typedef void (*pscanline)();

pscanline tmap_scanline;

int Tmap_screen_flags = -1;
int Tmap_npolys = 0;
int Tmap_nverts = 0;
int Tmap_nscanlines = 0;
int Tmap_npixels = 0;
tmapper_data Tmap;
int Tmap_show_layers = 0;

typedef struct tmap_scan_desc
{
    uint flags;
    pscanline scan_func;
} tmap_scan_desc;

// Convert from a 0-255 byte to a 0-1.0 float.
float Light_table[256];

//====================== 8-BPP SCANLINES ========================
tmap_scan_desc tmap_scanlines8[] = {
    { 0, tmapscan_flat8 },
    { TMAP_FLAG_TEXTURED, tmapscan_lnn8 },
    { TMAP_FLAG_TEXTURED | TMAP_FLAG_XPARENT, tmapscan_lnt8 },
    { TMAP_FLAG_TEXTURED | TMAP_FLAG_RAMP, tmapscan_lln8 },
    { TMAP_FLAG_TEXTURED | TMAP_FLAG_RAMP | TMAP_FLAG_CORRECT, tmapscan_pln8 },

    { TMAP_FLAG_RAMP | TMAP_FLAG_GOURAUD, tmapscan_flat_gouraud },

    { TMAP_FLAG_TEXTURED | TMAP_FLAG_RAMP | TMAP_FLAG_GOURAUD, tmapscan_lln8 },
    { TMAP_FLAG_TEXTURED | TMAP_FLAG_RAMP | TMAP_FLAG_GOURAUD | TMAP_FLAG_CORRECT,
      tmapscan_pln8 },

    { TMAP_FLAG_TEXTURED | TMAP_FLAG_RAMP | TMAP_FLAG_CORRECT | TMAP_FLAG_TILED,
      tmapscan_pln8_tiled },
    { TMAP_FLAG_TEXTURED | TMAP_FLAG_RAMP | TMAP_FLAG_CORRECT |
          TMAP_FLAG_GOURAUD | TMAP_FLAG_TILED,
      tmapscan_pln8_tiled },

    { TMAP_FLAG_RAMP | TMAP_FLAG_GOURAUD | TMAP_FLAG_NEBULA, tmapscan_nebula8 },

    //	{ TMAP_FLAG_TEXTURED|TMAP_FLAG_TILED, tmapscan_lnn8_tiled_256x256 },
    // Totally non-general specific inner loop for subspace effect
    { TMAP_FLAG_TEXTURED | TMAP_FLAG_CORRECT | TMAP_FLAG_TILED,
      tmapscan_pnn8_tiled_256x256_subspace },

    { 0, NULL }, // Dummy element to mark the end of fast scanlines.
};

pscanline tmap_scanline_table[TMAP_MAX_SCANLINES];

// -------------------------------------------------------------------------------------
// This sets up the tmapper at the start of a given frame, so everything
// can can be pre-calculated should be calculated in here.
// This just fills in the tmap_scanline_table for the
// appropriate scan lines.

void
tmapper_setup()
{
    int i;
    tmap_scan_desc *func_table = NULL;

    Tmap_screen_flags = gr_screen.mode;

    // Some constants for the inner loop
    Tmap.FixedScale = 65536.0f;
    Tmap.FixedScale8 = 2048.0f; //8192.0f;	// 2^16 / 8
    Tmap.One = 1.0f;

    // Set tmap_scanline to not call a function
    for (i = 0; i < TMAP_MAX_SCANLINES; i++) {
        tmap_scanline_table[i] = NULL;
    }

    func_table = tmap_scanlines8;

    while (func_table->scan_func != NULL) {
        tmap_scanline_table[func_table->flags] = func_table->scan_func;
        func_table++;
    }

    for (i = 0; i < 256; i++) {
        Light_table[i] = i2fl(i) / 255.0f;
    }
}

// Sets up flat-shaded lighting
void
tmapper_set_light(vertex *v, uint flags)
{
    if (flags & TMAP_FLAG_GOURAUD)
        return;

    if ((flags & (TMAP_FLAG_RAMP | TMAP_FLAG_RGB)) ==
        (TMAP_FLAG_RAMP | TMAP_FLAG_RGB)) {
        Int3(); // You're doing RGB and RAMP lighting!!!
    }

    if (flags & TMAP_FLAG_RAMP) {
        Tmap.l.b = Tmap.r.b = i2fl(v->b) / 255.0f;
        Tmap.deltas.b = 0.0f;
    }
}

void
tmapper_show_layers()
{
    int i;
    ubyte *ptr = (ubyte *)Tmap.dest_row_data;

    for (i = 0; i < Tmap.loop_count; i++, ptr++) {
        *ptr = (unsigned char)(*ptr + 1);
    }
}

/*
void tmap_scan_generic()
{
	int ui,vi,i;
	ubyte * dptr,c;
	float l, dl;
	float u, v, w, du, dv, dw;
	
	dptr = (ubyte *)Tmap.dest_row_data;

	Tmap.fx_w = fl2i(Tmap.l.sw * GR_Z_RANGE)+gr_zoffset;
	Tmap.fx_dwdx = fl2i(Tmap.deltas.sw * GR_Z_RANGE);

	l = Tmap.l.b;
	dl = Tmap.deltas.b;

	u = Tmap.l.u;
	v = Tmap.l.v;
	w = Tmap.l.sw;
	du = Tmap.deltas.u;
	dv = Tmap.deltas.v;
	dw = Tmap.deltas.sw;
	
	for (i=0; i<Tmap.loop_count; i++ )	{
		int tmp = (uint)dptr-Tmap.pScreenBits;
		if ( Tmap.fx_w > (int)gr_zbuffer[tmp] )	{
			gr_zbuffer[tmp] = Tmap.fx_w;

			ui = fl2i( u / w ) % Tmap.bp->w;
			vi = fl2i( v / w ) % Tmap.bp->h;

			c = Tmap.pixptr[vi*Tmap.bp->w+ui];
			*dptr = gr_fade_table[fl2i(l*31)*256+c];

		}
		Tmap.fx_w += Tmap.fx_dwdx;
		l+=dl;
		u+=du;
		v+=dv;
		w+=dw;
		dptr++;
	}
}
*/

// Same as ftol except that it might round up or down,
// unlike C's ftol, which must always round down.
// But, in the tmapper, we don't care, since this is
// just for Z and L.
inline int
tmap_ftol(float f)
{
    // was fld/fistp: converts with the current rounding mode (nearest),
    // which is exactly what lrintf does
    return (int)lrintf(f);
}

/*
#define tmap_ftol(f) ((int)(f))

__ftol:
004569c0   push      ebp
004569c1   mov       ebp,esp
004569c3   add       esp,fffffff4
004569c6   wait
004569c7   fnstcw    [ebp-02]
004569ca   wait
004569cb   mov       ax,word ptr [ebp-02]
004569cf   or        ah,0c
004569d2   mov       word ptr [ebp-04],ax
004569d6   fldcw     [ebp-04]
004569d9   fistp     qword ptr [ebp-0c]
004569dc   fldcw     [ebp-02]
004569df   mov       eax,dword ptr [ebp-0c]
004569e2   mov       edx,dword ptr [ebp-08]
004569e5   leave
004569e6   ret
*/

#ifdef RGB_LIGHTING

extern ubyte gr_palette[768];

uint last_code = 0xf;
void
change_fade_table(uint code)
{
    int i, l;

    if (last_code == code)
        return;
    last_code = code;

    int r1 = 0, g1 = 0, b1 = 0;

    for (i = 0; i < 256; i++) {
        int r, g, b;
        int ur, ug, ub;
        r = gr_palette[i * 3 + 0];
        g = gr_palette[i * 3 + 1];
        b = gr_palette[i * 3 + 2];

        if ((r == 255) && (g == 255) && (b == 255)) {
            // Make pure white not fade
            for (l = 0; l < 32; l++) {
                gr_fade_table[((l + 1) * 256) + i] = (unsigned char)i;
            }
        }
        else {
            for (l = 24; l < 32; l++) {
                int x, y;
                int gi, gr, gg, gb;

                gi = (r + g + b) / 3;

                //gr = r*2;
                //gg = g*2;
                //gb = b*2;

                if (code & 4)
                    gr = gi * 2;
                else
                    gr = r;
                if (code & 2)
                    gg = gi * 2;
                else
                    gg = g;
                if (code & 1)
                    gb = gi * 2;
                else
                    gb = b;

                //				gr = r1;
                //				gg = g1;
                //				gb = b1;	//gi*2;

                x = l - 24; // x goes from 0 to 7
                y = 31 - l; // y goes from 7 to 0

                ur = ((gr * x) + (r * y)) / 7;
                if (ur > 255)
                    ur = 255;
                ug = ((gg * x) + (g * y)) / 7;
                if (ug > 255)
                    ug = 255;
                ub = ((gb * x) + (b * y)) / 7;
                if (ub > 255)
                    ub = 255;

                gr_fade_table[((l + 1) * 256) + i] = (unsigned char)palette_find(
                    ur, ug, ub);
            }
        }
        gr_fade_table[(0 * 256) + i] = gr_fade_table[(1 * 256) + i];
        gr_fade_table[(33 * 256) + i] = gr_fade_table[(32 * 256) + i];
    }

    // Mirror the fade table
    for (i = 0; i < 34; i++) {
        for (l = 0; l < 256; l++) {
            gr_fade_table[((67 - i) * 256) + l] = gr_fade_table[(i * 256) + l];
        }
    }
}
#endif

void
grx_tmapper(int nverts, vertex **verts, uint flags)
{
    int i, y, li, ri, ly, ry, top, rem;
    float ymin;
    int next_break;
    float ulist[TMAP_MAX_VERTS], vlist[TMAP_MAX_VERTS], llist[TMAP_MAX_VERTS];

    flags &= (~TMAP_FLAG_ALPHA);
    flags &= (~TMAP_FLAG_NONDARKENING);
    flags &= (~TMAP_FLAG_PIXEL_FOG);

    // FS2-era callers request hardware modes freely; the software tmapper
    // keeps only the bits its table knows.  RGB lighting doesn't exist
    // here (and gouraud requires ramp, so it goes with it).
    if (flags & TMAP_FLAG_RGB) {
        flags &= ~(TMAP_FLAG_RGB | TMAP_FLAG_GOURAUD);
    }
    flags &= (TMAP_FLAG_TEXTURED | TMAP_FLAG_CORRECT | TMAP_FLAG_RAMP |
              TMAP_FLAG_GOURAUD | TMAP_FLAG_XPARENT | TMAP_FLAG_TILED |
              TMAP_FLAG_NEBULA);

    // Check for invalid flags
#ifndef NDEBUG
    if ((flags & (TMAP_FLAG_RAMP | TMAP_FLAG_RGB)) ==
        (TMAP_FLAG_RAMP | TMAP_FLAG_RGB)) {
        Int3(); // You're doing RGB and RAMP lighting!!!
    }

    if (flags & TMAP_FLAG_RGB) {
        Int3(); // RGB not supported!
    }

    if ((flags & TMAP_FLAG_GOURAUD) && (!(flags & TMAP_FLAG_RAMP))) {
        Int3(); // Ramp mode required for gouraud!
    }

    if (gr_screen.bits_per_pixel != 8) {
        Int3(); // Only 8-bpp tmapper supported
    }

    Tmap_npolys++;
    Tmap_nverts += nverts;

    Assert(nverts <= TMAP_MAX_VERTS);

#endif

    if (flags & (TMAP_FLAG_RAMP | TMAP_FLAG_GOURAUD)) {
        for (i = 0; i < nverts; i++) {
            llist[i] = Light_table[verts[i]->b];
        }
    }

    if (Tmap_screen_flags != gr_screen.mode) {
        tmapper_setup();
    }

    tmap_scanline = tmap_scanline_table[flags];
    //	tmap_scanline = tmap_scan_generic;

    // combos retail's table never listed (e.g. TEXTURED|CORRECT|XPARENT
    // from laser quads) degrade gracefully instead of asserting.  The
    // flags must degrade WITH the scanline: the per-span setup below
    // branches on them, and a linear scanline fed by the perspective
    // setup reads stale fx_u/fx_v and walks off the texture.
    if (tmap_scanline == NULL) {
        uint fallback[3] = { flags & ~TMAP_FLAG_CORRECT,
                             flags & (TMAP_FLAG_TEXTURED | TMAP_FLAG_XPARENT),
                             flags & TMAP_FLAG_TEXTURED };
        for (i = 0; i < 3; i++) {
            if (tmap_scanline_table[fallback[i]] != NULL) {
                flags = fallback[i];
                tmap_scanline = tmap_scanline_table[flags];
                break;
            }
        }
    }

#ifndef NDEBUG
    Assert(tmap_scanline != NULL);

    if (Tmap_show_layers)
        tmap_scanline = tmapper_show_layers;
#endif

    if (tmap_scanline == NULL)
        return;

    Tmap.FadeLookup = (uintptr_t)palette_get_fade_table();
    Tmap.BlendLookup = (uintptr_t)palette_get_blend_table(
        gr_screen.current_alpha);

    if (flags & TMAP_FLAG_TEXTURED) {
        Tmap.bp = bm_lock(gr_screen.current_bitmap, 8, 0);

        int was_tiled = 0, can_tile = 0;
        if (flags & TMAP_FLAG_TILED) {
            if ((Tmap.bp->w == 16) && (Tmap.bp->h == 16))
                can_tile = 1;
            if ((Tmap.bp->w == 32) && (Tmap.bp->h == 32))
                can_tile = 1;
            if ((Tmap.bp->w == 64) && (Tmap.bp->h == 64))
                can_tile = 1;
            if ((Tmap.bp->w == 128) && (Tmap.bp->h == 128))
                can_tile = 1;
            if ((Tmap.bp->w == 256) && (Tmap.bp->h == 256))
                can_tile = 1;

            if (!can_tile) {
                was_tiled = 1;
                flags &= (~TMAP_FLAG_TILED);
            }
        }

        float max_u = i2fl(Tmap.bp->w) - 0.5f;
        float max_v = i2fl(Tmap.bp->h) - 0.5f;

        for (i = 0; i < nverts; i++) {
            ulist[i] = verts[i]->u * Tmap.bp->w;
            vlist[i] = verts[i]->v * Tmap.bp->h;

            if (!(flags & TMAP_FLAG_TILED)) {
                if (ulist[i] < 1.0f)
                    ulist[i] = 1.0f;
                if (vlist[i] < 1.0f)
                    vlist[i] = 1.0f;
                if (ulist[i] > max_u)
                    ulist[i] = max_u;
                if (vlist[i] > max_v)
                    vlist[i] = max_v;
            }

            // Multiply all u,v's by sw for perspective correction
            if (flags & TMAP_FLAG_CORRECT) {
                ulist[i] *= verts[i]->sw;
                vlist[i] *= verts[i]->sw;
            }
        }

        Tmap.pixptr = (unsigned char *)Tmap.bp->data;
        Tmap.src_offset = Tmap.bp->rowsize;
    }

    // Find the topmost vertex
    //top = -1;			// Initialize to dummy value to avoid compiler warning
    //ymin = 0.0f;		// Initialize to dummy value to avoid compiler warning
    //	Instead of initializing to avoid compiler warnings, set to first value outside loop and remove (i==0)
    //	comparison, which otherwise happens nverts times.  MK, 3/20/98 (was tracing code figuring out my shield effect bug...)
    ymin = verts[0]->sy;
    top = 0;
    for (i = 1; i < nverts; i++) {
        if (verts[i]->sy < ymin) {
            ymin = verts[i]->sy;
            top = i;
        }
    }

    li = ri = top;
    rem = nverts;
    y = fl_round_2048(ymin); //(int)floor(ymin + 0.5);
    ly = ry = y - 1;

    gr_lock();
    Tmap.pScreenBits = (uintptr_t)gr_screen.offscreen_buffer_base;

    while (rem > 0) {
        while (ly <= y && rem > 0) { // Advance left edge?
            float dy, frac, recip;
            rem--;
            i = li - 1;
            if (i < 0)
                i = nverts - 1;
            ly = fl_round_2048(verts[i]->sy); //(int)floor(verts[i]->sy+0.5);

            dy = verts[i]->sy - verts[li]->sy;
            if (dy == 0.0f)
                dy = 1.0f;

            frac = y + 0.5f - verts[li]->sy;
            recip = 1.0f / dy;

            Tmap.dl.sx = (verts[i]->sx - verts[li]->sx) * recip;
            Tmap.l.sx = verts[li]->sx + Tmap.dl.sx * frac;

            if (flags & TMAP_FLAG_TEXTURED) {
                Tmap.dl.u = (ulist[i] - ulist[li]) * recip;
                Tmap.l.u = ulist[li] + Tmap.dl.u * frac;
                Tmap.dl.v = (vlist[i] - vlist[li]) * recip;
                Tmap.l.v = vlist[li] + Tmap.dl.v * frac;
            }

            if ((flags & TMAP_FLAG_CORRECT) || gr_zbuffering) {
                Tmap.dl.sw = (verts[i]->sw - verts[li]->sw) * recip;
                Tmap.l.sw = verts[li]->sw + Tmap.dl.sw * frac;
            }

            if (flags & TMAP_FLAG_GOURAUD) {
                if (flags & TMAP_FLAG_RAMP) {
                    Tmap.dl.b = (llist[i] - llist[li]) * recip;
                    Tmap.l.b = llist[li] + Tmap.dl.b * frac;
                }
            }

            li = i;
        }
        while (ry <= y && rem > 0) { // Advance right edge?
            float dy, frac, recip;
            rem--;
            i = ri + 1;
            if (i >= nverts)
                i = 0;
            ry = fl_round_2048(verts[i]->sy); //(int)floor(verts[i]->sy+0.5);

            dy = verts[i]->sy - verts[ri]->sy;
            if (dy == 0.0f)
                dy = 1.0f;

            frac = y + 0.5f - verts[ri]->sy;
            recip = 1.0f / dy;

            Tmap.dr.sx = (verts[i]->sx - verts[ri]->sx) * recip;
            Tmap.r.sx = verts[ri]->sx + Tmap.dr.sx * frac;

            if (flags & TMAP_FLAG_TEXTURED) {
                Tmap.dr.u = (ulist[i] - ulist[ri]) * recip;
                Tmap.r.u = ulist[ri] + Tmap.dr.u * frac;
                Tmap.dr.v = (vlist[i] - vlist[ri]) * recip;
                Tmap.r.v = vlist[ri] + Tmap.dr.v * frac;
            }

            if ((flags & TMAP_FLAG_CORRECT) || gr_zbuffering) {
                Tmap.dr.sw = (verts[i]->sw - verts[ri]->sw) * recip;
                Tmap.r.sw = verts[ri]->sw + Tmap.dr.sw * frac;
            }

            if (flags & TMAP_FLAG_GOURAUD) {
                if (flags & TMAP_FLAG_RAMP) {
                    Tmap.dr.b = (llist[i] - llist[ri]) * recip;
                    Tmap.r.b = llist[ri] + Tmap.dr.b * frac;
                }
            }

            ri = i;
        }

        if (ly < ry)
            next_break = ly;
        else
            next_break = ry;

        for (; y < next_break; y++) {
            if ((y >= gr_screen.clip_top) && (y <= gr_screen.clip_bottom)) {
                int lx, rx;

                lx = fl_round_2048(Tmap.l.sx);
                if (lx < gr_screen.clip_left) {
                    Tmap.clipped_left = i2fl(gr_screen.clip_left) - Tmap.l.sx;
                    lx = gr_screen.clip_left;
                }
                else {
                    Tmap.clipped_left = 0.0f;
                }
                rx = fl_round_2048(Tmap.r.sx - 1.0f);

                if (rx > gr_screen.clip_right)
                    rx = gr_screen.clip_right;
                if (lx <= rx) {
                    float dx, recip; //frac;

                    dx = Tmap.r.sx - Tmap.l.sx;
                    if (dx == 0.0f)
                        dx = 1.0f;

                    //frac = lx + 0.5f - Tmap.l.sx;
                    recip = 1.0f / dx;

                    Tmap.y = y;
                    Tmap.rx = rx;
                    Tmap.lx = lx;
                    Tmap.loop_count = rx - lx + 1;
#ifndef NDEBUG
                    Tmap_npixels += Tmap.loop_count;
                    Tmap_nscanlines++;
#endif

                    if ((flags & TMAP_FLAG_CORRECT) || gr_zbuffering) {
                        Tmap.deltas.sw = (Tmap.r.sw - Tmap.l.sw) * recip;
                        Tmap.fl_dwdx_wide = Tmap.deltas.sw * 32.0f;
                    }

                    if (flags & TMAP_FLAG_TEXTURED) {
                        Tmap.deltas.u = (Tmap.r.u - Tmap.l.u) * recip;
                        Tmap.deltas.v = (Tmap.r.v - Tmap.l.v) * recip;

                        if (flags & TMAP_FLAG_CORRECT) {
                            Tmap.fl_dudx_wide = Tmap.deltas.u * 32.0f;
                            Tmap.fl_dvdx_wide = Tmap.deltas.v * 32.0f;
                        }
                        else {
                            Tmap.fx_u = tmap_ftol(
                                (Tmap.l.u + Tmap.clipped_left * Tmap.deltas.u) *
                                65536.0f);
                            Tmap.fx_v = tmap_ftol(
                                (Tmap.l.v + Tmap.clipped_left * Tmap.deltas.v) *
                                65536.0f);
                            Tmap.fx_du_dx = tmap_ftol(Tmap.deltas.u * 65536.0f);
                            Tmap.fx_dv_dx = tmap_ftol(Tmap.deltas.v * 65536.0f);
                        }
                    }

                    if (flags & TMAP_FLAG_GOURAUD) {
                        if (flags & TMAP_FLAG_RAMP) {
                            Tmap.deltas.b = (Tmap.r.b - Tmap.l.b) * recip;

                            Tmap.fx_l = tmap_ftol(Tmap.l.b * 32.0f * 65536.0f);
                            Tmap.fx_l_right = tmap_ftol(Tmap.r.b * 32.0f *
                                                        65536.0f);
                            Tmap.fx_dl_dx = tmap_ftol(Tmap.deltas.b * 32.0f *
                                                      65536.0f);

                            if (Tmap.fx_dl_dx < 0) {
                                Tmap.fx_dl_dx = -Tmap.fx_dl_dx;
                                Tmap.fx_l = (67 * F1_0) - Tmap.fx_l;
                                Tmap.fx_l_right = (67 * F1_0) - Tmap.fx_l_right;
                                //		Assert( Tmap.fx_l > 31*F1_0 );
                                //		Assert( Tmap.fx_l < 66*F1_0 );
                                //		Assert( Tmap.fx_dl_dx >= 0 );
                                //		Assert( Tmap.fx_dl_dx < 31*F1_0 );
                            }
                        }
                    }

                    if (gr_zbuffering) {
                        Tmap.fx_w = tmap_ftol(Tmap.l.sw * GR_Z_RANGE) +
                                    gr_zoffset;
                        Tmap.fx_dwdx = tmap_ftol(Tmap.deltas.sw * GR_Z_RANGE);
                    }

                    Tmap.dest_row_data = GR_SCREEN_PTR_SIZE(
                        gr_screen.bytes_per_pixel, Tmap.lx, Tmap.y);

                    (*tmap_scanline)();
                }
            }

            Tmap.l.sx += Tmap.dl.sx;
            Tmap.r.sx += Tmap.dr.sx;

            if (flags & TMAP_FLAG_TEXTURED) {
                Tmap.l.u += Tmap.dl.u;
                Tmap.l.v += Tmap.dl.v;

                Tmap.r.u += Tmap.dr.u;
                Tmap.r.v += Tmap.dr.v;
            }

            if ((flags & TMAP_FLAG_CORRECT) || gr_zbuffering) {
                Tmap.l.sw += Tmap.dl.sw;
                Tmap.r.sw += Tmap.dr.sw;
            }

            if (flags & TMAP_FLAG_GOURAUD) {
                if (flags & TMAP_FLAG_RAMP) {
                    Tmap.l.b += Tmap.dl.b;
                    Tmap.r.b += Tmap.dr.b;
                }
            }
        }
    }

    gr_unlock();

    if (flags & TMAP_FLAG_TEXTURED) {
        bm_unlock(gr_screen.current_bitmap);
    }
}
