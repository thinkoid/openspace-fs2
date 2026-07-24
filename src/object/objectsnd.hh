/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef __OBJECTSND_H__
#define __OBJECTSND_H__

#define OS_USED (1 << 0)
#define OS_DS3D (1 << 1)
#define OS_MAIN                                                                  \
    (1                                                                           \
     << 2) // "main" sound. attentuation does not apply until outside the radius of the object

extern int Obj_snd_enabled;

void obj_snd_level_init();
void obj_snd_level_close();
void obj_snd_do_frame();

// pos is the position of the sound source in the object's frame of reference.
// so, if the objp->pos was at the origin, the pos passed here would be the exact
// model coords of the location of the engine
// by passing vmd_zero_vector here, you get a sound centered directly on the object
// NOTE : if main is true, the attentuation factors don't apply if you're within the radius of the object
int obj_snd_assign(int objnum, int sndnum, vector *pos, int main);

// if sndnum is not -1, deletes all instances of the given sound within the object
void obj_snd_delete(int objnum, int sndnum = -1);

void obj_snd_delete_all();
void obj_snd_stop_all();
int obj_snd_is_playing(int index);
int obj_snd_return_instance(int index);

#endif
