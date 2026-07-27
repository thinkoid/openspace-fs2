/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/


#ifndef __FREESPACE_SWARM_H__
#define __FREESPACE_SWARM_H__

#include <object/object.hh>
#include <cfile/cfile.hh>
#include <ship/ship.hh>

#define SWARM_DEFAULT_NUM_MISSILES_FIRED					4		// number of swarm missiles that launch when fired

void	swarm_level_init();
void	swarm_delete(int index);
int	swarm_create();
void	swarm_update_direction(object *objp, float frametime);
void	swarm_maybe_fire_missile(int shipnum);

int	turret_swarm_create();
void	turret_swarm_delete(int i);
void	turret_swarm_set_up_info(int parent_objnum, ship_subsys *turret, int turret_weapon_class);
void	turret_swarm_check_validity();

#endif /* __FREESPACE_SWARM_H__ */
