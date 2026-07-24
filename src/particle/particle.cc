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

// The whole system is one fixed array used as a ring: particles are never
// allocated or freed. A slot is live when its type is a PARTICLE_* value and
// free when its type is -1. Creation walks forward from Next_particle and
// takes the first slot it is willing to evict, so a busy frame recycles the
// oldest particles instead of failing. Nothing here can run out of memory;
// the cost of overload is paid in evicted particles.

#ifdef FS2_DEMO
#define MAX_PARTICLES 500
#else
// Reduced from 2000 to 800 by MK on 4/1/98.  Most I ever saw was 400 and the
// system recovers gracefully from running out of slots.
// AP: Put it to 1500 on 4/15/98.  Primary hit sparks weren't finding open
// slots.  Made todo item for John to force oldest smoke particles to give up
// their slots.
#define MAX_PARTICLES 2000
#endif

struct particle_t
{
    vector pos;      // world space, or object space while attached
    vector velocity; // unused while attached
    float  age;      // seconds since creation
    float  max_life; // seconds to live; animations are retimed to one play
    float  radius;

    int  type;          // a PARTICLE_* value, or -1 when the slot is free
    uint optional_data; // bitmap handle, for the PARTICLE_BITMAP* types
    int  nframes;       // frames in that bitmap, 1 when not animated

    float tracer_length;   // > 0 draws a rod along velocity, not a sprite
    short attached_objnum; // >= 0 rides that object; pos is then object space
    int   attached_sig;    // that object's signature, to catch slot reuse
    ubyte reverse;         // play the animation last frame first
};

particle_t Particles[MAX_PARTICLES];

int Next_particle = 0;     // where the next creation starts looking
int Num_particles = 0;     // live slots; particle_create notes a known drift
int Num_particles_hwm = 0; // high-water mark, reported to the console only

// The three built-in animations are loaded once and shared by every particle
// of that type; they outlive a level, so particle_init only loads them once.
// PARTICLE_BITMAP carries its own handle in optional_data instead.
int Anim_bitmap_id_fire = -1;
int Anim_num_frames_fire = -1;

int Anim_bitmap_id_smoke = -1;
int Anim_num_frames_smoke = -1;

int Anim_bitmap_id_smoke2 = -1;
int Anim_num_frames_smoke2 = -1;

// Session-only. Persisting this through os_config_{read,write}_uint was
// drafted alongside the console command below, but never enabled.
static int Particles_enabled = 1;

#ifndef NDEBUG
// Creation churn, reported every ten seconds: how much of the ring a busy
// frame is recycling. Requests that arrive while disabled are not counted.
int Total_requested = 0;
int Total_killed = 0;
int next_message = -1;
#endif

// Reset everything between levels
void particle_init()
{
    Num_particles = 0;
    Next_particle = 0;

    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particles[i].type = -1;
    }

    if (Anim_bitmap_id_fire == -1) {
        int fps;
        Anim_bitmap_id_fire = bm_load_animation(
            "particleexp01", &Anim_num_frames_fire, &fps, 0);
    }

    if (Anim_bitmap_id_smoke == -1) {
        int fps;
        Anim_bitmap_id_smoke = bm_load_animation(
            "particlesmoke01", &Anim_num_frames_smoke, &fps, 0);
    }

    if (Anim_bitmap_id_smoke2 == -1) {
        int fps;
        Anim_bitmap_id_smoke2 = bm_load_animation(
            "particlesmoke02", &Anim_num_frames_smoke2, &fps, 0);
    }
}

// Force every frame of the shared animations resident, so that the first
// particle of a type does not stall on a texture load mid-mission.
void particle_page_in()
{
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particles[i].type = -1;
    }

    for (int i = 0; i < Anim_num_frames_fire; i++) {
        bm_page_in_texture(Anim_bitmap_id_fire + i);
    }

    for (int i = 0; i < Anim_num_frames_smoke; i++) {
        bm_page_in_texture(Anim_bitmap_id_smoke + i);
    }

    for (int i = 0; i < Anim_num_frames_smoke2; i++) {
        bm_page_in_texture(Anim_bitmap_id_smoke2 + i);
    }
}

