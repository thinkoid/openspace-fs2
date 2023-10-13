// -*- mode: c++; -*-

#include <defs.hh>
#include <assert/assert.hh>
#include <log/log.hh>

// Parse a symbolic expression.
// These are identical to Lisp functions.
// It uses a very baggy format, allocating 16 characters per token, regardless
// of how many are used.

#include <ai/aigoals.hh>
#include <asteroid/asteroid.hh>
#include <autopilot/autopilot.hh>
#include <camera/camera.hh>
#include <cmdline/cmdline.hh>
#include <debugconsole/console.hh>
#include <fireball/fireballs.hh> // for explosion stuff
#include <freespace2/freespace.hh>
#include <gamesequence/gamesequence.hh>
#include <gamesnd/eventmusic.hh> // for change-soundtrack
#include <gamesnd/gamesnd.hh>
#include <graphics/2d.hh>
#include <graphics/font.hh>
#include <graphics/light.hh>
#include <hud/hud.hh>
#include <hud/hudartillery.hh>
#include <hud/hudconfig.hh>
#include <hud/hudescort.hh>
#include <hud/hudets.hh>
#include <hud/hudmessage.hh>
#include <hud/hudparse.hh>
#include <hud/hudshield.hh>
#include <hud/hudsquadmsg.hh> // for the order sexp
#include <iff_defs/iff_defs.hh>
#include <io/keycontrol.hh>
#include <io/timer.hh>
#include <jumpnode/jumpnode.hh>
#include <localization/localize.hh>
#include <math/fix.hh>
#include <math/fvi.hh>
#include <math/prng.hh>
#include <menuui/techmenu.hh> // for intel stuff
#include <mission/missionbriefcommon.hh>
#include <mission/missioncampaign.hh>
#include <mission/missiongoals.hh>
#include <mission/missionlog.hh>
#include <mission/missionmessage.hh>
#include <mission/missionparse.hh> // for p_object definition
#include <mission/missiontraining.hh>
#include <missionui/redalert.hh>
#include <mod_table/mod_table.hh>
#include <nebula/neb.hh>
#include <nebula/neblightning.hh>
#include <object/objcollide.hh>
#include <object/objectdock.hh>
#include <object/objectshield.hh>
#include <object/objectsnd.hh>
#include <object/waypoint.hh>
#include <parse/parselo.hh>
#include <parse/sexp.hh>
#include <playerman/player.hh>
#include <render/3d.hh>
#include <shared/alphacolors.hh>
#include <shared/globals.hh>
#include <shared/version.hh>
#include <ship/afterburner.hh>
#include <ship/awacs.hh>
#include <ship/ship.hh>
#include <ship/ship_flags.hh>
#include <ship/shiphit.hh>
#include <sound/audiostr.hh>
#include <sound/ds.hh>
#include <sound/sound.hh>
#include <starfield/starfield.hh>
#include <starfield/supernova.hh>
#include <stats/medals.hh>
#include <util/list.hh>
#include <util/unicode.hh>
#include <weapon/beam.hh>
#include <weapon/emp.hh>
#include <weapon/shockwave.hh>
#include <weapon/weapon.hh>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cassert>
#include <climits>
#include <cstdint>

#ifndef NDEBUG
#include "hud/hudmessage.hh"
#endif

// Stupid windows workaround...
#ifdef MessageBox
#undef MessageBox
#endif

#define TRUE 1
#define FALSE 0

std::vector< sexp_oper > Operators = {
    // Operator, Identity, Min / Max arguments Arithmetic Category
    { "+", OP_PLUS, 2, INT_MAX, SEXP_ARITHMETIC_OPERATOR },
    { "-", OP_MINUS, 2, INT_MAX, SEXP_ARITHMETIC_OPERATOR },
    { "*", OP_MUL, 2, INT_MAX, SEXP_ARITHMETIC_OPERATOR },
    { "/", OP_DIV, 2, INT_MAX, SEXP_ARITHMETIC_OPERATOR },
    { "mod", OP_MOD, 2, INT_MAX, SEXP_ARITHMETIC_OPERATOR },
    { "rand", OP_RAND, 2, 3, SEXP_ARITHMETIC_OPERATOR },

    // Logical Category
    { "true", OP_TRUE, 0, 0, SEXP_BOOLEAN_OPERATOR },
    { "false", OP_FALSE, 0, 0, SEXP_BOOLEAN_OPERATOR },
    { "and", OP_AND, 2, INT_MAX, SEXP_BOOLEAN_OPERATOR },
    { "and-in-sequence", OP_AND_IN_SEQUENCE, 2, INT_MAX, SEXP_BOOLEAN_OPERATOR },
    { "or", OP_OR, 2, INT_MAX, SEXP_BOOLEAN_OPERATOR },
    { "not", OP_NOT, 1, 1, SEXP_BOOLEAN_OPERATOR },
    { "=", OP_EQUALS, 2, INT_MAX, SEXP_BOOLEAN_OPERATOR },
    { ">", OP_GREATER_THAN, 2, INT_MAX, SEXP_BOOLEAN_OPERATOR },
    { "<", OP_LESS_THAN, 2, INT_MAX, SEXP_BOOLEAN_OPERATOR },
    { "has-time-elapsed", OP_HAS_TIME_ELAPSED, 1, 1, SEXP_BOOLEAN_OPERATOR },

    // Event/Goals Category
    { "is-goal-true-delay", OP_GOAL_TRUE_DELAY, 2, 2, SEXP_BOOLEAN_OPERATOR },
    { "is-goal-false-delay", OP_GOAL_FALSE_DELAY, 2, 2, SEXP_BOOLEAN_OPERATOR },
    { "is-goal-incomplete", OP_GOAL_INCOMPLETE, 1, 1, SEXP_BOOLEAN_OPERATOR },
    { "is-event-true", OP_EVENT_TRUE, 1, 1, SEXP_BOOLEAN_OPERATOR },
    { "is-event-true-delay", OP_EVENT_TRUE_DELAY, 2, 3, SEXP_BOOLEAN_OPERATOR },
    { "is-event-false", OP_EVENT_FALSE, 1, 1, SEXP_BOOLEAN_OPERATOR },
    { "is-event-false-delay", OP_EVENT_FALSE_DELAY, 2, 3, SEXP_BOOLEAN_OPERATOR },
    { "is-event-incomplete", OP_EVENT_INCOMPLETE, 1, 1, SEXP_BOOLEAN_OPERATOR },
    { "is-previous-goal-true", OP_PREVIOUS_GOAL_TRUE, 2, 3, SEXP_BOOLEAN_OPERATOR },
    { "is-previous-goal-false", OP_PREVIOUS_GOAL_FALSE, 2, 3, SEXP_BOOLEAN_OPERATOR },
    { "is-previous-goal-incomplete", OP_PREVIOUS_GOAL_INCOMPLETE, 2, 3, SEXP_BOOLEAN_OPERATOR },
    { "is-previous-event-true", OP_PREVIOUS_EVENT_TRUE, 2, 3, SEXP_BOOLEAN_OPERATOR },
    { "is-previous-event-false", OP_PREVIOUS_EVENT_FALSE, 2, 3, SEXP_BOOLEAN_OPERATOR },
    { "is-previous-event-incomplete", OP_PREVIOUS_EVENT_INCOMPLETE, 2, 3, SEXP_BOOLEAN_OPERATOR },

    // Objectives Category
    { "is-destroyed", OP_IS_DESTROYED, 1, INT_MAX, SEXP_BOOLEAN_OPERATOR },
    { "is-destroyed-delay", OP_IS_DESTROYED_DELAY, 2, INT_MAX, SEXP_BOOLEAN_OPERATOR },
    { "is-subsystem-destroyed", OP_IS_SUBSYSTEM_DESTROYED, 2, 2, SEXP_BOOLEAN_OPERATOR },
    { "is-subsystem-destroyed-delay", OP_IS_SUBSYSTEM_DESTROYED_DELAY, 3, 3, SEXP_BOOLEAN_OPERATOR },
    { "is-disabled", OP_IS_DISABLED, 1, INT_MAX, SEXP_BOOLEAN_OPERATOR },
    { "is-disabled-delay", OP_IS_DISABLED_DELAY, 2, INT_MAX, SEXP_BOOLEAN_OPERATOR },
    { "is-disarmed", OP_IS_DISARMED, 1, INT_MAX, SEXP_BOOLEAN_OPERATOR },
    { "is-disarmed-delay", OP_IS_DISARMED_DELAY, 2, INT_MAX, SEXP_BOOLEAN_OPERATOR },
    { "has-docked", OP_HAS_DOCKED, 3, 3, SEXP_BOOLEAN_OPERATOR },
    { "has-docked-delay", OP_HAS_DOCKED_DELAY, 4, 4, SEXP_BOOLEAN_OPERATOR },
    { "has-undocked", OP_HAS_UNDOCKED, 3, 3, SEXP_BOOLEAN_OPERATOR },
    { "has-undocked-delay", OP_HAS_UNDOCKED_DELAY, 4, 4, SEXP_BOOLEAN_OPERATOR },
    { "has-arrived", OP_HAS_ARRIVED, 1, INT_MAX, SEXP_BOOLEAN_OPERATOR },
    { "has-arrived-delay", OP_HAS_ARRIVED_DELAY, 2, INT_MAX, SEXP_BOOLEAN_OPERATOR },
    { "has-departed", OP_HAS_DEPARTED, 1, INT_MAX, SEXP_BOOLEAN_OPERATOR },
    { "has-departed-delay", OP_HAS_DEPARTED_DELAY, 2, INT_MAX, SEXP_BOOLEAN_OPERATOR },
    { "are-waypoints-done", OP_WAYPOINTS_DONE, 2, 2, SEXP_BOOLEAN_OPERATOR },
    { "are-waypoints-done-delay", OP_WAYPOINTS_DONE_DELAY, 3, 4, SEXP_BOOLEAN_OPERATOR },
    { "ship-type-destroyed", OP_SHIP_TYPE_DESTROYED, 2, 2, SEXP_BOOLEAN_OPERATOR },
    { "percent-ships-destroyed", OP_PERCENT_SHIPS_DESTROYED, 2, INT_MAX, SEXP_BOOLEAN_OPERATOR },
    { "percent-ships-departed", OP_PERCENT_SHIPS_DEPARTED, 2, INT_MAX, SEXP_BOOLEAN_OPERATOR },
    { "depart-node-delay", OP_DEPART_NODE_DELAY, 3, INT_MAX, SEXP_BOOLEAN_OPERATOR },
    { "destroyed-or-departed-delay", OP_DESTROYED_DEPARTED_DELAY, 2, INT_MAX, SEXP_BOOLEAN_OPERATOR },

    // Status Category Mission Sub-Category Player Sub-Category
    { "was-promotion-granted", OP_WAS_PROMOTION_GRANTED, 0, 1, SEXP_BOOLEAN_OPERATOR },
    { "was-medal-granted", OP_WAS_MEDAL_GRANTED, 0, 1, SEXP_BOOLEAN_OPERATOR },
    { "skill-level-at-least", OP_SKILL_LEVEL_AT_LEAST, 1, 1, SEXP_BOOLEAN_OPERATOR },
    { "num_kills", OP_NUM_KILLS, 1, 1, SEXP_INTEGER_OPERATOR },
    { "num_type_kills", OP_NUM_TYPE_KILLS, 2, 2, SEXP_INTEGER_OPERATOR },
    { "num_class_kills", OP_NUM_CLASS_KILLS, 2, 2, SEXP_INTEGER_OPERATOR },
    { "time-elapsed-last-order", OP_LAST_ORDER_TIME, 2, 2, SEXP_INTEGER_OPERATOR },

    // Multiplayer Sub-Category
    { "num-players", OP_NUM_PLAYERS, 0, 0, SEXP_INTEGER_OPERATOR },
    { "team-score", OP_TEAM_SCORE, 1, 1, SEXP_INTEGER_OPERATOR },

    // Ship Status Sub-Category
    { "is-ship-visible", OP_IS_SHIP_VISIBLE, 1, 1, SEXP_BOOLEAN_OPERATOR },
    { "is-iff", OP_IS_IFF, 2, INT_MAX, SEXP_BOOLEAN_OPERATOR },
    { "is_tagged", OP_IS_TAGGED, 1, 1, SEXP_BOOLEAN_OPERATOR },
    { "has-been-tagged-delay", OP_HAS_BEEN_TAGGED_DELAY, 2, INT_MAX, SEXP_BOOLEAN_OPERATOR },

    // Shields, Engines and Weapons Sub-Category
    { "is-primary-selected", OP_IS_PRIMARY_SELECTED, 2, 2, SEXP_BOOLEAN_OPERATOR },
    { "is-secondary-selected", OP_IS_SECONDARY_SELECTED, 2, 2, SEXP_BOOLEAN_OPERATOR },
    { "secondary-ammo-pct", OP_SECONDARY_AMMO_PCT, 2, 2, SEXP_INTEGER_OPERATOR },
    { "shield-recharge-pct", OP_SHIELD_RECHARGE_PCT, 1, 1, SEXP_INTEGER_OPERATOR },
    { "weapon-recharge-pct", OP_WEAPON_RECHARGE_PCT, 1, 1, SEXP_INTEGER_OPERATOR },
    { "engine-recharge-pct", OP_ENGINE_RECHARGE_PCT, 1, 1, SEXP_INTEGER_OPERATOR },
    { "shield-quad-low", OP_SHIELD_QUAD_LOW, 2, 2, SEXP_INTEGER_OPERATOR },

    // Cargo Sub-Category
    { "is-cargo-known", OP_IS_CARGO_KNOWN, 1, INT_MAX, SEXP_BOOLEAN_OPERATOR },
    { "is-cargo-known-delay", OP_CARGO_KNOWN_DELAY, 2, INT_MAX, SEXP_BOOLEAN_OPERATOR },
    { "cap-subsys-cargo-known-delay", OP_CAP_SUBSYS_CARGO_KNOWN_DELAY, 3, INT_MAX, SEXP_BOOLEAN_OPERATOR },

    // Damage Sub-Category
    { "shields-left", OP_SHIELDS_LEFT, 1, 1, SEXP_INTEGER_OPERATOR },
    { "hits-left", OP_HITS_LEFT, 1, 1, SEXP_INTEGER_OPERATOR },
    { "hits-left-subsystem", OP_HITS_LEFT_SUBSYSTEM, 2, 3, SEXP_INTEGER_OPERATOR },

    // Distance and Coordinates Sub-Category
    { "distance", OP_DISTANCE, 2, 2, SEXP_INTEGER_OPERATOR },
    { "special-warp-dist", OP_SPECIAL_WARP_DISTANCE, 1, 1, SEXP_INTEGER_OPERATOR },

    // Time Category
    { "time-ship-destroyed", OP_TIME_SHIP_DESTROYED, 1, 1, SEXP_INTEGER_OPERATOR },
    { "time-ship-arrived", OP_TIME_SHIP_ARRIVED, 1, 1, SEXP_INTEGER_OPERATOR },
    { "time-ship-departed", OP_TIME_SHIP_DEPARTED, 1, 1, SEXP_INTEGER_OPERATOR },
    { "time-wing-destroyed", OP_TIME_WING_DESTROYED, 1, 1, SEXP_INTEGER_OPERATOR },
    { "time-wing-arrived", OP_TIME_WING_ARRIVED, 1, 1, SEXP_INTEGER_OPERATOR },
    { "time-wing-departed", OP_TIME_WING_DEPARTED, 1, 1, SEXP_INTEGER_OPERATOR },
    { "mission-time", OP_MISSION_TIME, 0, 0, SEXP_INTEGER_OPERATOR },
    { "time-docked", OP_TIME_DOCKED, 3, 3, SEXP_INTEGER_OPERATOR },
    { "time-undocked", OP_TIME_UNDOCKED, 3, 3, SEXP_INTEGER_OPERATOR },

    // Conditionals Category
    { "cond", OP_COND, 1, INT_MAX, SEXP_CONDITIONAL_OPERATOR },
    { "when", OP_WHEN, 2, INT_MAX, SEXP_CONDITIONAL_OPERATOR },

    // Change Category @Messaging@ Sub-Category
    { "send-message", OP_SEND_MESSAGE, 3, 3, SEXP_ACTION_OPERATOR },
    { "send-message-list", OP_SEND_MESSAGE_LIST, 4, INT_MAX, SEXP_ACTION_OPERATOR },
    { "send-random-message", OP_SEND_RANDOM_MESSAGE, 3, INT_MAX, SEXP_ACTION_OPERATOR },

    // AI Control Sub-Category
    { "add-goal", OP_ADD_GOAL, 2, 2, SEXP_ACTION_OPERATOR },
    { "add-ship-goal", OP_ADD_SHIP_GOAL, 2, 2, SEXP_ACTION_OPERATOR },
    { "add-wing-goal", OP_ADD_WING_GOAL, 2, 2, SEXP_ACTION_OPERATOR },
    { "clear-goals", OP_CLEAR_GOALS, 1, INT_MAX, SEXP_ACTION_OPERATOR },
    { "clear-ship-goals", OP_CLEAR_SHIP_GOALS, 1, 1, SEXP_ACTION_OPERATOR },
    { "clear-wing-goals", OP_CLEAR_WING_GOALS, 1, 1, SEXP_ACTION_OPERATOR },
    { "good-rearm-time", OP_GOOD_REARM_TIME, 2, 2, SEXP_ACTION_OPERATOR },
    { "good-secondary-time", OP_GOOD_SECONDARY_TIME, 4, 4, SEXP_ACTION_OPERATOR },
    { "cap-waypoint-speed", OP_CAP_WAYPOINT_SPEED, 2, 2, SEXP_ACTION_OPERATOR },

    // Ship Status Sub-Category
    { "protect-ship", OP_PROTECT_SHIP, 1, INT_MAX, SEXP_ACTION_OPERATOR },
    { "unprotect-ship", OP_UNPROTECT_SHIP, 1, INT_MAX, SEXP_ACTION_OPERATOR },
    { "beam-protect-ship", OP_BEAM_PROTECT_SHIP, 1, INT_MAX, SEXP_ACTION_OPERATOR },
    { "beam-unprotect-ship", OP_BEAM_UNPROTECT_SHIP, 1, INT_MAX, SEXP_ACTION_OPERATOR },
    { "ship-invisible", OP_SHIP_INVISIBLE, 1, INT_MAX, SEXP_ACTION_OPERATOR },
    { "ship-visible", OP_SHIP_VISIBLE, 1, INT_MAX, SEXP_ACTION_OPERATOR },
    { "change-iff", OP_CHANGE_IFF, 2, INT_MAX, SEXP_ACTION_OPERATOR },
    { "add-remove-escort", OP_ADD_REMOVE_ESCORT, 2, 2, SEXP_ACTION_OPERATOR },

    // Shields, Engines and Weapons Sub-Category
    { "break-warp", OP_WARP_BROKEN, 1, INT_MAX, SEXP_ACTION_OPERATOR },
    { "fix-warp", OP_WARP_NOT_BROKEN, 1, INT_MAX, SEXP_ACTION_OPERATOR },
    { "never-warp", OP_WARP_NEVER, 1, INT_MAX, SEXP_ACTION_OPERATOR },
    { "allow-warp", OP_WARP_ALLOWED, 1, INT_MAX, SEXP_ACTION_OPERATOR },
    { "special-warpout-name", OP_SET_SPECIAL_WARPOUT_NAME, 2, 2, SEXP_ACTION_OPERATOR },

    // Subsystems and Health Sub-Category
    { "ship-invulnerable", OP_SHIP_INVULNERABLE, 1, INT_MAX, SEXP_ACTION_OPERATOR },
    { "ship-vulnerable", OP_SHIP_VULNERABLE, 1, INT_MAX, SEXP_ACTION_OPERATOR },
    { "ship-guardian", OP_SHIP_GUARDIAN, 1, INT_MAX, SEXP_ACTION_OPERATOR },
    { "ship-no-guardian", OP_SHIP_NO_GUARDIAN, 1, INT_MAX, SEXP_ACTION_OPERATOR },
    { "self-destruct", OP_SELF_DESTRUCT, 1, INT_MAX, SEXP_ACTION_OPERATOR },
    { "sabotage-subsystem", OP_SABOTAGE_SUBSYSTEM, 3, 3, SEXP_ACTION_OPERATOR },
    { "repair-subsystem", OP_REPAIR_SUBSYSTEM, 3, 4, SEXP_ACTION_OPERATOR },
    { "set-subsystem-strength", OP_SET_SUBSYSTEM_STRNGTH, 3, 4, SEXP_ACTION_OPERATOR },
    { "subsys-set-random", OP_SUBSYS_SET_RANDOM, 3, INT_MAX, SEXP_ACTION_OPERATOR },
    { "awacs-set-radius", OP_AWACS_SET_RADIUS, 3, 3, SEXP_ACTION_OPERATOR },

    // Cargo Sub-Category
    { "transfer-cargo", OP_TRANSFER_CARGO, 2, 2, SEXP_ACTION_OPERATOR },
    { "exchange-cargo", OP_EXCHANGE_CARGO, 2, 2, SEXP_ACTION_OPERATOR },
    { "cargo-no-deplete", OP_CARGO_NO_DEPLETE, 1, 2, SEXP_ACTION_OPERATOR },

    // Beams and Turrets Sub-Category
    { "fire-beam", OP_BEAM_FIRE, 3, 5, SEXP_ACTION_OPERATOR },
    { "beam-free", OP_BEAM_FREE, 2, INT_MAX, SEXP_ACTION_OPERATOR },
    { "beam-free-all", OP_BEAM_FREE_ALL, 1, INT_MAX, SEXP_ACTION_OPERATOR },
    { "beam-lock", OP_BEAM_LOCK, 2, INT_MAX, SEXP_ACTION_OPERATOR },
    { "beam-lock-all", OP_BEAM_LOCK_ALL, 1, INT_MAX, SEXP_ACTION_OPERATOR },
    { "turret-free", OP_TURRET_FREE, 2, INT_MAX, SEXP_ACTION_OPERATOR },
    { "turret-free-all", OP_TURRET_FREE_ALL, 1, INT_MAX, SEXP_ACTION_OPERATOR },
    { "turret-lock", OP_TURRET_LOCK, 2, INT_MAX, SEXP_ACTION_OPERATOR },
    { "turret-lock-all", OP_TURRET_LOCK_ALL, 1, INT_MAX, SEXP_ACTION_OPERATOR },
    { "turret-tagged-only", OP_TURRET_TAGGED_ONLY_ALL, 1, 1, SEXP_ACTION_OPERATOR },
    { "turret-tagged-clear", OP_TURRET_TAGGED_CLEAR_ALL, 1, 1, SEXP_ACTION_OPERATOR },

    // Mission and Campaign Sub-Category
    { "invalidate-goal", OP_INVALIDATE_GOAL, 1, INT_MAX, SEXP_ACTION_OPERATOR },
    { "validate-goal", OP_VALIDATE_GOAL, 1, INT_MAX, SEXP_ACTION_OPERATOR },
    { "red-alert", OP_RED_ALERT, 0, 0, SEXP_ACTION_OPERATOR },
    { "next-mission", OP_NEXT_MISSION, 1, 1, SEXP_ACTION_OPERATOR },
    { "end-campaign", OP_END_CAMPAIGN, 0, 1, SEXP_ACTION_OPERATOR },
    { "end-of-campaign", OP_END_OF_CAMPAIGN, 0, 0, SEXP_ACTION_OPERATOR },
    { "grant-promotion", OP_GRANT_PROMOTION, 0, 0, SEXP_ACTION_OPERATOR },
    { "grant-medal", OP_GRANT_MEDAL, 1, 1, SEXP_ACTION_OPERATOR },
    { "allow-ship", OP_ALLOW_SHIP, 1, 1, SEXP_ACTION_OPERATOR },
    { "allow-weapon", OP_ALLOW_WEAPON, 1, 1, SEXP_ACTION_OPERATOR },
    { "tech-add-ships", OP_TECH_ADD_SHIP, 1, INT_MAX, SEXP_ACTION_OPERATOR },
    { "tech-add-weapons", OP_TECH_ADD_WEAPON, 1, INT_MAX, SEXP_ACTION_OPERATOR },

    // Cutscene Sub-Category
    { "supernova-start", OP_SUPERNOVA_START, 1, 1, SEXP_ACTION_OPERATOR },

    // Special Effects Sub-Category
    { "ship-vanish", OP_SHIP_VANISH, 1, INT_MAX, SEXP_ACTION_OPERATOR },

    // Variable Category
    { "modify-variable", OP_MODIFY_VARIABLE, 2, 2, SEXP_ACTION_OPERATOR },

    // Other Sub-Category
    { "do-nothing", OP_NOP, 0, 0, SEXP_ACTION_OPERATOR },

    // AI Goals Category
    { "ai-chase", OP_AI_CHASE, 2, 3, SEXP_GOAL_OPERATOR },
    { "ai-chase-wing", OP_AI_CHASE_WING, 2, 3, SEXP_GOAL_OPERATOR },
    { "ai-chase-any", OP_AI_CHASE_ANY, 1, 1, SEXP_GOAL_OPERATOR },
    { "ai-guard", OP_AI_GUARD, 2, 2, SEXP_GOAL_OPERATOR },
    { "ai-guard-wing", OP_AI_GUARD_WING, 2, 2, SEXP_GOAL_OPERATOR },
    { "ai-destroy-subsystem", OP_AI_DESTROY_SUBSYS, 3, 4, SEXP_GOAL_OPERATOR },
    { "ai-disable-ship", OP_AI_DISABLE_SHIP, 2, 3, SEXP_GOAL_OPERATOR },
    { "ai-disarm-ship", OP_AI_DISARM_SHIP, 2, 3, SEXP_GOAL_OPERATOR },
    { "ai-warp", OP_AI_WARP, 2, 2, SEXP_GOAL_OPERATOR },
    { "ai-warp-out", OP_AI_WARP_OUT, 1, 1, SEXP_GOAL_OPERATOR },
    { "ai-dock", OP_AI_DOCK, 4, 4, SEXP_GOAL_OPERATOR },
    { "ai-undock", OP_AI_UNDOCK, 1, 2, SEXP_GOAL_OPERATOR },
    { "ai-waypoints", OP_AI_WAYPOINTS, 2, 2, SEXP_GOAL_OPERATOR },
    { "ai-waypoints-once", OP_AI_WAYPOINTS_ONCE, 2, 2, SEXP_GOAL_OPERATOR },
    { "ai-ignore", OP_AI_IGNORE, 2, 2, SEXP_GOAL_OPERATOR },
    { "ai-stay-near-ship", OP_AI_STAY_NEAR_SHIP, 2, 2, SEXP_GOAL_OPERATOR },
    { "ai-evade-ship", OP_AI_EVADE_SHIP, 2, 2, SEXP_GOAL_OPERATOR },
    { "ai-keep-safe-distance", OP_AI_KEEP_SAFE_DISTANCE, 1, 1, SEXP_GOAL_OPERATOR },
    { "ai-stay-still", OP_AI_STAY_STILL, 2, 2, SEXP_GOAL_OPERATOR },
    { "ai-play-dead", OP_AI_PLAY_DEAD, 1, 1, SEXP_GOAL_OPERATOR },
    { "goals", OP_GOALS_ID, 1, INT_MAX, SEXP_ACTION_OPERATOR },

    // Training Category
    { "key-pressed", OP_KEY_PRESSED, 1, 2, SEXP_BOOLEAN_OPERATOR },
    { "key-reset", OP_KEY_RESET, 1, INT_MAX, SEXP_ACTION_OPERATOR },
    { "targeted", OP_TARGETED, 1, 3, SEXP_BOOLEAN_OPERATOR },
    { "speed", OP_SPEED, 1, 1, SEXP_BOOLEAN_OPERATOR },
    { "facing", OP_FACING, 2, 2, SEXP_BOOLEAN_OPERATOR },
    { "facing-waypoint", OP_FACING2, 2, 2, SEXP_BOOLEAN_OPERATOR },
    { "order", OP_ORDER, 2, 3, SEXP_BOOLEAN_OPERATOR },
    { "waypoint-missed", OP_WAYPOINT_MISSED, 0, 0, SEXP_BOOLEAN_OPERATOR },
    { "waypoint-twice", OP_WAYPOINT_TWICE, 0, 0, SEXP_BOOLEAN_OPERATOR },
    { "path-flown", OP_PATH_FLOWN, 0, 0, SEXP_BOOLEAN_OPERATOR },
    { "training-msg", OP_TRAINING_MSG, 1, 4, SEXP_ACTION_OPERATOR },
    { "flash-hud-gauge", OP_FLASH_HUD_GAUGE, 1, 1, SEXP_ACTION_OPERATOR },
    { "secondaries-depleted", OP_SECONDARIES_DEPLETED, 1, 1, SEXP_BOOLEAN_OPERATOR },
    { "special-check", OP_SPECIAL_CHECK, 1, 1, SEXP_ACTION_OPERATOR },
    { "set-training-context-fly-path", OP_SET_TRAINING_CONTEXT_FLY_PATH, 2, 2, SEXP_ACTION_OPERATOR },
    { "set-training-context-speed", OP_SET_TRAINING_CONTEXT_SPEED, 2, 2, SEXP_ACTION_OPERATOR }
};

sexp_ai_goal_link Sexp_ai_goal_links[] = {
    { AI_GOAL_CHASE, OP_AI_CHASE },
    { AI_GOAL_CHASE_WING, OP_AI_CHASE_WING },
    { AI_GOAL_CHASE_ANY, OP_AI_CHASE_ANY },
    { AI_GOAL_DOCK, OP_AI_DOCK },
    { AI_GOAL_UNDOCK, OP_AI_UNDOCK },
    { AI_GOAL_WARP, OP_AI_WARP_OUT },
    { AI_GOAL_WARP, OP_AI_WARP },
    { AI_GOAL_WAYPOINTS, OP_AI_WAYPOINTS },
    { AI_GOAL_WAYPOINTS_ONCE, OP_AI_WAYPOINTS_ONCE },
    { AI_GOAL_DESTROY_SUBSYSTEM, OP_AI_DESTROY_SUBSYS },
    { AI_GOAL_DISABLE_SHIP, OP_AI_DISABLE_SHIP },
    { AI_GOAL_DISARM_SHIP, OP_AI_DISARM_SHIP },
    { AI_GOAL_GUARD, OP_AI_GUARD },
    { AI_GOAL_GUARD_WING, OP_AI_GUARD_WING },
    { AI_GOAL_EVADE_SHIP, OP_AI_EVADE_SHIP },
    { AI_GOAL_STAY_NEAR_SHIP, OP_AI_STAY_NEAR_SHIP },
    { AI_GOAL_KEEP_SAFE_DISTANCE, OP_AI_KEEP_SAFE_DISTANCE },
    { AI_GOAL_IGNORE, OP_AI_IGNORE },
    { AI_GOAL_STAY_STILL, OP_AI_STAY_STILL },
    { AI_GOAL_PLAY_DEAD, OP_AI_PLAY_DEAD }
};

const char* HUD_gauge_text[NUM_HUD_GAUGES] = { "LEAD_INDICATOR",
                                               "ORIENTATION_TEE",
                                               "HOSTILE_TRIANGLE",
                                               "TARGET_TRIANGLE",
                                               "MISSION_TIME",
                                               "RETICLE_CIRCLE",
                                               "THROTTLE_GAUGE",
                                               "RADAR",
                                               "TARGET_MONITOR",
                                               "CENTER_RETICLE",
                                               "TARGET_MONITOR_EXTRA_DATA",
                                               "TARGET_SHIELD_ICON",
                                               "PLAYER_SHIELD_ICON",
                                               "ETS_GAUGE",
                                               "AUTO_TARGET",
                                               "AUTO_SPEED",
                                               "WEAPONS_GAUGE",
                                               "ESCORT_VIEW",
                                               "DIRECTIVES_VIEW",
                                               "THREAT_GAUGE",
                                               "AFTERBURNER_ENERGY",
                                               "WEAPONS_ENERGY",
                                               "WEAPON_LINKING_GAUGE",
                                               "TARGER_MINI_ICON",
                                               "OFFSCREEN_INDICATOR",
                                               "TALKING_HEAD",
                                               "DAMAGE_GAUGE",
                                               "MESSAGE_LINES",
                                               "MISSILE_WARNING_ARROW",
                                               "CMEASURE_GAUGE",
                                               "OBJECTIVES_NOTIFY_GAUGE",
                                               "WINGMEN_STATUS",
                                               "OFFSCREEN RANGE",
                                               "KILLS GAUGE",
                                               "ATTACKING TARGET COUNT",
                                               "TEXT FLASH",
                                               "MESSAGE BOX",
                                               "SUPPORT GUAGE",
                                               "LAG GUAGE" };

int Directive_count;
int Sexp_useful_number; // a variable to pass useful info in from external
                        // modules
int Locked_sexp_true, Locked_sexp_false;
int Num_sexp_ai_goal_links = sizeof (Sexp_ai_goal_links) / sizeof (sexp_ai_goal_link);
int Sexp_clipboard = -1; // used by Fred
int Training_context = 0;
int Training_context_speed_set;
int Training_context_speed_min;
int Training_context_speed_max;
int Training_context_speed_timestamp;
waypoint_list* Training_context_path;
int Training_context_goal_waypoint;
int Training_context_at_waypoint;
float Training_context_distance;

#define SEXP_NODE_INCREMENT 250

int Num_sexp_nodes = 0;
sexp_node* Sexp_nodes = NULL;

sexp_variable Sexp_variables[MAX_SEXP_VARIABLES];
sexp_variable Block_variables[MAX_SEXP_VARIABLES]; // used for compatibility with retail.

int Num_special_expl_blocks;

std::vector< int > Current_sexp_operator;

int Players_target = UNINITIALIZED;
int Players_mlocked = UNINITIALIZED; // for is-missile-locked - Sesquipedalian

ship_subsys* Players_targeted_subsys;

int Players_target_timestamp;
int Players_mlocked_timestamp;

// for play-music - Goober5000
int Sexp_music_handle = -1;
void sexp_stop_music (bool fade = true);

int hud_gauge_type_lookup (char* name);

int get_sexp ();

void build_extended_sexp_string (std::string& accumulator, int cur_node, int level, int mode);
void update_sexp_references (const char* old_name, const char* new_name, int format, int node);
int sexp_determine_team (char* subj);
int extract_sexp_variable_index (int node);
void init_sexp_vars ();
int eval_num (int node);

// for handling variables
void add_block_variable (const char* text, const char* var_name, int type, int index);
void sexp_modify_variable (int node);
int sexp_get_variable_by_index (int node);
void sexp_set_variable_by_index (int node);
void sexp_copy_variable_from_index (int node);
void sexp_copy_variable_between_indexes (int node);

std::vector< char* > Sexp_replacement_arguments;
int Sexp_current_argument_nesting_level;

////////////////////////////////////////////////////////////////////////

// Goober5000 - adapted from sexp_list_item in Sexp_tree.h
struct arg_item {
    char* text;
    arg_item* next;
    int flags;
    int nesting_level;

    arg_item () : text (), next (), flags (0), nesting_level (0) { }

    void add_data (char* str);
    void expunge ();

    arg_item* get_next ();

    void clear_nesting_level ();
};

void arg_item::add_data (char* str) {
    arg_item *item, *ptr;

    // create item
    item = new arg_item;
    item->text = str;
    item->nesting_level = Sexp_current_argument_nesting_level;

    // prepend item to existing list
    ptr = this->next;
    this->next = item;
    item->next = ptr;
}

arg_item* arg_item::get_next () {
    if (this->next != NULL) {
        if (this->next->nesting_level >= Sexp_current_argument_nesting_level) { return this->next; }
    }

    return NULL;
}

void arg_item::expunge () {
    arg_item* ptr;

    // contiually delete first item of list
    while (this->next != NULL) {
        ptr = this->next->next;

        if (this->next->flags & ARG_ITEM_F_DUP) free (this->next->text);
        delete this->next;

        this->next = ptr;
    }
}

void arg_item::clear_nesting_level () {
    arg_item* ptr;

    // contiually delete first item of list
    while (this->next != NULL && this->next->nesting_level >= Sexp_current_argument_nesting_level) {
        ptr = this->next->next;

        if (this->next->flags & ARG_ITEM_F_DUP) free (this->next->text);
        delete this->next;

        this->next = ptr;
    }
}

////////////////////////////////////////////////////////////////////////

arg_item Sexp_applicable_argument_list;

// Karajorma
int get_generic_subsys (char* subsy_name);
bool ship_class_unchanged (int ship_index);

int get_effect_from_name (char* name);

#define NO_OPERATOR_INDEX_DEFINED -2
#define NOT_A_SEXP_OPERATOR -1

// Karajorma - some useful helper methods
player* get_player_from_ship_node (int node);
ship* sexp_get_ship_from_node (int node);

// hud-display-gauge magic values
#define SEXP_HUD_GAUGE_WARPOUT "warpout"

// event log stuff
std::vector< std::string >* Current_event_log_buffer;
std::vector< std::string >* Current_event_log_variable_buffer;
std::vector< std::string >* Current_event_log_argument_buffer;

void sexp_nodes_init () {
    if (Num_sexp_nodes == 0 || Sexp_nodes == NULL) return;

    WARNINGF (LOCATION, "Reinitializing sexp nodes...");
    WARNINGF (LOCATION, "Entered function with %d nodes.", Num_sexp_nodes);

    // usually, the persistent nodes are grouped at the beginning of the array;
    // so we ought to be able to free all the subsequent nodes
    int i, last_persistent_node = -1;

    for (i = 0; i < Num_sexp_nodes; i++) {
        if (Sexp_nodes[i].type & SEXP_FLAG_PERSISTENT)
            last_persistent_node = i; // keep track of it
        else
            Sexp_nodes[i].type = SEXP_NOT_USED; // it's not needed
    }

    WARNINGF (LOCATION, "Last persistent node index is %d.", last_persistent_node);

    // if all the persistent nodes are gone, free all the nodes
    if (last_persistent_node == -1) {
        free (Sexp_nodes);
        Sexp_nodes = NULL;
        Num_sexp_nodes = 0;
    }
    // if there's enough of a difference to make it worthwhile, free some nodes
    else if (Num_sexp_nodes - (last_persistent_node + 1) > 2 * SEXP_NODE_INCREMENT) {
        // round it up to the next evenly divisible size
        Num_sexp_nodes = (last_persistent_node + 1);
        Num_sexp_nodes += SEXP_NODE_INCREMENT - (Num_sexp_nodes % SEXP_NODE_INCREMENT);

        Sexp_nodes = (sexp_node*)realloc (Sexp_nodes, sizeof (sexp_node) * Num_sexp_nodes);
        ASSERT (Sexp_nodes != NULL);
    }

    WARNINGF (LOCATION, "Exited function with %d nodes.", Num_sexp_nodes);
}

static void sexp_nodes_close () {
    // free all sexp nodes... should only be done on game shutdown
    if (Sexp_nodes != NULL) {
        free (Sexp_nodes);
        Sexp_nodes = NULL;
        Num_sexp_nodes = 0;
    }
}

void init_sexp () {
    // Goober5000
    Sexp_replacement_arguments.clear ();
    Sexp_applicable_argument_list.expunge ();
    Sexp_current_argument_nesting_level = 0;

    sexp_nodes_init ();
    init_sexp_vars ();
    Locked_sexp_false = Locked_sexp_true = -1;

    Locked_sexp_false = alloc_sexp ("false", SEXP_LIST, SEXP_ATOM_OPERATOR, -1, -1);
    ASSERT (Locked_sexp_false != -1);
    Sexp_nodes[Locked_sexp_false].type = SEXP_ATOM; // fix bypassing value
    Sexp_nodes[Locked_sexp_false].value = SEXP_KNOWN_FALSE;

    Locked_sexp_true = alloc_sexp ("true", SEXP_LIST, SEXP_ATOM_OPERATOR, -1, -1);
    ASSERT (Locked_sexp_true != -1);
    Sexp_nodes[Locked_sexp_true].type = SEXP_ATOM; // fix bypassing value
    Sexp_nodes[Locked_sexp_true].value = SEXP_KNOWN_TRUE;
}

void sexp_shutdown () { sexp_nodes_close (); }

/**
 * Allocate an sexp node.
 */
int alloc_sexp (const char* text, int type, int subtype, int first, int rest) {
    int node;
    int sexp_const = get_operator_const (text);

    if ((sexp_const == OP_TRUE) && (type == SEXP_ATOM) && (subtype == SEXP_ATOM_OPERATOR))
        return Locked_sexp_true;

    else if ((sexp_const == OP_FALSE) && (type == SEXP_ATOM) && (subtype == SEXP_ATOM_OPERATOR))
        return Locked_sexp_false;

    node = find_free_sexp ();

    // need more sexp nodes?
    if (node == Num_sexp_nodes || node == -1) {
        int old_size = Num_sexp_nodes;

        ASSERT (SEXP_NODE_INCREMENT > 0);

        // allocate in blocks of SEXP_NODE_INCREMENT
        Num_sexp_nodes += SEXP_NODE_INCREMENT;
        Sexp_nodes = (sexp_node*)realloc (Sexp_nodes, sizeof (sexp_node) * Num_sexp_nodes);

        ASSERT (Sexp_nodes != NULL);
        WARNINGF (LOCATION, "Bumping dynamic sexp node limit from %d to %d...", old_size, Num_sexp_nodes);

        // clear all the new sexp nodes we just allocated
        memset (&Sexp_nodes[old_size], 0,
                sizeof (sexp_node) * SEXP_NODE_INCREMENT); //-V512

        // our new sexp is the first out of the ones we just created
        node = old_size;
    }

    ASSERT (node != Locked_sexp_true);
    ASSERT (node != Locked_sexp_false);
    ASSERT (strlen (text) < TOKEN_LENGTH);
    ASSERT (type >= 0);

    strcpy (Sexp_nodes[node].text, text);
    Sexp_nodes[node].type = type;
    Sexp_nodes[node].subtype = subtype;
    Sexp_nodes[node].first = first;
    Sexp_nodes[node].rest = rest;
    Sexp_nodes[node].value = SEXP_UNKNOWN;
    Sexp_nodes[node].flags = SNF_DEFAULT_VALUE; // Goober5000
    Sexp_nodes[node].op_index = NO_OPERATOR_INDEX_DEFINED;

    return node;
}

/**
 * Find the next free sexp and return its index.
 */
int find_free_sexp () {
    int i;

    // sanity
    if (Num_sexp_nodes == 0 || Sexp_nodes == NULL) return -1;

    for (i = 0; i < Num_sexp_nodes; i++) {
        if (Sexp_nodes[i].type == SEXP_NOT_USED) return i;
    }

    return -1;
}

/**
 * Mark a whole sexp tree with the persistent flag so that it won't get re-used
 * between missions
 */
void sexp_mark_persistent (int n) {
    if (n == -1) { return; }

    // total hack because of the true/false locked sexps -- we should make
    // those persistent as well
    if ((n == Locked_sexp_true) || (n == Locked_sexp_false)) { return; }

    ASSERT (!(Sexp_nodes[n].type & SEXP_FLAG_PERSISTENT));
    Sexp_nodes[n].type |= SEXP_FLAG_PERSISTENT;

    sexp_mark_persistent (Sexp_nodes[n].first);
    sexp_mark_persistent (Sexp_nodes[n].rest);
}

/**
 * Remove the persistent flag from all nodes in the tree
 */
void sexp_unmark_persistent (int n) {
    if (n == -1) { return; }

    if ((n == Locked_sexp_true) || (n == Locked_sexp_false)) { return; }

    ASSERT (Sexp_nodes[n].type & SEXP_FLAG_PERSISTENT);
    Sexp_nodes[n].type &= ~SEXP_FLAG_PERSISTENT;

    sexp_unmark_persistent (Sexp_nodes[n].first);
    sexp_unmark_persistent (Sexp_nodes[n].rest);
}

/**
 * Free a used sexp node, so it can be reused later.
 *
 * Should only be called on an atom or a list, and not an operator.  If on a
 * list, the list and everything in it will be freed (including the operator).
 */
int free_sexp (int num) {
    int i, rest, count = 0;

    ASSERT ((num >= 0) && (num < Num_sexp_nodes));
    ASSERT (Sexp_nodes[num].type != SEXP_NOT_USED); // make sure it is actually used
    ASSERT (!(Sexp_nodes[num].type & SEXP_FLAG_PERSISTENT));

    if ((num == Locked_sexp_true) || (num == Locked_sexp_false)) return 0;

    Sexp_nodes[num].type = SEXP_NOT_USED;
    count++;

    i = Sexp_nodes[num].first;
    while (i != -1) {
        count += free_sexp (i);
        i = Sexp_nodes[i].rest;
    }

    rest = Sexp_nodes[num].rest;
    for (i = 0; i < Num_sexp_nodes; i++) {
        if (Sexp_nodes[i].first == num) Sexp_nodes[i].first = rest;

        if (Sexp_nodes[i].rest == num) Sexp_nodes[i].rest = rest;
    }

    return count; // total elements freed up.
}

/**
 * Free up an entire sexp tree.
 *
 * Because the root node is an operator, instead of a list, we can't simply
 * call free_sexp(). This function should only be called on the root node of an
 * sexp, otherwise the linking will get screwed up.
 */
int free_sexp2 (int num) {
    int i, count = 0;

    if ((num == -1) || (num == Locked_sexp_true) || (num == Locked_sexp_false)) { return 0; }

    i = Sexp_nodes[num].rest;
    while (i != -1) {
        count += free_sexp (i);
        i = Sexp_nodes[i].rest;
    }

    count += free_sexp (num);
    return count;
}

/**
 * Reset the status of all the nodes in a tree, forcing them to all be
 * evaulated again.
 */
void flush_sexp_tree (int node) {
    if (node < 0) { return; }

    Sexp_nodes[node].value = SEXP_UNKNOWN;
    flush_sexp_tree (Sexp_nodes[node].first);
    flush_sexp_tree (Sexp_nodes[node].rest);
}

int verify_sexp_tree (int node) {
    if (node == -1) { return 0; }

    if ((Sexp_nodes[node].type == SEXP_NOT_USED) || (Sexp_nodes[node].first == node) || (Sexp_nodes[node].rest == node)) {
        ASSERTX (0, "Sexp node is corrupt");
        return -1;
    }

    if (Sexp_nodes[node].first != -1) { verify_sexp_tree (Sexp_nodes[node].first); }
    if (Sexp_nodes[node].rest != -1) { verify_sexp_tree (Sexp_nodes[node].rest); }

    return 0;
}

/**
 * @todo CASE OF SEXP VARIABLES - ONLY 1 COPY OF VARIABLE
 */
int dup_sexp_chain (int node) {
    int cur, first, rest;

    if (node == -1) { return -1; }

    // TODO - CASE OF SEXP VARIABLES - ONLY 1 COPY OF VARIABLE
    first = dup_sexp_chain (Sexp_nodes[node].first);
    rest = dup_sexp_chain (Sexp_nodes[node].rest);
    cur = alloc_sexp (Sexp_nodes[node].text, Sexp_nodes[node].type, Sexp_nodes[node].subtype, first, rest);

    if (cur == -1) {
        if (first != -1) { free_sexp (first); }
        if (rest != -1) { free_sexp (rest); }
    }

    return cur;
}

/**
 * Compare SEXP chains
 * @return 1 if they are the same, 0 if different
 */
int cmp_sexp_chains (int node1, int node2) {
    if ((node1 == -1) && (node2 == -1)) { return 1; }

    if ((node1 == -1) || (node2 == -1)) { return 0; }

    // DA: 1/7/99 Need to check the actual Sexp_node.text, not possible
    // variable, which can be equal
    if (strcasecmp (Sexp_nodes[node1].text, Sexp_nodes[node2].text) != 0) { return 0; }

    if (!cmp_sexp_chains (Sexp_nodes[node1].first, Sexp_nodes[node2].first)) { return 0; }

    if (!cmp_sexp_chains (Sexp_nodes[node1].rest, Sexp_nodes[node2].rest)) { return 0; }

    return 1;
}

/**
 * Determine if an sexp node is within the given sexp chain.
 */
int query_node_in_sexp (int node, int sexp) {
    if (sexp == -1) { return 0; }
    if (node == sexp) { return 1; }

    if (query_node_in_sexp (node, Sexp_nodes[sexp].first)) { return 1; }
    if (query_node_in_sexp (node, Sexp_nodes[sexp].rest)) { return 1; }

    return 0;
}

/**
 * Find the index of the list associated with an operator
 */
int find_sexp_list (int num) {
    int i;

    for (i = 0; i < Num_sexp_nodes; i++) {
        if (Sexp_nodes[i].first == num) return i;
    }

    // not found
    return -1;
}

/**
 * Find node of operator that item is an argument of.
 */
int find_parent_operator (int node) {
    int i;
    ASSERT ((node >= 0) && (node < Num_sexp_nodes));

    if (Sexp_nodes[node].subtype == SEXP_ATOM_OPERATOR) {
        node = find_sexp_list (node);

        // are we already at the top of the list?  this will happen for
        // non-standard sexps (sexps that fire instantly instead of using a
        // conditional) such as: $Formula: ( do-nothing )
        if (node < 0) return -1;
    }

    // iterate backwards through the sexps nodes (i.e. do the inverse of CDR)
    while (Sexp_nodes[node].subtype != SEXP_ATOM_OPERATOR) {
        for (i = 0; i < Num_sexp_nodes; i++) {
            if (Sexp_nodes[i].rest == node) break;
        }

        if (i == Num_sexp_nodes) return -1; // not found, probably at top node already.

        node = i;
    }

    return node;
}

/**
 * Determine if an sexpression node is the top level node of an sexpression
 * tree.
 *
 * Top level nodes do not have their node id in anyone elses first or rest
 * index.
 */
int is_sexp_top_level (int node) {
    int i;

    ASSERT ((node >= 0) && (node < Num_sexp_nodes));

    if (Sexp_nodes[node].type == SEXP_NOT_USED) return 0;

    for (i = 0; i < Num_sexp_nodes; i++) {
        if ((Sexp_nodes[i].type == SEXP_NOT_USED) || (i == node)) // don't check myself or unused nodes
            continue;

        if ((Sexp_nodes[i].first == node) || (Sexp_nodes[i].rest == node)) return 0;
    }

    return 1;
}

/**
 * Find argument number
 */
int find_argnum (int parent_node, int arg_node) {
    int n, tally;
    ASSERTX ((parent_node >= 0) && (parent_node < Num_sexp_nodes), "find_argnum was passed an invalid parent!");
    ASSERTX ((arg_node >= 0) && (arg_node < Num_sexp_nodes), "find_argnum was passed an invalid child!");

    n = CDR (parent_node);
    tally = 0;

    while (n >= 0) {
        // check if there is an operator node at this position which matches
        // our expected node
        if (CAR (n) == arg_node) return tally;

        tally++;
        n = CDR (n);
    }

    // argument node not found
    return -1;
}

/**
 * From an operator name, return its index in the array Operators
 */
int get_operator_index (const char* token) {
    ASSERTX (token != NULL, "get_operator_index(char*) called with a null token; get a coder!\n");

    for (size_t i = 0; i < Operators.size (); i++) {
        if (Operators[i].text == token) { return (int)i; }
    }

    return NOT_A_SEXP_OPERATOR;
}

/**
 * From a sexp node, return the index in the array Operators or
 * NOT_A_SEXP_OPERATOR if not an operator
 */
int get_operator_index (int node) {
    ASSERTX (node >= 0 && node < Num_sexp_nodes, "Passed an out-of-range node index (%d) to get_operator_index(int)!", node);

    if (Sexp_nodes[node].op_index != NO_OPERATOR_INDEX_DEFINED) { return Sexp_nodes[node].op_index; }

    int index = get_operator_index (Sexp_nodes[node].text);
    Sexp_nodes[node].op_index = index;
    return index;
}

/**
 * From an operator name, return its constant (the number it was define'd with)
 */
int get_operator_const (const char* token) {
    int idx = get_operator_index (token);

    if (idx == NOT_A_SEXP_OPERATOR) return 0;

    return Operators[idx].value;
}

int get_operator_const (int node) {
    if (Sexp_nodes[node].op_index >= 0) { return Operators[Sexp_nodes[node].op_index].value; }

    int idx = get_operator_index (node);

    if (idx == NOT_A_SEXP_OPERATOR) return 0;

    return Operators[idx].value;
}

int query_sexp_args_count (int node, bool only_valid_args = false) {
    int count = 0;
    int n = CDR (node);

    for (; n != -1; n = CDR (n)) {
        if (only_valid_args && !(Sexp_nodes[n].flags & SNF_ARGUMENT_VALID)) continue;

        count++;
    }

    return count;
}

/**
 * Needed to fix bug with sexps like send-message list which have arguments
 * that need to be supplied as a block
 *
 * @return 0 if the number of arguments for the supplied operation is wrong, 1
 * otherwise.
 */
int check_operator_argument_count (int count, int op) {
    if (count < Operators[op].min || count > Operators[op].max) return 0;

    // send-message-list has arguments as blocks of 4
    if (op == OP_SEND_MESSAGE_LIST)
        if (count % 4 != 0) return 0;

    return 1;
}

/**
 * Check SEXP syntax
 * @return 0 if ok, negative if there's an error in expression..
 * See the returns types in sexp.h
 */
int check_sexp_syntax (int node, int return_type, int recursive, int* bad_node, int) {
    int i = 0, z, t, type, argnum = 0, count, op, type2 = 0, op2;
    int op_node;
    int var_index = -1;

    ASSERT (node >= 0 && node < Num_sexp_nodes);
    ASSERT (Sexp_nodes[node].type != SEXP_NOT_USED);
    if (Sexp_nodes[node].subtype == SEXP_ATOM_NUMBER && return_type == OPR_BOOL) {
        // special case Mark seems to want supported
        ASSERT (Sexp_nodes[node].first == -1); // only lists should have a first pointer
        if (Sexp_nodes[node].rest != -1)       // anything after the number?
            return SEXP_CHECK_NONOP_ARGS;      // if so, it's a syntax error

        return 0;
    }

    op_node = node; // save the node of the operator since we need to get to
                    // other args.
    if (bad_node) *bad_node = op_node;

    if (Sexp_nodes[op_node].subtype != SEXP_ATOM_OPERATOR)
        return SEXP_CHECK_OP_EXPECTED; // not an operator, which it should
                                       // always be

    op = get_operator_index (CTEXT (op_node));
    if (op == -1) return SEXP_CHECK_UNKNOWN_OP; // unrecognized operator

    // check that types match - except that OPR_AMBIGUOUS matches everything
    if (return_type != OPR_AMBIGUOUS) {
        // get the return type of the next thing
        z = query_operator_return_type (op);
        if (z == OPR_POSITIVE && return_type == OPR_NUMBER) {
            // positive data type can map to number data type just fine
        }
        // Goober5000's number hack
        else if (z == OPR_NUMBER && return_type == OPR_POSITIVE) {
            // this isn't kosher, but we hack it to make it work
        }
        else if (z != return_type) {
            // anything else is a mismatch
            return SEXP_CHECK_TYPE_MISMATCH;
        }
    }

    count = query_sexp_args_count (op_node);

    if (!check_operator_argument_count (count, op)) return SEXP_CHECK_BAD_ARG_COUNT; // incorrect number of arguments

    // Goober5000 - if this is a list of stuff that has the special argument as
    // an item in the list, assume it's valid
    if (special_argument_appears_in_sexp_list (op_node)) return 0;

    node = Sexp_nodes[op_node].rest;
    while (node != -1) {
        type = query_operator_argument_type (op, argnum);
        ASSERT (Sexp_nodes[node].type != SEXP_NOT_USED);
        if (bad_node) *bad_node = node;

        if (Sexp_nodes[node].subtype == SEXP_ATOM_LIST) {
            i = Sexp_nodes[node].first;
            if (bad_node) *bad_node = i;

            // be sure to check to see if this node is a list of stuff and not
            // an actual operator type thing.  (i.e. in the case of a cond
            // statement, the conditional will fall into this if statement.
            // MORE TO DO HERE!!!!
            if (Sexp_nodes[i].subtype == SEXP_ATOM_LIST) return 0;

            op2 = get_operator_index (CTEXT (i));
            if (op2 == -1) return SEXP_CHECK_UNKNOWN_OP;

            type2 = query_operator_return_type (op2);
            if (recursive) {
                switch (type) {
                case OPF_NUMBER: t = OPR_NUMBER; break;

                case OPF_POSITIVE: t = OPR_POSITIVE; break;

                case OPF_BOOL: t = OPR_BOOL; break;

                case OPF_NULL: t = OPR_NULL; break;

                case OPF_AI_GOAL: t = OPR_AI_GOAL; break;

                // special case for modify-variable
                case OPF_AMBIGUOUS: t = OPR_AMBIGUOUS; break;

                default:
                    return SEXP_CHECK_UNKNOWN_TYPE; // no other return types
                                                    // available
                }

                if ((z = check_sexp_syntax (i, t, recursive, bad_node)) != 0) { return z; }
            }
        }
        else if (Sexp_nodes[node].subtype == SEXP_ATOM_NUMBER) {
            char* ptr;

            type2 = OPR_POSITIVE;
            ptr = CTEXT (node);
            if (*ptr == '-') {
                type2 = OPR_NUMBER;
                ptr++;
            }

            if (type == OPF_BOOL) // allow numbers to be used where boolean is
                                  // required.
                type2 = OPR_BOOL;

            while (*ptr) {
                if (!isdigit (*ptr)) return SEXP_CHECK_INVALID_NUM; // not a valid number

                ptr++;
            }

            i = atoi (CTEXT (node));
            z = get_operator_const (CTEXT (op_node));
            if ((z == OP_HAS_DOCKED_DELAY) || (z == OP_HAS_UNDOCKED_DELAY))
                if ((argnum == 2) && (i < 1)) return SEXP_CHECK_NUM_RANGE_INVALID;

            z = get_operator_index (CTEXT (op_node));
            if ((query_operator_return_type (z) == OPR_AI_GOAL) && (argnum == Operators[op].min - 1))
                if ((i < 0) || (i > 200)) return SEXP_CHECK_NUM_RANGE_INVALID;
        }
        else if (Sexp_nodes[node].subtype == SEXP_ATOM_STRING) { type2 = SEXP_ATOM_STRING; }
        else { ASSERT (0); }

        // variables should only be typechecked.
        if ((Sexp_nodes[node].type & SEXP_FLAG_VARIABLE) && (type != OPF_VARIABLE_NAME)) {
            var_index = get_index_sexp_variable_from_node (node);
            ASSERT (var_index != -1);

            switch (type) {
            case OPF_NUMBER:
            case OPF_POSITIVE:
                if (!(Sexp_variables[var_index].type & SEXP_VARIABLE_NUMBER)) return SEXP_CHECK_INVALID_VARIABLE_TYPE;
                break;

            case OPF_AMBIGUOUS: break;

            default:
                if (!(Sexp_variables[var_index].type & SEXP_VARIABLE_STRING)) return SEXP_CHECK_INVALID_VARIABLE_TYPE;
            }
            node = Sexp_nodes[node].rest;
            argnum++;
            continue;
        }

        switch (type) {
        case OPF_NUMBER:
            if ((type2 != OPR_NUMBER) && (type2 != OPR_POSITIVE)) { return SEXP_CHECK_TYPE_MISMATCH; }

            break;

        case OPF_POSITIVE:
            if (type2 == OPR_NUMBER) {
                // Goober5000's number hack
                break;
                // return SEXP_CHECK_NEGATIVE_NUM;
            }

            if (type2 != OPR_POSITIVE) { return SEXP_CHECK_TYPE_MISMATCH; }

            break;

        case OPF_SHIP_NOT_PLAYER:
            if (type2 != SEXP_ATOM_STRING) { return SEXP_CHECK_TYPE_MISMATCH; }

            if (ship_name_lookup (CTEXT (node), 0) < 0) {
                if (!mission_parse_get_arrival_ship (CTEXT (node))) { return SEXP_CHECK_INVALID_SHIP; }
            }

            break;

        case OPF_SHIP:
        case OPF_SHIP_POINT:
            if (type2 != SEXP_ATOM_STRING) { return SEXP_CHECK_TYPE_MISMATCH; }

            if (ship_name_lookup (CTEXT (node), 1) < 0) {
                if (!mission_parse_get_arrival_ship (CTEXT (node))) {
                    if (type == OPF_SHIP) { // return invalid ship if not also
                                            // looking for point
                        return SEXP_CHECK_INVALID_SHIP;
                    }

                    if (find_matching_waypoint (CTEXT (node)) == NULL) {
                        if (verify_vector (CTEXT (node))) // verify return non-zero on
                                                          // invalid point
                        {
                            return SEXP_CHECK_INVALID_POINT;
                        }
                    }
                }
            }

            break;

        case OPF_WING:
            if (type2 != SEXP_ATOM_STRING) { return SEXP_CHECK_TYPE_MISMATCH; }

            if (wing_name_lookup (CTEXT (node), 1) < 0) { return SEXP_CHECK_INVALID_WING; }

            break;

        case OPF_SHIP_WING:
        case OPF_SHIP_WING_POINT:
            if (type2 != SEXP_ATOM_STRING) { return SEXP_CHECK_TYPE_MISMATCH; }

            // all of these have ships and wings in common
            if (ship_name_lookup (CTEXT (node), 1) >= 0 || wing_name_lookup (CTEXT (node), 1) >= 0) { break; }
            // also check arrival list if we're running the game
            if (mission_parse_get_arrival_ship (CTEXT (node))) { break; }

            // only other possibility is waypoints
            if (type == OPF_SHIP_WING_POINT) {
                if (find_matching_waypoint (CTEXT (node)) == NULL) {
                    if (verify_vector (CTEXT (node))) { // non-zero on verify
                                                        // vector mean invalid!
                        return SEXP_CHECK_INVALID_POINT;
                    }
                }
                break;
            }

            // nothing left
            return SEXP_CHECK_INVALID_SHIP_WING;

        case OPF_AWACS_SUBSYSTEM:
        case OPF_SUBSYSTEM: {
            char* shipname;
            int shipnum, ship_class;
            int ship_index;

            if (type2 != SEXP_ATOM_STRING) { return SEXP_CHECK_TYPE_MISMATCH; }

            // we must get the model of the ship that is part of this
            // sexpression and find a subsystem with that name.  This code
            // assumes by default that the ship is *always* the first name in
            // the sexpression.  If this is ever not the case, the code here
            // must be changed to get the correct ship name.
            switch (get_operator_const (CTEXT (op_node))) {
            case OP_CAP_SUBSYS_CARGO_KNOWN_DELAY:
                ship_index = CDR (CDR (op_node)); break;

            case OP_BEAM_FIRE:
                if (argnum == 1) { ship_index = CDR (op_node); }
                else { ship_index = CDR (CDR (CDR (op_node))); }
                break;

            default: ship_index = CDR (op_node); break;
            }

            shipname = CTEXT (ship_index);
            shipnum = ship_name_lookup (shipname, 1);
            if (shipnum >= 0) { ship_class = Ships[shipnum].ship_info_index; }
            else {
                // must try to find the ship in the arrival list
                p_object* p_objp = mission_parse_get_arrival_ship (shipname);

                if (!p_objp) {
                    return SEXP_CHECK_INVALID_SHIP;
                }

                ship_class = p_objp->ship_class;
            }

            // check for the special "hull" value
            if ((Operators[op].value == OP_SABOTAGE_SUBSYSTEM) ||
                (Operators[op].value == OP_REPAIR_SUBSYSTEM) ||
                (Operators[op].value == OP_SET_SUBSYSTEM_STRNGTH) ||
                (Operators[op].value == OP_BEAM_FIRE)) {
                if (!strcasecmp (CTEXT (node), SEXP_HULL_STRING) ||
                    !strcasecmp (CTEXT (node), SEXP_SIM_HULL_STRING)) {
                    break;
                }
            }

            for (i = 0; i < Ship_info[ship_class].n_subsystems; i++) {
                if (!subsystem_strcasecmp (Ship_info[ship_class].subsystems[i].subobj_name, CTEXT (node))) { break; }
            }

            if (i == Ship_info[ship_class].n_subsystems) {
                return SEXP_CHECK_INVALID_SUBSYS;
            }

            break;
        }

        case OPF_POINT:
            if (type2 != SEXP_ATOM_STRING) { return SEXP_CHECK_TYPE_MISMATCH; }

            if (find_matching_waypoint (CTEXT (node)) == NULL) {
                if (verify_vector (CTEXT (node))) { return SEXP_CHECK_INVALID_POINT; }
            }

            break;

        case OPF_IFF:
            if (type2 != SEXP_ATOM_STRING) { return SEXP_CHECK_TYPE_MISMATCH; }

            if (iff_lookup (CTEXT (node)) < 0) { return SEXP_CHECK_INVALID_IFF; }

            break;

        case OPF_BOOL:
            if (type2 != OPR_BOOL) { return SEXP_CHECK_TYPE_MISMATCH; }

            break;

        case OPF_AI_ORDER:
            if (type2 != SEXP_ATOM_STRING) { return SEXP_CHECK_TYPE_MISMATCH; }

            break;

        case OPF_NULL:
            if (type2 != OPR_NULL) { return SEXP_CHECK_TYPE_MISMATCH; }

            break;

        case OPF_AI_GOAL:
            if (type2 != OPR_AI_GOAL) { return SEXP_CHECK_TYPE_MISMATCH; }

            // we should check the syntax of the actual goal!!!!
            z = Sexp_nodes[node].first;
            if ((z = check_sexp_syntax (z, OPR_AI_GOAL, recursive, bad_node)) != 0) { return z; }

            break;

        case OPF_SHIP_TYPE:
            if (type2 != SEXP_ATOM_STRING) { return SEXP_CHECK_TYPE_MISMATCH; }

            i = ship_type_name_lookup (CTEXT (node));

            if (i < 0) { return SEXP_CHECK_INVALID_SHIP_TYPE; }

            break;

        case OPF_WAYPOINT_PATH:
            if (find_matching_waypoint_list (CTEXT (node)) == NULL) { return SEXP_CHECK_TYPE_MISMATCH; }
            break;

        case OPF_MESSAGE:
            if (type2 != SEXP_ATOM_STRING) return SEXP_CHECK_TYPE_MISMATCH;

            break;

        case OPF_PRIORITY: {
            if (type2 != SEXP_ATOM_STRING) return SEXP_CHECK_TYPE_MISMATCH;

            break;
        }

        case OPF_MISSION_NAME:
            if (type2 != SEXP_ATOM_STRING) return SEXP_CHECK_TYPE_MISMATCH;
            break;

        case OPF_GOAL_NAME:
            if (type2 != SEXP_ATOM_STRING) return SEXP_CHECK_TYPE_MISMATCH;

            // we only need to check the campaign list if running in Fred and
            // are in campaign mode. otherwise, check the set of current goals
            // MWA -- short circuit evaluation of these things for now.
            if ((Operators[op].value == OP_PREVIOUS_GOAL_TRUE) || (Operators[op].value == OP_PREVIOUS_GOAL_FALSE) || (Operators[op].value == OP_PREVIOUS_GOAL_INCOMPLETE)) break;

            for (i = 0; i < Num_goals; i++)
                if (!strcasecmp (CTEXT (node), Mission_goals[i].name)) break;

            if (i == Num_goals) return SEXP_CHECK_INVALID_GOAL_NAME;

            break;

        case OPF_EVENT_NAME:
            if (type2 != SEXP_ATOM_STRING) return SEXP_CHECK_TYPE_MISMATCH;

            // like above checking for goals, check events in the campaign only
            // if in Fred and only if in campaign mode.  Otherwise, check the
            // current set of events
            // MWA -- short circuit evaluation of these things for now.
            if ((Operators[op].value == OP_PREVIOUS_EVENT_TRUE) || (Operators[op].value == OP_PREVIOUS_EVENT_FALSE) || (Operators[op].value == OP_PREVIOUS_EVENT_INCOMPLETE)) break;

            for (i = 0; i < Num_mission_events; i++) {
                if (!strcasecmp (CTEXT (node), Mission_events[i].name)) break;
            }

            if (i == Num_mission_events) return SEXP_CHECK_INVALID_EVENT_NAME;

            break;

        case OPF_DOCKER_POINT:
            if (type2 != SEXP_ATOM_STRING) return SEXP_CHECK_TYPE_MISMATCH;

            // This makes massive assumptions about the structure of the SEXP
            // using it. If you add any new SEXPs that use this OPF, you will
            // probably need to edit this section to accommodate them.
            break;

        case OPF_DOCKEE_POINT:
            if (type2 != SEXP_ATOM_STRING) return SEXP_CHECK_TYPE_MISMATCH;

            // This makes massive assumptions about the structure of the SEXP
            // using it. If you add any new SEXPs that use this OPF, you will
            // probably need to edit this section to accommodate them.
            break;

        case OPF_WHO_FROM:
            if (type2 != SEXP_ATOM_STRING) return SEXP_CHECK_TYPE_MISMATCH;

            if (*CTEXT (node) != '#') { // not a manual source?
                if (strcasecmp (CTEXT (node), "<any wingman>") != 0)
                    if (strcasecmp (CTEXT (node), "<none>") != 0)                                                    // not a special token?
                        if ((ship_name_lookup (CTEXT (node), TRUE) < 0) && (wing_name_lookup (CTEXT (node), 1) < 0)) // is it in the mission?
                            if (!mission_parse_get_arrival_ship (CTEXT (node))) return SEXP_CHECK_INVALID_MSG_SOURCE;
            }

            break;

        case OPF_KEYPRESS:
            if (type2 != SEXP_ATOM_STRING) return SEXP_CHECK_TYPE_MISMATCH;

            break;

        case OPF_SKILL_LEVEL:
            if (type2 != SEXP_ATOM_STRING) return SEXP_CHECK_TYPE_MISMATCH;

            for (i = 0; i < NUM_SKILL_LEVELS; i++) {
                if (!strcasecmp (CTEXT (node), Skill_level_names (i, 0))) break;
            }
            if (i == NUM_SKILL_LEVELS) return SEXP_CHECK_INVALID_SKILL_LEVEL;
            break;

        case OPF_MEDAL_NAME:
            if (type2 != SEXP_ATOM_STRING) return SEXP_CHECK_TYPE_MISMATCH;

            for (i = 0; i < Num_medals; i++) {
                if (!strcasecmp (CTEXT (node), Medals[i].name)) break;
            }

            if (i == Num_medals) return SEXP_CHECK_INVALID_MEDAL_NAME;
            break;

        case OPF_HUGE_WEAPON:
        case OPF_WEAPON_NAME:
            if (type2 != SEXP_ATOM_STRING) return SEXP_CHECK_TYPE_MISMATCH;

            for (i = 0; i < Num_weapon_types; i++) {
                if (!strcasecmp (CTEXT (node), Weapon_info[i].name)) break;
            }

            if (i == Num_weapon_types) return SEXP_CHECK_INVALID_WEAPON_NAME;

            // we need to be sure that for huge weapons, the WIF_HUGE flag is
            // set
            if (type == OPF_HUGE_WEAPON) {
                if (!(Weapon_info[i].wi_flags[Weapon::Info_Flags::Huge])) return SEXP_CHECK_INVALID_WEAPON_NAME;
            }

            break;

        case OPF_SHIP_CLASS_NAME:
            if (type2 != SEXP_ATOM_STRING) return SEXP_CHECK_TYPE_MISMATCH;

            if (ship_info_lookup (CTEXT (node)) < 0) return SEXP_CHECK_INVALID_SHIP_CLASS_NAME;

            break;

        case OPF_HUD_GAUGE_NAME:
            if (type2 != SEXP_ATOM_STRING) return SEXP_CHECK_TYPE_MISMATCH;

            for (i = 0; i < NUM_HUD_GAUGES; i++) {
                if (!strcasecmp (CTEXT (node), HUD_gauge_text[i])) break;
            }

            // if we reached the end of the list, then the name is invalid
            if (i == NUM_HUD_GAUGES) return SEXP_CHECK_INVALID_GAUGE_NAME;

            break;

        case OPF_JUMP_NODE_NAME:
            if (type2 != SEXP_ATOM_STRING) return SEXP_CHECK_TYPE_MISMATCH;

            if (jumpnode_get_by_name (CTEXT (node)) == NULL) return SEXP_CHECK_INVALID_JUMP_NODE;

            break;

        case OPF_VARIABLE_NAME:
            var_index = get_index_sexp_variable_from_node (node);

            if (var_index == -1) {
                return SEXP_CHECK_INVALID_VARIABLE;
            }

            break;

        case OPF_AMBIGUOUS:
            // type checking for modify-variable
            // string or number -- anything goes
            break;

        default:
            ASSERTX (0, "Unhandled argument format");
            break;
        }

        node = Sexp_nodes[node].rest;
        argnum++;
    }

    return 0;
}

// Goober5000
void get_unformatted_sexp_variable_name (char* unformatted, char* formatted_pre) {
    char* formatted;

    // Goober5000 - trim @ if needed
    if (formatted_pre[0] == SEXP_VARIABLE_CHAR)
        formatted = formatted_pre + 1;
    else
        formatted = formatted_pre;

    // get variable name (up to '['
    auto end_index = strcspn (formatted, "[");
    ASSERT ((end_index != 0) && (end_index < TOKEN_LENGTH - 1));
    strncpy (unformatted, formatted, end_index);
    unformatted[end_index] = '\0';
}

/**
 * Get text to stuff into Sexp_node in case of variable
 *
 * If Fred_running(that is never) - stuff Sexp_variables[].variable_name
 * otherwise - stuff index into Sexp_variables array.
 */
void get_sexp_text_for_variable (char* text, char* token) {
    int sexp_var_index;

    get_unformatted_sexp_variable_name (text, token);

    // freespace - get index into Sexp_variables array
    sexp_var_index = get_index_sexp_variable_name (text);
    ASSERT (sexp_var_index != -1);
    sprintf (text, "%d", sexp_var_index);
}

/**
 * Returns the first sexp index of data this function allocates. (start of this
 * sexp)
 *
 * NOTE: On entry into this function, Mp points to the first character past the
 * opening parenthesis.
 */
int get_sexp () {
    int start, node, last, op, count;
    char token[TOKEN_LENGTH];
    char variable_text[TOKEN_LENGTH];

    ASSERT (*(Mp - 1) == '(');

    // start - the node allocated in first instance of function
    // node - the node allocated in current instance of function
    // count - number of nodes allocated this instance of function [do we set
    // last.rest or .first] variable - whether string or number is a variable
    // referencing Sexp_variables

    // initialization
    start = last = -1;
    count = 0;

    ignore_white_space ();
    while (*Mp != ')') {
        // end of string or end of file
        if (*Mp == '\0') {
            ASSERTX (0, "Unexpected end of sexp!");
            return -1;
        }

        // Sexp list
        if (*Mp == '(') {
            Mp++;
            node = alloc_sexp ("", SEXP_LIST, SEXP_ATOM_LIST, get_sexp (), -1);
        }

        // Sexp string
        else if (*Mp == '\"') {
            auto len = strcspn (Mp + 1, "\"");
            // was closing quote not found?
            if (*(Mp + 1 + len) != '\"') {
                ASSERTX (0, "Unexpected end of quoted string embedded in sexp!");
                return -1;
            }

            // check if string variable
            if (*(Mp + 1) == SEXP_VARIABLE_CHAR) {
                char variable_token[2 * TOKEN_LENGTH + 2]; // variable_token[contents_token]

                // reduce length by 1 for end \"
                auto length = len - 1;
                if (length >= 2 * TOKEN_LENGTH + 2) {
                    ASSERTX (0, "Variable token %s is too long. Needs to be %d characters or shorter.", Mp, 2 * TOKEN_LENGTH + 2 - 1);
                    return -1;
                }

                // start copying after skipping 1st char (i.e. variable char)
                strncpy (variable_token, Mp + 2, length);
                variable_token[length] = 0;

                get_sexp_text_for_variable (variable_text, variable_token);
                node = alloc_sexp (variable_text, (SEXP_ATOM | SEXP_FLAG_VARIABLE), SEXP_ATOM_STRING, -1, -1);
            }
            else {
                // token is too long?
                if (len >= TOKEN_LENGTH) {
                    ASSERTX (0, "Token %s is too long. Needs to be %d characters or shorter.", Mp, TOKEN_LENGTH - 1);
                    return -1;
                }

                strncpy (token, Mp + 1, len);
                token[len] = 0;
                node = alloc_sexp (token, SEXP_ATOM, SEXP_ATOM_STRING, -1, -1);
            }

            // bump past closing \" by 1 char
            Mp += (len + 2);
        }

        // Sexp operator or number
        else {
            int len = 0;
            bool variable = false;
            while (*Mp != ')' && !is_white_space (*Mp)) {
                // numeric variable?
                if ((len == 0) && (*Mp == SEXP_VARIABLE_CHAR)) {
                    variable = true;
                    Mp++;
                    continue;
                }

                // end of string or end of file?
                if (*Mp == '\0') {
                    ASSERTX (0, "Unexpected end of sexp!");
                    return -1;
                }

                // token is too long?
                if (len >= TOKEN_LENGTH - 1) {
                    token[TOKEN_LENGTH - 1] = '\0';
                    ASSERTX (0, "Token %s is too long. Needs to be %d characters or shorter.", token, TOKEN_LENGTH - 1);
                    return -1;
                }

                // build the token
                token[len++] = *Mp++;
            }
            token[len] = 0;

            // maybe replace deprecated names
            if (!strcasecmp (token, "set-ship-position"))
                strcpy (token, "set-object-position");
            else if (!strcasecmp (token, "set-ship-facing"))
                strcpy (token, "set-object-facing");
            else if (!strcasecmp (token, "set-ship-facing-object"))
                strcpy (token, "set-object-facing-object");
            else if (!strcasecmp (token, "ai-chase-any-except"))
                strcpy (token, "ai-chase-any");
            else if (!strcasecmp (token, "change-ship-model"))
                strcpy (token, "change-ship-class");
            else if (!strcasecmp (token, "radar-set-max-range"))
                strcpy (token, "hud-set-max-targeting-range");
            else if (!strcasecmp (token, "ship-subsys-vanished"))
                strcpy (token, "ship-subsys-vanish");
            else if (!strcasecmp (token, "directive-is-variable"))
                strcpy (token, "directive-value");
            else if (!strcasecmp (token, "variable-array-get"))
                strcpy (token, "get-variable-by-index");
            else if (!strcasecmp (token, "variable-array-set"))
                strcpy (token, "set-variable-by-index");

            op = get_operator_index (token);
            if (op >= 0) { node = alloc_sexp (token, SEXP_ATOM, SEXP_ATOM_OPERATOR, -1, -1); }
            else {
                if (variable) {
                    // convert token text for variable
                    get_sexp_text_for_variable (variable_text, token);

                    node = alloc_sexp (variable_text, (SEXP_ATOM | SEXP_FLAG_VARIABLE), SEXP_ATOM_NUMBER, -1, -1);
                }
                else { node = alloc_sexp (token, SEXP_ATOM, SEXP_ATOM_NUMBER, -1, -1); }
            }
        }

        // update links
        if (count++) {
            ASSERT (last != -1);
            Sexp_nodes[last].rest = node;
        }
        else { start = node; }

        ASSERT (node != -1); // ran out of nodes.  Time to raise the MAX!
        last = node;
        ignore_white_space ();
    }

    Mp++; // skip past the ')'

    // Goober5000 - backwards compatibility for removed ai-chase-any-except
    if (get_operator_const (CTEXT (start)) == OP_AI_CHASE_ANY) {
        // if there is more than one argument, free the extras
        int n = CDR (CDR (start));
        if (n >= 0) {
            // free the entire rest of the argument list
            free_sexp2 (n);
        }
    }

    if (OP_SET_SPECIAL_WARPOUT_NAME == (op = get_operator_const (CTEXT (start))))
        // set flag for taylor
        Knossos_warp_ani_used = 1;

    return start;
}

/**
 * Stuffs a list of sexp variables
 */
int stuff_sexp_variable_list () {
    int count;
    char var_name[TOKEN_LENGTH];
    char default_value[TOKEN_LENGTH];
    char str_type[TOKEN_LENGTH];
    char persistent[TOKEN_LENGTH];
    char network[TOKEN_LENGTH];
    int index;
    int type;

    count = 0;
    required_string ("$Variables:");
    ignore_white_space ();

    // check for start of list
    if (*Mp != '(') {
        error_display (1, "Reading sexp variable list.  Found [%c].  Expecting '('.\n", *Mp);
        throw parse::ParseException ("Syntax error");
    }

    Mp++;
    ignore_white_space ();

    while (*Mp != ')') {
        ASSERT (count < MAX_SEXP_VARIABLES);

        // get index - for debug
        stuff_int (&index);
        ignore_gray_space ();

        // get var_name
        get_string (var_name);
        ignore_gray_space ();

        // get default_value;
        get_string (default_value);
        ignore_gray_space ();

        // get type
        get_string (str_type);
        ignore_white_space ();

        // determine type
        if (!strcasecmp (str_type, "number")) { type = SEXP_VARIABLE_NUMBER; }
        else if (!strcasecmp (str_type, "string")) { type = SEXP_VARIABLE_STRING; }
        else if (!strcasecmp (str_type, "block")) {
            // Goober5000 - This looks dangerous... these flags are needed for
            // certain things, but it looks like BLOCK_*_SIZE is the only thing
            // that keeps a block from running off the end of its boundary.
            type = SEXP_VARIABLE_BLOCK;
        }
        else {
            type = SEXP_VARIABLE_UNKNOWN;
            ASSERTX (0, "SEXP variable '%s' is an unknown type!", var_name);
        }

        // possibly get network-variable
        if (check_for_string ("\"network-variable\"")) {
            // eat it
            get_string (network);
            ignore_white_space ();

            // set type
            type |= SEXP_VARIABLE_NETWORK;
        }

        // maybe this is an eternal persistent variable of some type
        if (check_for_string ("\"eternal\"")) {
            // eat it
            get_string (persistent);
            ignore_white_space ();

            // set type
            type |= SEXP_VARIABLE_SAVE_TO_PLAYER_FILE;
        }

        // maybe this is a persistent variable of some type
        if (check_for_string ("\"player-persistent\"") || check_for_string ("\"save-on-mission-close\"")) {
            // eat it
            get_string (persistent);
            ignore_white_space ();

            // set type
            type |= SEXP_VARIABLE_SAVE_ON_MISSION_CLOSE;
        }
        else if (check_for_string ("\"campaign-persistent\"") || check_for_string ("\"save-on-mission-progress\"")) {
            // eat it
            get_string (persistent);
            ignore_white_space ();

            // set type
            type |= SEXP_VARIABLE_SAVE_ON_MISSION_PROGRESS;
            // trap error
        }
        else if (check_for_string ("\"")) {
            // eat garbage
            get_string (persistent);
            ignore_white_space ();

            // notify of error
            ASSERTX (0, "Error parsing sexp variables - unknown persistence type encountered.  You can continue from here without trouble.");
        }

        // check if variable name already exists
        if ((type & SEXP_VARIABLE_NUMBER) || (type & SEXP_VARIABLE_STRING)) { ASSERT (get_index_sexp_variable_name (var_name) == -1); }

        if (type & SEXP_VARIABLE_BLOCK) { add_block_variable (default_value, var_name, type, index); }
        else {
            count++;
            sexp_add_variable (default_value, var_name, type, index);
        }
    }

    Mp++;

    return count;
}

/**
 * Stuff SEXP text string
 */
void stuff_sexp_text_string (std::string& dest, int node, int mode) {
    ASSERT ((node >= 0) && (node < Num_sexp_nodes));

    if (Sexp_nodes[node].type & SEXP_FLAG_VARIABLE) {
        int sexp_variables_index = get_index_sexp_variable_name (Sexp_nodes[node].text);
        // during the last pass through error-reporting mode, sexp variables
        // have already been transcoded to their indexes
        if (mode == SEXP_ERROR_CHECK_MODE && sexp_variables_index < 0) {
            if (can_construe_as_integer (Sexp_nodes[node].text)) { sexp_variables_index = atoi (Sexp_nodes[node].text); }
        }
        ASSERTX (sexp_variables_index != -1, "Couldn't find variable: %s\n", Sexp_nodes[node].text);
        ASSERT ((Sexp_variables[sexp_variables_index].type & SEXP_VARIABLE_NUMBER) || (Sexp_variables[sexp_variables_index].type & SEXP_VARIABLE_STRING));

        // number
        if (Sexp_nodes[node].subtype == SEXP_ATOM_NUMBER) {
            ASSERT (Sexp_variables[sexp_variables_index].type & SEXP_VARIABLE_NUMBER);

            // Error check - can be Fred or FreeSpace
            if (mode == SEXP_ERROR_CHECK_MODE) { sprintf (dest, "%s[%s] ", Sexp_variables[sexp_variables_index].variable_name, Sexp_variables[sexp_variables_index].text); }
            else {
                // Save as string - only  Fred
                ASSERT (mode == SEXP_SAVE_MODE);
                sprintf (dest, "@%s[%s] ", Sexp_nodes[node].text, Sexp_variables[sexp_variables_index].text);
            }
        }
        else {
            // string
            ASSERT (Sexp_nodes[node].subtype == SEXP_ATOM_STRING);
            ASSERT (Sexp_variables[sexp_variables_index].type & SEXP_VARIABLE_STRING);

            // Error check - can be Fred or FreeSpace
            if (mode == SEXP_ERROR_CHECK_MODE) { sprintf (dest, "%s[%s] ", Sexp_nodes[node].text, Sexp_variables[sexp_variables_index].text); }
            else {
                // Save as string - only Fred
                ASSERT (mode == SEXP_SAVE_MODE);
                sprintf (dest, "\"@%s[%s]\" ", Sexp_nodes[node].text, Sexp_variables[sexp_variables_index].text);
            }
        }
    }
    else {
        // not a variable
        if (Sexp_nodes[node].subtype == SEXP_ATOM_STRING) { sprintf (dest, "\"%s\" ", CTEXT (node)); }
        else { sprintf (dest, "%s ", CTEXT (node)); }
    }
}

int build_sexp_string (std::string& accumulator, int cur_node, int level, int mode) {
    std::string buf;
    int node;
    auto old_length = accumulator.length ();

    accumulator += "( ";
    node = cur_node;
    while (node != -1) {
        ASSERT (node >= 0 && node < Num_sexp_nodes);
        if (Sexp_nodes[node].first == -1) {
            // build text to string
            stuff_sexp_text_string (buf, node, mode);
            accumulator += buf;
        }
        else { build_sexp_string (accumulator, Sexp_nodes[node].first, level + 1, mode); }

        node = Sexp_nodes[node].rest;
    }

    accumulator += ") ";
    if ((accumulator.length () - old_length) > 40) {
        accumulator.resize (old_length);
        build_extended_sexp_string (accumulator, cur_node, level, mode);
        return 1;
    }

    return 0;
}

void build_extended_sexp_string (std::string& accumulator, int cur_node, int level, int mode) {
    std::string buf;
    int i, flag = 0, node;

    accumulator += "( ";
    node = cur_node;
    while (node != -1) {
        // not the first line?
        if (flag) {
            for (i = 0; i < level + 1; i++) accumulator += "   ";
        }

        flag = 1;
        ASSERT (node >= 0 && node < Num_sexp_nodes);
        if (Sexp_nodes[node].first == -1) {
            stuff_sexp_text_string (buf, node, mode);
            accumulator += buf;
        }
        else { build_sexp_string (accumulator, Sexp_nodes[node].first, level + 1, mode); }

        accumulator += "\n";
        node = Sexp_nodes[node].rest;
    }

    for (i = 0; i < level; i++) accumulator += "   ";

    accumulator += ")";
}

void convert_sexp_to_string (std::string& dest, int cur_node, int mode) {
    if (cur_node >= 0) {
        dest = "";
        build_sexp_string (dest, cur_node, 0, mode);
    }
    else { dest = "( )"; }
}

// -----------------------------------------------------------------------------------
// Helper methods for getting data from nodes. Cause it's stupid to keep
// re-rolling this stuff for every single SEXP
// -----------------------------------------------------------------------------------

/**
 * Takes a SEXP node which contains the name of a ship and returns the player
 * for that ship or NULL if it is an AI ship
 */
player* get_player_from_ship_node (int node) {
    ASSERT (node != -1);

    int sindex = ship_name_lookup (CTEXT (node));

    if (sindex >= 0) {
        if (Player_obj == &Objects[Ships[sindex].objnum]) { return Player; }
    }

    return NULL;
}

/**
 * Given a node, returns a pointer to the ship or NULL if this isn't the name
 * of a ship
 */
ship* sexp_get_ship_from_node (int node) {
    int sindex;
    ship* shipp = NULL;

    sindex = ship_name_lookup (CTEXT (node));

    if (sindex < 0) { return shipp; }

    if (Ships[sindex].objnum < 0) { return shipp; }

    shipp = &Ships[sindex];
    return shipp;
}

/**
 * Determine if the named ship or wing hasn't arrived yet (wing or ship must be
 * on arrival list)
 */
int sexp_query_has_yet_to_arrive (char* name) {
    int i;

    if (ship_query_state (name) < 0) return 1;

    i = wing_name_lookup (name, 1);

    // has not arrived yet
    if ((i >= 0) && (Wings[i].num_waves >= 0) && !Wings[i].total_arrived_count) { return 1; }

    return 0;
}

// arithmetic functions
int add_sexps (int n) {
    int sum = 0, val;

    if (n != -1) {
        if (CAR (n) != -1) {
            sum = eval_sexp (CAR (n));
            // be sure to check for the NAN value when doing arithmetic -- this
            // value should get propagated to the next highest function.
            if (Sexp_nodes[CAR (n)].value == SEXP_NAN)
                return SEXP_NAN;
            else if (Sexp_nodes[CAR (n)].value == SEXP_NAN_FOREVER)
                return SEXP_NAN_FOREVER;
        }
        else
            sum = atoi (CTEXT (n));

        while (CDR (n) != -1) {
            val = eval_sexp (CDR (n));
            // be sure to check for the NAN value when doing arithmetic -- this
            // value should get propagated to the next highest function.
            if (Sexp_nodes[CDR (n)].value == SEXP_NAN)
                return SEXP_NAN;
            else if (Sexp_nodes[CDR (n)].value == SEXP_NAN_FOREVER)
                return SEXP_NAN_FOREVER;
            sum += val;
            n = CDR (n);
        }
    }

    return sum;
}

int sub_sexps (int n) {
    int sum = 0;

    if (n != -1) {
        if (Sexp_nodes[n].first != -1)
            sum = eval_sexp (CAR (n));
        else
            sum = atoi (CTEXT (n));

        while (CDR (n) != -1) {
            sum -= eval_sexp (CDR (n));
            n = CDR (n);
        }
    }

    return sum;
}

int mul_sexps (int n) {
    int sum = 0;

    if (n != -1) {
        if (Sexp_nodes[n].first != -1)
            sum = eval_sexp (Sexp_nodes[n].first);
        else
            sum = atoi (CTEXT (n));

        while (Sexp_nodes[n].rest != -1) {
            sum *= eval_sexp (Sexp_nodes[n].rest);
            n = Sexp_nodes[n].rest;
        }
    }

    return sum;
}

int div_sexps (int n) {
    int sum = 0;

    if (n != -1) {
        if (Sexp_nodes[n].first != -1)
            sum = eval_sexp (Sexp_nodes[n].first);
        else
            sum = atoi (CTEXT (n));

        while (Sexp_nodes[n].rest != -1) {
            int div = eval_sexp (Sexp_nodes[n].rest);
            n = Sexp_nodes[n].rest;
            if (div == 0) {
                WARNINGF (LOCATION, "Division by zero in sexp. Please check all uses of the / operator for possible causes.");
                continue;
            }
            sum /= div;
        }
    }

    return sum;
}

int mod_sexps (int n) {
    int sum = 0;

    if (n != -1) {
        if (Sexp_nodes[n].first != -1)
            sum = eval_sexp (Sexp_nodes[n].first);
        else
            sum = atoi (CTEXT (n));

        while (Sexp_nodes[n].rest != -1) {
            sum = sum % eval_sexp (Sexp_nodes[n].rest);
            n = Sexp_nodes[n].rest;
        }
    }

    return sum;
}

int rand_internal (int low, int high, int seed = 0) {
    int diff;

    // maybe seed it
    if (seed > 0) srand (seed);

    // get diff - don't allow negative or zero
    diff = high - low;
    if (diff < 0) diff = 0;

    return (low + rand32 () % (diff + 1));
}

int rand_sexp (int n, bool multiple) {
    int low, high, rand_num, seed;

    ASSERT (n >= 0);

    // when getting a saved value
    if (Sexp_nodes[n].value == SEXP_NUM_EVAL) {
        // don't regenerate new random number
        return atoi (CTEXT (n));
    }

    low = eval_num (n);

    // get high
    high = eval_num (CDR (n));

    // is there a seed provided?
    if (CDDR (n) != -1)
        seed = eval_num (CDDR (n));
    else
        seed = 0;

    // get the random number
    rand_num = rand_internal (low, high, seed);

    // when saving the value
    if (!multiple) {
        // set .value and .text so random number is generated only once.
        Sexp_nodes[n].value = SEXP_NUM_EVAL;
        sprintf (Sexp_nodes[n].text, "%d", rand_num);
    }
    // if this is multiple with a nonzero seed provided
    else if (seed > 0) {
        // Set the seed to a new seeded random value. This will ensure that the
        // next time the method is called it will return a predictable but
        // different number from the previous time.
        sprintf (Sexp_nodes[CDDR (n)].text, "%d", rand_internal (1, INT_MAX, seed));
    }

    return rand_num;
}

// boolean evaluation functions.  Evaluate all sexpressions in the 'or'
// operator.  Needed to mark entries in the mission log as essential so when
// pruning the log, we know which entries we might need to keep.
int sexp_or (int n) {
    bool all_false = true;
    bool result = false;

    if (n != -1) {
        if (CAR (n) != -1) {
            result = is_sexp_true (CAR (n)) || result;
            if (Sexp_nodes[CAR (n)].value == SEXP_KNOWN_TRUE)
                return SEXP_KNOWN_TRUE;                        // if one of the OR clauses is TRUE,
                                                               // whole clause is true
            if (Sexp_nodes[CAR (n)].value != SEXP_KNOWN_FALSE) // if the value is still unknown, they all
                                                               // can't be false
                all_false = false;
        }
        // this should never happen, because all arguments which return logical
        // values are operators
        else
            result = (atoi (CTEXT (n)) != 0) || result;

        while (CDR (n) != -1) {
            result = is_sexp_true (CDR (n)) || result;
            if (Sexp_nodes[CDR (n)].value == SEXP_KNOWN_TRUE)
                return SEXP_KNOWN_TRUE;                        // if one of the OR clauses is TRUE,
                                                               // whole clause is true
            if (Sexp_nodes[CDR (n)].value != SEXP_KNOWN_FALSE) // if the value is still unknown, they all
                                                               // can't be false
                all_false = false;

            n = CDR (n);
        }
    }

    if (all_false) return SEXP_KNOWN_FALSE;

    return result ? SEXP_TRUE : SEXP_FALSE;
}

// this function does the 'and' operator.  It will short circuit evaluation
// *but* it will still evaluate other members of the and construct.  I do this
// because I need events in the mission log to get marked as essential for goal
// purposes, and evaluation is pretty much the only way
int sexp_and (int n) {
    bool all_true = true;
    bool result = true;

    if (n != -1) {
        if (CAR (n) != -1) {
            result = is_sexp_true (CAR (n)) && result;
            if (Sexp_nodes[CAR (n)].value == SEXP_KNOWN_FALSE || Sexp_nodes[CAR (n)].value == SEXP_NAN_FOREVER)
                return SEXP_KNOWN_FALSE;                      // if one of the AND clauses is FALSE,
                                                              // whole clause is false
            if (Sexp_nodes[CAR (n)].value != SEXP_KNOWN_TRUE) // if the value is still unknown, they all
                                                              // can't be true
                all_true = false;
        }
        // this should never happen, because all arguments which return logical
        // values are operators
        else
            result = (atoi (CTEXT (n)) != 0) && result;

        while (CDR (n) != -1) {
            result = is_sexp_true (CDR (n)) && result;
            if (Sexp_nodes[CDR (n)].value == SEXP_KNOWN_FALSE || Sexp_nodes[CDR (n)].value == SEXP_NAN_FOREVER)
                return SEXP_KNOWN_FALSE;                      // if one of the AND clauses is FALSE,
                                                              // whole clause is false
            if (Sexp_nodes[CDR (n)].value != SEXP_KNOWN_TRUE) // if the value is still unknown, they all
                                                              // can't be true
                all_true = false;

            n = CDR (n);
        }
    }

    if (all_true) return SEXP_KNOWN_TRUE;

    return result ? SEXP_TRUE : SEXP_FALSE;
}

// this version of the 'and' operator determines whether or not its arguments
// become true in the order in which they are specified in the when statement.
// Should be a simple matter of seeing if anything evaluates to true later than
// something that evaluated to false
int sexp_and_in_sequence (int n) {
    bool all_true = true; // represents whether or not all nodes we have seen
                          // so far are true
    bool result = true;

    if (n != -1) {
        if (CAR (n) != -1) {
            result = is_sexp_true (CAR (n)) && result;
            if (Sexp_nodes[CAR (n)].value == SEXP_KNOWN_FALSE || Sexp_nodes[CAR (n)].value == SEXP_NAN_FOREVER)
                return SEXP_KNOWN_FALSE;                      // if one of the AND clauses is FALSE,
                                                              // whole clause is false
            if (Sexp_nodes[CAR (n)].value != SEXP_KNOWN_TRUE) // if value is true, mark our all_true
                                                              // variable for later checking
                all_true = false;
        }
        // this should never happen, because all arguments which return logical
        // values are operators
        else
            result = (atoi (CTEXT (n)) != 0) && result;

        // a little test -- if the previous sexpressions was true, then mark
        // the node itself as always true.  I did this because of the distance
        // function.  It might become true, then when waiting for the second
        // evalation, it might become false, rendering this function false. So,
        // when one becomes true -- mark it true forever.
        if (result) Sexp_nodes[CAR (n)].value = SEXP_KNOWN_TRUE;

        while (CDR (n) != -1) {
            bool next_result = is_sexp_true (CDR (n));
            if (next_result && !result) // if current result is true, and our running result
                                        // is false, things didn't become true in order
                return SEXP_KNOWN_FALSE;

            result = next_result && result;
            if (Sexp_nodes[CDR (n)].value == SEXP_KNOWN_FALSE || Sexp_nodes[CDR (n)].value == SEXP_NAN_FOREVER)
                return SEXP_KNOWN_FALSE;                      // if one of the AND clauses is FALSE,
                                                              // whole clause is false
            if (Sexp_nodes[CDR (n)].value != SEXP_KNOWN_TRUE) // if the value is still unknown, they all
                                                              // can't be true
                all_true = false;

            // see comment above for explanation of next lines
            if (result) Sexp_nodes[CDR (n)].value = SEXP_KNOWN_TRUE;

            n = CDR (n);
        }
    }

    if (all_true) return SEXP_KNOWN_TRUE;

    return result ? SEXP_TRUE : SEXP_FALSE;
}

// for these four basic boolean operations (not, <, >, and =), we have special
// cases that we must deal with.  We have sexpressions operators that might
// return a NAN type return value (such as the distance between two ships when
// one of the ships is destroyed or departed).  These operations need to check
// for this special NAN value and adjust their return types accordingly.  NAN
// values represent false return values
int sexp_not (int n) {
    bool result = false;

    if (n != -1) {
        if (CAR (n) != -1) {
            result = is_sexp_true (CAR (n));
            if (Sexp_nodes[CAR (n)].value == SEXP_KNOWN_FALSE || Sexp_nodes[CAR (n)].value == SEXP_NAN_FOREVER)
                return SEXP_KNOWN_TRUE;                            // not KNOWN_FALSE == KNOWN_TRUE;
            else if (Sexp_nodes[CAR (n)].value == SEXP_KNOWN_TRUE) // not KNOWN_TRUE == KNOWN_FALSE
                return SEXP_KNOWN_FALSE;
            else if (Sexp_nodes[CAR (n)].value == SEXP_NAN) // not NAN == TRUE
                                                            // (I think)
                return SEXP_TRUE;
        }
        // this should never happen, because all arguments which return logical
        // values are operators
        else
            result = (atoi (CTEXT (n)) != 0);
    }

    return result ? SEXP_FALSE : SEXP_TRUE;
}

#define OSWPT_TYPE_NONE 0
#define OSWPT_TYPE_SHIP 1
#define OSWPT_TYPE_WING 2
#define OSWPT_TYPE_WAYPOINT 3
#define OSWPT_TYPE_SHIP_ON_TEAM 4 // e.g. <any friendly>
#define OSWPT_TYPE_WHOLE_TEAM 5   // e.g. Friendly
#define OSWPT_TYPE_PARSE_OBJECT 6 // a "ship" that hasn't arrived yet
#define OSWPT_TYPE_EXITED 7
#define OSWPT_TYPE_WING_NOT_PRESENT 8 // a wing that hasn't arrived yet or is between waves

// Goober5000
typedef struct object_ship_wing_point_team {
    char* object_name;
    int type;

    p_object* p_objp;
    object* objp;
    ship* shipp;
    wing* wingp;
    waypoint* waypointp;
    int team;

    void clear ();
} object_ship_wing_point_team;

void object_ship_wing_point_team::clear () {
    object_name = NULL;
    type = OSWPT_TYPE_NONE;

    p_objp = NULL;
    objp = NULL;
    shipp = NULL;
    waypointp = NULL;
    wingp = NULL;
    team = -1;
}

void sexp_get_object_ship_wing_point_team (object_ship_wing_point_team* oswpt, char* object_name, bool set_parse_flag_too = false);

void object_ship_wing_point_team_set_ship (object_ship_wing_point_team* oswpt, ship* shipp, bool set_parse_flag_too) {
    oswpt->clear ();

    oswpt->shipp = shipp;
    oswpt->object_name = oswpt->shipp->ship_name;
    oswpt->objp = &Objects[shipp->objnum];
    oswpt->type = OSWPT_TYPE_SHIP;

    if (set_parse_flag_too) { oswpt->p_objp = mission_parse_get_arrival_ship (oswpt->object_name); }
}

void object_ship_wing_point_team_set_ship (object_ship_wing_point_team* oswpt, ship_obj* so, bool set_parse_flag_too) {
    object_ship_wing_point_team_set_ship (oswpt, &Ships[Objects[so->objnum].instance], set_parse_flag_too);
}

// Goober5000
void sexp_get_object_ship_wing_point_team (object_ship_wing_point_team* oswpt, char* object_name, bool set_parse_flag_too) {
    int team, ship_num, wing_num;
    waypoint* wpt;
    p_object* p_objp;

    ASSERT (oswpt != NULL);
    ASSERT (object_name != NULL);

    oswpt->clear ();
    oswpt->object_name = object_name;

    if (!strcasecmp (object_name, SEXP_NONE_STRING)) {
        oswpt->type = OSWPT_TYPE_NONE;
        return;
    }

    // check to see if ship destroyed or departed.  In either case, do nothing.
    if (mission_log_get_time (LOG_SHIP_DEPARTED, object_name, NULL, NULL) || mission_log_get_time (LOG_SHIP_DESTROYED, object_name, NULL, NULL) ||
        mission_log_get_time (LOG_SELF_DESTRUCTED, object_name, NULL, NULL)) {
        oswpt->type = OSWPT_TYPE_EXITED;
        return;
    }

    // the object might be the name of a wing.  Check to see if the wing is
    // destroyed or departed.
    if (mission_log_get_time (LOG_WING_DESTROYED, object_name, NULL, NULL) || mission_log_get_time (LOG_WING_DEPARTED, object_name, NULL, NULL)) {
        oswpt->type = OSWPT_TYPE_EXITED;
        return;
    }

    // check if we have a ship for a target
    ship_num = ship_name_lookup (object_name);
    if (ship_num >= 0) {
        oswpt->type = OSWPT_TYPE_SHIP;

        oswpt->shipp = &Ships[ship_num];
        oswpt->objp = &Objects[oswpt->shipp->objnum];

        if (!set_parse_flag_too) { return; }
    }

    // check to see if we have a parse object instead
    p_objp = mission_parse_get_arrival_ship (object_name);
    if (p_objp != NULL) {
        if (oswpt->type != OSWPT_TYPE_SHIP) { oswpt->type = OSWPT_TYPE_PARSE_OBJECT; }

        oswpt->p_objp = p_objp;

        return;
    }

    // check if we have a wing for a target
    wing_num = wing_name_lookup (object_name, 1);
    if (wing_num >= 0) {
        wing* wingp = &Wings[wing_num];

        // make sure that at least one ship exists
        if (wingp->current_count > 0) {
            oswpt->type = OSWPT_TYPE_WING;
            oswpt->wingp = wingp;

            // point to wing leader if he is valid
            if ((wingp->special_ship >= 0) && (wingp->ship_index[wingp->special_ship] >= 0)) {
                oswpt->shipp = &Ships[wingp->ship_index[wingp->special_ship]];
                oswpt->objp = &Objects[oswpt->shipp->objnum];
            }
            // boo... well, just point to ship at index 0
            else {
                oswpt->shipp = &Ships[wingp->ship_index[0]];
                oswpt->objp = &Objects[oswpt->shipp->objnum];
                WARNINGF (LOCATION, "Substituting ship '%s' at index 0 for nonexistent wing leader at index %d!", oswpt->shipp->ship_name, oswpt->wingp->special_ship);
            }
        }
        // it's still a valid wing even if nobody is here
        else {
            oswpt->type = OSWPT_TYPE_WING_NOT_PRESENT;
            oswpt->wingp = wingp;
        }

        return;
    }

    // check if we have a point for a target
    wpt = find_matching_waypoint (object_name);
    if ((wpt != NULL) && (wpt->get_objnum () >= 0)) {
        oswpt->type = OSWPT_TYPE_WAYPOINT;

        oswpt->waypointp = wpt;
        oswpt->objp = &Objects[wpt->get_objnum ()];

        return;
    }

    // check if we have an "<any team>" type
    team = sexp_determine_team (object_name);
    if (team >= 0) {
        oswpt->type = OSWPT_TYPE_SHIP_ON_TEAM;
        oswpt->team = team;
    }

    // check if we have a whole-team type
    team = iff_lookup (object_name);
    if (team >= 0) {
        oswpt->type = OSWPT_TYPE_WHOLE_TEAM;
        oswpt->team = team;
    }

    // we apparently don't have anything legal
    return;
}

/**
 * Evaluate if given ship is destroyed.
 * @return true if the ship in the expression has been destroyed.
 */
int sexp_is_destroyed (int n, fix* latest_time) {
    char* name;
    int count, num_destroyed, wing_index;
    fix time;

    ASSERT (n != -1);

    count = 0;
    num_destroyed = 0;
    wing_index = -1;
    while (n != -1) {
        count++;
        name = CTEXT (n);

        if (sexp_query_has_yet_to_arrive (name)) return SEXP_CANT_EVAL;

        // check to see if this ship/wing has departed.  If so, then function
        // is known false
        if (mission_log_get_time (LOG_SHIP_DEPARTED, name, NULL, NULL) || mission_log_get_time (LOG_WING_DEPARTED, name, NULL, NULL)) return SEXP_KNOWN_FALSE;

        // check the mission log.  If ship/wing not destroyed, immediately
        // return SEXP_FALSE.
        if (mission_log_get_time (LOG_SHIP_DESTROYED, name, NULL, &time) || mission_log_get_time (LOG_WING_DESTROYED, name, NULL, &time) ||
            mission_log_get_time (LOG_SELF_DESTRUCTED, name, NULL, &time)) {
            num_destroyed++;
            if (latest_time && (time > *latest_time)) *latest_time = time;
        }
        else {
            // If a previous SEXP already had an empty wing then this code
            // would expose the internal value as the directive count. Instead,
            // we reset the count to zero here to make sure that the wing or
            // ship count is correct.
            if (Directive_count == DIRECTIVE_WING_ZERO) {
#ifndef NDEBUG
                static bool wing_zero_warning_shown = false;
                if (!wing_zero_warning_shown) {
                    WARNINGF (
                        LOCATION,
                        "SEXP: is-destroyed-delay was used multiple times in a directive event! This might have unintended effects and should be replaced by a single use of "
                        "is-destroyed-delay.");
                    wing_zero_warning_shown = true;
                }
#endif
                Directive_count = 0;
            }
            // ship or wing isn't destroyed -- add to directive count
            if ((wing_index = wing_name_lookup (name, 1)) >= 0) { Directive_count += Wings[wing_index].current_count; }
            else
                Directive_count++;
        }

        // move to next ship/wing in list
        n = CDR (n);
    }

    // special case to mark a directive for destroy wing objectives true after
    // a short amount of time when there are more waves for this wing.
    if ((count == 1) && (wing_index >= 0) && (Directive_count == 0)) {
        if (Wings[wing_index].current_wave < Wings[wing_index].num_waves) Directive_count = DIRECTIVE_WING_ZERO;
    }

    if (count == num_destroyed)
        return SEXP_KNOWN_TRUE;
    else
        return SEXP_FALSE;
}

/**
 * Return true if the subsystem of the given ship has been destroyed
 */
int sexp_is_subsystem_destroyed (int n) {
    char *ship_name, *subsys_name;

    ASSERT (n != -1);

    ship_name = CTEXT (n);
    subsys_name = CTEXT (CDR (n));

    if (sexp_query_has_yet_to_arrive (ship_name)) return SEXP_CANT_EVAL;

    // if the ship has departed, no way to destroy it's subsystem.
    if (mission_log_get_time (LOG_SHIP_DEPARTED, ship_name, NULL, NULL)) return SEXP_KNOWN_FALSE;

    if (mission_log_get_time (LOG_SHIP_SUBSYS_DESTROYED, ship_name, subsys_name, NULL)) return SEXP_KNOWN_TRUE;

    return SEXP_FALSE;
}

/**
 * Determine if a ship has arrived onto the scene
 */
int sexp_has_arrived (int n, fix* latest_time) {
    char* name;
    int count, num_arrived;
    fix time;

    count = 0;
    num_arrived = 0;
    while (n != -1) {
        count++;
        name = CTEXT (n);
        // if there is no log entry for this ship/wing for arrival, sexpression
        // is false
        if (mission_log_get_time (LOG_SHIP_ARRIVED, name, NULL, &time) || mission_log_get_time (LOG_WING_ARRIVED, name, NULL, &time)) {
            num_arrived++;
            if (latest_time && (time > *latest_time)) *latest_time = time;
        }
        n = CDR (n);
    }

    if (count == num_arrived)
        return SEXP_KNOWN_TRUE;
    else
        return SEXP_FALSE;
}

/**
 * Determine if a ship/wing has departed
 */
int sexp_has_departed (int n, fix* latest_time) {
    char* name;
    int count, num_departed;
    fix time;

    count = 0;
    num_departed = 0;
    while (n != -1) {
        count++;
        name = CTEXT (n);

        if (sexp_query_has_yet_to_arrive (name)) return SEXP_CANT_EVAL;

        // if ship/wing destroyed, sexpression is known false.  Also, if there
        // is no departure log entry, then the sexpression is not true.
        if (mission_log_get_time (LOG_SHIP_DESTROYED, name, NULL, NULL) || mission_log_get_time (LOG_WING_DESTROYED, name, NULL, NULL) ||
            mission_log_get_time (LOG_SELF_DESTRUCTED, name, NULL, NULL))
            return SEXP_KNOWN_FALSE;
        else if (mission_log_get_time (LOG_SHIP_DEPARTED, name, NULL, &time) || mission_log_get_time (LOG_WING_DEPARTED, name, NULL, &time)) {
            num_departed++;
            if (latest_time && (time > *latest_time)) *latest_time = time;
        }
        n = CDR (n);
    }

    if (count == num_departed)
        return SEXP_KNOWN_TRUE;
    else
        return SEXP_FALSE;
}

/**
 * Determine if a ship is disabled
 */
int sexp_is_disabled (int n, fix* latest_time) {
    char* name;
    int count, num_disabled;
    fix time;

    count = 0;
    num_disabled = 0;
    while (n != -1) {
        count++;
        name = CTEXT (n);

        if (sexp_query_has_yet_to_arrive (name)) return SEXP_CANT_EVAL;

        // if ship/wing destroyed, sexpression is known false.  Also, if there
        // is no disable log entry, then the sexpression is not true.
        if (mission_log_get_time (LOG_SHIP_DEPARTED, name, NULL, &time) || mission_log_get_time (LOG_SHIP_DESTROYED, name, NULL, &time) ||
            mission_log_get_time (LOG_SELF_DESTRUCTED, name, NULL, &time))
            return SEXP_KNOWN_FALSE;
        else if (mission_log_get_time (LOG_SHIP_DISABLED, name, NULL, &time)) {
            num_disabled++;
            if (latest_time && (time > *latest_time)) *latest_time = time;
        }
        n = CDR (n);
    }

    if (count == num_disabled)
        return SEXP_KNOWN_TRUE;
    else
        return SEXP_FALSE;
}

/**
 * Determine if a ship is done flying waypoints
 */
int sexp_are_waypoints_done (int n) {
    char *ship_name, *waypoint_name;

    ship_name = CTEXT (n);
    waypoint_name = CTEXT (CDR (n));

    if (sexp_query_has_yet_to_arrive (ship_name)) return SEXP_CANT_EVAL;

    // a destroyed or departed ship will never reach their goal -- return known
    // false
    if (mission_log_get_time (LOG_SHIP_DESTROYED, ship_name, NULL, NULL) || mission_log_get_time (LOG_SELF_DESTRUCTED, ship_name, NULL, NULL))
        return SEXP_KNOWN_FALSE;
    else if (mission_log_get_time (LOG_SHIP_DEPARTED, ship_name, NULL, NULL))
        return SEXP_KNOWN_FALSE;

    // now check the log for the waypoints done entry
    if (mission_log_get_time (LOG_WAYPOINTS_DONE, ship_name, waypoint_name, NULL)) return SEXP_KNOWN_TRUE;

    return SEXP_FALSE;
}

/**
 * Determine if ships are disarmed
 */
int sexp_is_disarmed (int n, fix* latest_time) {
    char* name;
    int count, num_disarmed;
    fix time;

    count = 0;
    num_disarmed = 0;
    while (n != -1) {
        count++;
        name = CTEXT (n);

        if (sexp_query_has_yet_to_arrive (name)) return SEXP_CANT_EVAL;

        // if ship/wing destroyed, sexpression is known false.  Also, if there
        // is no disarm log entry, then the sexpression is not true.
        if (mission_log_get_time (LOG_SHIP_DEPARTED, name, NULL, &time) || mission_log_get_time (LOG_SHIP_DESTROYED, name, NULL, &time) ||
            mission_log_get_time (LOG_SELF_DESTRUCTED, name, NULL, &time))
            return SEXP_KNOWN_FALSE;
        else if (mission_log_get_time (LOG_SHIP_DISARMED, name, NULL, &time)) {
            num_disarmed++;
            if (latest_time && (time > *latest_time)) *latest_time = time;
        }
        n = CDR (n);
    }

    if (count == num_disarmed)
        return SEXP_KNOWN_TRUE;
    else
        return SEXP_FALSE;
}

// the following functions are similar to the above objective functions but
// return true/false if N seconds have elasped after the corresponding function
// is true.
int sexp_is_destroyed_delay (int n) {
    fix delay, time;
    int val;

    ASSERT (n >= 0);

    time = 0;

    delay = i2f (eval_num (n));

    // check value of is_destroyed function.  KNOWN_FALSE should be returned
    // immediately
    val = sexp_is_destroyed (CDR (n), &time);
    if (val == SEXP_KNOWN_FALSE) return val;

    if (val == SEXP_CANT_EVAL) return SEXP_CANT_EVAL;

    if (val) {
        if ((Missiontime - time) >= delay) return SEXP_KNOWN_TRUE;
    }

    return SEXP_FALSE;
}

int sexp_is_subsystem_destroyed_delay (int n) {
    char *ship_name, *subsys_name;
    fix delay, time;

    ASSERT (n != -1);

    ship_name = CTEXT (n);
    subsys_name = CTEXT (CDR (n));
    delay = i2f (eval_num (CDR (CDR (n))));

    if (sexp_query_has_yet_to_arrive (ship_name)) return SEXP_CANT_EVAL;

    // if the ship has departed, no way to destroy it's subsystem.
    if (mission_log_get_time (LOG_SHIP_DEPARTED, ship_name, NULL, NULL)) return SEXP_KNOWN_FALSE;

    if (mission_log_get_time (LOG_SHIP_SUBSYS_DESTROYED, ship_name, subsys_name, &time)) {
        if ((Missiontime - time) >= delay) return SEXP_KNOWN_TRUE;
    }

    return SEXP_FALSE;
}

int sexp_is_disabled_delay (int n) {
    fix delay, time;
    int val;

    ASSERT (n >= 0);

    time = 0;
    delay = i2f (eval_num (n));

    // check value of is_disable for known false and return immediately if it
    // is.
    val = sexp_is_disabled (CDR (n), &time);
    if (val == SEXP_KNOWN_FALSE) return val;

    if (val == SEXP_CANT_EVAL) return SEXP_CANT_EVAL;

    if (val) {
        if ((Missiontime - time) >= delay) return SEXP_KNOWN_TRUE;
    }

    return SEXP_FALSE;
}

int sexp_is_disarmed_delay (int n) {
    fix delay, time;
    int val;

    ASSERT (n >= 0);

    time = 0;
    delay = i2f (eval_num (n));

    // check value of is_disarmed for a known false value and return that
    // immediately if it is
    val = sexp_is_disarmed (CDR (n), &time);
    if (val == SEXP_KNOWN_FALSE) return val;

    if (val == SEXP_CANT_EVAL) return SEXP_CANT_EVAL;

    if (val) {
        if ((Missiontime - time) >= delay) return SEXP_KNOWN_TRUE;
    }

    return SEXP_FALSE;
}

int sexp_has_docked_or_undocked (int n, int op_num) {
    ASSERT (op_num == OP_HAS_DOCKED || op_num == OP_HAS_UNDOCKED || op_num == OP_HAS_DOCKED_DELAY || op_num == OP_HAS_UNDOCKED_DELAY);

    char* docker = CTEXT (n);
    char* dockee = CTEXT (CDR (n));
    int count = eval_num (CDR (CDR (n))); // count of times that we should look for

    if (count <= 0) {
        WARNINGF (
            LOCATION, "has-%sdocked%s count should be at least 1, adjusted.", (op_num == OP_HAS_UNDOCKED || op_num == OP_HAS_UNDOCKED_DELAY ? "un" : ""),
            (op_num == OP_HAS_DOCKED_DELAY || op_num == OP_HAS_UNDOCKED_DELAY ? "-delay" : ""));
        count = 1;
    }

    if (sexp_query_has_yet_to_arrive (docker)) return SEXP_CANT_EVAL;

    if (sexp_query_has_yet_to_arrive (dockee)) return SEXP_CANT_EVAL;

    if (op_num == OP_HAS_DOCKED_DELAY || op_num == OP_HAS_UNDOCKED_DELAY) {
        fix delay = i2f (eval_num (CDR (CDR (CDR (n)))));
        fix time;

        if (mission_log_get_time_indexed (op_num == OP_HAS_DOCKED_DELAY ? LOG_SHIP_DOCKED : LOG_SHIP_UNDOCKED, docker, dockee, count, &time)) {
            if ((Missiontime - time) >= delay) return SEXP_KNOWN_TRUE;
        }
    }
    else {
        if (mission_log_get_time_indexed (op_num == OP_HAS_DOCKED ? LOG_SHIP_DOCKED : LOG_SHIP_UNDOCKED, docker, dockee, count, NULL)) return SEXP_KNOWN_TRUE;
    }

    if (mission_log_get_time (LOG_SHIP_DESTROYED, docker, NULL, NULL) || mission_log_get_time (LOG_SHIP_DESTROYED, dockee, NULL, NULL)) return SEXP_KNOWN_FALSE;

    if (mission_log_get_time (LOG_SELF_DESTRUCTED, docker, NULL, NULL) || mission_log_get_time (LOG_SELF_DESTRUCTED, dockee, NULL, NULL)) return SEXP_KNOWN_FALSE;

    return SEXP_FALSE;
}

int sexp_has_arrived_delay (int n) {
    fix delay, time;
    int val;

    ASSERT (n >= 0);

    time = 0;
    delay = i2f (eval_num (n));

    // check return value from arrived function.  if can never arrive, then
    // return that value here as well
    val = sexp_has_arrived (CDR (n), &time);
    if (val == SEXP_KNOWN_FALSE) return val;

    if (val == SEXP_CANT_EVAL) return SEXP_CANT_EVAL;

    if (val) {
        if ((Missiontime - time) >= delay) return SEXP_KNOWN_TRUE;
    }

    return SEXP_FALSE;
}

int sexp_has_departed_delay (int n) {
    fix delay, time;
    int val;

    ASSERT (n >= 0);

    time = 0;
    delay = i2f (eval_num (n));

    // must first check to see if the departed function could ever be
    // true/false or is true or false. if it can never be true, return that
    // value
    val = sexp_has_departed (CDR (n), &time);
    if (val == SEXP_KNOWN_FALSE) return val;

    if (val == SEXP_CANT_EVAL) return SEXP_CANT_EVAL;

    if (val) {
        if ((Missiontime - time) >= delay) return SEXP_KNOWN_TRUE;
    }

    return SEXP_FALSE;
}

/**
 * Determine if a ship is done flying waypoints after N seconds
 */
int sexp_are_waypoints_done_delay (int node) {
    char *ship_name, *waypoint_name;
    int count, n = node;
    fix time, delay;

    ship_name = CTEXT (n);
    n = CDR (n);
    waypoint_name = CTEXT (n);
    n = CDR (n);
    delay = i2f (eval_num (n));
    n = CDR (n);
    count = (n >= 0) ? eval_num (n) : 1;
    if (count <= 0) {
        WARNINGF (LOCATION, "Are-waypoints-done-delay count should be at least 1!  This has been automatically adjusted.");
        count = 1;
    }

    if (sexp_query_has_yet_to_arrive (ship_name)) return SEXP_CANT_EVAL;

    // a destroyed or departed ship will never reach their goal -- return known
    // false
    //
    // Not checking the entries below.  Ships which warp out after reaching
    // their goal (or getting destroyed after their goal), but after reaching
    // their waypoints, may have this goal incorrectly marked false!!!!

    // now check the log for the waypoints done entry
    if (mission_log_get_time_indexed (LOG_WAYPOINTS_DONE, ship_name, waypoint_name, count, &time)) {
        if ((Missiontime - time) >= delay) return SEXP_KNOWN_TRUE;
    }
    else {
        if (mission_log_get_time (LOG_SHIP_DESTROYED, ship_name, NULL, NULL) || mission_log_get_time (LOG_SELF_DESTRUCTED, ship_name, NULL, NULL))
            return SEXP_KNOWN_FALSE;
        else if (mission_log_get_time (LOG_SHIP_DEPARTED, ship_name, NULL, NULL))
            return SEXP_KNOWN_FALSE;
    }

    return SEXP_FALSE;
}

/**
 * Determine is all of a given ship type are destroyed
 */
int sexp_ship_type_destroyed (int n) {
    int percent;
    int type;
    char* shiptype;

    percent = eval_num (n);
    shiptype = CTEXT (CDR (n));

    type = ship_type_name_lookup (shiptype);

    // bogus if we reach the end of this array!!!!
    if (type < 0) {
        WARNINGF (LOCATION, "Invalid shiptype passed to ship-type-destroyed");
        return SEXP_FALSE;
    }

    if (type >= (int)Ship_type_counts.size () || Ship_type_counts[type].total == 0) return SEXP_FALSE;

    // We are safe from array indexing probs b/c of previous if.
    // determine if the percentage of killed/total is >= percentage given in
    // the expression
    if ((Ship_type_counts[type].killed * 100 / Ship_type_counts[type].total) >= percent) return SEXP_KNOWN_TRUE;

    return SEXP_FALSE;
}

// following are time based functions
int sexp_has_time_elapsed (int n) {
    int time = eval_num (n);

    if (f2i (Missiontime) >= time) return SEXP_KNOWN_TRUE;

    return SEXP_FALSE;
}

/**
 * Returns the time into the mission
 */
int sexp_mission_time () { return f2i (Missiontime); }

/**
 * Returns percent of length of distance to special warpout plane
 */
int sexp_special_warp_dist (int n) {
    char* ship_name;
    int shipnum;

    // get shipname
    ship_name = CTEXT (n);

    // check to see if either ship was destroyed or departed.  If so, then make
    // this node known false
    if (mission_log_get_time (LOG_SHIP_DESTROYED, ship_name, NULL, NULL) || mission_log_get_time (LOG_SHIP_DEPARTED, ship_name, NULL, NULL) ||
        mission_log_get_time (LOG_SELF_DESTRUCTED, ship_name, NULL, NULL)) {
        return SEXP_NAN_FOREVER;
    }

    // get ship name
    shipnum = ship_name_lookup (ship_name);
    if (shipnum < 0) { return SEXP_NAN; }

    // check that ship has warpout_objnum
    if (Ships[shipnum].special_warpout_objnum < 0) { return SEXP_NAN; }

    ASSERT ((Ships[shipnum].special_warpout_objnum >= 0) && (Ships[shipnum].special_warpout_objnum < MAX_OBJECTS));
    if ((Ships[shipnum].special_warpout_objnum < 0) || (Ships[shipnum].special_warpout_objnum >= MAX_OBJECTS)) { return SEXP_NAN; }

    // check the special warpout device is valid
    int valid = FALSE;
    object* ship_objp = &Objects[Ships[shipnum].objnum];
    object* warp_objp = &Objects[Ships[shipnum].special_warpout_objnum];
    if (warp_objp->type == OBJ_SHIP) {
        if (Ship_info[Ships[warp_objp->instance].ship_info_index].flags[Ship::Info_Flags::Knossos_device]) { valid = TRUE; }
    }

    if (!valid) { return SEXP_NAN; }

    // check if within 45 degree half-angle cone of facing
    float dot = fabsf (vm_vec_dot (&warp_objp->orient.vec.fvec, &ship_objp->orient.vec.fvec));
    if (dot < 0.707f) { return SEXP_NAN; }

    // get distance
    vec3d hit_pt;
    float dist = fvi_ray_plane (&hit_pt, &warp_objp->pos, &warp_objp->orient.vec.fvec, &ship_objp->pos, &ship_objp->orient.vec.fvec, 0.0f);
    polymodel* pm = model_get (Ship_info[Ships[shipnum].ship_info_index].model_num);
    dist += pm->mins.xyz.z;

    // return as a percent of length
    return (int)(100.0f * dist / ship_class_get_length (&Ship_info[Ships[shipnum].ship_info_index]));
}

int sexp_time_destroyed (int n) {
    fix time;

    if (!mission_log_get_time (LOG_SHIP_DESTROYED, CTEXT (n), NULL, &time) && !mission_log_get_time (LOG_SELF_DESTRUCTED, CTEXT (n), NULL,
                                                                                                     &time)) { // returns 0 when not found
        return SEXP_NAN;
    }

    return f2i (time);
}

int sexp_time_wing_destroyed (int n) {
    fix time;

    if (!mission_log_get_time (LOG_WING_DESTROYED, CTEXT (n), NULL, &time)) { return SEXP_NAN; }

    return f2i (time);
}

int sexp_time_docked_or_undocked (int n, bool docked) {
    fix time;
    char* docker = CTEXT (n);
    char* dockee = CTEXT (CDR (n));
    int count = eval_num (CDR (CDR (n)));

    if (count <= 0) {
        WARNINGF (LOCATION, "time-%sdocked count should be at least 1, adjusted.", docked ? "" : "un");
        count = 1;
    }

    if (!mission_log_get_time_indexed (docked ? LOG_SHIP_DOCKED : LOG_SHIP_UNDOCKED, docker, dockee, count, &time)) { return SEXP_NAN; }

    return f2i (time);
}

int sexp_time_ship_arrived (int n) {
    fix time;

    ASSERT (n != -1);
    if (!mission_log_get_time (LOG_SHIP_ARRIVED, CTEXT (n), NULL, &time)) { return SEXP_NAN; }

    return f2i (time);
}

int sexp_time_wing_arrived (int n) {
    fix time;

    ASSERT (n != -1);
    if (!mission_log_get_time (LOG_WING_ARRIVED, CTEXT (n), NULL, &time)) { return SEXP_NAN; }

    return f2i (time);
}

int sexp_time_ship_departed (int n) {
    fix time;

    ASSERT (n != -1);
    if (!mission_log_get_time (LOG_SHIP_DEPARTED, CTEXT (n), NULL, &time)) { return SEXP_NAN; }

    return f2i (time);
}

int sexp_time_wing_departed (int n) {
    fix time;

    ASSERT (n != -1);
    if (!mission_log_get_time (LOG_WING_DEPARTED, CTEXT (n), NULL, &time)) { return SEXP_NAN; }

    return f2i (time);
}

/**
 * Return the remaining shields as a percentage of the given ship.
 */
int sexp_shields_left (int n) {
    int shipnum, percent;
    char* shipname;

    shipname = CTEXT (n);

    // if ship is gone or departed, cannot ever evaluate properly.  Return
    // NAN_FOREVER
    if (mission_log_get_time (LOG_SHIP_DESTROYED, shipname, NULL, NULL) || mission_log_get_time (LOG_SHIP_DEPARTED, shipname, NULL, NULL) ||
        mission_log_get_time (LOG_SELF_DESTRUCTED, shipname, NULL, NULL)) {
        return SEXP_NAN_FOREVER;
    }

    shipnum = ship_name_lookup (shipname);
    if (shipnum == -1) { // hmm.. if true, must not have arrived yet
        return SEXP_NAN;
    }

    // Goober5000: in case ship has no shields
    if (Ships[shipnum].ship_max_shield_strength == 0.0f) { return 0; }

    // now return the amount of shields left as a percentage of the whole.
    percent = (int)std::lround (get_shield_pct (&Objects[Ships[shipnum].objnum]) * 100.0f);
    return percent;
}

/**
 * Return the remaining hits left as a percentage of the whole.
 *
 * This hit amount counts for all hits on the ship (hull + subsystems).  Use
 * hits_left_hull to find hull hits remaining.
 */
int sexp_hits_left (int n) {
    int shipnum, percent;
    char* shipname;

    shipname = CTEXT (n);

    // if ship is gone or departed, cannot ever evaluate properly.  Return
    // NAN_FOREVER
    if (mission_log_get_time (LOG_SHIP_DESTROYED, shipname, NULL, NULL) || mission_log_get_time (LOG_SHIP_DEPARTED, shipname, NULL, NULL) ||
        mission_log_get_time (LOG_SELF_DESTRUCTED, shipname, NULL, NULL)) {
        return SEXP_NAN_FOREVER;
    }

    shipnum = ship_name_lookup (shipname);
    if (shipnum == -1) { // hmm.. if true, must not have arrived yet
        return SEXP_NAN;
    }

    // now return the amount of hits left as a percentage of the whole.
    // Subtract the percentage from 100 since we are working with total hit
    // points taken, not total remaining.
    ship* shipp = &Ships[shipnum];
    object* objp = &Objects[shipp->objnum];
    percent = (int)std::lround (100.0f * get_hull_pct (objp));
    return percent;
}


/**
 * Determine if ship visible on radar
 *
 * @return 0 - not visible
 * @return 1 - marginally targetable (jiggly on radar)
 * @return 2 - fully targetable
 */
int sexp_is_ship_visible (int n) {
    char* shipname;
    int shipnum;
    int ship_is_visible = 0;

    shipname = CTEXT (n);

    // if ship is gone or departed, cannot ever evaluate properly.  Return
    // NAN_FOREVER
    if (mission_log_get_time (LOG_SHIP_DESTROYED, shipname, NULL, NULL) || mission_log_get_time (LOG_SHIP_DEPARTED, shipname, NULL, NULL) ||
        mission_log_get_time (LOG_SELF_DESTRUCTED, shipname, NULL, NULL)) {
        return SEXP_NAN_FOREVER;
    }

    shipnum = ship_name_lookup (shipname);
    if (shipnum == -1) { // hmm.. if true, must not have arrived yet
        return SEXP_NAN;
    }

    // get ship's *radar* visiblity
    if (Player_ship != NULL) {
        if (ship_is_visible_by_team (&Objects[Ships[shipnum].objnum], Player_ship)) { ship_is_visible = 2; }
    }

    // only check awacs level if ship is not visible by team
    if (Player_ship != NULL && !ship_is_visible) {
        float awacs_level = awacs_get_level (&Objects[Ships[shipnum].objnum], Player_ship);
        if (awacs_level >= 1.0f) { ship_is_visible = 2; }
        else if (awacs_level > 0) { ship_is_visible = 1; }
    }

    return ship_is_visible;
}

/**
 * Return the remaining hits left on a subsystem as a percentage of the whole.
 *
 * Goober5000 - this sexp is DEPRECATED because it works just like the new
 * hits-left-substem-generic
 */
int sexp_hits_left_subsystem (int n) {
    bool single_subsystem = false;
    int shipnum, percent, type;
    char* shipname;
    char* subsys_name;

    shipname = CTEXT (n);

    // if ship is gone or departed, cannot ever evaluate properly.  Return
    // NAN_FOREVER
    if (mission_log_get_time (LOG_SHIP_DESTROYED, shipname, NULL, NULL) || mission_log_get_time (LOG_SHIP_DEPARTED, shipname, NULL, NULL) ||
        mission_log_get_time (LOG_SELF_DESTRUCTED, shipname, NULL, NULL)) {
        return SEXP_NAN_FOREVER;
    }

    shipnum = ship_name_lookup (shipname);
    if (shipnum == -1) { // hmm.. if true, must not have arrived yet
        return SEXP_NAN;
    }

    subsys_name = CTEXT (CDR (n));
    ship_subsys* ss = ship_get_subsys (&Ships[shipnum], subsys_name);
    if (ss != NULL)
        type = ss->system_info->type;
    else
        type = SUBSYSTEM_NONE;

    if ((type >= 0) && (type < SUBSYSTEM_MAX)) {
        // check for the optional argument
        n = CDDR (n);
        if (n >= 0) { single_subsystem = is_sexp_true (n); }

        // if the third option is present or if this is an unknown subsystem
        // type we only want to find the percentage of the named subsystem
        if (single_subsystem || (type == SUBSYSTEM_UNKNOWN)) {
            if (ss != NULL) {
                percent = (int)std::lround (ss->current_hits / ss->max_hits * 100.0f);
                return percent;
            }

            // we reached end of ship subsys list without finding subsys_name
            if (ship_class_unchanged (shipnum)) { ASSERTX (0, "Invalid subsystem '%s' passed to hits-left-subsystem", subsys_name); }
            return SEXP_NAN;

            // by default we return as a percentage the hits remaining on the
            // subsystem as a whole (i.e. for 3 engines, we are returning the
            // sum of the hits on the 3 engines)
        }
        else {
            percent = (int)std::lround (ship_get_subsystem_strength (&Ships[shipnum], type) * 100.0f);
            return percent;
        }
    }
    return SEXP_NAN; // if for some strange reason, the type field of the
                     // subsystem is bogus
}

int sexp_determine_team (char* subj) {
    char team_name[NAME_LENGTH];

    // quick check
    if (strncasecmp (subj, "<any ", 5) != 0) return -1;

    // grab IFF (rest of string except for closing angle bracket)
    // TODO: should be strings
    auto len = strlen (subj + 5) - 1;
    strncpy (team_name, subj + 5, len);
    team_name[len] = '\0';

    // find it
    return iff_lookup (team_name);
}

/**
 * Check distance between two given objects
 */
int sexp_distance3 (object* objp1, object* objp2) {
    // if either object isn't present in the mission now
    if (objp1 == NULL || objp2 == NULL) return SEXP_NAN;

    if ((objp1->type == OBJ_SHIP) && (objp2->type == OBJ_SHIP)) {
        if (Player_obj == objp1)
            return (int)hud_find_target_distance (objp2, objp1);
        else
            return (int)hud_find_target_distance (objp1, objp2);
    }
    else { return (int)vm_vec_dist_quick (&objp1->pos, &objp2->pos); }
}

/**
 * Check distance between a given ship and a given subject (ship, wing, any
 * \<team\>).
 */
int sexp_distance2 (object* objp1, object_ship_wing_point_team* oswpt2) {
    int dist, dist_min = 0, inited = 0;

    switch (oswpt2->type) {
    // we have a ship-on-team type, so check all ships of that type
    case OSWPT_TYPE_SHIP_ON_TEAM: {
        for (ship_obj* so = GET_FIRST (&Ship_obj_list); so != END_OF_LIST (&Ship_obj_list); so = GET_NEXT (so)) {
            if (Ships[Objects[so->objnum].instance].team == oswpt2->team) {
                dist = sexp_distance3 (objp1, &Objects[so->objnum]);
                if (dist != SEXP_NAN) {
                    if (!inited || (dist < dist_min)) {
                        dist_min = dist;
                        inited = 1;
                    }
                }
            }
        }

        // no objects were checked
        if (!inited) return SEXP_NAN;

        return dist_min;
    }

    // check ships and points
    case OSWPT_TYPE_SHIP:
    case OSWPT_TYPE_WAYPOINT: {
        return sexp_distance3 (objp1, oswpt2->objp);
    }

    // check wings
    case OSWPT_TYPE_WING: {
        for (int i = 0; i < oswpt2->wingp->current_count; i++) {
            dist = sexp_distance3 (objp1, &Objects[Ships[oswpt2->wingp->ship_index[i]].objnum]);
            if (dist != SEXP_NAN) {
                if (!inited || (dist < dist_min)) {
                    dist_min = dist;
                    inited = 1;
                }
            }
        }

        // no objects were checked
        if (!inited) return SEXP_NAN;

        return dist_min;
    }
    }

    return SEXP_NAN;
}

/**
 * Returns the distance between two objects.
 *
 * If a wing is specified as one (or both) of the arguments to this function,
 * we are looking for the closest distance
 */
int sexp_distance (int n) {
    int dist, dist_min = 0, inited = 0;
    object_ship_wing_point_team oswpt1, oswpt2;

    sexp_get_object_ship_wing_point_team (&oswpt1, CTEXT (n));
    sexp_get_object_ship_wing_point_team (&oswpt2, CTEXT (CDR (n)));

    // check to see if either object was destroyed or departed
    if (oswpt1.type == OSWPT_TYPE_EXITED || oswpt2.type == OSWPT_TYPE_EXITED) return SEXP_NAN_FOREVER;

    switch (oswpt1.type) {
    // we have a ship-on-team type, so check all ships of that type
    case OSWPT_TYPE_SHIP_ON_TEAM: {
        for (ship_obj* so = GET_FIRST (&Ship_obj_list); so != END_OF_LIST (&Ship_obj_list); so = GET_NEXT (so)) {
            if (Ships[Objects[so->objnum].instance].team == oswpt1.team) {
                dist = sexp_distance2 (&Objects[so->objnum], &oswpt2);
                if (dist != SEXP_NAN) {
                    if (!inited || (dist < dist_min)) {
                        dist_min = dist;
                        inited = 1;
                    }
                }
            }
        }

        // no objects were checked
        if (!inited) return SEXP_NAN;

        return dist_min;
    }

    // check ships and points
    case OSWPT_TYPE_SHIP:
    case OSWPT_TYPE_WAYPOINT: {
        return sexp_distance2 (oswpt1.objp, &oswpt2);
    }

    // check wings
    case OSWPT_TYPE_WING: {
        for (int i = 0; i < oswpt1.wingp->current_count; i++) {
            dist = sexp_distance2 (&Objects[Ships[oswpt1.wingp->ship_index[i]].objnum], &oswpt2);
            if (dist != SEXP_NAN) {
                if (!inited || (dist < dist_min)) {
                    dist_min = dist;
                    inited = 1;
                }
            }
        }

        // no objects were checked
        if (!inited) return SEXP_NAN;

        return dist_min;
    }
    }

    return SEXP_NAN;
}

/**
 * Locate the subsystem on a ship - Goober5000
 *
 * Switched to a boolean so that it can report failure to do so
 */
bool sexp_get_subsystem_world_pos (vec3d* subsys_world_pos, int shipnum, char* subsys_name) {
    ASSERT (subsys_name);
    ASSERT (subsys_world_pos);

    if (shipnum < 0) { ASSERTX (0, "Error - nonexistent ship.\n"); }

    // find the ship subsystem
    ship_subsys* ss = ship_get_subsys (&Ships[shipnum], subsys_name);
    if (ss != NULL) {
        // find world position of subsystem on this object (the ship)
        get_subsystem_world_pos (&Objects[Ships[shipnum].objnum], ss, subsys_world_pos);
        return true;
    }

    // we reached end of ship subsys list without finding subsys_name
    if (ship_class_unchanged (shipnum)) {
        // this ship should have had the subsystem named as it shouldn't have
        // changed class
        ASSERTX (0, "sexp_get_subsystem_world_pos could not find subsystem '%s'", subsys_name);
    }
    return false;
}

/**
 * Returns the distance between an object and a ship subsystem.
 *
 * If a wing is specified as the object argument to this function, we are
 * looking for the closest distance
 */

void sexp_set_object_speed (object* objp, int speed, int axis, bool subjective) {
    ASSERT (axis >= 0 && axis <= 2);

    if (subjective) {
        vec3d subjective_vel;

        // translate objective into subjective velocity
        vm_vec_rotate (&subjective_vel, &objp->phys_info.vel, &objp->orient);

        // set it
        subjective_vel.a1d[axis] = float (speed);

        // translate it back to objective
        vm_vec_unrotate (&objp->phys_info.vel, &subjective_vel, &objp->orient);
    }
    else { objp->phys_info.vel.a1d[axis] = float (speed); }
}

// Goober5000
void sexp_set_object_speed (int n, int axis) {
    ASSERT (n >= 0);

    int speed;
    bool subjective = false;
    object_ship_wing_point_team oswpt;

    sexp_get_object_ship_wing_point_team (&oswpt, CTEXT (n));
    n = CDR (n);

    speed = eval_num (n);
    n = CDR (n);

    if (n >= 0) {
        subjective = is_sexp_true (n);
        n = CDR (n);
    }

    switch (oswpt.type) {
    case OSWPT_TYPE_SHIP:
    case OSWPT_TYPE_WING: {
        sexp_set_object_speed (oswpt.objp, speed, axis, subjective);
        break;
    }
    }
}

int sexp_get_object_speed (object* objp, int axis, bool subjective) {
    ASSERTX (((axis >= 0) && (axis <= 2)), "Axis is out of range (%d)", axis);
    int speed;

    if (subjective) {
        // return the speed based on the orentation of the object
        vec3d subjective_vel;
        vm_vec_rotate (&subjective_vel, &objp->phys_info.vel, &objp->orient);
        speed = int (subjective_vel.a1d[axis]);
        vm_vec_unrotate (&objp->phys_info.vel, &subjective_vel, &objp->orient);
    }
    else {
        // return the speed according to the grid
        speed = int (objp->phys_info.vel.a1d[axis]);
    }
    return speed;
}

int sexp_get_object_speed (int n, int axis) {
    ASSERT (n >= 0);

    int speed;
    bool subjective = false;
    object_ship_wing_point_team oswpt;

    sexp_get_object_ship_wing_point_team (&oswpt, CTEXT (n));
    n = CDR (n);

    if (n >= 0) {
        subjective = is_sexp_true (n);
        n = CDR (n);
    }

    switch (oswpt.type) {
    case OSWPT_TYPE_EXITED: return SEXP_NAN_FOREVER;

    case OSWPT_TYPE_SHIP:
    case OSWPT_TYPE_WING: speed = sexp_get_object_speed (oswpt.objp, axis, subjective); break;

    default: return SEXP_NAN;
    }
    return speed;
}

/**
 * Determine when the last meaningful order was given to one or more ships.
 *
 * @return true or false depending on whether or not a meaningful order was
 * received
 */
int sexp_last_order_time (int n) {
    int instance, i;
    fix time;
    char* name;
    ai_goal* aigp;

    time = i2f (eval_num (n));
    ASSERT (time >= 0);

    n = CDR (n);
    while (n != -1) {
        name = CTEXT (n);
        instance = ship_name_lookup (name);
        if (instance != -1) { aigp = Ai_info[Ships[instance].ai_index].goals; }
        else {
            instance = wing_name_lookup (name);
            if (instance == -1) // if we cannot find ship or wing, return SEXP_FALSE
                return SEXP_FALSE;
            aigp = Wings[instance].ai_goals;
        }

        // with the ship, check the ai_goals structure for this ship and
        // determine if there are any orders which are < time seconds since
        // current mission time
        for (i = 0; i < MAX_AI_GOALS; i++) {
            int mode;

            mode = aigp->ai_mode;
            if ((mode != AI_GOAL_NONE) && (mode != AI_GOAL_WARP))
                if ((aigp->time + time) > Missiontime) break;
            aigp++;
        }
        if (i == MAX_AI_GOALS) return SEXP_TRUE;

        n = CDR (n);
    }

    return SEXP_FALSE;
}

/**
 * Return the number of players in the mission
 */
int sexp_num_players () {
    int count;
    object* objp;

    count = 0;
    for (objp = GET_FIRST (&obj_used_list); objp != END_OF_LIST (&obj_used_list); objp = GET_NEXT (objp)) {
        if ((objp->type == OBJ_SHIP) && (objp->flags[Object::Object_Flags::Player_ship])) count++;
    }

    return count;
}

/**
 * Determine if the current skill level of the game is at least the skill level
 * given in the SEXP
 */
int sexp_skill_level_at_least (int n) {
    int i;
    char* level_name;

    level_name = CTEXT (n);

    if (level_name == NULL) return SEXP_FALSE;

    for (i = 0; i < NUM_SKILL_LEVELS; i++) {
        if (!strcasecmp (level_name, Skill_level_names (i, 0))) {
            if (Game_skill_level >= i) { return SEXP_TRUE; }
            else { return SEXP_FALSE; }
        }
    }

    // return SEXP_FALSE if not found!!!
    return SEXP_FALSE;
}

int sexp_was_promotion_granted (int /*n*/) {
    if (Player->flags & PLAYER_FLAGS_PROMOTED) return SEXP_TRUE;

    return SEXP_FALSE;
}

int sexp_was_medal_granted (int n) {
    int i;
    char* medal_name;

    if (n < 0) {
        if (Player->stats.m_medal_earned >= 0) return SEXP_TRUE;

        return SEXP_FALSE;
    }

    medal_name = CTEXT (n);

    if (medal_name == NULL) return SEXP_FALSE;

    for (i = 0; i < Num_medals; i++) {
        if (!strcasecmp (medal_name, Medals[i].name)) break;
    }

    if ((i < Num_medals) && (Player->stats.m_medal_earned == i)) return SEXP_TRUE;

    return SEXP_FALSE;
}

/**
 * @todo Add code to check the damage ships which have exited have taken
 */
float get_damage_caused (int damaged_ship, int attacker) {
    int sindex, idx;
    float damage_total = 0.0f;

    // is the ship that took damage in the mission still?
    sindex = ship_get_by_signature (damaged_ship);

    if (sindex >= 0) {
        for (idx = 0; idx < MAX_DAMAGE_SLOTS; idx++) {
            if (Ships[sindex].damage_ship_id[idx] == attacker) {
                damage_total += Ships[sindex].damage_ship[idx];
                break;
            }
        }
    }
    else {
        // TO DO - Add code to check the damage ships which have exited have
        // taken

        sindex = ship_find_exited_ship_by_signature (damaged_ship);
        for (idx = 0; idx < MAX_DAMAGE_SLOTS; idx++) {
            if (Ships_exited[sindex].damage_ship_id[idx] == attacker) {
                damage_total += Ships_exited[sindex].damage_ship[idx];
                break;
            }
        }
    }
    return damage_total;
}

// Karajorma
int sexp_get_damage_caused (int node) {
    int sindex, damaged_sig, attacker_sig;
    float damage_caused = 0.0f;
    char* name;
    int ship_class;

    name = CTEXT (node);
    sindex = ship_name_lookup (name);
    if (sindex < 0) {
        // this ship may have exited already.
        sindex = ship_find_exited_ship_by_name (CTEXT (node));
        if (sindex < 0) {
            // this is probably a ship which hasn't arrived and thus can't have
            // taken any damage yet
            return int (damage_caused);
        }
        else {
            damaged_sig = Ships_exited[sindex].obj_signature;
            ship_class = Ships_exited[sindex].ship_class;
        }
    }
    else {
        damaged_sig = Objects[Ships[sindex].objnum].signature;
        ship_class = Ships[sindex].ship_info_index;
    }

    node = CDR (node);
    ASSERT (node != -1);

    // go through the list of ships who we think may have attacked the ship
    for (; node != -1; node = CDR (node)) {
        name = CTEXT (node);
        sindex = ship_name_lookup (name);
        if (sindex < 0) {
            sindex = ship_find_exited_ship_by_name (name);
            if (sindex < 0) { continue; }
            else { attacker_sig = Ships_exited[sindex].obj_signature; }
        }
        else { attacker_sig = Objects[Ships[sindex].objnum].signature; }

        if (attacker_sig < 0) { continue; }

        damage_caused += get_damage_caused (damaged_sig, attacker_sig);
    }

    ASSERTX (
        (ship_class > -1) && (ship_class < static_cast< int > (Ship_info.size ())),
        "Invalid ship class '%d' passed to sexp_get_damage_caused() (should "
        "be >= 0 and < %d); get a coder!\n",
        ship_class, static_cast< int > (Ship_info.size ()));
    return (int)((damage_caused / Ship_info[ship_class].max_hull_strength) * 100.0f);
}

/**
 * Returns true if the percentage of ships (and ships in wings) departed is at
 * least the percentage given.
 *
 * what determine if we should check destroyed or departed status
 * Goober5000 - added disarm and disable
 */
int sexp_percent_ships_arrive_depart_destroy_disarm_disable (int n, int what) {
    int percent;
    int total, count;
    char* name;

    percent = eval_num (n);

    total = 0;
    count = 0;
    // iterate through the rest of the ships/wings in the list and tally the
    // departures and the total
    for (n = CDR (n); n != -1; n = CDR (n)) {
        int wingnum;

        name = CTEXT (n);

        wingnum = wing_name_lookup (name, 1);
        if (wingnum != -1) {
            // for wings, we can increment the total by the total number of
            // ships that we expect for this wing, and the departures by the
            // number of departures stored for this wing
            total += (Wings[wingnum].wave_count * Wings[wingnum].num_waves);
            if (what == OP_PERCENT_SHIPS_DEPARTED)
                count += Wings[wingnum].total_departed;
            else if (what == OP_PERCENT_SHIPS_DESTROYED)
                count += Wings[wingnum].total_destroyed;
            else
                ASSERTX (0, "Invalid status check '%d' for wing '%s' in sexp_percent_ships_arrive_depart_destroy_disarm_disable", what, name);
        }
        else {
            // must be a ship, so increment the total by 1, then determine if
            // this ship has departed
            total++;
            if (what == OP_PERCENT_SHIPS_DEPARTED) {
                if (mission_log_get_time (LOG_SHIP_DEPARTED, name, NULL, NULL)) count++;
            }
            else if (what == OP_PERCENT_SHIPS_DESTROYED) {
                if (mission_log_get_time (LOG_SHIP_DESTROYED, name, NULL, NULL) || mission_log_get_time (LOG_SELF_DESTRUCTED, name, NULL, NULL)) count++;
            }
            else
                ASSERTX (0, "Invalid status check '%d' for ship '%s' in sexp_percent_ships_depart_destroy_disarm_disable", what, name);
        }
    }

    // now, look at the percentage
    if (((count * 100) / total) >= percent)
        return SEXP_KNOWN_TRUE;
    else
        return SEXP_FALSE;
}

/**
 * Determine if a list of ships has departed from within a radius of a given
 * jump node.
 * @return true N seconds after the list of ships have departed
 */
int sexp_depart_node_delay (int n) {
    int delay, count, num_departed;
    char *jump_node_name, *name;
    fix latest_time, this_time;

    ASSERT (n >= 0);

    delay = eval_num (n);
    n = CDR (n);
    jump_node_name = CTEXT (n);

    // iterate through the list of ships
    n = CDR (n);
    latest_time = 0;
    count = 0;
    num_departed = 0;
    while (n != -1) {
        count++;
        name = CTEXT (n);

        if (sexp_query_has_yet_to_arrive (name)) return SEXP_CANT_EVAL;

        // if ship/wing destroyed, sexpression is known false.  Also, if there
        // is no departure log entry, then the sexpression is not true.
        if (mission_log_get_time (LOG_SHIP_DESTROYED, name, NULL, NULL) || mission_log_get_time (LOG_SELF_DESTRUCTED, name, NULL, NULL))
            return SEXP_KNOWN_FALSE;
        else if (mission_log_get_time (LOG_SHIP_DEPARTED, name, jump_node_name, &this_time)) {
            num_departed++;
            if (this_time > latest_time) latest_time = this_time;
        }
        n = CDR (n);
    }

    if ((count == num_departed) && ((Missiontime - latest_time) >= delay))
        return SEXP_KNOWN_TRUE;
    else
        return SEXP_FALSE;
}

/**
 * Returns true when the listed ships/wings have all been destroyed or have
 * departed.
 */
int sexp_destroyed_departed_delay (int n) {
    int count, total;
    fix delay, latest_time;
    char* name;

    ASSERT (n >= 0);

    // get the delay
    delay = i2f (eval_num (n));
    n = CDR (n);

    count = 0; // number destroyed or departed
    total = 0; // total number of ships/wings to check
    latest_time = 0;
    while (n != -1) {
        int wingnum;
        fix time_gone = 0;

        total++;
        name = CTEXT (n);

        // for wings, check the WF_GONE flag to see if there are no more ships
        // in this wing to arrive.
        wingnum = wing_name_lookup (name, 1);
        if (wingnum != -1) {
            if (Wings[wingnum].flags[Ship::Wing_Flags::Gone]) {
                // be sure to get the latest time of one of these
                if (Wings[wingnum].time_gone > latest_time) { time_gone = Wings[wingnum].time_gone; }
                count++;
            }
        }
        else if (mission_log_get_time (LOG_SHIP_DEPARTED, name, NULL, &time_gone)) { count++; }
        else if (mission_log_get_time (LOG_SHIP_DESTROYED, name, NULL, &time_gone)) { count++; }
        else if (mission_log_get_time (LOG_SELF_DESTRUCTED, name, NULL, &time_gone)) { count++; }

        // check our latest time
        if (time_gone > latest_time) { latest_time = time_gone; }

        n = CDR (n);
    }

    if ((count == total) && (Missiontime > (latest_time + delay)))
        return SEXP_KNOWN_TRUE;
    else
        return SEXP_FALSE;
}

int sexp_special_warpout_name (int node) {
    int shipnum, knossos_shipnum;
    char *ship_name, *knossos;

    ship_name = CTEXT (node);
    knossos = CTEXT (CDR (node));

    // check to see if either ship was destroyed or departed.  If so, then make
    // this node known false
    if (mission_log_get_time (LOG_SHIP_DESTROYED, ship_name, NULL, NULL) || mission_log_get_time (LOG_SHIP_DEPARTED, ship_name, NULL, NULL) ||
        mission_log_get_time (LOG_SELF_DESTRUCTED, ship_name, NULL, NULL) || mission_log_get_time (LOG_SHIP_DESTROYED, knossos, NULL, NULL) ||
        mission_log_get_time (LOG_SHIP_DEPARTED, knossos, NULL, NULL) || mission_log_get_time (LOG_SELF_DESTRUCTED, knossos, NULL, NULL))
        return SEXP_NAN_FOREVER;

    // get ship name
    shipnum = ship_name_lookup (ship_name);
    if (shipnum < 0) { return SEXP_NAN; }

    // get knossos ship
    knossos_shipnum = ship_name_lookup (knossos);
    if (knossos_shipnum < 0) { return SEXP_NAN; }

    // set special warpout objnum
    Ships[shipnum].special_warpout_objnum = Ships[knossos_shipnum].objnum;
    return SEXP_FALSE;
}

/**
 * Determines if N seconds have elapsed since all discovery of all cargo of
 * given ships
 *
 * Goober5000 - I reworked this function to allow for the set-scanned and
 * set-unscanned sexps to work multiple times in a row and also to fix the
 * potential bug where exited ships are checked against their departure time,
 * not against their cargo known time
 */
int sexp_is_cargo_known (int n, int check_delay) {
    int count, ship_num, num_known, delay;

    char* name;

    ASSERT (n >= 0);

    count = 0;
    num_known = 0;

    // get the delay value (if there is one)
    delay = 0;
    if (check_delay) {
        delay = eval_num (n);
        n = CDR (n);
    }

    while (n != -1) {
        fix time_known;
        int is_known;

        is_known = 0;

        count++;

        // see if we have already checked this entry
        if (Sexp_nodes[n].value == SEXP_KNOWN_TRUE) { num_known++; }
        else {
            name = CTEXT (n);

            // find the index in the ship array (will be -1 if not in mission)
            ship_num = ship_name_lookup (name);

            // see if the ship has already exited the mission (either through
            // departure or destruction)
            int exited_index = ship_find_exited_ship_by_name (name);
            if (exited_index != -1) {
                // if not known, the whole thing is known false
                if (!(Ships_exited[exited_index].flags[Ship::Exit_Flags::Cargo_known])) return SEXP_KNOWN_FALSE;

                // check the delay of when we found out
                time_known = Missiontime - Ships_exited[exited_index].time_cargo_revealed;
                if (f2i (time_known) >= delay) {
                    is_known = 1;

                    // here is the only place in the new sexp that this can be
                    // known true
                    Sexp_nodes[n].value = SEXP_KNOWN_TRUE;
                }
            }
            // ship either in mission or not arrived yet
            else {
                // if ship_name_lookup returns -1, then ship is either exited
                // or yet to arrive, and we've already checked exited
                if (ship_num != -1) {
                    if (Ships[ship_num].flags[Ship::Ship_Flags::Cargo_revealed]) {
                        time_known = Missiontime - Ships[ship_num].time_cargo_revealed;
                        if (f2i (time_known) >= delay) { is_known = 1; }
                    }
                }
            }
        }

        // if cargo is known, mark our variable, but not the sexp, because it
        // may change later
        if (is_known) { num_known++; }

        n = CDR (n);
    }

    Directive_count += count - num_known;
    if (count == num_known)
        return SEXP_TRUE;
    else
        return SEXP_FALSE;
}

void get_cap_subsys_cargo_flags (int shipnum, char* subsys_name, int* known, fix* time_revealed) {
    // find the ship subsystem
    ship_subsys* ss = ship_get_subsys (&Ships[shipnum], subsys_name);
    if (ss != NULL) {
        // set the flags
        *known = (ss->flags[Ship::Subsystem_Flags::Cargo_revealed]);
        *time_revealed = ss->time_subsys_cargo_revealed;
    }
    // if we didn't find the subsystem, the ship hasn't arrived yet
    else {
        *known = -1;
        *time_revealed = 0;
    }
}

// reworked by Goober5000 to allow for set-scanned and set-unscanned to be used
// more than once
int sexp_cap_subsys_cargo_known_delay (int n) {
    int delay, count, num_known, ship_num;
    char *ship_name, *subsys_name;

    num_known = 0;
    count = 0;

    ASSERT (n >= 0);

    // get delay
    delay = eval_num (n);
    n = CDR (n);

    // get ship name
    ship_name = CTEXT (n);
    n = CDR (n);

    // find the index in the ship array
    ship_num = ship_name_lookup (ship_name);

    while (n != -1) {
        fix time_known;
        int is_known;

        is_known = 0;
        count++;

        // see if we have already checked this entry
        if (Sexp_nodes[n].value == SEXP_KNOWN_TRUE) { num_known++; }
        else {
            // get subsys name
            subsys_name = CTEXT (n);

            // see if the ship has already exited the mission (either through
            // departure or destruction)
            if (ship_find_exited_ship_by_name (ship_name) != -1) {
                // check the delay of when we found out...
                // Since there is no way to keep track of subsystem status once
                // a ship has departed or has been destroyed, check the mission
                // log.  This will work in 99.9999999% of all cases; however,
                // if the mission designer repeatedly sets and resets the
                // scanned status of the subsystem, the mission log will only
                // return the first occurrence of the subsystem cargo being
                // revealed (regardless of whether it was first hidden using
                // set-unscanned).  Normally, ships keep track of cargo data in
                // the subsystem struct, but once/ the ship has left the
                // mission, the subsystem linked list is purged, causing the
                // loss of this information.  I judged the significant rework
                // of the subsystem code not worth the rare instance that this
                // sexp may be required to work in this way, especially since
                // this problem only occurs after the ship departs.  If the
                // mission designer really needs this functionality, he or she
                // can achieve the same result with creative combinations of
                // event chaining and is-event-true.
                if (!mission_log_get_time (LOG_CAP_SUBSYS_CARGO_REVEALED, ship_name, subsys_name, &time_known)) {
                    // if not known, the whole thing is known false
                    return SEXP_KNOWN_FALSE;
                }

                if (f2i (Missiontime - time_known) >= delay) {
                    is_known = 1;

                    // here is the only place in the new sexp that this can be
                    // known true
                    Sexp_nodes[n].value = SEXP_KNOWN_TRUE;
                }
            }
            // ship either in mission or not arrived yet
            else {
                // if ship_name_lookup returns -1, then ship is either exited
                // or yet to arrive, and we've already checked exited
                if (ship_num != -1) {
                    int cargo_revealed (0);
                    fix time_revealed (0);

                    // get flags
                    get_cap_subsys_cargo_flags (ship_num, subsys_name, &cargo_revealed, &time_revealed);

                    if (cargo_revealed) {
                        time_known = Missiontime - time_revealed;
                        if (f2i (time_known) >= delay) { is_known = 1; }
                    }
                }
            }
        }

        // if cargo is known, mark our variable, but not the sexp, because it
        // may change later
        if (is_known) { num_known++; }

        n = CDR (n);
    }

    Directive_count += count - num_known;
    if (count == num_known)
        return SEXP_TRUE;
    else
        return SEXP_FALSE;
}

// Goober5000
void sexp_set_scanned_unscanned (int n, int flag) {
    char *ship_name, *subsys_name;
    int shipnum;
    ship_subsys* ss;

    // get ship name
    ship_name = CTEXT (n);

    // check to see the ship was destroyed or departed - if so, do nothing
    if (mission_log_get_time (LOG_SHIP_DESTROYED, ship_name, NULL, NULL) || mission_log_get_time (LOG_SHIP_DEPARTED, ship_name, NULL, NULL) ||
        mission_log_get_time (LOG_SELF_DESTRUCTED, ship_name, NULL, NULL)) {
        return;
    }

    // get ship number
    shipnum = ship_name_lookup (ship_name);

    // if the ship isn't in the mission, do nothing
    if (shipnum == -1) { return; }

    // check for possible next optional argument: subsystem
    n = CDR (n);

    // if no subsystem specified, just do it for the ship and exit
    if (n == -1) {
        if (flag)
            ship_do_cargo_revealed (&Ships[shipnum]);
        else
            ship_do_cargo_hidden (&Ships[shipnum]);

        return;
    }

    // iterate through all subsystems
    while (n != -1) {
        subsys_name = CTEXT (n);

        // find the ship subsystem
        ss = ship_get_subsys (&Ships[shipnum], subsys_name);
        if (ss != NULL) {
            // do it for the subsystem
            if (flag)
                ship_do_cap_subsys_cargo_revealed (&Ships[shipnum], ss);
            else
                ship_do_cap_subsys_cargo_hidden (&Ships[shipnum], ss);
        }

        // if we didn't find the subsystem -- bad
        if (ss == NULL && ship_class_unchanged (shipnum)) { ASSERTX (0, "Couldn't find subsystem '%s' on ship '%s' in sexp_set_scanned_unscanned", subsys_name, ship_name); }

        // but if it did, loop again
        n = CDR (n);
    }
}

int sexp_has_been_tagged_delay (int n) {
    int count, shipnum, num_known, delay;
    char* name;

    ASSERT (n >= 0);

    count = 0;
    num_known = 0;

    // get the delay value
    delay = eval_num (n);

    n = CDR (n);

    while (n != -1) {
        fix time_known;
        int is_known;

        is_known = 0;

        count++;

        // see if we have already checked this entry
        if (Sexp_nodes[n].value == SEXP_KNOWN_TRUE) { num_known++; }
        else {
            int exited_index;

            name = CTEXT (n);

            // see if the ship has already exited the mission (either through
            // departure or destruction).  If so, grab the status of whether
            // the cargo is known from this list
            exited_index = ship_find_exited_ship_by_name (name);
            if (exited_index != -1) {
                if (!(Ships_exited[exited_index].flags[Ship::Exit_Flags::Been_tagged])) return SEXP_KNOWN_FALSE;

                // check the delay of when we found out.  We use the ship died
                // time which isn't entirely accurate but won't cause huge
                // delays.
                time_known = Missiontime - Ships_exited[exited_index].time;
                if (f2i (time_known) >= delay) is_known = 1;
            }
            else {
                // otherwise, ship should still be in the mission.  If
                // ship_name_lookup returns -1, then ship is yet to arrive.
                shipnum = ship_name_lookup (name);
                if (shipnum != -1) {
                    if (Ships[shipnum].time_first_tagged != 0) {
                        time_known = Missiontime - Ships[shipnum].time_first_tagged;
                        if (f2i (time_known) >= delay) is_known = 1;
                    }
                }
            }
        }

        // if cargo is known, mark our variable and this sexpression.
        if (is_known) {
            num_known++;
            Sexp_nodes[n].value = SEXP_KNOWN_TRUE;
        }

        n = CDR (n);
    }

    Directive_count += count - num_known;
    if (count == num_known)
        return SEXP_KNOWN_TRUE;
    else
        return SEXP_FALSE;
}

// Karajorma
void eval_when_for_each_special_argument (int cur_node) {
    arg_item* ptr;

    // loop through all the supplied arguments
    ptr = Sexp_applicable_argument_list.get_next ();
    while (ptr != NULL) {
        // acquire argument to be used
        Sexp_replacement_arguments.push_back (ptr->text);

        Sexp_current_argument_nesting_level++;
        Sexp_applicable_argument_list.add_data (ptr->text);

        // execute sexp... CTEXT will insert the argument as necessary
        eval_sexp (cur_node);

        // clean up any special sexp stuff
        Sexp_applicable_argument_list.clear_nesting_level ();
        Sexp_current_argument_nesting_level--;

        // remove the argument
        Sexp_replacement_arguments.pop_back ();

        // continue along argument list
        ptr = ptr->get_next ();
    }
}

// Goober5000
void do_action_for_each_special_argument (int cur_node) {
    arg_item* ptr;

    // loop through all the supplied arguments
    ptr = Sexp_applicable_argument_list.get_next ();
    while (ptr != NULL) {
        // acquire argument to be used
        Sexp_replacement_arguments.push_back (ptr->text);

        // execute sexp... CTEXT will insert the argument as necessary
        // (since these are all actions, they don't return any meaningful
        // values)
        eval_sexp (cur_node);

        // remove the argument
        Sexp_replacement_arguments.pop_back ();
        // continue along argument list
        ptr = ptr->get_next ();
    }
}

// Goober5000
int special_argument_appears_in_sexp_tree (int node) {
    // empty tree
    if (node < 0) return 0;

    // special argument?
    if (!strcmp (Sexp_nodes[node].text, SEXP_ARGUMENT_STRING))
        return 1;

    return special_argument_appears_in_sexp_tree (CAR (node)) || special_argument_appears_in_sexp_tree (CDR (node));
}

// Goober5000
int special_argument_appears_in_sexp_list (int node) {
    // look through list
    while (node != -1) {
        // special argument?
        if (!strcmp (Sexp_nodes[node].text, SEXP_ARGUMENT_STRING)) return 1;

        node = CDR (node);
    }

    return 0;
}

// Goober5000
void eval_when_do_one_exp (int exp) {
    arg_item* ptr;

    switch (get_operator_const (CTEXT (exp))) {
    // if the op is a conditional then we just evaluate it
    case OP_WHEN:
        // need to account for the possibility this call uses <arguments>
        if (special_argument_appears_in_sexp_tree (exp)) {
            ptr = Sexp_applicable_argument_list.get_next ();
            if (ptr != NULL) { eval_when_for_each_special_argument (exp); }
            else { eval_sexp (exp); }
        }
        else { eval_sexp (exp); }
        break;

    // otherwise we need to check if arguments are used
    default:
        // if we're using the special argument in this action
        if (special_argument_appears_in_sexp_tree (exp)) {
            do_action_for_each_special_argument (exp); // these sexps eval'd only for side effects
        }
        // if not, just evaluate it once as-is
        else {
            // Goober5000 - possible bug? (see when val is used below)
            /*val = */ eval_sexp (exp); // these sexps eval'd only for side effects
        }
    }
}

// Karajorma
void eval_when_do_all_exp (int all_actions, int) {
    arg_item* ptr;
    int exp;
    int actions;
    int op_num;

    bool first_loop = true;

    // loop through all the supplied arguments
    ptr = Sexp_applicable_argument_list.get_next ();

    while (ptr != NULL) {
        // acquire argument to be used
        Sexp_replacement_arguments.push_back (ptr->text);
        actions = all_actions;

        while (actions != -1) {
            exp = CAR (actions);

            op_num = get_operator_const (CTEXT (exp));

            if (first_loop || special_argument_appears_in_sexp_tree (exp)) {
                switch (op_num) {
                    // if the op is a conditional we have to make sure that it can
                    // access arguments
                case OP_WHEN:
                    Sexp_current_argument_nesting_level++;
                    Sexp_applicable_argument_list.add_data (ptr->text);
                    eval_sexp (exp);
                    Sexp_applicable_argument_list.clear_nesting_level ();
                    Sexp_current_argument_nesting_level--;
                    break;

                default: eval_sexp (exp);
                }
            }

            // iterate
            actions = CDR (actions);
        }

        first_loop = false;

        // remove the argument
        Sexp_replacement_arguments.pop_back ();
        // continue along argument list
        ptr = ptr->get_next ();
    }
}

/**
 * Evaluates the when conditional
 *
 * @note Goober5000 - added capability for arguments
 * @note Goober5000 - and also if-then-else and perform-actions
 */
int eval_when (int n, int when_op_num) {
    int arg_handler = -1, cond, val, actions;
    ASSERT (n >= 0);

    cond = CAR (n);
    actions = CDR (n);

    // evaluate just as-is
    val = eval_sexp (cond);

    // if value is true, perform the actions in the 'then' part
    if (val == SEXP_TRUE) // note: SEXP_KNOWN_TRUE is never returned from eval_sexp
    {
        // get the operator
        int exp = CAR (actions);

        // if the mod.tbl setting is in effect we want to each evaluate all the
        // SEXPs for each argument
        if (True_loop_argument_sexps &&
            special_argument_appears_in_sexp_tree (exp)) {
            if (exp != -1) {
                eval_when_do_all_exp (actions, when_op_num);
            }
        }
        // without the mod.tbl setting (or if there are no arguments in this
        // SEXP) we loop through every action performing them for all arguments
        else {
            while (actions != -1) {
                // get the operator
                exp = CAR (actions);
                if (exp != -1) eval_when_do_one_exp (exp);

                // iterate
                actions = CDR (actions);
            }
        }
    }

    // thanks to MageKing17 for noticing that we need to short-circuit on the
    // correct node!
    int short_circuit_node = (arg_handler >= 0) ? arg_handler : cond;

    if (Sexp_nodes[short_circuit_node].value == SEXP_KNOWN_FALSE || Sexp_nodes[short_circuit_node].value == SEXP_NAN_FOREVER)
        return SEXP_KNOWN_FALSE; // no need to waste time on this anymore

    // note: val can't be SEXP_KNOWN_FALSE at this point

    return val;
}

/**
 * Evaluate the conditional
 */
int eval_cond (int n) {
    int cond = 0, node, val = SEXP_FALSE;

    ASSERT (n >= 0);
    while (n >= 0) {
        node = CAR (n);
        cond = CAR (node);
        val = eval_sexp (cond);

        // if the conditional evaluated to true, then we must evaluate the rest
        // of the expression returning the value of this evaluation
        if (val == SEXP_TRUE) // note: any SEXP_KNOWN_TRUE result is returned
                              // as SEXP_TRUE
        {
            int actions, exp;

            val = SEXP_FALSE;
            actions = CDR (node);
            while (actions >= 0) {
                exp = CAR (actions);
                if (exp >= -1) val = eval_sexp (exp); // these sexp evaled only for side effects

                actions = CDR (actions);
            }

            break;
        }

        // move onto the next cond clause
        n = CDR (n);
    }

    return val;
}

// is there a better place to put this?  seems useful...
// credit to
// http://stackoverflow.com/questions/1903954/is-there-a-standard-sign-function-signum-sgn-in-c-c
template< typename T >
T sign (T t) {
    if (t == 0)
        return T (0);
    else
        return (t < 0) ? T (-1) : T (1);
}

// Goober5000 - added wing capability
int sexp_is_iff (int n) {
    int i, team;

    // iff value is the first parameter, second is a list of one or more
    // ships/wings to check to see if the iff value matches
    team = iff_lookup (CTEXT (n));

    n = CDR (n);
    for (; n != -1; n = CDR (n)) {
        object_ship_wing_point_team oswpt;
        sexp_get_object_ship_wing_point_team (&oswpt, CTEXT (n));

        switch (oswpt.type) {
        case OSWPT_TYPE_SHIP: {
            // if the team doesn't match the team specified, return false
            // immediately
            if (oswpt.shipp->team != team) return SEXP_FALSE;

            break;
        }

        case OSWPT_TYPE_PARSE_OBJECT: {
            // if the team doesn't match the team specified, return false
            // immediately
            if (oswpt.p_objp->team != team) return SEXP_FALSE;

            break;
        }

        case OSWPT_TYPE_WING:
        case OSWPT_TYPE_WING_NOT_PRESENT: {
            for (i = 0; i < oswpt.wingp->current_count; i++) {
                // if the team doesn't match the team specified, return false
                // immediately
                if (Ships[oswpt.wingp->ship_index[i]].team != team) return SEXP_FALSE;
            }

            break;
        }

        case OSWPT_TYPE_EXITED: {
            // see if we can find information about the exited ship (if it is a
            // ship)
            int exited_index = ship_find_exited_ship_by_name (CTEXT (n));
            if (exited_index >= 0) {
                // if the team doesn't match the team specified, return false
                // immediately
                if (Ships_exited[exited_index].team != team) return SEXP_KNOWN_FALSE;
            }
            else {
                // it's probably an exited wing, which we don't store
                // information about
                return SEXP_NAN_FOREVER;
            }
        }

        // we don't handle the other cases
        default: return SEXP_NAN;
        }
    }

    // got this far: we must be okay for all ships/wings
    return SEXP_TRUE;
}

// Goober5000
void sexp_ingame_ship_change_iff (ship* shipp, int new_team) {
    ASSERT (shipp != NULL);

    shipp->team = new_team;
}

// Goober5000
void sexp_parse_ship_change_iff (p_object* parse_obj, int new_team) {
    ASSERT (parse_obj);

    parse_obj->team = new_team;
}

void sexp_change_iff_helper (object_ship_wing_point_team oswpt, int new_team) {
    switch (oswpt.type) {
    // change ingame ship
    case OSWPT_TYPE_SHIP: {
        sexp_ingame_ship_change_iff (oswpt.shipp, new_team);

        break;
    }

    // change ship yet to arrive
    case OSWPT_TYPE_PARSE_OBJECT: {
        sexp_parse_ship_change_iff (oswpt.p_objp, new_team);

        break;
    }

    // change wing (we must set the flags for all ships present as well as all
    // ships yet to arrive)
    case OSWPT_TYPE_WING:
    case OSWPT_TYPE_WING_NOT_PRESENT: {
        // current ships
        for (int i = 0; i < oswpt.wingp->current_count; i++) sexp_ingame_ship_change_iff (&Ships[oswpt.wingp->ship_index[i]], new_team);

        // ships yet to arrive
        for (p_object* p_objp = GET_FIRST (&Ship_arrival_list); p_objp != END_OF_LIST (&Ship_arrival_list); p_objp = GET_NEXT (p_objp)) {
            if (p_objp->wingnum == WING_INDEX (oswpt.wingp)) sexp_parse_ship_change_iff (p_objp, new_team);
        }

        break;
    }
    }
}

// Goober5000 - added wing capability
void sexp_change_iff (int n) {
    int new_team;
    char* name;

    new_team = iff_lookup (CTEXT (n));
    n = CDR (n);

    for (; n != -1; n = CDR (n)) {
        name = CTEXT (n);
        object_ship_wing_point_team oswpt;
        sexp_get_object_ship_wing_point_team (&oswpt, name);
        sexp_change_iff_helper (oswpt, new_team);
    }
}

void sexp_ingame_ship_change_iff_color (ship* shipp, int observer_team, int observed_team, int alternate_iff_color) {
    ASSERT (shipp != NULL);

    shipp->ship_iff_color[observer_team][observed_team] = alternate_iff_color;
}

void sexp_parse_ship_change_iff_color (p_object* parse_obj, int observer_team, int observed_team, int alternate_iff_color) {
    ASSERT (parse_obj);

    parse_obj->alt_iff_color[observer_team][observed_team] = alternate_iff_color;
}

// following routine adds an ai goal to a ship structure.  The sexpression
// index passed in should be an ai-goal of the proper form.  The code in
// MissionGoal should check the syntax.

void sexp_add_ship_goal (int n) {
    int num, sindex;
    char* ship_name;

    ASSERT (n >= 0);
    ship_name = CTEXT (n);
    num = ship_name_lookup (ship_name, 1); // Goober5000 - including player
    if (num < 0)                           // ship not around anymore???? then forget it!
        return;

    sindex = CDR (n);
    ai_add_ship_goal_sexp (sindex, AIG_TYPE_EVENT_SHIP, &(Ai_info[Ships[num].ai_index]));
}

// identical to above, except add a wing
void sexp_add_wing_goal (int n) {
    int num, sindex;
    char* wing_name;

    ASSERT (n >= 0);
    wing_name = CTEXT (n);
    num = wing_name_lookup (wing_name);
    if (num < 0) // ship not around anymore???? then forget it!
        return;

    sindex = CDR (n);
    ai_add_wing_goal_sexp (sindex, AIG_TYPE_EVENT_WING, num);
}

/**
 * Adds a goal to the specified entry (ships and wings have unique names
 * between the two sets).
 */
void sexp_add_goal (int n) {
    int num, sindex;
    char* name;

    ASSERT (n >= 0);
    name = CTEXT (n);
    sindex = CDR (n);

    // first, look for ship name -- if found, then add ship goal.  else look
    // for wing name -- if found, add wing goal
    if ((num = ship_name_lookup (name, 1)) != -1) // Goober5000 - include players
        ai_add_ship_goal_sexp (sindex, AIG_TYPE_EVENT_SHIP, &(Ai_info[Ships[num].ai_index]));
    else if ((num = wing_name_lookup (name)) != -1)
        ai_add_wing_goal_sexp (sindex, AIG_TYPE_EVENT_WING, num);
}

/**
 * Clear out all AI goals for a ship
 */
void sexp_clear_ship_goals (int n) {
    int num;
    char* ship_name;

    ASSERT (n >= 0);
    ship_name = CTEXT (n);
    if ((num = ship_name_lookup (ship_name, 1)) != -1) // Goober5000 - include players
    {
        ai_clear_ship_goals (&(Ai_info[Ships[num].ai_index]));
    }
}

/**
 * Clear out AI goals for a wing
 */
void sexp_clear_wing_goals (int n) {
    int num;
    char* wing_name;

    ASSERT (n >= 0);
    wing_name = CTEXT (n);
    num = wing_name_lookup (wing_name);
    if (num < 0) return;
    ai_clear_wing_goals (num);
}

/**
 * Clear all AI goals for the given ship or wing
 */
void sexp_clear_goals (int n) {
    int num;
    char* name;

    ASSERT (n >= 0);
    while (n != -1) {
        name = CTEXT (n);
        if ((num = ship_name_lookup (name, 1)) != -1) // Goober5000 - include players
            ai_clear_ship_goals (&(Ai_info[Ships[num].ai_index]));
        else if ((num = wing_name_lookup (name)) != -1)
            ai_clear_wing_goals (num);

        n = CDR (n);
    }
}

void sexp_hud_set_message (int n) {
    char* gaugename = CTEXT (n);
    char* text = CTEXT (CDR (n));
    std::string message;

    for (int i = 0; i < Num_messages; i++) {
        if (!strcasecmp (text, Messages[i].name)) {
            message = Messages[i].message;

            sexp_replace_variable_names_with_values (message);

            HudGauge* cg = hud_get_gauge (gaugename);
            if (cg) { cg->updateCustomGaugeText (message); }
            else { WARNINGF (LOCATION, "Could not find a hud gauge named %s", gaugename); }
            return;
        }
    }

    WARNINGF (LOCATION, "sexp_hud_set_message couldn't find a message by the name of %s in the mission", text);
}

/* Make sure that the Sexp_hud_display_* get added to the game_state
transitions in freespace.cpp (game_enter_state()). */
int Sexp_hud_display_warpout = 0;

int hud_gauge_type_lookup (char* name) {
    for (int i = 0; i < Num_hud_gauge_types; i++) {
        if (!strcasecmp (name, Hud_gauge_types[i].name)) return Hud_gauge_types[i].def;
    }
    return -1;
}

// Goober5000
void sexp_stop_music (bool fade) {
    if (Sexp_music_handle != -1) {
        audiostream_close_file (Sexp_music_handle, fade);
        Sexp_music_handle = -1;
    }
}

// Goober5000
void sexp_start_music (int loop) {
    if (Sexp_music_handle != -1) {
        if (!audiostream_is_playing (Sexp_music_handle)) audiostream_play (Sexp_music_handle, (Master_event_music_volume * aav_music_volume), loop);
    }
    else { WARNINGF (LOCATION, "Can not play music. sexp_start_music called when no music file is set for Sexp_music_handle!"); }
}

// Goober5000
void sexp_music_close () {
    if (Cmdline_freespace_no_music) { return; }
    sexp_stop_music ();
}

// this function get called by send-message or send-message random with the
// name of the message, sender, and priority.
void sexp_send_one_message (char* name, char* who_from, char* priority, int group, int delay) {
    int ipriority, num, ship_index, source;
    ship* shipp;

    if (physics_paused) { return; }

    ASSERT ((name != NULL) && (who_from != NULL) && (priority != NULL));

    // determine the priority of the message
    if (!strcasecmp (priority, "low"))
        ipriority = MESSAGE_PRIORITY_LOW;
    else if (!strcasecmp (priority, "normal"))
        ipriority = MESSAGE_PRIORITY_NORMAL;
    else if (!strcasecmp (priority, "high"))
        ipriority = MESSAGE_PRIORITY_HIGH;
    else {
        WARNINGF (LOCATION, "Encountered invalid priority \"%s\" in send-message", priority);
        ipriority = MESSAGE_PRIORITY_NORMAL;
    }

    // check to see if the 'who_from' string is a ship that had been destroyed
    // or departed.  If so, then don't send the message.  We must look at
    // 'who_from' to determine what to look for.  who_from may be any allied
    // person, any wingman, a wingman from a specific wing, or a specific ship
    ship_index = -1;
    shipp = NULL;
    source = MESSAGE_SOURCE_COMMAND;
    if (who_from[0] == '#') {
        message_send_unique_to_player (name, &(who_from[1]), MESSAGE_SOURCE_SPECIAL, ipriority, group, delay);
        return;
    }
    else if (!strcasecmp (who_from, "<any allied>")) { return; }
    else if ((num = wing_name_lookup (who_from)) != -1) {
        // message from a wing
        // choose wing leader to speak for wing (hence "1" at end of
        // ship_get_random_ship_in_wing)
        ship_index = ship_get_random_ship_in_wing (num, SHIP_GET_UNSILENCED, 1);
        if (ship_index == -1) {
            if (ipriority != MESSAGE_PRIORITY_HIGH) return;
        }
    }
    else if (
        mission_log_get_time (LOG_SHIP_DESTROYED, who_from, NULL, NULL) || mission_log_get_time (LOG_SHIP_DEPARTED, who_from, NULL, NULL) ||
        mission_log_get_time (LOG_SELF_DESTRUCTED, who_from, NULL, NULL) || mission_log_get_time (LOG_WING_DESTROYED, who_from, NULL, NULL) ||
        mission_log_get_time (LOG_WING_DEPARTED, who_from, NULL, NULL)) {
        // getting into this if statement means that the ship or wing (sender)
        // is no longer in the mission if message is high priority, make it
        // come from Terran Command
        if (ipriority != MESSAGE_PRIORITY_HIGH) return;

        source = MESSAGE_SOURCE_COMMAND;
    }
    else if (!strcasecmp (who_from, "<any wingman>") || (wing_name_lookup (who_from) != -1)) { source = MESSAGE_SOURCE_WINGMAN; }
    else if (!strcasecmp (who_from, "<none>")) { source = MESSAGE_SOURCE_NONE; }
    else {
        // Message from a apecific ship
        source = MESSAGE_SOURCE_SHIP;
        ship_index = ship_name_lookup (who_from);
        if (ship_index == -1) {
            // bail if not high priority, otherwise reroute to command
            if (ipriority != MESSAGE_PRIORITY_HIGH) return;
            source = MESSAGE_SOURCE_COMMAND;
        }
    }

    if (ship_index == -1) { shipp = NULL; }
    else { shipp = &Ships[ship_index]; }

    message_send_unique_to_player (name, shipp, source, ipriority, group, delay);
}

void sexp_send_message (int n) {
    char *name, *who_from, *priority, *tmp;

    if (physics_paused) { return; }

    ASSERT (n != -1);
    who_from = CTEXT (n);
    priority = CTEXT (CDR (n));
    name = CTEXT (CDR (CDR (n)));

    // a temporary check to see if the name field matched a priority since I am
    // in the process of reordering the arguments
    if (!strcasecmp (name, "low") || !strcasecmp (name, "normal") || !strcasecmp (name, "high")) {
        tmp = name;
        name = priority;
        priority = tmp;
    }

    sexp_send_one_message (name, who_from, priority, 0, 0);
}

void sexp_send_message_list (int n) {
    char *name, *who_from, *priority;
    int delay;

    if (physics_paused) { return; }

    // send a bunch of messages
    delay = 0;
    while (n != -1) {
        who_from = CTEXT (n);

        // next node
        n = CDR (n);
        if (n == -1) {
            WARNINGF (LOCATION, "Detected incomplete parameter list in sexp-send-message-list");
            return;
        }
        priority = CTEXT (n);

        // next node
        n = CDR (n);
        if (n == -1) {
            WARNINGF (LOCATION, "Detected incomplete parameter list in sexp-send-message-list");
            return;
        }
        name = CTEXT (n);

        // next node
        n = CDR (n);
        if (n == -1) {
            WARNINGF (LOCATION, "Detected incomplete parameter list in sexp-send-message-list");
            return;
        }
        delay += eval_num (n);

        // send the message
        sexp_send_one_message (name, who_from, priority, 1, delay);

        // next node
        n = CDR (n);
    }
}

void sexp_send_random_message (int n) {
    char *name, *who_from, *priority;
    int temp, num_messages, message_num;

    ASSERT (n != -1);
    who_from = CTEXT (n);
    priority = CTEXT (CDR (n));

    if (physics_paused) { return; }

    // count the number of messages that we have
    n = CDR (CDR (n));
    temp = n;
    num_messages = 0;
    while (n != -1) {
        n = CDR (n);
        num_messages++;
    }
    ASSERT (num_messages >= 1);

    // get a random message, and pass the parameters to send_one_message
    message_num = myrand () % num_messages;
    n = temp;
    while (n != -1) {
        if (message_num == 0) break;
        message_num--;
        n = CDR (n);
    }
    ASSERT (n != -1); // should have found the message!!!
    name = CTEXT (n);

    sexp_send_one_message (name, who_from, priority, 0, 0);
}

void sexp_self_destruct (int node) {
    int n, ship_num;

    for (n = node; n != -1; n = CDR (n)) {
        // get the ship
        ship_num = ship_name_lookup (CTEXT (n));

        // if it still exists, destroy it
        if (ship_num >= 0) { ship_self_destruct (&Objects[Ships[ship_num].objnum]); }
    }
}

void sexp_next_mission (int n) {
    char* mission_name;
    int i;

    mission_name = CTEXT (n);

    if (mission_name == NULL) { ASSERTX (0, "Mission name is NULL in campaign file for next-mission command!"); }

    for (i = 0; i < Campaign.num_missions; i++) {
        if (!strcasecmp (Campaign.missions[i].name, mission_name)) {
            Campaign.next_mission = i;
            return;
        }
    }
    ASSERTX (0, "Mission name %s not found in campaign file for next-mission command", mission_name);
}

/**
 * Deal with the end-of-campaign sexpression.
 */
void sexp_end_of_campaign (int /*n*/) {
    // this is really a do-nothing sexpression.  It is pretty much a
    // placeholder to allow campaigns to have repeat-mission branches at the
    // end of the campaign.  By not setting anything in this function, the
    // higher level campaign code will see this as end-of-campaign since
    // next_mission isn't set to anything.  (To be safe, we'll set to -1).
    Campaign.next_mission = -1;
}

// sexpression to end everything.  One parameter is the movie to play when this
// is over. Goober5000 - edited to only to the FS2-specific code when actually
// ending the FS2 main campaign, and otherwise to do the conventional code
void sexp_end_campaign (int n) {
    bool ignore_player_mortality = true;

    if (!(Game_mode & GM_CAMPAIGN_MODE)) { return; }

    if (n != -1) { ignore_player_mortality = is_sexp_true (n); }

    // if the player is dead we may want to let the death screen handle things
    if (!ignore_player_mortality && (Player_ship->flags[Ship::Ship_Flags::Dying])) { return; }

    // in FS2 our ending is a bit wacky. we'll just flag the mission as having
    // ended the campaign
    //
    // changed this to check for an active supernova rather than a special
    // campaign since the supernova code needs special time to execute and will
    // post GS_EVENT_END_CAMPAIGN with Game_mode check or show death-popup when
    // it's done - taylor
    if (supernova_active () /*&& !strcasecmp(Campaign.filename, "freespace2")*/) { Campaign_ending_via_supernova = 1; }
    else {
        // post and event to move us to the end-of-campaign state
        gameseq_post_event (GS_EVENT_END_CAMPAIGN);
    }
}

/**
 * Reduces the strength of a subsystem by the given percentage.
 *
 * If it is reduced to below 0%, then the hits of the subsystem are set to 0
 */
void sexp_sabotage_subsystem (int n) {
    char *shipname, *subsystem;
    int percentage, shipnum, index, generic_type;
    float sabotage_hits;
    ship* shipp;
    ship_subsys *ss = NULL, *ss_start;
    bool do_loop = true;

    shipname = CTEXT (n);
    subsystem = CTEXT (CDR (n));
    percentage = eval_num (CDR (CDR (n)));

    shipnum = ship_name_lookup (shipname);

    // if no ship, then return immediately.
    if (shipnum == -1) return;
    shipp = &Ships[shipnum];

    // see if we are dealing with the HULL
    if (!strcasecmp (subsystem, SEXP_HULL_STRING)) {
        float ihs;
        object* objp;

        ihs = shipp->ship_max_hull_strength;
        sabotage_hits = ihs * ((float)percentage / 100.0f);
        objp = &Objects[shipp->objnum];
        objp->hull_strength -= sabotage_hits;

        // self destruct the ship if <= 0.
        if (objp->hull_strength <= 0.0f) ship_self_destruct (objp);
        return;
    }

    // see if we are dealing with the Simulated HULL
    if (!strcasecmp (subsystem, SEXP_SIM_HULL_STRING)) {
        float ihs;
        object* objp;

        ihs = shipp->ship_max_hull_strength;
        sabotage_hits = ihs * ((float)percentage / 100.0f);
        objp = &Objects[shipp->objnum];
        objp->sim_hull_strength -= sabotage_hits;

        return;
    }

    // now find the given subsystem on the ship.  This could be a generic type
    // like <All Engines>
    generic_type = get_generic_subsys (subsystem);
    ss_start = GET_FIRST (&shipp->subsys_list);

    while (do_loop) {
        if (generic_type) {
            // loop until we find a subsystem of that type
            for (; ss_start != END_OF_LIST (&Ships[shipnum].subsys_list); ss_start = GET_NEXT (ss_start)) {
                ss = NULL;
                if (generic_type == ss_start->system_info->type) {
                    ss = ss_start;
                    ss_start = GET_NEXT (ss_start);
                    break;
                }
            }

            // reached the end of the subsystem list
            if (ss_start == END_OF_LIST (&Ships[shipnum].subsys_list)) {
                // If the last subsystem wasn't of interest we don't need to go
                // any further
                if (ss == NULL) { return; }
                do_loop = false;
            }
        }
        else {
            do_loop = false;
            index = ship_get_subsys_index (shipp, subsystem,
                                           1); // Bypass any error since we supply one here
            if (index == -1) {
                WARNINGF (LOCATION, "Couldn't find subsystem %s on ship %s for sabotage subsystem", subsystem, shipp->ship_name);
                return;
            }
            // get the pointer to the subsystem.  Check it's current hits
            // against it's max hits, and set the strength to the given
            // percentage if current strength is > given percentage
            ss = ship_get_indexed_subsys (shipp, index);
            if (ss == NULL) {
                WARNINGF (LOCATION, "Nonexistent subsystem for index %d on ship %s for sabotage subsystem", index, shipp->ship_name);
                return;
            }
        }

        sabotage_hits = ss->max_hits * ((float)percentage / 100.0f);
        ss->current_hits -= sabotage_hits;
        if (ss->current_hits < 0.0f) ss->current_hits = 0.0f;

        // maybe blow up subsys
        if (ss->current_hits <= 0) { do_subobj_destroyed_stuff (shipp, ss, NULL); }

        ship_recalc_subsys_strength (shipp);
    }
}

/**
 * Adds some percentage of hits to a subsystem.
 *
 * Anything repaired about 100% is set to max hits
 */
void sexp_repair_subsystem (int n) {
    char *shipname, *subsystem;
    int percentage, shipnum, index, generic_type;
    bool do_submodel_repair;
    float repair_hits;
    ship* shipp;
    ship_subsys *ss = NULL, *ss_start;
    bool do_loop = true;

    shipname = CTEXT (n);
    subsystem = CTEXT (CDR (n));
    shipnum = ship_name_lookup (shipname);

    do_submodel_repair = (CDDDR (n) == -1) || is_sexp_true (CDDDR (n));

    // if no ship, then return immediately.
    if (shipnum == -1) { return; }
    shipp = &Ships[shipnum];

    // get percentage
    percentage = eval_num (CDR (CDR (n)));

    // see if we are dealing with the HULL
    if (!strcasecmp (subsystem, SEXP_HULL_STRING)) {
        float ihs;
        object* objp;

        ihs = shipp->ship_max_hull_strength;
        repair_hits = ihs * ((float)percentage / 100.0f);
        objp = &Objects[shipp->objnum];
        objp->hull_strength += repair_hits;
        if (objp->hull_strength > ihs) objp->hull_strength = ihs;
        return;
    }

    // see if we are dealing with the Simulated HULL
    if (!strcasecmp (subsystem, SEXP_SIM_HULL_STRING)) {
        float ihs;
        object* objp;

        ihs = shipp->ship_max_hull_strength;
        repair_hits = ihs * ((float)percentage / 100.0f);
        objp = &Objects[shipp->objnum];
        objp->hull_strength += repair_hits;
        if (objp->sim_hull_strength > ihs) objp->sim_hull_strength = ihs;
        return;
    }

    // now find the given subsystem on the ship.This could be a generic type
    // like <All Engines>
    generic_type = get_generic_subsys (subsystem);
    ss_start = GET_FIRST (&shipp->subsys_list);

    while (do_loop) {
        if (generic_type) {
            // loop until we find a subsystem of that type
            for (; ss_start != END_OF_LIST (&Ships[shipnum].subsys_list); ss_start = GET_NEXT (ss_start)) {
                ss = NULL;
                if (generic_type == ss_start->system_info->type) {
                    ss = ss_start;
                    ss_start = GET_NEXT (ss_start);
                    break;
                }
            }

            // reached the end of the subsystem list
            if (ss_start == END_OF_LIST (&Ships[shipnum].subsys_list)) {
                // If the last subsystem wasn't of interest we don't need to go
                // any further
                if (ss == NULL) { return; }
                do_loop = false;
            }
        }
        else {
            do_loop = false;
            index = ship_get_subsys_index (shipp, subsystem,
                                           1); // Bypass any error since we supply one here
            if (index == -1) {
                WARNINGF (LOCATION, "Couldn't find subsystem %s on ship %s for repair subsystem", subsystem, shipp->ship_name);
                return;
            }
            // get the pointer to the subsystem.  Check it's current hits
            // against it's max hits, and set the strength to the given
            // percentage if current strength is < given percentage
            ss = ship_get_indexed_subsys (shipp, index);
            if (ss == NULL) {
                WARNINGF (LOCATION, "Nonexistent subsystem for index %d on ship %s for repair subsystem", index, shipp->ship_name);
                return;
            }
        }

        repair_hits = ss->max_hits * ((float)percentage / 100.0f);
        ss->current_hits += repair_hits;
        if (ss->current_hits > ss->max_hits) ss->current_hits = ss->max_hits;

        if ((ss->current_hits > 0) && (do_submodel_repair)) {
            ss->submodel_info_1.blown_off = 0;
            ss->submodel_info_2.blown_off = 0;
        }

        ship_recalc_subsys_strength (shipp);
    }
}

/**
 * Set a subsystem of a ship at a specific percentage
 */
void sexp_set_subsystem_strength (int n) {
    char *shipname, *subsystem;
    int percentage, shipnum, index, generic_type;
    bool do_submodel_repair;
    ship* shipp;
    ship_subsys *ss = NULL, *ss_start;
    bool do_loop = true;

    shipname = CTEXT (n);
    subsystem = CTEXT (CDR (n));
    percentage = eval_num (CDR (CDR (n)));

    do_submodel_repair = (CDDDR (n) == -1) || is_sexp_true (CDDDR (n));

    shipnum = ship_name_lookup (shipname);

    // if no ship, then return immediately.
    if (shipnum == -1) return;
    shipp = &Ships[shipnum];

    if (percentage > 100) {
        WARNINGF (LOCATION, "Percentage for set_subsystem_strength > 100 on ship %s for subsystem '%s'-- setting to 100", shipname, subsystem);
        percentage = 100;
    }
    else if (percentage < 0) {
        WARNINGF (LOCATION, "Percantage for set_subsystem_strength < 0 on ship %s for subsystem '%s' -- setting to 0", shipname, subsystem);
        percentage = 0;
    }

    // see if we are dealing with the HULL
    if (!strcasecmp (subsystem, SEXP_HULL_STRING)) {
        float ihs;
        object* objp;

        objp = &Objects[shipp->objnum];

        // destroy the ship if percentage is 0
        if (percentage == 0) { ship_self_destruct (objp); }
        else {
            ihs = shipp->ship_max_hull_strength;
            objp->hull_strength = ihs * ((float)percentage / 100.0f);
        }

        return;
    }

    // see if we are dealing with the Simulated HULL
    if (!strcasecmp (subsystem, SEXP_SIM_HULL_STRING)) {
        float ihs;
        object* objp;

        objp = &Objects[shipp->objnum];
        ihs = shipp->ship_max_hull_strength;
        objp->sim_hull_strength = ihs * ((float)percentage / 100.0f);

        return;
    }

    // now find the given subsystem on the ship.This could be a generic type
    // like <All Engines>
    generic_type = get_generic_subsys (subsystem);
    ss_start = GET_FIRST (&shipp->subsys_list);

    while (do_loop) {
        if (generic_type) {
            // loop until we find a subsystem of that type
            for (; ss_start != END_OF_LIST (&Ships[shipnum].subsys_list); ss_start = GET_NEXT (ss_start)) {
                ss = NULL;
                if (generic_type == ss_start->system_info->type) {
                    ss = ss_start;
                    ss_start = GET_NEXT (ss_start);
                    break;
                }
            }

            // reached the end of the subsystem list
            if (ss_start == END_OF_LIST (&Ships[shipnum].subsys_list)) {
                // If the last subsystem wasn't of interest we don't need to go
                // any further
                if (ss == NULL) { return; }
                do_loop = false;
            }
        }
        else {
            do_loop = false;
            index = ship_get_subsys_index (shipp, subsystem,
                                           1); // Bypass any error since we supply one here
            if (index == -1) {
                WARNINGF (LOCATION, "Couldn't find subsystem %s on ship %s for set subsystem strength", subsystem, shipp->ship_name);
                return;
            }

            // get the pointer to the subsystem.  Check it's current hits
            // against it's max hits, and set the strength to the given
            // percentage
            ss = ship_get_indexed_subsys (shipp, index);
            if (ss == NULL) {
                WARNINGF (LOCATION, "Nonexistent subsystem for index %d on ship %s for set subsystem strength", index, shipp->ship_name);
                return;
            }
        }

        // maybe blow up subsys
        if (ss->current_hits > 0) {
            if (percentage < 1) { do_subobj_destroyed_stuff (shipp, ss, NULL); }
        }

        // set hit points
        ss->current_hits = ss->max_hits * ((float)percentage / 100.0f);

        if ((ss->current_hits > 0) && (do_submodel_repair)) {
            ss->submodel_info_1.blown_off = 0;
            ss->submodel_info_2.blown_off = 0;
        }

        ship_recalc_subsys_strength (shipp);
    }
}

/**
 * Changes the validity of a goal.
 *
 * The flag paramater tells us whether to mark the goals as valid or invalid
 */
void sexp_change_goal_validity (int n, int flag) {
    char* name;

    while (n != -1) {
        name = CTEXT (n);
        if (flag)
            mission_goal_mark_valid (name);
        else
            mission_goal_mark_invalid (name);

        n = CDR (n);
    }
}

/**
 * Transfer cargo from one ship to another
 */
void sexp_transfer_cargo (int n) {
    char *shipname1, *shipname2;
    int shipnum1, shipnum2, i;

    shipname1 = CTEXT (n);
    shipname2 = CTEXT (CDR (n));

    // find the ships -- if neither in the mission, the abort
    shipnum1 = ship_name_lookup (shipname1);
    shipnum2 = ship_name_lookup (shipname2);
    if ((shipnum1 == -1) || (shipnum2 == -1)) return;

    // we must be sure that these two objects are indeed docked
    if (!dock_check_find_direct_docked_object (&Objects[Ships[shipnum1].objnum], &Objects[Ships[shipnum2].objnum])) {
        WARNINGF (LOCATION, "Tried to transfer cargo between %s and %s although they aren't docked!", Ships[shipnum1].ship_name, Ships[shipnum2].ship_name);
        return;
    }

    if (!strcasecmp (Cargo_names[Ships[shipnum1].cargo1 & CARGO_INDEX_MASK], "nothing")) { return; }

    // transfer cargo from ship1 to ship2
#ifndef NDEBUG
    // Don't give warning for large ships (cruiser on up)
    if (!(Ship_info[Ships[shipnum2].ship_info_index].is_big_or_huge ())) {
        if (strcasecmp (Cargo_names[Ships[shipnum2].cargo1 & CARGO_INDEX_MASK], "nothing") != 0) {
            WARNINGF (
                LOCATION, "Transferring cargo to %s which already\nhas cargo %s.\nCargo will be replaced", Ships[shipnum2].ship_name,
                Cargo_names[Ships[shipnum2].cargo1 & CARGO_INDEX_MASK]);
        }
    }
#endif
    Ships[shipnum2].cargo1 = char ((Ships[shipnum1].cargo1 & CARGO_INDEX_MASK) | (Ships[shipnum2].cargo1 & CARGO_NO_DEPLETE));

    if (!(Ships[shipnum1].cargo1 & CARGO_NO_DEPLETE)) {
        // need to set ship1's cargo to nothing.  scan the cargo_names array
        // looking for the string nothing. add it if not found
        for (i = 0; i < Num_cargo; i++) {
            if (!strcasecmp (Cargo_names[i], "nothing")) {
                Ships[shipnum1].cargo1 = char (i);
                return;
            }
        }
        strcpy (Cargo_names[i], "Nothing");
        Num_cargo++;
    }
}

/**
 * Exchanges cargo between two ships
 */
void sexp_exchange_cargo (int n) {
    char *shipname1, *shipname2;
    int shipnum1, shipnum2, temp;

    shipname1 = CTEXT (n);
    shipname2 = CTEXT (CDR (n));

    // find the ships -- if neither in the mission, abort
    shipnum1 = ship_name_lookup (shipname1);
    shipnum2 = ship_name_lookup (shipname2);
    if ((shipnum1 == -1) || (shipnum2 == -1)) return;

    // we must be sure that these two objects are indeed docked
    if (!dock_check_find_direct_docked_object (&Objects[Ships[shipnum1].objnum], &Objects[Ships[shipnum2].objnum])) {
        WARNINGF (LOCATION, "Tried to exchange cargo between %s and %s although they aren't docked!", Ships[shipnum1].ship_name, Ships[shipnum2].ship_name);
        return;
    }

    temp = (Ships[shipnum1].cargo1 & CARGO_INDEX_MASK);
    Ships[shipnum1].cargo1 = char (Ships[shipnum2].cargo1 & CARGO_INDEX_MASK);
    Ships[shipnum2].cargo1 = char (temp);
}

void sexp_cap_waypoint_speed (int n) {
    char* shipname;
    int shipnum;
    int speed;

    shipname = CTEXT (n);
    speed = eval_num (CDR (n));

    shipnum = ship_name_lookup (shipname);

    if (shipnum == -1) {
        // trying to set waypoint speed of ship not already in game
        return;
    }

    // cap speed to range (-1, 32767) to store within int
    if (speed < 0) { speed = -1; }

    if (speed > 32767) { speed = 32767; }

    Ai_info[Ships[shipnum].ai_index].waypoint_speed_cap = speed;
}

void sexp_cargo_no_deplete (int n) {
    char* shipname;
    int ship_index, no_deplete = 1;

    // get some data
    shipname = CTEXT (n);

    // lookup the ship
    ship_index = ship_name_lookup (shipname);
    if (ship_index < 0) { return; }

    if (!(Ship_info[Ships[ship_index].ship_info_index].is_big_or_huge ())) {
        WARNINGF (LOCATION, "Trying to make non BIG or HUGE ship %s with non-depletable cargo.", Ships[ship_index].ship_name);
        return;
    }

    if (CDR (n) != -1) {
        no_deplete = eval_num (CDR (n));
        ASSERT ((no_deplete == 0) || (no_deplete == 1));
        if ((no_deplete != 0) && (no_deplete != 1)) { no_deplete = 1; }
    }

    if (no_deplete) { Ships[ship_index].cargo1 |= CARGO_NO_DEPLETE; }
    else { Ships[ship_index].cargo1 &= (~CARGO_NO_DEPLETE); }
}

/**
 * Toggle the status bit for the AI code which tells the AI if it is a good
 * time to rearm.
 *
 * The status being set means good time.  Status not being set (unset), means
 * bad time. Designers must implement this.
 */
void sexp_good_time_to_rearm (int n) {
    int team, time;

    team = iff_lookup (CTEXT (n));
    time = eval_num (CDR (n)); // this is the time for how long a good rearm is
                               // active -- in seconds

    ai_set_rearm_status (team, time);
}

/**
 * Grants promotion to the player
 */
void sexp_grant_promotion () {
    // set a bit to tell player should get promoted at the end of the mission.
    // I suppose the other thing that we could do would be to set the players
    // score to at least the amount of points for the next level, but this way
    // is better I think.
    if (Game_mode & GM_CAMPAIGN_MODE) { Player->flags |= PLAYER_FLAGS_PROMOTED; }
}

/**
 * Gives the named medal to the players in the mission
 */
void sexp_grant_medal (int n) {
    int i;
    char* medal_name;

    // don't give medals in normal gameplay when not in campaign mode
    if ((Game_mode & GM_NORMAL) && !(Game_mode & GM_CAMPAIGN_MODE)) return;

    medal_name = CTEXT (n);
    if (medal_name == NULL) return;

    if (Player->stats.m_medal_earned >= 0) {
        WARNINGF (LOCATION, "Cannot grant more than one medal per mission!  New medal '%s' will replace old medal '%s'!", medal_name, Medals[Player->stats.m_medal_earned].name);
    }

    for (i = 0; i < Num_medals; i++) {
        if (!strcasecmp (medal_name, Medals[i].name)) break;
    }

    if (i < Num_medals) { Player->stats.m_medal_earned = i; }
}

void sexp_change_player_score (int node) {
    int sindex;
    int score;

    score = eval_num (node);
    node = CDR (node);

    if ((sindex = ship_name_lookup (CTEXT (node))) == -1) {
        WARNINGF (LOCATION, "Invalid shipname '%s' passed to sexp_change_player_score!", CTEXT (node));
        return;
    }

    if (Player_ship != &Ships[sindex]) {
        WARNINGF (LOCATION, "Can not award points to '%s'. Ship is not a player!", CTEXT (node));
        return;
    }
    Player->stats.m_score += score;
    if (Player->stats.m_score < 0) { Player->stats.m_score = 0; }
}

void sexp_tech_add_ship (int node) {
    int i;
    char* name;

    ASSERT (node >= 0);
    // this function doesn't mean anything when not in campaign mode
    if (!(Game_mode & GM_CAMPAIGN_MODE)) return;

    while (node >= 0) {
        name = CTEXT (node);
        i = ship_info_lookup (name);
        if (i >= 0)
            Ship_info[i].flags.set (Ship::Info_Flags::In_tech_database);
        else
            WARNINGF (LOCATION, "In tech-add-ship, ship class \"%s\" invalid", name);

        node = CDR (node);
    }
}

void sexp_tech_add_weapon (int node) {
    int i;
    char* name;

    ASSERT (node >= 0);
    // this function doesn't mean anything when not in campaign mode
    if (!(Game_mode & GM_CAMPAIGN_MODE)) return;

    while (node >= 0) {
        name = CTEXT (node);
        i = weapon_info_lookup (name);
        if (i >= 0)
            Weapon_info[i].wi_flags.set (Weapon::Info_Flags::In_tech_database);
        else
            WARNINGF (LOCATION, "In tech-add-weapon, weapon class \"%s\" invalid", name);

        node = CDR (node);
    }
}

/**
 * Set variables needed to grant a new ship/weapon to the player during the
 * course of a mission
 */
void sexp_allow_ship (int n) {
    int sindex;
    char* name;

    // this function doesn't mean anything when not in campaign mode
    if (!(Game_mode & GM_CAMPAIGN_MODE)) return;

    // get the name of the ship and lookup up the ship_info index for it
    name = CTEXT (n);
    sindex = ship_info_lookup (name);
    if (sindex == -1) return;

    // now we have a valid index --
    mission_campaign_save_persistent (CAMPAIGN_PERSISTENT_SHIP, sindex);
}

void sexp_allow_weapon (int n) {
    int sindex;
    char* name;

    // this function doesn't mean anything when not in campaign mode
    if (!(Game_mode & GM_CAMPAIGN_MODE)) return;

    // get the name of the weapon and lookup up the weapon_info index for it
    name = CTEXT (n);
    sindex = weapon_info_lookup (name);
    if (sindex == -1) return;

    // now we have a valid index --
    mission_campaign_save_persistent (CAMPAIGN_PERSISTENT_WEAPON, sindex);
}

/**
 * generic function for all those sexps that set flags
 * For all flag type parameters: If a particular flag should not be set, use
 * the ::NUM_VALUES member of that enum
 *
 * @note this function has a similar purpose to sexp_alter_ship_flag_helper;
 * make sure you check/update both
 */
void sexp_deal_with_ship_flag (
    int node, bool process_subsequent_nodes, Object::Object_Flags object_flag, Ship::Ship_Flags ship_flag, Mission::Parse_Object_Flags p_object_flag, bool set_it,
    bool /* send_multiplayer */ = false, bool include_players_in_ship_lookup = false) {
    char* ship_name;
    int ship_index;
    int n = node;

    // loop for all ships in the sexp
    // NB: if the flag is set, we will continue acting on nodes until we run
    // out of them;
    // if not, we will only act on the first one
    for (; n >= 0; process_subsequent_nodes ? n = CDR (n) : n = -1) {
        // get ship name
        ship_name = CTEXT (n);

        // check to see if ship destroyed or departed.  In either case, do
        // nothing.
        if (mission_log_get_time (LOG_SHIP_DEPARTED, ship_name, NULL, NULL) || mission_log_get_time (LOG_SELF_DESTRUCTED, ship_name, NULL, NULL) ||
            mission_log_get_time (LOG_SHIP_DESTROYED, ship_name, NULL, NULL))
            continue;

        // see if ship exists in-mission
        ship_index = ship_name_lookup (ship_name, include_players_in_ship_lookup ? 1 : 0);

        // if ship is in-mission
        if (ship_index >= 0) {
            // save flags for state change comparisons
            auto object_flag_orig = Objects[Ships[ship_index].objnum].flags;

            // see if we have an object flag to set
            if (object_flag != Object::Object_Flags::NUM_VALUES) {
                // set or clear?
                Objects[Ships[ship_index].objnum].flags.set ((Object::Object_Flags)object_flag, set_it);
            }

            // handle ETS when modifying shields
            if (object_flag == Object::Object_Flags::No_shields) {
                if (set_it) { zero_one_ets (&Ships[ship_index].shield_recharge_index, &Ships[ship_index].weapon_recharge_index, &Ships[ship_index].engine_recharge_index); }
                else if (object_flag_orig[Object::Object_Flags::No_shields]) { set_default_recharge_rates (&Objects[Ships[ship_index].objnum]); }
            }

            // see if we have a ship flag to set
            if (ship_flag != Ship::Ship_Flags::NUM_VALUES) {
                // set or clear?
                Ships[ship_index].flags.set ((Ship::Ship_Flags)ship_flag, set_it);
            }

            // the lock afterburner SEXP also needs to set a physics flag
            if (ship_flag == Ship::Ship_Flags::Afterburner_locked) {
                if (set_it) { afterburners_stop (&Objects[Ships[ship_index].objnum], 1); }
            }
        }
        // if it's not in-mission
        else {
            // grab it from the arrival list
            p_object* p_objp = mission_parse_get_arrival_ship (ship_name);

            // ships that have had ship-vanish used on them should be skipped
            if (!p_objp) { continue; }

            // see if we have a p_object flag to set
            if (p_object_flag != Mission::Parse_Object_Flags::NUM_VALUES) {
                // set or clear?
                p_objp->flags.set ((Mission::Parse_Object_Flags)p_object_flag, set_it);
            }
        }
    }
}

/**
 * sets flags on objects from alter-ship-flag
 *
 * @note this function has a similar purpose to sexp_deal_with_ship_flag; make
 * sure you check/update both
 */
void sexp_alter_ship_flag_helper (
    object_ship_wing_point_team& oswpt, bool future_ships, Object::Object_Flags object_flag, Ship::Ship_Flags ship_flag, Mission::Parse_Object_Flags parse_obj_flag,
    AI::AI_Flags ai_flag, bool set_flag) {
    int i;
    flagset< Object::Object_Flags > object_flag_orig;
    ship_obj* so;
    object_ship_wing_point_team oswpt2;
    p_object* p_objp;

    switch (oswpt.type) {
    case OSWPT_TYPE_NONE:
    case OSWPT_TYPE_EXITED: return;

    case OSWPT_TYPE_WHOLE_TEAM:
        ASSERT (oswpt.team >= 0);
        oswpt2.clear ();
        for (so = GET_FIRST (&Ship_obj_list); so != END_OF_LIST (&Ship_obj_list); so = GET_NEXT (so)) {
            if (Ships[Objects[so->objnum].instance].team == oswpt.team) {
                object_ship_wing_point_team_set_ship (&oswpt2, so, future_ships);

                // recurse
                sexp_alter_ship_flag_helper (oswpt2, future_ships, object_flag, ship_flag, parse_obj_flag, ai_flag, set_flag);
            }
        }

        if (future_ships) {
            for (p_objp = GET_FIRST (&Ship_arrival_list); p_objp != END_OF_LIST (&Ship_arrival_list); p_objp = GET_NEXT (p_objp)) {
                if (p_objp->team == oswpt.team) {
                    oswpt2.p_objp = p_objp;
                    oswpt2.type = OSWPT_TYPE_PARSE_OBJECT;
                    sexp_alter_ship_flag_helper (oswpt2, future_ships, object_flag, ship_flag, parse_obj_flag, ai_flag, set_flag);
                }
            }
        }
        break;

    case OSWPT_TYPE_WING:
    case OSWPT_TYPE_WING_NOT_PRESENT:
        // if the wing isn't here, and we're only dealing with ships which are,
        // we're done.
        if (!future_ships) {
            if (oswpt.type == OSWPT_TYPE_WING_NOT_PRESENT) { return; }
        }
        else {
            for (p_objp = GET_FIRST (&Ship_arrival_list); p_objp != END_OF_LIST (&Ship_arrival_list); p_objp = GET_NEXT (p_objp)) {
                if (p_objp->wingnum == WING_INDEX (oswpt.wingp)) {
                    oswpt2.p_objp = p_objp;
                    oswpt2.type = OSWPT_TYPE_PARSE_OBJECT;
                    sexp_alter_ship_flag_helper (oswpt2, future_ships, object_flag, ship_flag, parse_obj_flag, ai_flag, set_flag);
                }
            }
        }

        for (i = 0; i < oswpt.wingp->current_count; i++) {
            object_ship_wing_point_team_set_ship (&oswpt2, &Ships[oswpt.wingp->ship_index[i]], future_ships);
            sexp_alter_ship_flag_helper (oswpt2, future_ships, object_flag, ship_flag, parse_obj_flag, ai_flag, set_flag);
        }

        break;

    // finally! If we actually have a ship, we can set its flags!
    case OSWPT_TYPE_SHIP:
        // save flags for state change comparisons
        object_flag_orig = oswpt.objp->flags;

        // see if we have an object flag to set
        if (object_flag != Object::Object_Flags::NUM_VALUES) {
            auto tmp_flagset = oswpt.objp->flags;
            // set or clear?
            tmp_flagset.set (object_flag, set_flag);

            obj_set_flags (oswpt.objp, tmp_flagset);
        }

        // handle ETS when modifying shields
        if (object_flag == Object::Object_Flags::No_shields) {
            if (set_flag) { zero_one_ets (&oswpt.shipp->shield_recharge_index, &oswpt.shipp->weapon_recharge_index, &oswpt.shipp->engine_recharge_index); }
            else if (object_flag_orig[Object::Object_Flags::No_shields]) { set_default_recharge_rates (oswpt.objp); }
        }

        // see if we have a ship flag to set
        if (ship_flag != Ship::Ship_Flags::NUM_VALUES) {
            // set or clear?
            oswpt.shipp->flags.set ((Ship::Ship_Flags)ship_flag, set_flag);
        }

        // the lock afterburner SEXP also needs to set a physics flag
        if (ship_flag == Ship::Ship_Flags::Afterburner_locked) {
            if (set_flag) { afterburners_stop (oswpt.objp, 1); }
        }

        // see if we have an ai flag to set
        if (ai_flag != AI::AI_Flags::NUM_VALUES) {
            // set or clear?
            Ai_info[oswpt.shipp->ai_index].ai_flags.set (ai_flag, set_flag);
        }

    case OSWPT_TYPE_PARSE_OBJECT:
        if (!future_ships) { return; }

        // see if we have a p_object flag to set
        if (parse_obj_flag != Mission::Parse_Object_Flags::NUM_VALUES && oswpt.p_objp != NULL) { oswpt.p_objp->flags.set (parse_obj_flag, set_flag); }
        break;
    }
}

// modified by Goober5000; now it should work properly
// function to deal with breaking/fixing the warp engines on ships/wings.
// --repairable is true when we are breaking the warp drive (can be repaired)
// --damage_it is true when we are sabotaging it, one way or the other; false
// when fixing it
void sexp_deal_with_warp (int n, bool repairable, bool damage_it) {
    Ship::Ship_Flags ship_flag;
    Mission::Parse_Object_Flags p_object_flag;

    if (repairable) {
        ship_flag = Ship::Ship_Flags::Warp_broken;
        p_object_flag = Mission::Parse_Object_Flags::SF_Warp_broken;
    }
    else {
        ship_flag = Ship::Ship_Flags::Warp_never;
        p_object_flag = Mission::Parse_Object_Flags::SF_Warp_never;
        ;
    }

    sexp_deal_with_ship_flag (n, true, Object::Object_Flags::NUM_VALUES, ship_flag, p_object_flag, damage_it);
}

/**
 * Tell the AI when it is okay to fire certain secondary weapons at other
 * ships.
 */
void sexp_good_secondary_time (int n) {
    char *team_name, *weapon_name, *ship_name;
    int num_weapons, weapon_index, team;

    team_name = CTEXT (n);
    num_weapons = eval_num (CDR (n));
    weapon_name = CTEXT (CDR (CDR (n)));
    ship_name = CTEXT (CDR (CDR (CDR (n))));

    weapon_index = weapon_info_lookup (weapon_name);
    if (weapon_index == -1) {
        WARNINGF (LOCATION, "couldn't find weapon %s for good-secondary-time", weapon_name);
        return;
    }

    // get the team type from the team_name
    team = iff_lookup (team_name);

    // see if the ship has departed or has been destroyed.  If so, then we
    // don't need to set up the AI stuff
    if (mission_log_get_time (LOG_SHIP_DEPARTED, ship_name, NULL, NULL) || mission_log_get_time (LOG_SHIP_DESTROYED, ship_name, NULL, NULL) ||
        mission_log_get_time (LOG_SELF_DESTRUCTED, ship_name, NULL, NULL))
        return;

    ai_good_secondary_time (team, weapon_index, num_weapons, ship_name);
}

/**
 * Gets status of goals for previous missions (in the current campaign).
 *
 * @param n Sexp node number
 * @param status tell this function if we are looking for a goal_satisfied,
 * goal_failed, or goal incomplete event
 */
int sexp_previous_goal_status (int n, int status) {
    int rval = 0;
    char *goal_name, *mission_name;
    int i, mission_num;
    bool default_value = false, use_defaults = true;

    mission_name = CTEXT (n);
    goal_name = CTEXT (CDR (n));

    // check for possible next optional argument
    n = CDR (CDR (n));
    if (n != -1) { default_value = is_sexp_true (n); }

    // try to find the given mission name in the current list of missions in
    // the campaign.
    if (Game_mode & GM_CAMPAIGN_MODE) {
        i = mission_campaign_find_mission (mission_name);

        if (i == -1) {
            // if mission not found, assume that goal was false (so
            // previous-goal-false returns true)
            WARNINGF (
                LOCATION, "Couldn't find mission name \"%s\" in current campaign's list of missions.\nReturning %s for goal-status function.", mission_name,
                (status == GOAL_COMPLETE) ? "false" : "true");
            if (status == GOAL_COMPLETE)
                rval = SEXP_KNOWN_FALSE;
            else
                rval = SEXP_KNOWN_TRUE;

            use_defaults = false;
        }
        else if (Campaign.missions[i].flags & CMISSION_FLAG_SKIPPED) { use_defaults = true; }
        else {
            // now try and find the goal this mission
            mission_num = i;
            for (i = 0; i < Campaign.missions[mission_num].num_goals; i++) {
                if (!strcasecmp (Campaign.missions[mission_num].goals[i].name, goal_name)) break;
            }

            if (i == Campaign.missions[mission_num].num_goals) {
                WARNINGF (
                    LOCATION, "Couldn't find goal name \"%s\" in mission %s.\nReturning %s for goal-true function.", goal_name, mission_name,
                    (status == GOAL_COMPLETE) ? "false" : "true");
                if (status == GOAL_COMPLETE)
                    rval = SEXP_KNOWN_FALSE;
                else
                    rval = SEXP_KNOWN_TRUE;
            }
            else {
                // now return KNOWN_TRUE or KNOWN_FALSE based on the status
                // field in the goal structure
                if (Campaign.missions[mission_num].goals[i].status == status)
                    rval = SEXP_KNOWN_TRUE;
                else
                    rval = SEXP_KNOWN_FALSE;
            }

            use_defaults = false;
        }
    }

    if (use_defaults) {
        // when not in campaign mode, always return KNOWN_TRUE when looking for
        // goal complete, and KNOWN_FALSE otherwise
        if (n != -1) {
            if (default_value)
                rval = SEXP_KNOWN_TRUE;
            else
                rval = SEXP_KNOWN_FALSE;
        }
        else {
            if (status == GOAL_COMPLETE)
                rval = SEXP_KNOWN_TRUE;
            else
                rval = SEXP_KNOWN_FALSE;
        }
    }

    return rval;
}

// sexpression which gets the status of an event from a previous mission.  Like
// the above function but dealing with events instead of goals.  Again, the
// status parameter tells the code if we are looking for an event_true,
// event_false, or event_incomplete status
int sexp_previous_event_status (int n, int status) {
    int rval = 0;
    char *name, *mission_name;
    int i, mission_num;
    bool default_value = false, use_defaults = true;

    mission_name = CTEXT (n);
    name = CTEXT (CDR (n));

    // check for possible optional parameter
    n = CDR (CDR (n));
    if (n != -1) { default_value = is_sexp_true (n); }

    if (Game_mode & GM_CAMPAIGN_MODE) {
        // following function returns -1 when mission isn't found.
        i = mission_campaign_find_mission (mission_name);

        // if the mission name wasn't found -- make this return FALSE for the
        // event status.
        if (i == -1) {
            WARNINGF (
                LOCATION, "Couldn't find mission name \"%s\" in current campaign's list of missions.\nReturning %s for event-status function.", mission_name,
                (status == EVENT_SATISFIED) ? "false" : "true");
            if (status == EVENT_SATISFIED) { rval = SEXP_KNOWN_FALSE; }
            else { rval = SEXP_KNOWN_TRUE; }

            use_defaults = false;
        }
        else if (Campaign.missions[i].flags & CMISSION_FLAG_SKIPPED) { use_defaults = true; }
        else {
            // now try and find the goal this mission
            mission_num = i;
            for (i = 0; i < Campaign.missions[mission_num].num_events; i++) {
                if (!strcasecmp (Campaign.missions[mission_num].events[i].name, name)) break;
            }

            if (i == Campaign.missions[mission_num].num_events) {
                WARNINGF (
                    LOCATION, "Couldn't find event name \"%s\" in mission %s.\nReturning %s for event_status function.", name, mission_name,
                    (status == EVENT_SATISFIED) ? "false" : "true");
                if (status == EVENT_SATISFIED)
                    rval = SEXP_KNOWN_FALSE;
                else
                    rval = SEXP_KNOWN_TRUE;
            }
            else {
                // now return KNOWN_TRUE or KNOWN_FALSE based on the status
                // field in the goal structure
                if (Campaign.missions[mission_num].events[i].status == status)
                    rval = SEXP_KNOWN_TRUE;
                else
                    rval = SEXP_KNOWN_FALSE;
            }

            use_defaults = false;
        }
    }

    if (use_defaults) {
        if (n != -1) {
            if (default_value)
                rval = SEXP_KNOWN_TRUE;
            else
                rval = SEXP_KNOWN_FALSE;
        }
        else {
            if (status == EVENT_SATISFIED)
                rval = SEXP_KNOWN_TRUE;
            else
                rval = SEXP_KNOWN_FALSE;
        }
    }

    return rval;
}

/**
 * Return the status of an event in the current mission.
 *
 * @param n Sexp node number
 * @param want_true indicates if we are checking whether the event is true or
 * the event is false.
 */
int sexp_event_status (int n, int want_true) {
    char* name;
    int i, result;

    name = CTEXT (n);
    ASSERTX (name != nullptr, "CTEXT returned NULL for node %d!", n);

    for (i = 0; i < Num_mission_events; i++) {
        // look for the event name, check it's status.  If formula is gone, we
        // know the state won't ever change.
        if (!strcasecmp (Mission_events[i].name, name)) {
            result = Mission_events[i].result;
            if (Mission_events[i].formula < 0) {
                if ((want_true && result) || (!want_true && !result))
                    return SEXP_KNOWN_TRUE;
                else
                    return SEXP_KNOWN_FALSE;
            }
            else {
                if ((want_true && result) || (!want_true && !result))
                    return SEXP_TRUE;
                else
                    return SEXP_FALSE;
            }
        }
    }

    return SEXP_FALSE;
}

/**
 * Return the status of an event N seconds after the event is true or false.
 *
 * Similar to above function but waits N seconds before returning true
 */
int sexp_event_delay_status (int n, int want_true, bool use_msecs = false) {
    char* name;
    int i, result;
    fix delay;
    int rval = SEXP_FALSE;
    bool use_as_directive = false;

    name = CTEXT (n);
    ASSERTX (name != nullptr, "CTEXT returned NULL for node %d!", n);

    if (use_msecs) {
        uint64_t tempDelay = eval_num (CDR (n));
        tempDelay = tempDelay << 16;
        tempDelay = tempDelay / 1000;

        delay = (fix)tempDelay;
    }
    else { delay = i2f (eval_num (CDR (n))); }

    for (i = 0; i < Num_mission_events; i++) {
        // look for the event name, check it's status.  If formula is gone, we
        // know the state won't ever change.
        if (!strcasecmp (Mission_events[i].name, name)) {
            if ((fix)Mission_events[i].timestamp + delay >= Missiontime) {
                rval = SEXP_FALSE;
                break;
            }

            result = Mission_events[i].result;
            if (Mission_events[i].formula < 0) {
                if ((want_true && result) || (!want_true && !result)) {
                    rval = SEXP_KNOWN_TRUE;
                    break;
                }
                else {
                    rval = SEXP_KNOWN_FALSE;
                    break;
                }
            }
            else {
                if (want_true && result) { //) || (!want_true && !result) )
                    rval = SEXP_TRUE;
                    break;
                }
                else {
                    rval = SEXP_FALSE;
                    break;
                }
            }
        }
    }

    // check for possible optional parameter
    n = CDDR (n);
    if (n != -1) use_as_directive = is_sexp_true (n);

    // zero out Sexp_useful_number if it's not true and we don't want this for
    // specific directive use
    if (!use_as_directive && (rval != SEXP_TRUE) && (rval != SEXP_KNOWN_TRUE)) Sexp_useful_number = 0; // indicate sexp isn't current yet

    return rval;
}

/**
 * Returns true if the given event is still incomplete
 */
int sexp_event_incomplete (int n) {
    char* name;
    int i;

    name = CTEXT (n);
    ASSERTX (name != nullptr, "CTEXT returned NULL for node %d!", n);

    for (i = 0; i < Num_mission_events; i++) {
        if (!strcasecmp (Mission_events[i].name, name)) {
            // if the formula is still >= 0 (meaning it is still getting
            // eval'ed), then the event is incomplete
            if (Mission_events[i].formula != -1)
                return SEXP_TRUE;
            else
                return SEXP_KNOWN_FALSE;
        }
    }

    return SEXP_FALSE;
}

/**
 * Return the status of an goal N seconds after the goal is true or false.
 *
 * Similar to above function but operates on goals instead of events
 */
int sexp_goal_delay_status (int n, int want_true) {
    char* name;
    fix delay, time;

    name = CTEXT (n);
    delay = i2f (eval_num (CDR (n)));

    if (want_true) {
        // if we are looking for a goal true entry and we find a false, then
        // return known false here
        if (mission_log_get_time (LOG_GOAL_FAILED, name, NULL, NULL))
            return SEXP_KNOWN_FALSE;
        else if (mission_log_get_time (LOG_GOAL_SATISFIED, name, NULL, &time)) {
            if ((Missiontime - time) >= delay) return SEXP_KNOWN_TRUE;
        }
    }
    else {
        // if we are looking for a goal false entry and we find a true, then
        // return known false here
        if (mission_log_get_time (LOG_GOAL_SATISFIED, name, NULL, NULL))
            return SEXP_KNOWN_FALSE;
        else if (mission_log_get_time (LOG_GOAL_FAILED, name, NULL, &time)) {
            if ((Missiontime - time) >= delay) return SEXP_KNOWN_TRUE;
        }
    }

    return SEXP_FALSE;
}

/**
 * Returns true if the given goal is still incomplete
 */
int sexp_goal_incomplete (int n) {
    char* name;

    name = CTEXT (n);

    if (mission_log_get_time (LOG_GOAL_SATISFIED, name, NULL, NULL) || mission_log_get_time (LOG_GOAL_FAILED, name, NULL, NULL))
        return SEXP_KNOWN_FALSE;
    else
        return SEXP_TRUE;
}

/**
 * Protects/unprotects a ship.
 *
 * @param n Sexp node number
 * @param flag Whether or not the protect bit should be set (flag==true) or
 * cleared (flag==false)
 */
void sexp_protect_ships (int n, bool flag) {
    sexp_deal_with_ship_flag (n, true, Object::Object_Flags::Protected, Ship::Ship_Flags::NUM_VALUES, Mission::Parse_Object_Flags::OF_Protected, flag);
}

/**
 * Protects/unprotects a ship from beams.
 *
 * @param n Sexp node number
 * @param flag Whether or not the protect bit should be set (flag==true) or
 * cleared (flag==false)
 */
void sexp_beam_protect_ships (int n, bool flag) {
    sexp_deal_with_ship_flag (n, true, Object::Object_Flags::Beam_protected, Ship::Ship_Flags::NUM_VALUES, Mission::Parse_Object_Flags::OF_Beam_protected, flag);
}

/**
 * Make ships "visible" and "invisible" to sensors.
 *
 * @param n Sexp node number
 * @param visible Is true when making ships visible, false otherwise
 */
void sexp_ships_visible (int n, bool visible) {
    sexp_deal_with_ship_flag (
        n, true, Object::Object_Flags::NUM_VALUES, Ship::Ship_Flags::Hidden_from_sensors, Mission::Parse_Object_Flags::SF_Hidden_from_sensors, !visible, true);

    // we also have to add any escort ships that were made visible
    for (; n >= 0; n = CDR (n)) {
        int shipnum = ship_name_lookup (CTEXT (n));
        if (shipnum < 0) continue;

        if (!visible && Player_ai->target_objnum == Ships[shipnum].objnum) { hud_cease_targeting (); }
        else if (visible && (Ships[shipnum].flags[Ship::Ship_Flags::Escort])) { hud_add_ship_to_escort (Ships[shipnum].objnum, 1); }
    }
}

// sexpression to toggle invulnerability flag of ships.
void sexp_ships_invulnerable (int n, bool invulnerable) {
    sexp_deal_with_ship_flag (n, true, Object::Object_Flags::Invulnerable, Ship::Ship_Flags::NUM_VALUES, Mission::Parse_Object_Flags::OF_Invulnerable, invulnerable);
}

// sexpression to toggle KEEP ALIVE flag of ship object
void sexp_ships_guardian (int n, int guardian) {
    char* ship_name;
    int num;

    for (; n != -1; n = CDR (n)) {
        ship_name = CTEXT (n);

        // check to see if ship destroyed or departed.  In either case, do
        // nothing.
        if (mission_log_get_time (LOG_SHIP_DEPARTED, ship_name, NULL, NULL) || mission_log_get_time (LOG_SHIP_DESTROYED, ship_name, NULL, NULL) ||
            mission_log_get_time (LOG_SELF_DESTRUCTED, ship_name, NULL, NULL))
            continue;

        // get the ship num.  If we get a -1 for the number here, ship has yet
        // to arrive.  Store this ship in a list until created
        num = ship_name_lookup (ship_name);
        if (num != -1) { Ships[num].ship_guardian_threshold = guardian ? SHIP_GUARDIAN_THRESHOLD_DEFAULT : 0; }
        else {
            p_object* p_objp = mission_parse_get_arrival_ship (ship_name);
            if (p_objp) { p_objp->flags.set (Mission::Parse_Object_Flags::SF_Guardian, guardian != 0); }
        }
    }
}

// make ship vanish without a trace (and what its docked to)
void sexp_ship_vanish (int n) {
    char* ship_name;
    int num;

    for (; n != -1; n = CDR (n)) {
        ship_name = CTEXT (n);

        // check to see if ship destroyed or departed.  In either case, do
        // nothing.
        if (mission_log_get_time (LOG_SHIP_DEPARTED, ship_name, NULL, NULL) || mission_log_get_time (LOG_SHIP_DESTROYED, ship_name, NULL, NULL) ||
            mission_log_get_time (LOG_SELF_DESTRUCTED, ship_name, NULL, NULL))
            continue;

        // get the ship num.  If we get a -1 for the number here, ship has yet
        // to arrive
        num = ship_name_lookup (ship_name);
        if (num != -1) ship_actually_depart (num, SHIP_VANISHED);
    }
}

// Goober5000
void sexp_ingame_ship_kamikaze (ship* shipp, int kdamage) {
    ASSERTX (shipp, "Invalid ship pointer passed to sexp_ingame_ship_kamikaze.\n");
    ASSERTX (
        kdamage >= 0,
        "Invalid value passed to sexp_ingame_ship_kamikaze. Kamikaze damage "
        "must be >= 0, is %i.\n",
        kdamage);

    ai_info* aip = &Ai_info[shipp->ai_index];

    aip->ai_flags.set (AI::AI_Flags::Kamikaze, kdamage > 0);
    aip->kamikaze_damage = kdamage;
}

int sexp_key_pressed (int node) {
    int z, t;

    ASSERT (node != -1);
    z = translate_key_to_index (CTEXT (node), false);
    if (z < 0) { return SEXP_FALSE; }

    if (!Control_config[z].used) { return SEXP_FALSE; }

    if (CDR (node) < 0) { return SEXP_TRUE; }

    t = eval_num (CDR (node));
    return timestamp_has_time_elapsed (Control_config[z].used, t * 1000);
}

void sexp_key_reset (int node) {
    int n, z;

    for (n = node; n != -1; n = CDR (n)) {
        z = translate_key_to_index (CTEXT (n), false);
        if (z >= 0) Control_config[z].used = 0;
    }
}

int sexp_targeted (int node) {
    int z;
    ship_subsys* ptr;

    z = ship_query_state (CTEXT (node));
    if (z == 1) {
        return SEXP_KNOWN_FALSE; // ship isn't around, nor will it ever be
    }
    else if (z == -1) { return SEXP_CANT_EVAL; }

    z = ship_name_lookup (CTEXT (node), 1);
    if ((z < 0) || !Player_ai || (Ships[z].objnum != Players_target)) { return SEXP_FALSE; }

    if (CDR (node) >= 0) {
        z = eval_num (CDR (node)) * 1000;
        if (!timestamp_has_time_elapsed (Players_target_timestamp, z)) { return SEXP_FALSE; }

        if (CDR (CDR (node)) >= 0) {
            ptr = Players_targeted_subsys;
            if (!ptr || subsystem_strcasecmp (ptr->system_info->subobj_name, CTEXT (CDR (CDR (node))))) { return SEXP_FALSE; }
        }
    }

    return SEXP_TRUE;
}

int sexp_node_targeted (int node) {
    int z;

    CJumpNode* jnp = jumpnode_get_by_name (CTEXT (node));

    if (jnp == NULL || !Player_ai || (jnp->GetSCPObjectNumber () != Players_target)) { return SEXP_FALSE; }

    if (CDR (node) >= 0) {
        z = eval_num (CDR (node)) * 1000;
        if (!timestamp_has_time_elapsed (Players_target_timestamp, z)) { return SEXP_FALSE; }
    }

    return SEXP_TRUE;
}

int sexp_speed (int node) {
    if (Training_context & TRAINING_CONTEXT_SPEED) {
        if (Training_context_speed_set) {
            if (timestamp_has_time_elapsed (Training_context_speed_timestamp, eval_num (node) * 1000)) { return SEXP_KNOWN_TRUE; }
        }
    }

    return SEXP_FALSE;
}

int sexp_secondaries_depleted (int node) {
    int sindex, num_banks, num_depleted_banks;
    ship* shipp;

    // get ship
    sindex = ship_name_lookup (CTEXT (node));
    if (sindex < 0) { return SEXP_FALSE; }

    shipp = &Ships[sindex];
    if (shipp->objnum < 0) { return SEXP_FALSE; }

    // get num secondary banks
    num_banks = shipp->weapons.num_secondary_banks;
    num_depleted_banks = 0;

    // get number of depleted banks
    for (int idx = 0; idx < num_banks; idx++) {
        // is this bank out of ammo?
        if (shipp->weapons.secondary_bank_ammo[idx] == 0) { num_depleted_banks++; }
    }

    // are they all depleted?
    return (num_depleted_banks == num_banks) ? SEXP_TRUE : SEXP_FALSE;
}

int sexp_facing (int node) {
    float a1, a2;
    vec3d v1, v2;

    if (!Player_obj) { return SEXP_FALSE; }

    ship* target_shipp = sexp_get_ship_from_node (node);
    if (target_shipp == NULL) {
        // hasn't arrived yet
        if (mission_parse_get_arrival_ship (CTEXT (node)) != NULL) { return SEXP_CANT_EVAL; }
        // not found and won't arrive: invalid
        return SEXP_KNOWN_FALSE;
    }
    double angle = atof (CTEXT (CDR (node)));

    v1 = Player_obj->orient.vec.fvec;
    vm_vec_normalize (&v1);

    vm_vec_sub (&v2, &Objects[target_shipp->objnum].pos, &Player_obj->pos);
    vm_vec_normalize (&v2);

    a1 = vm_vec_dot (&v1, &v2);
    a2 = cosf (to_radians (angle));
    if (a1 >= a2) { return SEXP_TRUE; }

    return SEXP_FALSE;
}

// is ship facing first waypoint in waypoint path
int sexp_facing2 (int node) {
    float a1, a2;
    vec3d v1, v2;

    // bail if Player_obj is not good
    if (!Player_obj) { return SEXP_CANT_EVAL; }

    // get player fvec
    v1 = Player_obj->orient.vec.fvec;
    vm_vec_normalize (&v1);

    // get waypoint name
    char* waypoint_name = CTEXT (node);

    // get position of first waypoint
    waypoint_list* wp_list = find_matching_waypoint_list (waypoint_name);

    if (wp_list == NULL) { return SEXP_CANT_EVAL; }

    vm_vec_sub (&v2, wp_list->get_waypoints ().front ().get_pos (), &Player_obj->pos);
    vm_vec_normalize (&v2);
    a1 = vm_vec_dot (&v1, &v2);
    a2 = cosf (to_radians (atof (CTEXT (CDR (node)))));
    if (a1 >= a2) { return SEXP_TRUE; }

    return SEXP_FALSE;
}

int sexp_order (int n) {
    char* order_to = CTEXT (n);
    char* order = CTEXT (CDR (n));
    char* target = NULL;

    // target
    n = CDDR (n);
    if (n != -1) target = CTEXT (n);

    return hud_query_order_issued (order_to, order, target);
}

int sexp_waypoint_missed () {
    if (Training_context & TRAINING_CONTEXT_FLY_PATH) {
        if (Training_context_at_waypoint > Training_context_goal_waypoint) { return SEXP_TRUE; }
    }

    return SEXP_FALSE;
}

int sexp_waypoint_twice () {
    if (Training_context & TRAINING_CONTEXT_FLY_PATH) {
        if (Training_context_at_waypoint < Training_context_goal_waypoint - 1) { return SEXP_TRUE; }
    }

    return SEXP_FALSE;
}

int sexp_path_flown () {
    if (Training_context & TRAINING_CONTEXT_FLY_PATH) {
        if ((uint)Training_context_goal_waypoint == Training_context_path->get_waypoints ().size ()) { return SEXP_TRUE; }
    }

    return SEXP_FALSE;
}

void sexp_send_training_message (int node) {
    int t = -1, delay = 0;

    if (physics_paused) { return; }

    ASSERT (node >= 0);
    ASSERT (Event_index >= 0);

    if ((CDR (node) >= 0) && (CDR (CDR (node)) >= 0)) {
        delay = eval_num (CDR (CDR (node))) * 1000;
        t = CDR (CDR (CDR (node)));
        if (t >= 0) { t = eval_num (t); }
    }

    if ((Mission_events[Event_index].repeat_count > 1) || (CDR (node) < 0)) { message_training_queue (CTEXT (node), timestamp (delay), t); }
    else { message_training_queue (CTEXT (CDR (node)), timestamp (delay), t); }
}

int sexp_shield_recharge_pct (int node) {
    int sindex;

    // get the firing ship
    sindex = ship_name_lookup (CTEXT (node));
    if (sindex < 0) { return 0; }
    if (Ships[sindex].objnum < 0) { return 0; }

    // shield recharge pct
    return (int)(100.0f * Energy_levels[Ships[sindex].shield_recharge_index]);
}

int sexp_engine_recharge_pct (int node) {
    int sindex;

    // get the firing ship
    sindex = ship_name_lookup (CTEXT (node));
    if (sindex < 0) { return 0; }
    if (Ships[sindex].objnum < 0) { return 0; }

    // shield recharge pct
    return (int)(100.0f * Energy_levels[Ships[sindex].engine_recharge_index]);
}

int sexp_weapon_recharge_pct (int node) {
    int sindex;

    // get the firing ship
    sindex = ship_name_lookup (CTEXT (node));
    if (sindex < 0) { return 0; }
    if (Ships[sindex].objnum < 0) { return 0; }

    // shield recharge pct
    return (int)(100.0f * Energy_levels[Ships[sindex].weapon_recharge_index]);
}

int sexp_shield_quad_low (int node) {
    int sindex, idx;
    float max_quad, check;
    ship_info* sip;
    object* objp;

    // get the ship
    sindex = ship_name_lookup (CTEXT (node));
    if (sindex < 0) { return SEXP_FALSE; }
    if ((Ships[sindex].objnum < 0) || (Ships[sindex].objnum >= MAX_OBJECTS)) { return SEXP_FALSE; }
    if ((Ships[sindex].ship_info_index < 0) || (Ships[sindex].ship_info_index >= static_cast< int > (Ship_info.size ()))) { return SEXP_FALSE; }
    objp = &Objects[Ships[sindex].objnum];
    sip = &Ship_info[Ships[sindex].ship_info_index];
    if (!(sip->is_small_ship ())) { return SEXP_FALSE; }
    max_quad = shield_get_max_quad (objp);

    // shield pct
    check = (float)eval_num (CDR (node));

    // check his quadrants
    for (idx = 0; idx < objp->n_quadrants; idx++) {
        if (((objp->shield_quadrant[idx] / max_quad) * 100.0f) <= check) { return SEXP_TRUE; }
    }

    // all good
    return SEXP_FALSE;
}

int sexp_secondary_ammo_pct (int node) {
    ship* shipp;
    int sindex;
    int check, idx;
    int ret_sum[MAX_SHIP_SECONDARY_BANKS];
    int ret = 0;

    // get the ship
    sindex = ship_name_lookup (CTEXT (node));
    if (sindex < 0) { return 0; }
    if ((Ships[sindex].objnum < 0) || (Ships[sindex].objnum >= MAX_OBJECTS)) { return 0; }
    shipp = &Ships[sindex];

    // bank to check
    check = eval_num (CDR (node));

    // bogus check?
    if (check < 0) { return 0; }

    // cumulative sum?
    if (check >= shipp->weapons.num_secondary_banks) {
        for (idx = 0; idx < shipp->weapons.num_secondary_banks; idx++) {
            ret_sum[idx] = (int)(((float)shipp->weapons.secondary_bank_ammo[idx] / (float)shipp->weapons.secondary_bank_start_ammo[idx]) * 100.0f);
        }

        // add it up
        ret = 0;
        for (idx = 0; idx < shipp->weapons.num_secondary_banks; idx++) { ret += ret_sum[idx]; }
        ret = (int)((float)ret / (float)shipp->weapons.num_secondary_banks);
    }
    else { ret = (int)(((float)shipp->weapons.secondary_bank_ammo[check] / (float)shipp->weapons.secondary_bank_start_ammo[check]) * 100.0f); }

    // return
    return ret;
}

extern int insert_subsys_status (p_object* pobjp);

void sexp_beam_fire (int node, bool at_coords) {
    int sindex, n = node;
    beam_fire_info fire_info;
    int idx;

    // zero stuff out
    memset (&fire_info, 0, sizeof (beam_fire_info));
    fire_info.accuracy = 0.000001f; // this will guarantee a hit

    // get the firing ship
    sindex = ship_name_lookup (CTEXT (n));
    n = CDR (n);
    if (sindex < 0) { return; }
    if (Ships[sindex].objnum < 0) { return; }
    fire_info.shooter = &Objects[Ships[sindex].objnum];

    // get the subsystem
    fire_info.turret = ship_get_subsys (&Ships[sindex], CTEXT (n));
    n = CDR (n);
    if (fire_info.turret == NULL) { return; }

    if (at_coords) {
        // get the target coordinates
        fire_info.target_pos1.xyz.x = fire_info.target_pos2.xyz.x = static_cast< float > (eval_num (n));
        n = CDR (n);
        fire_info.target_pos1.xyz.y = fire_info.target_pos2.xyz.y = static_cast< float > (eval_num (n));
        n = CDR (n);
        fire_info.target_pos1.xyz.z = fire_info.target_pos2.xyz.z = static_cast< float > (eval_num (n));
        n = CDR (n);
        fire_info.bfi_flags |= BFIF_TARGETING_COORDS;
        fire_info.target = NULL;
        fire_info.target_subsys = NULL;
    }
    else {
        // get the target
        sindex = ship_name_lookup (CTEXT (n));
        n = CDR (n);
        if (sindex < 0) { return; }
        if (Ships[sindex].objnum < 0) { return; }
        fire_info.target = &Objects[Ships[sindex].objnum];

        // see if the optional subsystem can be found
        fire_info.target_subsys = NULL;
        if (n >= 0) {
            fire_info.target_subsys = ship_get_subsys (&Ships[sindex], CTEXT (n));
            n = CDR (n);
        }
    }

    // optionally force firing
    if (n >= 0 && is_sexp_true (n)) {
        fire_info.bfi_flags |= BFIF_FORCE_FIRING;
        n = CDR (n);
    }

    // get the second set of coordinates
    if (at_coords) {
        if (n >= 0) {
            fire_info.target_pos2.xyz.x = static_cast< float > (eval_num (n));
            n = CDR (n);
        }
        if (n >= 0) {
            fire_info.target_pos2.xyz.y = static_cast< float > (eval_num (n));
            n = CDR (n);
        }
        if (n >= 0) {
            fire_info.target_pos2.xyz.z = static_cast< float > (eval_num (n));
            n = CDR (n);
        }
    }

    // --- done getting arguments ---

    // if it has no primary weapons
    if (fire_info.turret->weapons.num_primary_banks <= 0) {
        WARNINGF (LOCATION, "Couldn't fire turret on ship %s; subsystem %s has no primary weapons", CTEXT (node), CTEXT (CDR (node)));
        return;
    }

    // if the turret is destroyed
    if (!(fire_info.bfi_flags & BFIF_FORCE_FIRING) && fire_info.turret->current_hits <= 0.0f) { return; }

    // hmm, this could be wacky. Let's just simply select the first beam weapon
    // in the turret
    fire_info.beam_info_index = -1;
    for (idx = 0; idx < fire_info.turret->weapons.num_primary_banks; idx++) {
        ASSERTX (
            fire_info.turret->weapons.primary_bank_weapons[idx] >= 0 && fire_info.turret->weapons.primary_bank_weapons[idx] < MAX_WEAPON_TYPES,
            "sexp_beam_fire: found invalid weapon index (%i), get a coder\n!", fire_info.turret->weapons.primary_bank_weapons[idx]);
        // store the weapon info index
        if (Weapon_info[fire_info.turret->weapons.primary_bank_weapons[idx]].wi_flags[Weapon::Info_Flags::Beam]) {
            fire_info.beam_info_index = fire_info.turret->weapons.primary_bank_weapons[idx];
        }
    }

    // fire the beam
    if (fire_info.beam_info_index != -1) { beam_fire (&fire_info); }
    else {
        // it would appear the turret doesn't have any beam weapons
        WARNINGF (LOCATION, "Couldn't fire turret on ship %s; subsystem %s has no beam weapons", CTEXT (node), CTEXT (CDR (node)));
    }
}

void sexp_beam_free (int node) {
    int sindex;
    ship_subsys* turret = NULL;

    // get the firing ship
    sindex = ship_name_lookup (CTEXT (node));
    if (sindex < 0) { return; }
    if (Ships[sindex].objnum < 0) { return; }

    node = CDR (node);
    for (; node >= 0; node = CDR (node)) {
        // get the subsystem
        turret = ship_get_subsys (&Ships[sindex], CTEXT (node));
        if (turret == NULL) { continue; }

        // flag it as beam free :)
        if (!(turret->weapons.flags[Ship::Weapon_Flags::Beam_Free])) {
            turret->weapons.flags.set (Ship::Weapon_Flags::Beam_Free);
            turret->turret_next_fire_stamp = timestamp ((int)fs2::prng::randf (0, 50.0f, 4000.0f));
        }
    }
}

void sexp_beam_free_all (int node) {
    ship_subsys* subsys;
    int sindex;

    for (int n = node; n >= 0; n = CDR (n)) {
        // get the firing ship
        sindex = ship_name_lookup (CTEXT (n));

        if (sindex < 0) { continue; }

        if (Ships[sindex].objnum < 0) { continue; }

        // free all beam weapons
        subsys = GET_FIRST (&Ships[sindex].subsys_list);

        while (subsys != END_OF_LIST (&Ships[sindex].subsys_list)) {
            // just mark all turrets as beam free
            if ((subsys->system_info->type == SUBSYSTEM_TURRET) && (!(subsys->weapons.flags[Ship::Weapon_Flags::Beam_Free]))) {
                subsys->weapons.flags.set (Ship::Weapon_Flags::Beam_Free);
                subsys->turret_next_fire_stamp = timestamp ((int)fs2::prng::randf (0, 50.0f, 4000.0f));
            }

            // next item
            subsys = GET_NEXT (subsys);
        }
    }
}

void sexp_beam_lock (int node) {
    int sindex;
    ship_subsys* turret = NULL;

    // get the firing ship
    sindex = ship_name_lookup (CTEXT (node));
    if (sindex < 0) { return; }
    if (Ships[sindex].objnum < 0) { return; }

    node = CDR (node);
    for (; node >= 0; node = CDR (node)) {
        // get the subsystem
        turret = ship_get_subsys (&Ships[sindex], CTEXT (node));
        if (turret == NULL) { continue; }

        // flag it as not beam free
        turret->weapons.flags.remove (Ship::Weapon_Flags::Beam_Free);
    }
}

void sexp_beam_lock_all (int node) {
    ship_subsys* subsys;
    int sindex;

    for (int n = node; n >= 0; n = CDR (n)) {
        // get the firing ship
        sindex = ship_name_lookup (CTEXT (n));

        if (sindex < 0) { continue; }

        if (Ships[sindex].objnum < 0) { continue; }

        // lock all beam weapons
        subsys = GET_FIRST (&Ships[sindex].subsys_list);

        while (subsys != END_OF_LIST (&Ships[sindex].subsys_list)) {
            // just mark all turrets as not beam free
            if (subsys->system_info->type == SUBSYSTEM_TURRET) { subsys->weapons.flags.remove (Ship::Weapon_Flags::Beam_Free); }

            // next item
            subsys = GET_NEXT (subsys);
        }
    }
}

void sexp_turret_free (int node) {
    int sindex;
    ship_subsys* turret = NULL;

    // get the firing ship
    sindex = ship_name_lookup (CTEXT (node));
    if (sindex < 0) { return; }
    if (Ships[sindex].objnum < 0) { return; }

    node = CDR (node);
    for (; node >= 0; node = CDR (node)) {
        // get the subsystem
        turret = ship_get_subsys (&Ships[sindex], CTEXT (node));
        if (turret == NULL) { continue; }

        // flag turret as no longer locked :)
        if (turret->weapons.flags[Ship::Weapon_Flags::Turret_Lock]) {
            turret->weapons.flags.remove (Ship::Weapon_Flags::Turret_Lock);
            turret->turret_next_fire_stamp = timestamp ((int)fs2::prng::randf (0, 50.0f, 4000.0f));
        }
    }
}

void sexp_turret_free_all (int node) {
    ship_subsys* subsys;
    int sindex;

    for (int n = node; n >= 0; n = CDR (n)) {
        // get the firing ship
        sindex = ship_name_lookup (CTEXT (n));

        if (sindex < 0) { continue; }

        if (Ships[sindex].objnum < 0) { continue; }

        // free all turrets
        subsys = GET_FIRST (&Ships[sindex].subsys_list);

        while (subsys != END_OF_LIST (&Ships[sindex].subsys_list)) {
            // just mark all turrets as free
            if ((subsys->system_info->type == SUBSYSTEM_TURRET) && (subsys->weapons.flags[Ship::Weapon_Flags::Turret_Lock])) {
                subsys->weapons.flags.remove (Ship::Weapon_Flags::Turret_Lock);
                subsys->turret_next_fire_stamp = timestamp ((int)fs2::prng::randf (0, 50.0f, 4000.0f));
            }

            // next item
            subsys = GET_NEXT (subsys);
        }
    }
}

void sexp_turret_lock (int node) {
    int sindex;
    ship_subsys* turret = NULL;

    // get the firing ship
    sindex = ship_name_lookup (CTEXT (node));
    if (sindex < 0) { return; }
    if (Ships[sindex].objnum < 0) { return; }

    node = CDR (node);
    for (; node >= 0; node = CDR (node)) {
        // get the subsystem
        turret = ship_get_subsys (&Ships[sindex], CTEXT (node));
        if (turret == NULL) { continue; }

        // flag turret as locked
        turret->weapons.flags.set (Ship::Weapon_Flags::Turret_Lock);
    }
}

void sexp_turret_lock_all (int node) {
    ship_subsys* subsys;
    int sindex;

    for (int n = node; n >= 0; n = CDR (n)) {
        // get the firing ship
        sindex = ship_name_lookup (CTEXT (n));

        if (sindex < 0) { continue; }

        if (Ships[sindex].objnum < 0) { continue; }

        // lock all turrets
        subsys = GET_FIRST (&Ships[sindex].subsys_list);

        while (subsys != END_OF_LIST (&Ships[sindex].subsys_list)) {
            // just mark all turrets as locked
            if (subsys->system_info->type == SUBSYSTEM_TURRET) { subsys->weapons.flags.set (Ship::Weapon_Flags::Turret_Lock); }

            // next item
            subsys = GET_NEXT (subsys);
        }
    }
}

void sexp_turret_tagged_only_all (int node) {
    ship_subsys* subsys;
    int sindex;

    // get the firing ship
    sindex = ship_name_lookup (CTEXT (node));
    if (sindex < 0) { return; }
    if (Ships[sindex].objnum < 0) { return; }

    // mark all turrets to only target tagged ships
    subsys = GET_FIRST (&Ships[sindex].subsys_list);
    while (subsys != END_OF_LIST (&Ships[sindex].subsys_list)) {
        // just mark all turrets as locked
        if (subsys->system_info->type == SUBSYSTEM_TURRET) { subsys->weapons.flags.set (Ship::Weapon_Flags::Tagged_Only); }

        // next item
        subsys = GET_NEXT (subsys);
    }
}

void sexp_turret_tagged_clear_all (int node) {
    ship_subsys* subsys;
    int sindex;

    // get the firing ship
    sindex = ship_name_lookup (CTEXT (node));
    if (sindex < 0) { return; }
    if (Ships[sindex].objnum < 0) { return; }

    // mark all turrets so not restricted to only tagged ships
    subsys = GET_FIRST (&Ships[sindex].subsys_list);
    while (subsys != END_OF_LIST (&Ships[sindex].subsys_list)) {
        // just mark all turrets as locked
        if (subsys->system_info->type == SUBSYSTEM_TURRET) { subsys->weapons.flags.remove (Ship::Weapon_Flags::Tagged_Only); }

        // next item
        subsys = GET_NEXT (subsys);
    }
}

// Goober5000
int sexp_is_in_turret_fov (int node) {
    char* target_ship_name;
    char* turret_ship_name;
    char* turret_subsys_name;
    int target_shipnum, turret_shipnum, range;
    object *target_objp, *turret_objp;
    ship_subsys* turret_subsys;
    vec3d tpos, tvec;

    target_ship_name = CTEXT (node);
    turret_ship_name = CTEXT (CDR (node));
    turret_subsys_name = CTEXT (CDDR (node));
    range = CDDDR (node) >= 0 ? eval_num (CDDDR (node)) : -1;

    if (sexp_query_has_yet_to_arrive (target_ship_name) || sexp_query_has_yet_to_arrive (turret_ship_name)) return SEXP_CANT_EVAL;

    // if ship is gone or departed, cannot ever evaluate properly.  Return
    // NAN_FOREVER
    if (mission_log_get_time (LOG_SHIP_DESTROYED, target_ship_name, NULL, NULL) || mission_log_get_time (LOG_SHIP_DEPARTED, target_ship_name, NULL, NULL) ||
        mission_log_get_time (LOG_SELF_DESTRUCTED, target_ship_name, NULL, NULL)) {
        return SEXP_NAN_FOREVER;
    }
    if (mission_log_get_time (LOG_SHIP_DESTROYED, turret_ship_name, NULL, NULL) || mission_log_get_time (LOG_SHIP_DEPARTED, turret_ship_name, NULL, NULL) ||
        mission_log_get_time (LOG_SELF_DESTRUCTED, turret_ship_name, NULL, NULL)) {
        return SEXP_NAN_FOREVER;
    }

    // find the two ships...
    target_shipnum = ship_name_lookup (target_ship_name);
    turret_shipnum = ship_name_lookup (turret_ship_name);
    ASSERTX (target_shipnum >= 0, "Couldn't find target ship '%s' in sexp_is_in_turret_fov!", target_ship_name);
    ASSERTX (turret_shipnum >= 0, "Couldn't find turreted ship '%s' in sexp_is_in_turret_fov!", turret_ship_name);

    // ...and their objects
    target_objp = &Objects[Ships[target_shipnum].objnum];
    turret_objp = &Objects[Ships[turret_shipnum].objnum];

    // find the turret
    turret_subsys = ship_get_subsys (&Ships[turret_shipnum], turret_subsys_name);
    if (turret_subsys == nullptr) {
        WARNINGF (LOCATION, "Couldn't find turret subsystem '%s' on ship '%s' in sexp_is_in_turret_fov!", turret_subsys_name, turret_ship_name);
        return SEXP_FALSE;
    }

    // find out where the turret is
    ship_get_global_turret_info (turret_objp, turret_subsys->system_info, &tpos, &tvec);

    // see how far away is the target (this isn't used for a range check, only
    // for vector math)
    float dist = vm_vec_dist (&target_objp->pos, &tpos);

    // but we can still use it for the range check if we are optionally
    // checking that
    if (range >= 0 && dist > range) return SEXP_FALSE;

    // perform the check
    return object_in_turret_fov (target_objp, turret_subsys, &tvec, &tpos, dist) != 0 ? SEXP_TRUE : SEXP_FALSE;
}

void sexp_add_remove_escort (int node) {
    int sindex;
    int flag;

    // get the firing ship
    sindex = ship_name_lookup (CTEXT (node));
    if (sindex < 0) { return; }
    if (Ships[sindex].objnum < 0) { return; }

    // determine whether to add or remove it
    flag = eval_num (CDR (node));

    // add/remove
    if (flag) {
        Ships[sindex].escort_priority = flag;
        hud_add_ship_to_escort (Ships[sindex].objnum, 1);
    }
    else { hud_remove_ship_from_escort (Ships[sindex].objnum); }
}

// Goober5000
// set *all* the escort priorities of ships in escort list as follows: most
// damaged ship gets first priority in the argument list, next damaged gets
// next priority, etc.; if there are more ships than priorities, all remaining
// ships get the final priority on the list
// -- As indicated in the argument specification, there must be at least one
// argument but no more than MAX_COMPLETE_ESCORT_LIST arguments
void sexp_awacs_set_radius (int node) {
    int sindex;
    ship_subsys* awacs;

    // get the firing ship
    sindex = ship_name_lookup (CTEXT (node));
    if (sindex < 0) { return; }
    if (Ships[sindex].objnum < 0) { return; }

    // get the awacs subsystem
    awacs = ship_get_subsys (&Ships[sindex], CTEXT (CDR (node)));
    if (awacs == NULL) { return; }

    if (!(awacs->system_info->flags[Model::Subsystem_Flags::Awacs])) return;

    // set the new awacs radius
    awacs->awacs_radius = (float)eval_num (CDR (CDR (node)));
}

void add_nav_waypoint (char* nav, char* WP_path, int vert, char* oswpt_name) {
    int i;
    object_ship_wing_point_team oswpt;
    bool add_for_this_player = true;

    if (oswpt_name != NULL) {
        sexp_get_object_ship_wing_point_team (&oswpt, oswpt_name);

        // we can't assume this nav should be visible to the player any more
        add_for_this_player = false;

        switch (oswpt.type) {
        case OSWPT_TYPE_WHOLE_TEAM:
            if (oswpt.team == Player_ship->team) { add_for_this_player = true; }
            break;

        case OSWPT_TYPE_SHIP:
            if (oswpt.shipp == Player_ship) { add_for_this_player = true; }
            break;
        case OSWPT_TYPE_WING:
            for (i = 0; i < oswpt.wingp->current_count; i++) {
                if (Ships[oswpt.wingp->ship_index[i]].objnum == Player_ship->objnum) { add_for_this_player = true; }
            }

        // for all other oswpt types we simply ignore this
        default: break;
        }
    }

    AddNav_Waypoint (nav, WP_path, vert, add_for_this_player ? 0 : NP_HIDDEN);
}

// text: add-nav-waypoint
// args: 4, Nav Name, Waypoint Path Name, Waypoint Path point, ShipWingTeam
void add_nav_waypoint (int node) {
    char* nav_name = CTEXT (node);
    char* way_name = CTEXT (CDR (node));
    int vert = eval_num (CDR (CDR (node)));
    char* oswpt_name;

    node = CDR (CDR (CDR (node)));
    if (node >= 0) { oswpt_name = CTEXT (node); }
    else { oswpt_name = NULL; }

    add_nav_waypoint (nav_name, way_name, vert, oswpt_name);
}

//*************************************************************************************************

int sexp_is_tagged (int node) {
    int sindex;

    // get the firing ship
    sindex = ship_name_lookup (CTEXT (node));
    if (sindex < 0) { return SEXP_FALSE; }
    if (Ships[sindex].objnum < 0) { return SEXP_FALSE; }
    object* caller = &Objects[Ships[sindex].objnum];
    if (ship_is_tagged (caller)) { // This line and the one above were added.
        return SEXP_TRUE;
    }

    // not tagged
    return SEXP_FALSE;
}

// Joint effort of Sesquipedalian and Goober5000.  Sesq found the code, mucked
// around making sexps with it and learned things, Goober taught Sesq and made
// the sexp work properly. =D Returns true so long as the player has held a
// missile lock for the specified time. If the optional ship and/or ship's
// subsystem are specified, returns true when that has been locked onto, but
// otherwise returns as long as anything has been locked onto.
int sexp_missile_locked (int node) {
    int z;

    // if we aren't targeting anything, it's false
    if ((Players_target == -1) || (Players_target == UNINITIALIZED)) return SEXP_FALSE;

    // if we aren't locked on to anything, it's false
    if (!Players_mlocked) return SEXP_FALSE;

    // do we have a specific ship?
    if (CDR (node) != -1) {
        // if we're not targeting the specific ship, it's false
        if (strcasecmp (Ships[Objects[Players_target].instance].ship_name, CTEXT (CDR (node))) != 0) return SEXP_FALSE;

        // do we have a specific subsystem?
        if (CDR (CDR (node)) != -1) {
            // if we aren't targeting a subsystem at all, it's false
            if (!Player_ai->targeted_subsys) return SEXP_FALSE;

            // if we're not targeting the specific subsystem, it's false
            if (subsystem_strcasecmp (Player_ai->targeted_subsys->system_info->subobj_name, CTEXT (CDR (CDR (node))))) return SEXP_FALSE;
        }
    }

    // if we've gotten this far, we must have satisfied whatever conditions the
    // sexp imposed finally, test if we've locked for a certain period of time
    z = eval_num (node) * 1000;
    if (timestamp_has_time_elapsed (Players_mlocked_timestamp, z)) { return SEXP_TRUE; }

    return SEXP_FALSE;
}


// helper function for the remove-weapons SEXP

int sexp_return_player_data (int node, int type) {
    int sindex;
    player* p = NULL;

    sindex = ship_name_lookup (CTEXT (node));

    if (sindex < 0) { return 0; }
    if (Ships[sindex].objnum < 0) { return 0; }

    if (Player_obj == &Objects[Ships[sindex].objnum]) { p = Player; }

    // now, if we have a valid player, return his kills
    if (p != NULL) {
        switch (type) {
        case OP_NUM_KILLS:
            return p->stats.m_kill_count_ok;

        default: ASSERTX (0, "return-player-data was called with invalid type %d on node %d!", type, node);
        }
    }

    // AI ships
    return 0;
}

int sexp_num_type_kills (int node) {
    int sindex, st_index;
    int idx, total;
    player* p = NULL;

    // get the ship we're interested in
    sindex = ship_name_lookup (CTEXT (node));
    if (sindex < 0) { return 0; }
    if (Ships[sindex].objnum < 0) { return 0; }

    if (Player_obj == &Objects[Ships[sindex].objnum]) { p = Player; }

    // bad
    if (p == NULL) { return 0; }

    // lookup ship type name
    st_index = ship_type_name_lookup (CTEXT (CDR (node)));
    if (st_index < 0) { return 0; }

    // look stuff up
    total = 0;
    for (idx = 0; idx < static_cast< int > (Ship_info.size ()); idx++) {
        if ((p->stats.m_okKills[idx] > 0) && ship_class_query_general_type (idx) == st_index) { total += p->stats.m_okKills[idx]; }
    }

    // total
    return total;
}

int sexp_num_class_kills (int node) {
    int sindex, si_index;
    player* p = NULL;

    // get the ship we're interested in
    sindex = ship_name_lookup (CTEXT (node));
    if (sindex < 0) { return 0; }
    if (Ships[sindex].objnum < 0) { return 0; }

    if (Player_obj == &Objects[Ships[sindex].objnum]) { p = Player; }

    // bad
    if (p == NULL) { return 0; }

    // get the ship type we're looking for
    si_index = ship_info_lookup (CTEXT (CDR (node)));
    if ((si_index < 0) || (si_index >= static_cast< int > (Ship_info.size ()))) { return 0; }

    // return the count
    return p->stats.m_okKills[si_index];
}

void sexp_subsys_set_random (int node) {
    int sindex, low, high, n, idx, rand, exclusion_list[MAX_MODEL_SUBSYSTEMS];
    ship_subsys* subsys;
    ship* shipp;

    // get ship
    sindex = ship_name_lookup (CTEXT (node));
    if (sindex < 0) { return; }
    if (Ships[sindex].objnum < 0) { return; }
    shipp = &Ships[sindex];

    // get low
    low = eval_num (CDR (node));
    if (low < 0) { low = 0; }

    // get high
    high = eval_num (CDR (CDR (node)));
    if (high > 100) { high = 100; }

    if (low > high) {
        ASSERTX (0, "subsys-set-random was passed an invalid range (%d ... %d)!", low, high);
        return;
    }

    n = CDR (CDR (CDR (node)));

    // init exclusion list
    memset (exclusion_list, 0, sizeof (int) * Ship_info[shipp->ship_info_index].n_subsystems);

    // get exclusion list
    while (n != -1) {
        int exclude_index = ship_get_subsys_index (shipp, CTEXT (n), 0);
        if (exclude_index >= 0) { exclusion_list[exclude_index] = 1; }

        n = CDR (n);
    }

    // apply to all others
    for (idx = 0; idx < Ship_info[shipp->ship_info_index].n_subsystems; idx++) {
        if (exclusion_list[idx] == 0) {
            // get non excluded subsystem
            subsys = ship_get_indexed_subsys (shipp, idx, NULL);
            if (subsys == NULL) {
                WARNINGF (LOCATION, "Nonexistent subsystem for index %d on ship %s for sabotage subsystem", idx, shipp->ship_name);
                continue;
            }

            // randomize its hit points
            rand = rand_internal (low, high);
            subsys->current_hits = 0.01f * rand * subsys->max_hits;
        }
    }
}

void sexp_supernova_start (int node) { supernova_start (eval_num (node)); }

int sexp_is_secondary_selected (int node) {
    int sindex;
    int bank;
    ship* shipp;

    // lookup ship
    sindex = ship_name_lookup (CTEXT (node));
    if (sindex < 0) { return SEXP_FALSE; }
    if (Ships[sindex].objnum < 0) { return SEXP_FALSE; }
    shipp = &Ships[sindex];

    // bogus value?
    bank = eval_num (CDR (node));
    if (bank >= shipp->weapons.num_secondary_banks) { return SEXP_FALSE; }

    // is this the bank currently selected
    if (bank == shipp->weapons.current_secondary_bank) { return SEXP_TRUE; }

    // nope
    return SEXP_FALSE;
}

int sexp_is_primary_selected (int node) {
    int sindex;
    int bank;
    ship* shipp;

    // lookup ship
    sindex = ship_name_lookup (CTEXT (node));
    if (sindex < 0) { return SEXP_FALSE; }
    if (Ships[sindex].objnum < 0) { return SEXP_FALSE; }
    shipp = &Ships[sindex];

    // bogus value?
    bank = eval_num (CDR (node));
    if (bank >= shipp->weapons.num_primary_banks) { return SEXP_FALSE; }

    // is this the bank currently selected
    if ((bank == shipp->weapons.current_primary_bank) || (shipp->flags[Ship::Ship_Flags::Primary_linked])) { return SEXP_TRUE; }

    // nope
    return SEXP_FALSE;
}

// Return SEXP_TRUE if quadrant quadnum is near max.
int shield_quad_near_max (int quadnum) {
    if (quadnum >= Player_obj->n_quadrants) return SEXP_FALSE;

    float remaining = 0.0f;
    for (int i = 0; i < Player_obj->n_quadrants; i++) {
        if (i == quadnum) { continue; }
        remaining += Player_obj->shield_quadrant[i];
    }

    if ((remaining < 2.0f) || (Player_obj->shield_quadrant[quadnum] > shield_get_max_quad (Player_obj) - 5.0f)) { return SEXP_TRUE; }
    else { return SEXP_FALSE; }
}

// Return truth value for special SEXP.
// Used in training#5, perhaps in other missions.
int process_special_sexps (int index) {
    switch (index) {
    case 0: // Ship "Freighter 1" is aspect locked by player.
        if (Player_ai->target_objnum != -1) {
            if (!(strcasecmp (Ships[Objects[Player_ai->target_objnum].instance].ship_name, "Freighter 1"))) {
                if (Player_ai->current_target_is_locked) return SEXP_TRUE;
            }
        }
        return SEXP_FALSE;

    case 1: // Fired Interceptors
        object* objp;
        for (objp = GET_FIRST (&obj_used_list); objp != END_OF_LIST (&obj_used_list); objp = GET_NEXT (objp)) {
            if (objp->type == OBJ_WEAPON) {
                if (!strcasecmp (Weapon_info[Weapons[objp->instance].weapon_info_index].name, "Interceptor#weak")) {
                    int target = Weapons[objp->instance].target_num;
                    if (target != -1) {
                        if (Objects[target].type == OBJ_SHIP) {
                            if (!(strcasecmp (Ships[Objects[target].instance].ship_name, "Freighter 1"))) return SEXP_TRUE;
                        }
                    }
                }
            }
        }
        return SEXP_FALSE;

    case 2: // Ship "Freighter 1", subsystem "Weapons" is aspect locked by
        // player.
        if (Player_ai->target_objnum != -1) {
            if (!(strcasecmp (Ships[Objects[Player_ai->target_objnum].instance].ship_name, "Freighter 1"))) {
                if (!(subsystem_strcasecmp (Player_ai->targeted_subsys->system_info->name, "Weapons"))) {
                    if (Player_ai->current_target_is_locked) { return SEXP_TRUE; }
                }
            }
        }
        return SEXP_FALSE;

    case 3: // Player ship suffering shield damage on front.
        if (!(Ship_info[Player_ship->ship_info_index].flags[Ship::Info_Flags::Model_point_shields])) {
            shield_apply_damage (Player_obj, FRONT_QUAD, 10.0f);
            hud_shield_quadrant_hit (Player_obj, FRONT_QUAD);
            return SEXP_TRUE;
        }
        else {
            WARNINGF (LOCATION, "Shield-related Special-check SEXPs do not work on ship %s because it uses model point shields.", Player_ship->ship_name);
            return SEXP_FALSE;
        }
        break;

    case 4: // Player ship suffering much damage.
        if (!(Ship_info[Player_ship->ship_info_index].flags[Ship::Info_Flags::Model_point_shields])) {
            WARNINGF (LOCATION, "Frame %i", Framecount);
            shield_apply_damage (Player_obj, FRONT_QUAD, 10.0f);
            hud_shield_quadrant_hit (Player_obj, FRONT_QUAD);
            if (Player_obj->shield_quadrant[FRONT_QUAD] < 2.0f)
                return SEXP_TRUE;
            else
                return SEXP_FALSE;
        }
        else {
            WARNINGF (LOCATION, "Shield-related Special-check SEXPs do not work on ship %s because it uses model point shields.", Player_ship->ship_name);
            return SEXP_FALSE;
        }
        break;

    case 5: // Player's shield is quick repaired
        if (!(Ship_info[Player_ship->ship_info_index].flags[Ship::Info_Flags::Model_point_shields])) {
            WARNINGF (LOCATION, "Frame %i, recharged to %7.3f", Framecount, Player_obj->shield_quadrant[FRONT_QUAD]);

            shield_apply_damage (Player_obj, FRONT_QUAD, -flFrametime * 200.0f);

            if (Player_obj->shield_quadrant[FRONT_QUAD] > shield_get_max_quad (Player_obj)) Player_obj->shield_quadrant[FRONT_QUAD] = shield_get_max_quad (Player_obj);

            if (Player_obj->shield_quadrant[FRONT_QUAD] > Player_obj->shield_quadrant[(FRONT_QUAD + 1) % DEFAULT_SHIELD_SECTIONS] - 2.0f)
                return SEXP_TRUE;
            else
                return SEXP_FALSE;
        }
        else {
            WARNINGF (LOCATION, "Shield-related Special-check SEXPs do not work on ship %s because it uses model point shields.", Player_ship->ship_name);
            return SEXP_FALSE;
        }
        break;

    case 6: // 3 of player's shield quadrants are reduced to 0.
        if (!(Ship_info[Player_ship->ship_info_index].flags[Ship::Info_Flags::Model_point_shields])) {
            Player_obj->shield_quadrant[1] = 1.0f;
            Player_obj->shield_quadrant[2] = 1.0f;
            Player_obj->shield_quadrant[3] = 1.0f;
            hud_shield_quadrant_hit (Player_obj, FRONT_QUAD);
        }
        else {
            WARNINGF (LOCATION, "Shield-related Special-check SEXPs do not work on ship %s because it uses model point shields.", Player_ship->ship_name);
            return SEXP_FALSE;
        }
        return SEXP_TRUE;

    case 7: // Make sure front quadrant has been maximized, or close to it.
        if (!(Ship_info[Player_ship->ship_info_index].flags[Ship::Info_Flags::Model_point_shields])) {
            if (shield_quad_near_max (FRONT_QUAD))
                return SEXP_TRUE;
            else
                return SEXP_FALSE;
        }
        else {
            WARNINGF (LOCATION, "Shield-related Special-check SEXPs do not work on ship %s because it uses model point shields.", Player_ship->ship_name);
            return SEXP_FALSE;
        }
        break;

    case 8: // Make sure rear quadrant has been maximized, or close to it.
        if (!(Ship_info[Player_ship->ship_info_index].flags[Ship::Info_Flags::Model_point_shields])) {
            if (shield_quad_near_max (REAR_QUAD))
                return SEXP_TRUE;
            else
                return SEXP_FALSE;
        }
        else {
            WARNINGF (LOCATION, "Shield-related Special-check SEXPs do not work on ship %s because it uses model point shields.", Player_ship->ship_name);
            return SEXP_FALSE;
        }
        break;

    case 9: // Zero left and right quadrants in preparation for maximizing
        // rear quadrant.
        if (!(Ship_info[Player_ship->ship_info_index].flags[Ship::Info_Flags::Model_point_shields])) {
            Player_obj->shield_quadrant[LEFT_QUAD] = 0.0f;
            Player_obj->shield_quadrant[RIGHT_QUAD] = 0.0f;
            hud_shield_quadrant_hit (Player_obj, LEFT_QUAD);
            return SEXP_TRUE;
        }
        else {
            WARNINGF (LOCATION, "Shield-related Special-check SEXPs do not work on ship %s because it uses model point shields.", Player_ship->ship_name);
            return SEXP_FALSE;
        }
        break;

    case 10: // Return true if player is low on Interceptors.
        if (Player_ship->weapons.secondary_bank_ammo[0] + Player_ship->weapons.secondary_bank_ammo[1] < 8)
            return SEXP_TRUE;
        else
            return SEXP_FALSE;
        break;

    case 11: // Return true if player has plenty of Interceptors.
        if (Player_ship->weapons.secondary_bank_ammo[0] + Player_ship->weapons.secondary_bank_ammo[1] >= 8)
            return SEXP_TRUE;
        else
            return SEXP_FALSE;
        break;

    case 12: // Return true if player is low on Interceptors.
        if (Player_ship->weapons.secondary_bank_ammo[0] + Player_ship->weapons.secondary_bank_ammo[1] < 4)
            return SEXP_TRUE;
        else
            return SEXP_FALSE;
        break;

    case 13: // Zero front shield quadrant.  Added for Jim Boone on August 26,
             // 1999 by MK.
        if (!(Ship_info[Player_ship->ship_info_index].flags[Ship::Info_Flags::Model_point_shields])) {
            Player_obj->shield_quadrant[FRONT_QUAD] = 0.0f;
            hud_shield_quadrant_hit (Player_obj, FRONT_QUAD);
            return SEXP_TRUE;
        }
        else {
            WARNINGF (LOCATION, "Shield-related Special-check SEXPs do not work on ship %s because it uses model point shields.", Player_ship->ship_name);
            return SEXP_FALSE;
        }
        break;

    case 100: // Return true if player is out of countermeasures.
        if (Player_ship->cmeasure_count <= 0)
            return SEXP_TRUE;
        else
            return SEXP_FALSE;

    default:
        ASSERTX (
            false,
            "Special sexp processing code was called for an unsupported node "
            "type!");
    }

    return SEXP_FALSE;
}

// custom sexp operator for handling misc training stuff
int sexp_special_training_check (int node) {
    int num, rtn;

    num = eval_num (node);
    if (num == SPECIAL_CHECK_TRAINING_FAILURE) return Training_failure ? SEXP_TRUE : SEXP_FALSE;

    // To MK: do whatever you want with this number here.
    rtn = process_special_sexps (eval_num (node));

    return rtn;
}

// sexpression to flash a hud gauge.  gauge name is text valud of node
void sexp_flash_hud_gauge (int node) {
    char* name;
    int i;

    name = CTEXT (node);
    for (i = 0; i < NUM_HUD_GAUGES; i++) {
        if (!strcasecmp (HUD_gauge_text[i], name)) {
            hud_gauge_start_flash (i); // call HUD function to flash gauge
            break;
        }
    }
}

void sexp_set_training_context_fly_path (int node) {
    waypoint_list* wp_list = find_matching_waypoint_list (CTEXT (node));
    if (wp_list == NULL) return;

    Training_context |= TRAINING_CONTEXT_FLY_PATH;
    Training_context_path = wp_list;
    Training_context_distance = (float)atof (CTEXT (CDR (node)));
    Training_context_goal_waypoint = 0;
    Training_context_at_waypoint = -1;
}

void sexp_set_training_context_speed (int node) {
    Training_context |= TRAINING_CONTEXT_SPEED;
    Training_context_speed_min = eval_num (node);
    Training_context_speed_max = eval_num (CDR (node));
    Training_context_speed_set = 0;
}

void sexp_fade (bool fade_in, int duration, ubyte R, ubyte G, ubyte B) {
    if (duration > 0) {
        Fade_start_timestamp = timestamp ();
        Fade_end_timestamp = timestamp (duration);
        Fade_type = fade_in ? FI_FADEIN : FI_FADEOUT;
        gr_create_shader (&Viewer_shader, R, G, B, Viewer_shader.c);
    }
    else {
        Fade_type = FI_NONE;
        gr_create_shader (&Viewer_shader, R, G, B, fade_in ? 0 : 255);
    }
}

static int Fade_out_r = -1;
static int Fade_out_g = -1;
static int Fade_out_b = -1;

void sexp_fade (int n, bool fade_in) {
    int duration = 0;
    int R = -1;
    int G = -1;
    int B = -1;

    if (n != -1) {
        duration = eval_num (n);
        n = CDR (n);

        if (n != -1) {
            R = eval_num (n);
            if (R < 0 || R > 255) R = -1;
            n = CDR (n);

            if (n != -1) {
                G = eval_num (n);
                if (G < 0 || G > 255) G = -1;
                n = CDR (n);

                if (n != -1) {
                    B = eval_num (n);
                    if (B < 0 || B > 255) B = -1;
                    n = CDR (n);
                }
            }
        }
    }

    // select legacy (or default) fade color
    if (R < 0 || G < 0 || B < 0) {
        // fade white
        if (R == 1) { R = G = B = 255; }
        // fade red
        else if (R == 2) {
            R = 255;
            G = B = 0;
        }
        // default: fade black
        else {
            // Mantis #2944: if we're fading in, and we previously faded out to
            // some specific color, use that same color to fade in
            if (fade_in && (Fade_out_r >= 0) && (Fade_out_g >= 0) && (Fade_out_b >= 0)) {
                R = Fade_out_r;
                G = Fade_out_g;
                B = Fade_out_b;
            }
            else { R = G = B = 0; }
        }
    }

    // Mantis #2944, if we're fading out to some specific color, save that
    // color
    if (!fade_in && ((R > 0) || (G > 0) || (B > 0))) {
        Fade_out_r = R;
        Fade_out_g = G;
        Fade_out_b = B;
    }

    sexp_fade (fade_in, duration, (ubyte)R, (ubyte)G, (ubyte)B);
}

extern float VIEWER_ZOOM_DEFAULT;
extern bool Perspective_locked;

int get_effect_from_name (char* name) {
    int i = 0;
    for (std::vector< ship_effect >::iterator sei = Ship_effects.begin (); sei != Ship_effects.end (); ++sei) {
        if (!strcasecmp (name, sei->name)) return i;
        i++;
    }
    return -1;
}

extern int Cheats_enabled;

/**
 * Returns the subsystem type if the name of a subsystem is actually a generic
 * type (e.g \<all engines\> or \<all turrets\>
 */
int get_generic_subsys (char* subsys_name) {
    if (!strcmp (subsys_name, SEXP_ALL_ENGINES_STRING)) { return SUBSYSTEM_ENGINE; }
    else if (!strcmp (subsys_name, SEXP_ALL_TURRETS_STRING)) { return SUBSYSTEM_TURRET; }

    ASSERT (SUBSYSTEM_NONE == 0);
    return SUBSYSTEM_NONE;
}

// Karajorma - returns false if the ship class has changed since the mission
// was parsed in Changes can be from use of the change-ship-class SEXP, loadout
// or any future method
bool ship_class_unchanged (int ship_index) {
    p_object* p_objp;

    ship* shipp = &Ships[ship_index];
    p_objp = mission_parse_get_parse_object (shipp->ship_name);

    if ((p_objp != NULL) && (p_objp->ship_class == shipp->ship_info_index)) { return true; }

    return false;
}

// Goober5000 - needed because any nonzero integer value is "true"
bool is_sexp_true (int cur_node, int referenced_node) {
    int result = eval_sexp (cur_node, referenced_node);

    // any SEXP_KNOWN_TRUE result will return SEXP_TRUE from eval_sexp, but
    // let's be defensive
    return (result == SEXP_TRUE) || (result == SEXP_KNOWN_TRUE);
}

/**
 *
 */
int generate_event_log_flags_mask (int result) {
    int matches = 0;
    mission_event* current_event = &Mission_events[Event_index];

    switch (result) {
    case SEXP_TRUE: matches |= MLF_SEXP_TRUE; break;

    case SEXP_FALSE: matches |= MLF_SEXP_FALSE; break;

    default: ASSERTX (0, "SEXP has a value which isn't true or false."); break;
    }

    if ((result == SEXP_TRUE) || (result == SEXP_KNOWN_TRUE)) {
        // now deal with the flags depending on repeat and trigger counts
        switch (current_event->mission_log_flags) {
        case MLF_FIRST_REPEAT_ONLY:
            if (current_event->repeat_count > 1) { matches |= MLF_FIRST_REPEAT_ONLY; }
            break;

        case MLF_LAST_REPEAT_ONLY:
            if (current_event->repeat_count == 1) { matches |= MLF_LAST_REPEAT_ONLY; }
            break;

        case MLF_FIRST_TRIGGER_ONLY:
            if (current_event->trigger_count > 1) { matches |= MLF_FIRST_TRIGGER_ONLY; }
            break;

        case MLF_LAST_TRIGGER_ONLY:
            if ((current_event->trigger_count == 1) && (current_event->flags & MEF_USING_TRIGGER_COUNT)) { matches |= MLF_LAST_TRIGGER_ONLY; }
            break;
        }
    }

    return matches;
}

void current_log_to_backup_log_buffer () {
    Mission_events[Event_index].backup_log_buffer.clear ();
    if (!(Mission_events[Event_index].mission_log_flags & MLF_STATE_CHANGE)) { return; }

    for (int i = 0; i < (int)Current_event_log_buffer->size (); i++) { Mission_events[Event_index].backup_log_buffer.push_back (Current_event_log_buffer->at (i)); }
}

void maybe_write_previous_event_to_log (int result) {
    mission_event* this_event = &Mission_events[Event_index];

    // if the old log is empty, all we do is record the result for the next
    // evaluation the old log should only be empty at mission start
    if (this_event->backup_log_buffer.empty ()) {
        this_event->previous_result = result;
        return;
    }

    // if there's no change in state, we don't write the previous state to the
    // log
    if ((this_event->mission_log_flags & MLF_STATE_CHANGE) && (result == this_event->previous_result)) {
        current_log_to_backup_log_buffer ();
        return;
    }

    II << "Event has changed state. Old state";

    while (!this_event->backup_log_buffer.empty ()) {
        II << this_event->backup_log_buffer.back ().c_str ();
        this_event->backup_log_buffer.pop_back ();
    }

    II << "New state";

    // backup the current buffer as this may be a repeating event
    current_log_to_backup_log_buffer ();
}

/**
 * Checks the mission logs flags for this event and writes to the log if this
 * has been asked for
 */
void maybe_write_to_event_log (int result) {
    char buffer[256];

    int mask = generate_event_log_flags_mask (result);
    sprintf (buffer, "Event: %s at mission time %d seconds (%d milliseconds)", Mission_events[Event_index].name, f2i (Missiontime), f2i ((longlong)Missiontime * 1000));
    Current_event_log_buffer->push_back (buffer);

    if (!Snapshot_all_events && (!(mask &= Mission_events[Event_index].mission_log_flags))) {
        current_log_to_backup_log_buffer ();
        Current_event_log_buffer->clear ();
        return;
    }

    // remove some of the flags
    if (mask & (MLF_FIRST_REPEAT_ONLY | MLF_FIRST_TRIGGER_ONLY)) { Mission_events[Event_index].mission_log_flags &= ~(MLF_FIRST_REPEAT_ONLY | MLF_FIRST_TRIGGER_ONLY); }

    if (Mission_events[Event_index].mission_log_flags & MLF_STATE_CHANGE) { maybe_write_previous_event_to_log (result); }

    while (!Current_event_log_buffer->empty ()) {
        II << Current_event_log_buffer->back ().c_str ();
        Current_event_log_buffer->pop_back ();
    }
}

/**
 * Returns the constant used as a SEXP's result as text for printing to the
 * event log
 */
const char* sexp_get_result_as_text (int result) {
    switch (result) {
    case SEXP_TRUE: return "TRUE";

    case SEXP_FALSE: return "FALSE";

    case SEXP_KNOWN_FALSE: return "ALWAYS FALSE";

    case SEXP_KNOWN_TRUE: return "ALWAYS TRUE";

    case SEXP_UNKNOWN: return "UNKNOWN";

    case SEXP_NAN: return "NOT A NUMBER";

    case SEXP_NAN_FOREVER: return "CAN NEVER BE A NUMBER";

    case SEXP_CANT_EVAL: return "CAN'T EVALUATE";

    default: return NULL;
    }
}

/**
 * Checks the mission logs flags for this event and writes to the log if this
 * has been asked for
 */
void add_to_event_log_buffer (int op_num, int result) {
    ASSERTX (
        (Current_event_log_buffer != NULL) && (Current_event_log_variable_buffer != NULL) && (Current_event_log_argument_buffer != NULL),
        "Attempting to write to a non-existent log buffer");

    if (op_num == -1) {
        WARNINGF (LOCATION, "ERROR: op_num function returned %i, this should not happen. Contact a coder.", op_num);
        return; // How does this happen?
    }

    char buffer[TOKEN_LENGTH];
    std::string tmp;
    tmp.append (Operators[op_num].text);
    tmp.append (" returned ");

    if ((Operators[op_num].type & (SEXP_INTEGER_OPERATOR | SEXP_ARITHMETIC_OPERATOR)) || (sexp_get_result_as_text (result) == NULL)) {
        sprintf (buffer, "%d", result);
        tmp.append (buffer);
    }
    else { tmp.append (sexp_get_result_as_text (result)); }

    if (True_loop_argument_sexps && !Sexp_replacement_arguments.empty ()) {
        tmp.append (" for argument ");
        tmp.append (Sexp_replacement_arguments.back ());
    }

    if (!Current_event_log_argument_buffer->empty ()) {
        tmp.append (" for the following arguments");
        while (!Current_event_log_argument_buffer->empty ()) {
            tmp.append ("\n");
            tmp.append (Current_event_log_argument_buffer->back ().c_str ());
            Current_event_log_argument_buffer->pop_back ();
        }
    }

    if (!Current_event_log_variable_buffer->empty ()) {
        tmp.append ("\nVariables:\n");
        while (!Current_event_log_variable_buffer->empty ()) {
            tmp.append (Current_event_log_variable_buffer->back ().c_str ());
            Current_event_log_variable_buffer->pop_back ();
            tmp.append ("[");
            tmp.append (Current_event_log_variable_buffer->back ().c_str ());
            Current_event_log_variable_buffer->pop_back ();
            tmp.append ("]");
        }
    }

    Current_event_log_buffer->push_back (tmp);
}

/**
 * High-level sexpression evaluator
 */
int eval_sexp (int cur_node, int) {
    int node, type, sexp_val = UNINITIALIZED;
    if (cur_node == -1) // empty list, i.e. sexp: ( )
        return SEXP_FALSE;

    ASSERT (cur_node >= 0); // we have special sexp nodes <= -1!!!  MWA
        // which should be intercepted before we get here.  HOFFOSS
    type = SEXP_NODE_TYPE (cur_node);
    ASSERT ((type == SEXP_LIST) || (type == SEXP_ATOM));

    // trap known true and known false sexpressions.  We don't trap on SEXP_NAN
    // sexpressions since they may yet evaluate to true or false.

    // we want to log event values for KNOWN_X or FOREVER_X before returning
    if (Log_event && ((Sexp_nodes[cur_node].value == SEXP_KNOWN_TRUE) || (Sexp_nodes[cur_node].value == SEXP_KNOWN_FALSE) || (Sexp_nodes[cur_node].value == SEXP_NAN_FOREVER))) {
        // if this is a node that has been assigned the value by
        // short-circuiting, it might not be the operator that returned the
        // value
        int op_index = get_operator_index (cur_node);
        if (op_index < 0) op_index = get_operator_index (CAR (cur_node));

        // log the known value
        add_to_event_log_buffer (op_index, Sexp_nodes[cur_node].value);
    }

    // now do a quick return whether or not we log, per the comment above about
    // trapping known sexpressions
    if (Sexp_nodes[cur_node].value == SEXP_KNOWN_TRUE) { return SEXP_TRUE; }
    else if (Sexp_nodes[cur_node].value == SEXP_KNOWN_FALSE) { return SEXP_FALSE; }
    else if (Sexp_nodes[cur_node].value == SEXP_NAN_FOREVER) { return SEXP_FALSE; }

    if (Sexp_nodes[cur_node].first != -1) {
        node = CAR (cur_node);
        sexp_val = eval_sexp (node);
        Sexp_nodes[cur_node].value = Sexp_nodes[node].value; // higher level node gets node value
        return sexp_val;
    }
    else {
        int op_num;

        node = CDR (cur_node); // makes reading the next bit of code a little easier.

        op_num = get_operator_const (cur_node);
        // add the op_num to the stack if it is an actual operator rather than
        // a number
        if (op_num) { Current_sexp_operator.push_back (op_num); }
        switch (op_num) {
            // arithmetic operators will always return just their value
        case OP_PLUS: sexp_val = add_sexps (node); break;

        case OP_MINUS: sexp_val = sub_sexps (node); break;

        case OP_MUL: sexp_val = mul_sexps (node); break;

        case OP_MOD: sexp_val = mod_sexps (node); break;

        case OP_DIV: sexp_val = div_sexps (node); break;

        case OP_RAND:
            sexp_val = rand_sexp (node, false);
            break;

        // boolean operators can have one of the special sexp values (known
        // true, known false, unknown)
        case OP_TRUE: sexp_val = SEXP_KNOWN_TRUE; break;

        case OP_FALSE: sexp_val = SEXP_KNOWN_FALSE; break;

        case OP_OR: sexp_val = sexp_or (node); break;

        case OP_AND: sexp_val = sexp_and (node); break;

        case OP_AND_IN_SEQUENCE: sexp_val = sexp_and_in_sequence (node); break;

        case OP_IS_IFF: sexp_val = sexp_is_iff (node); break;

        case OP_NOT: sexp_val = sexp_not (node); break;

        case OP_PREVIOUS_GOAL_TRUE: sexp_val = sexp_previous_goal_status (node, GOAL_COMPLETE); break;

        case OP_PREVIOUS_GOAL_FALSE: sexp_val = sexp_previous_goal_status (node, GOAL_FAILED); break;

        case OP_PREVIOUS_GOAL_INCOMPLETE: sexp_val = sexp_previous_goal_status (node, GOAL_INCOMPLETE); break;

        case OP_PREVIOUS_EVENT_TRUE: sexp_val = sexp_previous_event_status (node, EVENT_SATISFIED); break;

        case OP_PREVIOUS_EVENT_FALSE: sexp_val = sexp_previous_event_status (node, EVENT_FAILED); break;

        case OP_PREVIOUS_EVENT_INCOMPLETE: sexp_val = sexp_previous_event_status (node, EVENT_INCOMPLETE); break;

        case OP_EVENT_TRUE:
        case OP_EVENT_FALSE:
            sexp_val = sexp_event_status (node, (op_num == OP_EVENT_TRUE ? 1 : 0));
            if ((sexp_val != SEXP_TRUE) && (sexp_val != SEXP_KNOWN_TRUE)) Sexp_useful_number = 0; // indicate sexp isn't current yet
            break;

        case OP_EVENT_TRUE_DELAY:
        case OP_EVENT_FALSE_DELAY: sexp_val = sexp_event_delay_status (node, (op_num == OP_EVENT_TRUE_DELAY ? 1 : 0)); break;

        case OP_GOAL_TRUE_DELAY:
        case OP_GOAL_FALSE_DELAY: sexp_val = sexp_goal_delay_status (node, (op_num == OP_GOAL_TRUE_DELAY ? 1 : 0)); break;

        case OP_EVENT_INCOMPLETE:
            sexp_val = sexp_event_incomplete (node);
            if ((sexp_val != SEXP_TRUE) && (sexp_val != SEXP_KNOWN_TRUE)) Sexp_useful_number = 0; // indicate sexp isn't current yet
            break;

        case OP_GOAL_INCOMPLETE: sexp_val = sexp_goal_incomplete (node); break;

        // destroy type sexpressions
        case OP_IS_DESTROYED: sexp_val = sexp_is_destroyed (node, NULL); break;

        case OP_IS_SUBSYSTEM_DESTROYED:
            sexp_val = sexp_is_subsystem_destroyed (node);
            break;

        case OP_HAS_ARRIVED: sexp_val = sexp_has_arrived (node, NULL); break;

        case OP_HAS_DEPARTED: sexp_val = sexp_has_departed (node, NULL); break;

        case OP_IS_DISABLED: sexp_val = sexp_is_disabled (node, NULL); break;

        case OP_IS_DISARMED: sexp_val = sexp_is_disarmed (node, NULL); break;

        case OP_WAYPOINTS_DONE: sexp_val = sexp_are_waypoints_done (node); break;

        // objective operators that use a delay
        case OP_IS_DESTROYED_DELAY: sexp_val = sexp_is_destroyed_delay (node); break;

        case OP_IS_SUBSYSTEM_DESTROYED_DELAY: sexp_val = sexp_is_subsystem_destroyed_delay (node); break;

        case OP_HAS_DOCKED:
        case OP_HAS_UNDOCKED:
        case OP_HAS_DOCKED_DELAY:
        case OP_HAS_UNDOCKED_DELAY: sexp_val = sexp_has_docked_or_undocked (node, op_num); break;

        case OP_HAS_ARRIVED_DELAY: sexp_val = sexp_has_arrived_delay (node); break;

        case OP_HAS_DEPARTED_DELAY: sexp_val = sexp_has_departed_delay (node); break;

        case OP_IS_DISABLED_DELAY: sexp_val = sexp_is_disabled_delay (node); break;

        case OP_IS_DISARMED_DELAY: sexp_val = sexp_is_disarmed_delay (node); break;

        case OP_WAYPOINTS_DONE_DELAY: sexp_val = sexp_are_waypoints_done_delay (node); break;

        case OP_SHIP_TYPE_DESTROYED: sexp_val = sexp_ship_type_destroyed (node); break;

        // time based sexpressions
        case OP_HAS_TIME_ELAPSED: sexp_val = sexp_has_time_elapsed (node); break;

        case OP_MODIFY_VARIABLE:
            sexp_modify_variable (node);
            sexp_val = SEXP_TRUE; // SEXP_TRUE means only do once.
            break;

        case OP_TIME_SHIP_DESTROYED:
            sexp_val = sexp_time_destroyed (node);
            break;

        case OP_TIME_WING_DESTROYED: sexp_val = sexp_time_wing_destroyed (node); break;

        case OP_TIME_SHIP_ARRIVED: sexp_val = sexp_time_ship_arrived (node); break;

        case OP_TIME_WING_ARRIVED: sexp_val = sexp_time_wing_arrived (node); break;

        case OP_TIME_SHIP_DEPARTED: sexp_val = sexp_time_ship_departed (node); break;

        case OP_TIME_WING_DEPARTED: sexp_val = sexp_time_wing_departed (node); break;

        case OP_MISSION_TIME: sexp_val = sexp_mission_time (); break;

        case OP_TIME_DOCKED:
        case OP_TIME_UNDOCKED: sexp_val = sexp_time_docked_or_undocked (node, op_num == OP_TIME_DOCKED); break;

        case OP_SHIELDS_LEFT: sexp_val = sexp_shields_left (node); break;

        case OP_HITS_LEFT: sexp_val = sexp_hits_left (node); break;

        case OP_HITS_LEFT_SUBSYSTEM: sexp_val = sexp_hits_left_subsystem (node); break;

        case OP_SPECIAL_WARP_DISTANCE:
            sexp_val = sexp_special_warp_dist (node);
            break;

        case OP_DISTANCE: sexp_val = sexp_distance (node); break;

        case OP_IS_SHIP_VISIBLE: sexp_val = sexp_is_ship_visible (node); break;

        case OP_TEAM_SCORE: ASSERT (0); break;

        case OP_LAST_ORDER_TIME: sexp_val = sexp_last_order_time (node); break;

        case OP_NUM_PLAYERS: sexp_val = sexp_num_players (); break;

        case OP_SKILL_LEVEL_AT_LEAST: sexp_val = sexp_skill_level_at_least (node); break;

        case OP_IS_CARGO_KNOWN:
        case OP_CARGO_KNOWN_DELAY: sexp_val = sexp_is_cargo_known (node, (op_num == OP_IS_CARGO_KNOWN) ? 0 : 1); break;

        case OP_HAS_BEEN_TAGGED_DELAY: sexp_val = sexp_has_been_tagged_delay (node); break;

        case OP_CAP_SUBSYS_CARGO_KNOWN_DELAY:
            sexp_val = sexp_cap_subsys_cargo_known_delay (node);
            break;

        case OP_WAS_PROMOTION_GRANTED: sexp_val = sexp_was_promotion_granted (node); break;

        case OP_WAS_MEDAL_GRANTED: sexp_val = sexp_was_medal_granted (node); break;

        case OP_PERCENT_SHIPS_DEPARTED:
        case OP_PERCENT_SHIPS_DESTROYED:
            sexp_val = sexp_percent_ships_arrive_depart_destroy_disarm_disable (
                node, op_num);
            break;

        case OP_DEPART_NODE_DELAY: sexp_val = sexp_depart_node_delay (node); break;

        case OP_DESTROYED_DEPARTED_DELAY: sexp_val = sexp_destroyed_departed_delay (node); break;

        // conditional sexpressions
        case OP_WHEN:
            sexp_val = eval_when (node, op_num);
            break;

        case OP_COND: sexp_val = eval_cond (node); break;

        case OP_CHANGE_IFF:
            sexp_change_iff (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_ADD_SHIP_GOAL:
            sexp_add_ship_goal (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_ADD_WING_GOAL:
            sexp_add_wing_goal (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_ADD_GOAL:
            sexp_add_goal (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_CLEAR_SHIP_GOALS:
            sexp_clear_ship_goals (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_CLEAR_WING_GOALS:
            sexp_clear_wing_goals (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_CLEAR_GOALS:
            sexp_clear_goals (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_PROTECT_SHIP:
        case OP_UNPROTECT_SHIP:
            sexp_protect_ships (node, (op_num == OP_PROTECT_SHIP));
            sexp_val = SEXP_TRUE;
            break;

        case OP_BEAM_PROTECT_SHIP:
        case OP_BEAM_UNPROTECT_SHIP:
            sexp_beam_protect_ships (node, (op_num == OP_BEAM_PROTECT_SHIP));
            sexp_val = SEXP_TRUE;
            break;

        case OP_SHIP_INVISIBLE:
        case OP_SHIP_VISIBLE:
            sexp_ships_visible (node, (op_num == OP_SHIP_VISIBLE));
            sexp_val = SEXP_TRUE;
            break;

        case OP_SHIP_VULNERABLE:
        case OP_SHIP_INVULNERABLE:
            sexp_ships_invulnerable (node, (op_num == OP_SHIP_INVULNERABLE));
            sexp_val = SEXP_TRUE;
            break;

        case OP_SHIP_GUARDIAN:
        case OP_SHIP_NO_GUARDIAN:
            sexp_ships_guardian (node, (op_num == OP_SHIP_GUARDIAN));
            sexp_val = SEXP_TRUE;
            break;

        case OP_SHIP_VANISH:
            sexp_ship_vanish (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_SEND_MESSAGE:
            sexp_send_message (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_SEND_MESSAGE_LIST:
            sexp_send_message_list (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_SEND_RANDOM_MESSAGE:
            sexp_send_random_message (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_SELF_DESTRUCT:
            sexp_self_destruct (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_NEXT_MISSION:
            sexp_next_mission (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_END_OF_CAMPAIGN:
            sexp_end_of_campaign (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_END_CAMPAIGN:
            sexp_end_campaign (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_SABOTAGE_SUBSYSTEM:
            sexp_sabotage_subsystem (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_REPAIR_SUBSYSTEM:
            sexp_repair_subsystem (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_SET_SUBSYSTEM_STRNGTH:
            sexp_set_subsystem_strength (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_INVALIDATE_GOAL:
        case OP_VALIDATE_GOAL:
            sexp_change_goal_validity (node, (op_num == OP_INVALIDATE_GOAL ? 0 : 1));
            sexp_val = SEXP_TRUE;
            break;

        case OP_TRANSFER_CARGO:
            sexp_transfer_cargo (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_EXCHANGE_CARGO:
            sexp_exchange_cargo (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_CARGO_NO_DEPLETE:
            sexp_cargo_no_deplete (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_SET_SPECIAL_WARPOUT_NAME:
            sexp_special_warpout_name (node);
            sexp_val = SEXP_TRUE;
            break;

            // sexpressions for setting flag for good/bad time for someone to
            // reasm
        case OP_GOOD_REARM_TIME:
            sexp_good_time_to_rearm (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_GRANT_PROMOTION:
            sexp_grant_promotion ();
            sexp_val = SEXP_TRUE;
            break;

        case OP_GRANT_MEDAL:
            sexp_grant_medal (node);
            sexp_val = SEXP_TRUE;
            break;

        // Goober5000 - sigh, was this messed up all along?
        case OP_WARP_BROKEN:
        case OP_WARP_NOT_BROKEN:
        case OP_WARP_NEVER:
        case OP_WARP_ALLOWED:
            sexp_deal_with_warp (node, (op_num == OP_WARP_BROKEN) || (op_num == OP_WARP_NOT_BROKEN), (op_num == OP_WARP_BROKEN) || (op_num == OP_WARP_NEVER));
            sexp_val = SEXP_TRUE;
            break;

        case OP_GOOD_SECONDARY_TIME:
            sexp_good_secondary_time (node);
            sexp_val = SEXP_TRUE;
            break;

        // sexpressions to allow shpis/weapons during the course of a mission
        case OP_ALLOW_SHIP:
            sexp_allow_ship (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_ALLOW_WEAPON:
            sexp_allow_weapon (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_TECH_ADD_SHIP:
            sexp_tech_add_ship (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_TECH_ADD_WEAPON:
            sexp_tech_add_weapon (node);
            sexp_val = SEXP_TRUE;
            break;

            // in the case of a red_alert mission, simply call the red alert
            // function to close the current campaign's mission and move
            // forward to the next mission
        case OP_RED_ALERT:
            red_alert_start_mission ();
            sexp_val = SEXP_TRUE;
            break;

        // training operators
        case OP_KEY_PRESSED: sexp_val = sexp_key_pressed (node); break;

        case OP_SPECIAL_CHECK: sexp_val = sexp_special_training_check (node); break;

        case OP_KEY_RESET:
            sexp_key_reset (node);
            sexp_val = SEXP_KNOWN_TRUE; // only do it first time in repeating events.
            break;

        case OP_KEY_RESET_MULTIPLE:
            sexp_key_reset (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_MISSILE_LOCKED: sexp_val = sexp_missile_locked (node); break;

        case OP_TARGETED: sexp_val = sexp_targeted (node); break;

        case OP_NODE_TARGETED: sexp_val = sexp_node_targeted (node); break;

        case OP_SPEED: sexp_val = sexp_speed (node); break;

        case OP_SECONDARIES_DEPLETED:
            sexp_val = sexp_secondaries_depleted (node);
            break;

        case OP_FACING: sexp_val = sexp_facing (node); break;
        case OP_FACING2: sexp_val = sexp_facing2 (node); break;
        case OP_ORDER: sexp_val = sexp_order (node); break;
        case OP_WAYPOINT_MISSED: sexp_val = sexp_waypoint_missed (); break;
        case OP_WAYPOINT_TWICE: sexp_val = sexp_waypoint_twice (); break;
        case OP_PATH_FLOWN: sexp_val = sexp_path_flown (); break;

        case OP_TRAINING_MSG:
            sexp_send_training_message (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_FLASH_HUD_GAUGE:
            sexp_flash_hud_gauge (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_SET_TRAINING_CONTEXT_FLY_PATH:
            sexp_set_training_context_fly_path (node);
            sexp_val = SEXP_TRUE;
            break;

        case OP_SET_TRAINING_CONTEXT_SPEED:
            sexp_set_training_context_speed (node);
            sexp_val = SEXP_TRUE;
            break;

        case 0: // zero represents a non-operator
            return eval_num (cur_node);

        case OP_NOP: sexp_val = SEXP_TRUE; break;

        case OP_BEAM_FIRE:
            sexp_beam_fire (node, false);
            sexp_val = SEXP_TRUE;
            break;

        case OP_IS_TAGGED: sexp_val = sexp_is_tagged (node); break;

        case OP_NUM_KILLS:
            sexp_val = sexp_return_player_data (node, op_num);
            break;

        case OP_NUM_TYPE_KILLS: sexp_val = sexp_num_type_kills (node); break;

        case OP_NUM_CLASS_KILLS: sexp_val = sexp_num_class_kills (node); break;

        case OP_BEAM_FREE:
            sexp_val = SEXP_TRUE;
            sexp_beam_free (node);
            break;

        case OP_BEAM_FREE_ALL:
            sexp_val = SEXP_TRUE;
            sexp_beam_free_all (node);
            break;

        case OP_BEAM_LOCK:
            sexp_val = SEXP_TRUE;
            sexp_beam_lock (node);
            break;

        case OP_BEAM_LOCK_ALL:
            sexp_val = SEXP_TRUE;
            sexp_beam_lock_all (node);
            break;

        case OP_TURRET_FREE:
            sexp_val = SEXP_TRUE;
            sexp_turret_free (node);
            break;

        case OP_TURRET_FREE_ALL:
            sexp_val = SEXP_TRUE;
            sexp_turret_free_all (node);
            break;

        case OP_TURRET_LOCK:
            sexp_val = SEXP_TRUE;
            sexp_turret_lock (node);
            break;

        case OP_TURRET_LOCK_ALL:
            sexp_val = SEXP_TRUE;
            sexp_turret_lock_all (node);
            break;

        case OP_ADD_REMOVE_ESCORT:
            sexp_val = SEXP_TRUE;
            sexp_add_remove_escort (node);
            break;

        case OP_AWACS_SET_RADIUS:
            sexp_val = SEXP_TRUE;
            sexp_awacs_set_radius (node);
            break;

        case OP_CAP_WAYPOINT_SPEED:
            sexp_val = SEXP_TRUE;
            sexp_cap_waypoint_speed (node);
            break;

        case OP_TURRET_TAGGED_ONLY_ALL:
            sexp_val = SEXP_TRUE;
            sexp_turret_tagged_only_all (node);
            break;

        case OP_TURRET_TAGGED_CLEAR_ALL:
            sexp_val = SEXP_TRUE;
            sexp_turret_tagged_clear_all (node);
            break;

        case OP_SUBSYS_SET_RANDOM:
            sexp_val = SEXP_TRUE;
            sexp_subsys_set_random (node);
            break;

        case OP_SUPERNOVA_START:
            sexp_val = SEXP_TRUE;
            sexp_supernova_start (node);
            break;

        case OP_SHIELD_RECHARGE_PCT:
            sexp_val = sexp_shield_recharge_pct (node);
            break;

        case OP_ENGINE_RECHARGE_PCT: sexp_val = sexp_engine_recharge_pct (node); break;

        case OP_WEAPON_RECHARGE_PCT: sexp_val = sexp_weapon_recharge_pct (node); break;

        case OP_SHIELD_QUAD_LOW: sexp_val = sexp_shield_quad_low (node); break;

        case OP_SECONDARY_AMMO_PCT:
            sexp_val = sexp_secondary_ammo_pct (node);
            break;

        case OP_IS_SECONDARY_SELECTED:
            sexp_val = sexp_is_secondary_selected (node);
            break;

        case OP_IS_PRIMARY_SELECTED: sexp_val = sexp_is_primary_selected (node); break;

        default:
            break;
        }

        if (Log_event) { add_to_event_log_buffer (get_operator_index (cur_node), sexp_val); }

        ASSERT (!Current_sexp_operator.empty ());
        Current_sexp_operator.pop_back ();

        ASSERTX (sexp_val != UNINITIALIZED, "SEXP %s didn't return a value!", CTEXT (cur_node));

        // if we haven't returned, check the sexp value of the sexpression
        // evaluation.  A special value of known true or known false means that
        // we should set the sexp.value field for short circuit eval.
        if (sexp_val == SEXP_KNOWN_TRUE) {
            Sexp_nodes[cur_node].value = SEXP_KNOWN_TRUE;
            return SEXP_TRUE;
        }

        if (sexp_val == SEXP_KNOWN_FALSE) {
            Sexp_nodes[cur_node].value = SEXP_KNOWN_FALSE;
            return SEXP_FALSE;
        }

        if (sexp_val == SEXP_NAN) {
            Sexp_nodes[cur_node].value = SEXP_NAN; // not a number values are false I would suspect
            return SEXP_FALSE;
        }

        if (sexp_val == SEXP_NAN_FOREVER) {
            Sexp_nodes[cur_node].value = SEXP_NAN_FOREVER;
            return SEXP_FALSE; // Goober5000 changed from sexp_val to
                               // SEXP_FALSE on 2/21/2006 in accordance with
                               // above comment
        }

        if (sexp_val == SEXP_CANT_EVAL) {
            Sexp_nodes[cur_node].value = SEXP_CANT_EVAL;
            Sexp_useful_number = 0; // indicate sexp isn't current yet
            return SEXP_FALSE;
        }

        if (Sexp_nodes[cur_node].value == SEXP_NAN) { // if we had a nan, but now don't, reset the value
            Sexp_nodes[cur_node].value = SEXP_UNKNOWN;
            return sexp_val;
        }

        // now, reconcile positive and negative - Goober5000
        if (sexp_val < 0) {
            int parent_node = find_parent_operator (cur_node);

            // if the SEXP has no parent, the point is moot
            if (parent_node >= 0) {
                int arg_num = find_argnum (parent_node, cur_node);
                ASSERTX (
                    arg_num >= 0,
                    "Error finding sexp argument.  The SEXP is not listed "
                    "among its parent's children.");

                // if we need a positive value, make it positive
                if (query_operator_argument_type (get_operator_index (parent_node), arg_num) == OPF_POSITIVE) { sexp_val *= -1; }
            }
        }

        if (sexp_val) { Sexp_nodes[cur_node].value = SEXP_TRUE; }
        else { Sexp_nodes[cur_node].value = SEXP_FALSE; }

        return sexp_val;
    }
}

// get_sexp_main reads and builds the internal representation for a
// symbolic expression.
// On entry:
// Mp points at first character in expression.
// The symbolic expression is built in Sexp_nodes beginning at node 0.
int get_sexp_main () {
    int start_node, op;

    ignore_white_space ();

    if (*Mp != '(') {
        char buf[512];
        strncpy (buf, Mp, 511);
        if (buf[511] != '\0') strcpy (&buf[506], "[...]");

        ASSERTX (0, "Expected to find an open parenthesis in the following sexp:\n%s", buf);
        return -1;
    }

    Mp++;
    start_node = get_sexp ();

    // only need to check syntax if we have a operator
    if (start_node >= 0) {
        op = get_operator_index (CTEXT (start_node));
        if (op < 0) {
            ASSERTX (0, "Can't find operator %s in operator list!\n", CTEXT (start_node));
            return -1;
        }
    }

    return start_node;
}

int run_sexp (const char* sexpression) {
    char* oldMp = Mp;
    int n, i, sexp_val = UNINITIALIZED;
    char buf[8192];

    strcpy (buf, sexpression);

    // HACK: ! -> "
    for (i = 0; i < (int)strlen (buf); i++)
        if (buf[i] == '!') buf[i] = '\"';

    Mp = buf;

    n = get_sexp_main ();
    if (n != -1) {
        sexp_val = eval_sexp (n);
        free_sexp2 (n);
    }
    Mp = oldMp;

    return sexp_val;
}

DCF (
    sexpc,
    "Always runs the given sexp command (Warning! There is no undo for "
    "this!)") {
    std::string sexp;
    std::string sexp_always;

    if (dc_optional_string_either ("help", "--help")) {
        dc_printf (
            "Usage: sexpc sexpression\n. Always runs the given sexp as '( "
            "when ( true ) ( sexp ) )' .\n");
        return;
    }

    dc_stuff_string (sexp);

    sexp_always = "( when ( true ) ( " + sexp + " ) )";

    int sexp_val = run_sexp (sexp_always.c_str ());
    dc_printf ("SEXP '%s' run, sexp_val = %d\n", sexp_always.c_str (), sexp_val);
}

DCF (sexp, "Runs the given sexp") {
    std::string sexp;

    if (dc_optional_string_either ("help", "--help")) {
        dc_printf ("Usage: sexp 'sexpression'\n. Runs the given sexp.\n");
        return;
    }

    dc_stuff_string (sexp);

    int sexp_val = run_sexp (sexp.c_str ());
    dc_printf ("SEXP '%s' run, sexp_val = %d\n", sexp.c_str (), sexp_val);
}

// returns the data type returned by an operator
int query_operator_return_type (int op) {
    if (op < FIRST_OP) {
        ASSERT (op >= 0 && op < (int)Operators.size ());
        op = Operators[op].value;
    }

    switch (op) {
    case OP_TRUE:
    case OP_FALSE:
    case OP_AND:
    case OP_AND_IN_SEQUENCE:
    case OP_OR:
    case OP_NOT:
    case OP_EQUALS:
    case OP_GREATER_THAN:
    case OP_LESS_THAN:
    case OP_IS_DESTROYED:
    case OP_IS_SUBSYSTEM_DESTROYED:
    case OP_IS_DISABLED:
    case OP_IS_DISARMED:
    case OP_HAS_DOCKED:
    case OP_HAS_UNDOCKED:
    case OP_HAS_ARRIVED:
    case OP_HAS_DEPARTED:
    case OP_IS_DESTROYED_DELAY:
    case OP_IS_SUBSYSTEM_DESTROYED_DELAY:
    case OP_IS_DISABLED_DELAY:
    case OP_IS_DISARMED_DELAY:
    case OP_HAS_DOCKED_DELAY:
    case OP_HAS_UNDOCKED_DELAY:
    case OP_HAS_ARRIVED_DELAY:
    case OP_HAS_DEPARTED_DELAY:
    case OP_IS_IFF:
    case OP_HAS_TIME_ELAPSED:
    case OP_GOAL_INCOMPLETE:
    case OP_GOAL_TRUE_DELAY:
    case OP_GOAL_FALSE_DELAY:
    case OP_EVENT_INCOMPLETE:
    case OP_EVENT_TRUE_DELAY:
    case OP_EVENT_FALSE_DELAY:
    case OP_PREVIOUS_EVENT_TRUE:
    case OP_PREVIOUS_EVENT_FALSE:
    case OP_PREVIOUS_EVENT_INCOMPLETE:
    case OP_PREVIOUS_GOAL_TRUE:
    case OP_PREVIOUS_GOAL_FALSE:
    case OP_PREVIOUS_GOAL_INCOMPLETE:
    case OP_WAYPOINTS_DONE:
    case OP_WAYPOINTS_DONE_DELAY:
    case OP_SHIP_TYPE_DESTROYED:
    case OP_LAST_ORDER_TIME:
    case OP_KEY_PRESSED:
    case OP_TARGETED:
    case OP_SPEED:
    case OP_FACING:
    case OP_FACING2:
    case OP_ORDER:
    case OP_WAYPOINT_MISSED:
    case OP_WAYPOINT_TWICE:
    case OP_PATH_FLOWN:
    case OP_EVENT_TRUE:
    case OP_EVENT_FALSE:
    case OP_SKILL_LEVEL_AT_LEAST:
    case OP_IS_CARGO_KNOWN:
    case OP_HAS_BEEN_TAGGED_DELAY:
    case OP_CAP_SUBSYS_CARGO_KNOWN_DELAY:
    case OP_CARGO_KNOWN_DELAY:
    case OP_WAS_PROMOTION_GRANTED:
    case OP_WAS_MEDAL_GRANTED:
    case OP_PERCENT_SHIPS_DEPARTED:
    case OP_PERCENT_SHIPS_DESTROYED:
    case OP_DEPART_NODE_DELAY:
    case OP_DESTROYED_DEPARTED_DELAY:
    case OP_SPECIAL_CHECK:
    case OP_IS_TAGGED:
    case OP_SECONDARIES_DEPLETED:
    case OP_SHIELD_QUAD_LOW:
    case OP_IS_SECONDARY_SELECTED:
    case OP_IS_PRIMARY_SELECTED:
        return OPR_BOOL;

    case OP_PLUS:
    case OP_MINUS:
    case OP_MOD:
    case OP_MUL:
    case OP_DIV:
    case OP_RAND:
        return OPR_NUMBER;

    case OP_TIME_SHIP_DESTROYED:
    case OP_TIME_SHIP_ARRIVED:
    case OP_TIME_SHIP_DEPARTED:
    case OP_TIME_WING_DESTROYED:
    case OP_TIME_WING_ARRIVED:
    case OP_TIME_WING_DEPARTED:
    case OP_MISSION_TIME:
    case OP_TIME_DOCKED:
    case OP_TIME_UNDOCKED:
    case OP_SHIELDS_LEFT:
    case OP_HITS_LEFT:
    case OP_HITS_LEFT_SUBSYSTEM:
    case OP_DISTANCE:
    case OP_NUM_PLAYERS:
    case OP_NUM_KILLS:
    case OP_NUM_TYPE_KILLS:
    case OP_NUM_CLASS_KILLS:
    case OP_SHIELD_RECHARGE_PCT:
    case OP_ENGINE_RECHARGE_PCT:
    case OP_WEAPON_RECHARGE_PCT:
    case OP_SECONDARY_AMMO_PCT:
    case OP_SPECIAL_WARP_DISTANCE:
    case OP_IS_SHIP_VISIBLE:
    case OP_TEAM_SCORE:
        return OPR_POSITIVE;

    case OP_COND:
    case OP_WHEN:
    case OP_CHANGE_IFF:
    case OP_CLEAR_SHIP_GOALS:
    case OP_CLEAR_WING_GOALS:
    case OP_CLEAR_GOALS:
    case OP_ADD_SHIP_GOAL:
    case OP_ADD_WING_GOAL:
    case OP_ADD_GOAL:
    case OP_PROTECT_SHIP:
    case OP_UNPROTECT_SHIP:
    case OP_BEAM_PROTECT_SHIP:
    case OP_BEAM_UNPROTECT_SHIP:
    case OP_NOP:
    case OP_GOALS_ID:
    case OP_SEND_MESSAGE:
    case OP_SELF_DESTRUCT:
    case OP_NEXT_MISSION:
    case OP_END_CAMPAIGN:
    case OP_END_OF_CAMPAIGN:
    case OP_SABOTAGE_SUBSYSTEM:
    case OP_REPAIR_SUBSYSTEM:
    case OP_INVALIDATE_GOAL:
    case OP_VALIDATE_GOAL:
    case OP_SEND_RANDOM_MESSAGE:
    case OP_TRANSFER_CARGO:
    case OP_EXCHANGE_CARGO:
    case OP_CARGO_NO_DEPLETE:
    case OP_KEY_RESET:
    case OP_TRAINING_MSG:
    case OP_SET_TRAINING_CONTEXT_FLY_PATH:
    case OP_SET_TRAINING_CONTEXT_SPEED:
    case OP_SET_SUBSYSTEM_STRNGTH:
    case OP_GOOD_REARM_TIME:
    case OP_GRANT_PROMOTION:
    case OP_GRANT_MEDAL:
    case OP_ALLOW_SHIP:
    case OP_ALLOW_WEAPON:
    case OP_TECH_ADD_SHIP:
    case OP_TECH_ADD_WEAPON:
    case OP_WARP_BROKEN:
    case OP_WARP_NOT_BROKEN:
    case OP_WARP_NEVER:
    case OP_WARP_ALLOWED:
    case OP_FLASH_HUD_GAUGE:
    case OP_GOOD_SECONDARY_TIME:
    case OP_SHIP_VISIBLE:
    case OP_SHIP_INVISIBLE:
    case OP_SHIP_VULNERABLE:
    case OP_SHIP_INVULNERABLE:
    case OP_SHIP_GUARDIAN:
    case OP_SHIP_NO_GUARDIAN:
    case OP_SHIP_VANISH:
    case OP_RED_ALERT:
    case OP_MODIFY_VARIABLE:
    case OP_BEAM_FIRE:
    case OP_BEAM_FREE:
    case OP_BEAM_FREE_ALL:
    case OP_BEAM_LOCK:
    case OP_BEAM_LOCK_ALL:
    case OP_TURRET_FREE:
    case OP_TURRET_FREE_ALL:
    case OP_TURRET_LOCK:
    case OP_TURRET_LOCK_ALL:
    case OP_ADD_REMOVE_ESCORT:
    case OP_AWACS_SET_RADIUS:
    case OP_SEND_MESSAGE_LIST:
    case OP_CAP_WAYPOINT_SPEED:
    case OP_TURRET_TAGGED_ONLY_ALL:
    case OP_TURRET_TAGGED_CLEAR_ALL:
    case OP_SUBSYS_SET_RANDOM:
    case OP_SUPERNOVA_START:
    case OP_SET_SPECIAL_WARPOUT_NAME:
        return OPR_NULL;

    case OP_AI_CHASE:
    case OP_AI_CHASE_WING:
    case OP_AI_CHASE_ANY:
    case OP_AI_DOCK:
    case OP_AI_UNDOCK:
    case OP_AI_WARP: // this particular operator is obsolete
    case OP_AI_WARP_OUT:
    case OP_AI_WAYPOINTS:
    case OP_AI_WAYPOINTS_ONCE:
    case OP_AI_DESTROY_SUBSYS:
    case OP_AI_DISABLE_SHIP:
    case OP_AI_DISARM_SHIP:
    case OP_AI_GUARD:
    case OP_AI_GUARD_WING:
    case OP_AI_EVADE_SHIP:
    case OP_AI_STAY_NEAR_SHIP:
    case OP_AI_KEEP_SAFE_DISTANCE:
    case OP_AI_IGNORE:
    case OP_AI_STAY_STILL:
    case OP_AI_PLAY_DEAD:
        return OPR_AI_GOAL;

    default: break;
    }

    return 0;
}

/**
 * Return the data type of a specified argument to an operator.
 *
 * @param op operator index
 * @param argnum is 0 indexed.
 */
int query_operator_argument_type (int op, int argnum) {
    int index = op;

    if (op < FIRST_OP) {
        ASSERT (index >= 0 && index < (int)Operators.size ());
        op = Operators[index].value;
    }
    else {
        WARNINGF (LOCATION, "Possible unnecessary search for operator index.  Trace out and see if this is necessary.");

        for (index = 0; index < (int)Operators.size (); index++)
            if (Operators[index].value == op) break;

        ASSERT (index < (int)Operators.size ());
    }

    if (argnum >= Operators[index].max) return OPF_NONE;

    switch (op) {
    case OP_TRUE:
    case OP_FALSE:
    case OP_MISSION_TIME:
    case OP_NOP:
    case OP_WAYPOINT_MISSED:
    case OP_WAYPOINT_TWICE:
    case OP_PATH_FLOWN:
    case OP_GRANT_PROMOTION:
    case OP_WAS_PROMOTION_GRANTED:
    case OP_RED_ALERT:
        return OPF_NONE;

    case OP_AND:
    case OP_AND_IN_SEQUENCE:
    case OP_OR:
    case OP_NOT:
        return OPF_BOOL;

    case OP_PLUS:
    case OP_MINUS:
    case OP_MOD:
    case OP_MUL:
    case OP_DIV:
    case OP_EQUALS:
    case OP_GREATER_THAN:
    case OP_LESS_THAN:
    case OP_RAND:
        return OPF_NUMBER;

    case OP_HAS_TIME_ELAPSED:
    case OP_SPEED:
    case OP_SET_TRAINING_CONTEXT_SPEED:
    case OP_SPECIAL_CHECK:
    case OP_AI_WARP_OUT:
    case OP_TEAM_SCORE:
        return OPF_POSITIVE;

    case OP_AI_WARP: // this operator is obsolete
    case OP_SET_TRAINING_CONTEXT_FLY_PATH:
        if (!argnum)
            return OPF_WAYPOINT_PATH;
        else
            return OPF_NUMBER;

    case OP_AI_WAYPOINTS:
    case OP_AI_WAYPOINTS_ONCE:
        if (argnum == 0)
            return OPF_WAYPOINT_PATH;
        else
            return OPF_POSITIVE;

    case OP_IS_DISABLED:
    case OP_IS_DISARMED:
    case OP_TIME_SHIP_DESTROYED:
    case OP_TIME_SHIP_ARRIVED:
    case OP_TIME_SHIP_DEPARTED:
    case OP_SHIELDS_LEFT:
    case OP_HITS_LEFT:
    case OP_CLEAR_SHIP_GOALS:
    case OP_PROTECT_SHIP:
    case OP_UNPROTECT_SHIP:
    case OP_BEAM_PROTECT_SHIP:
    case OP_BEAM_UNPROTECT_SHIP:
    case OP_TRANSFER_CARGO:
    case OP_EXCHANGE_CARGO:
    case OP_SHIP_INVISIBLE:
    case OP_SHIP_VISIBLE:
    case OP_SHIP_INVULNERABLE:
    case OP_SHIP_VULNERABLE:
    case OP_SHIP_GUARDIAN:
    case OP_SHIP_NO_GUARDIAN:
    case OP_SHIP_VANISH:
    case OP_SECONDARIES_DEPLETED:
    case OP_SPECIAL_WARP_DISTANCE:
    case OP_SET_SPECIAL_WARPOUT_NAME:
    case OP_IS_SHIP_VISIBLE:
        return OPF_SHIP;

    case OP_IS_DESTROYED:
    case OP_HAS_ARRIVED:
    case OP_HAS_DEPARTED:
    case OP_CLEAR_GOALS:
        return OPF_SHIP_WING;

    case OP_IS_DISABLED_DELAY:
    case OP_IS_DISARMED_DELAY:
        if (argnum == 0)
            return OPF_POSITIVE;
        else
            return OPF_SHIP;

    case OP_FACING:
        if (argnum == 0)
            return OPF_SHIP;
        else
            return OPF_POSITIVE;

    case OP_FACING2:
        if (argnum == 0) { return OPF_WAYPOINT_PATH; }
        else { return OPF_POSITIVE; }

    case OP_ORDER:
        if (argnum == 1)
            return OPF_AI_ORDER;
        else
            return OPF_SHIP_WING; // arg 0 or 2

    case OP_IS_DESTROYED_DELAY:
    case OP_HAS_ARRIVED_DELAY:
    case OP_HAS_DEPARTED_DELAY:
    case OP_LAST_ORDER_TIME:
        if (argnum == 0)
            return OPF_POSITIVE;
        else
            return OPF_SHIP_WING;

    case OP_MODIFY_VARIABLE:
        if (argnum == 0) { return OPF_VARIABLE_NAME; }
        else { return OPF_AMBIGUOUS; }

    case OP_HAS_DOCKED:
    case OP_HAS_UNDOCKED:
    case OP_HAS_DOCKED_DELAY:
    case OP_HAS_UNDOCKED_DELAY:
    case OP_TIME_DOCKED:
    case OP_TIME_UNDOCKED:
        if (argnum < 2)
            return OPF_SHIP;
        else
            return OPF_POSITIVE;

    case OP_TIME_WING_DESTROYED:
    case OP_TIME_WING_ARRIVED:
    case OP_TIME_WING_DEPARTED:
    case OP_CLEAR_WING_GOALS:
        return OPF_WING;

    case OP_IS_SUBSYSTEM_DESTROYED:
        if (!argnum)
            return OPF_SHIP;
        else
            return OPF_SUBSYSTEM;

    case OP_HITS_LEFT_SUBSYSTEM:
        if (argnum == 0)
            return OPF_SHIP;
        else if (argnum == 1)
            return OPF_SUBSYSTEM;
        else
            return OPF_BOOL;

    case OP_TARGETED:
        if (!argnum)
            return OPF_SHIP;
        else if (argnum == 1)
            return OPF_POSITIVE;
        else
            return OPF_SUBSYSTEM;

    case OP_IS_SUBSYSTEM_DESTROYED_DELAY:
        if (argnum == 0)
            return OPF_SHIP;
        else if (argnum == 1)
            return OPF_SUBSYSTEM;
        else
            return OPF_POSITIVE;

    case OP_IS_IFF:
    case OP_CHANGE_IFF:
        if (!argnum)
            return OPF_IFF;
        else
            return OPF_SHIP_WING;

    case OP_ADD_SHIP_GOAL:
        if (!argnum)
            return OPF_SHIP;
        else
            return OPF_AI_GOAL;

    case OP_ADD_WING_GOAL:
        if (!argnum)
            return OPF_WING;
        else
            return OPF_AI_GOAL;

    case OP_ADD_GOAL:
        if (argnum == 0)
            return OPF_SHIP_WING;
        else
            return OPF_AI_GOAL;

    case OP_COND:
    case OP_WHEN:
        if (!argnum)
            return OPF_BOOL;
        else
            return OPF_NULL;

    case OP_AI_DISABLE_SHIP:
    case OP_AI_DISARM_SHIP:
        if (argnum == 0)
            return OPF_SHIP;
        else if (argnum == 1)
            return OPF_POSITIVE;
        else
            return OPF_BOOL;

    case OP_AI_EVADE_SHIP:
    case OP_AI_STAY_NEAR_SHIP:
    case OP_AI_IGNORE:
        if (!argnum)
            return OPF_SHIP;
        else
            return OPF_POSITIVE;

    case OP_AI_CHASE:
        if (argnum == 0)
            return OPF_SHIP_WING;
        else if (argnum == 1)
            return OPF_POSITIVE;
        else
            return OPF_BOOL;

    case OP_AI_CHASE_WING:
        if (argnum == 0)
            return OPF_WING;
        else if (argnum == 1)
            return OPF_POSITIVE;
        else
            return OPF_BOOL;

    case OP_AI_GUARD:
        if (!argnum)
            return OPF_SHIP_WING;
        else
            return OPF_POSITIVE;

    case OP_AI_GUARD_WING:
        if (!argnum)
            return OPF_WING;
        else
            return OPF_POSITIVE;

    case OP_AI_KEEP_SAFE_DISTANCE:
        return OPF_POSITIVE;

    case OP_AI_DOCK:
        if (!argnum)
            return OPF_SHIP;
        else if (argnum == 1)
            return OPF_DOCKER_POINT;
        else if (argnum == 2)
            return OPF_DOCKEE_POINT;
        else
            return OPF_POSITIVE;

    case OP_AI_UNDOCK:
        if (argnum == 0)
            return OPF_POSITIVE;
        else
            return OPF_SHIP;

    case OP_AI_DESTROY_SUBSYS:
        if (!argnum)
            return OPF_SHIP;
        else if (argnum == 1)
            return OPF_SUBSYSTEM;
        else if (argnum == 2)
            return OPF_POSITIVE;
        else
            return OPF_BOOL;

    case OP_GOALS_ID:
        return OPF_AI_GOAL;

    case OP_SEND_MESSAGE:
    case OP_SEND_RANDOM_MESSAGE:
        if (argnum == 0)
            return OPF_WHO_FROM;
        else if (argnum == 1)
            return OPF_PRIORITY;
        else
            return OPF_MESSAGE;

    case OP_SEND_MESSAGE_LIST: {
        // every four, the value repeats
        int a_mod = argnum % 4;

        // who from
        if (a_mod == 0)
            return OPF_WHO_FROM;
        else if (a_mod == 1)
            return OPF_PRIORITY;
        else if (a_mod == 2)
            return OPF_MESSAGE;
        else if (a_mod == 3)
            return OPF_POSITIVE;
        else
            // This can't happen
            return OPF_NONE;
    }

    case OP_TRAINING_MSG:
        if (argnum < 2)
            return OPF_MESSAGE;
        else
            return OPF_POSITIVE;

    case OP_SELF_DESTRUCT:
        return OPF_SHIP;

    case OP_NEXT_MISSION:
        return OPF_MISSION_NAME;

    case OP_END_CAMPAIGN:
        return OPF_BOOL;

    case OP_END_OF_CAMPAIGN:
        return OPF_NONE;

    case OP_PREVIOUS_GOAL_TRUE:
    case OP_PREVIOUS_GOAL_FALSE:
        if (argnum == 0)
            return OPF_MISSION_NAME;
        else if (argnum == 1)
            return OPF_GOAL_NAME;
        else
            return OPF_BOOL;

    case OP_PREVIOUS_GOAL_INCOMPLETE:
        return OPF_GOAL_NAME;

    case OP_PREVIOUS_EVENT_TRUE:
    case OP_PREVIOUS_EVENT_FALSE:
    case OP_PREVIOUS_EVENT_INCOMPLETE:
        if (!argnum)
            return OPF_MISSION_NAME;
        else if (argnum == 1)
            return OPF_EVENT_NAME;
        else
            return OPF_BOOL;

    case OP_SABOTAGE_SUBSYSTEM:
        if (!argnum)
            return OPF_SHIP; // changed from OPF_SHIP_NOT_PLAYER by Goober5000:
                             // now it can be the player ship also
        else
            return OPF_POSITIVE;

    case OP_REPAIR_SUBSYSTEM:
    case OP_SET_SUBSYSTEM_STRNGTH:
        if (!argnum)
            return OPF_SHIP; // changed from OPF_SHIP_NOT_PLAYER by Goober5000:
                             // now it can be the player ship also
        else if (argnum == 2)
            return OPF_POSITIVE;
        else
            return OPF_BOOL;

    case OP_WAYPOINTS_DONE:
        if (argnum == 0)
            return OPF_SHIP_WING;
        else
            return OPF_WAYPOINT_PATH;

    case OP_WAYPOINTS_DONE_DELAY:
        if (argnum == 0)
            return OPF_SHIP_WING;
        else if (argnum == 1)
            return OPF_WAYPOINT_PATH;
        else
            return OPF_POSITIVE;

    case OP_INVALIDATE_GOAL:
    case OP_VALIDATE_GOAL:
        return OPF_GOAL_NAME;

    case OP_SHIP_TYPE_DESTROYED:
        if (argnum == 0)
            return OPF_POSITIVE;
        else
            return OPF_SHIP_TYPE;

    case OP_KEY_PRESSED:
        if (!argnum)
            return OPF_KEYPRESS;
        else
            return OPF_POSITIVE;

    case OP_KEY_RESET:
        return OPF_KEYPRESS;

    case OP_EVENT_TRUE:
    case OP_EVENT_FALSE:
        return OPF_EVENT_NAME;

    case OP_EVENT_INCOMPLETE:
    case OP_EVENT_TRUE_DELAY:
    case OP_EVENT_FALSE_DELAY:
        if (argnum == 0)
            return OPF_EVENT_NAME;
        else if (argnum == 1)
            return OPF_POSITIVE;
        else if (argnum == 2)
            return OPF_BOOL;
        else
            return OPF_NONE;

    case OP_GOAL_INCOMPLETE:
    case OP_GOAL_TRUE_DELAY:
    case OP_GOAL_FALSE_DELAY:
        if (!argnum)
            return OPF_GOAL_NAME;
        else
            return OPF_POSITIVE;

    case OP_AI_PLAY_DEAD:
    case OP_AI_CHASE_ANY:
        return OPF_POSITIVE;

    case OP_AI_STAY_STILL:
        if (!argnum)
            return OPF_SHIP_POINT;
        else
            return OPF_POSITIVE;

    case OP_GOOD_REARM_TIME:
        if (argnum == 0)
            return OPF_IFF;
        else
            return OPF_POSITIVE;

    case OP_NUM_PLAYERS:
        return OPF_POSITIVE;

    case OP_SKILL_LEVEL_AT_LEAST:
        return OPF_SKILL_LEVEL;

    case OP_GRANT_MEDAL:
    case OP_WAS_MEDAL_GRANTED:
        return OPF_MEDAL_NAME;

    case OP_IS_CARGO_KNOWN:
        return OPF_SHIP;

    case OP_CARGO_KNOWN_DELAY:
        if (argnum == 0)
            return OPF_POSITIVE;
        else
            return OPF_SHIP;

    case OP_HAS_BEEN_TAGGED_DELAY:
        if (argnum == 0) { return OPF_POSITIVE; }
        else { return OPF_SHIP; }

    case OP_CAP_SUBSYS_CARGO_KNOWN_DELAY:
        if (argnum == 0) { return OPF_POSITIVE; }
        else if (argnum == 1) { return OPF_SHIP; }
        else { return OPF_SUBSYSTEM; }

    case OP_ALLOW_SHIP:
    case OP_TECH_ADD_SHIP:
        return OPF_SHIP_CLASS_NAME;

    case OP_ALLOW_WEAPON:
    case OP_TECH_ADD_WEAPON:
        return OPF_WEAPON_NAME;

    case OP_WARP_BROKEN:
    case OP_WARP_NOT_BROKEN:
    case OP_WARP_NEVER:
    case OP_WARP_ALLOWED:
        return OPF_SHIP;

    case OP_FLASH_HUD_GAUGE:
        return OPF_HUD_GAUGE_NAME;

    case OP_GOOD_SECONDARY_TIME:
        if (argnum == 0)
            return OPF_IFF;
        else if (argnum == 1)
            return OPF_POSITIVE;
        else if (argnum == 2)
            return OPF_HUGE_WEAPON;
        else
            return OPF_SHIP;

    case OP_PERCENT_SHIPS_DEPARTED:
    case OP_PERCENT_SHIPS_DESTROYED:
        if (argnum == 0) { return OPF_POSITIVE; }
        else { return OPF_SHIP_WING; }
        break;

    case OP_DEPART_NODE_DELAY:
        if (argnum == 0) { return OPF_POSITIVE; }
        else if (argnum == 1) { return OPF_JUMP_NODE_NAME; }
        else { return OPF_SHIP; }

    case OP_DESTROYED_DEPARTED_DELAY:
        if (argnum == 0) { return OPF_POSITIVE; }
        else { return OPF_SHIP_WING; }

    case OP_CARGO_NO_DEPLETE:
        if (argnum == 0) { return OPF_SHIP; }
        else { return OPF_NUMBER; }

    case OP_BEAM_FIRE:
        switch (argnum) {
        case 0:
        return OPF_SHIP;
        case 1:
        return OPF_SUBSYSTEM;
        case 2:
        return OPF_SHIP;
        case 3:
        return OPF_SUBSYSTEM;
        case 4:
        return OPF_BOOL;
        default: ASSERT (0); return OPF_NULL;
        }

    case OP_IS_TAGGED:
        return OPF_SHIP;

    case OP_NUM_KILLS:
        return OPF_SHIP;

    case OP_NUM_TYPE_KILLS:
        if (argnum == 0) { return OPF_SHIP; }
        else { return OPF_SHIP_TYPE; }

    case OP_NUM_CLASS_KILLS:
        if (argnum == 0) { return OPF_SHIP; }
        else { return OPF_SHIP_CLASS_NAME; }

    case OP_BEAM_FREE:
    case OP_BEAM_LOCK:
    case OP_TURRET_FREE:
    case OP_TURRET_LOCK:
        if (argnum == 0) { return OPF_SHIP; }
        else { return OPF_SUBSYSTEM; }

    case OP_BEAM_FREE_ALL:
    case OP_BEAM_LOCK_ALL:
    case OP_TURRET_FREE_ALL:
    case OP_TURRET_LOCK_ALL:
    case OP_TURRET_TAGGED_ONLY_ALL:
    case OP_TURRET_TAGGED_CLEAR_ALL:
        return OPF_SHIP;

    case OP_ADD_REMOVE_ESCORT:
        if (argnum == 0) { return OPF_SHIP; }
        else { return OPF_NUMBER; }

    case OP_AWACS_SET_RADIUS:
        if (argnum == 0) { return OPF_SHIP; }
        else if (argnum == 1) { return OPF_AWACS_SUBSYSTEM; }
        else { return OPF_NUMBER; }

    case OP_CAP_WAYPOINT_SPEED:
        if (argnum == 0) { return OPF_SHIP; }
        else { return OPF_NUMBER; }

    case OP_SUBSYS_SET_RANDOM:
        if (argnum == 0) { return OPF_SHIP; }
        else if (argnum == 1 || argnum == 2) { return OPF_NUMBER; }
        else { return OPF_SUBSYSTEM; }

    case OP_SUPERNOVA_START:
        return OPF_POSITIVE;

    case OP_SHIELD_RECHARGE_PCT:
    case OP_WEAPON_RECHARGE_PCT:
    case OP_ENGINE_RECHARGE_PCT:
        return OPF_SHIP;

    case OP_SHIELD_QUAD_LOW:
        if (argnum == 0) { return OPF_SHIP; }
        else { return OPF_NUMBER; }

    case OP_SECONDARY_AMMO_PCT:
        if (argnum == 0) { return OPF_SHIP; }
        else { return OPF_NUMBER; }

    case OP_IS_SECONDARY_SELECTED:
    case OP_IS_PRIMARY_SELECTED:
        if (argnum == 0) { return OPF_SHIP; }
        else { return OPF_NUMBER; }

    default:
        break;
    }

    return 0;
}

// DA: 1/7/99  Used to rename ships and waypoints, not variables
// Strictly used in FRED
void update_sexp_references (const char* old_name, const char* new_name) {
    int i;

    ASSERT (strlen (new_name) < TOKEN_LENGTH);
    for (i = 0; i < Num_sexp_nodes; i++) {
        if ((SEXP_NODE_TYPE (i) == SEXP_ATOM) && (Sexp_nodes[i].subtype == SEXP_ATOM_STRING))
            if (!strcasecmp (CTEXT (i), old_name)) strcpy (CTEXT (i), new_name);
    }
}

// DA: 1/7/99  Used to rename event names, goal names, not variables
// Strictly used in FRED
void update_sexp_references (const char* old_name, const char* new_name, int format) {
    int i;
    if (!strcmp (old_name, new_name)) { return; }

    ASSERT (strlen (new_name) < TOKEN_LENGTH);
    for (i = 0; i < Num_sexp_nodes; i++) {
        if (is_sexp_top_level (i)) update_sexp_references (old_name, new_name, format, i);
    }
}

// DA: 1/7/99  Used to rename event names, goal names, not variables
// recursive function to update references to a certain type of data
void update_sexp_references (const char* old_name, const char* new_name, int format, int node) {
    int i, n, op;

    if (node < 0) return;

    if ((SEXP_NODE_TYPE (node) == SEXP_LIST) && (Sexp_nodes[node].subtype == SEXP_ATOM_LIST)) {
        if (Sexp_nodes[node].first) update_sexp_references (old_name, new_name, format, Sexp_nodes[node].first);

        if (Sexp_nodes[node].rest) update_sexp_references (old_name, new_name, format, Sexp_nodes[node].rest);

        return;
    }

    if (SEXP_NODE_TYPE (node) != SEXP_ATOM) return;

    if (Sexp_nodes[node].subtype != SEXP_ATOM_OPERATOR) return;

    op = get_operator_index (CTEXT (node));
    ASSERT (Sexp_nodes[node].first < 0);
    n = Sexp_nodes[node].rest;
    i = 0;
    while (n >= 0) {
        if (SEXP_NODE_TYPE (n) == SEXP_LIST) { update_sexp_references (old_name, new_name, format, Sexp_nodes[n].first); }
        else {
            ASSERT ((SEXP_NODE_TYPE (n) == SEXP_ATOM) && ((Sexp_nodes[n].subtype == SEXP_ATOM_NUMBER) || (Sexp_nodes[n].subtype == SEXP_ATOM_STRING)));

            if (query_operator_argument_type (op, i) == format) {
                if (!strcasecmp (CTEXT (n), old_name)) strcpy (CTEXT (n), new_name);
            }
        }

        n = Sexp_nodes[n].rest;
        i++;
    }
}


int verify_vector (char* text) {
    char* str;

    if (text == NULL) return -1;

    auto len = strlen (text);
    if (text[0] != '(' || text[len - 1] != ')') { return -1; }

    str = &text[0];
    skip_white (&str);
    if (*str != '(') { return -1; }

    str++;
    skip_white (&str);
    if (validate_float (&str)) { return -1; }

    skip_white (&str);
    if (validate_float (&str)) { return -1; }

    skip_white (&str);
    if (validate_float (&str)) { return -1; }

    skip_white (&str);
    if (*str != ')') { return -1; }

    str++;
    skip_white (&str);
    if (*str) { return -1; }

    return 0;
}

void skip_white (char** str) {
    if ((**str == ' ') || (**str == '\t')) { (*str)++; }
}

int validate_float (char** str) {
    int count = 0, dot = 0;

    while (isdigit (**str) || **str == '.') {
        if (**str == '.') {
            if (dot) { return -1; }

            dot = 1;
        }

        (*str)++;
        count++;
    }

    if (!count) { return -1; }

    return 0;
}

/**
 * Check if operator return type opr is a valid match for operator argument
 * type opf
 */

const char* sexp_error_message (int num) {
    switch (num) {
    case SEXP_CHECK_NONOP_ARGS:
        return "Data shouldn't have arguments";

    case SEXP_CHECK_OP_EXPECTED:
        return "Operator expected instead of data";

    case SEXP_CHECK_UNKNOWN_OP:
        return "Unrecognized operator";

    case SEXP_CHECK_TYPE_MISMATCH:
        return "Argument type mismatch";

    case SEXP_CHECK_BAD_ARG_COUNT:
        return "Argument count is illegal";

    case SEXP_CHECK_UNKNOWN_TYPE:
        return "Unknown operator argument type";

    case SEXP_CHECK_INVALID_NUM:
        return "Not a number";

    case SEXP_CHECK_INVALID_SHIP:
        return "Invalid ship name";

    case SEXP_CHECK_INVALID_WING:
        return "Invalid wing name";

    case SEXP_CHECK_INVALID_SUBSYS:
        return "Invalid subsystem name";

    case SEXP_CHECK_INVALID_SUBSYS_TYPE:
        return "Invalid subsystem type";

    case SEXP_CHECK_INVALID_IFF:
        return "Invalid team name";

    case SEXP_CHECK_INVALID_AI_CLASS:
        return "Invalid AI class name";

    case SEXP_CHECK_INVALID_POINT:
        return "Invalid point";

    case SEXP_CHECK_NEGATIVE_NUM:
        return "Negative number not allowed";

    case SEXP_CHECK_INVALID_SHIP_WING:
        return "Invalid ship/wing name";

    case SEXP_CHECK_INVALID_SHIP_TYPE:
        return "Invalid ship type";

    case SEXP_CHECK_UNKNOWN_MESSAGE:
        return "Invalid message name";

    case SEXP_CHECK_INVALID_PRIORITY:
        return "Invalid priority";

    case SEXP_CHECK_INVALID_MISSION_NAME:
        return "Invalid mission filename";

    case SEXP_CHECK_INVALID_GOAL_NAME:
        return "Invalid goal name";

    case SEXP_CHECK_INVALID_LEVEL:
        return "Mission level too low in tree";

    case SEXP_CHECK_INVALID_MSG_SOURCE:
        return "Invalid message source";

    case SEXP_CHECK_INVALID_DOCKER_POINT:
        return "Invalid docker point";

    case SEXP_CHECK_INVALID_DOCKEE_POINT:
        return "Invalid dockee point";

    case SEXP_CHECK_ORDER_NOT_ALLOWED: return "Ship not allowed to have this order";

    case SEXP_CHECK_DOCKING_NOT_ALLOWED: return "Ship can't dock with target ship";

    case SEXP_CHECK_NUM_RANGE_INVALID:
        return "Number is out of range";

    case SEXP_CHECK_INVALID_EVENT_NAME: return "Event name is invalid (not known)";

    case SEXP_CHECK_INVALID_SKILL_LEVEL: return "Skill level name is invalid (not known)";

    case SEXP_CHECK_INVALID_MEDAL_NAME:
        return "Invalid medal name";

    case SEXP_CHECK_INVALID_WEAPON_NAME:
        return "Invalid weapon name";

    case SEXP_CHECK_INVALID_INTEL_NAME:
        return "Invalid intel name";

    case SEXP_CHECK_INVALID_SHIP_CLASS_NAME:
        return "Invalid ship class name";

    case SEXP_CHECK_INVALID_GAUGE_NAME:
        return "Invalid HUD gauges name";

    case SEXP_CHECK_INVALID_JUMP_NODE:
        return "Invalid Jump Node name";

    case SEXP_CHECK_UNKNOWN_ERROR:
        return "Unknown error";

    case SEXP_CHECK_INVALID_SUPPORT_SHIP_CLASS: return "Invalid support ship class";

    case SEXP_CHECK_INVALID_SHIP_WITH_BAY: return "Ship does not have a fighter bay";

    case SEXP_CHECK_INVALID_ARRIVAL_LOCATION: return "Invalid arrival location";

    case SEXP_CHECK_INVALID_DEPARTURE_LOCATION: return "Invalid departure location";

    case SEXP_CHECK_INVALID_ARRIVAL_ANCHOR_ALL: return "Invalid universal arrival anchor";

    case SEXP_CHECK_INVALID_SOUNDTRACK_NAME:
        return "Invalid soundtrack name";

    case SEXP_CHECK_INVALID_PERSONA_NAME:
        return "Invalid persona name";

    case SEXP_CHECK_INVALID_VARIABLE:
        return "Invalid variable name";

    case SEXP_CHECK_INVALID_VARIABLE_TYPE:
        return "Invalid variable type";

    case SEXP_CHECK_INVALID_FONT:
        return "Invalid font";

    case SEXP_CHECK_INVALID_HUD_ELEMENT: return "Invalid hud element magic name";

    case SEXP_CHECK_INVALID_SOUND_ENVIRONMENT: return "Invalid sound environment";

    case SEXP_CHECK_INVALID_SOUND_ENVIRONMENT_OPTION: return "Invalid sound environment option";

    case SEXP_CHECK_INVALID_AUDIO_VOLUME_OPTION: return "Invalid audio volume option";

    case SEXP_CHECK_INVALID_EXPLOSION_OPTION: return "Invalid explosion option";

    case SEXP_CHECK_INVALID_SHIP_EFFECT:
        return "Invalid ship effect name";

    case SEXP_CHECK_INVALID_TURRET_TARGET_ORDER: return "Invalid turret target order";

    case SEXP_CHECK_INVALID_ARMOR_TYPE:
        return "Invalid armor type";

    case SEXP_CHECK_INVALID_DAMAGE_TYPE:
        return "Invalid damage type";

    case SEXP_CHECK_INVALID_HUD_GAUGE:
        return "Invalid hud gauge";

    case SEXP_CHECK_INVALID_TARGET_PRIORITIES: return "Invalid target priorities";

    case SEXP_CHECK_INVALID_ANIMATION_TYPE:
        return "Invalid animation type";

    case SEXP_CHECK_INVALID_MISSION_MOOD:
        return "Invalid mission mood";

    case SEXP_CHECK_INVALID_SHIP_FLAG:
        return "Invalid ship flag";

    case SEXP_CHECK_INVALID_TEAM_COLOR: return "Not a valid Team Color setting";

    case SEXP_CHECK_INVALID_GAME_SND:
        return "Invalid game sound";

    case SEXP_CHECK_INVALID_SSM_CLASS:
        return "Invalid SSM class";

    default: WARNINGF (LOCATION, "Unhandled sexp error code %d!", num); return "Unhandled sexp error code!";
    }
}

int query_sexp_ai_goal_valid (int sexp_ai_goal, int ship_num) {
    int i, op;

    for (op = 0; op < (int)Operators.size (); op++)
        if (Operators[op].value == sexp_ai_goal) break;

    ASSERT (op < (int)Operators.size ());
    for (i = 0; i < Num_sexp_ai_goal_links; i++)
        if (Sexp_ai_goal_links[i].op_code == sexp_ai_goal) break;

    ASSERT (i < Num_sexp_ai_goal_links);
    return ai_query_goal_valid (ship_num, Sexp_ai_goal_links[i].ai_goal);
}

// Takes an Sexp_node.text pointing to a variable (of form
// "Sexp_variables[xx]=string" or "Sexp_variables[xx]=number") and returns the
// index into the Sexp_variables array of the actual value
int extract_sexp_variable_index (int node) {
    char* text = Sexp_nodes[node].text;
    char char_index[8];
    char* start_index;
    int variable_index;

    // get past the '['
    start_index = text + 15;
    ASSERT (isdigit (*start_index));

    int len = 0;

    while (*start_index != ']') {
        char_index[len++] = *(start_index++);
        ASSERT (len < 3);
    }

    ASSERT (len > 0);
    char_index[len] = 0; // append null termination to string

    variable_index = atoi (char_index);
    ASSERT ((variable_index >= 0) && (variable_index < MAX_SEXP_VARIABLES));

    return variable_index;
}

/**
 * Wrapper around Sexp_node[xx].text for normal and variable
 */
char* CTEXT (int n) {
    int sexp_variable_index = -1;
    char variable_name[TOKEN_LENGTH];
    char* current_argument;

    ASSERTX (n >= 0 && n < Num_sexp_nodes, "Passed an out-of-range node index (%d) to CTEXT!", n);
    if (n < 0) { return NULL; }

    // Goober5000 - MWAHAHAHAHAHAHAHA!  Thank you, Volition programmers!
    // Without the CTEXT wrapper, when-argument would probably be infeasibly
    // difficult to code.
    if (!strcmp (Sexp_nodes[n].text, SEXP_ARGUMENT_STRING)) {
        // make sure we have an argument to replace it with
        if (Sexp_replacement_arguments.empty ()) return Sexp_nodes[n].text;

        // if the replacement argument is a variable name, get the variable
        // index
        sexp_variable_index = get_index_sexp_variable_name (Sexp_replacement_arguments.back ());

        current_argument = Sexp_replacement_arguments.back ();

        // if the replacement argument is a formatted variable name, get the
        // variable index
        if (current_argument[0] == SEXP_VARIABLE_CHAR) {
            get_unformatted_sexp_variable_name (variable_name, current_argument);
            sexp_variable_index = get_index_sexp_variable_name (variable_name);
        }

        // if we have a variable, return the variable value, else return the
        // regular argument
        if (sexp_variable_index != -1)
            return Sexp_variables[sexp_variable_index].text;
        else
            return Sexp_replacement_arguments.back ();
    }

    // Goober5000 - if not special argument, proceed as normal
    if (Sexp_nodes[n].type & SEXP_FLAG_VARIABLE) {
        sexp_variable_index = atoi (Sexp_nodes[n].text);
        // Reference a Sexp_variable
        // string format -- "Sexp_variables[xx]=number" or
        // "Sexp_variables[xx]=string", where xx is the index

        ASSERT (!(Sexp_variables[sexp_variable_index].type & SEXP_VARIABLE_NOT_USED));
        ASSERT (Sexp_variables[sexp_variable_index].type & SEXP_VARIABLE_SET);

        if (Log_event) {
            Current_event_log_variable_buffer->push_back (Sexp_variables[sexp_variable_index].text);
            Current_event_log_variable_buffer->push_back (Sexp_variables[sexp_variable_index].variable_name);
        }

        return Sexp_variables[sexp_variable_index].text;
    }
    else { return Sexp_nodes[n].text; }
}

/**
 * Set all Sexp_variables to type uninitialized
 */
void init_sexp_vars () {
    for (int i = 0; i < MAX_SEXP_VARIABLES; i++) {
        Sexp_variables[i].type = SEXP_VARIABLE_NOT_USED;
        Block_variables[i].type = SEXP_VARIABLE_NOT_USED;
    }
}

/**
 * Add a variable to the block variable array rather than the Sexp_variables
 * array
 */
void add_block_variable (const char* text, const char* var_name, int type, int index) {
    ASSERT ((index >= 0) && (index < MAX_SEXP_VARIABLES));

    strcpy (Block_variables[index].text, text);
    strcpy (Block_variables[index].variable_name, var_name);
    Block_variables[index].type &= ~SEXP_VARIABLE_NOT_USED;
    Block_variables[index].type = (type | SEXP_VARIABLE_SET);
}

/**
 * Add a Sexp_variable to be used in a mission.
 *
 * This should be called from within mission parse.
 */
int sexp_add_variable (const char* text, const char* var_name, int type, int index) {
    // if index == -1, find next open slot
    if (index == -1) {
        for (int i = 0; i < MAX_SEXP_VARIABLES; i++) {
            if (Sexp_variables[i].type == SEXP_VARIABLE_NOT_USED) {
                index = i;
                break;
            }
        }
    }
    else { ASSERT ((index >= 0) && (index < MAX_SEXP_VARIABLES)); }

    if (index >= 0) {
        strcpy (Sexp_variables[index].text, text);
        strcpy (Sexp_variables[index].variable_name, var_name);
        Sexp_variables[index].type &= ~SEXP_VARIABLE_NOT_USED;
        Sexp_variables[index].type = (type | SEXP_VARIABLE_SET);
    }

    return index;
}

// Goober5000 - minor variant of the above that is now required for variable
// arrays
void sexp_add_array_block_variable (int index, bool is_numeric) {
    ASSERT (index >= 0 && index < MAX_SEXP_VARIABLES);

    strcpy (Sexp_variables[index].text, "");
    strcpy (Sexp_variables[index].variable_name, "variable array block");

    if (is_numeric)
        Sexp_variables[index].type = SEXP_VARIABLE_NUMBER | SEXP_VARIABLE_SET;
    else
        Sexp_variables[index].type = SEXP_VARIABLE_STRING | SEXP_VARIABLE_SET;
}

/**
 * Modify a Sexp_variable to be used in a mission
 *
 * This should be called in mission when an sexp_variable is to be modified
 */
void sexp_modify_variable (const char* text, int index, bool /* sexp_callback */) {
    ASSERT (index >= 0 && index < MAX_SEXP_VARIABLES);
    ASSERT (Sexp_variables[index].type & SEXP_VARIABLE_SET);

    if (strchr (text, '$') != NULL) {
        // we want to use the same variable substitution that's in messages
        // etc.
        std::string temp_text = text;
        sexp_replace_variable_names_with_values (temp_text);

        // copy to original buffer
        auto len = temp_text.copy (Sexp_variables[index].text, TOKEN_LENGTH);
        Sexp_variables[index].text[len] = 0;
    }
    else {
        // no variables, so no substitution
        strcpy (Sexp_variables[index].text, text);
    }

    Sexp_variables[index].type |= SEXP_VARIABLE_MODIFIED;
}

void sexp_modify_variable (int n) {
    int sexp_variable_index;
    int new_number;
    char* new_text;
    char number_as_str[TOKEN_LENGTH];

    ASSERT (n >= 0);

    // get sexp_variable index
    ASSERT (Sexp_nodes[n].first == -1);
    sexp_variable_index = atoi (Sexp_nodes[n].text);

    // verify variable set
    ASSERT (Sexp_variables[sexp_variable_index].type & SEXP_VARIABLE_SET);

    if (Sexp_variables[sexp_variable_index].type & SEXP_VARIABLE_NUMBER) {
        // get new numerical value
        new_number = eval_sexp (CDR (n));
        sprintf (number_as_str, "%d", new_number);

        // assign to variable
        sexp_modify_variable (number_as_str, sexp_variable_index);
    }
    else if (Sexp_variables[sexp_variable_index].type & SEXP_VARIABLE_STRING) {
        // get new string
        new_text = CTEXT (CDR (n));

        // assign to variable
        sexp_modify_variable (new_text, sexp_variable_index);
    }
    else { ASSERTX (0, "Invalid variable type.\n"); }
}

bool is_sexp_node_numeric (int node) {
    ASSERT (node >= 0);

    // make the common case fast: if the node has a CAR node, that means it
    // uses an operator; and operators cannot currently return strings
    if (Sexp_nodes[node].first >= 0) return true;

    // if the node text is numeric, the node is too
    if (can_construe_as_integer (Sexp_nodes[node].text)) return true;

    // otherwise it's gotta be text
    return false;
}

// By Goober5000. Very similar to sexp_modify_variable(). Even uses the same
// multi code! :)
void sexp_set_variable_by_index (int node) {
    int sexp_variable_index;
    int new_number;
    char* new_text;
    char number_as_str[TOKEN_LENGTH];

    ASSERT (node >= 0);

    // get sexp_variable index
    sexp_variable_index = eval_num (node);

    // check range
    if (sexp_variable_index < 0 || sexp_variable_index >= MAX_SEXP_VARIABLES) {
        WARNINGF (LOCATION, "set-variable-by-index: sexp variable index %d out of range!  min is 0; max is %d", sexp_variable_index, MAX_SEXP_VARIABLES - 1);
        return;
    }

    // verify variable set
    if (!(Sexp_variables[sexp_variable_index].type & SEXP_VARIABLE_SET)) {
        // well phooey.  go ahead and create it
        sexp_add_array_block_variable (sexp_variable_index, is_sexp_node_numeric (CDR (node)));
    }

    if (Sexp_variables[sexp_variable_index].type & SEXP_VARIABLE_NUMBER) {
        // get new numerical value
        new_number = eval_sexp (CDR (node));
        sprintf (number_as_str, "%d", new_number);

        // assign to variable
        sexp_modify_variable (number_as_str, sexp_variable_index);
    }
    else if (Sexp_variables[sexp_variable_index].type & SEXP_VARIABLE_STRING) {
        // get new string
        new_text = CTEXT (CDR (node));

        // assign to variable
        sexp_modify_variable (new_text, sexp_variable_index);
    }
    else { ASSERTX (0, "Invalid variable type.\n"); }
}

// Goober5000
int sexp_get_variable_by_index (int node) {
    int sexp_variable_index;

    ASSERT (node >= 0);

    // get sexp_variable index
    sexp_variable_index = eval_num (node);

    // check range
    if (sexp_variable_index < 0 || sexp_variable_index >= MAX_SEXP_VARIABLES) {
        WARNINGF (LOCATION, "get-variable-by-index: sexp variable index %d out of range!  min is 0; max is %d", sexp_variable_index, MAX_SEXP_VARIABLES - 1);
        return 0;
    }

    if (Sexp_variables[sexp_variable_index].type & SEXP_VARIABLE_NOT_USED) { WARNINGF (LOCATION, "warning: retrieving a value from a sexp variable which is not in use!"); }
    else if (!(Sexp_variables[sexp_variable_index].type & SEXP_VARIABLE_SET)) { WARNINGF (LOCATION, "warning: retrieving a value from a sexp variable which is not set!"); }

    if (Sexp_variables[sexp_variable_index].type & SEXP_VARIABLE_STRING) {
        WARNINGF (LOCATION, "warning: variable %d is a string but it is not possible to return a string value through a sexp!", sexp_variable_index);
        return SEXP_NAN_FOREVER;
    }

    return atoi (Sexp_variables[sexp_variable_index].text);
}

// Goober5000
// (yes, this reuses a lot of code, but it's a major pain to consolidate it)
void sexp_copy_variable_from_index (int node) {
    int from_index;
    int to_index;

    ASSERT (node >= 0);

    // get sexp_variable index
    from_index = eval_num (node);

    // check range
    if (from_index < 0 || from_index >= MAX_SEXP_VARIABLES) {
        WARNINGF (LOCATION, "copy-variable-from-index: sexp variable index %d out of range!  min is 0; max is %d", from_index, MAX_SEXP_VARIABLES - 1);
        return;
    }

    if (Sexp_variables[from_index].type & SEXP_VARIABLE_NOT_USED) { WARNINGF (LOCATION, "warning: retrieving a value from a sexp variable which is not in use!"); }
    else if (!(Sexp_variables[from_index].type & SEXP_VARIABLE_SET)) { WARNINGF (LOCATION, "warning: retrieving a value from a sexp variable which is not set!"); }

    // now get the variable we are modifying
    to_index = atoi (Sexp_nodes[CDR (node)].text);

    // verify variable set
    ASSERT (Sexp_variables[to_index].type & SEXP_VARIABLE_SET);

    // verify matching types
    if (((Sexp_variables[from_index].type & SEXP_VARIABLE_NUMBER) && !(Sexp_variables[to_index].type & SEXP_VARIABLE_NUMBER)) ||
        ((Sexp_variables[from_index].type & SEXP_VARIABLE_STRING) && !(Sexp_variables[to_index].type & SEXP_VARIABLE_STRING))) {
        WARNINGF (
            LOCATION, "copy-variable-from-index: cannot copy variables of different types!  source = '%s', destination = '%s'", Sexp_variables[from_index].variable_name,
            Sexp_variables[to_index].variable_name);
        return;
    }

    // assign to variable
    sexp_modify_variable (Sexp_variables[from_index].text, to_index);
}

// Goober5000
// (yes, this reuses a lot of code, but it's a major pain to consolidate it)
// (and yes, that reused a comment :p)
void sexp_copy_variable_between_indexes (int node) {
    int from_index;
    int to_index;

    ASSERT (node >= 0);

    // get sexp_variable indexes
    from_index = eval_num (node);
    to_index = eval_num (CDR (node));

    // check ranges
    if (from_index < 0 || from_index >= MAX_SEXP_VARIABLES) {
        WARNINGF (LOCATION, "copy-variable-between-indexes: sexp variable index %d out of range!  min is 0; max is %d", from_index, MAX_SEXP_VARIABLES - 1);
        return;
    }
    if (to_index < 0 || to_index >= MAX_SEXP_VARIABLES) {
        WARNINGF (LOCATION, "copy-variable-between-indexes: sexp variable index %d out of range!  min is 0; max is %d", to_index, MAX_SEXP_VARIABLES - 1);
        return;
    }

    if (Sexp_variables[from_index].type & SEXP_VARIABLE_NOT_USED) { WARNINGF (LOCATION, "warning: retrieving a value from a sexp variable which is not in use!"); }
    else if (!(Sexp_variables[from_index].type & SEXP_VARIABLE_SET)) { WARNINGF (LOCATION, "warning: retrieving a value from a sexp variable which is not set!"); }

    if (!(Sexp_variables[to_index].type & SEXP_VARIABLE_SET)) {
        // well phooey.  go ahead and create it
        sexp_add_array_block_variable (to_index, (Sexp_variables[from_index].type & SEXP_VARIABLE_NUMBER) != 0);
    }

    // verify matching types
    if (((Sexp_variables[from_index].type & SEXP_VARIABLE_NUMBER) && !(Sexp_variables[to_index].type & SEXP_VARIABLE_NUMBER)) ||
        ((Sexp_variables[from_index].type & SEXP_VARIABLE_STRING) && !(Sexp_variables[to_index].type & SEXP_VARIABLE_STRING))) {
        WARNINGF (
            LOCATION, "copy-variable-between-indexes: cannot copy variables of different types!  source = '%s', destination = '%s'", Sexp_variables[from_index].variable_name,
            Sexp_variables[to_index].variable_name);
        return;
    }

    // assign to variable
    sexp_modify_variable (Sexp_variables[from_index].text, to_index);
}

// Different type needed for Fred (1) allow modification of type (2) no
// callback required

/**
 * Given a sexp node return the index of the variable at that node, -1 if not
 * found
 */
int get_index_sexp_variable_from_node (int node) {
    int var_index;

    if (!(Sexp_nodes[node].type & SEXP_FLAG_VARIABLE)) { return -1; }

    var_index = atoi (Sexp_nodes[node].text);

    return var_index;
}

/**
 * Return index of sexp_variable_name, -1 if not found
 */
int get_index_sexp_variable_name (const char* text) {
    for (int i = 0; i < MAX_SEXP_VARIABLES; i++) {
        if (Sexp_variables[i].type & SEXP_VARIABLE_SET) {
            // check case sensitive
            if (!strcmp (Sexp_variables[i].variable_name, text)) { return i; }
        }
    }

    // not found
    return -1;
}

/**
 * Return index of sexp_variable_name, -1 if not found
 */
int get_index_sexp_variable_name (std::string& text) {
    for (int i = 0; i < MAX_SEXP_VARIABLES; i++) {
        if (Sexp_variables[i].type & SEXP_VARIABLE_SET) {
            // check case sensitive
            if (text == Sexp_variables[i].variable_name) { return i; }
        }
    }

    // not found
    return -1;
}

// Goober5000 - tests whether a variable name starts here
// return index of sexp_variable_name, -1 if not found
int get_index_sexp_variable_name_special (const char* startpos) {
    for (int i = MAX_SEXP_VARIABLES - 1; i >= 0; i--) {
        if (Sexp_variables[i].type & SEXP_VARIABLE_SET) {
            // check case sensitive
            // check number of chars in variable name
            if (!strncmp (startpos, Sexp_variables[i].variable_name, strlen (Sexp_variables[i].variable_name))) { return i; }
        }
    }

    // not found
    return -1;
}

// Goober5000 - tests whether a variable name starts here
// return index of sexp_variable_name, -1 if not found
int get_index_sexp_variable_name_special (std::string& text, size_t startpos) {
    for (int i = MAX_SEXP_VARIABLES - 1; i >= 0; i--) {
        if (Sexp_variables[i].type & SEXP_VARIABLE_SET) {
            // check case sensitive
            // check that the variable name starts here, as opposed to farther
            // down the string
            size_t pos = text.find (Sexp_variables[i].variable_name, startpos);
            if (pos != std::string::npos && pos == startpos) { return i; }
        }
    }

    // not found
    return -1;
}

// Goober5000
bool sexp_replace_variable_names_with_values (char* text, int max_len) {
    ASSERT (text != NULL);
    ASSERT (max_len >= 0);

    bool replaced_anything = false;
    char* pos = text;
    do {
        // look for the meta-character
        pos = strchr (pos, '$');

        // found?
        if (pos != NULL) {
            // see if a variable starts at the next char
            int var_index = get_index_sexp_variable_name_special (pos + 1);
            if (var_index >= 0) {
                // get the replacement string ($variable)
                char what_to_replace[TOKEN_LENGTH + 1];
                memset (what_to_replace, 0, TOKEN_LENGTH + 1);
                strncpy (what_to_replace, pos, strlen (Sexp_variables[var_index].variable_name) + 1);

                // replace it
                pos = text + replace_one (text, what_to_replace, Sexp_variables[var_index].text, max_len);
                replaced_anything = true;
            }
            // no match... so keep iterating along the string
            else { pos++; }
        }
    } while (pos != NULL);

    return replaced_anything;
}

// Goober5000
bool sexp_replace_variable_names_with_values (std::string& text) {
    bool replaced_anything = false;

    size_t lookHere = 0;
    size_t foundHere;

    do {
        // look for the meta-character
        foundHere = text.find ('$', lookHere);

        // found?
        if (foundHere != std::string::npos) {
            // see if a variable starts at the next char
            int var_index = get_index_sexp_variable_name_special (text, foundHere + 1);
            if (var_index >= 0) {
                // replace $variable with the value
                text.replace (foundHere, strlen (Sexp_variables[var_index].variable_name) + 1, Sexp_variables[var_index].text);
                replaced_anything = true;

                lookHere = foundHere + strlen (Sexp_variables[var_index].text);
            }
            // no match... so keep iterating along the string
            else { lookHere = foundHere + 1; }
        }
    } while (foundHere != std::string::npos);

    return replaced_anything;
}

/**
 * Count number of sexp_variables that are set
 */
int sexp_variable_count () {
    int count = 0;

    for (int i = 0; i < MAX_SEXP_VARIABLES; i++) {
        if (Sexp_variables[i].type & SEXP_VARIABLE_SET) { count++; }
    }

    return count;
}

/**
 * Count number of persistent sexp_variables that are set
 */
int sexp_campaign_file_variable_count () {
    int count = 0;

    for (int i = 0; i < MAX_SEXP_VARIABLES; i++) {
        if ((Sexp_variables[i].type & SEXP_VARIABLE_SET) && (Sexp_variables[i].type & SEXP_VARIABLE_IS_PERSISTENT) &&
            !(Sexp_variables[i].type & SEXP_VARIABLE_SAVE_TO_PLAYER_FILE)) {
            count++;
        }
    }

    return count;
}

/**
 * Evaluate number which may result from an operator or may be text
 */
int eval_num (int n) {
    ASSERT (n >= 0);

    if (CAR (n) != -1) // if argument is a sexp
        return eval_sexp (CAR (n));
    else
        return atoi (CTEXT (n)); // otherwise, just get the number
}

int get_category (int sexp_id) {
    int category = (sexp_id & OP_CATEGORY_MASK);

    // hack so that CHANGE and CHANGE2 show up in the same menu
    if (category == OP_CATEGORY_CHANGE2) category = OP_CATEGORY_CHANGE;

    return category;
}

// Goober5000 - for FRED2 menu subcategories
int get_subcategory (int sexp_id) {
    switch (sexp_id) {
    case OP_SEND_MESSAGE_LIST:
    case OP_SEND_MESSAGE:
    case OP_SEND_RANDOM_MESSAGE:
        return CHANGE_SUBCATEGORY_MESSAGING;

    case OP_ADD_GOAL:
    case OP_CLEAR_GOALS:
    case OP_GOOD_REARM_TIME:
    case OP_GOOD_SECONDARY_TIME:
    case OP_CAP_WAYPOINT_SPEED:
        return CHANGE_SUBCATEGORY_AI_CONTROL;

    case OP_PROTECT_SHIP:
    case OP_UNPROTECT_SHIP:
    case OP_BEAM_PROTECT_SHIP:
    case OP_BEAM_UNPROTECT_SHIP:
    case OP_SHIP_INVISIBLE:
    case OP_SHIP_VISIBLE:
    case OP_CHANGE_IFF:
    case OP_ADD_REMOVE_ESCORT:
        return CHANGE_SUBCATEGORY_SHIP_STATUS;

    case OP_SET_SPECIAL_WARPOUT_NAME:
    case OP_WARP_BROKEN:
    case OP_WARP_NOT_BROKEN:
    case OP_WARP_NEVER:
    case OP_WARP_ALLOWED:
        return CHANGE_SUBCATEGORY_SHIELDS_ENGINES_AND_WEAPONS;

    case OP_SHIP_INVULNERABLE:
    case OP_SHIP_VULNERABLE:
    case OP_SHIP_GUARDIAN:
    case OP_SHIP_NO_GUARDIAN:
    case OP_SELF_DESTRUCT:
    case OP_SABOTAGE_SUBSYSTEM:
    case OP_REPAIR_SUBSYSTEM:
    case OP_SET_SUBSYSTEM_STRNGTH:
    case OP_SUBSYS_SET_RANDOM:
    case OP_AWACS_SET_RADIUS:
        return CHANGE_SUBCATEGORY_SUBSYSTEMS;

    case OP_TRANSFER_CARGO:
    case OP_EXCHANGE_CARGO:
    case OP_CARGO_NO_DEPLETE:
        return CHANGE_SUBCATEGORY_CARGO;

    case OP_BEAM_FIRE:
    case OP_BEAM_FREE:
    case OP_BEAM_FREE_ALL:
    case OP_BEAM_LOCK:
    case OP_BEAM_LOCK_ALL:
    case OP_TURRET_FREE:
    case OP_TURRET_FREE_ALL:
    case OP_TURRET_LOCK:
    case OP_TURRET_LOCK_ALL:
    case OP_TURRET_TAGGED_ONLY_ALL:
    case OP_TURRET_TAGGED_CLEAR_ALL:
        return CHANGE_SUBCATEGORY_BEAMS_AND_TURRETS;

    case OP_INVALIDATE_GOAL:
    case OP_VALIDATE_GOAL:
    case OP_RED_ALERT:
    case OP_END_CAMPAIGN:
    case OP_GRANT_PROMOTION:
    case OP_GRANT_MEDAL:
    case OP_ALLOW_SHIP:
    case OP_ALLOW_WEAPON:
    case OP_TECH_ADD_SHIP:
    case OP_TECH_ADD_WEAPON:
        return CHANGE_SUBCATEGORY_MISSION_AND_CAMPAIGN;

    case OP_SUPERNOVA_START:
        return CHANGE_SUBCATEGORY_CUTSCENES;

    case OP_SHIP_VANISH:
        return CHANGE_SUBCATEGORY_SPECIAL_EFFECTS;

    case OP_MODIFY_VARIABLE:
        return CHANGE_SUBCATEGORY_VARIABLES;

    case OP_WAS_PROMOTION_GRANTED:
    case OP_WAS_MEDAL_GRANTED:
    case OP_SKILL_LEVEL_AT_LEAST:
    case OP_NUM_KILLS:
    case OP_NUM_TYPE_KILLS:
    case OP_NUM_CLASS_KILLS:
    case OP_LAST_ORDER_TIME:
        return STATUS_SUBCATEGORY_PLAYER;

    case OP_NUM_PLAYERS:
    case OP_TEAM_SCORE:
        return STATUS_SUBCATEGORY_MULTIPLAYER;

    case OP_HAS_BEEN_TAGGED_DELAY:
    case OP_IS_TAGGED:
    case OP_IS_SHIP_VISIBLE:
    case OP_IS_IFF:
        return STATUS_SUBCATEGORY_SHIP_STATUS;

    case OP_SHIELD_RECHARGE_PCT:
    case OP_ENGINE_RECHARGE_PCT:
    case OP_WEAPON_RECHARGE_PCT:
    case OP_SHIELD_QUAD_LOW:
    case OP_SECONDARY_AMMO_PCT:
    case OP_IS_PRIMARY_SELECTED:
    case OP_IS_SECONDARY_SELECTED:
        return STATUS_SUBCATEGORY_SHIELDS_ENGINES_AND_WEAPONS;

    case OP_CARGO_KNOWN_DELAY:
    case OP_CAP_SUBSYS_CARGO_KNOWN_DELAY:
        return STATUS_SUBCATEGORY_CARGO;

    case OP_SHIELDS_LEFT:
    case OP_HITS_LEFT:
    case OP_HITS_LEFT_SUBSYSTEM:
        return STATUS_SUBCATEGORY_DAMAGE;

    case OP_DISTANCE:
    case OP_SPECIAL_WARP_DISTANCE:
        return STATUS_SUBCATEGORY_DISTANCE_AND_COORDINATES;

    default:
        BOOST_ASSERT (0);
        break;
    }
}

// clang-format off
std::vector<sexp_help_struct> Sexp_help = {
        { OP_PLUS, "Plus (Arithmetic operator)\r\n"
                "\tAdds numbers and returns results.\r\n\r\n"
                "Returns a number.  Takes 2 or more numeric arguments." },

        { OP_MINUS, "Minus (Arithmetic operator)\r\n"
                "\tSubtracts numbers and returns results.\r\n\r\n"
                "Returns a number.  Takes 2 or more numeric arguments." },

        { OP_MOD, "Mod (Arithmetic operator)\r\n"
                "\tDivides numbers and returns the remainer.\r\n\r\n"
                "Returns a number.  Takes 2 or more numeric arguments." },

        { OP_MUL, "Multiply (Arithmetic operator)\r\n"
                "\tMultiplies numbers and returns results.\r\n\r\n"
                "Returns a number.  Takes 2 or more numeric arguments." },

        { OP_DIV, "Divide (Arithmetic operator)\r\n"
                "\tDivides numbers and returns results.\r\n\r\n"
                "Returns a number.  Takes 2 or more numeric arguments." },

        { OP_RAND, "Rand (Arithmetic operator)\r\n"
                "\tGets a random number.  This number will not change on successive calls to this sexp.\r\n\r\n"
                "Returns a number.  Takes 2 or 3 numeric arguments...\r\n"
                "\t1:\tLow range of random number.\r\n"
                "\t2:\tHigh range of random number.\r\n"
                "\t3:\t(optional) A seed to use when generating numbers. (Setting this to 0 is the same as having no seed at all)" },

        { OP_TRUE, "True (Boolean operator)\r\n"
                "\tA true boolean state\r\n\r\n"
                "Returns a boolean value." },

        { OP_FALSE, "False (Boolean operator)\r\n"
                "\tA false boolean state\r\n\r\n"
                "Returns a boolean value." },

        { OP_AND, "And (Boolean operator)\r\n"
                "\tAnd is true if all of its arguments are true.\r\n\r\n"
                "Returns a boolean value.  Takes 2 or more boolean arguments." },

        { OP_OR, "Or (Boolean operator)\r\n"
                "\tOr is true if any of its arguments are true.\r\n\r\n"
                "Returns a boolean value.  Takes 2 or more boolean arguments." },

        { OP_EQUALS, "Equals (Boolean operator)\r\n"
                "\tIs true if all of its arguments are equal.\r\n\r\n"
                "Returns a boolean value.  Takes 2 or more numeric arguments." },

        { OP_GREATER_THAN, "Greater Than (Boolean operator)\r\n"
                "\tTrue if the first argument is greater than the subsequent argument(s).\r\n\r\n"
                "Returns a boolean value.  Takes 2 numeric arguments." },

        { OP_LESS_THAN, "Less Than (Boolean operator)\r\n"
                "\tTrue if the first argument is less than the subsequent argument(s).\r\n\r\n"
                "Returns a boolean value.  Takes 2 numeric arguments." },

        { OP_IS_IFF, "Is IFF (Boolean operator)\r\n"
                "\tTrue if ship(s) or wing(s) are all of the specified team.\r\n\r\n"
                "Returns a boolean value.  Takes 2 or more arguments...\r\n"
                "\t1:\tTeam (\"friendly\", \"hostile\", \"neutral\", or \"unknown\").\r\n"
                "\tRest:\tName of ship or wing to check." },

        { OP_HAS_TIME_ELAPSED, "Has time elapsed (Boolean operator)\r\n"
                "\tBecomes true when the specified amount of time has elapsed (Mission time "
                "becomes greater than the specified time).\r\n"
                "Returns a boolean value.  Takes 1 numeric argument...\r\n"
                "\t1:\tThe amount of time in seconds." },

        { OP_NOT, "Not (Boolean operator)\r\n"
                "\tReturns opposite boolean value of argument (True becomes false, and "
                "false becomes true).\r\n\r\n"
                "Returns a boolean value.  Takes 1 boolean argument." },

        { OP_PREVIOUS_GOAL_TRUE, "Previous Mission Goal True (Boolean operator)\r\n"
                "\tReturns true if the specified goal in the specified mission is true "
                "(or succeeded).  It returns false otherwise.\r\n\r\n"
                "Returns a boolean value.  Takes 2 required arguments and 1 optional argument...\r\n"
                "\t1:\tName of the mission.\r\n"
                "\t2:\tName of the goal in the mission.\r\n"
                "\t3:\t(Optional) True/False which signifies what this sexpression should return when "
                "this mission is played as a single mission." },

        { OP_PREVIOUS_GOAL_FALSE, "Previous Mission Goal False (Boolean operator)\r\n"
                "\tReturns true if the specified goal in the specified mission "
                "is false (or failed).  It returns false otherwise.\r\n\r\n"
                "Returns a boolean value.  Takes 2 required arguments and 1 optional argument...\r\n"
                "\t1:\tName of the mission.\r\n"
                "\t2:\tName of the goal in the mission.\r\n"
                "\t3:\t(Optional) True/False which signifies what this sexpression should return when "
                "this mission is played as a single mission." },

        { OP_PREVIOUS_GOAL_INCOMPLETE, "Previous Mission Goal Incomplete (Boolean operator)\r\n"
                "\tReturns true if the specified goal in the specified mission "
                "is incomplete (not true or false).  It returns false otherwise.\r\n\r\n"
                "Returns a boolean value.  Takes 2 required arguments and 1 optional argument...\r\n"
                "\t1:\tName of the mission.\r\n"
                "\t2:\tName of the goal in the mission.\r\n"
                "\t3:\t(Optional) True/False which signifies what this sexpression should return when "
                "this mission is played as a single mission." },

        { OP_PREVIOUS_EVENT_TRUE, "Previous Mission Event True (Boolean operator)\r\n"
                "\tReturns true if the specified event in the specified mission is true "
                "(or succeeded).  It returns false otherwise.\r\n\r\n"
                "Returns a boolean value.  Takes 2 required arguments and 1 optional argument...\r\n"
                "\t1:\tName of the mission.\r\n"
                "\t2:\tName of the event in the mission.\r\n"
                "\t3:\t(Optional) True/False which signifies what this sexpression should return when "
                "this mission is played as a single mission." },

        { OP_PREVIOUS_EVENT_FALSE, "Previous Mission Event False (Boolean operator)\r\n"
                "\tReturns true if the specified event in the specified mission "
                "is false (or failed).  It returns false otherwise.\r\n\r\n"
                "Returns a boolean value.  Takes 2 required arguments and 1 optional argument...\r\n"
                "\t1:\tName of the mission.\r\n"
                "\t2:\tName of the event in the mission.\r\n"
                "\t3:\t(Optional) True/False which signifies what this sexpression should return when "
                "this mission is played as a single mission." },

        { OP_PREVIOUS_EVENT_INCOMPLETE, "Previous Mission Event Incomplete (Boolean operator)\r\n"
                "\tReturns true if the specified event in the specified mission "
                "is incomplete (not true or false).  It returns false otherwise.\r\n\r\n"
                "Returns a boolean value.  Takes 2 required arguments and 1 optional argument...\r\n"
                "\t1:\tName of the mission.\r\n"
                "\t2:\tName of the event in the mission.\r\n"
                "\t3:\t(Optional) True/False which signifies what this sexpression should return when "
                "this mission is played as a single mission." },

        { OP_GOAL_TRUE_DELAY, "Mission Goal True (Boolean operator)\r\n"
                "\tReturns true N seconds after the specified goal in the this mission is true "
                "(or succeeded).  It returns false otherwise.\r\n\r\n"
                "Returns a boolean value.  Takes 2 required arguments and 1 optional argument...\r\n"
                "\t1:\tName of the event in the mission.\r\n"
                "\t2:\tNumber of seconds to delay before returning true."},

        { OP_GOAL_FALSE_DELAY, "Mission Goal False (Boolean operator)\r\n"
                "\tReturns true N seconds after the specified goal in the this mission is false "
                "(or failed).  It returns false otherwise.\r\n\r\n"
                "Returns a boolean value.  Takes 2 required arguments and 1 optional argument...\r\n"
                "\t1:\tName of the event in the mission.\r\n"
                "\t2:\tNumber of seconds to delay before returning true."},

        { OP_GOAL_INCOMPLETE, "Mission Goal Incomplete (Boolean operator)\r\n"
                "\tReturns true if the specified goal in the this mission is incomplete.  This "
                "sexpression will only be useful in conjunction with another sexpression like"
                "has-time-elapsed.  Used alone, it will return true upon misison startup."
                "Returns a boolean value.  Takes 1 argument...\r\n"
                "\t1:\tName of the event in the mission."},

        { OP_EVENT_TRUE_DELAY, "Mission Event True (Boolean operator)\r\n"
                "\tReturns true N seconds after the specified event in the this mission is true "
                "(or succeeded).  It returns false otherwise.\r\n\r\n"
                "Returns a boolean value.  Takes 2 required arguments and 1 optional argument...\r\n"
                "\t1:\tName of the event in the mission.\r\n"
                "\t2:\tNumber of seconds to delay before returning true.\r\n"
                "\t3:\t(Optional) Defaults to False. When set to false, directives will only appear as soon as the specified event is true.\r\n"
                "\t\tWhen set to true, the event only affects whether the directive succeeds/fails, and has no effect on when it appears"},

        { OP_EVENT_FALSE_DELAY, "Mission Event False (Boolean operator)\r\n"
                "\tReturns true N seconds after the specified event in the this mission is false "
                "(or failed).  It returns false otherwise.\r\n\r\n"
                "Returns a boolean value.  Takes 2 required arguments and 1 optional argument...\r\n"
                "\t1:\tName of the event in the mission.\r\n"
                "\t2:\tNumber of seconds to delay before returning true.\r\n"
                "\t3:\t(Optional) Defaults to False. When set to false, directives will only appear as soon as the specified event is true.\r\n"
                "\t\tWhen set to true, the event only affects whether the directive succeeds/fails, and has no effect on when it appears"},

        { OP_EVENT_INCOMPLETE, "Mission Event Incomplete (Boolean operator)\r\n"
                "\tReturns true if the specified event in the this mission is incomplete.  This "
                "sexpression will only be useful in conjunction with another sexpression like"
                "has-time-elapsed.  Used alone, it will return true upon misison startup."
                "Returns a boolean value.  Takes 1 argument...\r\n"
                "\t1:\tName of the event in the mission."},

        { OP_IS_DESTROYED_DELAY, "Is destroyed delay (Boolean operator)\r\n"
                "\tBecomes true <delay> seconds after all specified ships have been destroyed.\r\n"
                "\tWARNING: If multiple is-destroyed-delay SEXPs are used in a directive event, unexpected results may be "
                "observed. Instead, use a single is-destroyed-delay SEXP with multiple parameters.\r\n"
                "Returns a boolean value.  Takes 2 or more arguments...\r\n"
                "\t1:\tTime delay in seconds (see above).\r\n"
                "\tRest:\tName of ship (or wing) to check status of." },

        { OP_IS_SUBSYSTEM_DESTROYED_DELAY, "Is subsystem destroyed delay (Boolean operator)\r\n"
                "\tBecomes true <delay> seconds after the specified subsystem of the specified "
                "ship is destroyed.\r\n\r\n"
                "Returns a boolean value.  Takes 3 arguments...\r\n"
                "\t1:\tName of ship the subsystem we are checking is on.\r\n"
                "\t2:\tThe name of the subsystem we are checking status of.\r\n"
                "\t3:\tTime delay in seconds (see above)." },

        { OP_IS_DISABLED_DELAY, "Is disabled delay (Boolean operator)\r\n"
                "\tBecomes true <delay> seconds after the specified ship(s) are disabled.  A "
                "ship is disabled when all of its engine subsystems are destroyed.  All "
                "ships must be diabled for this function to return true.\r\n\r\n"
                "Returns a boolean value.  Takes 2 or more arguments...\r\n"
                "\t1:\tTime delay is seconds (see above).\r\n"
                "\tRest:\tNames of ships to check disabled status of." },

        { OP_IS_DISARMED_DELAY, "Is disarmed delay (Boolean operator)\r\n"
                "\tBecomes true <delay> seconds after the specified ship(s) are disarmed.  A "
                "ship is disarmed when all of its turret subsystems are destroyed.  All "
                "ships must be disarmed for this function to return true.\r\n\r\n"
                "Returns a boolean value.  Takes 2 or more arguments...\r\n"
                "\t1:\tTime delay is seconds (see above).\r\n"
                "\tRest:\tNames of ships to check disarmed status of." },

        { OP_HAS_DOCKED_DELAY, "Has docked delay (Boolean operator)\r\n"
                "\tBecomes true <delay> seconds after the specified ships have docked the "
                "specified number of times.\r\n\r\n"
                "Returns a boolean value.  Takes 4 arguments...\r\n"
                "\t1:\tThe name of the docker ship\r\n"
                "\t2:\tThe name of the dockee ship\r\n"
                "\t3:\tThe number of times they have to have docked\r\n"
                "\t4:\tTime delay in seconds (see above)." },

        { OP_HAS_UNDOCKED_DELAY, "Has undocked delay (Boolean operator)\r\n"
                "\tBecomes true <delay> seconds after the specified ships have undocked the "
                "specified number of times.\r\n\r\n"
                "Returns a boolean value.  Takes 4 arguments...\r\n"
                "\t1:\tThe name of the docker ship\r\n"
                "\t2:\tThe name of the dockee ship\r\n"
                "\t3:\tThe number of times they have to have undocked\r\n"
                "\t4:\tTime delay in seconds (see above)." },

        { OP_HAS_ARRIVED_DELAY, "Has arrived delay (Boolean operator)\r\n"
                "\tBecomes true <delay> seconds after the specified ship(s) have arrived into the mission\r\n\r\n"
                "Returns a boolean value.  Takes 2 or more arguments...\r\n"
                "\t1:\tTime delay in seconds (see above).\r\n"
                "\tRest:\tName of ship (or wing) we want to check has arrived." },

        { OP_HAS_DEPARTED_DELAY, "Has departed delay (Boolean operator)\r\n"
                "\tBecomes true <delay> seconds after the specified ship(s) or wing(s) have departed "
                "from the mission by warping out.  If any ship was destroyed, this operator will "
                "never be true.\r\n\r\n"
                "Returns a boolean value.  Takes 2 or more arguments...\r\n"
                "\t1:\tTime delay in seconds (see above).\r\n"
                "\tRest:\tName of ship (or wing) we want to check has departed." },

        { OP_WAYPOINTS_DONE_DELAY, "Waypoints done delay (Boolean operator)\r\n"
                "\tBecomes true <delay> seconds after the specified ship has completed flying the "
                "specified waypoint path.\r\n\r\n"
                "Returns a boolean value.  Takes 3 or 4 arguments...\r\n"
                "\t1:\tName of ship we are checking.\r\n"
                "\t2:\tWaypoint path we want to check if ship has flown.\r\n"
                "\t3:\tTime delay in seconds (see above).\r\n"
                "\t4:\tHow many times the ship has completed the waypoint path (optional)." },

        { OP_SHIP_TYPE_DESTROYED, "Ship Type Destroyed (Boolean operator)\r\n"
                "\tBecomes true when the specified percentage of ship types in this mission "
                "have been destroyed.  The ship type is a generic type such as fighter/bomber, "
                "transport, etc.  Fighters and bombers count as the same type.\r\n\r\n"
                "Returns a boolean value.  Takes 2 arguments...\r\n"
                "\t1:\tPercentage of ships that must be destroyed.\r\n"
                "\t2:\tShip type to check for." },

        { OP_TIME_SHIP_DESTROYED, "Time ship destroyed (Time operator)\r\n"
                "\tReturns the time the specified ship was destroy.\r\n\r\n"
                "Returns a numeric value.  Takes 1 argument...\r\n"
                "\t1:\tName of ship we want to check." },

        { OP_TIME_SHIP_ARRIVED, "Time ship arrived (Time operator)\r\n"
                "\tReturns the time the specified ship arrived into the mission.\r\n\r\n"
                "Returns a numeric value.  Takes 1 argument...\r\n"
                "\t1:\tName of ship we want to check." },

        { OP_TIME_SHIP_DEPARTED, "Time ship departed (Time operator)\r\n"
                "\tReturns the time the specified ship departed the mission by warping out.  Being "
                "destroyed doesn't count departed.\r\n\r\n"
                "Returns a numeric value.  Takes 1 argument...\r\n"
                "\t1:\tName of ship we want to check." },

        { OP_TIME_WING_DESTROYED, "Time wing destroyed (Time operator)\r\n"
                "\tReturns the time the specified wing was destroy.\r\n\r\n"
                "Returns a numeric value.  Takes 1 argument...\r\n"
                "\t1:\tName of wing we want to check." },

        { OP_TIME_WING_ARRIVED, "Time wing arrived (Time operator)\r\n"
                "\tReturns the time the specified wing arrived into the mission.\r\n\r\n"
                "Returns a numeric value.  Takes 1 argument...\r\n"
                "\t1:\tName of wing we want to check." },

        { OP_TIME_WING_DEPARTED, "Time wing departed (Time operator)\r\n"
                "\tReturns the time the specified wing departed the mission by warping out.  All "
                "ships in the wing have to have warped out.  If any are destroyed, the wing can "
                "never be considered departed.\r\n\r\n"
                "Returns a numeric value.  Takes 1 argument...\r\n"
                "\t1:\tName of ship we want to check." },

        { OP_MISSION_TIME, "Mission time (Time operator)\r\n"
                "\tReturns the current time into the mission.\r\n\r\n"
                "Returns a numeric value.  Takes no arguments." },

        { OP_TIME_DOCKED, "Time docked (Time operator)\r\n"
                "\tReturns the time the specified ships docked.\r\n\r\n"
                "Returns a numeric value.  Takes 3 arguments...\r\n"
                "\t1:\tThe name of the docker ship.\r\n"
                "\t2:\tThe name of the dockee ship.\r\n"
                "\t3:\tThe number of times they must have docked to be true." },

        { OP_TIME_UNDOCKED, "Time undocked (Time operator)\r\n"
                "\tReturns the time the specified ships undocked.\r\n\r\n"
                "Returns a numeric value.  Takes 3 arguments...\r\n"
                "\t1:\tThe name of the docker ship.\r\n"
                "\t2:\tThe name of the dockee ship.\r\n"
                "\t3:\tThe number of times they must have undocked to be true." },

        { OP_SHIELDS_LEFT, "Shields left (Status operator)\r\n"
                "\tReturns the current level of the specified ship's shields as a percentage.\r\n\r\n"
                "Returns a numeric value.  Takes 1 argument...\r\n"
                "\t1:\tName of ship to check." },

        { OP_HITS_LEFT, "Hits left (Status operator)\r\n"
                "\tReturns the current level of the specified ship's hull as a percentage.\r\n\r\n"
                "Returns a numeric value.  Takes 1 argument...\r\n"
                "\t1:\tName of ship to check." },

        { OP_HITS_LEFT_SUBSYSTEM, "Hits left subsystem (status operator, deprecated)\r\n"
                "\tReturns the current level of the specified ship's subsystem integrity as a percentage of the damage done to *all "
                "subsystems of the same type*.  This operator provides the same functionality as the new hits-left-subsystem-generic "
                "operator, except that it gets the subsystem type in a very misleading way.  Common consensus among SCP programmers is "
                "that this operator was intended to behave like hits-left-subsystem-specific but was programmed incorrectly.  As such, "
                "this operator is deprecated.  Mission designers are strongly encouraged to use hits-left-subsystem-specific rather than "
                "the optional boolean parameter.\r\n\r\n"
                "Returns a numeric value.  Takes 2 or 3 arguments...\r\n"
                "\t1:\tName of ship to check.\r\n"
                "\t2:\tName of subsystem on ship to check.\r\n"
                "\t3:\t(Optional) True/False. When set to true only the subsystem supplied will be tested; when set to false (the default), "
                "all subsystems of that type will be tested." },

        { OP_DISTANCE, "Distance (Status operator)\r\n"
                "\tReturns the distance between two objects.  These objects can be either a ship, "
                "a wing, or a waypoint.\r\n"
                "When a wing or team is given (for either argument) the answer will be the shortest distance. \r\n\r\n"
                "Returns a numeric value.  Takes 2 arguments...\r\n"
                "\t1:\tThe name of one of the objects.\r\n"
                "\t2:\tThe name of the other object." },

        { OP_LAST_ORDER_TIME, "Last order time (Status operator)\r\n"
                "\tReturns true if <count> seconds have elapsed since one or more ships have received "
                "a meaningful order from the player.  A meaningful order is currently any order that "
                "is not the warp out order.\r\n\r\n"
                "Returns a boolean value.  Takes 2 or more arguments...\r\n"
                "\t1:\tTime in seconds that must elapse.\r\n"
                "\tRest:\tName of ship or wing to check for having received orders." },

        { OP_WHEN, "When (Conditional operator)\r\n"
                "\tPerforms specified actions when a condition becomes true\r\n\r\n"
                "Takes 2 or more arguments...\r\n"
                "\t1:\tBoolean expression that must be true for actions to take place.\r\n"
                "\tRest:\tActions to take when boolean expression becomes true." },

        { OP_COND, "Blah" },

        { OP_CHANGE_IFF, "Change IFF (Action operator)\r\n"
                "\tSets the specified ship(s) or wing(s) to the specified team.\r\n"
                "Takes 2 or more arguments...\r\n"
                "\t1:\tTeam to change to (\"friendly\", \"hostile\" or \"unknown\").\r\n"
                "\tRest:\tName of ship or wing to change team status of." },

        { OP_MODIFY_VARIABLE, "Modify-variable (Misc. operator)\r\n"
                "\tModifies variable to specified value\r\n\r\n"
                "Takes 2 arguments...\r\n"
                "\t1:\tName of Variable.\r\n"
                "\t2:\tValue to be set." },

        { OP_PROTECT_SHIP, "Protect ship (Action operator)\r\n"
                "\tProtects a ship from being attacked by any enemy ship.  Any ship "
                "that is protected will not come under enemy fire.\r\n\r\n"
                "Takes 1 or more arguments...\r\n"
                "\tAll:\tName of ship(s) to protect." },

        { OP_UNPROTECT_SHIP, "Unprotect ship (Action operator)\r\n"
                "\tUnprotects a ship from being attacked by any enemy ship.  Any ship "
                "that is not protected can come under enemy fire.  This function is the opposite "
                "of protect-ship.\r\n\r\n"
                "Takes 1 or more arguments...\r\n"
                "\tAll:\tName of ship(s) to unprotect." },

        { OP_BEAM_PROTECT_SHIP, "Beam Protect ship (Action operator)\r\n"
                "\tProtects a ship from being attacked with beam weapon.  Any ship "
                "that is beam protected will not come under enemy beam fire.\r\n\r\n"
                "Takes 1 or more arguments...\r\n"
                "\tAll:\tName of ship(s) to protect." },

        { OP_BEAM_UNPROTECT_SHIP, "Beam Unprotect ship (Action operator)\r\n"
                "\tUnprotects a ship from being attacked with beam weapon.  Any ship "
                "that is not beam protected can come under enemy beam fire.  This function is the opposite "
                "of beam-protect-ship.\r\n\r\n"
                "Takes 1 or more arguments...\r\n"
                "\tAll:\tName of ship(s) to unprotect." },

        { OP_SEND_MESSAGE, "Send message (Action operator)\r\n"
                "\tSends a message to the player.  Can be send by a ship, wing, or special "
                "source.  To send it from a special source, make the first character of the first "
                "argument a \"#\".\r\n\r\n"
                "Takes 3 arguments...\r\n"
                "\t1:\tName of who the message is from.\r\n"
                "\t2:\tPriority of message (\"Low\", \"Normal\" or \"High\").\r\n"
                "\t3:\tName of message (from message editor)." },

        { OP_SELF_DESTRUCT, "Self destruct (Action operator)\r\n"
                "\tCauses the specified ship(s) to self destruct.\r\n\r\n"
                "Takes 1 or more arguments...\r\n"
                "\tAll:\tName of ship to self destruct." },

        { OP_NEXT_MISSION, "Next Mission (Action operator)\r\n"
                "\tThe next mission operator is used for campaign branching in the campaign editor.  "
                "It specifies which mission should played be next in the campaign.  This operator "
                "generally follows a 'when' or 'cond' statment in the campaign file.\r\n\r\n"
                "Takes 1 argument...\r\n"
                "\t1:\tName of mission (filename) to proceed to." },

        { OP_CLEAR_GOALS, "Clear goals (Action operator)\r\n"
                "\tClears the goals for the specified ships and/or wings.\r\n\r\n"
                "Takes 1 or more arguments...\r\n"
                "\tAll:\tName of ship or wing." },

        { OP_ADD_GOAL, "Add goal (Action operator)\r\n"
                "\tAdds a goal to a ship or wing.\r\n\r\n"
                "Takes 2 arguments...\r\n"
                "\t1:\tName of ship or wing to add goal to.\r\n"
                "\t2:\tGoal to add." },

        { OP_SABOTAGE_SUBSYSTEM, "Sabotage subystem (Action operator)\r\n"
                "\tReduces the specified subsystem integrity by the specified percentage."
                "If the percntage strength of the subsystem (after completion) is less than 0%,"
                "subsystem strength is set to 0%.\r\n\r\n"
                "Takes 3 arguments...\r\n"
                "\t1:\tName of ship subsystem is on.\r\n"
                "\t2:\tName of subsystem to sabotage.\r\n"
                "\t3:\tPercentage to reduce subsystem integrity by." },

        { OP_REPAIR_SUBSYSTEM, "Repair Subystem (Action operator)\r\n"
                "\tIncreases the specified subsystem integrity by the specified percentage."
                "If the percntage strength of the subsystem (after completion) is greater than 100%,"
                "subsystem strength is set to 100%.\r\n\r\n"
                "Takes 4 arguments...\r\n"
                "\t1:\tName of ship subsystem is on.\r\n"
                "\t2:\tName of subsystem to repair.\r\n"
                "\t3:\tPercentage to increase subsystem integrity by.\r\n"
                "\t4:\tRepair turret submodel.  Optional argument that defaults to true."},

        { OP_SET_SUBSYSTEM_STRNGTH, "Set Subsystem Strength (Action operator)\r\n"
                "\tSets the specified subsystem to the the specified percentage."
                "If the percentage specified is < 0, strength is set to 0.  If the percentage is "
                "> 100 % the subsystem strength is set to 100%.\r\n\r\n"
                "Takes 3 arguments...\r\n"
                "\t1:\tName of ship subsystem is on.\r\n"
                "\t2:\tName of subsystem to set strength.\r\n"
                "\t3:\tPercentage to set subsystem integrity to.\r\n"
                "\t4:\tRepair turret submodel.  Optional argument that defaults to true."},

        { OP_INVALIDATE_GOAL, "Invalidate goal (Action operator)\r\n"
                "\tMakes a mission goal invalid, which causes it to not show up on mission goals "
                "screen, or be evaluated.\r\n"
                "Takes 1 or more arguments...\r\n"
                "\tAll:\tName of mission goal to invalidate." },

        { OP_VALIDATE_GOAL, "Validate goal (Action operator)\r\n"
                "\tMakes a mission goal valid again, so it shows up on mission goals screen.\r\n"
                "Takes 1 or more arguments...\r\n"
                "\tAll:\tName of mission goal to validate." },

        { OP_SEND_RANDOM_MESSAGE, "Send random message (Action operator)\r\n"
                "\tSends a random message to the player from those supplied.  Can be send by a "
                "ship, wing, or special source.  To send it from a special source, make the first "
                "character of the first argument a \"#\".\r\n\r\n"
                "Takes 3 or more arguments...\r\n"
                "\t1:\tName of who the message is from.\r\n"
                "\t2:\tPriority of message (\"Low\", \"Normal\" or \"High\")."
                "\tRest:\tName of message (from message editor)." },

        { OP_TRANSFER_CARGO, "Transfer Cargo (Action operator)\r\n"
                "\tTransfers the cargo from one ship to another ship.\r\n\r\n"
                "Takes 2 arguments...\r\n"
                "\t1:\tName of ship that cargo is being transferred from.\r\n"
                "\t2:\tName of ship that cargo is being transferred to." },

        { OP_EXCHANGE_CARGO, "Exchange Cargo (Action operator)\r\n"
                "\tExchanges the cargos of two ships.  If one of the two ships contains no cargo, "
                "the cargo is transferred instead.\r\n"
                "Takes 2 arguments...\r\n"
                "\t1:\tName of one of the ships.\r\n"
                "\t2:\tName of the other ship." },

        { OP_AI_CHASE, "Ai-chase (Ship goal)\r\n"
                "\tCauses the specified ship to chase and attack the specified target.\r\n\r\n"
                "Takes 2 or 3 arguments...\r\n"
                "\t1:\tName of ship to chase.\r\n"
                "\t2:\tGoal priority (number between 0 and 200. Player orders have a priority of 90-100).\r\n"
                "\t3 (optional):\tWhether to attack the target even if it is on the same team; defaults to false."
        },

        { OP_AI_CHASE_WING, "Ai-chase wing (Ship goal)\r\n"
                "\tCauses the specified ship to chase and attack the specified target.\r\n\r\n"
                "Takes 2 or 3 arguments...\r\n"
                "\t1:\tName of wing to chase.\r\n"
                "\t2:\tGoal priority (number between 0 and 200. Player orders have a priority of 90-100).\r\n"
                "\t3 (optional):\tWhether to attack the target even if it is on the same team; defaults to false."
        },

        { OP_AI_CHASE_ANY, "Ai-chase-any (Ship goal)\r\n"
                "\tCauses the specified ship to chase and attack any ship on the opposite team.\r\n\r\n"
                "Takes 1 argument...\r\n"
                "\t1:\tGoal priority (number between 0 and 200. Player orders have a priority of 90-100)."
        },

        { OP_AI_DOCK, "Ai-dock (Ship goal)\r\n"
                "\tCauses one ship to dock with another ship.\r\n\r\n"
                "Takes 4 arguments...\r\n"
                "\t1:\tName of dockee ship (The ship that \"docker\" will dock with).\r\n"
                "\t2:\tDocker's docking point - Which dock point docker uses to dock.\r\n"
                "\t3:\tDockee's docking point - Which dock point on dockee docker will move to.\r\n"
                "\t4:\tGoal priority (number between 0 and 200. Player orders have a priority of 90-100)." },

        { OP_AI_UNDOCK, "Ai-undock (Ship goal)\r\n"
                "\tCauses the specified ship to undock from who it is currently docked with.\r\n\r\n"
                "Takes 1 or 2 arguments...\r\n"
                "\t1:\tGoal priority (number between 0 and 89).\r\n"
                "\t2 (optional):\tShip to undock from.  If none is specified, the code will pick the first docked ship." },

        { OP_AI_WARP_OUT, "Ai-warp-out (Ship/Wing Goal)\r\n"
                "\tCauses the specified ship/wing to immediately warp out of the mission, from its current location.  "
                "It will warp even if its departure cue is specified as a hangar bay.\r\n\r\n"
                "Takes 2 arguments...\r\n"
                "\t1:\tName of waypoint path to follow to warp out (not used).\r\n"
                "\t2:\tGoal priority (number between 0 and 200. Player orders have a priority of 90-100)." },

        { OP_AI_WAYPOINTS, "Ai-waypoints (Ship goal)\r\n"
                "\tCauses the specified ship to fly a waypoint path continuously.\r\n\r\n"
                "Takes 2 arguments...\r\n"
                "\t1:\tName of waypoint path to fly.\r\n"
                "\t2:\tGoal priority (number between 0 and 200. Player orders have a priority of 90-100)." },

        { OP_AI_WAYPOINTS_ONCE, "Ai-waypoints once (Ship goal)\r\n"
                "\tCauses the specified ship to fly a waypoint path.\r\n\r\n"
                "Takes 2 arguments...\r\n"
                "\t1:\tName of waypoint path to fly.\r\n"
                "\t2:\tGoal priority (number between 0 and 200. Player orders have a priority of 90-100)." },

        { OP_AI_DESTROY_SUBSYS, "Ai-destroy subsys (Ship goal)\r\n"
                "\tCauses the specified ship to attack and try and destroy the specified subsystem "
                "on the specified ship.\r\n\r\n"
                "Takes 3 or 4 arguments...\r\n"
                "\t1:\tName of ship subsystem is on.\r\n"
                "\t2:\tName of subsystem on the ship to attack and destroy.\r\n"
                "\t3:\tGoal priority (number between 0 and 200. Player orders have a priority of 90-100).\r\n"
                "\t4 (optional):\tWhether to attack the target even if it is on the same team; defaults to false."
        },

        { OP_AI_DISABLE_SHIP, "Ai-disable-ship (Ship/wing goal)\r\n"
                "\tThis AI goal causes a ship/wing to destroy all of the engine subsystems on "
                "the specified ship.  This goal is different than ai-destroy-subsystem since a ship "
                "may have multiple engine subsystems requiring the use of > 1 ai-destroy-subsystem "
                "goals.\r\n"
                "Please note that this goal may call \"protect-ship\" on the target "
                "to prevent overzealous AI ships from destroying it in the process of disabling it.  "
                "If the ship must be destroyed later on, be sure to call an \"unprotect-ship\" sexp.\r\n\r\n"
                "Takes 2 or 3 arguments...\r\n"
                "\t1:\tName of ship whose engine subsystems should be destroyed\r\n"
                "\t2:\tGoal priority (number between 0 and 200. Player orders have a priority of 90-100).\r\n"
                "\t3 (optional):\tWhether to attack the target even if it is on the same team; defaults to false."
        },

        { OP_AI_DISARM_SHIP, "Ai-disarm-ship (Ship/wing goal)\r\n"
                "\tThis AI goal causes a ship/wing to destroy all of the turret subsystems on "
                "the specified ship.  This goal is different than ai-destroy-subsystem since a ship "
                "may have multiple turret subsystems requiring the use of > 1 ai-destroy-subsystem "
                "goals.\r\n"
                "Please note that this goal may call \"protect-ship\" on the target "
                "to prevent overzealous AI ships from destroying it in the process of disarming it.  "
                "If the ship must be destroyed later on, be sure to call an \"unprotect-ship\" sexp.\r\n\r\n"
                "Takes 2 arguments...\r\n"
                "\t1:\tName of ship whose turret subsystems should be destroyed\r\n"
                "\t2:\tGoal priority (number between 0 and 200. Player orders have a priority of 90-100).\r\n"
                "\t3 (optional):\tWhether to attack the target even if it is on the same team; defaults to false."
        },

        { OP_AI_GUARD, "Ai-guard (Ship goal)\r\n"
                "\tCauses the specified ship to guard a ship from other ships not on the same team.\r\n\r\n"
                "Takes 2 arguments...\r\n"
                "\t1:\tName of ship to guard.\r\n"
                "\t2:\tGoal priority (number between 0 and 200. Player orders have a priority of 90-100)." },

        { OP_AI_GUARD_WING, "Ai-guard wing (Ship goal)\r\n"
                "\tCauses the specified ship to guard a wing of ships from other ships not on the "
                "same team.\r\n\r\n"
                "Takes 2 arguments...\r\n"
                "\t1:\tName of wing to guard.\r\n"
                "\t2:\tGoal priority (number between 0 and 200. Player orders have a priority of 90-100)." },

        { OP_NOP, "Do-nothing (Action operator)\r\n"
                "\tDoes nothing.  This is used as the default for any required action arguments "
                "of an operator." },

        { OP_KEY_PRESSED, "Key-pressed (Boolean training operator)\r\n"
                "\tBecomes true when the specified default key has been pressed.  Default key "
                "refers to the what the key normally is when not remapped.  FreeSpace will "
                "automatically account for any keys that have been remapped.  If the optional "
                "delay is specified, becomes true that many seconds after the key has been pressed.\r\n\r\n"
                "Returns a boolean value.  Takes 1 or 2 arguments...\r\n"
                "\t1:\tDefault key to check for.\r\n"
                "\t2:\tDelay before operator registers as true (optional).\r\n" },

        { OP_KEY_RESET, "Key-reset (Training operator)\r\n"
                "\tMarks the specified default key as having not been pressed, so key-pressed will be false "
                "until the player presses it again.  See key-pressed help for more information about "
                "what a default key is.\r\n\r\n"
                "\tNote that this sexp will not work properly in repeating events.  Use key-reset-multiple "
                "if this is to be called multiple times in one event.\r\n\r\n"
                "Returns a boolean value.  Takes 1 or more arguments...\r\n"
                "\tAll:\tDefault key to reset." },

        { OP_TARGETED, "Targeted (Boolean training operator)\r\n"
                "\tIs true as long as the player has the specified ship (or ship's subsystem) targeted, "
                "or has been targeted for the specified amount of time.\r\n\r\n"
                "Returns a boolean value.  Takes 1 to 3 arguments (first required, rest optional):\r\n"
                "\t1:\tName of ship to check if targeted by player.\r\n"
                "\t2:\tLength of time target should have been kept for (optional).\r\n"
                "\t3:\tName of subsystem on ship to check if targeted (optional)." },

        { OP_SPEED, "Speed (Boolean training operator)\r\n"
                "\tBecomes true when the player has been within the specified speed range set by "
                "set-training-context-speed for the specified amount of time.\r\n\r\n"
                "Returns a boolean value.  Takes 1 argument...\r\n"
                "\t1:\tTime in seconds." },

        { OP_FACING, "Facing (Boolean training operator)\r\n"
                "\tIs true as long as the specified ship is within the player's specified "
                "forward cone.  A forward cone is defined as any point that the angle between the "
                "vector of the point and the player, and the forward facing vector is within the "
                "given angle.\r\n\r\n"
                "Returns a boolean value.  Takes 2 argument...\r\n"
                "\t1:\tShip to check is withing forward cone.\r\n"
                "\t2:\tAngle in degrees of the forward cone." },

        { OP_FACING2, "Facing Waypoint(Boolean training operator)\r\n"
                "\tIs true as long as the specified first waypoint is within the player's specified "
                "forward cone.  A forward cone is defined as any point that the angle between the "
                "vector of the point and the player, and the forward facing vector is within the "
                "given angle.\r\n\r\n"
                "Returns a boolean value.  Takes 2 argument...\r\n"
                "\t1:\tName of waypoint path whose first point is within forward cone.\r\n"
                "\t2:\tAngle in degrees of the forward cone." },

        { OP_ORDER, "Order (Boolean training operator)\r\n"
                "\tDeprecated - Use Query-Orders in any new mission.\r\n\r\n"
                "\tBecomes true when the player had given the specified ship or wing the specified order.\r\n\r\n"
                "Returns a boolean value.  Takes 2 or 3 arguments...\r\n"
                "\t1:\tName of ship or wing to check if given order to.\r\n"
                "\t2:\tName of order to check if player has given.\r\n"
                "\t3:\tName of the target of the order (optional)." },

        { OP_WAYPOINT_MISSED, "Waypoint-missed (Boolean training operator)\r\n"
                "\tBecomes true when a waypoint is flown, but the waypoint is ahead of the one "
                "they are supposed to be flying.  The one they are supposed to be flying is the "
                "next one in sequence in the path after the last one they have hit.\r\n\r\n"
                "Returns a boolean value.  Takes no arguments." },

        { OP_PATH_FLOWN, "Path-flown (Boolean training operator)\r\n"
                "\tBecomes true when all the waypoints in the path have been flown, in sequence.\r\n\r\n"
                "Returns a boolean value.  Takes no arguments." },

        { OP_WAYPOINT_TWICE, "Waypoint-twice (Boolean training operator)\r\n"
                "\tBecomes true when a waypoint is hit that is before the last one hit, which "
                "indicates they have flown a waypoint twice.\r\n\r\n"
                "Returns a boolean value.  Takes no arguments." },

        { OP_TRAINING_MSG, "Training-msg (Action training operator)\r\n"
                "\tSends the player a training message.  Uses the same messages as normal messages, "
                "only they get displayed differently using this operator.  If a secondary message "
                "is specified, it is sent the last time, while the primary message is sent all other "
                "times (event should have a repeat count greater than 1).\r\n\r\n"
                "Takes 1-3 arguments...\r\n"
                "\t1:\tName of primary message to send.\r\n"
                "\t2:\tName of secondary message to send (optional).\r\n"
                "\t3:\tDelay (in seconds) to wait before sending message. (optional)\r\n"
                "\t4:\tAmount of Time (in seconds) to display message (optional)." },

        { OP_SET_TRAINING_CONTEXT_FLY_PATH, "Set-training-context-fly-path (Training context operator)\r\n"
                "\tTells FreeSpace that the player is expected to fly a waypoint path.  This must be "
                "executed before waypoint-missed, waypoint-twice and path-flown operators become valid.\r\n\r\n"
                "Takes 2 arguments...\r\n"
                "\t1:\tName of waypoint path player should fly.\r\n"
                "\t2:\tDistance away a player needs to be from a waypoint for it to be registered as flown." },

        { OP_SET_TRAINING_CONTEXT_SPEED, "Set-training-context-speed (Training context operator)\r\n"
                "\tTells FreeSpace that the player is expected to fly within a certain speed range.  Once "
                "this operator has been executed, you can measure how long they have been within this "
                "speed range with the speed operator.\r\n\r\n"
                "Takes 2 arguments...\r\n"
                "\t1:\tMinimum speed of range player is to fly between.\r\n"
                "\t2:\tMaximum speed of range player is to fly between." },

        { OP_GRANT_PROMOTION, "Grant promotion (Action operator)\r\n"
                "\tIn a single player game, this function grants a player an automatic promotion to the "
                "next rank which the player can obtain.  If he is already at the highest rank, this "
                "operator has no effect.  It takes no arguments." },

        { OP_GRANT_MEDAL, "Grant medal (Action operator)\r\n"
                "\tIn single player missions, this function grants the given medal to the player.  "
                "Currently, only 1 medal will be allowed to be given per mission.\r\n\r\n"
                "Takes 1 argument...\r\n"
                "\t1:\tName of medal to grant to player." },

        { OP_GOOD_SECONDARY_TIME, "Set preferred secondary weapons\r\n"
                "\tThis sexpression is used to inform the AI about preferred secondary weapons to "
                "fire during combat.  When this expression is evaluated, any AI ships of the given "
                "team prefer to fire the given weapon at the given ship. (Preferred over other "
                "secondary weapons)\r\n\r\n"
                "Takes 4 argument...\r\n"
                "\t1:\tTeam name which will prefer firing given weapon\r\n"
                "\t2:\tMaximum number of this type of weapon above team can fire.\r\n"
                "\t3:\tWeapon name (list includes only the valid weapons for this expression\r\n"
                "\t4:\tShip name at which the above named team should fire the above named weapon." },

        { OP_AND_IN_SEQUENCE, "And in sequence (Boolean operator)\r\n"
                "\tReturns true if all of its arguments have become true in the order they are "
                "listed in.\r\n\r\n"
                "Returns a boolean value.  Takes 2 or more boolean arguments." },

        { OP_SKILL_LEVEL_AT_LEAST, "Skill level at least (Boolean operator)\r\n"
                "\tReturns true if the player has selected the given skill level or higher.\r\n\r\n"
                "Returns a boolean value.  Takes 1 argument...\r\n"
                "\t1:\tName of the skill level to check." },

        { OP_NUM_PLAYERS, "Num players (Status operator)\r\n"
                "\tReturns the current number of players (multiplayer) playing in the current mission.\r\n\r\n"
                "Returns a numeric value.  Takes no arguments." },

        { OP_IS_CARGO_KNOWN, "Is cargo known (Boolean operator)\r\n"
                "\tReturns true if all of the specified objects' cargo is known by the player (i.e. they "
                "have scanned each one.\r\n\r\n"
                "Returns a boolean value.  Takes 1 or more arguments...\r\n"
                "\tAll:\tName of ship to check if its cargo is known." },

        { OP_HAS_BEEN_TAGGED_DELAY, "Has ship been tagged (delay) (Boolean operator)\r\n"
                "\tReturns true if all of the specified ships have been tagged.\r\n\r\n"
                "Returns a boolean value after <delay> seconds when all ships have been tagged.  Takes 2 or more arguments...\r\n"
                "\t1:\tDelay in seconds after which sexpression will return true when all cargo scanned."
                "\tRest:\tNames of ships to check if tagged.." },

        { OP_CAP_SUBSYS_CARGO_KNOWN_DELAY, "Is capital ship subsystem cargo known (delay) (Boolean operator)\r\n"
                "\tReturns true if all of the specified subsystem cargo is known by the player.\r\n"
                "\tNote: Cargo must be explicitly named.\r\n\r\n"
                "Returns a boolean value after <delay> seconds when all cargo is known.  Takes 3 or more arguments...\r\n"
                "\t1:\tDelay in seconds after which sexpression will return true when all cargo scanned.\r\n"
                "\t2:\tName of capital ship\r\n"
                "\tRest:\tNames of subsystems to check for cargo known.." },

        { OP_CARGO_KNOWN_DELAY, "Is cargo known (delay) (Boolean operator)\r\n"
                "\tReturns true if all of the specified objects' cargo is known by the player (i.e. they "
                "have scanned each one.\r\n\r\n"
                "Returns a boolean value after <delay> seconds when all cargo is known.  Takes 2 or more arguments...\r\n"
                "\t1:\tDelay in seconds after which sexpression will return true when all cargo scanned."
                "\tRest:\tNames of ships/cargo to check for cargo known." },

        { OP_WAS_PROMOTION_GRANTED, "Was promotion granted (Boolean operator)\r\n"
                "\tReturns true if a promotion was granted via the 'Grant promotion' operator in the mission.\r\n\r\n"
                "Returns a boolean value.  Takes no arguments." },

        { OP_WAS_MEDAL_GRANTED, "Was medal granted (Boolean operator)\r\n"
                "\tReturns true if a medal was granted via via the 'Grant medal' operator in the mission.  "
                "If you provide the optional argument to this operator, then true is only returned if the "
                "specified medal was granted.\r\n\r\n"
                "Returns a boolean value.  Takes 0 or 1 arguments...\r\n"
                "\t1:\tName of medal to specifically check for (optional)." },

        { OP_GOOD_REARM_TIME, "Good rearm time (Action operator)\r\n"
                "\tInforms the game logic that right now is a good time for a given team to attempt to "
                "rearm their ships.  The time parameter specified how long the \"good time\" will last.\r\n\r\n"
                "Takes 2 arguments...\r\n"
                "\t1:\tTeam Name\r\n"
                "\t2:\tTime in seconds rearm window should last" },

        { OP_ALLOW_SHIP, "Allow ship (Action operator)\r\n"
                "\tThis operator makes the given ship type available to the Terran team.  Players will be "
                "able to have ships of this type in their starting wings in all future missions of this "
                "campaign.\r\n\r\n"
                "Takes 1 argument...\r\n"
                "\t1:\tName of ship type (or ship class) to allow." },

        { OP_ALLOW_WEAPON, "Allow weapon (Action operator)\r\n"
                "\tThis operator makes the given weapon available to the Terran team.  Players will be "
                "able to equip ships with in all future missions of this campaign.\r\n\r\n"
                "Takes 1 argument...\r\n"
                "\t1:\tName of weapon (primary or secondary) to allow." },

        { OP_TECH_ADD_SHIP, "Tech add ship (Action operator)\r\n"
                "\tThis operator makes the given ship type available in the techroom database.  Players will "
                "then be able to view this ship's specs there.\r\n\r\n"
                "Takes 1 or more arguments...\r\n"
                "\tAll:\tName of ship type (or ship class) to add." },

        { OP_TECH_ADD_WEAPON, "Tech add weapon (Action operator)\r\n"
                "\tThis operator makes the given weapon available in the techroom database.  Players will "
                "then be able to view this weapon's specs there.\r\n\r\n"
                "Takes 1 or more arguments...\r\n"
                "\tAll:\tName of weapon (primary or secondary) to add." },

        { OP_AI_EVADE_SHIP, "Ai-evade ship (Ship goal)\r\n"
                "\tCauses the specified ship to go into evade mode and run away like the weak "
                "sally-boy it is.\r\n\r\n"
                "Takes 2 arguments...\r\n"
                "\t1:\tName of ship to evade from.\r\n"
                "\t2:\tGoal priority (number between 0 and 89)." },

        { OP_AI_STAY_NEAR_SHIP, "Ai-stay near ship (Ship goal)\r\n"
                "\tCauses the specified ship to keep itself near the given ship and not stray too far "
                "away from it.\r\n\r\n"
                "Takes 2 arguments...\r\n"
                "\t1:\tName of ship to stay near.\r\n"
                "\t2:\tGoal priority (number between 0 and 89)." },

        { OP_AI_KEEP_SAFE_DISTANCE, "Ai-keep safe distance (Ship goal)\r\n"
                "\tTells the specified ship to stay a safe distance away from any ship that isn't on the "
                "same team as it.\r\n\r\n"
                "Takes 1 argument...\r\n"
                "\t1:\tGoal priority (number between 0 and 89)." },

        { OP_AI_IGNORE, "Ai-ignore (Ship goal)\r\n"
                "\tTells all ships to ignore the given ship and not consider it as a valid "
                "target to attack.\r\n\r\n"
                "Takes 2 arguments...\r\n"
                "\t1:\tName of ship to ignore.\r\n"
                "\t2:\tGoal priority (number between 0 and 89)." },

        { OP_AI_STAY_STILL, "Ai-stay still (Ship goal)\r\n"
                "\tCauses the specified ship to stay still.  The ship will do nothing until attacked at "
                "which time the ship will come to life and defend itself.\r\n\r\n"
                "Takes 2 arguments...\r\n"
                "\t1:\tShip or waypoint the ship staying still will directly face (currently not implemented)\r\n"
                "\t2:\tGoal priority (number between 0 and 89)." },

        { OP_AI_PLAY_DEAD, "Ai-play dead (Ship goal)\r\n"
                "\tCauses the specified ship to pretend that it is dead and not do anything.  This "
                "expression should be used to indicate that a ship has no pilot and cannot respond "
                "to any enemy threats.  A ship playing dead will not respond to any attack.  This "
                "should really be named ai-is-dead\r\n\r\n"
                "Takes 1 argument...\r\n"
                "\t1:\tGoal priority (number between 0 and 89)." },

        { OP_FLASH_HUD_GAUGE, "Ai-flash hud gauge (Training goal)\r\n"
                "\tCauses the specified hud gauge to flash to draw the player's attention to it.\r\n\r\n"
                "Takes 1 argument...\r\n"
                "\t1:\tName of hud gauge to flash." },

        { OP_SHIP_VISIBLE, "ship-visible\r\n"
                "\tCauses the ships listed in this sexpression to be visible with player sensors.\r\n\r\n"
                "Takes 1 or more arguments...\r\n"
                "\t1+:\tName of ships to make visible to sensors." },

        { OP_SHIP_INVISIBLE, "ship-invisible\r\n"
                "\tCauses the ships listed in this sexpression to be invisible to player sensors.\r\n\r\n"
                "Takes 1 or more arguments...\r\n"
                "\t1+:\tName of ships to make invisible to sensors." },

        { OP_SHIP_VULNERABLE, "ship-vulnerable\r\n"
                "\tCauses the ship listed in this sexpression to be vulnerable to weapons.\r\n\r\n"
                "Takes 1 or more arguments...\r\n"
                "\t1+:\tName of ships to make vulnerable to weapons." },

        { OP_SHIP_INVULNERABLE, "ship-invulnerable\r\n"
                "\tCauses the ships listed in this sexpression to be invulnerable to weapons.  Use with caution!!!!\r\n\r\n"
                "Takes 1 or more arguments...\r\n"
                "\t1+:\tName of ships to make invulnerable to weapons." },

        { OP_SHIP_GUARDIAN, "ship-guardian\r\n"
                "\tCauses the ships listed in this sexpression to not be killable by weapons.  Use with caution!!!!\r\n\r\n"
                "Takes 1 or more arguments...\r\n"
                "\t1+:\tName of ships to make unkillable." },

        { OP_SHIP_NO_GUARDIAN, "ship-no-guardian\r\n"
                "\tCauses the ships listed in this sexpression to be killable by weapons, if not invulnerable.\r\n\r\n"
                "Takes 1 or more arguments...\r\n"
                "\t1+:\tName of ships to make killable." },

        { OP_PERCENT_SHIPS_DEPARTED, "percent-ships-departed\r\n"
                "\tBoolean function which returns true if the percentage of ships in the listed ships and wings "
                "which have departed is greater or equal to the given percentage.  For wings, all ships of all waves "
                "are used for calculation for the total possible ships to depart.\r\n\r\n"
                "Takes 2 or more arguments...\r\n"
                "\t1:\tPercentge of departed ships at which this function will return true.\r\n"
                "\t2+:\tList of ships/wings whose departure status should be determined." },

        { OP_PERCENT_SHIPS_DESTROYED, "percent-ships-destroyed\r\n"
                "\tBoolean function which returns true if the percentage of ships in the listed ships and wings "
                "which have been destroyed is greater or equal to the given percentage.  For wings, all ships of all waves "
                "are used for calculation for the total possible ships to be destroyed.\r\n\r\n"
                "Takes 2 or more arguments...\r\n"
                "\t1:\tPercentge of destroyed ships at which this function will return true.\r\n"
                "\t2+:\tList of ships/wings whose destroyed status should be determined." },

        { OP_RED_ALERT, "red-alert\r\n"
                "\tCauses Red Alert status in a mission.  This function ends the current mission, and moves to "
                "the next mission in the campaign under red alert status.  There should only be one branch from "
                "a mission that uses this expression\r\n\r\n"
                "Takes no arguments."},

        { OP_DEPART_NODE_DELAY, "depart-node-delay\r\n"
                "\tReturns true N seconds after the listed ships depart, if those ships depart within the "
                "radius of the given jump node.  The delay value is given in seconds.\r\n\r\n"
                "Takes 3 or more arguments...r\n"
                "\t1:\tDelay in seconds after the last ship listed departed before this expression can return true.\r\n"
                "\t2:\tName of a jump node\r\n"
                "\t3+:\tList of ships to check for departure within radius of the jump node." },

        { OP_DESTROYED_DEPARTED_DELAY, "destroyed-or-departed-delay\r\n"
                "\tReturns true N seconds after all the listed ships or wings have been destroyed or have "
                "departed.\r\n\r\n"
                "Takes 2 or more arguments...\r\n"
                "\t1:\tDelay in seconds after the last ship/wing is destroyed or departed before this expression can return true.\r\n"
                "\t2+:\tName of a ship or wing" },

        { OP_SPECIAL_CHECK, "Special-check\r\n"
                "\tMike K.'s special training sexp.  Returns a boolean value.  Takes 1 argument as follows:\r\n"
                "\t0:    Ship \"Freighter 1\" is aspect locked by the player\r\n"
                "\t1:    Player has fired Interceptor#Weak at Freighter 1\r\n"
                "\t2:    Ship \"Freighter 1\", subsystem \"Weapons\" is aspect locked by the player\r\n"
                "\t3:    Apply 10 points of damage to player's forward shields (action operator)\r\n"
                "\t4:    Player's front shields are nearly gone\r\n"
                "\t5:    Quickly recharge player's front shields (action operator)\r\n"
                "\t6:    Reduce all shield quadrants except front to 0 (action operator)\r\n"
                "\t7:    Front shield quadrant is near maximum strength\r\n"
                "\t8:    Rear shield quadrant is near maximum strength\r\n"
                "\t9:    Reduce left and right shield quadrants to 0 (action operator)\r\n"
                "\t10:   Player has fewer than 8 missiles left\r\n"
                "\t11:   Player has 8 or more missiles left\r\n"
                "\t12:   Player has fewer than 4 missiles left\r\n"
                "\t13:   Reduce front shield quadrant to 0 (action operator)\r\n"
                "\t100:  Player is out of countermeasures\r\n"
                "\t2000: Training failed"
        },

        { OP_END_CAMPAIGN, "end-campaign\r\n"
                "\tEnds the builtin campaign.  Should only be used by the main FreeSpace campaign\r\n"
                "\t1:\tEnd Campaign even if the player is dead (optional; defaults to true)\r\n" },

        { OP_WARP_BROKEN, "break-warp\r\n"
                "\tBreak the warp drive on the specified ship.  A broken warp drive can be repaired by "
                "a repair ship.  Takes 1 or more arguments...\r\n"
                "\tAll:\tList of ships to break the warp drive on" },

        { OP_WARP_NOT_BROKEN, "fix-warp\r\n"
                "\tFixes a broken warp drive instantaneously.  This option applies to warp drives broken with "
                "the break-warp sepxression.  Takes 1 or more arguments...\r\n"
                "\tAll:\tList of ships whose warp drive should be fixed"},

        { OP_WARP_NEVER, "never-warp\r\n"
                "\tNever allows a ship to warp out.  When this sexpression is used, the given ships will "
                "never be able to warp out.  The warp drive cannot be repaired.  Takes 1 or more arguments...\r\n"
                "\tAll:\tList of ships whose are not allowed to warp out under any condition"},

        { OP_WARP_ALLOWED, "allow-warp\r\n"
                "\tAllows a ship which was previously not allowed to warp out to do so.  When this sexpression is "
                "used, the given ships will be able to warp out again.  Takes 1 or more arguments...\r\n"
                "\tAll:\tList of ships whose are allowed to warp out"},

        { OP_BEAM_FIRE, "fire-beam\r\n"
                "\tFire a beam weapon from a specified subsystem\r\n"
                "\t1:\tShip which will be firing\r\n"
                "\t2:\tTurret which will fire the beam (note, this turret must have at least 1 beam weapon on it)\r\n"
                "\t3:\tShip which will be targeted\r\n"
                "\t4:\tSubsystem to target (optional)\r\n"
                "\t5:\tWhether to force the beam to fire (disregarding FOV and subsystem status) (optional)\r\n" },

        { OP_IS_TAGGED, "is-tagged\r\n"
                "\tReturns whether a given ship is tagged or not\r\n"},

        { OP_NUM_KILLS, "num-kills\r\n"
                "\tReturns the # of kills a player has. The ship specified in the first field should be the ship the player is in.\r\n"
                "\tSo, for single player, this would be Alpha 1. For multiplayer, it can be any ship with a player in it. If, at any\r\n"
                "\ttime there is no player in a given ship, this sexpression will return 0"},

        { OP_NUM_TYPE_KILLS, "num-type-kills\r\n"
                "\tReturns the # of kills a player has on a given ship type (fighter, bomber, cruiser, etc).\r\n"
                "The ship specified in the first field should be the ship the player is in.\r\n"
                "\tSo, for single player, this would be Alpha 1. For multiplayer, it can be any ship with a player in it. If, at any\r\n"
                "\ttime there is no player in a given ship, this sexpression will return 0"},

        { OP_NUM_CLASS_KILLS, "num-class-kills\r\n"
                "\tReturns the # of kills a player has on a specific ship class (Ulysses, Hercules, etc).\r\n"
                "The ship specified in the first field should be the ship the player is in.\r\n"
                "\tSo, for single player, this would be Alpha 1. For multiplayer, it can be any ship with a player in it. If, at any\r\n"
                "\ttime there is no player in a given ship, this sexpression will return 0"},

        { OP_BEAM_FREE, "beam-free\r\n"
                "\tSets one or more beam weapons to allow firing for a given ship\r\n"
                "\t1: Ship to be operated on\r\n"
                "\t2, 3, etc : List of turrets to activate\r\n"},

        { OP_BEAM_FREE_ALL, "beam-free-all\r\n"
                "\tSets all beam weapons on the specified ship to be active\r\n"},

        { OP_BEAM_LOCK, "beam-lock\r\n"
                "\tSets one or more beam weapons to NOT allow firing for a given ship\r\n"
                "\t1: Ship to be operated on\r\n"
                "\t2, 3, etc : List of turrets to deactivate\r\n"},

        { OP_BEAM_LOCK_ALL, "beam-lock-all\r\n"
                "\tSets all beam weapons on the specified ship to be deactivated\r\n"},

        { OP_TURRET_FREE, "turret-free\r\n"
                "\tSets one or more turret weapons to allow firing for a given ship\r\n"
                "\t1: Ship to be operated on\r\n"
                "\t2, 3, etc : List of turrets to activate\r\n"},

        { OP_TURRET_FREE_ALL, "turret-free-all\r\n"
                "\tSets all turret weapons on the specified ship to be active\r\n"},

        { OP_TURRET_LOCK, "turret-lock\r\n"
                "\tSets one or more turret weapons to NOT allow firing for a given ship\r\n"
                "\t1: Ship to be operated on\r\n"
                "\t2, 3, etc : List of turrets to deactivate\r\n"},

        { OP_TURRET_LOCK_ALL, "turret-lock-all\r\n"
                "\tSets all turret weapons on the specified ship to be deactivated\r\n"},

        { OP_ADD_REMOVE_ESCORT, "add-remove-escort\r\n"
                "\tAdds or removes a ship from an escort list.\r\n"
                "\t1: Ship to be added or removed\r\n"
                "\t2: 0 to remove from the list, any positive value will be used as the escort priority\r\n"
                "NOTE : it _IS_ safe to add a ship which may already be on the list or remove\r\n"
                "a ship which is not on the list\r\n"},

        { OP_AWACS_SET_RADIUS, "awacs-set-radius\r\n"
                "\tSets the awacs radius for a given ship subsystem. NOTE : does not work properly in multiplayer\r\n"
                "\t1: Ship which has the awacs subsystem\r\n"
                "\t2: Awacs subsystem\r\n"
                "\t3: New radius\r\n"},

        { OP_SEND_MESSAGE_LIST, "send-message-list\r\n"
                "\tSends a series of delayed messages. All times are accumulated.\r\n"
                "\t1:\tName of who the message is from.\r\n"
                "\t2:\tPriority of message (\"Low\", \"Normal\" or \"High\").\r\n"
                "\t3:\tName of message (from message editor).\r\n"
                "\t4:\tDelay from previous message in list (if any) in ms\r\n"
                "Use Add-Data for multiple messages.\r\n\r\n"
                "IMPORTANT: Each additional message in the list MUST HAVE four entries; "
                "any message without the four proper fields will be ignored, as will any "
                "successive messages."},

        { OP_CAP_WAYPOINT_SPEED, "cap-waypoint-speed\r\n"
                "\tSets the maximum speed of a ship while flying waypoints.\r\n"
                "\t1: Ship name\r\n"
                "\t2: Maximum speed while flying waypoints (must be greater than 0)\r\n"
                "\tNOTE: This will only work if the ship is already in the game\r\n"
                "\tNOTE: Set to -1 to reset\r\n"},

        { OP_TURRET_TAGGED_ONLY_ALL, "turret-tagged-only\r\n"
                "\tMakes turrets target and hence fire strictly at tagged objects\r\n"
                "\t1: Ship name\r\n"
                "\tNOTE: Will not stop a turret already firing at an untagged ship\r\n"},

        { OP_TURRET_TAGGED_CLEAR_ALL, "turret-tagged-clear\r\n"
                "\tRelaxes restriction on turrets targeting only tagged ships\r\n"
                "\t1: Ship name\r\n"},

        { OP_SECONDARIES_DEPLETED, "secondaries-depleted\r\n"
                "\tReturns true if ship is out of secondary weapons\r\n"
                "\t1: Ship name\r\n"},

        { OP_SUBSYS_SET_RANDOM, "subsys-set-random\r\n"
                "\tSets ship subsystem strength in a given range\r\n"
                "\t1: Ship name\r\n"
                "\t2: Low range\r\n"
                "\t3: High range\r\n"
                "\t4: List of subsys names not to be randomized\r\n"},

        { OP_SUPERNOVA_START, "supernova-start\r\n"
                "\t1: Time in seconds until the supernova shockwave hits the player\r\n"},

        { OP_WEAPON_RECHARGE_PCT, "weapon-recharge-pct\r\n"
                "\tReturns a percentage from 0 to 100\r\n"
                "\t1: Ship name\r\n" },

        { OP_SHIELD_RECHARGE_PCT, "shield-recharge-pct\r\n"
                "\tReturns a percentage from 0 to 100\r\n"
                "\t1: Ship name\r\n" },

        { OP_ENGINE_RECHARGE_PCT, "engine-recharge-pct\r\n"
                "\tReturns a percentage from 0 to 100\r\n"
                "\t1: Ship name\r\n" },

        { OP_CARGO_NO_DEPLETE, "cargo-no-deplete\r\n"
                "\tCauses the named ship to have unlimited cargo.\r\n"
                "\tNote:  only applies to BIG or HUGE ships\r\n"
                "Takes 1 or more arguments...\r\n"
                "\t1:\tName of one of the ships.\r\n"
                "\t2:\toptional: 1 disallow depletion, 0 allow depletion." },

        { OP_SHIELD_QUAD_LOW, "shield-quad-low\r\n"
                "\tReturns true if the specified ship has a shield quadrant below\r\n"
                "\tthe specified threshold percentage\r\n"
                "\t1: Ship name\r\n"
                "\t2: Percentage\r\n" },

        { OP_SECONDARY_AMMO_PCT, "secondary-ammo-pct\r\n"
                "\tReturns the percentage of ammo remaining in the specified bank (0 to 100)\r\n"
                "\t1: Ship name\r\n"
                "\t2: Bank to check (from 0 to N-1, where N is the number of secondary banks in the ship; N or higher will return the cumulative average for all banks)" },

        { OP_IS_SECONDARY_SELECTED, "is-secondary-selected\r\n"
                "\tReturns true if the specified bank is selected\r\n"
                "\t1: Ship name\r\n"
                "\t2: Bank to check (This is a zero-based index. The first bank is numbered 0.)\r\n"},

        { OP_IS_PRIMARY_SELECTED, "is-primary-selected\r\n"
                "\tReturns true if the specified bank is selected\r\n"
                "\t1: Ship name\r\n"
                "\t2: Bank to check (This is a zero-based index. The first bank is numbered 0.)\r\n"},

        { OP_SPECIAL_WARP_DISTANCE, "special-warp-dist\r\n"
                "\tReturns distance to the plane of the knossos device in percent length of ship\r\n"
                "\t(ie, 100 means front of ship is 1 ship length from plane of knossos device)\r\n"
                "\t1: Ship name\r\n"},

        { OP_SET_SPECIAL_WARPOUT_NAME, "special-warpout-name\r\n"
                "\tSets the name of the knossos device to be used for warpout\r\n"
                "\t1: Ship name to exit\r\n"
                "\t2: Name of knossos device\r\n"},

        { OP_SHIP_VANISH, "ship-vanish\r\n"
                "\tMakes the named ship vanish (no log and vanish)\r\n"
                "\tSingle Player Only!  Warning: This will cause ship exit not to be logged, so 'has-departed', etc. will not work\r\n"
                "\t1: List of ship names to vanish\r\n"},

        { OP_IS_SHIP_VISIBLE, "is-ship-visible\r\n"
                "\tCheck whether ship is visible on Player's radar\r\n"
                "\tSingle Player Only!  Returns 0 - not visible, 1 - partially visible, 2 - fully visible.\r\n"
                "\t1: Name of ship to check\r\n"},

        { OP_TEAM_SCORE, "team-score\r\n"
                "\tGet the score of a multi team vs team game.\r\n"
                "\t1: Team index (1 for team 1 and 2 for team 2)\r\n"},
};
// clang-format on

std::vector< op_menu_struct > op_menu = {
    { "Objectives", OP_CATEGORY_OBJECTIVE },   { "Time", OP_CATEGORY_TIME },         { "Logical", OP_CATEGORY_LOGICAL },          { "Arithmetic", OP_CATEGORY_ARITHMETIC },
    { "Status", OP_CATEGORY_STATUS },          { "Change", OP_CATEGORY_CHANGE },     { "Conditionals", OP_CATEGORY_CONDITIONAL }, { "Ai goals", OP_CATEGORY_AI },
    { "Event/Goals", OP_CATEGORY_GOAL_EVENT }, { "Training", OP_CATEGORY_TRAINING },
};

// Goober5000's subcategorization of the Change menu (and possibly other menus
// in the future, if people so choose - see sexp.h)
std::vector< op_menu_struct > op_submenu = { { "Messages and Personas", CHANGE_SUBCATEGORY_MESSAGING },
                                             { "AI Control", CHANGE_SUBCATEGORY_AI_CONTROL },
                                             { "Ship Status", CHANGE_SUBCATEGORY_SHIP_STATUS },
                                             { "Weapons, Shields, and Engines", CHANGE_SUBCATEGORY_SHIELDS_ENGINES_AND_WEAPONS },
                                             { "Subsystems and Health", CHANGE_SUBCATEGORY_SUBSYSTEMS },
                                             { "Cargo", CHANGE_SUBCATEGORY_CARGO },
                                             { "Armor and Damage Types", CHANGE_SUBCATEGORY_ARMOR_AND_DAMAGE_TYPES },
                                             { "Beams and Turrets", CHANGE_SUBCATEGORY_BEAMS_AND_TURRETS },
                                             { "Models and Textures", CHANGE_SUBCATEGORY_MODELS_AND_TEXTURES },
                                             { "Coordinate Manipulation", CHANGE_SUBCATEGORY_COORDINATE_MANIPULATION },
                                             { "Mission and Campaign", CHANGE_SUBCATEGORY_MISSION_AND_CAMPAIGN },
                                             { "Music and Sound", CHANGE_SUBCATEGORY_MUSIC_AND_SOUND },
                                             { "HUD", CHANGE_SUBCATEGORY_HUD },
                                             { "Nav Points", CHANGE_SUBCATEGORY_NAV },
                                             { "Cutscenes", CHANGE_SUBCATEGORY_CUTSCENES },
                                             { "Backgrounds and Nebulae", CHANGE_SUBCATEGORY_BACKGROUND_AND_NEBULA },
                                             { "Jump Nodes", CHANGE_SUBCATEGORY_JUMP_NODES },
                                             { "Special Effects", CHANGE_SUBCATEGORY_SPECIAL_EFFECTS },
                                             { "Variables", CHANGE_SUBCATEGORY_VARIABLES },
                                             { "Other", CHANGE_SUBCATEGORY_OTHER },
                                             { "Mission", STATUS_SUBCATEGORY_MISSION },
                                             { "Player", STATUS_SUBCATEGORY_PLAYER },
                                             { "Multiplayer", STATUS_SUBCATEGORY_MULTIPLAYER },
                                             { "Ship Status", STATUS_SUBCATEGORY_SHIP_STATUS },
                                             { "Weapons, Shields, and Engines", STATUS_SUBCATEGORY_SHIELDS_ENGINES_AND_WEAPONS },
                                             { "Cargo", STATUS_SUBCATEGORY_CARGO },
                                             { "Damage", STATUS_SUBCATEGORY_DAMAGE },
                                             { "Distance and Coordinates", STATUS_SUBCATEGORY_DISTANCE_AND_COORDINATES },
                                             { "Variables", STATUS_SUBCATEGORY_VARIABLES },
                                             { "Other", STATUS_SUBCATEGORY_OTHER } };

/**
 * Internal file used by output_sexps, should not be called from output_sexps
 */
static void output_sexp_html (int sexp_idx, FILE* fp) {
    if (sexp_idx < 0 || sexp_idx > (int)Operators.size ()) return;

    bool printed = false;

    for (auto& help : Sexp_help) {
        if (help.id == Operators[sexp_idx].value) {
            char* new_buf = new char[2 * help.help.size ()];
            char* dest_ptr = new_buf;
            const char* curr_ptr = help.help.c_str ();
            const char* end_ptr = curr_ptr + help.help.size ();
            while (curr_ptr < end_ptr) {
                if (*curr_ptr == '\n') {
                    strcpy (dest_ptr, "\n<br>");
                    dest_ptr += 5;
                }
                else { *dest_ptr++ = *curr_ptr; }
                curr_ptr++;
            }
            *dest_ptr = '\0';

            fprintf (fp, "<dt><b>%s</b></dt>\n<dd>%s</dd>\n", Operators[sexp_idx].text.c_str (), new_buf);
            delete[] new_buf;

            printed = true;
        }
    }

    if (!printed)
        fprintf (
            fp,
            "<dt><b>%s</b></dt>\n<dd>Min arguments: %d, Max arguments: "
            "%d</dd>\n",
            Operators[sexp_idx].text.c_str (), Operators[sexp_idx].min, Operators[sexp_idx].max);
}

/**
 * Output sexp.html file
 */
bool output_sexps (const char* filepath) {
    FILE* fp = fopen (filepath, "w");

    if (0 == fp) {
        EE << "error creating SEXP operator list";
        return false;
    }

    // Header
    fprintf (fp, "<html>\n<head>\n\t<title>SEXP Output - FSO v%s</title>\n</head>\n", FS_VERSION_FULL);
    fputs ("<body>", fp);
    fprintf (fp, "\t<h1>SEXP Output - FSO v%s</h1>\n", FS_VERSION_FULL);

    std::vector< int > done_sexp_ids;
    int x, y, z;

    // Output an overview
    fputs ("<dl>", fp);
    for (x = 0; x < (int)op_menu.size (); x++) {
        fprintf (fp, "<dt><a href=\"#%d\">%s</a></dt>", (op_menu[x].id & OP_CATEGORY_MASK), op_menu[x].name.c_str ());
        for (y = 0; y < (int)op_submenu.size (); y++) {
            if (((op_submenu[y].id & OP_CATEGORY_MASK) == op_menu[x].id)) {
                fprintf (fp, "<dd><a href=\"#%d\">%s</a></dd>", op_submenu[y].id & (OP_CATEGORY_MASK | SUBCATEGORY_MASK), op_submenu[y].name.c_str ());
            }
        }
    }
    fputs ("</dl>", fp);

    // Output the full descriptions
    fputs ("<dl>", fp);
    for (x = 0; x < (int)op_menu.size (); x++) {
        fprintf (fp, "<dt id=\"%d\"><h2>%s</h2></dt>\n", (op_menu[x].id & OP_CATEGORY_MASK), op_menu[x].name.c_str ());
        fputs ("<dd>", fp);
        fputs ("<dl>", fp);
        for (y = 0; y < (int)op_submenu.size (); y++) {
            if (((op_submenu[y].id & OP_CATEGORY_MASK) == op_menu[x].id)) {
                fprintf (fp, "<dt id=\"%d\"><h3>%s</h3></dt>\n", op_submenu[y].id & (OP_CATEGORY_MASK | SUBCATEGORY_MASK), op_submenu[y].name.c_str ());
                fputs ("<dd>", fp);
                fputs ("<dl>", fp);
                for (z = 0; z < (int)Operators.size (); z++) {
                    if ((get_category (Operators[z].value) == op_menu[x].id) && (get_subcategory (Operators[z].value) != -1) &&
                        (get_subcategory (Operators[z].value) == op_submenu[y].id)) {
                        output_sexp_html (z, fp);
                    }
                }
                fputs ("</dl>", fp);
                fputs ("</dd>", fp);
            }
        }
        for (z = 0; z < (int)Operators.size (); z++) {
            if ((get_category (Operators[z].value) == op_menu[x].id) && (get_subcategory (Operators[z].value) == -1)) { output_sexp_html (z, fp); }
        }
        fputs ("</dl>", fp);
        fputs ("</dd>", fp);
    }
    for (z = 0; z < (int)Operators.size (); z++) {
        if (!get_category (Operators[z].value)) { output_sexp_html (z, fp); }
    }
    fputs ("</dl>", fp);
    fputs ("</body>\n</html>\n", fp);

    fclose (fp);

    return true;
}
