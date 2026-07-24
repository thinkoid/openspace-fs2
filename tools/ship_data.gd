# -*- mode: gdscript -*-
#
# ShipData -- the FS2-specific half of a converted POF, the part glTF has no
# slot for (docs/godot-migration-plan.md, "POF is not merely a mesh"). Emitted
# by tools/pof2glb.cc as a .tres beside the .glb; the inspection scene, and
# later the game, load it typed.
#
# This file is the *contract* the emitter targets, versioned next to it. It is
# not itself generated -- the .tres files are (gitignored). The inspection
# Godot project keeps a copy of this script at its res:// root so `res://
# ship_data.gd` below resolves.
#
# COORDINATES. Every point, normal and offset here is in Godot's frame: +Y up,
# -Z forward, the same frame as the sibling GLB. pof2glb.cc's to_godot() puts
# them there from libpof's memory frame; the net map from the POF file frame is
# (x, y, -z) (docs/pof-corpus-survey.txt, tools/pof2glb.cc "the axis map").
class_name ShipData
extends Resource

@export var source_pof: String = ""
@export var pof_version: int = 0

@export var radius: float = 0.0
@export var mass: float = 0.0
@export var mass_center: Vector3 = Vector3.ZERO

# Bounding box, component-wise min/max in the Godot frame (the file's mirrored
# corners crossed on X under the axis map, so these are recomputed, not the
# transformed corners in place).
@export var bbox_min: Vector3 = Vector3.ZERO
@export var bbox_max: Vector3 = Vector3.ZERO

@export var detail_levels: PackedInt32Array = PackedInt32Array()
@export var debris_pieces: PackedInt32Array = PackedInt32Array()

# Weapon muzzles. A bank is one firing group; retail keeps gun banks and
# missile banks as separate lists (the pof_dump oracle prints them apart).
# Each bank: { "points": PackedVector3Array, "normals": PackedVector3Array }.
@export var gun_banks: Array = []
@export var missile_banks: Array = []

# Turrets, one per base submodel. Retail merges the gun/missile turret banks
# last-wins and drops which chunk each came from (dump.cc; survey), so this is
# the merged form -- what the game actually turns. Each:
# { "base": int, "arm": int, "normal": Vector3, "fire_points": PackedVector3Array }.
# `base`/`arm` are submodel indices (the turret's base and the submodel it
# physically rotates with).
@export var turrets: Array = []

# Thruster glow banks. Each:
# { "points": PackedVector3Array, "normals": PackedVector3Array,
#   "radii": PackedFloat32Array, "properties": String }.
@export var thrusters: Array = []

# Docking bays. Each:
# { "name": String, "paths": PackedInt32Array,
#   "points": PackedVector3Array, "normals": PackedVector3Array }.
# `paths` indexes the `paths` array below. `name` is a human label derived from
# the bay's $name property (cosmetic -- the game keys bays by index).
@export var docks: Array = []

# Cockpit/view eyes. Each: { "parent": int, "point": Vector3, "normal": Vector3 }.
# `parent` is the submodel the eye rides.
@export var eyes: Array = []

# AI paths (docking approaches, fly-by lanes). Each:
# { "name": String, "parent": String, "sub": int,
#   "points": PackedVector3Array, "radii": PackedFloat32Array }.
# `sub` is `parent` resolved to a submodel index the way retail resolves it
# (leading '$' dropped, last case-insensitive name match, -1 if none).
@export var paths: Array = []

# Subsystem/special points. NOT in the pof_dump oracle: retail converts these
# against an external ships.tbl subsystem list and keeps only $split z
# (dump.cc). Emitted straight from libpof's SPCL chunk, so this field is
# unverified against the oracle -- see tests/check_tres.py. Each:
# { "name": String, "properties": String, "point": Vector3, "radius": float }.
@export var subsystems: Array = []

# Shield mesh -- a flat vertex table and triangles indexing it, both file data
# (no BSP walk; dump.cc). Each tri:
# { "normal": Vector3, "verts": PackedInt32Array (3), "neighbors": PackedInt32Array (3) }.
@export var shield_verts: PackedVector3Array = PackedVector3Array()
@export var shield_tris: Array = []
