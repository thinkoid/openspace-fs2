/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#include <globalincs/pstypes.hh>
#include <freespace2/freespace.hh>
#include <globalincs/linklist.hh>
#include <render/3d.hh>
#include <bmpman/bmpman.hh>
#include <particle/particle.hh>
#include <osapi/osapi.hh>
#include <object/object.hh>
#include <io/timer.hh>

typedef struct particle
{
    // old style data
    vector pos; // position
    vector velocity; // velocity
    float age; // How long it's been alive
    float max_life; // How much life we had
    float radius; // radius
    int type; // type                              // -1 = None
    uint optional_data; // depends on type
    int nframes; // If an ani, how many frames?

    // new style data
    float tracer_length; // if this is set, draw as a rod to simulate a "tracer" effect
    short attached_objnum; // if this is set, pos is relative to the attached object. velocity is ignored
    int attached_sig; // to check for dead/nonexistent objects
    ubyte reverse; // play any animations in reverse
} particle;

#ifdef FS2_DEMO
#define MAX_PARTICLES 500
#else
// Reduced from 2000 to 800 by MK on 4/1/98.  Most I ever saw was 400 and the system recovers
// gracefully from running out of slots.
// AP: Put it to 1500 on 4/15/98.  Primary hit sparks weren't finding open slots.
// Made todo item for John to force oldest smoke particles to give up their slots.
#define MAX_PARTICLES 2000
#endif

int Num_particles = 0;
particle Particles[MAX_PARTICLES];
int Next_particle = 0;

int Anim_bitmap_id_fire = -1;
int Anim_num_frames_fire = -1;

int Anim_bitmap_id_smoke = -1;
int Anim_num_frames_smoke = -1;

int Anim_bitmap_id_smoke2 = -1;
int Anim_num_frames_smoke2 = -1;

static int Particles_enabled = 1;

// Reset everything between levels
void
particle_init()
{
    int i;

    //   Particles_enabled = os_config_read_uint( NULL, "UseParticles", 0 );

    Num_particles = 0;
    Next_particle = 0;

    for (i = 0; i < MAX_PARTICLES; i++) {
        Particles[i].type = -1;
    }

    if (Anim_bitmap_id_fire == -1) {
        int fps;
        Anim_bitmap_id_fire = bm_load_animation("particleexp01",
                                                &Anim_num_frames_fire, &fps, 0);
    }
    //Anim_bitmap_id = bm_load( "particleglow01" );
    //Anim_num_frames = 1;

    if (Anim_bitmap_id_smoke == -1) {
        int fps;
        Anim_bitmap_id_smoke = bm_load_animation("particlesmoke01",
                                                 &Anim_num_frames_smoke, &fps, 0);
    }

    if (Anim_bitmap_id_smoke2 == -1) {
        int fps;
        Anim_bitmap_id_smoke2 = bm_load_animation(
            "particlesmoke02", &Anim_num_frames_smoke2, &fps, 0);
    }
}

void
particle_page_in()
{
    int i;

    for (i = 0; i < MAX_PARTICLES; i++) {
        Particles[i].type = -1;
    }

    for (i = 0; i < Anim_num_frames_fire; i++) {
        bm_page_in_texture(Anim_bitmap_id_fire + i);
    }

    for (i = 0; i < Anim_num_frames_smoke; i++) {
        bm_page_in_texture(Anim_bitmap_id_smoke + i);
    }

    for (i = 0; i < Anim_num_frames_smoke2; i++) {
        bm_page_in_texture(Anim_bitmap_id_smoke2 + i);
    }
}

DCF(particles, "Turns particles on/off")
{
    if (Dc_command) {
        dc_get_arg(ARG_TRUE | ARG_FALSE | ARG_NONE);
        if (Dc_arg_type & ARG_TRUE)
            Particles_enabled = 1;
        else if (Dc_arg_type & ARG_FALSE)
            Particles_enabled = 0;
        else if (Dc_arg_type & ARG_NONE)
            Particles_enabled ^= 1;
    }
    if (Dc_help)
        dc_printf(
            "Usage: particles [bool]\nTurns particle system on/off.  If nothing passed, then toggles it.\n");
    if (Dc_status)
        dc_printf("particles are %s\n", (Particles_enabled ? "ON" : "OFF"));

    //   os_config_write_uint( NULL, "UseParticles", Particles_enabled );
}

