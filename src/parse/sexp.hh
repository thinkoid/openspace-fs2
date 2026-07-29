/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef _SEXP_H
#define _SEXP_H

#include <cfile/cfile.hh>
#include <ship/ship.hh>

// if this ever exceeds TOKEN_LENGTH, let JasonH know!
#define OPERATOR_LENGTH 24
#define TOKEN_LENGTH 32

// Reduced from 2000 to 1200 by MK on 4/1/98.
//  Most used nodes is 698 in sm1-10a.  Sandeep thinks that's the most complex mission.
// AL 2-4-98: upped to 1600, btm03 ran out of sexps, since campaign took a bunch
// DA 12/15 bumped up to 2000 - Dan ran out
#define MAX_SEXP_NODES 2200

#define MAX_SEXP_VARIABLES 100

#define MAX_SEXP_TEXT 2000
// Yes, this is used, but not by the Sexp code.
#define MAX_OPERATORS 200

// Operator argument formats (data types of an argument)
// argument cannot exist at this position if it's this
#define OPF_NONE 1
// no value.  Can still be used for type matching, however
#define OPF_NULL 2
#define OPF_BOOL 3
#define OPF_NUMBER 4
#define OPF_SHIP 5
#define OPF_WING 6
#define OPF_SUBSYSTEM 7
// either a 3d point in space, or a waypoint name
#define OPF_POINT 8
#define OPF_IFF 9
// special to match ai goals
#define OPF_AI_GOAL 10
// docking point on docker ship
#define OPF_DOCKER_POINT 11
// docking point on dockee ship
#define OPF_DOCKEE_POINT 12
// the name (id) of a message in Messages[] array
#define OPF_MESSAGE 13
// who sent the message -- doesn't necessarily have to be a ship!!!
#define OPF_WHO_FROM 14
// priority for messages
#define OPF_PRIORITY 15
// name of a waypoint
#define OPF_WAYPOINT_PATH 16
// positive number or zero
#define OPF_POSITIVE 17
// name of a mission for various mission related things
#define OPF_MISSION_NAME 18
// a waypoint or a ship
#define OPF_SHIP_POINT 19
// name of goal (or maybe event?) from a mission
#define OPF_GOAL_NAME 20
// either a ship or wing name (they don't conflict)
#define OPF_SHIP_WING 21
// name of a ship, wing, or a point
#define OPF_SHIP_WING_POINT 22
// type of ship (fighter/bomber/etc)
#define OPF_SHIP_TYPE 23
// a default key
#define OPF_KEYPRESS 24
// name of an event
#define OPF_EVENT_NAME 25
// a squadmsg order player can give to a ship
#define OPF_AI_ORDER 26
// current skill level of the game
#define OPF_SKILL_LEVEL 27
// name of medals
#define OPF_MEDAL_NAME 28
// name of a weapon
#define OPF_WEAPON_NAME 29
// name of a ship class
#define OPF_SHIP_CLASS_NAME 30
// name of HUD gauge
#define OPF_HUD_GAUGE_NAME 31
// name of a secondary bomb type weapon
#define OPF_HUGE_WEAPON 32
// a ship, but not a player ship
#define OPF_SHIP_NOT_PLAYER 33
// name of a jump node
#define OPF_JUMP_NODE_NAME 34
// variable name
#define OPF_VARIABLE_NAME 35
// type used with variable
#define OPF_AMBIGUOUS 36
// an awacs subsystem
#define OPF_AWACS_SUBSYSTEM 37

// Operand return types
// returns number
#define OPR_NUMBER 1
// returns true/false value
#define OPR_BOOL 2
// doesn't return a value
#define OPR_NULL 3
// is an ai operator (doesn't really return a value, but used for type matching)
#define OPR_AI_GOAL 4
// returns a non-negative number
#define OPR_POSITIVE 5
// not really a return type, but used for type matching.
#define OPR_STRING 6
// not really a return type, but used for type matching.
#define OPR_AMBIGUOUS 7

#define OP_INSERT_FLAG 0x8000
#define OP_REPLACE_FLAG 0x4000
#define OP_NONCAMPAIGN_FLAG 0x2000
#define OP_CAMPAIGN_ONLY_FLAG 0x1000
#define FIRST_OP 0x0100
#define OP_CATAGORY_MASK 0x0f00

#define OP_CATAGORY_OBJECTIVE 0x0100
#define OP_CATAGORY_TIME 0x0200
#define OP_CATAGORY_LOGICAL 0x0300
#define OP_CATAGORY_ARITHMETIC 0x0400
#define OP_CATAGORY_STATUS 0x0500
#define OP_CATAGORY_CHANGE 0x0600
#define OP_CATAGORY_CONDITIONAL 0x0700
#define OP_CATAGORY_DEBUG 0x0800
// used for AI goals
#define OP_CATAGORY_AI 0x0900
#define OP_CATAGORY_TRAINING 0x0a00
#define OP_CATAGORY_UNLISTED 0x0b00
#define OP_CATAGORY_GOAL_EVENT 0x0c00

