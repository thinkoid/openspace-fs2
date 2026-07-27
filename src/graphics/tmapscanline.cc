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
#include <io/key.hh>

// Needed to keep warning 4725 to stay away.  See PsTypes.h for details why.
void
disable_warning_4725_stub_ts32()
{ }

extern void tmapscan_pln8_tiled_256x256();
extern void tmapscan_pln8_tiled_128x128();
extern void tmapscan_pln8_tiled_64x64();
extern void tmapscan_pln8_tiled_32x32();
extern void tmapscan_pln8_tiled_16x16();

void
tmapscan_pln8_tiled()
{
    if ((Tmap.bp->w == 256) && (Tmap.bp->h == 256)) {
        tmapscan_pln8_tiled_256x256();
    }
    else if ((Tmap.bp->w == 128) && (Tmap.bp->h == 128)) {
        tmapscan_pln8_tiled_128x128();
    }
    else if ((Tmap.bp->w == 64) && (Tmap.bp->h == 64)) {
        tmapscan_pln8_tiled_64x64();
    }
    else if ((Tmap.bp->w == 32) && (Tmap.bp->h == 32)) {
        tmapscan_pln8_tiled_32x32();
    }
    else if ((Tmap.bp->w == 16) && (Tmap.bp->h == 16)) {
        tmapscan_pln8_tiled_16x16();
    }
    else {
        // argh! write another texure mapper!
        tmapscan_pln8();
    }
}

void
tmapscan_write_z()
{
    int i;
    ubyte *dptr;
    uint w, dw;

    dptr = (ubyte *)Tmap.dest_row_data;

    w = Tmap.fx_w;
    dw = Tmap.fx_dwdx;

    uint *zbuf = &gr_zbuffer[(uintptr_t)dptr - Tmap.pScreenBits];

    for (i = 0; i < Tmap.loop_count; i++) {
        *zbuf = w;
        zbuf++;
        w += dw;
    }
}

void
tmapscan_flat_gouraud_zbuffered()
{
    int i;
    ubyte *dptr, c;
    fix l, dl;
    uint w, dw;

    dptr = (ubyte *)Tmap.dest_row_data;
    c = gr_screen.current_color.raw8;

    w = Tmap.fx_w;
    dw = Tmap.fx_dwdx;

    l = Tmap.fx_l;
    dl = Tmap.fx_dl_dx;

    uint *zbuf = &gr_zbuffer[(uintptr_t)dptr - Tmap.pScreenBits];

    for (i = 0; i < Tmap.loop_count; i++) {
        if (w > *zbuf) {
            *zbuf = w;
            *dptr = gr_fade_table[(f2i(l) << 8) + c];
        }
        zbuf++;
        w += dw;
        l += dl;
        dptr++;
    }
}

// ADAM: Change Nebula colors here:
#define NEBULA_COLORS 20

void
tmapscan_nebula8()
{
    ubyte *dptr;
    int l1, l2, dldx;

    dptr = (ubyte *)Tmap.dest_row_data;

    float max_neb_color = i2fl(NEBULA_COLORS - 1);

    l1 = (int)(Tmap.l.b * max_neb_color * 256.0f);
    l2 = l1 + 256 / 2; // dithering
    dldx = (int)(Tmap.deltas.b * max_neb_color * 2.0f * 256.0f);

#ifdef USE_INLINE_ASM
    //         memset( dptr, 31, Tmap.loop_count );
    _asm push eax _asm push ebx _asm push ecx _asm push edx _asm push edi

        // eax - l1
        // ebx - l2
        // ecx - count
        // edx - dldx
        // edi - dest
        _asm mov eax,
        l1 _asm mov ebx, l2 _asm mov edx, dldx _asm mov edi,
        dptr

        _asm mov ecx,
        Tmap.loop_count _asm shr ecx,
        1 _asm jz DoFinal _asm pushf

            Next2Pixels
        : _asm mov[edi], ah _asm add eax,
          edx

          _asm mov[edi + 1],
          bh _asm add ebx,
          edx

          _asm add edi,
          2 _asm dec ecx _asm jnz Next2Pixels

          _asm popf DoFinal
        : _asm jnc NotDoFinal _asm mov[edi], ah NotDoFinal
        :

        _asm pop edi _asm pop edx _asm pop ecx _asm pop ebx _asm pop eax

#else
    int i;
    if (Tmap.loop_count > 1) {
        for (i = 0; i < Tmap.loop_count / 2; i++) {
            dptr[0] = (ubyte)((l1 & 0xFF00) >> 8);
            l1 += dldx;
            dptr[1] = (ubyte)((l2 & 0xFF00) >> 8);
            l2 += dldx;
            dptr += 2;
        }
    }
    if (Tmap.loop_count & 1) {
        dptr[0] = (ubyte)((l1 & 0xFF00) >> 8);
        dptr++;
    }
#endif
}

void
tmapscan_flat_gouraud()
{
    if (gr_zbuffering) {
        switch (gr_zbuffering_mode) {
        case GR_ZBUFF_NONE:
            break;
        case GR_ZBUFF_FULL: // both
            tmapscan_flat_gouraud_zbuffered();
            return;
        case GR_ZBUFF_WRITE: // write only
            tmapscan_flat_gouraud_zbuffered();
            break;
        case GR_ZBUFF_READ: // read only
            tmapscan_flat_gouraud_zbuffered();
            return;
        }
    }

    /* HARDWARE_ONLY
   if ( Current_alphacolor )  {
      ubyte *lookup = &Current_alphacolor->table.lookup[0][0];

      int i;
      ubyte * dptr;
      fix l, dl;
      
      dptr = (ubyte *)Tmap.dest_row_data;

      l = Tmap.fx_l;
      dl = Tmap.fx_dl_dx;
      
      for (i=0; i<Tmap.loop_count; i++ )  {
         *dptr = lookup[f2i(l*16)*256+*dptr];
         l+=dl;
         dptr++;
      }

   } else {
   */
    int i;
    ubyte *dptr, c;
    fix l, dl;

    dptr = (ubyte *)Tmap.dest_row_data;
    c = gr_screen.current_color.raw8;

    l = Tmap.fx_l;
    dl = Tmap.fx_dl_dx;

    for (i = 0; i < Tmap.loop_count; i++) {
        *dptr = gr_fade_table[f2i(l * 32) * 256 + c];
        l += dl;
        dptr++;
    }
}