//mprintf(( "%s\n", text ));

int Num_particles_hwm = 0;

#ifndef NDEBUG
int Total_requested = 0;
int Total_killed = 0;
int next_message = -1;
#endif

// Creates a single particle. See the PARTICLE_?? defines for types.
void
particle_create(particle_info *pinfo)
{
    int particle_num;
    particle *p;

#ifndef NDEBUG
    if (next_message == -1) {
        next_message = timestamp(10000);
    }

    if (timestamp_elapsed(next_message)) {
        next_message = timestamp(10000);
        if (Total_requested > 1) {
            nprintf(("Particles", "Particles: Killed off %d%% of the particles\n",
                     (Total_killed * 100 / Total_requested)));
        }
        Total_requested = 0;
        Total_killed = 0;
    }
#endif

    if (!Particles_enabled)
        return;

#ifndef NDEBUG
    Total_requested++;
#endif

    int RetryCount = 0;
    int FirstParticleFound = Next_particle;
KillAnother:
    p = &Particles[Next_particle++];
    if (Next_particle >= MAX_PARTICLES) {
        Next_particle = 0;
    }

    if (p->type > -1) {
        // Only remove non-persistent ones
        if (p->type != PARTICLE_BITMAP_PERSISTENT) {
            p->type = -1;
            Num_particles--;
#ifndef NDEBUG
            Total_killed++;
#endif
        }
        else {
            RetryCount++;
            // Keep trying to find a non-persistent one until we searched through 1/3 the slots.
            if (RetryCount < MAX_PARTICLES / 3) {
                goto KillAnother;
            }
            // Couldn't find any non-persistent ones to remove, so just remove the
            // first one we would have removed.
            mprintf((
                "DELETING A PERSISTENT PARTICLE!!! This is ok if this only happens rarely. Get John if not.\n"));
            Next_particle = FirstParticleFound;
            p = &Particles[Next_particle++];
            if (Next_particle >= MAX_PARTICLES) {
                Next_particle = 0;
            }
        }
    }

    // increment counter
    Num_particles++;
    if (Num_particles > Num_particles_hwm) {
        Num_particles_hwm = Num_particles;

        if (Num_particles_hwm == MAX_PARTICLES) {
            mprintf(("All particle slots filled!\n"));
        }
        //mprintf(( "Num_particles high water mark = %i\n", Num_particles_hwm));
    }

    // get objnum
    particle_num = p - Particles;

    // Init the particle data
    p->pos = pinfo->pos;
    p->velocity = pinfo->vel;
    p->age = 0.0f;
    p->max_life = pinfo->lifetime;
    p->radius = pinfo->rad;
    p->type = pinfo->type;
    p->optional_data = pinfo->optional_data;
    p->tracer_length = pinfo->tracer_length;
    p->attached_objnum = pinfo->attached_objnum;
    p->attached_sig = pinfo->attached_sig;
    p->reverse = pinfo->reverse;

    if ((p->type == PARTICLE_BITMAP) || (p->type == PARTICLE_BITMAP_PERSISTENT)) {
        int fps;
        bm_get_info(p->optional_data, NULL, NULL, NULL, &p->nframes, &fps);
        if (p->nframes > 1) {
            // Recalculate max life for ani's
            p->max_life = i2fl(p->nframes) / i2fl(fps);
        }
    }
    else {
        p->nframes = 1;
    }
}

void
particle_create(vector *pos, vector *vel, float lifetime, float rad, int type,
                uint optional_data)
{
    particle_info pinfo;

    // setup old data
    pinfo.pos = *pos;
    pinfo.vel = *vel;
    pinfo.lifetime = lifetime;
    pinfo.rad = rad;
    pinfo.type = type;
    pinfo.optional_data = optional_data;

    // setup new data
    pinfo.tracer_length = -1.0f;
    pinfo.attached_objnum = -1;
    pinfo.attached_sig = -1;
    pinfo.reverse = 0;

    // lower level function
    particle_create(&pinfo);
}