#define OP_PLUS (0x0000 | OP_CATAGORY_ARITHMETIC)
#define OP_MINUS (0x0001 | OP_CATAGORY_ARITHMETIC)
#define OP_MOD (0x0002 | OP_CATAGORY_ARITHMETIC)
#define OP_MUL (0x0003 | OP_CATAGORY_ARITHMETIC)
#define OP_DIV (0x0004 | OP_CATAGORY_ARITHMETIC)
#define OP_RAND (0x0005 | OP_CATAGORY_ARITHMETIC)

#define OP_TRUE (0x0000 | OP_CATAGORY_LOGICAL)
#define OP_FALSE (0x0001 | OP_CATAGORY_LOGICAL)
#define OP_AND (0x0002 | OP_CATAGORY_LOGICAL)
#define OP_AND_IN_SEQUENCE (0x0003 | OP_CATAGORY_LOGICAL)
#define OP_OR (0x0004 | OP_CATAGORY_LOGICAL)
#define OP_EQUALS (0x0005 | OP_CATAGORY_LOGICAL)
#define OP_GREATER_THAN (0x0006 | OP_CATAGORY_LOGICAL)
#define OP_LESS_THAN (0x0007 | OP_CATAGORY_LOGICAL)
#define OP_IS_IFF (0x0008 | OP_CATAGORY_LOGICAL | OP_NONCAMPAIGN_FLAG)
#define OP_HAS_TIME_ELAPSED (0x0009 | OP_CATAGORY_LOGICAL | OP_NONCAMPAIGN_FLAG)
#define OP_NOT (0x000a | OP_CATAGORY_LOGICAL)

#define OP_GOAL_INCOMPLETE (0x0000 | OP_CATAGORY_GOAL_EVENT | OP_NONCAMPAIGN_FLAG)
#define OP_GOAL_TRUE_DELAY (0x0001 | OP_CATAGORY_GOAL_EVENT | OP_NONCAMPAIGN_FLAG)
#define OP_GOAL_FALSE_DELAY                                                      \
    (0x0002 | OP_CATAGORY_GOAL_EVENT | OP_NONCAMPAIGN_FLAG)
#define OP_EVENT_INCOMPLETE                                                      \
    (0x0003 | OP_CATAGORY_GOAL_EVENT | OP_NONCAMPAIGN_FLAG)
#define OP_EVENT_TRUE_DELAY                                                      \
    (0x0004 | OP_CATAGORY_GOAL_EVENT | OP_NONCAMPAIGN_FLAG)
#define OP_EVENT_FALSE_DELAY                                                     \
    (0x0005 | OP_CATAGORY_GOAL_EVENT | OP_NONCAMPAIGN_FLAG)
#define OP_PREVIOUS_EVENT_TRUE (0x0006 | OP_CATAGORY_GOAL_EVENT)
#define OP_PREVIOUS_EVENT_FALSE (0x0007 | OP_CATAGORY_GOAL_EVENT)
#define OP_PREVIOUS_GOAL_TRUE (0x0009 | OP_CATAGORY_GOAL_EVENT)
#define OP_PREVIOUS_GOAL_FALSE (0x000a | OP_CATAGORY_GOAL_EVENT)

#define OP_IS_DESTROYED_DELAY                                                    \
    (0x0008 | OP_CATAGORY_OBJECTIVE | OP_NONCAMPAIGN_FLAG)
#define OP_IS_SUBSYSTEM_DESTROYED_DELAY                                          \
    (0x0009 | OP_CATAGORY_OBJECTIVE | OP_NONCAMPAIGN_FLAG)
#define OP_IS_DISABLED_DELAY                                                     \
    (0x000a | OP_CATAGORY_OBJECTIVE | OP_NONCAMPAIGN_FLAG)
#define OP_IS_DISARMED_DELAY                                                     \
    (0x000b | OP_CATAGORY_OBJECTIVE | OP_NONCAMPAIGN_FLAG)
#define OP_HAS_DOCKED_DELAY (0x000c | OP_CATAGORY_OBJECTIVE | OP_NONCAMPAIGN_FLAG)
#define OP_HAS_UNDOCKED_DELAY                                                    \
    (0x000d | OP_CATAGORY_OBJECTIVE | OP_NONCAMPAIGN_FLAG)
