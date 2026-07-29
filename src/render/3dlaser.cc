/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#include <graphics/2d.hh>
#include <render/3d.hh>
#include <render/3dinternal.hh>
#include <globalincs/systemvars.hh>
#include <io/key.hh>

int Lasers = 1;
DCF_BOOL(lasers, Lasers);

// This assumes you have already set a color with gr_set_color or gr_set_color_fast
// and a bitmap with gr_set_bitmap.  If it is very far away, it draws the laser
// as flat-shaded using current color, else textured using current texture.
// If max_len is > 1.0, then this caps the length to be no longer than max_len pixels.
float
g3_draw_laser(vector *headp, float head_width, vector *tailp, float tail_width,
              uint tmap_flags, float max_len)
{
    if (!Lasers) {
        return 0.0f;
    }

    float headx, heady, headr, tailx, taily, tailr;
    vertex pt1, pt2;
    float depth;

    Assert(G3_count == 1);

    g3_rotate_vertex(&pt1, headp);

    g3_project_vertex(&pt1);
    if (pt1.flags & PF_OVERFLOW)
        return 0.0f;

    g3_rotate_vertex(&pt2, tailp);

    g3_project_vertex(&pt2);
    if (pt2.flags & PF_OVERFLOW)
        return 0.0f;

    if ((pt1.codes & pt2.codes) != 0) {
        // Both off the same side
        return 0.0f;
    }

    headx = pt1.sx;
    heady = pt1.sy;
    headr = (head_width * Matrix_scale.x * Canv_w2 * pt1.sw);

    tailx = pt2.sx;
    taily = pt2.sy;
    tailr = (tail_width * Matrix_scale.x * Canv_w2 * pt2.sw);

    float len_2d = fl_sqrt((tailx - headx) * (tailx - headx) +
                           (taily - heady) * (taily - heady));

    // Cap the length if needed.
    if ((max_len > 1.0f) && (len_2d > max_len)) {
        float ratio = max_len / len_2d;

        tailx = headx + (tailx - headx) * ratio;
        taily = heady + (taily - heady) * ratio;
        tailr = headr + (tailr - headr) * ratio;

        len_2d = fl_sqrt((tailx - headx) * (tailx - headx) +
                         (taily - heady) * (taily - heady));
    }

    depth = (pt1.z + pt2.z) * 0.5f;

    float max_r = headr;
    float a;
    if (tailr > max_r)
        max_r = tailr;

    if (max_r < 1.0f)
        max_r = 1.0f;

    float mx, my, w, h1, h2;

    if (len_2d < max_r) {
        h1 = headr + (max_r - len_2d);
        if (h1 > max_r)
            h1 = max_r;
        h2 = tailr + (max_r - len_2d);
        if (h2 > max_r)
            h2 = max_r;

        len_2d = max_r;
        if (fl_abs(tailx - headx) > 0.01f) {
            a = (float)atan2(taily - heady, tailx - headx);
        }
        else {
            a = 0.0f;
        }

        w = len_2d;
    }
    else {
        a = atan2_safe(taily - heady, tailx - headx);

        w = len_2d;

        h1 = headr;
        h2 = tailr;
    }

    mx = (tailx + headx) / 2.0f;
    my = (taily + heady) / 2.0f;

    //   gr_set_color(255,0,0);
    //   g3_draw_line( &pt1, &pt2 );

    //   gr_set_color( 255, 0, 0 );
    //   gr_pixel( fl2i(mx),fl2i(my) );

    // Draw box with width 'w' and height 'h' at angle 'a' from horizontal
    // centered around mx, my

    if (h1 < 1.0f)
        h1 = 1.0f;
    if (h2 < 1.0f)
        h2 = 1.0f;

    float sa, ca;

    sa = (float)sin(a);
    ca = (float)cos(a);

    vertex v[4];
    vertex *vertlist[4] = { &v[3], &v[2], &v[1], &v[0] };

    if (depth < 0.0f)
        depth = 0.0f;

    v[0].sx = (-w / 2.0f) * ca + (-h1 / 2.0f) * sa + mx;
    v[0].sy = (-w / 2.0f) * sa - (-h1 / 2.0f) * ca + my;
    v[0].z = pt1.z;
    v[0].sw = pt1.sw;
    v[0].u = 0.0f;
    v[0].v = 0.0f;
    v[0].b = 191;

    v[1].sx = (w / 2.0f) * ca + (-h2 / 2.0f) * sa + mx;
    v[1].sy = (w / 2.0f) * sa - (-h2 / 2.0f) * ca + my;
    v[1].z = pt2.z;
    v[1].sw = pt2.sw;
    v[1].u = 1.0f;
    v[1].v = 0.0f;
    v[1].b = 191;

    v[2].sx = (w / 2.0f) * ca + (h2 / 2.0f) * sa + mx;
    v[2].sy = (w / 2.0f) * sa - (h2 / 2.0f) * ca + my;
    v[2].z = pt2.z;
    v[2].sw = pt2.sw;
    v[2].u = 1.0f;
    v[2].v = 1.0f;
    v[2].b = 191;

    v[3].sx = (-w / 2.0f) * ca + (h1 / 2.0f) * sa + mx;
    v[3].sy = (-w / 2.0f) * sa - (h1 / 2.0f) * ca + my;
    v[3].z = pt1.z;
    v[3].sw = pt1.sw;
    v[3].u = 0.0f;
    v[3].v = 1.0f;
    v[3].b = 191;

    gr_tmapper(4, vertlist, tmap_flags | TMAP_FLAG_CORRECT);

    return depth;
}

