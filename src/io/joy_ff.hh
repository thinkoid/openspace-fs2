// -*- mode: c++; -*-

#ifndef FREESPACE2_IO_JOY_FF_HH
#define FREESPACE2_IO_JOY_FF_HH

#include <defs.hh>

int joy_ff_init ();
void joy_ff_shutdown ();
void joy_ff_stop_effects ();
void joy_ff_mission_init (vec3d v);
void joy_reacquire_ff ();
void joy_unacquire_ff ();
void joy_ff_play_vector_effect (vec3d* v, float scaler);
void joy_ff_play_dir_effect (float x, float y);
void joy_ff_play_primary_shoot (int gain);
void joy_ff_play_secondary_shoot (int gain);
void joy_ff_adjust_handling (int speed);
void joy_ff_docked ();
void joy_ff_play_reload_effect ();
void joy_ff_afterburn_on ();
void joy_ff_afterburn_off ();
void joy_ff_explode ();
void joy_ff_fly_by (int mag);
void joy_ff_deathroll ();

#endif // FREESPACE2_IO_JOY_FF_HH
