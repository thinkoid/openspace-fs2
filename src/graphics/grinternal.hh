/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef _GRINTERNAL_H
#define _GRINTERNAL_H

#include <graphics/font.hh>
#include <graphics/2d.hh>
#include <graphics/grzbuffer.hh>

extern int Gr_cursor;

// pointer math through uintptr_t; retail used uint (32-bit pointers)
#define GR_SCREEN_PTR(type, x, y)                                                \
    ((type *)(uintptr_t(gr_screen.offscreen_buffer) +                            \
              uintptr_t(((x) + gr_screen.offset_x) * sizeof(type)) +             \
              uintptr_t(((y) + gr_screen.offset_y) * gr_screen.rowsize)))
#define GR_SCREEN_PTR_SIZE(bpp, x, y)                                            \
    ((uintptr_t)(uintptr_t(gr_screen.offscreen_buffer) +                         \
                 uintptr_t(((x) + gr_screen.offset_x) * (bpp)) +                 \
                 uintptr_t(((y) + gr_screen.offset_y) * gr_screen.rowsize)))

extern ubyte Gr_original_palette[768]; // The palette
extern ubyte Gr_current_palette[768];

typedef struct alphacolor
{
    int used;
    int r, g, b, alpha;
    int type; // See AC_TYPE_??? define
    uint palette_checksum; // palette the table below was computed against
    color *clr;
    union
    {
        ubyte lookup[16][256]; // For 8-bpp rendering modes
    } table;
} alphacolor;

// for backwards fred aabitmap compatibility
typedef struct alphacolor_old
{
    int used;
    int r, g, b, alpha;
    int type; // See AC_TYPE_??? define
    color *clr;
    union
    {
        ubyte lookup[16][256]; // For 8-bpp rendering modes
    } table;
} alphacolor_old;

extern alphacolor *Current_alphacolor;

extern char Gr_current_palette_name[128];

typedef struct color_gun
{
    int bits;
    int shift;
    int scale;
    int mask;
} color_gun;

// screen format
extern color_gun Gr_red, Gr_green, Gr_blue, Gr_alpha;

// texture format
extern color_gun Gr_t_red, Gr_t_green, Gr_t_blue, Gr_t_alpha;

// alpha texture format
extern color_gun Gr_ta_red, Gr_ta_green, Gr_ta_blue, Gr_ta_alpha;

// CURRENT FORMAT - note - this is what bmpman uses when fiddling with pixels/colors. so be sure its properly set to one
// of the above values
extern color_gun *Gr_current_red, *Gr_current_green, *Gr_current_blue,
    *Gr_current_alpha;

// Translate the 768 byte 'src' palette into
// CPU identification variables
extern int Gr_cpu; // What type of CPU.  5=Pentium, 6=Ppro/PII
extern int Gr_mmx; // MMX capabilities?  0=No, 1=Yes

extern float Gr_gamma;
extern int Gr_gamma_int; // int(Gr_gamma*100)
extern int Gr_gamma_lookup[256];

// HUD bitmap.  All Alpha.
#define TCACHE_TYPE_AABITMAP 0
// Normal bitmap. Alpha = 0.
#define TCACHE_TYPE_NORMAL 1
// Bitmap with 0,255,0 = transparent.  Alpha=0 if transparent, 1 if not.
#define TCACHE_TYPE_XPARENT 2
// Bitmap with 255,255,255 = non-darkening.  Alpha=1 if non-darkening, 0 if not.
#define TCACHE_TYPE_NONDARKENING 3
// section of a bitmap
#define TCACHE_TYPE_BITMAP_SECTION 4

#endif
