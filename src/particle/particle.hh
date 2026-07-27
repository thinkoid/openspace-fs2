/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef _PARTICLE_H
#define _PARTICLE_H

//============================================================================
//==================== PARTICLE SYSTEM GAME SEQUENCING CODE ==================
//============================================================================

// Resets particle system.  Call between levels.
void particle_init();

// Moves the particles for each frame
void particle_move_all(float frametime);

// Renders all the particles
void particle_render_all();

// kill all active particles
void particle_kill_all();

//============================================================================
//=============== LOW-LEVEL SINGLE PARTICLE CREATION CODE ====================
//============================================================================

// The different types of particles...
#define PARTICLE_DEBUG 0 // A red sphere, no optional data required
#define PARTICLE_BITMAP                                                          \
    1 // A bitmap, optional data is the bitmap number.  If bitmap is an animation,
        // lifetime is calculated by the number of frames and fps.
#define PARTICLE_FIRE 2 // The vclip used for explosions, optional means nothing
#define PARTICLE_SMOKE 3 // The vclip used for smoke, optional means nothing
#define PARTICLE_SMOKE2 4 // The vclip used for smoke, optional means nothing
#define PARTICLE_BITMAP_PERSISTENT                                               \
    5 // A bitmap, optional data is the bitmap number.  If bitmap is an animation,
        // lifetime is calculated by the number of frames and fps.

// particle creation stuff
typedef struct particle_info
{
    // old-style particle info
    vector pos;
    vector vel;
    float lifetime;
    float rad;
    int type;
    uint optional_data;

    // new-style particle info
    float tracer_length;
    short attached_objnum; // if these are set, the pos is relative to the pos of the origin of the attached object
    int attached_sig; // to make sure the object hasn't changed or died. velocity is ignored in this case
    ubyte reverse; // play any animations in reverse
} particle_info;

// Creates a single particle. See the PARTICLE_?? defines for types.
void particle_create(particle_info *pinfo);
void particle_create(vector *pos, vector *vel, float lifetime, float rad,
                     int type, uint optional_data = 0);

//============================================================================
//============== HIGH-LEVEL PARTICLE SYSTEM CREATION CODE ====================
//============================================================================

// Use a structure rather than pass a ton of parameters to particle_emit
typedef struct particle_emitter
{
    int num_low; // Lowest number of particles to create
    int num_high; // Highest number of particles to create
    vector pos; // Where the particles emit from
    vector vel; // Initial velocity of all the particles
    float min_life; // How long the particles live
    float max_life; // How long the particles live
    vector normal; // What normal the particle emit arond
    float normal_variance; //	How close they stick to that normal 0=good, 1=360 degree
    float min_vel; // How fast the slowest particle can move
    float max_vel; // How fast the fastest particle can move
    float min_rad; // Min radius
    float max_rad; // Max radius
} particle_emitter;

// Creates a bunch of particles. You pass a structure
// rather than a bunch of parameters.
void particle_emit(particle_emitter *pe, int type, uint optional_data,
                   float range = 1.0);

#endif // _PARTICLE_H
