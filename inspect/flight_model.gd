# -*- mode: gdscript -*-
#
# FlightModel -- retail's flight integrator, ported function-for-function
# from src/physics/physics.cc (+ the vecmat primitives it leans on) and
# oracle-pinned: tests/physics_dump runs the ORIGINAL code over a scripted
# control sequence and flight-check demands this port reproduce the
# trajectory. Same function names, same phase structure, so it reads against
# the reference.
#
# Scope is the PF_ACCELERATES flight path plus PF_AFTERBURNER_ON -- no
# slide, reduced-damp, shockwave or warp specials; those arrive with the
# slices that need them. The afterburner is retail's exact branch
# (physics.cc:601/626/716): the burner floors the stick, swaps the goal
# to afterburner_max_vel and the accel ramp to the burner's constant;
# with the flag off the code path is IDENTICAL to the oracle-pinned one
# (flight-check replays carry no afterburner). Everything here is in FS2's OWN frame (+Z forward, orient
# rows rvec/uvec/fvec); the scene maps to Godot at the boundary
# (fly.gd; the map is (x, y, -z), tools/pof2glb.cc "the axis map").
#
# The FIGHTER parameters are a synthetic fighter, mirrored byte-for-byte in
# tests/physics_dump.cc -- the contract is the pair; change one, change
# both. ships.tbl is a later slice.
class_name FlightModel

const FIGHTER := {
    "mass": 12.0,
    "max_vel": Vector3(0.0, 0.0, 65.0),   # FS2 fighters do not strafe
    "max_rear_vel": 15.0,
    "max_rotvel": Vector3(1.75, 1.4, 2.75),  # pitch, heading, bank
    "forward_accel_time_const": 3.0,
    "forward_decel_time_const": 2.0,
    "side_slip_time_const": 0.05,
    "rotdamp": 0.35,
}

var p := FIGHTER

# Adopt a ships.tbl entry (a ShipParams dict from shiptbl2tres, retail's
# own parse) in place of the synthetic FIGHTER. pof_mass is the POF's --
# retail computes mass = POF mass * tbl density (ship.cc:1300); nothing in
# the PF_ACCELERATES path reads it, but the record stays faithful.
func set_params(tbl: Dictionary, pof_mass: float) -> void:
    var mv: Vector3 = tbl["max_vel"]
    if mv.x > 0.0 or mv.y > 0.0:
        # 5 retail ships can slide; the slide branch is out of scope until
        # a slice needs it (see header), so their lateral axes go inert
        push_warning("FlightModel: slide not implemented, zeroing lateral max_vel")
        mv.x = 0.0
        mv.y = 0.0
    p = {
        "mass": pof_mass * tbl["density"],
        "max_vel": mv,
        "max_rear_vel": tbl["max_rear_vel"],
        "max_rotvel": tbl["max_rotvel"],
        "forward_accel_time_const": tbl["forward_accel"],
        "forward_decel_time_const": tbl["forward_decel"],
        "side_slip_time_const": tbl["damp"],
        "rotdamp": tbl["rotdamp"],
        "afterburner_max_vel": tbl["afterburner_max_vel"],
        "afterburner_forward_accel_time_const":
            tbl["afterburner_forward_accel"],
    }

# state -- physics_info's living fields, FS2 frame throughout
var pos := Vector3.ZERO
var rvec := Vector3.RIGHT              # orient rows
var uvec := Vector3.UP
var fvec := Vector3.BACK               # FS2 forward is +Z
var vel := Vector3.ZERO                # world
var rotvel := Vector3.ZERO             # local p/h/b rates
var desired_vel := Vector3.ZERO        # world
var desired_rotvel := Vector3.ZERO     # local
var prev_ramp_vel := Vector3.ZERO      # local, the ramp's memory
var speed := 0.0
var fspeed := 0.0
var afterburner := false               # PF_AFTERBURNER_ON

# a ship without burner tanks (afterburner_max_vel.z == 0, retail's
# SIF_AFTERBURNER gate) never engages -- the synthetic FIGHTER has none
func has_afterburner() -> bool:
    return p.get("afterburner_max_vel", Vector3.ZERO).z > 0.0

