/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell
 * or otherwise commercially exploit the source or things you created based on the
 * source.
 *
*/

#include <render/3d.hh>
#include <graphics/2d.hh>
#include <graphics/grinternal.hh>
#include <graphics/tmapper.hh>
#include <graphics/tmapscanline.hh>
#include <math/floating.hh>
#include <palman/palman.hh>
#include <math/fix.hh>

// Needed to keep warning 4725 to stay away.  See PsTypes.h for details why.
void
disable_warning_4725_stub_tst256()
{ }

// 256x256 tiles: 8 integer bits per coordinate.  The asm that lived
// here was the tile-size specialization of the generic C mapper in
// tmapscanline.cpp.  Like the 128x128 file (and unlike 16/32/64), this
// file's asm did not recompute the per-scanline setup (fx_l,
// fl_*_wide, fx_w); it used whatever the outer loop or a previous call
// left in Tmap, hence do_setup = 0.

void
tmapscan_pln8_zbuffered_tiled_256x256()
{
    tmapscan_pln8_zbuffered_tiled_g(8, 0);
}

void
tmapscan_pln8_tiled_256x256()
{
    tmapscan_pln8_tiled_g(8, 0);
}

void
tmapscan_lnn8_tiled_256x256()
{
    if (Tmap.src_offset != 256) {
        Int3(); // This only works on 256 wide textures!
        return;
    }

    int i;

    ubyte *src = (ubyte *)Tmap.pixptr;
    ubyte *dst = (ubyte *)Tmap.dest_row_data;

    for (i = 0; i < Tmap.loop_count; i++) {
        int u, v;
        u = f2i(Tmap.fx_u) & 255;
        v = f2i(Tmap.fx_v) & 255;

        ubyte c = src[u + v * Tmap.src_offset];
        *dst = c;
        dst++;

        Tmap.fx_u += Tmap.fx_du_dx;
        Tmap.fx_v += Tmap.fx_dv_dx;
    }
}

int Rand_value = 1;

// used only for subpsace effect

#define MASK 0x00ff00ff
//#define MASK 0x0

// The subspace mappers are perspective-correct, unlit, always-write
// walkers over a 256x256 tile.  Inside a span u and v travel packed
// [ u 8.8 | v 8.8 ] in one 32-bit register (see the generic tiled
// mapper in tmapscanline.cpp for the scheme); the texel index is
// (v_int << 8) | u_int.