#define OP_HAS_ARRIVED_DELAY                                                     \
    (0x000e | OP_CATAGORY_OBJECTIVE | OP_NONCAMPAIGN_FLAG)
#define OP_HAS_DEPARTED_DELAY                                                    \
    (0x000f | OP_CATAGORY_OBJECTIVE | OP_NONCAMPAIGN_FLAG)
#define OP_WAYPOINTS_DONE_DELAY                                                  \
    (0x0011 | OP_CATAGORY_OBJECTIVE | OP_NONCAMPAIGN_FLAG)
#define OP_SHIP_TYPE_DESTROYED                                                   \
    (0x0012 | OP_CATAGORY_OBJECTIVE | OP_NONCAMPAIGN_FLAG)
#define OP_PERCENT_SHIPS_DEPARTED                                                \
    (0x0013 | OP_CATAGORY_OBJECTIVE | OP_NONCAMPAIGN_FLAG)
#define OP_PERCENT_SHIPS_DESTROYED                                               \
    (0x0014 | OP_CATAGORY_OBJECTIVE | OP_NONCAMPAIGN_FLAG)
#define OP_DEPART_NODE_DELAY                                                     \
    (0x0015 | OP_CATAGORY_OBJECTIVE | OP_NONCAMPAIGN_FLAG)
#define OP_DESTROYED_DEPARTED_DELAY                                              \
    (0x0016 | OP_CATAGORY_OBJECTIVE | OP_NONCAMPAIGN_FLAG)

#define OP_TIME_SHIP_DESTROYED (0x0000 | OP_CATAGORY_TIME | OP_NONCAMPAIGN_FLAG)
#define OP_TIME_SHIP_ARRIVED (0x0001 | OP_CATAGORY_TIME | OP_NONCAMPAIGN_FLAG)
#define OP_TIME_SHIP_DEPARTED (0x0002 | OP_CATAGORY_TIME | OP_NONCAMPAIGN_FLAG)
#define OP_TIME_WING_DESTROYED (0x0003 | OP_CATAGORY_TIME | OP_NONCAMPAIGN_FLAG)
#define OP_TIME_WING_ARRIVED (0x0004 | OP_CATAGORY_TIME | OP_NONCAMPAIGN_FLAG)
#define OP_TIME_WING_DEPARTED (0x0005 | OP_CATAGORY_TIME | OP_NONCAMPAIGN_FLAG)
#define OP_MISSION_TIME (0x0006 | OP_CATAGORY_TIME | OP_NONCAMPAIGN_FLAG)
#define OP_TIME_DOCKED (0x0007 | OP_CATAGORY_TIME | OP_NONCAMPAIGN_FLAG)
#define OP_TIME_UNDOCKED (0x0008 | OP_CATAGORY_TIME | OP_NONCAMPAIGN_FLAG)

#define OP_SHIELDS_LEFT (0x0000 | OP_CATAGORY_STATUS | OP_NONCAMPAIGN_FLAG)
#define OP_HITS_LEFT (0x0001 | OP_CATAGORY_STATUS | OP_NONCAMPAIGN_FLAG)
#define OP_HITS_LEFT_SUBSYSTEM (0x0002 | OP_CATAGORY_STATUS | OP_NONCAMPAIGN_FLAG)
#define OP_DISTANCE (0x0003 | OP_CATAGORY_STATUS | OP_NONCAMPAIGN_FLAG)
#define OP_LAST_ORDER_TIME (0x0004 | OP_CATAGORY_STATUS | OP_NONCAMPAIGN_FLAG)
#define OP_NUM_PLAYERS (0x0005 | OP_CATAGORY_STATUS | OP_NONCAMPAIGN_FLAG)
#define OP_SKILL_LEVEL_AT_LEAST                                                  \
    (0x0006 | OP_CATAGORY_STATUS | OP_NONCAMPAIGN_FLAG)
#define OP_WAS_PROMOTION_GRANTED                                                 \
    (0x0008 | OP_CATAGORY_STATUS | OP_NONCAMPAIGN_FLAG)
#define OP_WAS_MEDAL_GRANTED (0x0009 | OP_CATAGORY_STATUS | OP_NONCAMPAIGN_FLAG)
#define OP_CARGO_KNOWN_DELAY (0x000a | OP_CATAGORY_STATUS | OP_NONCAMPAIGN_FLAG)
#define OP_CAP_SUBSYS_CARGO_KNOWN_DELAY                                          \
    (0x000b | OP_CATAGORY_STATUS | OP_NONCAMPAIGN_FLAG)
#define OP_HAS_BEEN_TAGGED_DELAY                                                 \
    (0x000c | OP_CATAGORY_STATUS | OP_NONCAMPAIGN_FLAG)
