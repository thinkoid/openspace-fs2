/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell
 * or otherwise commercially exploit the source or things you created based on the
 * source.
 *
*/

// Retail shipped this file as an unwired skeleton: gr_opengl_* stubs and the
// vtable binding block, no GL calls at all.  Revived for the port: SDL2
// provides the context; the drawing machinery (texture cache, tmapper, 2D
// quads, fonts) is transcribed from the first fs2open GL backend (2002,
// icculus/DDOI/penguin — itself grown from this skeleton) against the retail
// D3D backend as the vtable contract.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <osapi/osapi.hh>
#include <graphics/2d.hh>
#include <bmpman/bmpman.hh>
#include <math/floating.hh>
#include <palman/palman.hh>
#include <graphics/grinternal.hh>
#include <graphics/gropengl.hh>
#include <graphics/font.hh>
#include <graphics/line.hh>
#include <io/mouse.hh>
#include <globalincs/systemvars.hh>
#include <nebula/neb.hh>
#include <graphics/tmapper.hh>

static int Inited = 0;

static SDL_GLContext GL_context = NULL;

extern uint Gr_signature;

// shared software-zbuffer mode flags (grzbuffer.cpp); the GL backend keys
// glDepthFunc/glDepthMask off the same globals the game toggles
extern int gr_zbuffering, gr_zbuffering_mode, gr_global_zbuffering;

typedef enum gr_texture_source {
    TEXTURE_SOURCE_NONE,
    TEXTURE_SOURCE_DECAL,
    TEXTURE_SOURCE_NO_FILTERING,
} gr_texture_source;

typedef enum gr_alpha_blend {
    ALPHA_BLEND_NONE, // 1*SrcPixel + 0*DestPixel
    ALPHA_BLEND_ALPHA_ADDITIVE, // Alpha*SrcPixel + 1*DestPixel
    ALPHA_BLEND_ALPHA_BLEND_ALPHA, // Alpha*SrcPixel + (1-Alpha)*DestPixel
    ALPHA_BLEND_ALPHA_BLEND_SRC_COLOR, // Alpha*SrcPixel + (1-SrcPixel)*DestPixel
} gr_alpha_blend;

typedef enum gr_zbuffer_type {
    ZBUFFER_TYPE_NONE,
    ZBUFFER_TYPE_READ,
    ZBUFFER_TYPE_WRITE,
    ZBUFFER_TYPE_FULL,
} gr_zbuffer_type;

#define NEBULA_COLORS 20

static char *Gr_saved_screen = NULL;
static int Gr_saved_screen_bitmap;

// texture cache ---------------------------------------------------------

typedef struct tcache_slot_opengl
{
    GLuint texture_handle;
    float u_scale, v_scale;
    int bitmap_id;
    int size;
    char used_this_frame;
    int time_created;
    ushort w, h;

    // sections (unused: no caller passes TMAP_FLAG_BITMAP_SECTION in this
    // tree; kept so the code reads against the 2002/D3D references)
    tcache_slot_opengl *data_sections[MAX_BMAP_SECTIONS_X][MAX_BMAP_SECTIONS_Y];
    tcache_slot_opengl *parent;
} tcache_slot_opengl;

static void *Texture_sections = NULL;
static tcache_slot_opengl *Textures = NULL;

int GL_texture_sections = 0;
int GL_frame_count = 0;
int GL_min_texture_width = 0;
int GL_max_texture_width = 0;
int GL_min_texture_height = 0;
int GL_max_texture_height = 0;
int GL_square_textures = 0;
int GL_textures_in = 0;
int GL_textures_in_frame = 0;
int GL_last_bitmap_id = -1;

// rolling copy of the last finished frame (glCopyTexSubImage2D at flip);
// gr_opengl_save_screen reads it because GL_FRONT is undefined under a
// compositor.  Frozen while a saved screen is outstanding so the popup's
// background can't be overwritten by its own frames
static GLuint GL_screen_stash_tex = 0;
static int GL_screen_stash_frozen = 0;
int GL_last_detail = -1;
int GL_last_bitmap_type = -1;
int GL_last_section_x = -1;
int GL_last_section_y = -1;
int GL_should_preload = 0;

static int vram_full = 0;

void
gr_opengl_set_state(gr_texture_source ts, gr_alpha_blend ab, gr_zbuffer_type zt)
{
    switch (ts) {
    case TEXTURE_SOURCE_NONE:
        glBindTexture(GL_TEXTURE_2D, 0);
        gr_tcache_set(-1, -1, NULL, NULL);
        break;
    case TEXTURE_SOURCE_DECAL:
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        break;
    case TEXTURE_SOURCE_NO_FILTERING:
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        break;
    default:
        break;
    }

    switch (ab) {
    case ALPHA_BLEND_NONE:
        glBlendFunc(GL_ONE, GL_ZERO);
        break;
    case ALPHA_BLEND_ALPHA_ADDITIVE:
        glBlendFunc(GL_ONE, GL_ONE);
        break;
    case ALPHA_BLEND_ALPHA_BLEND_ALPHA:
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;
    case ALPHA_BLEND_ALPHA_BLEND_SRC_COLOR:
        glBlendFunc(GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR);
        break;
    default:
        break;
    }

    switch (zt) {
    case ZBUFFER_TYPE_NONE:
        glDepthFunc(GL_ALWAYS);
        glDepthMask(GL_FALSE);
        break;
    case ZBUFFER_TYPE_READ:
        glDepthFunc(GL_LESS);
        glDepthMask(GL_FALSE);
        break;
    case ZBUFFER_TYPE_WRITE:
        glDepthFunc(GL_ALWAYS);
        glDepthMask(GL_TRUE);
        break;
    case ZBUFFER_TYPE_FULL:
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        break;
    default:
        break;
    }
}

void
gr_opengl_clear()
{
    glClearColor(gr_screen.current_clear_color.red / 255.0f,
                 gr_screen.current_clear_color.green / 255.0f,
                 gr_screen.current_clear_color.blue / 255.0f, 1.0f);

    glClear(GL_COLOR_BUFFER_BIT);
}

void opengl_tcache_frame();
void opengl_tcache_flush();

void
gr_opengl_flip()
{
    if (!Inited)
        return;

    gr_reset_clip();

    mouse_eval_deltas();

    if (mouse_is_visible()) {
        int mx, my;

        gr_reset_clip();
        mouse_get_pos(&mx, &my);

        if (Gr_cursor != -1) {
            gr_set_bitmap(Gr_cursor);
            gr_bitmap(mx, my);
        }
    }

#ifndef NDEBUG
    GLenum error;
    int ic = 0;
    do {
        error = glGetError();

        if (error != GL_NO_ERROR) {
            mprintf(("!!DEBUG!! OpenGL Error: 0x%04x (%d this frame)\n",
                     (int)error, ic));
        }
        ic++;
    } while (error != GL_NO_ERROR);
#endif

    // stash the finished frame GPU-side: save_screen reads this instead of
    // GL_FRONT, whose contents are undefined under a compositor (the popup
    // backgrounds came back black through XWayland)
    if (!GL_screen_stash_frozen) {
        if (GL_screen_stash_tex == 0) {
            glGenTextures(1, &GL_screen_stash_tex);
            glBindTexture(GL_TEXTURE_2D, GL_screen_stash_tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, gr_screen.max_w,
                         gr_screen.max_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        }
        else {
            glBindTexture(GL_TEXTURE_2D, GL_screen_stash_tex);
        }
        glReadBuffer(GL_BACK);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, gr_screen.max_w,
                            gr_screen.max_h);
        // this bound its own texture behind the tcache's back
        GL_last_bitmap_id = -1;
    }

    // TEMPORARY bring-up aid: FS2_FRAME_DUMP=<dir> writes every Nth frame
    // (FS2_FRAME_DUMP_STRIDE, default 60) as P6 PPM, read back from the back
    // buffer before the swap; same hook as the software renderer, delete
    // when stable
    static int frame_no = 0;
    static int frame_stride = 0;
    const char *dumpdir = getenv("FS2_FRAME_DUMP");
    if (frame_stride == 0) {
        const char *s = getenv("FS2_FRAME_DUMP_STRIDE");
        frame_stride = (s && atoi(s) > 0) ? atoi(s) : 60;
    }
    if (dumpdir && (frame_no++ % frame_stride) == 0) {
        int w = gr_screen.max_w, h = gr_screen.max_h;
        ubyte *pixels = (ubyte *)malloc(w * h * 3);
        if (pixels) {
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadBuffer(GL_BACK);
            glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels);

            char path[512];
            snprintf(path, sizeof(path), "%s/frame%05d.ppm", dumpdir, frame_no);
            FILE *out = fopen(path, "wb");
            if (out) {
                fprintf(out, "P6\n%d %d\n255\n", w, h);
                for (int y = h - 1; y >= 0; y--) { // GL rows are bottom-up
                    fwrite(pixels + y * w * 3, 1, w * 3, out);
                }
                fclose(out);
            }
            free(pixels);
        }
    }

    SDL_Window *win = os_get_sdl_window();
    if (win) {
        SDL_GL_SwapWindow(win);
    }

    opengl_tcache_frame();

    // start the new back buffer deterministic: the two buffers otherwise
    // alternate stale contents (visible as flicker) wherever the game does
    // not draw over every pixel
    gr_opengl_clear();
}

void
gr_opengl_flip_window(uint _hdc, int x, int y, int w, int h)
{ }

void
gr_opengl_set_clip(int x, int y, int w, int h)
{
    // check for sanity of parameters
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;

    if (x >= gr_screen.max_w)
        x = gr_screen.max_w - 1;
    if (y >= gr_screen.max_h)
        y = gr_screen.max_h - 1;

    if (x + w > gr_screen.max_w)
        w = gr_screen.max_w - x;
    if (y + h > gr_screen.max_h)
        h = gr_screen.max_h - y;

    if (w > gr_screen.max_w)
        w = gr_screen.max_w;
    if (h > gr_screen.max_h)
        h = gr_screen.max_h;

    gr_screen.offset_x = x;
    gr_screen.offset_y = y;
    gr_screen.clip_left = 0;
    gr_screen.clip_right = w - 1;
    gr_screen.clip_top = 0;
    gr_screen.clip_bottom = h - 1;
    gr_screen.clip_width = w;
    gr_screen.clip_height = h;

    glEnable(GL_SCISSOR_TEST);
    glScissor(x, gr_screen.max_h - y - h, w, h);
}

void
gr_opengl_reset_clip()
{
    gr_screen.offset_x = 0;
    gr_screen.offset_y = 0;
    gr_screen.clip_left = 0;
    gr_screen.clip_top = 0;
    gr_screen.clip_right = gr_screen.max_w - 1;
    gr_screen.clip_bottom = gr_screen.max_h - 1;
    gr_screen.clip_width = gr_screen.max_w;
    gr_screen.clip_height = gr_screen.max_h;

    glDisable(GL_SCISSOR_TEST);
}

