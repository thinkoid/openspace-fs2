/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

// Detail level effects (Detail.shield_effects)
//    0     Nothing rendered
//    1     An animating bitmap rendered per hit, not shrink-wrapped.  Lasts half time.  One per ship.
//    2     Animating bitmap per hit, not shrink-wrapped.  Lasts full time.  Unlimited.
//    3     Shrink-wrapped texture.  Lasts half-time.
//    4     Shrink-wrapped texture.  Lasts full-time.

#include <math.h>

#include <graphics/2d.hh>
#include <render/3d.hh>
#include <model/model.hh>
#include <graphics/tmapper.hh>
#include <math/floating.hh>
#include <math/fvi.hh>
#include <lighting/lighting.hh>
#include <fireball/fireballs.hh>
#include <math/fix.hh>
#include <bmpman/bmpman.hh>
#include <object/object.hh>
#include <playerman/player.hh> //   #include of "player.h" is only for debugging!
#include <io/timer.hh>
#include <freespace2/freespace.hh>
#include <anim/packunpack.hh>
#include <anim/animplay.hh>
#include <ship/shiphit.hh>
#include <mission/missionparse.hh>

int New_shield_system = 1;
int Show_shield_mesh = 0;


// One unit in 3d means this in the shield hit texture map.
//#define   SHIELD_HIT_SCALE  0.075f         // Scale decreased by MK on 12/18/97, made about 1/4x as large. Note, larger constant means smaller effect
// Doubled on 12/23/97 by MK.  Was overflowing.  See todo item #924.
#define SHIELD_HIT_SCALE 0.15f
//#define   MAX_SHIELD_HITS   20
// Number of triangles per shield hit, maximum.
#define MAX_TRIS_PER_HIT 40
// Maximum number of active shield hits.
#define MAX_SHIELD_HITS 20
#define MAX_SHIELD_TRI_BUFFER                                                    \
    (MAX_SHIELD_HITS *                                                           \
     20) // Persistent buffer of triangle comprising all active shield hits.
// Duration, in milliseconds, of shield hit effect
#define SHIELD_HIT_DURATION (3 * F1_0 / 4)

// Indicates an unused record in Shield_hits
#define SH_UNUSED -1
// Indicates Shield_hits record is of type 1.
#define SH_TYPE_1 1

// max allowed value until tmapper bugs fixed, 1/24/97
#define UV_MAX (63.95f / 64.0f)

float Shield_scale = SHIELD_HIT_SCALE;

// Structure which mimics the shield_tri structure in model.h.  Since the global shield triangle
// array needs the vertex information, we will acutally store the information in this
// structure instead of the indices into the vertex list
typedef struct gshield_tri
{
    int used; //  Set if this triangle is currently in use.
    int trinum; //   a debug parameter
    fix creation_time; //  time at which created.
    shield_vertex verts[4]; //   Triangles, but at lower detail level, a square.
} gshield_tri;

typedef struct shield_hit
{
    int start_time; //  start time of this object
    int type; //  type, probably the weapon type, to indicate the bitmap to use
    int objnum; //   Object index, needed to get current orientation, position.
    int num_tris; // Number of Shield_tris comprising this shield.
    int tri_list
        [MAX_TRIS_PER_HIT]; //   Indices into Shield_tris, triangles for this shield hit.
    ubyte rgb[3]; // rgb colors
} shield_hit;

// Stores point at which shield was hit.
// Gets processed in frame interval.
typedef struct shield_point
{
    int objnum; //   Object that was hit.
    int shield_tri; //  Triangle in shield mesh that took hit.
    vector hit_point; //   Point in global 3-space of hit.
} shield_point;

#define MAX_SHIELD_POINTS 100
shield_point Shield_points[MAX_SHIELD_POINTS];
int Num_shield_points;

gshield_tri Global_tris
    [MAX_SHIELD_TRI_BUFFER]; //  The persistent triangles, part of shield hits.
int Num_tris; //  Number of triangles in current shield.  Would be a local, but needed in numerous routines.

shield_hit Shield_hits[MAX_SHIELD_HITS];

typedef struct shield_ani
{
    const char *filename;
    int first_frame;
    int nframes;
} shield_ani;

//XSTR:OFF
#define MAX_SHIELD_ANIMS MAX_SPECIES_NAMES
shield_ani Sheild_ani[MAX_SHIELD_ANIMS] = {
    { "shieldhit01a", -1, -1 },
    { "shieldhit01a", -1, -1 },
    { "shieldhit01a", -1, -1 },
};
//XSTR:ON

