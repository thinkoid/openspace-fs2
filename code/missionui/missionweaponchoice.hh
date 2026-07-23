/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef __MISSION_WEAPON_CHOICE_H__
#define __MISSION_WEAPON_CHOICE_H__

#include <mission/missionparse.hh>
#include <missionui/missionscreencommon.hh>

// mask regions for icons in the scrollable lists
#define ICON_PRIMARY_0 28
#define ICON_PRIMARY_1 29
#define ICON_PRIMARY_2 30
#define ICON_PRIMARY_3 31
#define ICON_SECONDARY_0 10
#define ICON_SECONDARY_1 11
#define ICON_SECONDARY_2 12
#define ICON_SECONDARY_3 13

// mask regions for icons that sit above the ship
#define ICON_SHIP_PRIMARY_0 32
#define ICON_SHIP_PRIMARY_1 33
#define ICON_SHIP_PRIMARY_2 34
#define ICON_SHIP_SECONDARY_0 35
#define ICON_SHIP_SECONDARY_1 36
#define ICON_SHIP_SECONDARY_2 37
#define ICON_SHIP_SECONDARY_3 38

// mask region for weapon loadout specific buttons
#define PRIMARY_SCROLL_UP 27
#define PRIMARY_SCROLL_DOWN 26
#define SECONDARY_SCROLL_UP 9
#define SECONDARY_SCROLL_DOWN 8
#define WL_RESET_BUTTON_MASK 39

#define NUM_WEAPON_REGIONS (NUM_COMMON_REGIONS + 32)

void weapon_select_init();
void weapon_select_common_init();
void weapon_select_do(float frametime);
void weapon_select_close();

void wl_update_parse_object_weapons(p_object *pobjp, wss_unit *slot);
int wl_update_ship_weapons(int objnum, wss_unit *slot);
void wl_bash_ship_weapons(ship_weapon *swp, wss_unit *slot);

void wl_set_default_weapons(int index, int ship_class);
void wl_reset_to_defaults();

// Set selected slot to first placed ship
void wl_reset_selected_slot();

void wl_remove_weps_from_pool(int *wep, int *wep_count, int ship_class);
void wl_get_ship_class_weapons(int ship_class, int *wep, int *wep_count);
void wl_get_default_weapons(int ship_class, int slot_num, int *wep,
                            int *wep_count);

void wl_synch_interface();
void wl_apply(int mode, int from_bank, int from_list, int to_bank, int to_list,
              int ship_slot, int player_index = -1);
void wl_drop(int from_bank, int from_list, int to_bank, int to_list,
             int ship_slot, int player_index = -1);

#endif /* __MISSION_WEAPON_CHOICE_H__ */