void
gr_opengl_set_bitmap(int bitmap_num, int alphablend_mode, int bitblt_mode,
                     float alpha, int sx, int sy)
{
    gr_screen.current_alpha = alpha;
    gr_screen.current_alphablend_mode = alphablend_mode;
    gr_screen.current_bitblt_mode = bitblt_mode;
    gr_screen.current_bitmap = bitmap_num;

    gr_screen.current_bitmap_sx = sx;
    gr_screen.current_bitmap_sy = sy;
}

void
gr_opengl_create_shader(shader *shade, float r, float g, float b, float c)
{
    shade->screen_sig = gr_screen.signature;
    shade->r = r;
    shade->g = g;
    shade->b = b;
    shade->c = c;
}

void
gr_opengl_set_shader(shader *shade)
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

// the 2002 backend routed this through g3_draw_poly_constant_sw; a direct
// blended quad in the ortho projection is the same result without dragging
// the 3d frame machinery into 2D rectangles
static void
gr_opengl_rect_internal(int x, int y, int w, int h, int r, int g, int b, int a)
{
    gr_opengl_set_state(TEXTURE_SOURCE_NONE, ALPHA_BLEND_ALPHA_BLEND_ALPHA,
                        ZBUFFER_TYPE_NONE);

    float x1 = i2fl(x + gr_screen.offset_x);
    float y1 = i2fl(y + gr_screen.offset_y);
    float x2 = i2fl(x + w + gr_screen.offset_x);
    float y2 = i2fl(y + h + gr_screen.offset_y);

    glColor4ub((ubyte)r, (ubyte)g, (ubyte)b, (ubyte)a);
    glBegin(GL_QUADS);
    glVertex3f(x1, y2, -0.99f);
    glVertex3f(x2, y2, -0.99f);
    glVertex3f(x2, y1, -0.99f);
    glVertex3f(x1, y1, -0.99f);
    glEnd();
}

void
gr_opengl_rect(int x, int y, int w, int h)
{
    gr_opengl_rect_internal(x, y, w, h, gr_screen.current_color.red,
                            gr_screen.current_color.green,
                            gr_screen.current_color.blue,
                            gr_screen.current_color.alpha);
}

void
gr_opengl_shade(int x, int y, int w, int h)
{
    int r, g, b, a;

    float shade1 = 1.0f;
    float shade2 = 6.0f;

    r = fl2i(gr_screen.current_shader.r * 255.0f * shade1);
    if (r < 0)
        r = 0;
    else if (r > 255)
        r = 255;
    g = fl2i(gr_screen.current_shader.g * 255.0f * shade1);
    if (g < 0)
        g = 0;
    else if (g > 255)
        g = 255;
    b = fl2i(gr_screen.current_shader.b * 255.0f * shade1);
    if (b < 0)
        b = 0;
    else if (b > 255)
        b = 255;
    a = fl2i(gr_screen.current_shader.c * 255.0f * shade2);
    if (a < 0)
        a = 0;
    else if (a > 255)
        a = 255;

    gr_opengl_rect_internal(x, y, w, h, r, g, b, a);
}

void
gr_opengl_flash(int r, int g, int b)
{
    if (r < 0)
        r = 0;
    else if (r > 255)
        r = 255;
    if (g < 0)
        g = 0;
    else if (g > 255)
        g = 255;
    if (b < 0)
        b = 0;
    else if (b > 255)
        b = 255;

    if (r || g || b) {
        gr_opengl_set_state(TEXTURE_SOURCE_NONE, ALPHA_BLEND_ALPHA_ADDITIVE,
                            ZBUFFER_TYPE_NONE);

        float x1, x2, y1, y2;
        x1 = i2fl(gr_screen.clip_left + gr_screen.offset_x);
        y1 = i2fl(gr_screen.clip_top + gr_screen.offset_y);
        x2 = i2fl(gr_screen.clip_right + gr_screen.offset_x);
        y2 = i2fl(gr_screen.clip_bottom + gr_screen.offset_y);

        glColor4ub((ubyte)r, (ubyte)g, (ubyte)b, 255);
        glBegin(GL_QUADS);
        glVertex3f(x1, y2, -0.99f);
        glVertex3f(x2, y2, -0.99f);
        glVertex3f(x2, y1, -0.99f);
        glVertex3f(x1, y1, -0.99f);
        glEnd();
    }
}

static void
gr_opengl_aabitmap_ex_internal(int x, int y, int w, int h, int sx, int sy)
{
    if (w < 1)
        return;
    if (h < 1)
        return;

    float u_scale, v_scale;

    gr_opengl_set_state(TEXTURE_SOURCE_NO_FILTERING,
                        ALPHA_BLEND_ALPHA_BLEND_ALPHA, ZBUFFER_TYPE_NONE);

    if (!gr_tcache_set(gr_screen.current_bitmap, TCACHE_TYPE_AABITMAP, &u_scale,
                       &v_scale)) {
        // Couldn't set texture
        mprintf(("WARNING: Error setting aabitmap texture!\n"));
        return;
    }

    float u0, u1, v0, v1;
    float x1, x2, y1, y2;
    int bw, bh;

    bm_get_info(gr_screen.current_bitmap, &bw, &bh);

    u0 = u_scale * i2fl(sx) / i2fl(bw);
    v0 = v_scale * i2fl(sy) / i2fl(bh);

    u1 = u_scale * i2fl(sx + w) / i2fl(bw);
    v1 = v_scale * i2fl(sy + h) / i2fl(bh);

    x1 = i2fl(x + gr_screen.offset_x);
    y1 = i2fl(y + gr_screen.offset_y);
    x2 = i2fl(x + w + gr_screen.offset_x);
    y2 = i2fl(y + h + gr_screen.offset_y);

    // plain (non-alpha) colors draw fully opaque -- black text on a white
    // selection bar comes through here
    ubyte fc_alpha = gr_screen.current_color.is_alphacolor
                         ? gr_screen.current_color.alpha
                         : 255;
    glColor4ub(gr_screen.current_color.red, gr_screen.current_color.green,
               gr_screen.current_color.blue, fc_alpha);

    glBegin(GL_QUADS);
    glTexCoord2f(u0, v1);
    glVertex3f(x1, y2, -0.99f);

    glTexCoord2f(u1, v1);
    glVertex3f(x2, y2, -0.99f);

    glTexCoord2f(u1, v0);
    glVertex3f(x2, y1, -0.99f);

    glTexCoord2f(u0, v0);
    glVertex3f(x1, y1, -0.99f);
    glEnd();
}

void
gr_opengl_aabitmap_ex(int x, int y, int w, int h, int sx, int sy)
{
    int reclip;
#ifndef NDEBUG
    int count = 0;
#endif

    int dx1 = x, dx2 = x + w - 1;
    int dy1 = y, dy2 = y + h - 1;

    int bw, bh;
    bm_get_info(gr_screen.current_bitmap, &bw, &bh, NULL);

    do {
        reclip = 0;
#ifndef NDEBUG
        if (count > 1)
            Int3();
        count++;
#endif

        if ((dx1 > gr_screen.clip_right) || (dx2 < gr_screen.clip_left))
            return;
        if ((dy1 > gr_screen.clip_bottom) || (dy2 < gr_screen.clip_top))
            return;
        if (dx1 < gr_screen.clip_left) {
            sx += gr_screen.clip_left - dx1;
            dx1 = gr_screen.clip_left;
        }
        if (dy1 < gr_screen.clip_top) {
            sy += gr_screen.clip_top - dy1;
            dy1 = gr_screen.clip_top;
        }
        if (dx2 > gr_screen.clip_right) {
            dx2 = gr_screen.clip_right;
        }
        if (dy2 > gr_screen.clip_bottom) {
            dy2 = gr_screen.clip_bottom;
        }

        if (sx < 0) {
            dx1 -= sx;
            sx = 0;
            reclip = 1;
        }

        if (sy < 0) {
            dy1 -= sy;
            sy = 0;
            reclip = 1;
        }

        w = dx2 - dx1 + 1;
        h = dy2 - dy1 + 1;

        if (sx + w > bw) {
            w = bw - sx;
            dx2 = dx1 + w - 1;
        }

        if (sy + h > bh) {
            h = bh - sy;
            dy2 = dy1 + h - 1;
        }

        if (w < 1)
            return; // clipped away!
        if (h < 1)
            return; // clipped away!

    } while (reclip);

// Make sure clipping algorithm works
#ifndef NDEBUG
    Assert(w > 0);
    Assert(h > 0);
    Assert(w == (dx2 - dx1 + 1));
    Assert(h == (dy2 - dy1 + 1));
    Assert(sx >= 0);
    Assert(sy >= 0);
    Assert(sx + w <= bw);
    Assert(sy + h <= bh);
    Assert(dx2 >= dx1);
    Assert(dy2 >= dy1);
    Assert((dx1 >= gr_screen.clip_left) && (dx1 <= gr_screen.clip_right));
    Assert((dx2 >= gr_screen.clip_left) && (dx2 <= gr_screen.clip_right));
    Assert((dy1 >= gr_screen.clip_top) && (dy1 <= gr_screen.clip_bottom));
    Assert((dy2 >= gr_screen.clip_top) && (dy2 <= gr_screen.clip_bottom));
#endif

    // We now have dx1,dy1 and dx2,dy2 and sx, sy all set validly within clip regions.
    gr_opengl_aabitmap_ex_internal(dx1, dy1, dx2 - dx1 + 1, dy2 - dy1 + 1, sx,
                                   sy);
}

void
gr_opengl_aabitmap(int x, int y)
{
    int w, h;

    bm_get_info(gr_screen.current_bitmap, &w, &h, NULL);
    int dx1 = x, dx2 = x + w - 1;
    int dy1 = y, dy2 = y + h - 1;
    int sx = 0, sy = 0;

    if ((dx1 > gr_screen.clip_right) || (dx2 < gr_screen.clip_left))
        return;
    if ((dy1 > gr_screen.clip_bottom) || (dy2 < gr_screen.clip_top))
        return;
    if (dx1 < gr_screen.clip_left) {
        sx = gr_screen.clip_left - dx1;
        dx1 = gr_screen.clip_left;
    }
    if (dy1 < gr_screen.clip_top) {
        sy = gr_screen.clip_top - dy1;
        dy1 = gr_screen.clip_top;
    }
    if (dx2 > gr_screen.clip_right) {
        dx2 = gr_screen.clip_right;
    }
    if (dy2 > gr_screen.clip_bottom) {
        dy2 = gr_screen.clip_bottom;
    }

    if (sx < 0)
        return;
    if (sy < 0)
        return;
    if (sx >= w)
        return;
    if (sy >= h)
        return;

    // Draw bitmap bm[sx,sy] into (dx1,dy1)-(dx2,dy2)
    gr_aabitmap_ex(dx1, dy1, dx2 - dx1 + 1, dy2 - dy1 + 1, sx, sy);
}

