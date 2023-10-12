// -*- mode: c++; -*-

#ifndef FREESPACE2_SHIP_AWACS_HH
#define FREESPACE2_SHIP_AWACS_HH

#include <defs.hh>

// ----------------------------------------------------------------------------------------------------
// AWACS DEFINES/VARS
//

class object;
class ship;

// DAVE'S OFFICIAL DEFINITION OF AWACS

// total awacs levels for all teams
extern float Awacs_team[MAX_IFFS]; // total AWACS capabilities for each team
extern float Awacs_level;          // Awacs_friendly - Awacs_hostile

// ----------------------------------------------------------------------------------------------------
// AWACS FUNCTIONS
//

// call when initializing level, before parsing mission
void awacs_level_init ();

// call every frame to process AWACS details
void awacs_process ();

// get the total AWACS level for target to viewer
// < 0.0f               : untargetable
// 0.0 - 1.0f   : marginally targetable
// 1.0f                 : fully targetable as normal
float awacs_get_level (object* target, ship* viewer, int use_awacs = 1);

// Determine if ship is visible by team
// return 1 if ship is fully visible
// return 0 if ship is only partly visible
int ship_is_visible_by_team (object* target, ship* viewer);

#endif // FREESPACE2_SHIP_AWACS_HH