int Shield_bitmaps_loaded = 0;

void
load_shield_hit_bitmap()
{

    int i;
    // Check if we've already allocated the shield effect bitmaps
    if (Shield_bitmaps_loaded)
        return;

    Shield_bitmaps_loaded = 1;

    for (i = 0; i < MAX_SHIELD_ANIMS; i++) {
        Sheild_ani[i].first_frame = bm_load_animation(
            Sheild_ani[i].filename, &Sheild_ani[i].nframes, NULL, 1);
        if (Sheild_ani[i].first_frame < 0)
            Int3();
    }

}

void
shield_hit_page_in()
{
    int i;

    if (!Shield_bitmaps_loaded) {
        load_shield_hit_bitmap();
    }

    for (i = 0; i < MAX_SHIELD_ANIMS; i++) {
        bm_page_in_xparent_texture(Sheild_ani[i].first_frame,
                                   Sheild_ani[i].nframes);
    }
}

// Initialize shield hit system.  Called from game_level_init()
void
shield_hit_init()
{
    int i;

    for (i = 0; i < MAX_SHIELD_HITS; i++)
        Shield_hits[i].type = SH_UNUSED;

    for (i = 0; i < MAX_SHIELD_TRI_BUFFER; i++) {
        Global_tris[i].used = 0;
        Global_tris[i].creation_time = Missiontime;
    }

    load_shield_hit_bitmap();
}

// ---------------------------------------------------------------------
// release_shield_hit_bitmap()
//
// Release the storage allocated to store the shield effect.
//
void
release_shield_hit_bitmap()
{
    if (!Shield_bitmaps_loaded)
        return;

    // This doesn't need to do anything; the bitmap manager will
    // release everything.
}

int Poly_count = 0;

// ---------------------------------------------------------------------
// shield_hit_close()
//
// De-initalize the shield hit system.  Called from game_level_close().
//
// TODO: We should probably not bother releasing the shield hit bitmaps every level.
//
void
shield_hit_close()
{
    release_shield_hit_bitmap();
}

void
shield_frame_init()
{
    //nprintf(("AI", "Frame %i: Number of shield hits: %i, polycount = %i\n", Framecount, Num_shield_points, Poly_count));

    Poly_count = 0;

    Num_shield_points = 0;
}

void
create_low_detail_poly(int global_index, vector *tcp, vector *rightv, vector *upv)
{
    float scale;
    gshield_tri *trip;

    trip = &Global_tris[global_index];

    scale = vm_vec_mag(tcp) * 2.0f;

    vm_vec_scale_add(&trip->verts[0].pos, tcp, rightv, -scale / 2.0f);
    vm_vec_scale_add2(&trip->verts[0].pos, upv, scale / 2.0f);

    vm_vec_scale_add(&trip->verts[1].pos, &trip->verts[0].pos, rightv, scale);

    vm_vec_scale_add(&trip->verts[2].pos, &trip->verts[1].pos, upv, -scale);

    vm_vec_scale_add(&trip->verts[3].pos, &trip->verts[2].pos, rightv, -scale);

    //   Set u, v coordinates.
    //   Note, this need only be done once, as it's common for all explosions.
    trip->verts[0].u = 0.0f;
    trip->verts[0].v = 0.0f;

    trip->verts[1].u = 1.0f;
    trip->verts[1].v = 0.0f;

    trip->verts[2].u = 1.0f;
    trip->verts[2].v = 1.0f;

    trip->verts[3].u = 0.0f;
    trip->verts[3].v = 1.0f;
}

// ----------------------------------------------------------------------------------------------------
// Free records in Global_tris previously used by Shield_hits[shnum].tri_list
void
free_global_tri_records(int shnum)
{
    int i;

    Assert((shnum >= 0) && (shnum < MAX_SHIELD_HITS));

    //mprintf(("Freeing up %i global records.\n", Shield_hits[shnum].num_tris));

    for (i = 0; i < Shield_hits[shnum].num_tris; i++) {
        Global_tris[Shield_hits[shnum].tri_list[i]].used = 0;
    }
}

