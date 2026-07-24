/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef _AIGOALS_H
#define _AIGOALS_H

#include <ship/ai.hh>
#include <cfile/cfile.hh>

// macros for goals which get set via sexpressions in the mission code

// IMPORTANT!  If you add a new AI_GOAL_x define, be sure to update the functions
// ai_update_goal_references() and query_referenced_in_ai_goals() or else risk breaking
// Fred.  If the goal you add doesn't have a target (such as chase_any), then you don't have
// to worry about doing this.  Also add it to list in Fred\Management.cpp, and let Hoffoss know!
#define AI_GOAL_CHASE (1 << 1)
// used for undocking as well
#define AI_GOAL_DOCK (1 << 2)
#define AI_GOAL_WAYPOINTS (1 << 3)
#define AI_GOAL_WAYPOINTS_ONCE (1 << 4)
#define AI_GOAL_WARP (1 << 5)
#define AI_GOAL_DESTROY_SUBSYSTEM (1 << 6)
#define AI_GOAL_FORM_ON_WING (1 << 7)
#define AI_GOAL_UNDOCK (1 << 8)
#define AI_GOAL_CHASE_WING (1 << 9)
#define AI_GOAL_GUARD (1 << 10)
#define AI_GOAL_DISABLE_SHIP (1 << 11)
#define AI_GOAL_DISARM_SHIP (1 << 12)
#define AI_GOAL_CHASE_ANY (1 << 13)
#define AI_GOAL_IGNORE (1 << 14)
#define AI_GOAL_GUARD_WING (1 << 15)
#define AI_GOAL_EVADE_SHIP (1 << 16)

// the next goals are for support ships only
#define AI_GOAL_STAY_NEAR_SHIP (1 << 17)
#define AI_GOAL_KEEP_SAFE_DISTANCE (1 << 18)
#define AI_GOAL_REARM_REPAIR (1 << 19)

#define AI_GOAL_STAY_STILL (1 << 20)
#define AI_GOAL_PLAY_DEAD (1 << 21)
#define AI_GOAL_CHASE_WEAPON (1 << 22)

// now the masks for ship types

#define AI_GOAL_ACCEPT_FIGHTER                                                   \
    (AI_GOAL_CHASE | AI_GOAL_WAYPOINTS | AI_GOAL_WAYPOINTS_ONCE | AI_GOAL_WARP | \
     AI_GOAL_DESTROY_SUBSYSTEM | AI_GOAL_CHASE_WING | AI_GOAL_GUARD |            \
     AI_GOAL_DISABLE_SHIP | AI_GOAL_DISARM_SHIP | AI_GOAL_CHASE_ANY |            \
     AI_GOAL_IGNORE | AI_GOAL_GUARD_WING | AI_GOAL_EVADE_SHIP |                  \
     AI_GOAL_STAY_STILL | AI_GOAL_PLAY_DEAD)
#define AI_GOAL_ACCEPT_BOMBER (AI_GOAL_ACCEPT_FIGHTER)
#define AI_GOAL_ACCEPT_STEALTH (AI_GOAL_ACCEPT_FIGHTER)
#define AI_GOAL_ACCEPT_TRANSPORT                                                 \
    (AI_GOAL_CHASE | AI_GOAL_CHASE_WING | AI_GOAL_DOCK | AI_GOAL_WAYPOINTS |     \
     AI_GOAL_WAYPOINTS_ONCE | AI_GOAL_WARP | AI_GOAL_UNDOCK |                    \
     AI_GOAL_STAY_STILL | AI_GOAL_PLAY_DEAD)
#define AI_GOAL_ACCEPT_FREIGHTER (AI_GOAL_ACCEPT_TRANSPORT)
#define AI_GOAL_ACCEPT_CRUISER (AI_GOAL_ACCEPT_FREIGHTER)
#define AI_GOAL_ACCEPT_CORVETTE (AI_GOAL_ACCEPT_CRUISER)
#define AI_GOAL_ACCEPT_GAS_MINER (AI_GOAL_ACCEPT_CRUISER)
#define AI_GOAL_ACCEPT_AWACS (AI_GOAL_ACCEPT_CRUISER)
#define AI_GOAL_ACCEPT_CAPITAL                                                   \
    (AI_GOAL_ACCEPT_CRUISER & ~(AI_GOAL_DOCK | AI_GOAL_UNDOCK))
#define AI_GOAL_ACCEPT_SUPERCAP (AI_GOAL_ACCEPT_CAPITAL)
#define AI_GOAL_ACCEPT_SUPPORT                                                   \
    (AI_GOAL_DOCK | AI_GOAL_UNDOCK | AI_GOAL_WAYPOINTS |                         \
     AI_GOAL_WAYPOINTS_ONCE | AI_GOAL_STAY_NEAR_SHIP |                           \
     AI_GOAL_KEEP_SAFE_DISTANCE | AI_GOAL_STAY_STILL | AI_GOAL_PLAY_DEAD)
#define AI_GOAL_ACCEPT_ESCAPEPOD (AI_GOAL_ACCEPT_TRANSPORT)

#define MAX_AI_DOCK_NAMES 25

extern int Num_ai_dock_names;
extern char Ai_dock_names[MAX_AI_DOCK_NAMES][NAME_LENGTH];

extern char *Ai_goal_text(int goal);

// extern function definitions
extern void ai_post_process_mission();
extern void ai_maybe_add_form_goal(wing *wingp);
extern void ai_process_mission_orders(int objnum, ai_info *aip);

// adds goals to ships/wing through sexpressions
extern void ai_add_ship_goal_sexp(int sexp, int type, ai_info *aip);
extern void ai_add_wing_goal_sexp(int sexp, int type, int wingnum);
extern void ai_add_goal_sub_sexp(int sexp, int type, ai_goal *aigp);

// adds goals to ships/sings through player orders
extern void ai_add_ship_goal_player(int type, int mode, int submode,
                                    char *shipname, ai_info *aip);
extern void ai_add_wing_goal_player(int type, int mode, int submode,
                                    char *shipname, int wingnum);

extern void ai_remove_ship_goal(ai_info *aip, int index);
extern void ai_clear_ship_goals(ai_info *aip);
extern void ai_clear_wing_goals(int wingnum);

extern void ai_copy_mission_wing_goal(ai_goal *aigp, ai_info *aip);

extern void ai_mission_goal_complete(ai_info *aip);
extern void ai_mission_wing_goal_complete(int wingnum, ai_goal *remove_goalp);

extern int ai_get_subsystem_type(char *subsystem);
extern char *ai_get_subsystem_type_name(int type);
extern void ai_update_goal_references(ai_goal *goals, int type, char *old_name,
                                      char *new_name);
extern int query_referenced_in_ai_goals(ai_goal *goals, int type, char *name);
extern char *ai_add_dock_name(char *str);

extern int ai_query_goal_valid(int ship, int ai_goal);

extern void ai_add_goal_ship_internal(ai_info *aip, int goal_type, char *name,
                                      int docker_point, int dockee_point,
                                      int immediate = 1);
extern void ai_add_goal_wing_internal(wing *wingp, int goal_type, char *name,
                                      int immediate = 1);

#endif
