// -*- mode: c++; -*-

#ifndef FREESPACE2_MENUUI_TECHMENU_HH
#define FREESPACE2_MENUUI_TECHMENU_HH

#include <defs.hh>

#define MAX_INTEL_ENTRIES 75
#define TECH_INTEL_DESC_LEN 5120

struct intel_data {
    char name[NAME_LENGTH];
    char desc[TECH_INTEL_DESC_LEN];
    char anim_filename[NAME_LENGTH];
    int flags;
};

// flags by Goober5000
#define IIF_DEFAULT_VALUE 0
#define IIF_IN_TECH_DATABASE (1 << 0) // in tech database? - Goober5000
#define IIF_DEFAULT_IN_TECH_DATABASE \
    (1 << 1) // in tech database by default? - Goober5000

extern int Techroom_overlay_id;

extern intel_data Intel_info[MAX_INTEL_ENTRIES];
extern int Intel_info_size;

// function prototypes
void techroom_init ();
void techroom_close ();
void techroom_do_frame (float frametime);
int techroom_on_ships_tab ();
void techroom_intel_init ();  // called on startup so campaigns can manipulate
                              // tech room visibility
void techroom_intel_reset (); // for testing
int intel_info_lookup (char* name);
extern void tech_reset_to_default ();

#endif // FREESPACE2_MENUUI_TECHMENU_HH
