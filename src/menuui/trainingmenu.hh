// -*- mode: c++; -*-

#ifndef FREESPACE2_MENUUI_TRAININGMENU_HH
#define FREESPACE2_MENUUI_TRAININGMENU_HH

#include <defs.hh>

#define TRAINING_MENU_MAX_CHOICES \
    3 // keep up to date if any more choices added!

// these are the colour values of the pixels that form the different training
// menu regions
#define TRAINING_MENU_TRAINING_MISSIONS_MASK 1
#define TRAINING_MENU_REPLAY_MISSIONS_MASK 2
#define TRAINING_MENU_RETURN_MASK 3

// function prototypes
//
void training_menu_init ();
void training_menu_close ();
void training_menu_do_frame (float frametime);

#endif // FREESPACE2_MENUUI_TRAININGMENU_HH
