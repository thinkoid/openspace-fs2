/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

int Num_jump_nodes = 0;

#include <object/object.hh>
#include <jumpnode/jumpnode.hh>
#include <model/model.hh>
#include <hud/hud.hh>

jump_node_struct Jump_nodes[MAX_JUMP_NODES];

void jumpnode_render(object *jumpnode_objp, vector *pos, vector *view_pos)
{
	jump_node_struct	*node;
	matrix				node_orient = IDENTITY_MATRIX;

	node = &Jump_nodes[jumpnode_objp->instance];

	if ( Fred_running ) {
		model_set_outline_color(0, 255, 0);		
		model_render(node->modelnum, &node_orient, pos, MR_NO_LIGHTING | MR_LOCK_DETAIL | MR_NO_POLYS | MR_SHOW_OUTLINE );
	} else {
		if ( view_pos ) {
			int alpha_index = HUD_color_alpha;

			// generate alpha index based on distance to jump node
			float dist;

			dist = vm_vec_dist_quick(view_pos, pos);

			// linearly interpolate alpha.  At 1000m or less, full intensity.  At 10000m or more 1/2 intensity.
			if ( dist < 1000 ) {
				alpha_index = HUD_COLOR_ALPHA_USER_MAX - 2;
			} else if ( dist > 10000 ) {
				alpha_index = HUD_COLOR_ALPHA_USER_MIN;
			} else {
				alpha_index = fl2i( HUD_COLOR_ALPHA_USER_MAX - 2 + (dist-1000) * (HUD_COLOR_ALPHA_USER_MIN-HUD_COLOR_ALPHA_USER_MAX-2) / (9000) + 0.5f);
				if ( alpha_index < HUD_COLOR_ALPHA_USER_MIN ) {
					alpha_index = HUD_COLOR_ALPHA_USER_MIN;
				}
			}

	//		nprintf(("Alan","alpha index is: %d\n", alpha_index));
			gr_set_color_fast(&HUD_color_defaults[alpha_index]);
//			model_set_outline_color(HUD_color_red, HUD_color_green, HUD_color_blue);

		} else {
			gr_set_color(HUD_color_red, HUD_color_green, HUD_color_blue);
		}
		model_render(node->modelnum, &node_orient, pos, MR_NO_LIGHTING | MR_LOCK_DETAIL | MR_NO_POLYS | MR_SHOW_OUTLINE_PRESET );
	}

}

// create a jump node object and return index to it.
int jumpnode_create(vector *pos)
{
	int obj;

	Assert(Num_jump_nodes < MAX_JUMP_NODES);

	Jump_nodes[Num_jump_nodes].modelnum = model_load(NOX("subspacenode.pof"), NULL, NULL);
	if ( Jump_nodes[Num_jump_nodes].modelnum < 0 ) {
		Int3();
		return -1;
	}

	obj = obj_create(OBJ_JUMP_NODE, -1, Num_jump_nodes, NULL, pos, model_get_radius(Jump_nodes[Num_jump_nodes].modelnum), OF_RENDERS);
	sprintf(Jump_nodes[Num_jump_nodes].name, XSTR( "Jump Node %d", 632), Num_jump_nodes);
	if (obj >= 0) {
		Jump_nodes[Num_jump_nodes].objnum = obj;
		Num_jump_nodes++;
	}
	return obj;
}

// only called by FRED
void jumpnode_render_all()
{
	int		i;
	object	*jumpnode_objp;

	for ( i = 0; i < Num_jump_nodes; i++ ) {	
		jumpnode_objp = &Objects[Jump_nodes[i].objnum];
		jumpnode_render(jumpnode_objp, &jumpnode_objp->pos);
	}
}
