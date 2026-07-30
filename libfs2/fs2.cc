#include "fs2.hh"
#include "version.hh"

#include <string.h>

const char *
fs2_t::version() const
{
    return LIBFS2_VERSION;
}

fs2_t::fs2_t()
{
    physics_init(&m_pi);
    m_pi.flags = PF_ACCELERATES;
}

void
fs2_t::fly_reset(const flight_params_t &params)
{
    physics_init(&m_pi);

    // the physics_dump recipe, verbatim -- PF_ACCELERATES only, the flag
    // pair the oracle pins; PF_AFTERBURNER_ON comes and goes per step
    m_pi.flags = PF_ACCELERATES;

    m_pi.mass = params.mass;
    m_pi.max_vel = params.max_vel;
    m_pi.max_rear_vel = params.max_rear_vel;
    m_pi.max_rotvel = params.max_rotvel;

    m_pi.forward_accel_time_const = params.forward_accel_time_const;
    m_pi.forward_decel_time_const = params.forward_decel_time_const;
    m_pi.slide_accel_time_const = 0.0f;
    m_pi.slide_decel_time_const = 0.0f;
    m_pi.side_slip_time_const = params.side_slip_time_const;
    m_pi.rotdamp = params.rotdamp;

    m_pi.afterburner_max_vel = params.afterburner_max_vel;
    m_pi.afterburner_forward_accel_time_const =
        params.afterburner_forward_accel_time_const;

    m_pos = vmd_zero_vector;
    m_orient = vmd_identity_matrix;

    m_has_afterburner = params.afterburner_max_vel.z > 0.0f;
}

void
fs2_t::fly_step(float dt, const flight_controls_t &controls)
{
    control_info ci;
    memset(&ci, 0, sizeof(ci));

    ci.pitch = controls.pitch;
    ci.heading = controls.heading;
    ci.bank = controls.bank;
    ci.forward = controls.forward;

    // the engage policy lives here; the burner's flight behavior (stick
    // floor, goal swap, accel-const swap) is retail's own physics.cc,
    // reading the flag
    if (controls.afterburner && m_has_afterburner)
        m_pi.flags |= PF_AFTERBURNER_ON;
    else
        m_pi.flags &= ~PF_AFTERBURNER_ON;

    physics_read_flying_controls(&m_orient, &m_pi, &ci, dt, NULL);
    physics_sim(&m_pos, &m_orient, &m_pi, dt);
}

flight_state_t
fs2_t::fly_state() const
{
    flight_state_t state;

    state.pos = m_pos;
    state.orient = m_orient;
    state.vel = m_pi.vel;
    state.rotvel = m_pi.rotvel;
    state.speed = m_pi.speed;
    state.fspeed = m_pi.fspeed;

    return state;
}