# physics.cc apply_physics: frame-rate-independent damped approach; damping
# zero snaps to the target. Returns [new_vel, delta_pos].
static func apply_physics(damping: float, desired: float, initial: float,
                          t: float) -> Array:
    if damping < 0.0001:
        return [desired, desired * t]
    var dv := initial - desired
    var e := exp(-t / damping)
    return [dv * e + desired, (1.0 - e) * dv * damping + desired * t]

# physics.cc velocity_ramp, including the close-to-goal speedup hack
static func velocity_ramp(v_in: float, v_goal: float, ramp_time_const: float,
                          t: float) -> float:
    if t == 0.0:
        return v_in
    var delta_v := v_goal - v_in
    var dist: float = abs(delta_v)
    if dist < ramp_time_const / 3.0:
        ramp_time_const = dist / 3.0
    if ramp_time_const < 0.0001:
        return v_goal
    return v_in + delta_v * (1.0 - exp(-t / ramp_time_const))

# vecmat.cc vm_vec_rotate: world -> local through the orient rows
func vec_rotate(src: Vector3) -> Vector3:
    return Vector3(rvec.dot(src), uvec.dot(src), fvec.dot(src))

# vecmat.cc vm_vec_unrotate: local -> world
func vec_unrotate(src: Vector3) -> Vector3:
    return src.x * rvec + src.y * uvec + src.z * fvec

# vecmat.cc sincos_2_matrix by way of vm_angles_2_matrix -- returns the
# rotation's rows [rvec, uvec, fvec] for angles (p, b, h)
static func angles_2_matrix(ap: float, ab: float, ah: float) -> Array:
    var sinp := sin(ap)
    var cosp := cos(ap)
    var sinb := sin(ab)
    var cosb := cos(ab)
    var sinh_ := sin(ah)
    var cosh_ := cos(ah)

    var sbsh := sinb * sinh_
    var cbch := cosb * cosh_
    var cbsh := cosb * sinh_
    var sbch := sinb * cosh_

    return [
        Vector3(cbch + sinp * sbsh, sinb * cosp, sinp * sbch - cbsh),
        Vector3(sinp * cbsh - sbch, cosb * cosp, sbsh + sinp * cbch),
        Vector3(sinh_ * cosp, -sinp, cosh_ * cosp),
    ]

# vecmat.cc vm_orthogonalize_matrix: fvec is truth, uvec advisory, rvec
# rebuilt -- retail's cross order kept exactly
func orthogonalize() -> void:
    var f := fvec.normalized()
    var u: Vector3
    if uvec.length() <= 0.0:
        if rvec.length() <= 0.0:
            u = Vector3(0, 0, 1) if (f.x == 0.0 and f.z == 0.0 and f.y != 0.0) \
                else Vector3(0, 1, 0)
        else:
            u = f.cross(rvec).normalized()
    else:
        u = uvec.normalized()
    var r := u.cross(f).normalized()
    u = f.cross(r)
    rvec = r
    uvec = u
    fvec = f