void
tmapscan_flat8_zbuffered()
{
    int i;
    ubyte *dptr, c;

    dptr = (ubyte *)Tmap.dest_row_data;
    c = gr_screen.current_color.raw8;

    for (i = 0; i < Tmap.loop_count; i++) {
        int tmp = (int)((uintptr_t)dptr - Tmap.pScreenBits);
        if (Tmap.fx_w > (int)gr_zbuffer[tmp]) {
            gr_zbuffer[tmp] = Tmap.fx_w;
            *dptr = c;
        }
        Tmap.fx_w += Tmap.fx_dwdx;
        dptr++;
    }
}

void
tmapscan_flat8()
{
    if (gr_zbuffering) {
        switch (gr_zbuffering_mode) {
        case GR_ZBUFF_NONE:
            break;
        case GR_ZBUFF_FULL: // both
            tmapscan_flat8_zbuffered();
            return;
        case GR_ZBUFF_WRITE: // write only
            tmapscan_write_z();
            break;
        case GR_ZBUFF_READ: // read only
            tmapscan_flat8_zbuffered();
            return;
        }
    }

    memset((ubyte *)Tmap.dest_row_data, gr_screen.current_color.raw8,
           Tmap.loop_count);
}

void tmapscan_pln8_zbuffered();

// C conversions of the retail MSVC inline-asm scanline mappers.  The
// register roles survive as variable names:
//
//    edi -> dptr      destination pixel pointer
//    esi -> tex       texture texel pointer
//    ebx -> u_frac    u fraction 0.32; in the ramp-lit walkers the low
//                     word doubles as the 8.8 light level (the asm kept
//                     both packed in EBX and let the light carry ripple
//                     into the u fraction -- the 32-bit adds below wrap
//                     the same way)
//    ecx -> v_frac    v fraction 0.32
//    edx -> du_frac   u fraction step (low word = 8.8 light step)
//
// Each pixel the fractions advance and the texture pointer moves by a
// whole-texel step selected by the v-fraction carry, plus one texel on
// the u-fraction carry:
//
//    add  ecx,Tmap.DeltaVFrac         // increment v fraction
//    sbb  ebp,ebp                     // get -1 if carry
//    add  ebx,edx                     // increment u fraction
//    adc  esi,Tmap.uv_delta[4*ebp+4]  // add in step ints & carries
//
// The perspective mappers ran the FPU in 24-bit precision mode; plain
// float arithmetic below rounds identically.  fistp rounded to nearest,
// hence lrintf.
static inline ubyte *
tmap_uv_step(ubyte *tex, uint *v_frac, uint *u_frac, uint dv_frac, uint du_frac)
{
    *v_frac += dv_frac;
    int v_carry = (*v_frac < dv_frac); // sbb ebp,ebp
    *u_frac += du_frac;
    int u_carry = (*u_frac < du_frac); // carry into the adc
    return tex + (int)Tmap.uv_delta[v_carry ? 0 : 1] + u_carry;
}

// "setup delta values": split a 16.16 u/v step into the fraction steps
// and the two whole-texel steps (without and with the v carry).
static void
tmap_setup_uv_deltas(int du, int dv)
{
    Tmap.DeltaVFrac = (uint)dv << 16; // v frac step
    Tmap.DeltaUFrac = (uint)du << 16; // u frac step
    Tmap.uv_delta[1] = (uint)((dv >> 16) * Tmap.src_offset +
                              (du >> 16)); // whole step in non-v-carry slot
    Tmap.uv_delta[0] = Tmap.uv_delta[1] +
                       (uint)Tmap.src_offset; // whole step + v carry
}

// "setup initial coordinates": texel address and 0.32 fractions from
// 16.16 u and v.
static ubyte *
tmap_uv_start(int u, int v, uint *u_frac, uint *v_frac)
{
    *u_frac = (uint)u << 16; // get fractional part
    *v_frac = (uint)v << 16;
    return Tmap.pixptr + (u >> 16) + (v >> 16) * Tmap.src_offset; // calc address
}

