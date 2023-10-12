// -*- mode: c++; -*-

#ifndef FREESPACE2_MISSION_MISSIONHOTKEY_HH
#define FREESPACE2_MISSION_MISSIONHOTKEY_HH

#include <defs.hh>

void mission_hotkey_init ();
void mission_hotkey_close ();
void mission_hotkey_do_frame (float frametime);
void mission_hotkey_set_defaults ();
void mission_hotkey_validate ();
void mission_hotkey_maybe_save_sets ();
void mission_hotkey_reset_saved ();
void mission_hotkey_mf_add (int set, int objnum, int how_to_add);

void mission_hotkey_exit ();

// function to return the hotkey set number of the given key
extern int mission_hotkey_get_set_num (int k);

extern int Hotkey_overlay_id;

#endif // FREESPACE2_MISSION_MISSIONHOTKEY_HH
