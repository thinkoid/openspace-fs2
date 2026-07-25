# -*- mode: gdscript -*-
#
# The flight-model oracle check: replay physics_dump's control inputs
# through inspect/flight_model.gd and compare the trajectories frame by
# frame. The reference ran retail's float32 pipeline, this port runs
# GDScript doubles, so equality is toleranced, not exact -- but the
# tolerances are tight enough that any semantic slip (a wrong damping
# constant, a flipped angle, a reordered update) blows through them by
# orders of magnitude, while float-width drift stays well under.
#
#   godot --headless --path <repo>/inspect --script check_flight.gd \
#       -- <trace.txt>
#
# Prints the measured maxima so the margin stays visible in every log.
extends SceneTree

const TOL_POS := 0.05      # world units over the whole 9s flight
const TOL_ORIENT := 1e-4   # per-component basis error
const TOL_VEL := 0.01      # world units/s

static func _cmax(v: Vector3) -> float:
    return maxf(absf(v.x), maxf(absf(v.y), absf(v.z)))

func _init() -> void:
    var args := OS.get_cmdline_user_args()
    if args.size() != 1:
        printerr("usage: ... -- <trace.txt>")
        quit(2)
        return

    var fm = load("res://flight_model.gd").new()
    var dt := 1.0 / 60.0

    var max_pos := 0.0
    var max_orient := 0.0
    var max_vel := 0.0
    var frames := 0
    var bad := 0

    for line in FileAccess.get_file_as_string(args[0]).split("\n"):
        if line.is_empty():
            continue
        var t := line.split(" ")
        # <frame> ci P H B F pos X Y Z orient R..U..F.. vel X Y Z
        var ci := {
            "pitch": float(t[2]), "heading": float(t[3]),
            "bank": float(t[4]), "forward": float(t[5]),
        }
        fm.read_flying_controls(ci, dt)
        fm.sim(dt)

        var ref_pos := Vector3(float(t[7]), float(t[8]), float(t[9]))
        var ref_r := Vector3(float(t[11]), float(t[12]), float(t[13]))
        var ref_u := Vector3(float(t[14]), float(t[15]), float(t[16]))
        var ref_f := Vector3(float(t[17]), float(t[18]), float(t[19]))
        var ref_vel := Vector3(float(t[21]), float(t[22]), float(t[23]))

        max_pos = maxf(max_pos, (fm.pos - ref_pos).length())
        max_orient = maxf(max_orient,
            maxf(_cmax(fm.rvec - ref_r),
                 maxf(_cmax(fm.uvec - ref_u), _cmax(fm.fvec - ref_f))))
        max_vel = maxf(max_vel, (fm.vel - ref_vel).length())
        frames += 1

    if frames == 0:
        printerr("FAIL: empty trace")
        quit(1)
        return

    if max_pos > TOL_POS:
        printerr("FAIL: position diverges %s (tol %s)" % [str(max_pos), str(TOL_POS)])
        bad += 1
    if max_orient > TOL_ORIENT:
        printerr("FAIL: orientation diverges %s (tol %s)"
            % [str(max_orient), str(TOL_ORIENT)])
        bad += 1
    if max_vel > TOL_VEL:
        printerr("FAIL: velocity diverges %s (tol %s)" % [str(max_vel), str(TOL_VEL)])
        bad += 1

    if bad == 0:
        print("OK: %d frames vs retail physics -- max |dpos| %s, |dorient| %s, |dvel| %s"
            % [frames, str(max_pos), str(max_orient), str(max_vel)])
    quit(1 if bad > 0 else 0)