// not used, but cool
//
// Same as tmapscan_pnn8_tiled_256x256_subspace but each pixel jitters
// the packed u/v with an LFSR ('r' repeats every 2^32 steps): the
// random word is masked to the two 8-bit fraction fields and added to
// the packed coordinate before the texel index is extracted.  Carries
// from the jittered fractions ripple into the integer fields exactly
// like the asm's 32-bit add.
void
tmapscan_pnn8_tiled_256x256_subspace_dithered()
{
    if (Tmap.src_offset != 256) {
        Int3(); // This only works on 256 wide textures!
        return;
    }

    uint r = (uint)Rand_value; // ebx

    ubyte *dptr = (ubyte *)Tmap.dest_row_data; // edi
    ubyte *src = (ubyte *)Tmap.pixptr; // esi
    int width = Tmap.loop_count; // ecx

    int subdivisions = width >> 5; // width / subdivision length
    int leftover = width & 31; // width mod subdivision length
    if (leftover == 0) { // no leftover? special case last span
        subdivisions--;
        leftover = 32;
    }
    Tmap.Subdivisions = (uint)subdivisions;
    Tmap.WidthModLength = leftover;

    // calculate ULeft and VLeft
    float v_over_z = Tmap.l.v;
    float u_over_z = Tmap.l.u;
    float one_over_z = Tmap.l.sw;
    float z_left = 1.0f / one_over_z;
    float v_left = z_left * v_over_z;
    float u_left = z_left * u_over_z;

    // calculate right side OverZ terms and coords
    one_over_z += Tmap.fl_dwdx_wide;
    u_over_z += Tmap.fl_dudx_wide;
    v_over_z += Tmap.fl_dvdx_wide;

    float z_right = 1.0f / one_over_z;
    float v_right = z_right * v_over_z;
    float u_right = z_right * u_over_z;

    uint uv = 0, duv = 0; // ecx, edx: [ u 8.8 | v 8.8 ]

    if (subdivisions > 0) {
        do { // SpanLoop
            // convert left side coords to 16.16
            Tmap.UFixed = (uint)lrintf(u_left * Tmap.FixedScale);
            Tmap.VFixed = (uint)lrintf(v_left * Tmap.FixedScale);

            // calculate deltas; FixedScale8 is 2^16/32
            Tmap.DeltaV = (uint)lrintf((v_right - v_left) * Tmap.FixedScale8);
            Tmap.DeltaU = (uint)lrintf((u_right - u_left) * Tmap.FixedScale8);

            // increment terms for next span; right terms become left terms
            v_over_z += Tmap.fl_dvdx_wide;
            one_over_z += Tmap.fl_dwdx_wide;
            u_over_z += Tmap.fl_dudx_wide;

            // This divide happened while the pixel span was drawn.
            z_right = 1.0f / one_over_z;

            // make EDX = DV:DU and ECX = V:U in 8.8:8.8
            duv = (((uint)Tmap.DeltaU << 8) & 0xffff0000u) |
                  (((uint)Tmap.DeltaV >> 8) & 0xffffu);
            uv = (((uint)Tmap.UFixed << 8) & 0xffff0000u) |
                 (((uint)Tmap.VFixed >> 8) & 0xffffu);

            for (Tmap.InnerLooper = 32; Tmap.InnerLooper > 0;
                 Tmap.InnerLooper--) {
                uint t = r >> 1; // shr eax,1
                if (r & 1) // jnc L*
                    t ^= 0xA3000000u; // makes 'r' take 2^32 iterations to repeat
                r = t;
                uint jittered = (t & MASK) + uv; // jitter the fraction fields
                uv += duv;
                uint u_int = jittered >> 24;
                uint v_int = (jittered & 0xffffu) >> 8;
                *dptr++ = src[(v_int << 8) + u_int]; // (V*256)+U
            }

            // the fdiv is done, finish right
            v_left = v_right;
            u_left = u_right;
            v_right = z_right * v_over_z;
            u_right = z_right * u_over_z;
        } while (--Tmap.Subdivisions > 0);
    }

    // HandleLeftoverPixels
    if (Tmap.WidthModLength != 0) {
        // convert left side coords
        Tmap.UFixed = (uint)lrintf(u_left * Tmap.FixedScale);
        Tmap.VFixed = (uint)lrintf(v_left * Tmap.FixedScale);

        if (--Tmap.WidthModLength > 0) {
            // calculate right edge coordinates: r -> R+1
            v_over_z = Tmap.r.v - Tmap.deltas.v;
            u_over_z = Tmap.r.u - Tmap.deltas.u;
            one_over_z = Tmap.r.sw - Tmap.deltas.sw;

            z_right = Tmap.One / one_over_z;
            u_right = u_over_z * z_right;
            v_right = v_over_z * z_right;

            Tmap.DeltaV = (uint)lrintf((v_right - v_left) /
                                       (float)Tmap.WidthModLength *
                                       Tmap.FixedScale);
            Tmap.DeltaU = (uint)lrintf((u_right - u_left) /
                                       (float)Tmap.WidthModLength *
                                       Tmap.FixedScale);
        }

        // OnePixelSpan
        duv = (((uint)Tmap.DeltaU << 8) & 0xffff0000u) |
              (((uint)Tmap.DeltaV >> 8) & 0xffffu);
        uv = (((uint)Tmap.UFixed << 8) & 0xffff0000u) |
             (((uint)Tmap.VFixed >> 8) & 0xffffu);

        int n = ++Tmap.WidthModLength;
        int pairs = n >> 1;
        if (pairs != 0) {
            Tmap.WidthModLength = pairs;
            do { // NextPixel drew pixel pairs
                for (int i = 0; i < 2; i++) {
                    uint t = r >> 1;
                    if (r & 1)
                        t ^= 0xA3000000u;
                    r = t;
                    uint jittered = (t & MASK) + uv;
                    uv += duv;
                    uint u_int = jittered >> 24;
                    uint v_int = (jittered & 0xffffu) >> 8;
                    *dptr++ = src[(v_int << 8) + u_int];
                }
            } while (--Tmap.WidthModLength > 0);
        }
        if (n & 1) {
            // one_more_pix
            uint t = r >> 1;
            if (r & 1)
                t ^= 0xA3000000u;
            r = t;
            uint jittered = (t & MASK) + uv;
            uint u_int = jittered >> 24;
            uint v_int = (jittered & 0xffffu) >> 8;
            *dptr = src[(v_int << 8) + u_int];
        }
    }

    Rand_value = (int)r;
}