// tmapscan_pln8_ppro / tmapscan_pln8_pentium
//
// Perspective-correct, ramp-lit, non-tiled 8-bpp mapper.  The scanline
// is cut into 32-pixel spans; u/z, v/z and 1/z step on the FPU across
// span boundaries and the walk inside a span is affine 16.16 fixed
// point.  The Pentium and PPro asm variants differed only in
// instruction scheduling -- the pixel results are identical -- so both
// entry points share this body.
//
// Note the pipelined texel read: the asm preread the first texel, and
// inside the loop read the next pixel's texel *before* stepping the
// texture pointer, so every pixel after the first in a span reuses the
// previous pixel's texel.  That schedule (and its one-texel lag) is
// reproduced exactly.
static void
tmapscan_pln8_c()
{
    ubyte *dptr = (ubyte *)Tmap.dest_row_data; // edi
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

    uint u_frac = 0, v_frac = 0; // ebx, ecx
    uint du_frac = 0; // edx
    ubyte *tex = NULL; // esi
    ubyte texel = 0; // al

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

            tmap_setup_uv_deltas((int)Tmap.DeltaU, (int)Tmap.DeltaV);
            tex = tmap_uv_start((int)Tmap.UFixed, (int)Tmap.VFixed, &u_frac,
                                &v_frac);

            // set up affine registers: low word of EBX carries the 8.8
            // light, low word of EDX its per-pixel step
            uint light = (uint)(Tmap.fx_l >> 8) & 0xffff; // bx
            Tmap.fx_l += Tmap.fx_dl_dx << 5; // walk the light a whole span
            ushort lstep = (ushort)((ushort)(uint)(Tmap.fx_l >> 8) -
                                    (ushort)light);
            lstep = (ushort)(lstep >> 5); // per-pixel light step
            u_frac = (u_frac & 0xffff0000u) | light;
            du_frac = (Tmap.DeltaUFrac & 0xffff0000u) | lstep;

            // This divide happened while the pixel span was drawn.
            z_right = 1.0f / one_over_z;

            texel = *tex; // get texture pixel 0
            for (Tmap.InnerLooper = 32; Tmap.InnerLooper > 0;
                 Tmap.InnerLooper--) {
                *dptr++ =
                    gr_fade_table[(u_frac & 0xff00) + texel]; // get shaded pixel
                texel = *tex; // read for the next pixel *before* stepping
                tex = tmap_uv_step(tex, &v_frac, &u_frac, Tmap.DeltaVFrac,
                                   du_frac);
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

        tmap_setup_uv_deltas((int)Tmap.DeltaU, (int)Tmap.DeltaV);
    }

    // OnePixelSpan
    tex = tmap_uv_start((int)Tmap.UFixed, (int)Tmap.VFixed, &u_frac, &v_frac);

    uint light = (uint)(Tmap.fx_l >> 8) & 0xffff;
    u_frac = (u_frac & 0xffff0000u) | light;
    du_frac = Tmap.DeltaUFrac;
    if (Tmap.WidthModLength > 1) { // NoDeltaLight otherwise
        du_frac = (du_frac & 0xffff0000u) | ((uint)(Tmap.fx_dl_dx >> 8) & 0xffff);
    }

    int n = ++Tmap.WidthModLength;
    int pairs = n >> 1;
    if (pairs != 0) {
        texel = *tex; // re-sync the pipelined read
        Tmap.WidthModLength = pairs;
        do { // NextPixel drew pixel pairs
            for (int i = 0; i < 2; i++) {
                *dptr++ = gr_fade_table[(u_frac & 0xff00) + texel];
                texel = *tex;
                tex = tmap_uv_step(tex, &v_frac, &u_frac, Tmap.DeltaVFrac,
                                   du_frac);
            }
        } while (--Tmap.WidthModLength > 0);
    }
    if (n & 1) {
        // one_more_pix: reads its texel afresh
        texel = *tex;
        *dptr = gr_fade_table[(u_frac & 0xff00) + texel];
    }
}

void
tmapscan_pln8_ppro()
{
    tmapscan_pln8_c();
}

void
tmapscan_pln8_pentium()
{
    tmapscan_pln8_c();
}

void
tmapscan_pln8()
{
    if (gr_zbuffering) {
        switch (gr_zbuffering_mode) {
        case GR_ZBUFF_NONE:
            break;
        case GR_ZBUFF_FULL: // both
            tmapscan_pln8_zbuffered();
            return;
        case GR_ZBUFF_WRITE: // write only
            tmapscan_write_z();
            break;
        case GR_ZBUFF_READ: // read only
            tmapscan_pln8_zbuffered();
            return;
        }
    }

    if (Gr_cpu > 5) {
        tmapscan_pln8_ppro();
    }
    else {
        tmapscan_pln8_pentium();
    }
}

// Linear (non-perspective) ramp-lit mapper.  u/v come in as 16.16 in
// fx_u/fx_v.  The light is re-synced from Tmap.fx_l every 4-pixel
// block: fx_l advances a whole block and the per-pixel step is the
// resulting 8.8 difference divided back down, all in 16-bit arithmetic
// like the asm's bx/bp.  Same pipelined (one-texel lag) read as
// tmapscan_pln8_c.
void
tmapscan_lln8()
{
    tmap_setup_uv_deltas((int)Tmap.fx_du_dx, (int)Tmap.fx_dv_dx);

    uint u_frac, v_frac;
    ubyte *tex = tmap_uv_start((int)Tmap.fx_u, (int)Tmap.fx_v, &u_frac, &v_frac);
    ubyte *dptr = (ubyte *)Tmap.dest_row_data; // edi
    uint du_frac = Tmap.DeltaUFrac; // edx
    ubyte texel = 0; // al

    int nblocks = Tmap.loop_count >> 2;
    if (nblocks != 0) {
        Tmap.num_big_steps = nblocks;
        Tmap.loop_count &= 3;

        texel = *tex; // get texture pixel 0
        do { // NextPixelBlock: re-sync the light each 4 pixels
            uint light = (uint)(Tmap.fx_l >> 8) & 0xffff; // bx
            Tmap.fx_l += Tmap.fx_dl_dx << 2; // walk the light a whole block
            ushort lstep = (ushort)((ushort)(uint)(Tmap.fx_l >> 8) -
                                    (ushort)light);
            lstep = (ushort)(lstep >> 2); // per-pixel light step
            u_frac = (u_frac & 0xffff0000u) | light;
            du_frac = (du_frac & 0xffff0000u) | lstep;

            for (int i = 0; i < 4; i++) {
                *dptr++ =
                    gr_fade_table[(u_frac & 0xff00) + texel]; // get shaded pixel
                texel = *tex; // read for the next pixel *before* stepping
                tex = tmap_uv_step(tex, &v_frac, &u_frac, Tmap.DeltaVFrac,
                                   du_frac);
            }
        } while (--Tmap.num_big_steps != 0);
    }

    // DoLeftOverPixels: light steps at fx_dl_dx per pixel, undivided
    {
        uint light = (uint)(Tmap.fx_l >> 8) & 0xffff;
        u_frac = (u_frac & 0xffff0000u) | light;
        du_frac = (du_frac & 0xffff0000u) | ((uint)(Tmap.fx_dl_dx >> 8) & 0xffff);
    }

    int nleft = Tmap.loop_count;
    if (nleft == 0)
        return;
    int pairs = nleft >> 1;
    if (pairs != 0) {
        texel = *tex; // re-sync the pipelined read
        Tmap.loop_count = pairs;
        do {
            for (int i = 0; i < 2; i++) {
                *dptr++ = gr_fade_table[(u_frac & 0xff00) + texel];
                texel = *tex;
                tex = tmap_uv_step(tex, &v_frac, &u_frac, Tmap.DeltaVFrac,
                                   du_frac);
            }
        } while (--Tmap.loop_count != 0);
    }
    if (nleft & 1) {
        // one_more_pix: reads its texel afresh
        texel = *tex;
        *dptr = gr_fade_table[(u_frac & 0xff00) + texel];
    }
}

