/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef __FREESPACE_HUDSHIELD_H__
#define __FREESPACE_HUDSHIELD_H__

#define SHIELD_HIT_DURATION 1400 // time a shield quadrant flashes after being hit
#define SHIELD_FLASH_INTERVAL 200 // time between shield quadrant flashes

#define NUM_SHIELD_HIT_MEMBERS 5
#define HULL_HIT_OFFSET                                                          \
    4 // used to access the members in shield_hit_info that pertain to the hull
typedef struct shield_hit_info
{
    int shield_hit_status; // bitfield, if offset for shield quadrant is set, that means shield is being hit
    int shield_show_bright; // bitfield, if offset for shield quadrant is set, that means play bright frame
    int shield_hit_timers
        [NUM_SHIELD_HIT_MEMBERS]; // timestamps that get set for SHIELD_FLASH_TIME when a quadrant is hit
    int shield_hit_next_flash[NUM_SHIELD_HIT_MEMBERS];
} shield_hit_info;

extern ubyte Quadrant_xlate[4];

struct player;

void hud_shield_game_init();
void hud_shield_level_init();
void hud_shield_show(object *objp);
void hud_shield_equalize(object *objp, player *pl);
void hud_augment_shield_quadrant(object *objp, int direction);
void hud_shield_assign_info(ship_info *sip, char *filename);
void hud_show_mini_ship_integrity(object *objp, int force_x = -1,
                                  int force_y = -1);
void hud_shield_show_mini(object *objp, int x_force = -1, int y_force = -1,
                          int x_hull_offset = 0, int y_hull_offset = 0);
void hud_shield_hit_update();
void hud_shield_quadrant_hit(object *objp, int quadrant);
void hud_shield_hit_reset(int player = 0);

void shield_info_reset(shield_hit_info *shi);

#endif /* __FREESPACE_HUDSHIELDBOX_H__ */