#define OP_IS_TAGGED (0x000d | OP_CATAGORY_STATUS | OP_NONCAMPAIGN_FLAG)
#define OP_NUM_KILLS (0x000e | OP_CATAGORY_STATUS | OP_NONCAMPAIGN_FLAG)
#define OP_NUM_TYPE_KILLS (0x000f | OP_CATAGORY_STATUS | OP_NONCAMPAIGN_FLAG)
#define OP_NUM_CLASS_KILLS (0x0010 | OP_CATAGORY_STATUS | OP_NONCAMPAIGN_FLAG)
#define OP_SHIELD_RECHARGE_PCT (0x0011 | OP_CATAGORY_STATUS | OP_NONCAMPAIGN_FLAG)
#define OP_ENGINE_RECHARGE_PCT (0x0012 | OP_CATAGORY_STATUS | OP_NONCAMPAIGN_FLAG)
#define OP_WEAPON_RECHARGE_PCT (0x0013 | OP_CATAGORY_STATUS | OP_NONCAMPAIGN_FLAG)
#define OP_SHIELD_QUAD_LOW (0x0014 | OP_CATAGORY_STATUS | OP_NONCAMPAIGN_FLAG)
#define OP_SECONDARY_AMMO_PCT (0x0015 | OP_CATAGORY_STATUS | OP_NONCAMPAIGN_FLAG)
#define OP_IS_SECONDARY_SELECTED                                                 \
    (0x0016 | OP_CATAGORY_STATUS | OP_NONCAMPAIGN_FLAG)
#define OP_IS_PRIMARY_SELECTED (0x0017 | OP_CATAGORY_STATUS | OP_NONCAMPAIGN_FLAG)
#define OP_SPECIAL_WARP_DISTANCE                                                 \
    (0X0018 | OP_CATAGORY_STATUS | OP_NONCAMPAIGN_FLAG)
#define OP_IS_SHIP_VISIBLE (0X0019 | OP_CATAGORY_STATUS | OP_NONCAMPAIGN_FLAG)
#define OP_TEAM_SCORE (0X0020 | OP_CATAGORY_STATUS | OP_NONCAMPAIGN_FLAG)

// conditional sexpressions
#define OP_WHEN (0x0000 | OP_CATAGORY_CONDITIONAL)

