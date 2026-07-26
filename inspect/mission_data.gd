# -*- mode: gdscript -*-
#
# MissionData -- a mission's layout and logic, emitted by
# tools/mission2tres.cc through RETAIL'S OWN mission parser running under
# Fred_running (every object created regardless of arrival cues, the
# editor's view of the mission). Arrival/departure timing and wings belong
# to a later refinement.
#
# Everything is in FS2's OWN frame (+Z forward, orient rows rvec/uvec/fvec)
# exactly as parsed; the scene maps to Godot at the visual boundary, same
# (x, y, -z) map as everything else. Each `ships` entry:
#   { "name": String,          # "Alpha 1"
#     "ship_class": String,    # "GTF Myrmidon" (ships.tbl $Name)
#     "pof": String,           # lowercased POF stem -> the converted GLB
#     "team": int,
#     "pos": Vector3,
#     "rvec": Vector3, "uvec": Vector3, "fvec": Vector3,
#     "player_start": bool,
#     "ai_goals": Array }      # initial orders, decoded by retail:
#                              # { "mode": int (AI_GOAL_* bit, aigoals.hh),
#                              #   "submode": int, "priority": int,
#                              #   "target": String }  # ship OR waypoint path
#
# `events` entries mirror retail's mission_event (missiongoals.hh):
#   { "name": String, "formula": String,   # canonical one-line sexp text
#     "repeat_count": int, "interval": int (seconds, -1 none),
#     "score": int, "chain_delay": int (-1 = not chained),
#     "objective_text": String,            # directives gauge line
#     "objective_key_text": String }       # still carries $KEY$ tokens --
#                                          # substitute via the bindings table
# `goals`: { "name", "type" (PRIMARY 0/SECONDARY 1/BONUS 2 | INVALID bit 16),
#            "score", "message", "formula" }
# `messages`: { "name", "text", "avi", "wave" }  # text keeps $token$/[bracket]
# `waypoints`: { "name", "points": [Vector3...] }  # FS2 frame
class_name MissionData
extends Resource

@export var mission_name: String = ""
@export var ships: Array = []
@export var events: Array = []
@export var goals: Array = []
@export var messages: Array = []
@export var waypoints: Array = []