void
tmapscan_pnn8_tiled_256x256_subspace()
{
    if (Tmap.src_offset != 256) {
        Int3(); // This only works on 256 wide textures!
        return;
    }

    ubyte *dptr = (ubyte *)Tmap.dest_row_data; // edi
    ubyte *src = (ubyte *)Tmap.pixptr; // esi
    int width = Tmap.loop_count; // ecx

    int subdivisions = width >> 5; // width / subdivision length
    int leftover = width & 31; // width mod subdivision length
    if (leftover == 0) { // no leftover? special case last span
        subdivisions--;
        leftover = 32;
    }
    Tmap.Subdivisions = (uint)subdivisions;
    Tmap.WidthModLength = leftover;

    // calculate ULeft and VLeft
    float v_over_z = Tmap.l.v;
    float u_over_z = Tmap.l.u;
    float one_over_z = Tmap.l.sw;
    float z_left = 1.0f / one_over_z;
    float v_left = z_left * v_over_z;
    float u_left = z_left * u_over_z;

    // calculate right side OverZ terms and coords
    one_over_z += Tmap.fl_dwdx_wide;
    u_over_z += Tmap.fl_dudx_wide;
    v_over_z += Tmap.fl_dvdx_wide;

    float z_right = 1.0f / one_over_z;
    float v_right = z_right * v_over_z;
    float u_right = z_right * u_over_z;

    uint uv = 0, duv = 0; // ecx, edx: [ u 8.8 | v 8.8 ]

    if (subdivisions > 0) {
        do { // SpanLoop
            // convert left side coords to 16.16
            Tmap.UFixed = (uint)lrintf(u_left * Tmap.FixedScale);
            Tmap.VFixed = (uint)lrintf(v_left * Tmap.FixedScale);

            // calculate deltas; FixedScale8 is 2^16/32
            Tmap.DeltaV = (uint)lrintf((v_right - v_left) * Tmap.FixedScale8);
            Tmap.DeltaU = (uint)lrintf((u_right - u_left) * Tmap.FixedScale8);

            // increment terms for next span; right terms become left terms
            v_over_z += Tmap.fl_dvdx_wide;
            one_over_z += Tmap.fl_dwdx_wide;
            u_over_z += Tmap.fl_dudx_wide;

            // This divide happened while the pixel span was drawn.
            z_right = 1.0f / one_over_z;

            // make EDX = DV:DU and ECX = V:U in 8.8:8.8
            duv = (((uint)Tmap.DeltaU << 8) & 0xffff0000u) |
                  (((uint)Tmap.DeltaV >> 8) & 0xffffu);
            uv = (((uint)Tmap.UFixed << 8) & 0xffff0000u) |
                 (((uint)Tmap.VFixed >> 8) & 0xffffu);

            for (Tmap.InnerLooper = 32; Tmap.InnerLooper > 0;
                 Tmap.InnerLooper--) {
                uint u_int = uv >> 24;
                uint v_int = (uv & 0xffffu) >> 8;
                *dptr++ = src[(v_int << 8) + u_int]; // (V*256)+U
                uv += duv;
            }

            // the fdiv is done, finish right
            v_left = v_right;
            u_left = u_right;
            v_right = z_right * v_over_z;
            u_right = z_right * u_over_z;
        } while (--Tmap.Subdivisions > 0);
    }

    // HandleLeftoverPixels
    if (Tmap.WidthModLength == 0)
        return;

    // convert left side coords
    Tmap.UFixed = (uint)lrintf(u_left * Tmap.FixedScale);
    Tmap.VFixed = (uint)lrintf(v_left * Tmap.FixedScale);

    if (--Tmap.WidthModLength > 0) {
        // calculate right edge coordinates: r -> R+1
        v_over_z = Tmap.r.v - Tmap.deltas.v;
        u_over_z = Tmap.r.u - Tmap.deltas.u;
        one_over_z = Tmap.r.sw - Tmap.deltas.sw;

        z_right = Tmap.One / one_over_z;
        u_right = u_over_z * z_right;
        v_right = v_over_z * z_right;

        Tmap.DeltaV = (uint)lrintf((v_right - v_left) /
                                   (float)Tmap.WidthModLength * Tmap.FixedScale);
        Tmap.DeltaU = (uint)lrintf((u_right - u_left) /
                                   (float)Tmap.WidthModLength * Tmap.FixedScale);
    }

    // OnePixelSpan
    duv = (((uint)Tmap.DeltaU << 8) & 0xffff0000u) |
          (((uint)Tmap.DeltaV >> 8) & 0xffffu);
    uv = (((uint)Tmap.UFixed << 8) & 0xffff0000u) |
         (((uint)Tmap.VFixed >> 8) & 0xffffu);

    int n = ++Tmap.WidthModLength;
    int pairs = n >> 1;
    if (pairs != 0) {
        Tmap.WidthModLength = pairs;
        do { // NextPixel drew pixel pairs
            for (int i = 0; i < 2; i++) {
                uint u_int = uv >> 24;
                uint v_int = (uv & 0xffffu) >> 8;
                *dptr++ = src[(v_int << 8) + u_int];
                uv += duv;
            }
        } while (--Tmap.WidthModLength > 0);
    }
    if (n & 1) {
        // one_more_pix
        uint u_int = uv >> 24;
        uint v_int = (uv & 0xffffu) >> 8;
        *dptr = src[(v_int << 8) + u_int];
    }
}