void
gr_opengl_string(int sx, int sy, char *s)
{
    int width, spacing, letter;
    int x, y;

    if (!Current_font) {
        return;
    }

    gr_set_bitmap(Current_font->bitmap_id);

    x = sx;
    y = sy;

    if (sx == 0x8000) { //centered
        x = get_centered_x(s);
    }
    else {
        x = sx;
    }

    spacing = 0;

    while (*s) {
        x += spacing;

        while (*s == '\n') {
            s++;
            y += Current_font->h;
            if (sx == 0x8000) { //centered
                x = get_centered_x(s);
            }
            else {
                x = sx;
            }
        }
        if (*s == 0)
            break;

        letter = get_char_width(s[0], s[1], &width, &spacing);
        s++;

        //not in font, draw as space
        if (letter < 0) {
            continue;
        }

        int xd, yd, xc, yc;
        int wc, hc;

        // Check if this character is totally clipped
        if (x + width < gr_screen.clip_left)
            continue;
        if (y + Current_font->h < gr_screen.clip_top)
            continue;
        if (x > gr_screen.clip_right)
            continue;
        if (y > gr_screen.clip_bottom)
            continue;

        xd = yd = 0;
        if (x < gr_screen.clip_left)
            xd = gr_screen.clip_left - x;
        if (y < gr_screen.clip_top)
            yd = gr_screen.clip_top - y;
        xc = x + xd;
        yc = y + yd;

        wc = width - xd;
        hc = Current_font->h - yd;
        if (xc + wc > gr_screen.clip_right)
            wc = gr_screen.clip_right - xc;
        if (yc + hc > gr_screen.clip_bottom)
            hc = gr_screen.clip_bottom - yc;

        if (wc < 1)
            continue;
        if (hc < 1)
            continue;

        int u = Current_font->bm_u[letter];
        int v = Current_font->bm_v[letter];

        gr_opengl_aabitmap_ex_internal(xc, yc, wc, hc, u + xd, v + yd);
    }
}

void
gr_opengl_line(int x1, int y1, int x2, int y2)
{
    int clipped = 0, swapped = 0;

    gr_opengl_set_state(TEXTURE_SOURCE_NONE, ALPHA_BLEND_ALPHA_BLEND_ALPHA,
                        ZBUFFER_TYPE_NONE);

    INT_CLIPLINE(x1, y1, x2, y2, gr_screen.clip_left, gr_screen.clip_top,
                 gr_screen.clip_right, gr_screen.clip_bottom, return, clipped = 1,
                 swapped = 1);

    float sx1, sy1;
    float sx2, sy2;

    sx1 = i2fl(x1 + gr_screen.offset_x);
    sy1 = i2fl(y1 + gr_screen.offset_y);
    sx2 = i2fl(x2 + gr_screen.offset_x);
    sy2 = i2fl(y2 + gr_screen.offset_y);

    if (x1 == x2 && y1 == y2) {
        glBegin(GL_POINTS);
        glColor4ub(gr_screen.current_color.red, gr_screen.current_color.green,
                   gr_screen.current_color.blue, gr_screen.current_color.alpha);
        glVertex3f(sx1, sy1, -0.99f);
        glEnd();

        return;
    }

    if (x1 == x2) {
        if (sy1 < sy2) {
            sy2 += 0.5f;
        }
        else {
            sy1 += 0.5f;
        }
    }
    else if (y1 == y2) {
        if (sx1 < sx2) {
            sx2 += 0.5f;
        }
        else {
            sx1 += 0.5f;
        }
    }

    glBegin(GL_LINES);
    glColor4ub(gr_screen.current_color.red, gr_screen.current_color.green,
               gr_screen.current_color.blue, gr_screen.current_color.alpha);
    glVertex3f(sx2, sy2, -0.99f);
    glVertex3f(sx1, sy1, -0.99f);
    glEnd();
}

void
gr_opengl_pixel(int x, int y)
{
    gr_opengl_line(x, y, x, y);
}

void
gr_opengl_aaline(vertex *v1, vertex *v2)
{
    gr_opengl_line(fl2i(v1->sx), fl2i(v1->sy), fl2i(v2->sx), fl2i(v2->sy));
}

void
gr_opengl_gradient(int x1, int y1, int x2, int y2)
{
    int clipped = 0, swapped = 0;

    if (!gr_screen.current_color.is_alphacolor) {
        gr_line(x1, y1, x2, y2);
        return;
    }

    INT_CLIPLINE(x1, y1, x2, y2, gr_screen.clip_left, gr_screen.clip_top,
                 gr_screen.clip_right, gr_screen.clip_bottom, return, clipped = 1,
                 swapped = 1);

    gr_opengl_set_state(TEXTURE_SOURCE_NONE, ALPHA_BLEND_ALPHA_BLEND_ALPHA,
                        ZBUFFER_TYPE_NONE);

    int aa = swapped ? 0 : gr_screen.current_color.alpha;
    int ba = swapped ? gr_screen.current_color.alpha : 0;

    float sx1, sy1;
    float sx2, sy2;

    sx1 = i2fl(x1 + gr_screen.offset_x);
    sy1 = i2fl(y1 + gr_screen.offset_y);
    sx2 = i2fl(x2 + gr_screen.offset_x);
    sy2 = i2fl(y2 + gr_screen.offset_y);

    if (x1 == x2) {
        if (sy1 < sy2) {
            sy2 += 0.5f;
        }
        else {
            sy1 += 0.5f;
        }
    }
    else if (y1 == y2) {
        if (sx1 < sx2) {
            sx2 += 0.5f;
        }
        else {
            sx1 += 0.5f;
        }
    }

    glBegin(GL_LINES);
    glColor4ub((ubyte)gr_screen.current_color.red,
               (ubyte)gr_screen.current_color.green,
               (ubyte)gr_screen.current_color.blue, (ubyte)ba);
    glVertex3f(sx2, sy2, -0.99f);
    glColor4ub((ubyte)gr_screen.current_color.red,
               (ubyte)gr_screen.current_color.green,
               (ubyte)gr_screen.current_color.blue, (ubyte)aa);
    glVertex3f(sx1, sy1, -0.99f);
    glEnd();
}

void
gr_opengl_circle(int xc, int yc, int d)
{
    int p, x, y, r;

    r = d / 2;
    p = 3 - d;
    x = 0;
    y = r;

    // Big clip
    if ((xc + r) < gr_screen.clip_left)
        return;
    if ((xc - r) > gr_screen.clip_right)
        return;
    if ((yc + r) < gr_screen.clip_top)
        return;
    if ((yc - r) > gr_screen.clip_bottom)
        return;

    while (x < y) {
        // Draw the first octant
        gr_opengl_line(xc - y, yc - x, xc + y, yc - x);
        gr_opengl_line(xc - y, yc + x, xc + y, yc + x);

        if (p < 0)
            p = p + (x << 2) + 6;
        else {
            // Draw the second octant
            gr_opengl_line(xc - x, yc - y, xc + x, yc - y);
            gr_opengl_line(xc - x, yc + y, xc + x, yc + y);

            p = p + ((x - y) << 2) + 10;
            y--;
        }
        x++;
    }
    if (x == y) {
        gr_opengl_line(xc - x, yc - y, xc + x, yc - y);
        gr_opengl_line(xc - x, yc + y, xc + x, yc + y);
    }
    return;
}

static void
gr_opengl_tmapper_internal(int nv, vertex **verts, uint flags, int is_scaler)
{
    int i;
    float u_scale = 1.0f, v_scale = 1.0f;

    // Make nebula use the texture mapper... this blends the colors better.
    if (flags & TMAP_FLAG_NEBULA) {
        Int3();
    }

    gr_texture_source texture_source = TEXTURE_SOURCE_NONE;
    gr_alpha_blend alpha_blend = ALPHA_BLEND_NONE;
    gr_zbuffer_type zbuffer_type = ZBUFFER_TYPE_NONE;

    if (gr_zbuffering) {
        if (is_scaler ||
            (gr_screen.current_alphablend_mode == GR_ALPHABLEND_FILTER)) {
            zbuffer_type = ZBUFFER_TYPE_READ;
        }
        else {
            zbuffer_type = ZBUFFER_TYPE_FULL;
        }
    }

    int alpha;

    int tmap_type = TCACHE_TYPE_NORMAL;

    int r, g, b;

    if (flags & TMAP_FLAG_TEXTURED) {
        r = g = b = 255;
    }
    else {
        r = gr_screen.current_color.red;
        g = gr_screen.current_color.green;
        b = gr_screen.current_color.blue;
    }

    if (gr_screen.current_alphablend_mode == GR_ALPHABLEND_FILTER) {
        tmap_type = TCACHE_TYPE_NORMAL;
        alpha_blend = ALPHA_BLEND_ALPHA_ADDITIVE;

        // Blend with screen pixel using src*alpha+dst
        float factor = gr_screen.current_alpha;

        alpha = 255;

        if (factor <= 1.0f) {
            int tmp_alpha = fl2i(gr_screen.current_alpha * 255.0f);
            r = (r * tmp_alpha) / 255;
            g = (g * tmp_alpha) / 255;
            b = (b * tmp_alpha) / 255;
        }
    }
    else {
        if (Bm_pixel_format == BM_PIXEL_FORMAT_ARGB) {
            alpha_blend = ALPHA_BLEND_ALPHA_BLEND_ALPHA;
        }
        else {
            alpha_blend = ALPHA_BLEND_NONE;
        }
        alpha = 255;
    }

    if (flags & TMAP_FLAG_BITMAP_SECTION) {
        tmap_type = TCACHE_TYPE_BITMAP_SECTION;
    }

    if (flags & TMAP_FLAG_TEXTURED) {
        if (!gr_tcache_set(gr_screen.current_bitmap, tmap_type, &u_scale,
                           &v_scale, 0, gr_screen.current_bitmap_sx,
                           gr_screen.current_bitmap_sy)) {
            mprintf(("Not rendering a texture because it didn't fit in VRAM!\n"));
            return;
        }

        // use nonfiltered textures for bitmap sections
        if (flags & TMAP_FLAG_BITMAP_SECTION) {
            texture_source = TEXTURE_SOURCE_NO_FILTERING;
        }
        else {
            texture_source = TEXTURE_SOURCE_DECAL;
        }
    }

    gr_opengl_set_state(texture_source, alpha_blend, zbuffer_type);

    if (flags & TMAP_FLAG_PIXEL_FOG) {
        int fr, fg, fb;
        int ra, ga, ba;
        ra = ga = ba = 0;

        for (i = nv - 1; i >= 0; i--) // DDOI - change polygon winding
        {
            vertex *va = verts[i];
            float sx, sy;

            int x, y;
            x = fl2i(va->sx * 16.0f);
            y = fl2i(va->sy * 16.0f);

            x += gr_screen.offset_x * 16;
            y += gr_screen.offset_y * 16;

            sx = i2fl(x) / 16.0f;
            sy = i2fl(y) / 16.0f;

            neb2_get_pixel((int)sx, (int)sy, &fr, &fg, &fb);

            ra += fr;
            ga += fg;
            ba += fb;
        }

        ra /= nv;
        ga /= nv;
        ba /= nv;

        gr_fog_set(GR_FOGMODE_FOG, ra, ga, ba);
    }

    glBegin(GL_TRIANGLE_FAN);
    for (i = nv - 1; i >= 0; i--) {
        vertex *va = verts[i];
        float sx, sy, sz;
        float tu, tv;
        float rhw;
        int a;

        if (gr_zbuffering || (flags & TMAP_FLAG_NEBULA)) {
            sz = float(1.0 - 1.0 / (1.0 + va->z / (32768.0 / 256.0)));

            if (sz > 0.98f) {
                sz = 0.98f;
            }
        }
        else {
            sz = 0.99f;
        }

        if (flags & TMAP_FLAG_CORRECT) {
            rhw = va->sw;
        }
        else {
            rhw = 1.0f;
        }

        if (flags & TMAP_FLAG_ALPHA) {
            a = verts[i]->a;
        }
        else {
            a = alpha;
        }

        if (flags & TMAP_FLAG_NEBULA) {
            int pal = (verts[i]->b * (NEBULA_COLORS - 1)) / 255;
            r = gr_palette[pal * 3 + 0];
            g = gr_palette[pal * 3 + 1];
            b = gr_palette[pal * 3 + 2];
        }
        else if ((flags & TMAP_FLAG_RAMP) && (flags & TMAP_FLAG_GOURAUD)) {
            r = Gr_gamma_lookup[verts[i]->b];
            g = Gr_gamma_lookup[verts[i]->b];
            b = Gr_gamma_lookup[verts[i]->b];
        }
        else if ((flags & TMAP_FLAG_RGB) && (flags & TMAP_FLAG_GOURAUD)) {
            r = Gr_gamma_lookup[verts[i]->r];
            g = Gr_gamma_lookup[verts[i]->g];
            b = Gr_gamma_lookup[verts[i]->b];
        }
        else {
            // use constant RGB values...
        }
        glColor4ub((ubyte)r, (ubyte)g, (ubyte)b, (ubyte)a);

        int x, y;
        x = fl2i(va->sx * 16.0f);
        y = fl2i(va->sy * 16.0f);

        x += gr_screen.offset_x * 16;
        y += gr_screen.offset_y * 16;

        sx = i2fl(x) / 16.0f;
        sy = i2fl(y) / 16.0f;

        if (flags & TMAP_FLAG_TEXTURED) {
            tu = va->u * u_scale;
            tv = va->v * v_scale;
            glTexCoord2f(tu, tv);
        }

        glVertex4f(sx / rhw, sy / rhw, -sz / rhw, 1.0f / rhw);
    }
    glEnd();
}

