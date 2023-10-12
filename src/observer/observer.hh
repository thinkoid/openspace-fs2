// -*- mode: c++; -*-

#ifndef FREESPACE2_OBSERVER_OBSERVER_HH
#define FREESPACE2_OBSERVER_OBSERVER_HH

#include <defs.hh>

class object;
struct vec3d;
struct matrix;

#define OBS_MAX_VEL_X (85.0f) // side to side
#define OBS_MAX_VEL_Y (85.0f) // side to side
#define OBS_MAX_VEL_Z (85.0f) // forwards and backwards

#define OBS_FLAG_USED (1 << 1)

struct observer  {
    int objnum;

    int target_objnum; // not used as of yet
    int flags;
};

#define MAX_OBSERVER_OBS 17
extern observer Observers[MAX_OBSERVER_OBS];

void observer_init ();
int observer_create (matrix* orient, vec3d* pos); // returns objnum
void observer_delete (object* obj);

// get the eye position and orientation for the passed observer object
void observer_get_eye (vec3d* eye_pos, matrix* eye_orient, object* obj);

#endif // FREESPACE2_OBSERVER_OBSERVER_HH
