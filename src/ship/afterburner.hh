// -*- mode: c++; -*-

#ifndef FREESPACE2_SHIP_AFTERBURNER_HH
#define FREESPACE2_SHIP_AFTERBURNER_HH

#include <defs.hh>

#define AFTERBURNER_MIN_DELAY 1000
#define ABURN_DECAY_TIME \
    1500 // time in ms ship shakes after afterburner pressed

class object;

void afterburners_start (object* objp);
void afterburners_stop (object* objp, int key_released = 0);
void afterburner_stop_sounds ();
void afterburners_update (object* objp, float fl_frametime);
void afterburner_level_init ();

#endif // FREESPACE2_SHIP_AFTERBURNER_HH