void
gr_opengl_tmapper(int nverts, vertex **verts, uint flags)
{
    gr_opengl_tmapper_internal(nverts, verts, flags, 0);
}

#define FIND_SCALED_NUM(x, x0, x1, y0, y1)                                       \
    (((((x) - (x0)) * ((y1) - (y0))) / ((x1) - (x0))) + (y0))

void
gr_opengl_scaler(vertex *va, vertex *vb)
{
    float x0, y0, x1, y1;
    float u0, v0, u1, v1;
    float clipped_x0, clipped_y0, clipped_x1, clipped_y1;
    float clipped_u0, clipped_v0, clipped_u1, clipped_v1;
    float xmin, xmax, ymin, ymax;
    int dx0, dy0, dx1, dy1;

    //============= CLIP IT =====================

    x0 = va->sx;
    y0 = va->sy;
    x1 = vb->sx;
    y1 = vb->sy;

    xmin = i2fl(gr_screen.clip_left);
    ymin = i2fl(gr_screen.clip_top);
    xmax = i2fl(gr_screen.clip_right);
    ymax = i2fl(gr_screen.clip_bottom);

    u0 = va->u;
    v0 = va->v;
    u1 = vb->u;
    v1 = vb->v;

    // Check for obviously offscreen bitmaps...
    if ((y1 <= y0) || (x1 <= x0))
        return;
    if ((x1 < xmin) || (x0 > xmax))
        return;
    if ((y1 < ymin) || (y0 > ymax))
        return;

    clipped_u0 = u0;
    clipped_v0 = v0;
    clipped_u1 = u1;
    clipped_v1 = v1;

    clipped_x0 = x0;
    clipped_y0 = y0;
    clipped_x1 = x1;
    clipped_y1 = y1;

    // Clip the left, moving u0 right as necessary
    if (x0 < xmin) {
        clipped_u0 = FIND_SCALED_NUM(xmin, x0, x1, u0, u1);
        clipped_x0 = xmin;
    }

    // Clip the right, moving u1 left as necessary
    if (x1 > xmax) {
        clipped_u1 = FIND_SCALED_NUM(xmax, x0, x1, u0, u1);
        clipped_x1 = xmax;
    }

    // Clip the top, moving v0 down as necessary
    if (y0 < ymin) {
        clipped_v0 = FIND_SCALED_NUM(ymin, y0, y1, v0, v1);
        clipped_y0 = ymin;
    }

    // Clip the bottom, moving v1 up as necessary
    if (y1 > ymax) {
        clipped_v1 = FIND_SCALED_NUM(ymax, y0, y1, v0, v1);
        clipped_y1 = ymax;
    }

    dx0 = fl2i(clipped_x0);
    dx1 = fl2i(clipped_x1);
    dy0 = fl2i(clipped_y0);
    dy1 = fl2i(clipped_y1);

    if (dx1 <= dx0)
        return;
    if (dy1 <= dy0)
        return;

    //============= DRAW IT =====================

    vertex v[4];
    vertex *vl[4];

    vl[0] = &v[0];
    v->sx = clipped_x0;
    v->sy = clipped_y0;
    v->sw = va->sw;
    v->z = va->z;
    v->u = clipped_u0;
    v->v = clipped_v0;

    vl[1] = &v[1];
    v[1].sx = clipped_x1;
    v[1].sy = clipped_y0;
    v[1].sw = va->sw;
    v[1].z = va->z;
    v[1].u = clipped_u1;
    v[1].v = clipped_v0;

    vl[2] = &v[2];
    v[2].sx = clipped_x1;
    v[2].sy = clipped_y1;
    v[2].sw = va->sw;
    v[2].z = va->z;
    v[2].u = clipped_u1;
    v[2].v = clipped_v1;

    vl[3] = &v[3];
    v[3].sx = clipped_x0;
    v[3].sy = clipped_y1;
    v[3].sw = va->sw;
    v[3].z = va->z;
    v[3].u = clipped_u0;
    v[3].v = clipped_v1;

    gr_opengl_tmapper_internal(4, vl, TMAP_FLAG_TEXTURED, 1);
}

void
gr_opengl_aascaler(vertex *va, vertex *vb)
{
    gr_opengl_scaler(va, vb);
}

// draw a bitmap as a one-shot textured quad: lock at 16bpp, upload, draw,
// delete.  Menu screens draw one or two of these per frame, so the immediate
// upload is fine for now; route through the tcache if it ever shows up hot.
static void
gr_opengl_bitmap_internal(int x, int y, int w, int h, int sx, int sy)
{
    unsigned int tex = 0;
    bitmap *bmp;

    int htemp = (int)pow(2, ceil(log10((double)h) / log10(2.0)));
    int wtemp = (int)pow(2, ceil(log10((double)w) / log10(2.0)));

    glGenTextures(1, &tex);

    Assert(tex != 0);

    // XPARENT: converts through the 1555 texture guns (matching the upload
    // format below) and turns the interface art's green transparency key
    // into alpha 0
    bmp = bm_lock(gr_screen.current_bitmap, 16, BMP_TEX_XPARENT);
    if (!bmp)
        return;

    const ushort *sptr = (const ushort *)bmp->data;
    sptr += sy * bmp->w + sx;

    gr_opengl_set_state(TEXTURE_SOURCE_NO_FILTERING,
                        ALPHA_BLEND_ALPHA_BLEND_ALPHA, ZBUFFER_TYPE_NONE);

    glColor3ub(255, 255, 255);

    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, bmp->w);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, wtemp, htemp, 0, GL_BGRA,
                 GL_UNSIGNED_SHORT_1_5_5_5_REV, NULL);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_BGRA,
                    GL_UNSIGNED_SHORT_1_5_5_5_REV, sptr);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    int x1 = x + gr_screen.offset_x;
    int y1 = y + gr_screen.offset_y;

    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2i(x1, y1);

    glTexCoord2f(0, i2fl(h) / i2fl(htemp));
    glVertex2i(x1, y1 + h);

    glTexCoord2f(i2fl(w) / i2fl(wtemp), i2fl(h) / i2fl(htemp));
    glVertex2i(x1 + w, y1 + h);

    glTexCoord2f(i2fl(w) / i2fl(wtemp), 0);
    glVertex2i(x1 + w, y1);
    glEnd();

    bm_unlock(gr_screen.current_bitmap);

    glDeleteTextures(1, &tex);

    // this bound (and deleted) its own texture behind the tcache's back;
    // invalidate the last-bound shortcut or the next cached draw skips its
    // glBindTexture and samples a dead texture
    GL_last_bitmap_id = -1;
}

void
gr_opengl_bitmap_ex(int x, int y, int w, int h, int sx, int sy)
{
    int reclip;
#ifndef NDEBUG
    int count = 0;
#endif

    int dx1 = x, dx2 = x + w - 1;
    int dy1 = y, dy2 = y + h - 1;

    int bw, bh;
    bm_get_info(gr_screen.current_bitmap, &bw, &bh, NULL);

    do {
        reclip = 0;
#ifndef NDEBUG
        if (count > 1)
            Int3();
        count++;
#endif

        if ((dx1 > gr_screen.clip_right) || (dx2 < gr_screen.clip_left))
            return;
        if ((dy1 > gr_screen.clip_bottom) || (dy2 < gr_screen.clip_top))
            return;
        if (dx1 < gr_screen.clip_left) {
            sx += gr_screen.clip_left - dx1;
            dx1 = gr_screen.clip_left;
        }
        if (dy1 < gr_screen.clip_top) {
            sy += gr_screen.clip_top - dy1;
            dy1 = gr_screen.clip_top;
        }
        if (dx2 > gr_screen.clip_right) {
            dx2 = gr_screen.clip_right;
        }
        if (dy2 > gr_screen.clip_bottom) {
            dy2 = gr_screen.clip_bottom;
        }

        if (sx < 0) {
            dx1 -= sx;
            sx = 0;
            reclip = 1;
        }

        if (sy < 0) {
            dy1 -= sy;
            sy = 0;
            reclip = 1;
        }

        w = dx2 - dx1 + 1;
        h = dy2 - dy1 + 1;

        if (sx + w > bw) {
            w = bw - sx;
            dx2 = dx1 + w - 1;
        }

        if (sy + h > bh) {
            h = bh - sy;
            dy2 = dy1 + h - 1;
        }

        if (w < 1)
            return; // clipped away!
        if (h < 1)
            return; // clipped away!

    } while (reclip);

    gr_opengl_bitmap_internal(dx1, dy1, w, h, sx, sy);
}

