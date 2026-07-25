# -*- mode: gdscript -*-
#
# MissionData -- a mission's ship layout, emitted by tools/mission2tres.cc
# through RETAIL'S OWN mission parser running under Fred_running (every
# object created regardless of arrival cues, the editor's view of the
# mission). Arrival/departure timing, wings, events and directives belong
# to later slices; this is the placement contract.
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
#     "player_start": bool }
class_name MissionData
extends Resource

@export var mission_name: String = ""
@export var ships: Array = []