MONITOR(NumParticles);

void
particle_move_all(float frametime)
{
    particle *p;

    MONITOR_INC(NumParticles, Num_particles);

    if (!Particles_enabled)
        return;

    p = Particles;

    int i;
    for (i = 0; i < MAX_PARTICLES; i++, p++) {
        if (p->type == -1) {
            continue;
        }

        // bogus attached objnum
        if (p->attached_objnum >= MAX_OBJECTS) {
            p->type = -1;

            // decrement counter
            Num_particles--;

            Assert(Num_particles >= 0);
            continue;
        }

        // if the vector is attached to an object which has become invalid, kill if
        if (p->attached_objnum >= 0) {
            // if the signature has changed, kill it
            if (p->attached_sig != Objects[p->attached_objnum].signature) {
                p->type = -1;

                // decrement counter
                Num_particles--;

                Assert(Num_particles >= 0);
                continue;
            }
        }
        // move as a regular particle
        else {
            // Move the particle
            vm_vec_scale_add2(&p->pos, &p->velocity, frametime);
        }

        p->age += frametime;

        if (p->age > p->max_life) {
            // If it's time expired, remove it from the used list and
            // into the free list
            p->type = -1;

            // decrement counter
            Num_particles--;

            Assert(Num_particles >= 0);
        }
    }
}

// kill all active particles
void
particle_kill_all()
{
    int idx;

    // kill all active particles
    Num_particles = 0;
    Next_particle = 0;
    Num_particles_hwm = 0;
    for (idx = 0; idx < MAX_PARTICLES; idx++) {
        Particles[idx].type = -1;
    }
}

MONITOR(NumParticlesRend);

