/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#include <math/vecmat.hh>
#include <graphics/tmapper.hh>
#include <graphics/2d.hh>
#include <render/3d.hh>
#include <bmpman/bmpman.hh>
#include <model/model.hh>
#include <io/key.hh>
#include <physics/physics.hh>
#include <math/floating.hh>
#include <model/model.hh>
#include <lighting/lighting.hh>
#include <object/object.hh>
#include <ship/ship.hh>
#include <globalincs/systemvars.hh>
#include <anim/animplay.hh>
#include <fireball/fireballs.hh>
#include <globalincs/linklist.hh>
#include <io/timer.hh>

DCF(norm, "normalize a zero length vector")
{
    if (Dc_command) {
        vector tmp = vmd_zero_vector;
        vm_vec_normalize(&tmp);
    }
}

void
draw_face(vertex *v1, vertex *v2, vertex *v3)
{
    vector norm;
    vertex *vertlist[3];

    vm_vec_perp(&norm, (vector *)&v1->x, (vector *)&v2->x, (vector *)&v3->x);
    if (vm_vec_dot(&norm, (vector *)&v1->x) >= 0.0) {
        vertlist[0] = v3;
        vertlist[1] = v2;
        vertlist[2] = v1;
    }
    else {
        vertlist[0] = v1;
        vertlist[1] = v2;
        vertlist[2] = v3;
    }

    g3_draw_poly(3, vertlist, TMAP_FLAG_TEXTURED);
}

void
warpin_render(matrix *orient, vector *pos, int texture_bitmap_num, float radius,
              float life_percent, float max_radius)
{
    int i;

    int saved_gr_zbuffering = gr_zbuffer_get();

    //	gr_zbuffering = 0;

    gr_set_bitmap(texture_bitmap_num, GR_ALPHABLEND_FILTER, GR_BITBLT_MODE_NORMAL,
                  1.0f);

    float Grid_depth = radius / 2.5f;

    vector center;

    vm_vec_scale_add(&center, pos, &orient->fvec, -(max_radius / 2.5f) / 3.0f);

    vector vecs[5];
    vertex verts[5];

    vm_vec_scale_add(&vecs[0], &center, &orient->uvec, radius);
    vm_vec_scale_add2(&vecs[0], &orient->rvec, -radius);
    vm_vec_scale_add2(&vecs[0], &orient->fvec, Grid_depth);

    vm_vec_scale_add(&vecs[1], &center, &orient->uvec, radius);
    vm_vec_scale_add2(&vecs[1], &orient->rvec, radius);
    vm_vec_scale_add2(&vecs[1], &orient->fvec, Grid_depth);

    vm_vec_scale_add(&vecs[2], &center, &orient->uvec, -radius);
    vm_vec_scale_add2(&vecs[2], &orient->rvec, radius);
    vm_vec_scale_add2(&vecs[2], &orient->fvec, Grid_depth);

    vm_vec_scale_add(&vecs[3], &center, &orient->uvec, -radius);
    vm_vec_scale_add2(&vecs[3], &orient->rvec, -radius);
    vm_vec_scale_add2(&vecs[3], &orient->fvec, Grid_depth);

    //	vm_vec_scale_add( &vecs[4], &center, &orient->fvec, -Grid_depth );
    vecs[4] = center;

    verts[0].u = 0.01f;
    verts[0].v = 0.01f;
    verts[1].u = 0.99f;
    verts[1].v = 0.01f;
    verts[2].u = 0.99f;
    verts[2].v = 0.99f;
    verts[3].u = 0.01f;
    verts[3].v = 0.99f;
    verts[4].u = 0.5f;
    verts[4].v = 0.5f;

    for (i = 0; i < 5; i++) {
        g3_rotate_vertex(&verts[i], &vecs[i]);
    }

    draw_face(&verts[0], &verts[4], &verts[1]);
    draw_face(&verts[1], &verts[4], &verts[2]);
    draw_face(&verts[4], &verts[3], &verts[2]);
    draw_face(&verts[0], &verts[3], &verts[4]);

    if (Warp_glow_bitmap != -1) {
        gr_set_bitmap(Warp_glow_bitmap, GR_ALPHABLEND_FILTER,
                      GR_BITBLT_MODE_NORMAL, 1.0f);

        float r = radius;

        int render_it;

#define OUT_PERCENT1 0.80f
#define OUT_PERCENT2 0.90f

#define IN_PERCENT1 0.10f
#define IN_PERCENT2 0.20f

        if (life_percent < IN_PERCENT1) {
            // do nothing
            render_it = 0;
        }
        else if (life_percent < IN_PERCENT2) {
            r *= (life_percent - IN_PERCENT1) / (IN_PERCENT2 - IN_PERCENT1);
            render_it = 1;
        }
        else if (life_percent < OUT_PERCENT1) {
            // do nothing
            render_it = 1;
        }
        else if (life_percent < OUT_PERCENT2) {
            r *= (OUT_PERCENT2 - life_percent) / (OUT_PERCENT2 - OUT_PERCENT1);
            render_it = 1;
        }
        else {
            // do nothing
            render_it = 0;
        }

        if (render_it) {
            int saved_gr_zbuffering = gr_zbuffer_get();
            gr_zbuffer_set(GR_ZBUFF_READ);

            // Add in noise
            //float Noise[NOISE_NUM_FRAMES] = {
            int noise_frame = fl2i(Missiontime / 15.0f) % NOISE_NUM_FRAMES;

            r *= (0.40f + Noise[noise_frame] * 0.30f);

            g3_draw_bitmap(&verts[4], 0, r, TMAP_FLAG_TEXTURED);
            gr_zbuffer_set(saved_gr_zbuffering);
        }
    }

    gr_zbuffer_set(saved_gr_zbuffering);
}