// tmapscan_lna8_zbuffered_ppro / _pentium
//
// Linear alpha-blended mapper with z-buffer *test* only -- the retail
// asm had the z write commented out.  BlendLookup maps
// (texel<<8 | dest pixel) -> blended pixel.  The two CPU variants
// differed only in scheduling; both entry points share this body.
static void
tmapscan_lna8_zbuffered_c()
{
    tmap_setup_uv_deltas((int)Tmap.fx_du_dx, (int)Tmap.fx_dv_dx);

    uint u_frac, v_frac;
    ubyte *tex = tmap_uv_start((int)Tmap.fx_u, (int)Tmap.fx_v, &u_frac, &v_frac);
    ubyte *dptr = (ubyte *)Tmap.dest_row_data; // edi
    ubyte *blend = (ubyte *)Tmap.BlendLookup;

    int w = Tmap.fx_w; // ebp
    uint *zbuf = &gr_zbuffer[(uintptr_t)dptr - Tmap.pScreenBits]; // edx

    int nblocks = Tmap.loop_count >> 2;
    if (nblocks != 0) {
        Tmap.num_big_steps = nblocks;
        Tmap.loop_count &= 3;

        do { // NextPixelBlock
            for (int i = 0; i < 4; i++) {
                if (w > (int)*zbuf) { // z test; covered pixels skipped
                    *dptr = blend[((uint)*tex << 8) + *dptr]; // blend them
                }
                w += Tmap.fx_dwdx;
                tex = tmap_uv_step(tex, &v_frac, &u_frac, Tmap.DeltaVFrac,
                                   Tmap.DeltaUFrac);
                zbuf++;
                dptr++;
            }
        } while (--Tmap.num_big_steps != 0);
    }

    // DoLeftOverPixels
    int nleft = Tmap.loop_count;
    if (nleft == 0)
        return;
    int pairs = nleft >> 1;
    if (pairs != 0) {
        Tmap.loop_count = pairs;
        do {
            for (int i = 0; i < 2; i++) {
                if (w > (int)*zbuf) {
                    *dptr = blend[((uint)*tex << 8) + *dptr];
                }
                w += Tmap.fx_dwdx;
                tex = tmap_uv_step(tex, &v_frac, &u_frac, Tmap.DeltaVFrac,
                                   Tmap.DeltaUFrac);
                zbuf++;
                dptr++;
            }
        } while (--Tmap.loop_count != 0);
    }
    if (nleft & 1) {
        // one_more_pix
        if (w > (int)*zbuf) {
            *dptr = blend[((uint)*tex << 8) + *dptr];
        }
    }
}

void
tmapscan_lna8_zbuffered_ppro()
{
    tmapscan_lna8_zbuffered_c();
}

void
tmapscan_lna8_zbuffered_pentium()
{
    tmapscan_lna8_zbuffered_c();
}

void
tmapscan_lna8_zbuffered()
{
    if (Gr_cpu > 5) {
        tmapscan_lna8_zbuffered_ppro();
    }
    else {
        tmapscan_lna8_zbuffered_pentium();
    }
}

extern float Tmap_clipped_left;

void
tmapscan_lna8()
{
    if (gr_zbuffering) {
        switch (gr_zbuffering_mode) {
        case GR_ZBUFF_NONE:
            break;
        case GR_ZBUFF_FULL: // both
        case GR_ZBUFF_WRITE: // write only
        case GR_ZBUFF_READ: // read only
            tmapscan_lna8_zbuffered();
            return;
        }
    }

    // Linear alpha-blended mapper, no z-buffering.
    tmap_setup_uv_deltas((int)Tmap.fx_du_dx, (int)Tmap.fx_dv_dx);

    uint u_frac, v_frac;
    ubyte *tex = tmap_uv_start((int)Tmap.fx_u, (int)Tmap.fx_v, &u_frac, &v_frac);
    ubyte *dptr = (ubyte *)Tmap.dest_row_data; // edi
    ubyte *blend = (ubyte *)Tmap.BlendLookup;
    uint du_frac = Tmap.DeltaUFrac; // edx

    int nblocks = Tmap.loop_count >> 2;
    if (nblocks != 0) {
        Tmap.num_big_steps = nblocks;
        Tmap.loop_count &= 3;

        do { // NextPixelBlock
            for (int i = 0; i < 4; i++) {
                ubyte c = *tex; // get texture pixel
                *dptr = blend[((uint)c << 8) + *dptr]; // blend them
                tex = tmap_uv_step(tex, &v_frac, &u_frac, Tmap.DeltaVFrac,
                                   du_frac);
                dptr++;
            }
        } while (--Tmap.num_big_steps != 0);
    }

    // DoLeftOverPixels
    int nleft = Tmap.loop_count;
    if (nleft == 0)
        return;
    int pairs = nleft >> 1;
    if (pairs != 0) {
        Tmap.loop_count = pairs;
        do {
            for (int i = 0; i < 2; i++) {
                ubyte c = *tex;
                *dptr = blend[((uint)c << 8) + *dptr];
                tex = tmap_uv_step(tex, &v_frac, &u_frac, Tmap.DeltaVFrac,
                                   du_frac);
                dptr++;
            }
        } while (--Tmap.loop_count != 0);
    }
    if (nleft & 1) {
        // one_more_pix
        *dptr = blend[((uint)*tex << 8) + *dptr];
    }
}