void
render_low_detail_shield_bitmap(gshield_tri *trip, matrix *orient, vector *pos,
                                ubyte r, ubyte g, ubyte b)
{
    matrix m;
    int j;
    vector pnt;
    vertex verts[4];

    vm_copy_transpose_matrix(&m, orient);

    for (j = 0; j < 4; j++) {
        // Rotate point into world coordinates
        vm_vec_rotate(&pnt, &trip->verts[j].pos, &m);
        vm_vec_add2(&pnt, pos);

        // Pnt is now the x,y,z world coordinates of this vert.
        g3_rotate_vertex(&verts[j], &pnt);
        verts[j].u = trip->verts[j].u;
        verts[j].v = trip->verts[j].v;
    }

    verts[0].r = r;
    verts[0].g = g;
    verts[0].b = b;
    verts[1].r = r;
    verts[1].g = g;
    verts[1].b = b;
    verts[2].r = r;
    verts[2].g = g;
    verts[2].b = b;
    verts[3].r = r;
    verts[3].g = g;
    verts[3].b = b;

    vector norm;
    vm_vec_perp(&norm, &trip->verts[0].pos, &trip->verts[1].pos,
                &trip->verts[2].pos);
    vertex *vertlist[4];
    if (vm_vec_dot(&norm, &trip->verts[1].pos) < 0.0) {
        vertlist[0] = &verts[3];
        vertlist[1] = &verts[2];
        vertlist[2] = &verts[1];
        vertlist[3] = &verts[0];
        g3_draw_poly(4, vertlist,
                     TMAP_FLAG_TEXTURED | TMAP_FLAG_RGB | TMAP_FLAG_GOURAUD);
    }
    else {
        vertlist[0] = &verts[0];
        vertlist[1] = &verts[1];
        vertlist[2] = &verts[2];
        vertlist[3] = &verts[3];
        g3_draw_poly(4, vertlist,
                     TMAP_FLAG_TEXTURED | TMAP_FLAG_RGB | TMAP_FLAG_GOURAUD);
    }
}

MONITOR(NumShieldRend);

// Render a shield mesh in the global array Shield_hits[]
void
render_shield(int shield_num) //, matrix *orient, vector *centerp)
{
    vector *centerp;
    matrix *orient;
    object *objp;
    ship *shipp;
    ship_info *si;

    if (Shield_hits[shield_num].type == SH_UNUSED) {
        return;
    }

    Assert(Shield_hits[shield_num].objnum >= 0);

    objp = &Objects[Shield_hits[shield_num].objnum];

    if (objp->flags & OF_NO_SHIELDS) {
        return;
    }

    //   If this object didn't get rendered, don't render its shields.  In fact, make the shield hit go away.
    if (!(objp->flags & OF_WAS_RENDERED)) {
        Shield_hits[shield_num].type = SH_UNUSED;
        return;
    }

    //   Animations play at double speed to reduce load.
    Shield_hits[shield_num].start_time -= Frametime;

    MONITOR_INC(NumShieldRend, 1);

    shipp = &Ships[objp->instance];
    si = &Ship_info[shipp->ship_info_index];

    // objp, shipp, and si are now setup correctly

    //   If this ship is in its deathroll, make the shield hit effects go away faster.
    if (shipp->flags & SF_DYING) {
        Shield_hits[shield_num].start_time -= fl2f(2 * flFrametime);
    }

    //   Detail level stuff.  When lots of shield hits, maybe make them go away faster.
    if (Poly_count > 50) {
        if (Shield_hits[shield_num].start_time +
                (SHIELD_HIT_DURATION * 50) / Poly_count <
            Missiontime) {
            Shield_hits[shield_num].type = SH_UNUSED;
            free_global_tri_records(shield_num);
            // nprintf(("AI", "* "));
            return;
        }
    }
    else if ((Shield_hits[shield_num].start_time + SHIELD_HIT_DURATION) <
             Missiontime) {
        Shield_hits[shield_num].type = SH_UNUSED;
        free_global_tri_records(shield_num);
        return;
    }

    orient = &objp->orient;
    centerp = &objp->pos;

    int bitmap_id, frame_num, n;

    // mprintf(("Percent = %7.3f\n", f2fl(Missiontime - Shield_hits[shield_num].start_time)));

    n = si->species;
    // Do some sanity checking
    Assert((n >= 0) && (n < MAX_SPECIES_NAMES));
    Assert((n >= 0) && (n < MAX_SHIELD_ANIMS));

    frame_num = fl2i(f2fl(Missiontime - Shield_hits[shield_num].start_time) *
                     Sheild_ani[n].nframes);
    if (frame_num >= Sheild_ani[n].nframes) {
        frame_num = Sheild_ani[n].nframes - 1;
    }
    else if (frame_num < 0) {
        mprintf(("HEY! Missiontime went backwards! (Shield.cpp)\n"));
        frame_num = 0;
    }
    bitmap_id = Sheild_ani[n].first_frame + frame_num;

    float alpha = 0.9999f;
    if (The_mission.flags & MISSION_FLAG_FULLNEB) {
        alpha *= 0.85f;
    }
    gr_set_bitmap(bitmap_id, GR_ALPHABLEND_FILTER, GR_BITBLT_MODE_NORMAL, alpha);

    if (bitmap_id != -1) {
        render_low_detail_shield_bitmap(
            &Global_tris[Shield_hits[shield_num].tri_list[0]], orient, centerp,
            Shield_hits[shield_num].rgb[0], Shield_hits[shield_num].rgb[1],
            Shield_hits[shield_num].rgb[2]);
    }
}

