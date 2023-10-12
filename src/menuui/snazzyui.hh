// -*- mode: c++; -*-

#ifndef FREESPACE2_MENUUI_SNAZZYUI_HH
#define FREESPACE2_MENUUI_SNAZZYUI_HH

#include <defs.hh>

#define MAX_CHAR 150
#define ESC_PRESSED -2

#include <gamesnd/gamesnd.hh>

struct MENU_REGION {
    int mask;            // mask color for the region
    int key;             // shortcut key for the region
    char text[MAX_CHAR]; // The text associated with this item.
    interface_snd_id
        click_sound; // Id of sound to play when mask area clicked on
};

// These are the actions thare are returned in the action parameter.
#define SNAZZY_OVER 1    // mouse is over a region
#define SNAZZY_CLICKED 2 // mouse button has gone from down to up over a region

int snazzy_menu_do (
    ubyte* data, int mask_w, int mask_h, int num_regions, MENU_REGION* regions,
    int* action, int poll_key = 1, int* key = NULL);
void read_menu_tbl (
    const char* menu_name, char* bkg_filename, char* mask_filename,
    MENU_REGION* regions, int* num_regions, int play_sound = 1);
void snazzy_menu_add_region (
    MENU_REGION* region, const char* text, int mask, int key,
    interface_snd_id click_sound = interface_snd_id ());

void snazzy_menu_init (); // Call the first time a snazzy menu is inited
void snazzy_menu_close ();
void snazzy_flush ();

#endif // FREESPACE2_MENUUI_SNAZZYUI_HH