// HACKED IN SYSTEM FOR DOING MODEL CACHING
int Tmap_scan_read = 0; // 0 = normal mapper, 1=read, 2=write

// HACKED IN SYSTEM FOR DOING MODEL CACHING
void
tmapscan_lnn8_read()
{
    Tmap.fx_u = fl2f(Tmap.l.u);
    Tmap.fx_v = fl2f(Tmap.l.v);
    Tmap.fx_du_dx = fl2f(Tmap.deltas.u);
    Tmap.fx_dv_dx = fl2f(Tmap.deltas.v);

    /*
   int i;

   ubyte * src = (ubyte *)Tmap.pixptr;
   ubyte * dst = (ubyte *)Tmap.dest_row_data;
   
   for (i=0; i<Tmap.loop_count; i++ )  {
      int u,v;
      u = f2i(Tmap.fx_u);
      v = f2i(Tmap.fx_v);
      
      src[u+v*Tmap.src_offset] = *dst++;
                  
      Tmap.fx_u += Tmap.fx_du_dx;
      Tmap.fx_v += Tmap.fx_dv_dx;
   }
*/

    // Reverse mapper: copies the screen back into the texture along the
    // same u/v walk (model caching).
    tmap_setup_uv_deltas((int)Tmap.fx_du_dx, (int)Tmap.fx_dv_dx);

    uint u_frac, v_frac;
    ubyte *tex = tmap_uv_start((int)Tmap.fx_u, (int)Tmap.fx_v, &u_frac, &v_frac);
    ubyte *dptr = (ubyte *)Tmap.dest_row_data; // edi
    uint du_frac = Tmap.DeltaUFrac; // edx

    int nblocks = Tmap.loop_count >> 2;
    if (nblocks != 0) {
        Tmap.num_big_steps = nblocks;
        Tmap.loop_count &= 3;
        do {
            for (int i = 0; i < 4; i++) {
                *tex = *dptr++; // screen pixel -> texture
                tex = tmap_uv_step(tex, &v_frac, &u_frac, Tmap.DeltaVFrac,
                                   du_frac);
            }
        } while (--Tmap.num_big_steps != 0);
    }

    int nleft = Tmap.loop_count;
    if (nleft == 0)
        return;
    int pairs = nleft >> 1;
    if (pairs != 0) {
        Tmap.loop_count = pairs;
        do {
            for (int i = 0; i < 2; i++) {
                *tex = *dptr++;
                tex = tmap_uv_step(tex, &v_frac, &u_frac, Tmap.DeltaVFrac,
                                   du_frac);
            }
        } while (--Tmap.loop_count != 0);
    }
    if (nleft & 1) {
        *tex = *dptr; // one_more_pix
    }
}

// HACKED IN SYSTEM FOR DOING MODEL CACHING
void
tmapscan_lnn8_write()
{
    Tmap.fx_u = fl2f(Tmap.l.u);
    Tmap.fx_v = fl2f(Tmap.l.v);
    Tmap.fx_du_dx = fl2f(Tmap.deltas.u);
    Tmap.fx_dv_dx = fl2f(Tmap.deltas.v);

    /*
   int i;

   ubyte * src = (ubyte *)Tmap.pixptr;
   ubyte * dst = (ubyte *)Tmap.dest_row_data;
   
   for (i=0; i<Tmap.loop_count; i++ )  {
      int u,v;
      u = f2i(Tmap.fx_u);
      v = f2i(Tmap.fx_v);

      ubyte c = src[u+v*Tmap.src_offset];
      if ( c != 0 )  {
         *dst = c;
      }
      dst++;
               
      Tmap.fx_u += Tmap.fx_du_dx;
      Tmap.fx_v += Tmap.fx_dv_dx;
   }
*/

    // Forward mapper for model caching: texel 255 is transparent.
    tmap_setup_uv_deltas((int)Tmap.fx_du_dx, (int)Tmap.fx_dv_dx);

    uint u_frac, v_frac;
    ubyte *tex = tmap_uv_start((int)Tmap.fx_u, (int)Tmap.fx_v, &u_frac, &v_frac);
    ubyte *dptr = (ubyte *)Tmap.dest_row_data; // edi
    uint du_frac = Tmap.DeltaUFrac; // edx

    int nblocks = Tmap.loop_count >> 2;
    if (nblocks != 0) {
        Tmap.num_big_steps = nblocks;
        Tmap.loop_count &= 3;
        do {
            for (int i = 0; i < 4; i++) {
                ubyte c = *tex; // get texture pixel
                tex = tmap_uv_step(tex, &v_frac, &u_frac, Tmap.DeltaVFrac,
                                   du_frac);
                if (c != 255)
                    *dptr = c; // store pixel
                dptr++;
            }
        } while (--Tmap.num_big_steps != 0);
    }

    int nleft = Tmap.loop_count;
    if (nleft == 0)
        return;
    int pairs = nleft >> 1;
    if (pairs != 0) {
        Tmap.loop_count = pairs;
        do {
            for (int i = 0; i < 2; i++) {
                ubyte c = *tex;
                tex = tmap_uv_step(tex, &v_frac, &u_frac, Tmap.DeltaVFrac,
                                   du_frac);
                if (c != 255)
                    *dptr = c;
                dptr++;
            }
        } while (--Tmap.loop_count != 0);
    }
    if (nleft & 1) {
        ubyte c = *tex; // one_more_pix
        if (c != 255)
            *dptr = c;
    }
}