void
gr_opengl_bitmap(int x, int y)
{
    int w, h;

    bm_get_info(gr_screen.current_bitmap, &w, &h, NULL);
    int dx1 = x, dx2 = x + w - 1;
    int dy1 = y, dy2 = y + h - 1;
    int sx = 0, sy = 0;

    if ((dx1 > gr_screen.clip_right) || (dx2 < gr_screen.clip_left))
        return;
    if ((dy1 > gr_screen.clip_bottom) || (dy2 < gr_screen.clip_top))
        return;
    if (dx1 < gr_screen.clip_left) {
        sx = gr_screen.clip_left - dx1;
        dx1 = gr_screen.clip_left;
    }
    if (dy1 < gr_screen.clip_top) {
        sy = gr_screen.clip_top - dy1;
        dy1 = gr_screen.clip_top;
    }
    if (dx2 > gr_screen.clip_right) {
        dx2 = gr_screen.clip_right;
    }
    if (dy2 > gr_screen.clip_bottom) {
        dy2 = gr_screen.clip_bottom;
    }

    if (sx < 0)
        return;
    if (sy < 0)
        return;
    if (sx >= w)
        return;
    if (sy >= h)
        return;

    // Draw bitmap bm[sx,sy] into (dx1,dy1)-(dx2,dy2)

    gr_bitmap_ex(dx1, dy1, dx2 - dx1 + 1, dy2 - dy1 + 1, sx, sy);
}

void
gr_opengl_set_palette(ubyte *new_palette, int is_alphacolor)
{ }

void
gr_opengl_get_color(int *r, int *g, int *b)
{
    if (r)
        *r = gr_screen.current_color.red;
    if (g)
        *g = gr_screen.current_color.green;
    if (b)
        *b = gr_screen.current_color.blue;
}

void
gr_opengl_init_color(color *c, int r, int g, int b)
{
    c->screen_sig = gr_screen.signature;
    c->red = (unsigned char)r;
    c->green = (unsigned char)g;
    c->blue = (unsigned char)b;
}

// hardware-mode alphacolors carry real rgba on the color struct (the software
// renderer builds palette remap tables instead); same shape as gr_d3d_init_alphacolor
void
gr_opengl_init_alphacolor(color *clr, int r, int g, int b, int alpha, int type)
{
    if (r < 0)
        r = 0;
    else if (r > 255)
        r = 255;
    if (g < 0)
        g = 0;
    else if (g > 255)
        g = 255;
    if (b < 0)
        b = 0;
    else if (b > 255)
        b = 255;
    if (alpha < 0)
        alpha = 0;
    else if (alpha > 255)
        alpha = 255;

    gr_opengl_init_color(clr, r, g, b);

    clr->alpha = (unsigned char)alpha;
    clr->ac_type = (ubyte)type;
    clr->alphacolor = -1;
    clr->is_alphacolor = 1;
}

void
gr_opengl_set_color(int r, int g, int b)
{
    Assert((r >= 0) && (r < 256));
    Assert((g >= 0) && (g < 256));
    Assert((b >= 0) && (b < 256));

    gr_screen.current_color.red = (unsigned char)r;
    gr_screen.current_color.green = (unsigned char)g;
    gr_screen.current_color.blue = (unsigned char)b;
}

void
gr_opengl_set_color_fast(color *dst)
{
    if (dst->screen_sig != gr_screen.signature) {
        gr_init_color(dst, dst->red, dst->green, dst->blue);
        return;
    }
    gr_screen.current_color = *dst;
}

void
gr_opengl_print_screen(char *filename)
{
    int w = gr_screen.max_w, h = gr_screen.max_h;

    ubyte *pixels = (ubyte *)malloc(w * h * 3);
    if (!pixels)
        return;

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_FRONT);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    char path[512];
    snprintf(path, sizeof(path), "%s.ppm", filename);
    FILE *out = fopen(path, "wb");
    if (out) {
        fprintf(out, "P6\n%d %d\n255\n", w, h);
        for (int y = h - 1; y >= 0; y--) { // GL rows are bottom-up
            fwrite(pixels + y * w * 3, 1, w * 3, out);
        }
        fclose(out);
    }
    free(pixels);
}

void
gr_opengl_start_frame()
{ }

void
gr_opengl_stop_frame()
{ }

int
gr_opengl_supports_res_ingame(int res)
{
    return 1;
}

int
gr_opengl_supports_res_interface(int res)
{
    return 1;
}

// texture cache ---------------------------------------------------------

void
opengl_tcache_init(int use_sections)
{
    int i, idx, s_idx;

    GL_should_preload = 1;

    GL_min_texture_width = 16;
    GL_min_texture_height = 16;

    // 2002 hardware capped these at 256 (Voodoo-era); take what GL gives,
    // within reason
    GLint max_texture_size = 256;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
    if (max_texture_size > 4096) {
        max_texture_size = 4096;
    }
    GL_max_texture_width = max_texture_size;
    GL_max_texture_height = max_texture_size;

    GL_square_textures = 0;

    Textures = (tcache_slot_opengl *)malloc(MAX_BITMAPS *
                                            sizeof(tcache_slot_opengl));
    if (!Textures) {
        exit(1);
    }

    if (use_sections) {
        Texture_sections = (tcache_slot_opengl *)malloc(
            MAX_BITMAPS * MAX_BMAP_SECTIONS_X * MAX_BMAP_SECTIONS_Y *
            sizeof(tcache_slot_opengl));
        if (!Texture_sections) {
            exit(1);
        }
        memset(Texture_sections, 0,
               MAX_BITMAPS * MAX_BMAP_SECTIONS_X * MAX_BMAP_SECTIONS_Y *
                   sizeof(tcache_slot_opengl));
    }

    // Init the texture structures
    int section_count = 0;
    for (i = 0; i < MAX_BITMAPS; i++) {
        Textures[i].texture_handle = 0;

        Textures[i].bitmap_id = -1;
        Textures[i].size = 0;
        Textures[i].used_this_frame = 0;

        Textures[i].parent = NULL;

        // allocate sections
        if (use_sections) {
            for (idx = 0; idx < MAX_BMAP_SECTIONS_X; idx++) {
                for (s_idx = 0; s_idx < MAX_BMAP_SECTIONS_Y; s_idx++) {
                    Textures[i].data_sections[idx][s_idx] = &(
                        (tcache_slot_opengl *)Texture_sections)[section_count++];
                    Textures[i].data_sections[idx][s_idx]->parent = &Textures[i];
                    Textures[i].data_sections[idx][s_idx]->texture_handle = 0;
                    Textures[i].data_sections[idx][s_idx]->bitmap_id = -1;
                    Textures[i].data_sections[idx][s_idx]->size = 0;
                    Textures[i].data_sections[idx][s_idx]->used_this_frame = 0;
                }
            }
        }
        else {
            for (idx = 0; idx < MAX_BMAP_SECTIONS_X; idx++) {
                for (s_idx = 0; s_idx < MAX_BMAP_SECTIONS_Y; s_idx++) {
                    Textures[i].data_sections[idx][s_idx] = NULL;
                }
            }
        }
    }

    GL_texture_sections = use_sections;

    GL_last_bitmap_id = -1;
    GL_last_bitmap_type = -1;

    GL_last_section_x = -1;
    GL_last_section_y = -1;

    GL_textures_in = 0;
    GL_textures_in_frame = 0;
}

int opengl_free_texture(tcache_slot_opengl *t);

void
opengl_tcache_flush()
{
    int i;

    for (i = 0; i < MAX_BITMAPS; i++) {
        opengl_free_texture(&Textures[i]);
    }
    if (GL_textures_in != 0) {
        mprintf(("WARNING: VRAM is at %d instead of zero after flushing!\n",
                 GL_textures_in));
        GL_textures_in = 0;
    }

    GL_last_bitmap_id = -1;
    GL_last_section_x = -1;
    GL_last_section_y = -1;
}

void
opengl_tcache_cleanup()
{
    opengl_tcache_flush();

    GL_textures_in = 0;
    GL_textures_in_frame = 0;

    if (Textures) {
        free(Textures);
        Textures = NULL;
    }

    if (Texture_sections != NULL) {
        free(Texture_sections);
        Texture_sections = NULL;
    }
}

void
opengl_tcache_frame()
{
    int idx, s_idx;

    GL_last_bitmap_id = -1;
    GL_textures_in_frame = 0;

    GL_frame_count++;

    int i;
    for (i = 0; i < MAX_BITMAPS; i++) {
        Textures[i].used_this_frame = 0;

        // data sections
        if (Textures[i].data_sections[0][0] != NULL) {
            Assert(GL_texture_sections);
            if (GL_texture_sections) {
                for (idx = 0; idx < MAX_BMAP_SECTIONS_X; idx++) {
                    for (s_idx = 0; s_idx < MAX_BMAP_SECTIONS_Y; s_idx++) {
                        if (Textures[i].data_sections[idx][s_idx] != NULL) {
                            Textures[i]
                                .data_sections[idx][s_idx]
                                ->used_this_frame = 0;
                        }
                    }
                }
            }
        }
    }

    if (vram_full) {
        opengl_tcache_flush();
        vram_full = 0;
    }
}

int
opengl_free_texture(tcache_slot_opengl *t)
{
    int idx, s_idx;

    // Bitmap changed!!
    if (t->bitmap_id > -1) {
        // if I, or any of my children have been used this frame, bail
        if (t->used_this_frame) {
            return 0;
        }
        for (idx = 0; idx < MAX_BMAP_SECTIONS_X; idx++) {
            for (s_idx = 0; s_idx < MAX_BMAP_SECTIONS_Y; s_idx++) {
                if ((t->data_sections[idx][s_idx] != NULL) &&
                    (t->data_sections[idx][s_idx]->used_this_frame)) {
                    return 0;
                }
            }
        }

        // ok, now we know its legal to free everything safely
        if (t->texture_handle) {
            glDeleteTextures(1, &t->texture_handle);
            t->texture_handle = 0;
        }

        if (GL_last_bitmap_id == t->bitmap_id) {
            GL_last_bitmap_id = -1;
        }

        // if this guy has children, free them too, since the children
        // actually make up his size
        for (idx = 0; idx < MAX_BMAP_SECTIONS_X; idx++) {
            for (s_idx = 0; s_idx < MAX_BMAP_SECTIONS_Y; s_idx++) {
                if (t->data_sections[idx][s_idx] != NULL) {
                    opengl_free_texture(t->data_sections[idx][s_idx]);
                }
            }
        }

        t->bitmap_id = -1;
        t->used_this_frame = 0;
        GL_textures_in -= t->size;
    }

    return 1;
}