// Render all the shield hits  in the global array Shield_hits[]
// This is a temporary function.  Shield hit rendering will at least have to
// occur with the ship, perhaps even internal to the ship.
void
render_shields()
{
    int i;

    if (Detail.shield_effects == 0) {
        return; //   No shield effect rendered at lowest detail level.
    }

    if (!New_shield_system) {
        return;
    }

    for (i = 0; i < MAX_SHIELD_HITS; i++) {
        if (Shield_hits[i].type != SH_UNUSED) {
            render_shield(i);
        }
    }
}

int Gi_max = 0;

int
get_free_global_shield_index()
{
    int gi = 0;

    while ((gi < MAX_SHIELD_TRI_BUFFER) && (Global_tris[gi].used) &&
           (Global_tris[gi].creation_time + SHIELD_HIT_DURATION > Missiontime)) {
        gi++;
    }

    //   If couldn't find one, choose a random one.
    if (gi == MAX_SHIELD_TRI_BUFFER)
        gi = (int)(frand() * MAX_SHIELD_TRI_BUFFER);

    return gi;
}

int
get_global_shield_tri()
{
    int shnum;

    //   Find unused shield hit buffer
    for (shnum = 0; shnum < MAX_SHIELD_HITS; shnum++)
        if (Shield_hits[shnum].type == SH_UNUSED)
            break;

    if (shnum == MAX_SHIELD_HITS) {
        //nprintf(("AI", "Warning: Shield_hit buffer full!  Stealing an old one!\n"));
        shnum = myrand() % MAX_SHIELD_HITS;
    }

    Assert((shnum >= 0) && (shnum < MAX_SHIELD_HITS));

    return shnum;
}

// ***** This is the version that works on a quadrant basis.
// Return absolute amount of damage not applied.
float
apply_damage_to_shield(object *objp, int shield_quadrant, float damage)
{
    ai_info *aip;

    if ((shield_quadrant < 0) || (shield_quadrant > 3))
        return damage;

    Assert(objp->type == OBJ_SHIP);
    aip = &Ai_info[Ships[objp->instance].ai_index];
    aip->last_hit_quadrant = shield_quadrant;

    objp->shields[shield_quadrant] -= damage;

    if (objp->shields[shield_quadrant] < 0.0f) {
        float remaining_damage;

        remaining_damage = -objp->shields[shield_quadrant];
        objp->shields[shield_quadrant] = 0.0f;
        //nprintf(("AI", "Applied %7.3f damage to quadrant #%i, %7.3f passes through\n", damage - remaining_damage, quadrant_num, remaining_damage));
        return remaining_damage;
    }
    else {
        //nprintf(("AI", "Applied %7.3f damage to quadrant #%i\n", damage, quadrant_num));
        return 0.0f;
    }
}