void
particle_render_all()
{
    particle *p;
    ubyte flags;
    float pct_complete;
    float alpha;
    vertex pos;
    vector ts, te, temp;
    int rotate = 1;

    if (!Particles_enabled)
        return;

    MONITOR_INC(NumParticlesRend, Num_particles);

    int n = 0;
    int nclipped = 0;

    p = Particles;

    int i;
    for (i = 0; i < MAX_PARTICLES; i++, p++) {
        if (p->type == -1) {
            continue;
        }

        n++;

        // pct complete for the particle
        pct_complete = p->age / p->max_life;

        // calculate the alpha to draw at
        alpha = 1.0f;

        // if this is a tracer style particle, calculate tracer vectors
        if (p->tracer_length > 0.0f) {
            ts = p->pos;
            temp = p->velocity;
            vm_vec_normalize_quick(&temp);
            vm_vec_scale_add(&te, &ts, &temp, p->tracer_length);

            // don't bother rotating
            rotate = 0;
        }
        // if this is an "attached" particle. move it
        else if (p->attached_objnum >= 0) {
            // offset the vector, and transform to view coords
            // vm_vec_add(&te, &Objects[p->attached_objnum].pos, &p->pos);
            vm_vec_unrotate(&temp, &p->pos, &Objects[p->attached_objnum].orient);
            vm_vec_add2(&temp, &Objects[p->attached_objnum].pos);

            flags = g3_rotate_vertex(&pos, &temp);
            if (flags) {
                nclipped++;
                continue;
            }

            // don't bother rotating again
            rotate = 0;
        }

        // rotate the vertex
        if (rotate) {
            flags = g3_rotate_vertex(&pos, &p->pos);
            if (flags) {
                nclipped++;
                continue;
            }
        }

        switch (p->type) {
        case PARTICLE_DEBUG: // A red sphere, no optional data required
            gr_set_color(255, 0, 0);
            g3_draw_sphere_ez(&p->pos, p->radius);
            break;

        case PARTICLE_BITMAP:
        case PARTICLE_BITMAP_PERSISTENT: { // A bitmap, optional data is the bitmap number
            int framenum = p->optional_data;

            if (p->nframes > 1) {
                int n = fl2i(pct_complete * p->nframes + 0.5);

                if (n < 0)
                    n = 0;
                else if (n > p->nframes - 1)
                    n = p->nframes - 1;

                framenum += n;
            }

            // set the bitmap
            gr_set_bitmap(framenum, GR_ALPHABLEND_FILTER, GR_BITBLT_MODE_NORMAL,
                          alpha);

            // if this is a tracer style particle
            if (p->tracer_length > 0.0f) {
                g3_draw_laser(&ts, p->radius, &te, p->radius,
                              TMAP_FLAG_TEXTURED | TMAP_FLAG_XPARENT, 25.0f);
            }
            // draw as a regular bitmap
            else {
                g3_draw_bitmap(&pos, (p - Particles) % 8, p->radius,
                               TMAP_FLAG_TEXTURED);
            }
            break;
        }

        case PARTICLE_FIRE: {
            int framenum = fl2i(pct_complete * Anim_num_frames_fire + 0.5);

            if (framenum < 0)
                framenum = 0;
            else if (framenum > Anim_num_frames_fire - 1)
                framenum = Anim_num_frames_fire - 1;

            /*
               vertex pos;
               flags = g3_rotate_vertex( &pos, &p->pos );
               if ( flags )   {
                  nclipped++;
                  break;
               }
               */

            // set the bitmap
            gr_set_bitmap(p->reverse ? Anim_bitmap_id_fire +
                                           (Anim_num_frames_fire - framenum - 1)
                                     : Anim_bitmap_id_fire + framenum,
                          GR_ALPHABLEND_FILTER, GR_BITBLT_MODE_NORMAL, alpha);

            // if this is a tracer style particle
            if (p->tracer_length > 0.0f) {
                g3_draw_laser(&ts, p->radius, &te, p->radius,
                              TMAP_FLAG_TEXTURED | TMAP_FLAG_XPARENT, 25.0f);
            }
            // draw as a regular bitmap
            else {
                g3_draw_bitmap(&pos, (p - Particles) % 8, p->radius,
                               TMAP_FLAG_TEXTURED);
            }
            break;
        }

        case PARTICLE_SMOKE: {
            int framenum = fl2i(pct_complete * Anim_num_frames_smoke + 0.5);

            if (framenum < 0)
                framenum = 0;
            else if (framenum > Anim_num_frames_smoke - 1)
                framenum = Anim_num_frames_smoke - 1;

            /*
               vertex pos;
               flags = g3_rotate_vertex( &pos, &p->pos );
               if ( flags )   {
                  nclipped++;
                  break;
               }
               */

            // set the bitmap
            gr_set_bitmap(p->reverse ? Anim_bitmap_id_smoke +
                                           (Anim_num_frames_smoke - framenum - 1)
                                     : Anim_bitmap_id_smoke + framenum,
                          GR_ALPHABLEND_FILTER, GR_BITBLT_MODE_NORMAL, alpha);

            // if this is a tracer style particle
            if (p->tracer_length > 0.0f) {
                g3_draw_laser(&ts, p->radius, &te, p->radius,
                              TMAP_FLAG_TEXTURED | TMAP_FLAG_XPARENT, 25.0f);
            }
            // draw as a regular bitmap
            else {
                g3_draw_bitmap(&pos, (p - Particles) % 8, p->radius,
                               TMAP_FLAG_TEXTURED);
            }
            break;
        }

        case PARTICLE_SMOKE2: {
            int framenum = fl2i(pct_complete * Anim_num_frames_smoke2 + 0.5);

            if (framenum < 0)
                framenum = 0;
            else if (framenum > Anim_num_frames_smoke2 - 1)
                framenum = Anim_num_frames_smoke2 - 1;

            /*
               vertex pos;
               flags = g3_rotate_vertex( &pos, &p->pos );
               if ( flags )   {
                  nclipped++;
                  break;
               }
               */

            // set the bitmap
            gr_set_bitmap(p->reverse ? Anim_bitmap_id_smoke2 +
                                           (Anim_num_frames_smoke2 - framenum - 1)
                                     : Anim_bitmap_id_smoke2 + framenum,
                          GR_ALPHABLEND_FILTER, GR_BITBLT_MODE_NORMAL, alpha);

            // if this is a tracer style particle
            if (p->tracer_length > 0.0f) {
                g3_draw_laser(&ts, p->radius, &te, p->radius,
                              TMAP_FLAG_TEXTURED | TMAP_FLAG_XPARENT, 25.0f);
            }
            // draw as a regular bitmap
            else {
                g3_draw_bitmap(&pos, (p - Particles) % 8, p->radius,
                               TMAP_FLAG_TEXTURED);
            }
            break;
        }
        }
    }
    //   mprintf(( "NP=%d, NCP=%d\n", n, nclipped ));
}