// kill all active particles
void particle_kill_all()
{
    Num_particles = 0;
    Next_particle = 0;
    Num_particles_hwm = 0;

    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particles[i].type = -1;
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
            "Usage: particles [bool]\n"
            "Turns particle system on/off.  If nothing passed, then toggles "
            "it.\n");
    if (Dc_status)
        dc_printf("particles are %s\n", (Particles_enabled ? "ON" : "OFF"));
}

// Creates a single particle. See the PARTICLE_?? defines for types.
void particle_create(particle_info *pinfo)
{
#ifndef NDEBUG
    if (next_message == -1) {
        next_message = timestamp(10000);
    }

    if (timestamp_elapsed(next_message)) {
        next_message = timestamp(10000);
        if (Total_requested > 1) {
            nprintf(
                ("Particles", "Particles: Killed off %d%% of the particles\n",
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

    // Claim a slot. Walk forward from the cursor and evict the first
    // non-persistent particle met; persistent ones are stepped over. After a
    // third of the ring has refused us, give up and overwrite the very first
    // slot examined, persistent or not.
    //
    // Note that the give-up path does not decrement Num_particles for the
    // particle it destroys, while the increment below still runs, so the live
    // count drifts upward by one every time it is taken.
    particle_t *p;
    int         retry_count = 0;
    int         first_examined = Next_particle;

KillAnother:
    p = &Particles[Next_particle++];
    if (Next_particle >= MAX_PARTICLES) {
        Next_particle = 0;
    }

    if (p->type > -1) {
        if (p->type != PARTICLE_BITMAP_PERSISTENT) {
            p->type = -1;
            Num_particles--;
#ifndef NDEBUG
            Total_killed++;
#endif
        }
        else {
            retry_count++;
            if (retry_count < MAX_PARTICLES / 3) {
                goto KillAnother;
            }

            mprintf(("DELETING A PERSISTENT PARTICLE!!! This is ok if this "
                     "only happens rarely. Get John if not.\n"));
            Next_particle = first_examined;
            p = &Particles[Next_particle++];
            if (Next_particle >= MAX_PARTICLES) {
                Next_particle = 0;
            }
        }
    }

    Num_particles++;
    if (Num_particles > Num_particles_hwm) {
        Num_particles_hwm = Num_particles;

        if (Num_particles_hwm == MAX_PARTICLES) {
            mprintf(("All particle slots filled!\n"));
        }
    }

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
            // An animated bitmap lives exactly one play-through, whatever
            // lifetime the caller asked for.
            p->max_life = i2fl(p->nframes) / i2fl(fps);
        }
    }
    else {
        p->nframes = 1;
    }
}

// Convenience form: everything the attached/tracer fields describe is left off.
void particle_create(
    vector *pos, vector *vel, float lifetime, float rad, int type,
    uint optional_data)
{
    particle_info pinfo;

    pinfo.pos = *pos;
    pinfo.vel = *vel;
    pinfo.lifetime = lifetime;
    pinfo.rad = rad;
    pinfo.type = type;
    pinfo.optional_data = optional_data;

    pinfo.tracer_length = -1.0f;
    pinfo.attached_objnum = -1;
    pinfo.attached_sig = -1;
    pinfo.reverse = 0;

    particle_create(&pinfo);
}

MONITOR(NumParticles);

void particle_move_all(float frametime)
{
    MONITOR_INC(NumParticles, Num_particles);

    if (!Particles_enabled)
        return;

    particle_t *p = Particles;

    for (int i = 0; i < MAX_PARTICLES; i++, p++) {
        if (p->type == -1) {
            continue;
        }

        // An objnum past the end of the object table can only come from a
        // caller that stored garbage; drop the particle rather than index it.
        if (p->attached_objnum >= MAX_OBJECTS) {
            p->type = -1;
            Num_particles--;
            Assert(Num_particles >= 0);
            continue;
        }

        if (p->attached_objnum >= 0) {
            // Attached particles hold a slot index, which the object system
            // may have recycled; the signature is what proves it is still the
            // object we were attached to. Attached particles do not move on
            // their own -- they are carried by whatever they ride.
            if (p->attached_sig != Objects[p->attached_objnum].signature) {
                p->type = -1;
                Num_particles--;
                Assert(Num_particles >= 0);
                continue;
            }
        }
        else {
            vm_vec_scale_add2(&p->pos, &p->velocity, frametime);
        }

        p->age += frametime;

        if (p->age > p->max_life) {
            p->type = -1;
            Num_particles--;
            Assert(Num_particles >= 0);
        }
    }
}

MONITOR(NumParticlesRend);

void particle_render_all()
{
    particle_t *p;
    ubyte       flags;
    float       pct_complete;
    float       alpha;
    vertex      pos;
    vector      ts, te, temp;

    // Note that rotate is set up once for the whole sweep, not per particle,
    // and the branches below only ever clear it. The first tracer or attached
    // particle therefore turns off vertex rotation for every ordinary particle
    // behind it in the array, which then draws at whatever screen position was
    // last computed.
    int rotate = 1;

    if (!Particles_enabled)
        return;

    MONITOR_INC(NumParticlesRend, Num_particles);

    p = Particles;

    for (int i = 0; i < MAX_PARTICLES; i++, p++) {
        if (p->type == -1) {
            continue;
        }

        pct_complete = p->age / p->max_life;

        // A per-particle fade was intended here; it stayed a constant.
        alpha = 1.0f;

        // A tracer keeps its two world-space endpoints and is drawn as a rod,
        // so it needs no projected vertex at all.
        if (p->tracer_length > 0.0f) {
            ts = p->pos;
            temp = p->velocity;
            vm_vec_normalize_quick(&temp);
            vm_vec_scale_add(&te, &ts, &temp, p->tracer_length);

            rotate = 0;
        }
        // An attached particle stores its position in the object's frame, so
        // it has to be carried to world space before it can be projected.
        else if (p->attached_objnum >= 0) {
            vm_vec_unrotate(&temp, &p->pos, &Objects[p->attached_objnum].orient);
            vm_vec_add2(&temp, &Objects[p->attached_objnum].pos);

            flags = g3_rotate_vertex(&pos, &temp);
            if (flags) {
                continue;
            }

            rotate = 0;
        }

        if (rotate) {
            flags = g3_rotate_vertex(&pos, &p->pos);
            if (flags) {
                continue;
            }
        }

        switch (p->type) {
        case PARTICLE_DEBUG:
            gr_set_color(255, 0, 0);
            g3_draw_sphere_ez(&p->pos, p->radius);
            break;

        // The caller's own bitmap. This is the one type that honours nframes,
        // and the one type that ignores reverse.
        case PARTICLE_BITMAP:
        case PARTICLE_BITMAP_PERSISTENT: {
            int framenum = p->optional_data;

            if (p->nframes > 1) {
                int n = fl2i(pct_complete * p->nframes + 0.5);

                if (n < 0)
                    n = 0;
                else if (n > p->nframes - 1)
                    n = p->nframes - 1;

                framenum += n;
            }

            gr_set_bitmap(
                framenum, GR_ALPHABLEND_FILTER, GR_BITBLT_MODE_NORMAL, alpha);

            if (p->tracer_length > 0.0f) {
                g3_draw_laser(
                    &ts, p->radius, &te, p->radius,
                    TMAP_FLAG_TEXTURED | TMAP_FLAG_XPARENT, 25.0f);
            }
            else {
                g3_draw_bitmap(
                    &pos, (p - Particles) % 8, p->radius, TMAP_FLAG_TEXTURED);
            }
            break;
        }

        case PARTICLE_FIRE: {
            int framenum = fl2i(pct_complete * Anim_num_frames_fire + 0.5);

            if (framenum < 0)
                framenum = 0;
            else if (framenum > Anim_num_frames_fire - 1)
                framenum = Anim_num_frames_fire - 1;

            gr_set_bitmap(
                p->reverse
                    ? Anim_bitmap_id_fire + (Anim_num_frames_fire - framenum - 1)
                    : Anim_bitmap_id_fire + framenum,
                GR_ALPHABLEND_FILTER, GR_BITBLT_MODE_NORMAL, alpha);

            if (p->tracer_length > 0.0f) {
                g3_draw_laser(
                    &ts, p->radius, &te, p->radius,
                    TMAP_FLAG_TEXTURED | TMAP_FLAG_XPARENT, 25.0f);
            }
            else {
                g3_draw_bitmap(
                    &pos, (p - Particles) % 8, p->radius, TMAP_FLAG_TEXTURED);
            }
            break;
        }

        case PARTICLE_SMOKE: {
            int framenum = fl2i(pct_complete * Anim_num_frames_smoke + 0.5);

            if (framenum < 0)
                framenum = 0;
            else if (framenum > Anim_num_frames_smoke - 1)
                framenum = Anim_num_frames_smoke - 1;

            gr_set_bitmap(
                p->reverse ? Anim_bitmap_id_smoke +
                                 (Anim_num_frames_smoke - framenum - 1)
                           : Anim_bitmap_id_smoke + framenum,
                GR_ALPHABLEND_FILTER, GR_BITBLT_MODE_NORMAL, alpha);

            if (p->tracer_length > 0.0f) {
                g3_draw_laser(
                    &ts, p->radius, &te, p->radius,
                    TMAP_FLAG_TEXTURED | TMAP_FLAG_XPARENT, 25.0f);
            }
            else {
                g3_draw_bitmap(
                    &pos, (p - Particles) % 8, p->radius, TMAP_FLAG_TEXTURED);
            }
            break;
        }

        case PARTICLE_SMOKE2: {
            int framenum = fl2i(pct_complete * Anim_num_frames_smoke2 + 0.5);

            if (framenum < 0)
                framenum = 0;
            else if (framenum > Anim_num_frames_smoke2 - 1)
                framenum = Anim_num_frames_smoke2 - 1;

            gr_set_bitmap(
                p->reverse ? Anim_bitmap_id_smoke2 +
                                 (Anim_num_frames_smoke2 - framenum - 1)
                           : Anim_bitmap_id_smoke2 + framenum,
                GR_ALPHABLEND_FILTER, GR_BITBLT_MODE_NORMAL, alpha);

            if (p->tracer_length > 0.0f) {
                g3_draw_laser(
                    &ts, p->radius, &te, p->radius,
                    TMAP_FLAG_TEXTURED | TMAP_FLAG_XPARENT, 25.0f);
            }
            else {
                g3_draw_bitmap(
                    &pos, (p - Particles) % 8, p->radius, TMAP_FLAG_TEXTURED);
            }
            break;
        }
        }
    }
}

#if MAX_DETAIL_LEVEL != 4
#error Max details assumed to be 4 here
#endif

// Percentage of the caller's requested particle count to actually emit, by
// detail level. The top level deliberately exceeds 100.
int detail_max_num[5] = { 0, 50, 75, 100, 125 };

// Creates a bunch of particles. You pass a structure
// rather than a bunch of parameters.
void particle_emit(particle_emitter *pe, int type, uint optional_data, float range)
{
    if (!Particles_enabled)
        return;

    int percent = detail_max_num[Detail.num_particles];

    // Thin the emission with distance, so that distant effects cost less than
    // near ones. range lets a caller declare its effect bigger than it looks,
    // pushing back the distance at which the thinning starts.
    //Particle rendering drops out too soon.  Seems to be around 150 m.  Is it
    //detail level controllable?  I'd like it to be 500-1000
    float min_dist = 125.0f;
    float dist = vm_vec_dist_quick(&pe->pos, &Eye_position) / range;
    if (dist > min_dist) {
        percent = fl2i(i2fl(percent) * min_dist / dist);
        if (percent < 1) {
            return;
        }
    }

    int n1 = (pe->num_low * percent) / 100;
    int n2 = (pe->num_high * percent) / 100;

    int n = (rand() % (n2 - n1 + 1)) + n1;

    if (n < 1)
        return;

    for (int i = 0; i < n; i++) {
        float radius = ((pe->max_rad - pe->min_rad) * frand()) + pe->min_rad;
        float speed = ((pe->max_vel - pe->min_vel) * frand()) + pe->min_vel;
        float life = ((pe->max_life - pe->min_life) * frand()) + pe->min_life;

        // Scatter each particle around the emitter normal. normal_variance is
        // a raw per-axis offset before renormalising, so it is a cone width
        // only loosely: 0 emits straight along the normal, 1 emits anywhere.
        vector normal;
        normal.x = pe->normal.x + (frand() * 2.0f - 1.0f) * pe->normal_variance;
        normal.y = pe->normal.y + (frand() * 2.0f - 1.0f) * pe->normal_variance;
        normal.z = pe->normal.z + (frand() * 2.0f - 1.0f) * pe->normal_variance;
        vm_vec_normalize_safe(&normal);

        vector tmp_vel;
        vm_vec_scale_add(&tmp_vel, &pe->vel, &normal, speed);

        particle_create(&pe->pos, &tmp_vel, life, radius, type, optional_data);
    }
}