// At lower detail levels, shield hit effects are a single texture, applied to one enlarged triangle.
void
create_shield_low_detail(int objnum, int model_num, matrix *orient,
                         vector *centerp, vector *tcp, int tr0,
                         shield_info *shieldp)
{
    matrix tom;
    int gi;
    int shnum;

    shnum = get_global_shield_tri();
    Shield_hits[shnum].type = SH_TYPE_1;

    gi = get_free_global_shield_index();

    Global_tris[gi].used = 1;
    Global_tris[gi].trinum =
        -1; // This tells triangle renderer to not render in case detail_level was switched.
    Global_tris[gi].creation_time = Missiontime;

    Shield_hits[shnum].tri_list[0] = gi;
    Shield_hits[shnum].num_tris = 1;
    Shield_hits[shnum].start_time = Missiontime;
    Shield_hits[shnum].objnum = objnum;

    Shield_hits[shnum].rgb[0] = 255;
    Shield_hits[shnum].rgb[1] = 255;
    Shield_hits[shnum].rgb[2] = 255;
    if ((objnum >= 0) && (objnum < MAX_OBJECTS) &&
        (Objects[objnum].type == OBJ_SHIP) && (Objects[objnum].instance >= 0) &&
        (Objects[objnum].instance < MAX_SHIPS) &&
        (Ships[Objects[objnum].instance].ship_info_index >= 0) &&
        (Ships[Objects[objnum].instance].ship_info_index < Num_ship_types)) {
        ship_info *sip =
            &Ship_info[Ships[Objects[objnum].instance].ship_info_index];

        Shield_hits[shnum].rgb[0] = sip->shield_color[0];
        Shield_hits[shnum].rgb[1] = sip->shield_color[1];
        Shield_hits[shnum].rgb[2] = sip->shield_color[2];
    }

    vm_vector_2_matrix(&tom, &shieldp->tris[tr0].norm, NULL, NULL);

    create_low_detail_poly(gi, tcp, &tom.rvec, &tom.uvec);
}

void
create_shield_explosion(int objnum, int model_num, matrix *orient,
                        vector *centerp, vector *tcp, int tr0)
{
    shield_info *shieldp;
    polymodel *pm;

    if (!New_shield_system)
        return;

    if (Objects[objnum].flags & OF_NO_SHIELDS)
        return;

    pm = model_get(model_num);
    Num_tris = pm->shield.ntris;
    //Assert(Num_tris < MAX_SHIELD_HITS);
    shieldp = &pm->shield;

    if (Num_tris == 0)
        return;

    //nprintf(("AI", "Frame %i: Creating explosion on %i.\n", Framecount, objnum));

    create_shield_low_detail(objnum, model_num, orient, centerp, tcp, tr0,
                             shieldp);
}

MONITOR(NumShieldHits);

// Add data for a shield hit.
void
add_shield_point(int objnum, int tri_num, vector *hit_pos)
{
    //Assert(Num_shield_points < MAX_SHIELD_POINTS);
    if (Num_shield_points >= MAX_SHIELD_POINTS)
        return;

    MONITOR_INC(NumShieldHits, 1);

    Shield_points[Num_shield_points].objnum = objnum;
    Shield_points[Num_shield_points].shield_tri = tri_num;
    Shield_points[Num_shield_points].hit_point = *hit_pos;

    Num_shield_points++;

    Ships[Objects[objnum].instance].shield_hits++;
}

// Create all the shield explosions that occurred on object *objp this frame.
void
create_shield_explosion_all(object *objp)
{
    int i;
    int num;
    int count;
    int objnum;
    ship *shipp;

    if (Detail.shield_effects == 0) {
        return;
    }

    num = objp->instance;
    shipp = &Ships[num];

    count = shipp->shield_hits;
    objnum = objp - Objects;

    for (i = 0; i < Num_shield_points; i++) {
        if (Shield_points[i].objnum == objnum) {
            create_shield_explosion(objnum, shipp->modelnum, &objp->orient,
                                    &objp->pos, &Shield_points[i].hit_point,
                                    Shield_points[i].shield_tri);
            count--;
            if (count <= 0) {
                break;
            }
        }
    }

    //mprintf(("Creating %i explosions took %7.3f seconds\n", shipp->shield_hits, (float) (timer_get_milliseconds() - start_time)/1000.0f));

    Assert(count == 0); // Couldn't find all the alleged shield hits.  Bogus!
}

int Break_value = -1;

