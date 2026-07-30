// fs2_t is the boundary object of libfs2: the retail simulation behind a
// narrow, engine-agnostic API (docs/godot-migration-plan.md, "Second step").
// No Godot header may appear here or in fs2.cc -- the Godot marshalling
// lives in extension.cc, the one file that speaks both languages. Oracle
// tools (sim_dump) link fs2_t directly, engine absent.
//
// Everything crosses the boundary in FS2's own frame and units; the
// (x, y, -z) visual map stays presentation-side, exactly where the GDScript
// era put it.

#pragma once

#include <math/vecmat.hh>
#include <physics/physics.hh>

// A ship's flight parameters -- the physics_ship_init subset of ships.tbl,
// the same fields flight_model.gd carried. Zero afterburner_max_vel means
// no afterburner.
struct flight_params_t {
    float mass;
    vector max_vel;                    // x,y lateral (slide) -- fighters: 0
    float max_rear_vel;
    vector max_rotvel;                 // pitch, heading, bank caps

    float forward_accel_time_const;
    float forward_decel_time_const;
    float side_slip_time_const;
    float rotdamp;

    vector afterburner_max_vel;
    float afterburner_forward_accel_time_const;
};

// Stick deflections in [-1, 1]; forward is the throttle (negative =
// reverse). The afterburner request engages only if the params carry one.
struct flight_controls_t {
    float pitch, heading, bank;
    float forward;
    bool afterburner;
};

// The flying state after a step -- physics_info's living fields.
struct flight_state_t {
    vector pos;
    matrix orient;                     // rvec/uvec/fvec
    vector vel;                        // world frame
    vector rotvel;                     // local pitch/heading/bank rates
    float speed;
    float fspeed;                      // signed, forward direction
};

// The boundary object. Slice 1: one ship's flight -- retail's
// physics_read_flying_controls + physics_sim over a single physics_info,
// the same call pair tests/physics_dump.cc pins. The mission world (load /
// step / snapshot / events) arrives in slice 2.
struct fs2_t {
    const char *version() const;

    void fly_reset(const flight_params_t &params);
    void fly_step(float dt, const flight_controls_t &controls);
    flight_state_t fly_state() const;

private:
    physics_info m_pi;
    vector m_pos = vmd_zero_vector;
    matrix m_orient = vmd_identity_matrix;
    bool m_has_afterburner = false;

public:
    fs2_t();
};