void
opengl_free_texture_with_handle(int handle)
{
    for (int i = 0; i < MAX_BITMAPS; i++) {
        if (Textures[i].bitmap_id == handle) {
            Textures[i].used_this_frame =
                0; // this bmp doesn't even exist any longer...
            opengl_free_texture(&Textures[i]);
        }
    }
}

void
opengl_tcache_get_adjusted_texture_size(int w_in, int h_in, int *w_out,
                                        int *h_out)
{
    int tex_w, tex_h;

    // bogus
    if ((w_out == NULL) || (h_out == NULL)) {
        return;
    }

    // starting size
    tex_w = w_in;
    tex_h = h_in;

    int i;
    for (i = 0; i < 16; i++) {
        if ((tex_w > (1 << i)) && (tex_w <= (1 << (i + 1)))) {
            tex_w = 1 << (i + 1);
            break;
        }
    }

    for (i = 0; i < 16; i++) {
        if ((tex_h > (1 << i)) && (tex_h <= (1 << (i + 1)))) {
            tex_h = 1 << (i + 1);
            break;
        }
    }

    if (tex_w < GL_min_texture_width) {
        tex_w = GL_min_texture_width;
    }
    else if (tex_w > GL_max_texture_width) {
        tex_w = GL_max_texture_width;
    }

    if (tex_h < GL_min_texture_height) {
        tex_h = GL_min_texture_height;
    }
    else if (tex_h > GL_max_texture_height) {
        tex_h = GL_max_texture_height;
    }

    if (GL_square_textures) {
        int new_size;
        // Make the both be equal to larger of the two
        new_size = max(tex_w, tex_h);
        tex_w = new_size;
        tex_h = new_size;
    }

    // store the outgoing size
    *w_out = tex_w;
    *h_out = tex_h;
}

// data == start of bitmap data
// sx == x offset into bitmap
// sy == y offset into bitmap
// src_w == absolute width of section on source bitmap
// src_h == absolute height of section on source bitmap
// bmap_w == width of source bitmap
// bmap_h == height of source bitmap
// tex_w == width of final texture
// tex_h == height of final texture
int
opengl_create_texture_sub(int bitmap_type, int texture_handle, ushort *data,
                          int sx, int sy, int src_w, int src_h, int bmap_w,
                          int bmap_h, int tex_w, int tex_h, tcache_slot_opengl *t,
                          int reload, int fail_on_full)
{
    int ret_val = 1;

    // bogus
    if (t == NULL) {
        return 0;
    }

    if (t->used_this_frame) {
        mprintf(("ARGHH!!! Texture already used this frame!  Cannot free it!\n"));
        return 0;
    }
    if (!reload) {
        if (!opengl_free_texture(t)) {
            return 0;
        }
    }

    // get final texture size
    opengl_tcache_get_adjusted_texture_size(tex_w, tex_h, &tex_w, &tex_h);

    if ((tex_w < 1) || (tex_h < 1)) {
        mprintf(("Bitmap is to small at %dx%d.\n", tex_w, tex_h));
        return 0;
    }

    if (bitmap_type == TCACHE_TYPE_AABITMAP) {
        t->u_scale = (float)bmap_w / (float)tex_w;
        t->v_scale = (float)bmap_h / (float)tex_h;
    }
    else if (bitmap_type == TCACHE_TYPE_BITMAP_SECTION) {
        t->u_scale = (float)src_w / (float)tex_w;
        t->v_scale = (float)src_h / (float)tex_h;
    }
    else {
        t->u_scale = 1.0f;
        t->v_scale = 1.0f;
    }

    if (!reload) {
        glGenTextures(1, &t->texture_handle);
    }

    if (t->texture_handle == 0) {
        nprintf(("Error", "!!DEBUG!! t->texture_handle == 0"));
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, t->texture_handle);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    switch (bitmap_type) {
    case TCACHE_TYPE_AABITMAP: {
        // white + per-pixel alpha, modulated by the draw color.  The
        // 2002 backend used GL_LUMINANCE_ALPHA; plain RGBA sidesteps
        // legacy-format holes in modern drivers
        int i, j;
        ubyte *bmp_data = ((ubyte *)data);
        ubyte *texmem = (ubyte *)malloc(tex_w * tex_h * 4);
        ubyte *texmemp = texmem;
        ubyte xlat[256];

        // font/HUD data is 4-bit coverage (0..15), gamma-corrected up
        for (i = 0; i < 16; i++) {
            xlat[i] = (ubyte)Gr_gamma_lookup[(i * 255) / 15];
        }
        for (; i < 256; i++) {
            xlat[i] = xlat[0];
        }

        for (i = 0; i < tex_h; i++) {
            for (j = 0; j < tex_w; j++) {
                ubyte a = 0;
                if (i < bmap_h && j < bmap_w) {
                    a = xlat[bmp_data[i * bmap_w + j]];
                }
                *texmemp++ = 0xff;
                *texmemp++ = 0xff;
                *texmemp++ = 0xff;
                *texmemp++ = a;
            }
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_w, tex_h, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, texmem);

        free(texmem);
    } break;
    case TCACHE_TYPE_BITMAP_SECTION: {
        int i, j;
        ubyte *bmp_data = ((ubyte *)data);
        ubyte *texmem = (ubyte *)malloc(tex_w * tex_h * 2);
        ubyte *texmemp = texmem;

        for (i = 0; i < tex_h; i++) {
            for (j = 0; j < tex_w; j++) {
                if (i < src_h && j < src_w) {
                    *texmemp++ = bmp_data[((i + sy) * bmap_w + (j + sx)) * 2 + 0];
                    *texmemp++ = bmp_data[((i + sy) * bmap_w + (j + sx)) * 2 + 1];
                }
                else {
                    *texmemp++ = 0;
                    *texmemp++ = 0;
                }
            }
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_w, tex_h, 0, GL_BGRA,
                     GL_UNSIGNED_SHORT_1_5_5_5_REV, texmem);

        free(texmem);
        break;
    }
    default: {
        int i, j;
        ubyte *bmp_data = ((ubyte *)data);
        ubyte *texmem = (ubyte *)malloc(tex_w * tex_h * 2);
        ubyte *texmemp = texmem;

        fix u, utmp, v, du, dv;

        u = v = 0;

        du = ((bmap_w - 1) * F1_0) / tex_w;
        dv = ((bmap_h - 1) * F1_0) / tex_h;

        for (j = 0; j < tex_h; j++) {
            utmp = u;
            for (i = 0; i < tex_w; i++) {
                *texmemp++ = bmp_data[(f2i(v) * bmap_w + f2i(utmp)) * 2 + 0];
                *texmemp++ = bmp_data[(f2i(v) * bmap_w + f2i(utmp)) * 2 + 1];
                utmp += du;
            }
            v += dv;
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_w, tex_h, 0, GL_BGRA,
                     GL_UNSIGNED_SHORT_1_5_5_5_REV, texmem);

        free(texmem);
        break;
    }
    }

    t->bitmap_id = texture_handle;
    t->time_created = GL_frame_count;
    t->used_this_frame = 0;
    t->size = tex_w * tex_h * 2;
    t->w = (ushort)tex_w;
    t->h = (ushort)tex_h;
    GL_textures_in_frame += t->size;
    if (!reload) {
        GL_textures_in += t->size;
    }

    return ret_val;
}

int
opengl_create_texture(int bitmap_handle, int bitmap_type,
                      tcache_slot_opengl *tslot, int fail_on_full)
{
    ubyte flags;
    bitmap *bmp;
    int final_w, final_h;
    ubyte bpp = 16;
    int reload = 0;

    // setup texture/bitmap flags
    flags = 0;
    switch (bitmap_type) {
    case TCACHE_TYPE_AABITMAP:
        flags |= BMP_AABITMAP;
        bpp = 8;
        break;
    case TCACHE_TYPE_NORMAL:
        flags |= BMP_TEX_OTHER;
    case TCACHE_TYPE_XPARENT:
        flags |= BMP_TEX_XPARENT;
        break;
    case TCACHE_TYPE_NONDARKENING:
        Int3();
        flags |= BMP_TEX_NONDARK;
        break;
    }

    // lock the bitmap into the proper format
    bmp = bm_lock(bitmap_handle, bpp, flags);
    if (bmp == NULL) {
        mprintf(("Couldn't lock bitmap %d.\n", bitmap_handle));
        return 0;
    }

    int max_w = bmp->w;
    int max_h = bmp->h;

    if (bitmap_type != TCACHE_TYPE_AABITMAP) {
        // Detail.hardware_textures goes from 0 to 4.
        max_w /= (16 >> Detail.hardware_textures);
        max_h /= (16 >> Detail.hardware_textures);
    }

    // get final texture size as it will be allocated
    opengl_tcache_get_adjusted_texture_size(max_w, max_h, &final_w, &final_h);

    // if this tcache slot has no bitmap
    if (tslot->bitmap_id < 0) {
        reload = 0;
    }
    // different bitmap altogether - determine if the new one can use the old one's slot
    else if (tslot->bitmap_id != bitmap_handle) {
        if ((final_w == tslot->w) && (final_h == tslot->h)) {
            reload = 1;
        }
        else {
            reload = 0;
        }
    }

    // call the helper
    int ret_val = opengl_create_texture_sub(bitmap_type, bitmap_handle,
                                            (ushort *)bmp->data, 0, 0, bmp->w,
                                            bmp->h, bmp->w, bmp->h, max_w, max_h,
                                            tslot, reload, fail_on_full);

    // unlock the bitmap
    bm_unlock(bitmap_handle);

    return ret_val;
}

int
opengl_create_texture_sectioned(int bitmap_handle, int bitmap_type,
                                tcache_slot_opengl *tslot, int sx, int sy,
                                int fail_on_full)
{
    ubyte flags;
    bitmap *bmp;
    int final_w, final_h;
    int section_x, section_y;
    int reload = 0;

    // setup texture/bitmap flags
    Assert(bitmap_type == TCACHE_TYPE_BITMAP_SECTION);
    if (bitmap_type != TCACHE_TYPE_BITMAP_SECTION) {
        bitmap_type = TCACHE_TYPE_BITMAP_SECTION;
    }
    flags = BMP_TEX_XPARENT;

    // lock the bitmap in the proper format
    bmp = bm_lock(bitmap_handle, 16, flags);
    if (bmp == NULL) {
        mprintf(("Couldn't lock bitmap %d.\n", bitmap_handle));
        return 0;
    }
    // determine the width and height of this section
    bm_get_section_size(bitmap_handle, sx, sy, &section_x, &section_y);

    // get final texture size as it will be allocated as an opengl texture
    opengl_tcache_get_adjusted_texture_size(section_x, section_y, &final_w,
                                            &final_h);

    // if this tcache slot has no bitmap
    if (tslot->bitmap_id < 0) {
        reload = 0;
    }
    // different bitmap altogether - determine if the new one can use the old one's slot
    else if (tslot->bitmap_id != bitmap_handle) {
        if ((final_w == tslot->w) && (final_h == tslot->h)) {
            reload = 1;
        }
        else {
            reload = 0;
        }
    }

    // call the helper
    int ret_val = opengl_create_texture_sub(
        bitmap_type, bitmap_handle, (ushort *)bmp->data, bmp->sections.sx[sx],
        bmp->sections.sy[sy], section_x, section_y, bmp->w, bmp->h, section_x,
        section_y, tslot, reload, fail_on_full);

    // unlock the bitmap
    bm_unlock(bitmap_handle);

    return ret_val;
}

