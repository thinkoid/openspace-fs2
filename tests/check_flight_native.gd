# -*- mode: gdscript -*-
#
# The INVERTED flight gate: replay physics_dump's control inputs through
# the native boundary (libfs2's FS2.fly_step -- retail's integrator, the
# same machine code physics_dump runs) and demand EXACT equality with the
# trace. Where check_flight.gd tolerances the GDScript port's double-vs-
# float32 drift, this side has no drift to tolerance: same objects, same
# instruction stream, same inputs -- any difference is a boundary bug.
#
# The parameters come from flight_model.gd's FIGHTER constant -- the
# retired port serving as the spec, exactly as the plan's gate-inversion
# promises (the physics_dump mirror stays the contract's other half).
#
#   godot --headless --path <repo>/inspect --script check_flight_native.gd \
#       -- <fs2.gdextension> <trace.txt>
#
# Exit 0 clean, 1 on any failure, 2 on usage.
extends SceneTree

func _init() -> void:
    var args := OS.get_cmdline_user_args()
    if args.size() != 2:
        printerr("usage: -- <fs2.gdextension> <trace.txt>")
        quit(2)
        return

    if GDExtensionManager.load_extension(args[0]) != \
            GDExtensionManager.LOAD_STATUS_OK:
        printerr("FAIL: could not load " + args[0])
        quit(1)
        return

    var sim = ClassDB.instantiate("FS2")
    sim.fly_reset(load("res://flight_model.gd").FIGHTER)

    var dt := 1.0 / 60.0
    var frames := 0
    var bad := 0

    for line in FileAccess.get_file_as_string(args[1]).split("\n"):
        if line.is_empty():
            continue
        var t := line.split(" ")
        # <frame> ci P H B F pos X Y Z orient R..U..F.. vel X Y Z
        sim.fly_step(dt, {
            "pitch": float(t[2]), "heading": float(t[3]),
            "bank": float(t[4]), "forward": float(t[5]),
        })
        var st: Dictionary = sim.fly_state()

        # Vector3 construction snaps the parsed doubles through float32,
        # the bomber05 lesson -- then equality is exact, no tolerances
        var checks := [
            ["pos", Vector3(float(t[7]), float(t[8]), float(t[9]))],
            ["rvec", Vector3(float(t[11]), float(t[12]), float(t[13]))],
            ["uvec", Vector3(float(t[14]), float(t[15]), float(t[16]))],
            ["fvec", Vector3(float(t[17]), float(t[18]), float(t[19]))],
            ["vel", Vector3(float(t[21]), float(t[22]), float(t[23]))],
        ]
        for c in checks:
            if st[c[0]] != c[1]:
                if bad < 10:
                    printerr("FAIL frame %d %s: native %s, trace %s" %
                             [frames, c[0], st[c[0]], c[1]])
                bad += 1
        frames += 1

    if frames == 0:
        printerr("FAIL: empty trace")
        quit(1)
        return

    if bad > 0:
        printerr("FAIL: %d component mismatches over %d frames" %
                 [bad, frames])
        quit(1)
        return

    print("OK: %d frames bit-exact through the native boundary" % frames)
    quit(0)
