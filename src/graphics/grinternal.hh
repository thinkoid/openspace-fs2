// -*- mode: c++; -*-

#ifndef FREESPACE2_GRAPHICS_GRINTERNAL_HH
#define FREESPACE2_GRAPHICS_GRINTERNAL_HH

#include <defs.hh>

#include <graphics/font.hh>

extern ubyte Gr_original_palette[768]; // The palette
extern ubyte Gr_current_palette[768];

extern char Gr_current_palette_name[128];

struct color_gun  {
    int bits;
    int shift;
    int scale;
    int mask;
};

// screen format
extern color_gun Gr_red, Gr_green, Gr_blue, Gr_alpha;

// texture format
extern color_gun Gr_t_red, Gr_t_green, Gr_t_blue, Gr_t_alpha;

// alpha texture format
extern color_gun Gr_ta_red, Gr_ta_green, Gr_ta_blue, Gr_ta_alpha;

// CURRENT FORMAT - note - this is what bmpman uses when fiddling with
// pixels/colors. so be sure its properly set to one of the above values
extern color_gun *Gr_current_red, *Gr_current_green, *Gr_current_blue,
    *Gr_current_alpha;

extern float Gr_gamma;
extern int Gr_gamma_int;

#define TCACHE_TYPE_AABITMAP 0 // HUD bitmap.  All Alpha.
#define TCACHE_TYPE_NORMAL 1   // Normal bitmap. Alpha = 0.
#define TCACHE_TYPE_XPARENT \
    2 // Bitmap with 0,255,0 = transparent.  Alpha=0 if transparent, 1 if not.
#define TCACHE_TYPE_INTERFACE \
    3 // for graphics that are using in the interface (for special filtering or
      // sizing)
#define TCACHE_TYPE_COMPRESSED 4 // Compressed bitmap type (DXT1, DXT3, DXT5)
#define TCACHE_TYPE_CUBEMAP 5

#define NEBULA_COLORS 20

enum gr_alpha_blend {
    ALPHA_BLEND_NONE,                  // 1*SrcPixel + 0*DestPixel
    ALPHA_BLEND_ADDITIVE,              // 1*SrcPixel + 1*DestPixel
    ALPHA_BLEND_ALPHA_ADDITIVE,        // Alpha*SrcPixel + 1*DestPixel
    ALPHA_BLEND_ALPHA_BLEND_ALPHA,     // Alpha*SrcPixel + (1-Alpha)*DestPixel
    ALPHA_BLEND_ALPHA_BLEND_SRC_COLOR, // Alpha*SrcPixel +
                                       // (1-SrcPixel)*DestPixel
    ALPHA_BLEND_PREMULTIPLIED          // 1*SrcPixel + (1-Alpha)*DestPixel
};

enum gr_zbuffer_type {
    ZBUFFER_TYPE_NONE,
    ZBUFFER_TYPE_READ,
    ZBUFFER_TYPE_WRITE,
    ZBUFFER_TYPE_FULL,
    ZBUFFER_TYPE_DEFAULT
};

#endif // FREESPACE2_GRAPHICS_GRINTERNAL_HH