extern int bm_get_cache_slot(int bitmap_id, int separate_ani_frames);
int
gr_opengl_tcache_set(int bitmap_id, int bitmap_type, float *u_scale,
                     float *v_scale, int fail_on_full = 0, int sx = -1,
                     int sy = -1, int force = 0)
{
    bitmap *bmp = NULL;

    int idx, s_idx;
    int ret_val = 1;

    if (bitmap_id < 0) {
        GL_last_bitmap_id = -1;
        return 0;
    }

    if (GL_last_detail != Detail.hardware_textures) {
        GL_last_detail = Detail.hardware_textures;
        opengl_tcache_flush();
    }

    if (vram_full) {
        return 0;
    }

    int n = bm_get_cache_slot(bitmap_id, 1);
    tcache_slot_opengl *t = &Textures[n];

    if ((GL_last_bitmap_id == bitmap_id) &&
        (GL_last_bitmap_type == bitmap_type) && (t->bitmap_id == bitmap_id) &&
        (GL_last_section_x == sx) && (GL_last_section_y == sy)) {
        t->used_this_frame++;

        // mark all children as used
        if (GL_texture_sections) {
            for (idx = 0; idx < MAX_BMAP_SECTIONS_X; idx++) {
                for (s_idx = 0; s_idx < MAX_BMAP_SECTIONS_Y; s_idx++) {
                    if (t->data_sections[idx][s_idx] != NULL) {
                        t->data_sections[idx][s_idx]->used_this_frame++;
                    }
                }
            }
        }

        *u_scale = t->u_scale;
        *v_scale = t->v_scale;
        return 1;
    }

    if (bitmap_type == TCACHE_TYPE_BITMAP_SECTION) {
        Assert((sx >= 0) && (sy >= 0) && (sx < MAX_BMAP_SECTIONS_X) &&
               (sy < MAX_BMAP_SECTIONS_Y));
        if (!((sx >= 0) && (sy >= 0) && (sx < MAX_BMAP_SECTIONS_X) &&
              (sy < MAX_BMAP_SECTIONS_Y))) {
            return 0;
        }

        ret_val = 1;

        // if the texture sections haven't been created yet
        if ((t->bitmap_id < 0) || (t->bitmap_id != bitmap_id)) {
            // lock the bitmap in the proper format
            bmp = bm_lock(bitmap_id, 16, BMP_TEX_XPARENT);
            bm_unlock(bitmap_id);

            // now lets do something for each texture

            for (idx = 0; idx < bmp->sections.num_x; idx++) {
                for (s_idx = 0; s_idx < bmp->sections.num_y; s_idx++) {
                    // hmm. i'd rather we didn't have to do it this way...
                    if (!opengl_create_texture_sectioned(
                            bitmap_id, bitmap_type, t->data_sections[idx][s_idx],
                            idx, s_idx, fail_on_full)) {
                        ret_val = 0;
                    }

                    // not used this frame
                    t->data_sections[idx][s_idx]->used_this_frame = 0;
                }
            }

            // zero out pretty much everything in the parent struct since he's just the root
            t->bitmap_id = bitmap_id;
            t->texture_handle = 0;
            t->time_created = t->data_sections[sx][sy]->time_created;
            t->used_this_frame = 0;
        }

        // argh. we failed to upload. free anything we can
        if (!ret_val) {
            opengl_free_texture(t);
        }
        // swap in the texture we want
        else {
            t = t->data_sections[sx][sy];
        }
    }

    // all other "normal" textures
    else if ((bitmap_id < 0) || (bitmap_id != t->bitmap_id)) {
        ret_val = opengl_create_texture(bitmap_id, bitmap_type, t, fail_on_full);
    }

    // everything went ok
    if (ret_val && (t->texture_handle) && !vram_full) {
        *u_scale = t->u_scale;
        *v_scale = t->v_scale;

        glBindTexture(GL_TEXTURE_2D, t->texture_handle);

        GL_last_bitmap_id = t->bitmap_id;
        GL_last_bitmap_type = bitmap_type;
        GL_last_section_x = sx;
        GL_last_section_y = sy;

        t->used_this_frame++;
    }
    // gah
    else {
        glBindTexture(GL_TEXTURE_2D, 0);
        return 0;
    }

    return 1;
}

// ------------------------------------------------------------------------

void
gr_opengl_set_clear_color(int r, int g, int b)
{
    gr_init_color(&gr_screen.current_clear_color, r, g, b);
}

int
gr_opengl_zbuffer_get()
{
    if (!gr_global_zbuffering) {
        return GR_ZBUFF_NONE;
    }
    return gr_zbuffering_mode;
}

int
gr_opengl_zbuffer_set(int mode)
{
    int tmp = gr_zbuffering_mode;

    gr_zbuffering_mode = mode;

    if (gr_zbuffering_mode == GR_ZBUFF_NONE) {
        gr_zbuffering = 0;
    }
    else {
        gr_zbuffering = 1;
    }
    return tmp;
}

void
gr_opengl_zbuffer_clear(int mode)
{
    if (mode) {
        gr_zbuffering = 1;
        gr_zbuffering_mode = GR_ZBUFF_FULL;
        gr_global_zbuffering = 1;

        gr_opengl_set_state(TEXTURE_SOURCE_NONE, ALPHA_BLEND_NONE,
                            ZBUFFER_TYPE_FULL);
        glClear(GL_DEPTH_BUFFER_BIT);
    }
    else {
        gr_zbuffering = 0;
        gr_zbuffering_mode = GR_ZBUFF_NONE;
        gr_global_zbuffering = 0;
    }
}

void
gr_opengl_set_gamma(float gamma)
{
    Gr_gamma = gamma;
    Gr_gamma_int = int(Gr_gamma * 100);

    // Create the Gamma lookup table
    int i;
    for (i = 0; i < 256; i++) {
        int v = fl2i(pow(i2fl(i) / 255.0f, 1.0f / Gr_gamma) * 255.0f);
        if (v > 255) {
            v = 255;
        }
        else if (v < 0) {
            v = 0;
        }
        Gr_gamma_lookup[i] = v;
    }

    // Flush any existing textures so they get rebuilt through the new LUT
    opengl_tcache_flush();

    gr_screen.signature = Gr_signature++;
}

void
gr_opengl_fade_in(int instantaneous)
{ }

void
gr_opengl_fade_out(int instantaneous)
{ }

void
gr_opengl_get_region(int front, int w, int h, ubyte *data)
{
    if (front) {
        glReadBuffer(GL_FRONT);
    }
    else {
        glReadBuffer(GL_BACK);
    }

    gr_opengl_set_state(TEXTURE_SOURCE_NO_FILTERING, ALPHA_BLEND_NONE,
                        ZBUFFER_TYPE_NONE);

    glPixelStorei(GL_PACK_ROW_LENGTH, gr_screen.max_w);

    glReadPixels(0, gr_screen.max_h - h - 1, w, h, GL_BGRA,
                 GL_UNSIGNED_SHORT_1_5_5_5_REV, data);

    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
}

int
gr_opengl_save_screen()
{
    gr_reset_clip();

    int bypp = gr_screen.bytes_per_pixel;

    if (Gr_saved_screen) {
        mprintf(("Screen alread saved!\n"));
        return -1;
    }

    Gr_saved_screen = (char *)malloc(gr_screen.max_w * gr_screen.max_h * bypp);
    if (!Gr_saved_screen) {
        mprintf(("Couldn't get memory for saved screen!\n"));
        return -1;
    }

    char *Gr_saved_screen_tmp = (char *)malloc(gr_screen.max_w * gr_screen.max_h *
                                               bypp);
    if (!Gr_saved_screen_tmp) {
        free(Gr_saved_screen);
        Gr_saved_screen = NULL;
        mprintf(("Couldn't get memory for temporary saved screen!\n"));
        return -1;
    }

    gr_opengl_set_state(TEXTURE_SOURCE_NO_FILTERING, ALPHA_BLEND_NONE,
                        ZBUFFER_TYPE_NONE);

    // read the last finished frame from the flip-time stash (GL_FRONT is
    // undefined under a compositor), and freeze it while the popup owns it
    if (GL_screen_stash_tex == 0) {
        free(Gr_saved_screen_tmp);
        free(Gr_saved_screen);
        Gr_saved_screen = NULL;
        mprintf(("No screen stash yet!\n"));
        return -1;
    }
    glBindTexture(GL_TEXTURE_2D, GL_screen_stash_tex);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV,
                  Gr_saved_screen_tmp);
    GL_last_bitmap_id = -1;
    GL_screen_stash_frozen = 1;

    // flip rows: GL reads bottom-up, the bitmap wants top-down
    ubyte *sptr, *dptr;

    sptr = (ubyte *)&Gr_saved_screen_tmp[gr_screen.max_w * gr_screen.max_h * bypp];
    dptr = (ubyte *)Gr_saved_screen;
    for (int j = 0; j < gr_screen.max_h; j++) {
        sptr -= gr_screen.max_w * bypp;
        memcpy(dptr, sptr, gr_screen.max_w * bypp);
        dptr += gr_screen.max_w * bypp;
    }

    free(Gr_saved_screen_tmp);

    Gr_saved_screen_bitmap = bm_create(16, gr_screen.max_w, gr_screen.max_h,
                                       Gr_saved_screen, 0);

    return Gr_saved_screen_bitmap;
}

void
gr_opengl_restore_screen(int id)
{
    gr_reset_clip();

    if (!Gr_saved_screen) {
        gr_clear();
        return;
    }

    gr_opengl_set_state(TEXTURE_SOURCE_NO_FILTERING, ALPHA_BLEND_NONE,
                        ZBUFFER_TYPE_NONE);

    gr_set_bitmap(Gr_saved_screen_bitmap);
    gr_bitmap(0, 0);
}

void
gr_opengl_free_screen(int id)
{
    GL_screen_stash_frozen = 0;

    if (!Gr_saved_screen)
        return;

    bm_release(Gr_saved_screen_bitmap);

    free(Gr_saved_screen);
    Gr_saved_screen = NULL;
}

void
gr_opengl_dump_frame_start(int first_frame_number, int nframes_between_dumps)
{ }

void
gr_opengl_dump_frame()
{ }

void
gr_opengl_dump_frame_stop()
{ }

uint
gr_opengl_lock()
{
    return 1;
}

void
gr_opengl_unlock()
{ }