void
tmapscan_lnn8()
{
    // HACKED IN SYSTEM FOR DOING MODEL CACHING
    if (Tmap_scan_read == 1) {
        tmapscan_lnn8_read();
        return;
    }
    else if (Tmap_scan_read == 2) {
        tmapscan_lnn8_write();
        //tmapscan_lnt8();
        return;
    }

    if (gr_screen.current_alphablend_mode == GR_ALPHABLEND_FILTER) {
        tmapscan_lna8();
        return;
    }

    // Linear unlit opaque mapper.  Pipelined texel read as in
    // tmapscan_pln8_c: the in-loop read happens before the step, so
    // every pixel after the first reuses the previous pixel's texel;
    // the leftover section re-syncs the read.
    tmap_setup_uv_deltas((int)Tmap.fx_du_dx, (int)Tmap.fx_dv_dx);

    uint u_frac, v_frac;
    ubyte *tex = tmap_uv_start((int)Tmap.fx_u, (int)Tmap.fx_v, &u_frac, &v_frac);
    ubyte *dptr = (ubyte *)Tmap.dest_row_data; // edi
    uint du_frac = Tmap.DeltaUFrac; // edx

    ubyte texel = *tex; // get texture pixel 0

    int nblocks = Tmap.loop_count >> 2;
    if (nblocks != 0) {
        Tmap.num_big_steps = nblocks;
        Tmap.loop_count &= 3;
        do {
            for (int i = 0; i < 4; i++) {
                *dptr++ = texel; // store pixel
                texel = *tex; // read *before* stepping
                tex = tmap_uv_step(tex, &v_frac, &u_frac, Tmap.DeltaVFrac,
                                   du_frac);
            }
        } while (--Tmap.num_big_steps != 0);
    }

    int nleft = Tmap.loop_count;
    if (nleft == 0)
        return;
    int pairs = nleft >> 1;
    if (pairs != 0) {
        texel = *tex; // re-sync the pipelined read
        Tmap.loop_count = pairs;
        do {
            for (int i = 0; i < 2; i++) {
                *dptr++ = texel;
                texel = *tex;
                tex = tmap_uv_step(tex, &v_frac, &u_frac, Tmap.DeltaVFrac,
                                   du_frac);
            }
        } while (--Tmap.loop_count != 0);
    }
    if (nleft & 1) {
        *dptr = texel; // one_more_pix: no re-read
    }
}

void
tmapscan_lnt8()
{
    if (gr_screen.current_alphablend_mode == GR_ALPHABLEND_FILTER) {
        tmapscan_lna8();
        return;
    }

    // Linear unlit transparent mapper: texel 255 is not drawn.  Same
    // pipelined texel read (and one-texel lag) as tmapscan_lnn8.
    tmap_setup_uv_deltas((int)Tmap.fx_du_dx, (int)Tmap.fx_dv_dx);

    uint u_frac, v_frac;
    ubyte *tex = tmap_uv_start((int)Tmap.fx_u, (int)Tmap.fx_v, &u_frac, &v_frac);
    ubyte *dptr = (ubyte *)Tmap.dest_row_data; // edi
    uint du_frac = Tmap.DeltaUFrac; // edx

    ubyte texel = *tex; // get texture pixel 0

    int nblocks = Tmap.loop_count >> 2;
    if (nblocks != 0) {
        Tmap.num_big_steps = nblocks;
        Tmap.loop_count &= 3;
        do {
            for (int i = 0; i < 4; i++) {
                if (texel != 255)
                    *dptr = texel; // store pixel
                dptr++;
                texel = *tex; // read *before* stepping
                tex = tmap_uv_step(tex, &v_frac, &u_frac, Tmap.DeltaVFrac,
                                   du_frac);
            }
        } while (--Tmap.num_big_steps != 0);
    }

    int nleft = Tmap.loop_count;
    if (nleft == 0)
        return;
    int pairs = nleft >> 1;
    if (pairs != 0) {
        texel = *tex; // re-sync the pipelined read
        Tmap.loop_count = pairs;
        do {
            for (int i = 0; i < 2; i++) {
                if (texel != 255)
                    *dptr = texel;
                dptr++;
                texel = *tex;
                tex = tmap_uv_step(tex, &v_frac, &u_frac, Tmap.DeltaVFrac,
                                   du_frac);
            }
        } while (--Tmap.loop_count != 0);
    }
    if (nleft & 1) {
        if (texel != 255) // one_more_pix: no re-read
            *dptr = texel;
    }
}

