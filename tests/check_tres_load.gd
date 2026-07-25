# -*- mode: gdscript -*-
#
# Load an emitted ship-data .tres through the REAL engine and validate it
# against the inspect/ship_data.gd schema. tres-check pins every number in the
# file against the retail oracle, but it does so with a text parser -- this is
# the gate that proves Godot's own VariantParser accepts what pof2glb writes
# (the Vector3(...) literals, the packed arrays, the ext_resource script
# reference) and that the result walks and quacks like ShipData.
#
#   godot --headless --path <repo>/inspect --script check_tres_load.gd -- <ship.tres>
#
# --path must be the inspection project: the .tres carries
# `ext_resource path="res://ship_data.gd"`, and that resolves against the
# project root. Exit 0 clean, 1 on any failure.
extends SceneTree

var bad := 0

func fail(msg: String) -> void:
    printerr("FAIL: " + msg)
    bad += 1

func check(cond: bool, msg: String) -> void:
    if not cond:
        fail(msg)

# The emitter prints every number %.9g, so a whole-valued float (mass 12.0,
# radius 15.0) lands in the file as "12" and loads as int -- accept both.
func is_num(v: Variant) -> bool:
    return v is float or v is int

# points + normals arrays of equal size -- the common bank shape.
func check_bank(b: Variant, what: String) -> void:
    if not (b is Dictionary):
        fail(what + " is not a Dictionary")
        return
    var pts: Variant = b.get("points")
    var nrm: Variant = b.get("normals")
    if not (pts is PackedVector3Array):
        fail(what + ".points is not PackedVector3Array")
        return
    check(nrm is PackedVector3Array and nrm.size() == pts.size(),
          what + ".normals does not mirror .points")

func _init() -> void:
    var args := OS.get_cmdline_user_args()
    if args.size() != 1:
        printerr("usage: godot --headless --path <repo>/inspect"
                 + " --script check_tres_load.gd -- <ship.tres>")
        quit(2)
        return

    var data: Resource = ResourceLoader.load(args[0])
    if data == null:
        printerr("FAIL: ResourceLoader could not load " + args[0])
        quit(1)
        return

    # Identity by script, not `is ShipData`: under headless --script runs the
    # class_name registry is not reliably primed, the script resource is.
    if data.get_script() != load("res://ship_data.gd"):
        printerr("FAIL: resource script is not res://ship_data.gd")
        quit(1)
        return

    check(data.source_pof is String and not data.source_pof.is_empty(),
          "source_pof empty")
    check(data.pof_version is int and data.pof_version > 0, "pof_version")
    check(is_num(data.radius) and data.radius > 0.0, "radius")
    check(is_num(data.mass) and data.mass > 0.0, "mass")
    check(data.mass_center is Vector3, "mass_center")
    check(data.bbox_min is Vector3 and data.bbox_max is Vector3, "bbox")
    check(data.bbox_min.x < data.bbox_max.x
          and data.bbox_min.y < data.bbox_max.y
          and data.bbox_min.z < data.bbox_max.z, "bbox not min<max")

    check(data.detail_levels is PackedInt32Array
          and not data.detail_levels.is_empty(), "detail_levels empty")
    check(data.debris_pieces is PackedInt32Array, "debris_pieces")

    for b in data.gun_banks:
        check_bank(b, "gun_bank")
    for b in data.missile_banks:
        check_bank(b, "missile_bank")

    for t in data.turrets:
        check(t is Dictionary and t.get("base") is int and t.get("arm") is int
              and t.get("normal") is Vector3
              and t.get("fire_points") is PackedVector3Array, "turret shape")

    for b in data.thrusters:
        check_bank(b, "thruster")
        if b is Dictionary:
            var r: Variant = b.get("radii")
            check(r is PackedFloat32Array and b.get("points") is PackedVector3Array
                  and r.size() == b.get("points").size(),
                  "thruster.radii does not mirror .points")
            check(b.get("properties") is String, "thruster.properties")

    for d in data.docks:
        check_bank(d, "dock")
        if d is Dictionary:
            check(d.get("name") is String, "dock.name")
            check(d.get("paths") is PackedInt32Array, "dock.paths")

    for e in data.eyes:
        check(e is Dictionary and e.get("parent") is int
              and e.get("point") is Vector3 and e.get("normal") is Vector3,
              "eye shape")

    for p in data.paths:
        if not (p is Dictionary):
            fail("path is not a Dictionary")
            continue
        check(p.get("name") is String and p.get("parent") is String
              and p.get("sub") is int, "path name/parent/sub")
        var pts: Variant = p.get("points")
        var radii: Variant = p.get("radii")
        check(pts is PackedVector3Array and radii is PackedFloat32Array
              and radii.size() == pts.size(), "path.radii does not mirror .points")

    for s in data.subsystems:
        check(s is Dictionary and s.get("name") is String
              and s.get("properties") is String and s.get("point") is Vector3
              and is_num(s.get("radius")), "subsystem shape")

    check(data.shield_verts is PackedVector3Array, "shield_verts")
    for t in data.shield_tris:
        if not (t is Dictionary and t.get("normal") is Vector3
                and t.get("verts") is PackedInt32Array
                and t.get("neighbors") is PackedInt32Array):
            fail("shield tri shape")
            continue
        var v: PackedInt32Array = t.get("verts")
        check(v.size() == 3 and t.get("neighbors").size() == 3,
              "shield tri arity")
        for k in v:
            check(k >= 0 and k < data.shield_verts.size(),
                  "shield tri vertex index out of range")

    if bad == 0:
        print("OK: %s loads as ShipData: %d/%d gun/missile banks, %d turrets, %d thrusters, %d docks, %d eyes, %d paths, %d subsystems, shield %d/%d verts/tris"
            % [args[0].get_file(), data.gun_banks.size(),
               data.missile_banks.size(), data.turrets.size(),
               data.thrusters.size(), data.docks.size(), data.eyes.size(),
               data.paths.size(), data.subsystems.size(),
               data.shield_verts.size(), data.shield_tris.size()])
    quit(1 if bad > 0 else 0)