void
gr_opengl_fog_set(int fog_mode, int r, int g, int b, float fog_near,
                  float fog_far)
{
    Assert((r >= 0) && (r < 256));
    Assert((g >= 0) && (g < 256));
    Assert((b >= 0) && (b < 256));

    if (fog_mode == GR_FOGMODE_NONE) {
        if (gr_screen.current_fog_mode != fog_mode) {
            glDisable(GL_FOG);
        }
        gr_screen.current_fog_mode = fog_mode;

        return;
    }

    if (gr_screen.current_fog_mode != fog_mode) {
        glEnable(GL_FOG);
        glFogi(GL_FOG_MODE, GL_EXP2);
        glFogf(GL_FOG_DENSITY, 0.6f);

        gr_screen.current_fog_mode = fog_mode;
    }

    if ((gr_screen.current_fog_color.red != r) ||
        (gr_screen.current_fog_color.green != g) ||
        (gr_screen.current_fog_color.blue != b)) {
        GLfloat fc[4];

        gr_opengl_init_color(&gr_screen.current_fog_color, r, g, b);

        fc[0] = (float)r / 255.0f;
        fc[1] = (float)g / 255.0f;
        fc[2] = (float)b / 255.0f;
        fc[3] = 1.0f;

        glFogfv(GL_FOG_COLOR, fc);
    }

    if ((fog_near >= 0.0f) && (fog_far >= 0.0f) &&
        ((fog_near != gr_screen.fog_near) || (fog_far != gr_screen.fog_far))) {
        gr_screen.fog_near = fog_near;
        gr_screen.fog_far = fog_far;
    }
}

void
gr_opengl_get_pixel(int x, int y, int *r, int *g, int *b)
{ }

void
gr_opengl_set_cull(int cull)
{
    if (cull) {
        glEnable(GL_CULL_FACE);
        glFrontFace(GL_CCW);
    }
    else {
        glDisable(GL_CULL_FACE);
    }
}

void
gr_opengl_filter_set(int filter)
{ }

// cross fade
void
gr_opengl_cross_fade(int bmap1, int bmap2, int x1, int y1, int x2, int y2,
                     float pct)
{
    if (pct <= 50) {
        gr_set_bitmap(bmap1);
        gr_bitmap(x1, y1);
    }
    else {
        gr_set_bitmap(bmap2);
        gr_bitmap(x2, y2);
    }
}

void
gr_opengl_activate(int active)
{ }

void
gr_opengl_force_windowed()
{ }

void
gr_opengl_cleanup()
{
    if (!Inited)
        return;

    gr_reset_clip();
    gr_clear();
    gr_flip();

    Inited = 0;

    opengl_tcache_cleanup();

    if (GL_context) {
        SDL_GL_DeleteContext(GL_context);
        GL_context = NULL;
    }
}

void
gr_opengl_init()
{
    if (Inited) {
        gr_opengl_cleanup();
        Inited = 0;
    }

    mprintf(("Initializing opengl graphics device...\n"));

    // context attributes must be set before the window is created
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    if (os_create_window(gr_screen.max_w, gr_screen.max_h, 1)) {
        Error(LOCATION, "Can't create window for OpenGL");
    }

    GL_context = SDL_GL_CreateContext(os_get_sdl_window());
    if (!GL_context) {
        Error(LOCATION, "SDL_GL_CreateContext failed: %s", SDL_GetError());
    }

    SDL_GL_SetSwapInterval(1);

    mprintf(("  Vendor   : %s\n", (const char *)glGetString(GL_VENDOR)));
    mprintf(("  Renderer : %s\n", (const char *)glGetString(GL_RENDERER)));
    mprintf(("  Version  : %s\n", (const char *)glGetString(GL_VERSION)));

    // hardware mode is 16bpp to the rest of the game.  Framebuffer guns are
    // 565, texture guns 1555 (matches the GL_BGRA + 1_5_5_5_REV uploads),
    // alpha-texture guns 4444 -- same as the 2002 backend reported.
    Gr_red.bits = 5;
    Gr_red.shift = 11;
    Gr_red.scale = 8;
    Gr_red.mask = 0xF800;
    Gr_green.bits = 6;
    Gr_green.shift = 5;
    Gr_green.scale = 4;
    Gr_green.mask = 0x7E0;
    Gr_blue.bits = 5;
    Gr_blue.shift = 0;
    Gr_blue.scale = 8;
    Gr_blue.mask = 0x1F;

    Gr_t_red.bits = 5;
    Gr_t_red.shift = 10;
    Gr_t_red.scale = 8;
    Gr_t_red.mask = 0x7C00;
    Gr_t_green.bits = 5;
    Gr_t_green.shift = 5;
    Gr_t_green.scale = 8;
    Gr_t_green.mask = 0x03e0;
    Gr_t_blue.bits = 5;
    Gr_t_blue.shift = 0;
    Gr_t_blue.scale = 8;
    Gr_t_blue.mask = 0x001f;
    Gr_t_alpha.bits = 1;
    Gr_t_alpha.shift = 15;
    Gr_t_alpha.scale = 255;
    Gr_t_alpha.mask = 0x8000;

    Gr_ta_red.bits = 4;
    Gr_ta_red.shift = 8;
    Gr_ta_red.scale = 17;
    Gr_ta_red.mask = 0x0f00;
    Gr_ta_green.bits = 4;
    Gr_ta_green.shift = 4;
    Gr_ta_green.scale = 17;
    Gr_ta_green.mask = 0x00f0;
    Gr_ta_blue.bits = 4;
    Gr_ta_blue.shift = 0;
    Gr_ta_blue.scale = 17;
    Gr_ta_blue.mask = 0x000f;
    Gr_ta_alpha.bits = 4;
    Gr_ta_alpha.shift = 12;
    Gr_ta_alpha.scale = 17;
    Gr_ta_alpha.mask = 0xf000;

    Gr_current_red = &Gr_red;
    Gr_current_green = &Gr_green;
    Gr_current_blue = &Gr_blue;
    Gr_current_alpha = &Gr_alpha;

    // every bitmap is a textured poly under GL, so pixel producers that key
    // off this (anim unpack, options detail previews) must pack with the
    // texture guns.  Retail D3D set it for its 32-bit textured-bitmap mode;
    // its 16-bit mode blitted surfaces in screen format instead -- we have
    // no surface blits at all
    Gr_bitmap_poly = 1;

    gr_screen.bits_per_pixel = 16;
    gr_screen.bytes_per_pixel = 2;

    glViewport(0, 0, gr_screen.max_w, gr_screen.max_h);

    // 2D screen coordinates, top-left origin, y down
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, gr_screen.max_w, gr_screen.max_h, 0, 0.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glShadeModel(GL_SMOOTH);
    glEnable(GL_DITHER);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glHint(GL_FOG_HINT, GL_NICEST);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);

    glEnable(GL_TEXTURE_2D);

    glDepthRange(0.0, 1.0);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glFlush();

    Bm_pixel_format = BM_PIXEL_FORMAT_ARGB;

    opengl_tcache_init(0);

    Inited = 1;

    gr_screen.gf_flip = gr_opengl_flip;
    gr_screen.gf_flip_window = gr_opengl_flip_window;
    gr_screen.gf_set_clip = gr_opengl_set_clip;
    gr_screen.gf_reset_clip = gr_opengl_reset_clip;
    gr_screen.gf_set_font = grx_set_font;
    gr_screen.gf_set_color = gr_opengl_set_color;
    gr_screen.gf_set_bitmap = gr_opengl_set_bitmap;
    gr_screen.gf_create_shader = gr_opengl_create_shader;
    gr_screen.gf_set_shader = gr_opengl_set_shader;
    gr_screen.gf_clear = gr_opengl_clear;
    // gr_screen.gf_bitmap = gr_opengl_bitmap;
    // gr_screen.gf_bitmap_ex = gr_opengl_bitmap_ex;

    gr_screen.gf_aabitmap = gr_opengl_aabitmap;
    gr_screen.gf_aabitmap_ex = gr_opengl_aabitmap_ex;

    gr_screen.gf_rect = gr_opengl_rect;
    gr_screen.gf_shade = gr_opengl_shade;
    gr_screen.gf_string = gr_opengl_string;
    gr_screen.gf_circle = gr_opengl_circle;

    gr_screen.gf_line = gr_opengl_line;
    gr_screen.gf_aaline = gr_opengl_aaline;
    gr_screen.gf_pixel = gr_opengl_pixel;
    gr_screen.gf_scaler = gr_opengl_scaler;
    gr_screen.gf_aascaler = gr_opengl_aascaler;
    gr_screen.gf_tmapper = gr_opengl_tmapper;

    gr_screen.gf_gradient = gr_opengl_gradient;

    gr_screen.gf_set_palette = gr_opengl_set_palette;
    gr_screen.gf_get_color = gr_opengl_get_color;
    gr_screen.gf_init_color = gr_opengl_init_color;
    gr_screen.gf_init_alphacolor = gr_opengl_init_alphacolor;
    gr_screen.gf_set_color_fast = gr_opengl_set_color_fast;
    gr_screen.gf_print_screen = gr_opengl_print_screen;
    gr_screen.gf_start_frame = gr_opengl_start_frame;
    gr_screen.gf_stop_frame = gr_opengl_stop_frame;

    gr_screen.gf_fade_in = gr_opengl_fade_in;
    gr_screen.gf_fade_out = gr_opengl_fade_out;
    gr_screen.gf_flash = gr_opengl_flash;

    gr_screen.gf_zbuffer_get = gr_opengl_zbuffer_get;
    gr_screen.gf_zbuffer_set = gr_opengl_zbuffer_set;
    gr_screen.gf_zbuffer_clear = gr_opengl_zbuffer_clear;

    gr_screen.gf_save_screen = gr_opengl_save_screen;
    gr_screen.gf_restore_screen = gr_opengl_restore_screen;
    gr_screen.gf_free_screen = gr_opengl_free_screen;

    gr_screen.gf_dump_frame_start = gr_opengl_dump_frame_start;
    gr_screen.gf_dump_frame_stop = gr_opengl_dump_frame_stop;
    gr_screen.gf_dump_frame = gr_opengl_dump_frame;

    gr_screen.gf_set_gamma = gr_opengl_set_gamma;

    gr_screen.gf_lock = gr_opengl_lock;
    gr_screen.gf_unlock = gr_opengl_unlock;

    gr_screen.gf_get_region = gr_opengl_get_region;

    gr_screen.gf_fog_set = gr_opengl_fog_set;

    gr_screen.gf_get_pixel = gr_opengl_get_pixel;

    gr_screen.gf_set_cull = gr_opengl_set_cull;

    gr_screen.gf_cross_fade = gr_opengl_cross_fade;

    gr_screen.gf_filter_set = gr_opengl_filter_set;

    gr_screen.gf_tcache_set = gr_opengl_tcache_set;

    gr_screen.gf_set_clear_color = gr_opengl_set_clear_color;

    Mouse_hidden++;
    gr_reset_clip();
    gr_clear();
    gr_flip();
    Mouse_hidden--;
}