// tmapscan_pln8_zbuffered_ppro / _pentium
//
// Perspective-correct, ramp-lit, non-tiled mapper with full z-buffer
// test and write.  Same span structure as tmapscan_pln8_c; each pixel
// reads its texel afresh (no pipelined lag here), and the light step is
// parked in the low word of Tmap.DeltaUFrac like the asm did.  The two
// CPU variants differed only in scheduling; both entry points share
// this body.
static void
tmapscan_pln8_zbuffered_c()
{
    ubyte *dptr = (ubyte *)Tmap.dest_row_data; // edi
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

    uint u_frac = 0, v_frac = 0; // ebx, ecx
    ubyte *tex = NULL; // esi

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

            tmap_setup_uv_deltas((int)Tmap.DeltaU, (int)Tmap.DeltaV);
            tex = tmap_uv_start((int)Tmap.UFixed, (int)Tmap.VFixed, &u_frac,
                                &v_frac);

            // set up affine registers; light step goes into the low
            // word of the DeltaUFrac field itself
            uint light = (uint)(Tmap.fx_l >> 8) & 0xffff; // bx
            Tmap.fx_l += Tmap.fx_dl_dx << 5; // walk the light a whole span
            ushort lstep = (ushort)((ushort)(uint)(Tmap.fx_l >> 8) -
                                    (ushort)light);
            lstep = (ushort)(lstep >> 5); // per-pixel light step
            u_frac = (u_frac & 0xffff0000u) | light;
            Tmap.DeltaUFrac = (Tmap.DeltaUFrac & 0xffff0000u) | lstep;

            // This divide happened while the pixel span was drawn.
            z_right = 1.0f / one_over_z;

            int w = Tmap.fx_w; // ebp
            uint *zbuf = &gr_zbuffer[(uintptr_t)dptr - Tmap.pScreenBits]; // edx

            for (Tmap.InnerLooper = 32; Tmap.InnerLooper > 0;
                 Tmap.InnerLooper--) {
                if (w > (int)*zbuf) { // compare the Z depth of this pixel
                    *zbuf = (uint)w; // write new Z value
                    *dptr =
                        gr_fade_table[(u_frac & 0xff00) + *tex]; // light the texel
                }
                w += Tmap.fx_dwdx;
                tex = tmap_uv_step(tex, &v_frac, &u_frac, Tmap.DeltaVFrac,
                                   Tmap.DeltaUFrac);
                zbuf++;
                dptr++;
            }

            Tmap.fx_w = w;

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

        tmap_setup_uv_deltas((int)Tmap.DeltaU, (int)Tmap.DeltaV);
    }

    // OnePixelSpan
    tex = tmap_uv_start((int)Tmap.UFixed, (int)Tmap.VFixed, &u_frac, &v_frac);

    uint light = (uint)(Tmap.fx_l >> 8) & 0xffff;
    u_frac = (u_frac & 0xffff0000u) | light;
    if (Tmap.WidthModLength > 1) { // NoDeltaLight otherwise
        Tmap.DeltaUFrac = (Tmap.DeltaUFrac & 0xffff0000u) |
                          ((uint)(Tmap.fx_dl_dx >> 8) & 0xffff);
    }

    int w = Tmap.fx_w; // ebp
    uint *zbuf = &gr_zbuffer[(uintptr_t)dptr - Tmap.pScreenBits]; // edx

    int n = ++Tmap.WidthModLength;
    int pairs = n >> 1;
    if (pairs != 0) {
        Tmap.WidthModLength = pairs;
        do { // NextPixel drew pixel pairs
            for (int i = 0; i < 2; i++) {
                if (w > (int)*zbuf) {
                    *zbuf = (uint)w;
                    *dptr = gr_fade_table[(u_frac & 0xff00) + *tex];
                }
                w += Tmap.fx_dwdx;
                tex = tmap_uv_step(tex, &v_frac, &u_frac, Tmap.DeltaVFrac,
                                   Tmap.DeltaUFrac);
                zbuf++;
                dptr++;
            }
        } while (--Tmap.WidthModLength > 0);
    }
    if (n & 1) {
        // one_more_pix
        if (w > (int)*zbuf) {
            *zbuf = (uint)w;
            *dptr = gr_fade_table[(u_frac & 0xff00) + *tex];
        }
    }
}

void
tmapscan_pln8_zbuffered_ppro()
{
    tmapscan_pln8_zbuffered_c();
}

void
tmapscan_pln8_zbuffered_pentium()
{
    tmapscan_pln8_zbuffered_c();
}

void
tmapscan_pln8_zbuffered()
{
    if (Gr_cpu > 5) {
        // Pentium Pro optimized code.
        tmapscan_pln8_zbuffered_ppro();
    }
    else {
        tmapscan_pln8_zbuffered_pentium();
    }
}

void
tmapscan_lnaa8_zbuffered()
{
    // FS1-era AA-textured scanline; asm not yet converted.  Trap loudly
    // if anything ever selects it.
    Int3();
}

void
tmapscan_lnaa8()
{
    // FS1-era AA-textured scanline; asm not yet converted.  Trap loudly
    // if anything ever selects it.
    Int3();
}

