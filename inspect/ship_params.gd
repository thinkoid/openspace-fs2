# -*- mode: gdscript -*-
#
# ShipParams -- the flight-parameter half of ships.tbl, one entry per ship,
# emitted by tools/shiptbl2tres.cc THROUGH RETAIL'S OWN TABLE PARSER (the
# same authoritative-reader pattern as vpstage/cfile): what lands here is
# what parse_shiptbl loaded, including derived values like max_rotvel =
# 2*PI / rotation_time. The shiptbl-check gate cross-checks the slice
# ships' entries against an independent python read of the table text.
#
# `ships` keys are lowercased POF stems ("fighter01"), matching the
# converted asset names; each value:
#   { "name": String,               # "GTF Ulysses"
#     "density": float,             # mass = POF mass * density (ship.cc)
#     "damp": float,                # -> side_slip_time_const
#     "rotdamp": float,
#     "max_vel": Vector3,           # x/y nonzero => retail sets slide
#     "max_rear_vel": float,
#     "max_rotvel": Vector3,        # 2*PI / rotation_time, retail-derived
#     "forward_accel": float, "forward_decel": float,
#     "slide_accel": float, "slide_decel": float,
#     "afterburner_max_vel": Vector3, "afterburner_forward_accel": float }
class_name ShipParams
extends Resource

@export var ships: Dictionary = {}
