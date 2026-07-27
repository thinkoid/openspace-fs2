/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#ifndef __JUMPNODE_H__
#define __JUMPNODE_H__

#include <parse/parselo.hh>

#define MAX_JUMP_NODES	3

typedef struct {
	int	modelnum;
	int	objnum;						// objnum of this jump node
	char	name[NAME_LENGTH];
} jump_node_struct;

extern int Num_jump_nodes;
extern jump_node_struct Jump_nodes[MAX_JUMP_NODES];

int	jumpnode_create(vector *pos);
void	jumpnode_render(object *jumpnode_objp, vector *pos, vector *view_pos = NULL);
void	jumpnode_render_all();	// called by FRED

#endif