//============== HIGH-LEVEL PARTICLE SYSTEM CREATION CODE ====================

// Use a structure rather than pass a ton of parameters to particle_emit
/*
typedef struct particle_emitter {
   int      num_low;          // Lowest number of particles to create
   int      num_high;         // Highest number of particles to create
   vector   pos;              // Where the particles emit from
   vector   vel;              // Initial velocity of all the particles
   float    lifetime;         // How long the particles live
   vector   normal;           // What normal the particle emit arond
   float    normal_variance;  // How close they stick to that normal 0=good, 1=360 degree
   float    min_vel;          // How fast the slowest particle can move
   float    max_vel;          // How fast the fastest particle can move
   float    min_rad;          // Min radius
   float    max_rad;          // Max radius
} particle_emitter;
*/

#if MAX_DETAIL_LEVEL != 4
#error Max details assumed to be 4 here
#endif
int detail_max_num[5] = { 0, 50, 75, 100, 125 };

// Creates a bunch of particles. You pass a structure
// rather than a bunch of parameters.
void
particle_emit(particle_emitter *pe, int type, uint optional_data, float range)
{
    int i, n;

    if (!Particles_enabled)
        return;

    int n1, n2;

// Account for detail
#if MAX_DETAIL_LEVEL != 4
#error Code in Particle.cpp assumes MAX_DETAIL_LEVEL == 4
#endif

    int percent = detail_max_num[Detail.num_particles];

    //Particle rendering drops out too soon.  Seems to be around 150 m.  Is it detail level controllable?  I'd like it to be 500-1000
    float min_dist = 125.0f;
    float dist = vm_vec_dist_quick(&pe->pos, &Eye_position) / range;
    if (dist > min_dist) {
        percent = fl2i(i2fl(percent) * min_dist / dist);
        if (percent < 1) {
            return;
        }
    }
    //mprintf(( "Dist = %.1f, percent = %d%%\n", dist, percent ));

    n1 = (pe->num_low * percent) / 100;
    n2 = (pe->num_high * percent) / 100;

    // How many to emit?
    n = (rand() % (n2 - n1 + 1)) + n1;

    if (n < 1)
        return;

    for (i = 0; i < n; i++) {
        // Create a particle
        vector tmp_vel;
        vector normal; // What normal the particle emit arond

        float radius = ((pe->max_rad - pe->min_rad) * frand()) + pe->min_rad;

        float speed = ((pe->max_vel - pe->min_vel) * frand()) + pe->min_vel;

        float life = ((pe->max_life - pe->min_life) * frand()) + pe->min_life;

        normal.x = pe->normal.x + (frand() * 2.0f - 1.0f) * pe->normal_variance;
        normal.y = pe->normal.y + (frand() * 2.0f - 1.0f) * pe->normal_variance;
        normal.z = pe->normal.z + (frand() * 2.0f - 1.0f) * pe->normal_variance;
        vm_vec_normalize_safe(&normal);
        vm_vec_scale_add(&tmp_vel, &pe->vel, &normal, speed);

        particle_create(&pe->pos, &tmp_vel, life, radius, type, optional_data);
    }
}
