// physics_dump is the flight-model oracle: retail's physics_sim +
// physics_read_flying_controls run over a fixed, scripted control sequence,
// printing the full state trace. The Godot flight model
// (inspect/flight_model.gd) replays the same sequence and flight-check
// compares the trajectories -- so the flight *feel* of the port is retail's
// own integrator, not an approximation judged by eye. On the road to the
// first training mission: this pins the physics; ships.tbl parameters and
// the mission itself come as their own slices.
//
//   physics_dump > trace.txt
//
// Per frame: the control inputs consumed, then position, orientation (nine
// floats, rvec/uvec/fvec), velocity -- all %.9g so every float32 round-trips
// exactly.
//
// The parameters are a SYNTHETIC fighter -- fixed here and mirrored in
// flight_model.gd's FIGHTER constants (the contract is the pair; change one,
// change both). They are plausible FS2-fighter numbers, NOT ships.tbl data:
// table parsing is a later slice, and this oracle pins the integrator, not
// the data source. Flags: PF_ACCELERATES only -- no slide, no afterburner,
// no reduced-damp/shockwave/warp specials; the GDScript port implements
// exactly this subset and says so.

#include <stdio.h>
#include <string.h>

#include <globalincs/pstypes.hh>
#include <math/vecmat.hh>
#include <physics/physics.hh>

// the scripted flight: throttle up, pitch pulse, a banked turn (exercises
// BANK_WHEN_TURN's coupled delta_bank), manual bank, reverse, coast
struct phase_t
{
    int frames;
    float pitch, heading, bank, forward;
};

static const phase_t script[] = {
    { 60, 0.0f, 0.0f, 0.0f, 0.0f },     // rest
    { 90, 0.0f, 0.0f, 0.0f, 1.0f },     // full throttle ramp
    { 60, 0.5f, 0.0f, 0.0f, 1.0f },     // pitch up
    { 90, 0.0f, -0.8f, 0.0f, 1.0f },    // hard left turn (auto-bank)
    { 60, 0.0f, 0.0f, 1.0f, 0.7f },     // manual roll
    { 60, -0.3f, 0.4f, -0.2f, 0.4f },   // combined
    { 60, 0.0f, 0.0f, 0.0f, -1.0f },    // full reverse
    { 60, 0.0f, 0.0f, 0.0f, 0.0f },     // coast down
};

int
main()
{
    physics_info pi;
    physics_init(&pi);

    // the synthetic fighter (mirrored in flight_model.gd FIGHTER)
    pi.flags = PF_ACCELERATES;
    pi.mass = 12.0f;
    pi.max_vel.x = 0.0f;   // FS2 fighters do not strafe
    pi.max_vel.y = 0.0f;
    pi.max_vel.z = 65.0f;
    pi.max_rear_vel = 15.0f;
    pi.max_rotvel.x = 1.75f;  // pitch
    pi.max_rotvel.y = 1.4f;   // heading
    pi.max_rotvel.z = 2.75f;  // bank
    pi.forward_accel_time_const = 3.0f;
    pi.forward_decel_time_const = 2.0f;
    pi.slide_accel_time_const = 0.0f;
    pi.slide_decel_time_const = 0.0f;
    pi.side_slip_time_const = 0.05f;
    pi.rotdamp = 0.35f;

    vector pos = ZERO_VECTOR;
    matrix orient = IDENTITY_MATRIX;

    const float dt = 1.0f / 60.0f;

    int frame = 0;
    for (const phase_t &ph : script) {
        for (int i = 0; i < ph.frames; ++i, ++frame) {
            control_info ci;
            memset(&ci, 0, sizeof(ci));
            ci.pitch = ph.pitch;
            ci.heading = ph.heading;
            ci.bank = ph.bank;
            ci.forward = ph.forward;

            physics_read_flying_controls(&orient, &pi, &ci, dt, NULL);
            physics_sim(&pos, &orient, &pi, dt);

            printf("%d ci %.9g %.9g %.9g %.9g "
                   "pos %.9g %.9g %.9g "
                   "orient %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g "
                   "vel %.9g %.9g %.9g\n",
                   frame, ph.pitch, ph.heading, ph.bank, ph.forward,
                   pos.x, pos.y, pos.z,
                   orient.rvec.x, orient.rvec.y, orient.rvec.z,
                   orient.uvec.x, orient.uvec.y, orient.uvec.z,
                   orient.fvec.x, orient.fvec.y, orient.fvec.z,
                   pi.vel.x, pi.vel.y, pi.vel.z);
        }
    }

    return 0;
}
