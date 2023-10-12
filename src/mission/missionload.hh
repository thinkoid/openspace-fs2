// -*- mode: c++; -*-

#ifndef FREESPACE2_MISSION_MISSIONLOAD_HH
#define FREESPACE2_MISSION_MISSIONLOAD_HH

#include <defs.hh>

// -----------------------------------------------
// For recording most recent missions played
// -----------------------------------------------
#define MAX_RECENT_MISSIONS 10
extern char Recent_missions[MAX_RECENT_MISSIONS][MAX_FILENAME_LEN];
extern int Num_recent_missions;

extern std::vector< std::string > Ignored_missions;

// Mission_load takes no parameters.
// It sets the following global variables:
// Game_current_mission_filename

int mission_load (char* filename_ext);

bool mission_is_ignored (const char* filename);

// Functions for mission load menu
void mission_load_menu_init ();
void mission_load_menu_close ();
void mission_load_menu_do ();

#endif // FREESPACE2_MISSION_MISSIONLOAD_HH