// This is a debug function.
// Draw the whole shield as a wireframe mesh, not looking at the current
// integrity.
#ifndef NDEBUG
void
ship_draw_shield(object *objp)
{
    int model_num;
    matrix m;
    int i;
    vector pnt;
    polymodel *pm;

    if (!New_shield_system)
        return;

    if (objp->flags & OF_NO_SHIELDS)
        return;

    Assert(objp->instance >= 0);

    model_num = Ships[objp->instance].modelnum;


    pm = model_get(model_num);

    if (pm->shield.ntris < 1)
        return;

    vm_copy_transpose_matrix(&m, &objp->orient);

    //   Scan all the triangles in the mesh.
    for (i = 0; i < pm->shield.ntris; i++) {
        int j;
        vector gnorm, v2f, tri_point;
        vertex prev_pnt, pnt0;
        shield_tri *tri;

        tri = &pm->shield.tris[i];

        if (i == Break_value)
            Int3();

        //  Hack! Only works for object in identity orientation.
        //  Need to rotate eye position into object's reference frame.
        //  Only draw facing triangles.
        vm_vec_rotate(&tri_point, &pm->shield.verts[tri->verts[0]].pos,
                      &Eye_matrix);
        vm_vec_add2(&tri_point, &objp->pos);

        vm_vec_sub(&v2f, &tri_point, &Eye_position);
        vm_vec_rotate(&gnorm, &tri->norm, &m);

        if (vm_vec_dot(&gnorm, &v2f) < 0.0f) {
            int intensity;

            intensity = (int)(Ships[objp->instance].shield_integrity[i] * 255);

            if (intensity < 0)
                intensity = 0;
            else if (intensity > 255)
                intensity = 255;

            gr_set_color(0, 0, intensity);

            // Process the vertices.
            // Note this rotates each vertex each time it's needed, very dumb.
            for (j = 0; j < 3; j++) {
                vertex tmp;

                // Rotate point into world coordinates
                vm_vec_rotate(&pnt, &pm->shield.verts[tri->verts[j]].pos, &m);
                //vm_vec_rotate(&pnt,&pm->shield[i].pnt[j],&m);
                vm_vec_add2(&pnt, &objp->pos);

                // Pnt is now the x,y,z world coordinates of this vert.
                // For this example, I am just drawing a sphere at that
                // point.
                g3_rotate_vertex(&tmp, &pnt);

                if (j)
                    g3_draw_line(&prev_pnt, &tmp);
                else
                    pnt0 = tmp;
                prev_pnt = tmp;
            }

            g3_draw_line(&pnt0, &prev_pnt);
        }
    }
}
#endif

// Returns true if the shield presents any opposition to something
// trying to force through it.
// If quadrant is -1, looks at entire shield, otherwise
// just one quadrant
int
ship_is_shield_up(object *obj, int quadrant)
{
    if ((quadrant >= 0) && (quadrant <= 3)) {
        // Just check one quadrant
        if (obj->shields[quadrant] >
            max(2.0f,
                0.1f * Ship_info[Ships[obj->instance].ship_info_index].shields /
                    4.0f)) {
            return 1;
        }
    }
    else {
        // Check all quadrants
        float strength = get_shield_strength(obj);

        if (strength >
            max(2.0f * 4.0f,
                0.1f * Ship_info[Ships[obj->instance].ship_info_index].shields)) {
            return 1;
        }
    }
    return 0; // no shield strength
}

/*
//-- CODE TO "BOUNCE" AN ARRAY FROM A GIVEN POINT.
//-- LIKE A MATTRESS.
#define BOUNCE_SIZE ???

byte Bouncer1[BOUNCE_SIZE];
byte Bouncer2[BOUNCE_SIZE];

byte * Bouncer = Bouncer1;
byte * OldBouncer = Bouncer2;

// To wiggle, add value to Bouncer[] 

void bounce_it()
{
   int i, tmp;


   for (i=0; i<BOUNCE_SIZE; i++ )   {
      int t = 0;

      t += OldBouncer[ LEFT ];
      t += OldBouncer[ RIGHT ];
      t += OldBouncer[ UP ];
      t += OldBouncer[ DOWN ];

      t = (t/2) - Bouncer[i];
      tmp = t - t/16;      // 8
      
      if ( tmp < -127 ) tmp = -127;
      if ( tmp > 127 ) tmp = 127;
      Bouncer[i] = tmp;
   }

   if ( Bouncer == Bouncer1 ) {
      OldBouncer = Bouncer1;
      Bouncer = Bouncer2;
   } else {
      OldBouncer = Bouncer2;
      Bouncer = Bouncer1;
   }
}
*/


// return quadrant containing hit_pnt.
// \  1  /.
// 3 \ / 0
//   / \.
// /  2  \.
// Note: This is in the object's local reference frame.  Do _not_ pass a vector in the world frame.
int
get_quadrant(vector *hit_pnt)
{
    int result = 0;

    if (hit_pnt->x < hit_pnt->z)
        result |= 1;

    if (hit_pnt->x < -hit_pnt->z)
        result |= 2;

    return result;
}
