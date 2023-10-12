// -*- mode: c++; -*-

#ifndef FREESPACE2_WEAPON_CORKSCREW_HH
#define FREESPACE2_WEAPON_CORKSCREW_HH

#include <defs.hh>

class object;

extern int Corkscrew_num_missiles_fired;

void cscrew_level_init ();
void cscrew_delete (int index);
int cscrew_create (object* obj);

// pre process the corkscrew weapon by putting him in the "center" of his
// corkscrew
void cscrew_process_pre (object* objp);

// post process the corkscrew weapon by putting him back to the right spot on
// his corkscrew
void cscrew_process_post (object* objp);

// maybe fire another corkscrew-style missile
void cscrew_maybe_fire_missile (int shipnum);

#endif // FREESPACE2_WEAPON_CORKSCREW_HH