// sexpressions with side-effects
#define OP_CHANGE_IFF (0x0000 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_REPAIR_SUBSYSTEM (0x0001 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_SABOTAGE_SUBSYSTEM (0x0002 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_SET_SUBSYSTEM_STRNGTH                                                 \
    (0x0003 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_PROTECT_SHIP (0x0004 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_SEND_MESSAGE (0x0005 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_SELF_DESTRUCT (0x0006 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_CLEAR_GOALS (0x0007 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_ADD_GOAL (0x0008 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_INVALIDATE_GOAL (0x0009 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_VALIDATE_GOAL (0x000a | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_SEND_RANDOM_MESSAGE (0x000b | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_TRANSFER_CARGO (0x000c | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_EXCHANGE_CARGO (0x000d | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_UNPROTECT_SHIP (0x000e | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_GOOD_REARM_TIME (0x0010 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_BAD_REARM_TIME (0x0011 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_GRANT_PROMOTION (0x0012 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_GRANT_MEDAL (0x0013 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_ALLOW_SHIP (0x0014 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_ALLOW_WEAPON (0x0015 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_GOOD_SECONDARY_TIME (0x0016 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_WARP_BROKEN (0x0017 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_WARP_NOT_BROKEN (0x0018 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_WARP_NEVER (0x0019 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_WARP_ALLOWED (0x0020 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_SHIP_INVISIBLE (0x0021 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_SHIP_VISIBLE (0x0022 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_SHIP_INVULNERABLE (0x0023 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_SHIP_VULNERABLE (0x0024 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_RED_ALERT (0x0025 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_TECH_ADD_SHIP (0x0026 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_TECH_ADD_WEAPON (0x0027 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_END_CAMPAIGN (0x0028 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_JETTISON_CARGO (0x0029 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_MODIFY_VARIABLE (0X0030 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_NOP (0x0031 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_BEAM_FIRE (0x0032 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_BEAM_FREE (0x0033 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_BEAM_FREE_ALL (0x0034 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_BEAM_LOCK (0x0035 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_BEAM_LOCK_ALL (0x0036 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_BEAM_PROTECT_SHIP (0x0037 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_BEAM_UNPROTECT_SHIP (0x0038 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_TURRET_FREE (0x0039 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_TURRET_FREE_ALL (0x0040 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_TURRET_LOCK (0x0041 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_TURRET_LOCK_ALL (0x0042 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_ADD_REMOVE_ESCORT (0x0043 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_AWACS_SET_RADIUS (0x0044 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_SEND_MESSAGE_LIST (0x0045 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_CAP_WAYPOINT_SPEED (0x0046 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_SHIP_GUARDIAN (0x0047 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_SHIP_NO_GUARDIAN (0x0048 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_TURRET_TAGGED_ONLY_ALL                                                \
    (0x0049 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_TURRET_TAGGED_CLEAR_ALL                                               \
    (0x0050 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_SUBSYS_SET_RANDOM (0x0051 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_SUPERNOVA_START (0x0052 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_CARGO_NO_DEPLETE (0x0053 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_SET_SPECIAL_WARPOUT_NAME                                              \
    (0X0054 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)
#define OP_SHIP_VANISH (0X0055 | OP_CATAGORY_CHANGE | OP_NONCAMPAIGN_FLAG)

// debugging sexpressions
#define OP_INT3 (0x0000 | OP_CATAGORY_DEBUG)

// defined for AI goals
#define OP_AI_CHASE (0x0000 | OP_CATAGORY_AI | OP_NONCAMPAIGN_FLAG)
#define OP_AI_DOCK (0x0001 | OP_CATAGORY_AI | OP_NONCAMPAIGN_FLAG)
#define OP_AI_UNDOCK (0x0002 | OP_CATAGORY_AI | OP_NONCAMPAIGN_FLAG)
#define OP_AI_WARP_OUT (0x0003 | OP_CATAGORY_AI | OP_NONCAMPAIGN_FLAG)
#define OP_AI_WAYPOINTS (0x0004 | OP_CATAGORY_AI | OP_NONCAMPAIGN_FLAG)
#define OP_AI_WAYPOINTS_ONCE (0x0005 | OP_CATAGORY_AI | OP_NONCAMPAIGN_FLAG)
#define OP_AI_DESTROY_SUBSYS (0x0006 | OP_CATAGORY_AI | OP_NONCAMPAIGN_FLAG)
#define OP_AI_DISABLE_SHIP (0x0008 | OP_CATAGORY_AI | OP_NONCAMPAIGN_FLAG)
#define OP_AI_DISARM_SHIP (0x0009 | OP_CATAGORY_AI | OP_NONCAMPAIGN_FLAG)
#define OP_AI_GUARD (0x000a | OP_CATAGORY_AI | OP_NONCAMPAIGN_FLAG)
#define OP_AI_CHASE_ANY (0x000b | OP_CATAGORY_AI | OP_NONCAMPAIGN_FLAG)
#define OP_AI_EVADE_SHIP (0x000d | OP_CATAGORY_AI | OP_NONCAMPAIGN_FLAG)
#define OP_AI_STAY_NEAR_SHIP (0x000e | OP_CATAGORY_AI | OP_NONCAMPAIGN_FLAG)
#define OP_AI_KEEP_SAFE_DISTANCE (0x000f | OP_CATAGORY_AI | OP_NONCAMPAIGN_FLAG)
#define OP_AI_IGNORE (0x0010 | OP_CATAGORY_AI | OP_NONCAMPAIGN_FLAG)
#define OP_AI_STAY_STILL (0x0011 | OP_CATAGORY_AI | OP_NONCAMPAIGN_FLAG)
#define OP_AI_PLAY_DEAD (0x0012 | OP_CATAGORY_AI | OP_NONCAMPAIGN_FLAG)

#define OP_GOALS_ID (0x0001 | OP_CATAGORY_UNLISTED)
// used in campaign files for branching
#define OP_NEXT_MISSION (0x0002 | OP_CATAGORY_UNLISTED)
#define OP_IS_DESTROYED (0x0003 | OP_CATAGORY_UNLISTED)
#define OP_IS_SUBSYSTEM_DESTROYED (0x0004 | OP_CATAGORY_UNLISTED)
#define OP_IS_DISABLED (0x0005 | OP_CATAGORY_UNLISTED)
#define OP_IS_DISARMED (0x0006 | OP_CATAGORY_UNLISTED)
#define OP_HAS_DOCKED (0x0007 | OP_CATAGORY_UNLISTED)
#define OP_HAS_UNDOCKED (0x0008 | OP_CATAGORY_UNLISTED)
#define OP_HAS_ARRIVED (0x0009 | OP_CATAGORY_UNLISTED)
#define OP_HAS_DEPARTED (0x000a | OP_CATAGORY_UNLISTED)
#define OP_WAYPOINTS_DONE (0x000b | OP_CATAGORY_UNLISTED)
#define OP_ADD_SHIP_GOAL (0x000c | OP_CATAGORY_UNLISTED)
#define OP_CLEAR_SHIP_GOALS (0x000d | OP_CATAGORY_UNLISTED)
#define OP_ADD_WING_GOAL (0x000e | OP_CATAGORY_UNLISTED)
#define OP_CLEAR_WING_GOALS (0x000f | OP_CATAGORY_UNLISTED)
#define OP_AI_CHASE_WING (0x0010 | OP_CATAGORY_UNLISTED)
#define OP_AI_GUARD_WING (0x0011 | OP_CATAGORY_UNLISTED)
#define OP_EVENT_TRUE (0x0012 | OP_CATAGORY_UNLISTED)
#define OP_EVENT_FALSE (0x0013 | OP_CATAGORY_UNLISTED)
#define OP_PREVIOUS_GOAL_INCOMPLETE (0x0014 | OP_CATAGORY_UNLISTED)
#define OP_PREVIOUS_EVENT_INCOMPLETE (0x0015 | OP_CATAGORY_UNLISTED)
#define OP_AI_WARP (0x0016 | OP_CATAGORY_UNLISTED)
#define OP_END_MISSION_DELAY (0x0017 | OP_CATAGORY_UNLISTED)
#define OP_IS_CARGO_KNOWN (0x001c | OP_CATAGORY_UNLISTED)
#define OP_COND (0x001e | OP_CATAGORY_UNLISTED)
#define OP_END_OF_CAMPAIGN (0x001f | OP_CATAGORY_UNLISTED)

#define OP_KEY_PRESSED (0x0000 | OP_CATAGORY_TRAINING)
#define OP_KEY_RESET (0x0001 | OP_CATAGORY_TRAINING)
#define OP_TARGETED (0x0002 | OP_CATAGORY_TRAINING)
#define OP_SPEED (0x0003 | OP_CATAGORY_TRAINING)
#define OP_FACING (0x0004 | OP_CATAGORY_TRAINING)
#define OP_ORDER (0x0005 | OP_CATAGORY_TRAINING)
#define OP_WAYPOINT_MISSED (0x0006 | OP_CATAGORY_TRAINING)
#define OP_PATH_FLOWN (0x0007 | OP_CATAGORY_TRAINING)
#define OP_WAYPOINT_TWICE (0x0008 | OP_CATAGORY_TRAINING)
#define OP_TRAINING_MSG (0x0009 | OP_CATAGORY_TRAINING)
#define OP_FLASH_HUD_GAUGE (0x000a | OP_CATAGORY_TRAINING)
#define OP_SPECIAL_CHECK (0x000b | OP_CATAGORY_TRAINING)
#define OP_SECONDARIES_DEPLETED (0x000c | OP_CATAGORY_TRAINING)
#define OP_FACING2 (0x000d | OP_CATAGORY_TRAINING)

#define OP_SET_TRAINING_CONTEXT_FLY_PATH (0x0080 | OP_CATAGORY_TRAINING)
#define OP_SET_TRAINING_CONTEXT_SPEED (0x0081 | OP_CATAGORY_TRAINING)

// defines for string constants
#define SEXP_HULL_STRING "Hull"

// macros for accessing sexpression atoms
#define CAR(n) (Sexp_nodes[n].first)
#define CDR(n) (Sexp_nodes[n].rest)
#define CADR(n) (Sexp_nodes[Sexp_nodes[n].rest].first)
// #define CTEXT(n)  (Sexp_nodes[n].text)
char *CTEXT(int n);

#define REF_TYPE_SHIP 1
#define REF_TYPE_WING 2
#define REF_TYPE_PLAYER 3
#define REF_TYPE_WAYPOINT 4
// waypoint path
#define REF_TYPE_PATH 5

#define SRC_SHIP_ARRIVAL 0x10000
#define SRC_SHIP_DEPARTURE 0x20000
#define SRC_WING_ARRIVAL 0x30000
#define SRC_WING_DEPARTURE 0x40000
#define SRC_EVENT 0x50000
#define SRC_MISSION_GOAL 0x60000
#define SRC_SHIP_ORDER 0x70000
#define SRC_WING_ORDER 0x80000
#define SRC_DEBRIEFING 0x90000
#define SRC_BRIEFING 0xa0000
#define SRC_UNKNOWN 0xffff0000
#define SRC_MASK 0xffff0000
#define SRC_DATA_MASK 0xffff

#define SEXP_MODE_GENERAL 0
#define SEXP_MODE_CAMPAIGN 1

// defines for type field of sexp nodes.  The actual type of the node will be stored in the lower
// two bytes of the field.  The upper two bytes will be used for flags (bleah...)
// Be sure not to conflict with type field of sexp_variable
#define SEXP_NOT_USED 0
#define SEXP_LIST 1
#define SEXP_ATOM 2

// flags for sexpressions -- masked onto the end of the type field
// should this sexp node be persistant across missions
#define SEXP_FLAG_PERSISTENT (1 << 31)
#define SEXP_FLAG_VARIABLE (1 << 30)

// sexp variable definitions
#define SEXP_VARIABLE_CHAR ('@')
// defines for type field of sexp_variable.  Be sure not to conflict with type field of sexp_node
#define SEXP_VARIABLE_NUMBER (0x0010)
#define SEXP_VARIABLE_STRING (0x0020)
#define SEXP_VARIABLE_UNKNOWN (0x0040)
#define SEXP_VARIABLE_NOT_USED (0x0080)

#define SEXP_VARIABLE_BLOCK (0X0001)
#define SEXP_VARIABLE_BLOCK_EXP (0X0002)

#define BLOCK_EXP_SIZE 6
#define INNER_RAD 0
#define OUTER_RAD 1
#define DAMAGE 2
#define BLAST 3
#define PROPAGATE 4
#define SHOCK_SPEED 5

#define SEXP_VARIABLE_SET (0x0100)
#define SEXP_VARIABLE_MODIFIED (0x0200)

#define SEXP_TYPE_MASK(t) (t & 0x00ff)
#define SEXP_NODE_TYPE(n) (Sexp_nodes[n].type & 0x00ff)

// defines for subtypes of atoms
#define SEXP_ATOM_LIST 0
#define SEXP_ATOM_OPERATOR 1
#define SEXP_ATOM_NUMBER 2
#define SEXP_ATOM_STRING 3

// defines to short circuit evaluation when possible. Also used then goals can't
// be satisfied yet because ship (or wing) hasn't been created yet.

#define SEXP_TRUE 1
#define SEXP_FALSE 0
#define SEXP_KNOWN_FALSE -1
#define SEXP_KNOWN_TRUE -2
#define SEXP_UNKNOWN -3
// not a number -- used when ships/wing part of boolean and haven't arrived yet
#define SEXP_NAN -4
// not a number and will never change -- used to falsify boolean sexpressions
#define SEXP_NAN_FOREVER -5
// can't evaluate yet for whatever reason (acts like false)
#define SEXP_CANT_EVAL -6
// already completed an arithmetic operation and result is stored
#define SEXP_NUM_EVAL -7

// defines for check_sexp_syntax
// non-operator has arguments
#define SEXP_CHECK_NONOP_ARGS -1
// operator expected, but found data instead
#define SEXP_CHECK_OP_EXPTECTED -2
// unrecognized operator
#define SEXP_CHECK_UNKNOWN_OP -3
// return type or data type mismatch
#define SEXP_CHECK_TYPE_MISMATCH -4
// argument count in incorrect
#define SEXP_CHECK_BAD_ARG_COUNT -5
// unrecognized return type of data type
#define SEXP_CHECK_UNKNOWN_TYPE -6

// number is not valid
#define SEXP_CHECK_INVALID_NUM -101
// invalid ship name
#define SEXP_CHECK_INVALID_SHIP -102
// invalid wing name
#define SEXP_CHECK_INVALID_WING -103
// invalid subsystem
#define SEXP_CHECK_INVALID_SUBSYS -104
// invalid iff string
#define SEXP_CHECK_INVALID_IFF -105
// invalid point
#define SEXP_CHECK_INVALID_POINT -106
// negative number wasn't allowed
#define SEXP_CHECK_NEGATIVE_NUM -107
// invalid ship/wing
#define SEXP_CHECK_INVALID_SHIP_WING -108
// invalid ship type
#define SEXP_CHECK_INVALID_SHIP_TYPE -109
// invalid message
#define SEXP_CHECK_UNKNOWN_MESSAGE -110
// invalid priority for a message
#define SEXP_CHECK_INVALID_PRIORITY -111
// invalid mission name
#define SEXP_CHECK_INVALID_MISSION_NAME -112
// invalid goal name
#define SEXP_CHECK_INVALID_GOAL_NAME -113
// mission level too high in campaign
#define SEXP_CHECK_INVALID_LEVEL -114
// invalid 'who-from' for a message being sent
#define SEXP_CHECK_INVALID_MSG_SOURCE -115
#define SEXP_CHECK_INVALID_DOCKER_POINT -116
#define SEXP_CHECK_INVALID_DOCKEE_POINT -117
// ship goal (order) isn't allowed for given ship
#define SEXP_CHECK_ORDER_NOT_ALLOWED -118
#define SEXP_CHECK_DOCKING_NOT_ALLOWED -119
#define SEXP_CHECK_NUM_RANGE_INVALID -120
#define SEXP_CHECK_INVALID_EVENT_NAME -121
#define SEXP_CHECK_INVALID_SKILL_LEVEL -122
#define SEXP_CHECK_INVALID_MEDAL_NAME -123
#define SEXP_CHECK_INVALID_WEAPON_NAME -124
#define SEXP_CHECK_INVALID_SHIP_CLASS_NAME -125
#define SEXP_CHECK_INVALID_GAUGE_NAME -126
#define SEXP_CHECK_INVALID_JUMP_NODE -127
#define SEXP_CHECK_INVALID_VARIABLE -128

#define TRAINING_CONTEXT_SPEED (1 << 0)
#define TRAINING_CONTEXT_FLY_PATH (1 << 1)

// numbers used in special_training_check() function
#define SPECIAL_CHECK_TRAINING_FAILURE 2000

typedef struct sexp_ai_goal_link
{
    int ai_goal;
    int op_code;
} sexp_ai_goal_link;

typedef struct sexp_oper
{
    const char *text;
    int value;
    int min, max;
} sexp_oper;

typedef struct sexp_node
{
    char text[TOKEN_LENGTH];
    int type; // atom, list, or not used
    int subtype; // type of atom or list?
    int first; // if first parameter is sexp, index into Sexp_nodes
    int rest; // index into Sexp_nodes of rest of parameters
    int value; // known to be true, known to be false, or not known
} sexp_node;

typedef struct sexp_variable
{
    int type;
    char text[TOKEN_LENGTH];
    char variable_name[TOKEN_LENGTH];
} sexp_variable;

// next define used to eventually mark a directive as satisfied even though there may be more
// waves for a wing.  bascially a hack for the directives display.
#define DIRECTIVE_WING_ZERO -999

extern sexp_oper Operators[];
extern sexp_node Sexp_nodes[MAX_SEXP_NODES];
extern sexp_variable Sexp_variables[MAX_SEXP_VARIABLES];
extern int Num_operators;
extern int Locked_sexp_true, Locked_sexp_false;
extern int Directive_count;
extern int
    Sexp_useful_number; // a variable to pass useful info in from external modules
extern char *Sexp_string;
extern int Training_context;
extern int Training_context_speed_min;
extern int Training_context_speed_max;
extern int Training_context_speed_set;
extern int Training_context_speed_timestamp;
extern int Training_context_path;
extern int Training_context_goal_waypoint;
extern int Training_context_at_waypoint;
extern float Training_context_distance;
extern int Players_target;
extern ship_subsys *Players_targeted_subsys;
extern int Players_target_timestamp;
extern int Sexp_clipboard; // used by Fred

extern void init_sexp();
extern int alloc_sexp(const char *text, int type, int subtype, int first,
                      int rest);
extern int find_free_sexp();
extern int free_one_sexp(int num);
extern int free_sexp(int num);
extern int free_sexp2(int num);
extern int dup_sexp_chain(int node);
extern int cmp_sexp_chains(int node1, int node2);
extern int find_sexp_list(int num);
extern int find_parent_operator(int num);
extern int is_sexp_top_level(int node);
extern int identify_operator(char *token);
extern int find_operator(const char *token);
extern int query_sexp_args_count(int index);
extern int check_sexp_syntax(int index, int return_type = OPR_BOOL,
                             int recursive = 0, int *bindex = NULL, int mode = 0);
extern int get_sexp_main(void); //  Returns start node
extern int stuff_sexp_variable_list();
extern int eval_sexp(int index);
extern int query_operator_return_type(int op);
extern int query_operator_argument_type(int op, int argnum);
extern void update_sexp_references(char *old_name, char *new_name);
extern void update_sexp_references(char *old_name, char *new_name, int format);
extern int verify_vector(char *text);
extern void skip_white(char **str);
extern int validate_float(char **str);
extern int build_sexp_string(int cur_node, int level, int mode);
extern const char *sexp_error_message(int num);
extern int count_free_sexp_nodes();

// functions to change the attributes of an sexpression tree to persistent or not persistent
extern void sexp_unmark_persistent(int n);
extern void sexp_mark_persistent(int n);
extern int waypoint_lookup(char *name);
extern int verify_sexp_tree(int node);
extern int query_sexp_ai_goal_valid(int sexp_ai_goal, int ship);
int query_node_in_sexp(int node, int sexp);
void flush_sexp_tree(int node);

// sexp_variable
void sexp_modify_variable(int);
void sexp_modify_variable(char *text, int index);
int get_index_sexp_variable_name(const char *temp_name);
int sexp_variable_count();
void sexp_variable_sort();
int sexp_add_variable(const char *text, const char *var_name, int type,
                      int index = -1);
void sexp_variable_condense_block();

#endif