// Draw a laser shaped 3d looking thing using vertex coloring (useful for things like colored laser glows)
// If max_len is > 1.0, then this caps the length to be no longer than max_len pixels
float
g3_draw_laser_rgb(vector *headp, float head_width, vector *tailp,
                  float tail_width, int r, int g, int b, uint tmap_flags,
                  float max_len)
{
    if (!Lasers) {
        return 0.0f;
    }

    float headx, heady, headr, tailx, taily, tailr;
    vertex pt1, pt2;
    float depth;

    Assert(G3_count == 1);

    g3_rotate_vertex(&pt1, headp);

    g3_project_vertex(&pt1);
    if (pt1.flags & PF_OVERFLOW)
        return 0.0f;

    g3_rotate_vertex(&pt2, tailp);

    g3_project_vertex(&pt2);
    if (pt2.flags & PF_OVERFLOW)
        return 0.0f;

    if ((pt1.codes & pt2.codes) != 0) {
        // Both off the same side
        return 0.0f;
    }

    headx = pt1.sx;
    heady = pt1.sy;
    headr = (head_width * Matrix_scale.x * Canv_w2 * pt1.sw);

    tailx = pt2.sx;
    taily = pt2.sy;
    tailr = (tail_width * Matrix_scale.x * Canv_w2 * pt2.sw);

    float len_2d = fl_sqrt((tailx - headx) * (tailx - headx) +
                           (taily - heady) * (taily - heady));

    // Cap the length if needed.
    if ((max_len > 1.0f) && (len_2d > max_len)) {
        float ratio = max_len / len_2d;

        tailx = headx + (tailx - headx) * ratio;
        taily = heady + (taily - heady) * ratio;
        tailr = headr + (tailr - headr) * ratio;

        len_2d = fl_sqrt((tailx - headx) * (tailx - headx) +
                         (taily - heady) * (taily - heady));
    }

    depth = (pt1.z + pt2.z) * 0.5f;

    float max_r = headr;
    float a;
    if (tailr > max_r)
        max_r = tailr;

    if (max_r < 1.0f)
        max_r = 1.0f;

    float mx, my, w, h1, h2;

    if (len_2d < max_r) {
        h1 = headr + (max_r - len_2d);
        if (h1 > max_r)
            h1 = max_r;
        h2 = tailr + (max_r - len_2d);
        if (h2 > max_r)
            h2 = max_r;

        len_2d = max_r;
        if (fl_abs(tailx - headx) > 0.01f) {
            a = (float)atan2(taily - heady, tailx - headx);
        }
        else {
            a = 0.0f;
        }

        w = len_2d;
    }
    else {
        a = atan2_safe(taily - heady, tailx - headx);

        w = len_2d;

        h1 = headr;
        h2 = tailr;
    }

    mx = (tailx + headx) / 2.0f;
    my = (taily + heady) / 2.0f;

    //   gr_set_color(255,0,0);
    //   g3_draw_line( &pt1, &pt2 );

    //   gr_set_color( 255, 0, 0 );
    //   gr_pixel( fl2i(mx),fl2i(my) );

    // Draw box with width 'w' and height 'h' at angle 'a' from horizontal
    // centered around mx, my

    if (h1 < 1.0f)
        h1 = 1.0f;
    if (h2 < 1.0f)
        h2 = 1.0f;

    float sa, ca;

    sa = (float)sin(a);
    ca = (float)cos(a);

    vertex v[4];
    vertex *vertlist[4] = { &v[3], &v[2], &v[1], &v[0] };

    if (depth < 0.0f)
        depth = 0.0f;

    v[0].sx = (-w / 2.0f) * ca + (-h1 / 2.0f) * sa + mx;
    v[0].sy = (-w / 2.0f) * sa - (-h1 / 2.0f) * ca + my;
    v[0].z = pt1.z;
    v[0].sw = pt1.sw;
    v[0].u = 0.0f;
    v[0].v = 0.0f;
    v[0].r = (ubyte)r;
    v[0].g = (ubyte)g;
    v[0].b = (ubyte)b;
    v[0].a = 255;

    v[1].sx = (w / 2.0f) * ca + (-h2 / 2.0f) * sa + mx;
    v[1].sy = (w / 2.0f) * sa - (-h2 / 2.0f) * ca + my;
    v[1].z = pt2.z;
    v[1].sw = pt2.sw;
    v[1].u = 1.0f;
    v[1].v = 0.0f;
    v[1].r = (ubyte)r;
    v[1].g = (ubyte)g;
    v[1].b = (ubyte)b;
    v[1].a = 255;

    v[2].sx = (w / 2.0f) * ca + (h2 / 2.0f) * sa + mx;
    v[2].sy = (w / 2.0f) * sa - (h2 / 2.0f) * ca + my;
    v[2].z = pt2.z;
    v[2].sw = pt2.sw;
    v[2].u = 1.0f;
    v[2].v = 1.0f;
    v[2].r = (ubyte)r;
    v[2].g = (ubyte)g;
    v[2].b = (ubyte)b;
    v[2].a = 255;

    v[3].sx = (-w / 2.0f) * ca + (h1 / 2.0f) * sa + mx;
    v[3].sy = (-w / 2.0f) * sa - (h1 / 2.0f) * ca + my;
    v[3].z = pt1.z;
    v[3].sw = pt1.sw;
    v[3].u = 0.0f;
    v[3].v = 1.0f;
    v[3].r = (ubyte)r;
    v[3].g = (ubyte)g;
    v[3].b = (ubyte)b;
    v[3].a = 255;

    gr_tmapper(4, vertlist,
               tmap_flags | TMAP_FLAG_RGB | TMAP_FLAG_GOURAUD |
                   TMAP_FLAG_CORRECT);

    return depth;
}