# physics.cc physics_read_flying_controls, PF_ACCELERATES path (slide
# disabled: prev_ramp_vel.x/y are held at zero like retail does). ci fields
# in [-1, 1]: pitch, heading, bank, forward.
func read_flying_controls(ci: Dictionary, sim_time: float) -> void:
    var pitch := clampf(ci.get("pitch", 0.0), -1.0, 1.0)
    var heading := clampf(ci.get("heading", 0.0), -1.0, 1.0)
    var bank := clampf(ci.get("bank", 0.0), -1.0, 1.0)
    var forward := clampf(ci.get("forward", 0.0), -1.0, 1.0)

    var burn := afterburner and has_afterburner()
    if burn:                     # physics.cc:601 -- the burner floors it
        forward = 1.0

    desired_rotvel.x = pitch * p["max_rotvel"].x
    desired_rotvel.y = heading * p["max_rotvel"].y

    # BANK_WHEN_TURN: the whole math of banking-while-turning
    var delta_bank: float = -(heading * p["max_rotvel"].y) / 2.0

    desired_rotvel.z = bank * p["max_rotvel"].z + delta_bank

    # physics.cc:626: the burner swaps the whole goal, not just the cap
    var vmax: Vector3 = p["afterburner_max_vel"] if burn else p["max_vel"]
    var goal_vel := Vector3(0.0, 0.0, forward * vmax.z)
    if goal_vel.z < -p["max_rear_vel"]:
        goal_vel.z = -p["max_rear_vel"]

    prev_ramp_vel.x = 0.0
    prev_ramp_vel.y = 0.0

    var ramp_time_const: float
    if goal_vel.z >= prev_ramp_vel.z:
        ramp_time_const = p["afterburner_forward_accel_time_const"] \
            if burn else p["forward_accel_time_const"]
    else:
        ramp_time_const = p["forward_decel_time_const"]
    prev_ramp_vel.z = velocity_ramp(prev_ramp_vel.z, goal_vel.z,
                                    ramp_time_const, sim_time)

    desired_vel = rvec * prev_ramp_vel.x + uvec * prev_ramp_vel.y \
        + fvec * prev_ramp_vel.z

# physics.cc physics_sim_rot, no-shockwave path: damp the rotation rates,
# then compound the frame's rotation into orient and re-orthogonalize
func sim_rot(sim_time: float) -> void:
    var rotdamp: float = p["rotdamp"]
    rotvel.x = apply_physics(rotdamp, desired_rotvel.x, rotvel.x, sim_time)[0]
    rotvel.y = apply_physics(rotdamp, desired_rotvel.y, rotvel.y, sim_time)[0]
    rotvel.z = apply_physics(rotdamp, desired_rotvel.z, rotvel.z, sim_time)[0]

    var rot := angles_2_matrix(rotvel.x * sim_time, rotvel.z * sim_time,
                               rotvel.y * sim_time)

    # vm_matrix_x_matrix(tmp, orient, rotmat): row i of the product mixes
    # the orient rows by the columns of the frame rotation
    var r := rvec
    var u := uvec
    var f := fvec
    rvec = _mxm_row(r, u, f, rot, 0)
    uvec = _mxm_row(r, u, f, rot, 1)
    fvec = _mxm_row(r, u, f, rot, 2)

    orthogonalize()

# one row of vm_matrix_x_matrix: dest.row[i].c = dot(src0 column c, src1.row[i])
static func _mxm_row(r: Vector3, u: Vector3, f: Vector3, rot: Array,
                     i: int) -> Vector3:
    return Vector3(
        rot[i].x * r.x + rot[i].y * u.x + rot[i].z * f.x,
        rot[i].x * r.y + rot[i].y * u.y + rot[i].z * f.y,
        rot[i].x * r.z + rot[i].y * u.z + rot[i].z * f.z)

# physics.cc physics_sim_vel, regular-damping path: ramp the LOCAL velocity
# (side-slip damping on x/y, none forward), move in world coords
func sim_vel(sim_time: float) -> void:
    var damp := Vector3(p["side_slip_time_const"],
                        p["side_slip_time_const"], 0.0)

    var local_v_in := vec_rotate(vel)
    var local_desired_vel := vec_rotate(desired_vel)

    var x := apply_physics(damp.x, local_desired_vel.x, local_v_in.x, sim_time)
    var y := apply_physics(damp.y, local_desired_vel.y, local_v_in.y, sim_time)
    var z := apply_physics(damp.z, local_desired_vel.z, local_v_in.z, sim_time)

    var local_disp := Vector3(x[1], y[1], z[1])
    pos += vec_unrotate(local_disp)
    vel = vec_unrotate(Vector3(x[0], y[0], z[0]))

# physics.cc physics_sim: velocity then rotation, then the derived speeds
func sim(sim_time: float) -> void:
    sim_vel(sim_time)
    sim_rot(sim_time)
    speed = vel.length()
    fspeed = fvec.dot(vel)