// Generic tiled perspective mapper -- the C conversion of the asm that
// lived in the five tmapscantiled{16x16..256x256}.cpp files, which
// differed only in their shift/mask constants (and in whether they
// recomputed the per-scanline setup; see tmapscan_pln8_tiled_setup).
//
// Inside a span, u and v travel packed in one 32-bit register with
// 'shift' = log2(tilesize) integer bits and 16-shift fraction bits per
// component, u in the high half:
//
//    ecx = [ u_int.u_frac | v_int.v_frac ]
//
// so a single 32-bit add steps both coordinates and the wrap of each
// component implements the tiling.  The texel index is recovered as
// (v_int << shift) | u_int -- the asm's shr ax,16-shift / rol eax,shift
// / and eax,(size*size-1).  The v-fraction carry into u_frac on the
// packed add is an inherited artifact of the packing.
//
// Conversion notes (deliberate deviations, see also the commit log):
//  - the 64x64 original packed the leftover span's registers as V:U
//    instead of U:V in its non-zbuffered path, transposing the tile for
//    those pixels; the consistent packing is used for all sizes here.
//  - the originals branched to the single-pixel leftover case before
//    packing the registers, so a 1-pixel leftover drew from garbage
//    (and compared the texture address against the z-buffer); here the
//    registers are packed first, as the subspace mapper already did.
//  - the non-zbuffered originals stored a leftover register into
//    Tmap.fx_w at the end of each span (dead copy-paste from the
//    z-buffered loop); never read, not reproduced.
static void
tmapscan_pln8_tiled_c(int shift, int zbuffered)
{
    const int frac_bits = 16 - shift;

    ubyte *dptr = (ubyte *)Tmap.dest_row_data; // edi
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

    uint uv = 0, duv = 0; // ecx and its packed step
    ushort light = 0, lstep = 0; // bx, bp

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

            // the asm still set up the linear-walk fields here even
            // though the packed walk below doesn't use them
            tmap_setup_uv_deltas((int)Tmap.DeltaU, (int)Tmap.DeltaV);

            // set up affine light registers
            light = (ushort)(uint)(Tmap.fx_l >> 8); // bx
            Tmap.fx_l += Tmap.fx_dl_dx << 5; // walk the light a whole span
            lstep = (ushort)((ushort)(uint)(Tmap.fx_l >> 8) - light);
            lstep = (ushort)(lstep >> 5); // per-pixel light step

            // This divide happened while the pixel span was drawn.
            z_right = 1.0f / one_over_z;

            // pack DU:DV and U:V
            duv = (((uint)Tmap.DeltaU << frac_bits) & 0xffff0000u) |
                  (((uint)Tmap.DeltaV >> shift) & 0xffffu);
            Tmap.DeltaUFrac = duv; // the asm parked the packed step here
            uv = (((uint)Tmap.UFixed << frac_bits) & 0xffff0000u) |
                 (((uint)Tmap.VFixed >> shift) & 0xffffu);

            if (zbuffered) {
                int w = Tmap.fx_w; // esi
                uint *zbuf =
                    &gr_zbuffer[(uintptr_t)dptr - Tmap.pScreenBits]; // edx

                for (Tmap.InnerLooper = 32; Tmap.InnerLooper > 0;
                     Tmap.InnerLooper--) {
                    if (w > (int)*zbuf) { // compare the Z depth of this pixel
                        *zbuf = (uint)w; // write z
                        uint u_int = uv >> (32 - shift);
                        uint v_int = (uv & 0xffffu) >> frac_bits;
                        ubyte c =
                            Tmap.pixptr[(v_int << shift) + u_int]; // (V*size)+U
                        *dptr = gr_fade_table[(uint)(light & 0xff00) + c];
                    }
                    uv += duv;
                    w += Tmap.fx_dwdx;
                    light = (ushort)(light + lstep);
                    zbuf++;
                    dptr++;
                }

                Tmap.fx_w = w;
            }
            else {
                for (Tmap.InnerLooper = 32; Tmap.InnerLooper > 0;
                     Tmap.InnerLooper--) {
                    uint u_int = uv >> (32 - shift);
                    uint v_int = (uv & 0xffffu) >> frac_bits;
                    ubyte c = Tmap.pixptr[(v_int << shift) + u_int];
                    *dptr = gr_fade_table[(uint)(light & 0xff00) + c];
                    uv += duv;
                    light = (ushort)(light + lstep);
                    dptr++;
                }
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

        tmap_setup_uv_deltas((int)Tmap.DeltaU, (int)Tmap.DeltaV);
    }

    // OnePixelSpan
    light = (ushort)(uint)(Tmap.fx_l >> 8);
    lstep = (ushort)(uint)(Tmap.fx_dl_dx >> 8); // undivided per-pixel step

    duv = (((uint)Tmap.DeltaU << frac_bits) & 0xffff0000u) |
          (((uint)Tmap.DeltaV >> shift) & 0xffffu);
    Tmap.DeltaUFrac = duv;
    uv = (((uint)Tmap.UFixed << frac_bits) & 0xffff0000u) |
         (((uint)Tmap.VFixed >> shift) & 0xffffu);

    int w = Tmap.fx_w; // esi
    uint *zbuf = &gr_zbuffer[(uintptr_t)dptr - Tmap.pScreenBits];

    int n = ++Tmap.WidthModLength;
    int pairs = n >> 1;
    if (pairs != 0) {
        Tmap.WidthModLength = pairs;
        do { // NextPixel drew pixel pairs
            for (int i = 0; i < 2; i++) {
                if (!zbuffered || w > (int)*zbuf) {
                    if (zbuffered)
                        *zbuf = (uint)w;
                    uint u_int = uv >> (32 - shift);
                    uint v_int = (uv & 0xffffu) >> frac_bits;
                    ubyte c = Tmap.pixptr[(v_int << shift) + u_int];
                    *dptr = gr_fade_table[(uint)(light & 0xff00) + c];
                }
                uv += duv;
                w += Tmap.fx_dwdx;
                light = (ushort)(light + lstep);
                zbuf++;
                dptr++;
            }
        } while (--Tmap.WidthModLength > 0);
    }
    if (n & 1) {
        // one_more_pix
        if (!zbuffered || w > (int)*zbuf) {
            if (zbuffered)
                *zbuf = (uint)w;
            uint u_int = uv >> (32 - shift);
            uint v_int = (uv & 0xffffu) >> frac_bits;
            ubyte c = Tmap.pixptr[(v_int << shift) + u_int];
            *dptr = gr_fade_table[(uint)(light & 0xff00) + c];
        }
    }
}

// Per-scanline setup that the 16/32/64 tile files performed before
// their asm.  The 128/256 files did not, relying on grx_tmapper's
// outer loop having set the same fields; the do_setup flag preserves
// each size's original behavior.
static void
tmapscan_pln8_tiled_setup()
{
    Tmap.fx_l = fl2f(Tmap.l.b * 32.0);
    Tmap.fx_l_right = fl2f(Tmap.r.b * 32.0);
    Tmap.fx_dl_dx = fl2f(Tmap.deltas.b * 32.0);

    if (Tmap.fx_dl_dx < 0) {
        Tmap.fx_dl_dx = -Tmap.fx_dl_dx;
        Tmap.fx_l = (67 * F1_0) - Tmap.fx_l;
        Tmap.fx_l_right = (67 * F1_0) - Tmap.fx_l_right;
    }

    Tmap.fl_dudx_wide = Tmap.deltas.u * 32.0f;
    Tmap.fl_dvdx_wide = Tmap.deltas.v * 32.0f;
    Tmap.fl_dwdx_wide = Tmap.deltas.sw * 32.0f;

    Tmap.fx_w = fl2i(Tmap.l.sw * GR_Z_RANGE) + gr_zoffset;
    Tmap.fx_dwdx = fl2i(Tmap.deltas.sw * GR_Z_RANGE);
}

void
tmapscan_pln8_zbuffered_tiled_g(int shift, int do_setup)
{
    if (do_setup)
        tmapscan_pln8_tiled_setup();

    tmapscan_pln8_tiled_c(shift, 1);
}

void
tmapscan_pln8_tiled_g(int shift, int do_setup)
{
    if (gr_zbuffering) {
        switch (gr_zbuffering_mode) {
        case GR_ZBUFF_NONE:
            break;
        case GR_ZBUFF_FULL: // both
            tmapscan_pln8_zbuffered_tiled_g(shift, do_setup);
            return;
        case GR_ZBUFF_WRITE: // write only
            tmapscan_pln8_zbuffered_tiled_g(shift, do_setup);
            break;
        case GR_ZBUFF_READ: // read only
            tmapscan_pln8_zbuffered_tiled_g(shift, do_setup);
            return;
        }
    }

    if (do_setup)
        tmapscan_pln8_tiled_setup();

    tmapscan_pln8_tiled_c(shift, 0);
}
