// The FS2 shim: the one translation unit that includes both a Godot header
// and fs2.hh. It registers class FS2 with ClassDB and forwards its calls to
// the held fs2_t, marshalling to Variant types -- pure translation, no
// logic. GDScript reads `var sim := FS2.new()`; the sim side never learns
// Godot exists.
//
// Include order is load-bearing: fs2.hh drags in retail's pstypes.hh,
// whose PI macro and min/max templates would mangle the Godot headers --
// Godot first, retail last.

#include <gdextension_interface.h>

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "fs2.hh"

// FS2 <-> retail frame plumbing: same axes, different containers
static vector
to_vec(const godot::Vector3 &v)
{
    vector out;
    out.x = v.x;
    out.y = v.y;
    out.z = v.z;
    return out;
}

static godot::Vector3
from_vec(const vector &v)
{
    return godot::Vector3(v.x, v.y, v.z);
}

class FS2 : public godot::RefCounted {
    GDCLASS(FS2, godot::RefCounted)

    fs2_t m_sim;

protected:
    static void _bind_methods()
    {
        godot::ClassDB::bind_method(godot::D_METHOD("version"),
                                    &FS2::version);
        godot::ClassDB::bind_method(godot::D_METHOD("fly_reset", "params"),
                                    &FS2::fly_reset);
        godot::ClassDB::bind_method(godot::D_METHOD("fly_step", "dt",
                                                    "controls"),
                                    &FS2::fly_step);
        godot::ClassDB::bind_method(godot::D_METHOD("fly_state"),
                                    &FS2::fly_state);
    }

public:
    godot::String version() const { return m_sim.version(); }

    // params carries flight_model.gd's p-dict keys (the ships.tbl
    // physics_ship_init subset); the afterburner pair is optional
    void fly_reset(const godot::Dictionary &params)
    {
        flight_params_t p;

        p.mass = params["mass"];
        p.max_vel = to_vec(params["max_vel"]);
        p.max_rear_vel = params["max_rear_vel"];
        p.max_rotvel = to_vec(params["max_rotvel"]);

        p.forward_accel_time_const = params["forward_accel_time_const"];
        p.forward_decel_time_const = params["forward_decel_time_const"];
        p.side_slip_time_const = params["side_slip_time_const"];
        p.rotdamp = params["rotdamp"];

        p.afterburner_max_vel = vmd_zero_vector;
        p.afterburner_forward_accel_time_const = 0.0f;
        if (params.has("afterburner_max_vel")) {
            p.afterburner_max_vel = to_vec(params["afterburner_max_vel"]);
            p.afterburner_forward_accel_time_const =
                params["afterburner_forward_accel_time_const"];
        }

        m_sim.fly_reset(p);
    }

    void fly_step(double dt, const godot::Dictionary &controls)
    {
        flight_controls_t c;

        c.pitch = controls.get("pitch", 0.0);
        c.heading = controls.get("heading", 0.0);
        c.bank = controls.get("bank", 0.0);
        c.forward = controls.get("forward", 0.0);
        c.afterburner = controls.get("afterburner", false);

        m_sim.fly_step(float(dt), c);
    }

    godot::Dictionary fly_state() const
    {
        flight_state_t s = m_sim.fly_state();
        godot::Dictionary out;

        out["pos"] = from_vec(s.pos);
        out["rvec"] = from_vec(s.orient.rvec);
        out["uvec"] = from_vec(s.orient.uvec);
        out["fvec"] = from_vec(s.orient.fvec);
        out["vel"] = from_vec(s.vel);
        out["rotvel"] = from_vec(s.rotvel);
        out["speed"] = s.speed;
        out["fspeed"] = s.fspeed;

        return out;
    }
};

static void
initialize_fs2_module(godot::ModuleInitializationLevel level)
{
    if (level != godot::MODULE_INITIALIZATION_LEVEL_SCENE)
        return;

    GDREGISTER_CLASS(FS2);
}

static void
uninitialize_fs2_module(godot::ModuleInitializationLevel level)
{
}

extern "C" GDExtensionBool GDE_EXPORT
fs2_library_init(GDExtensionInterfaceGetProcAddress get_proc_address,
                 GDExtensionClassLibraryPtr library,
                 GDExtensionInitialization *initialization)
{
    godot::GDExtensionBinding::InitObject init_obj(get_proc_address, library,
                                                   initialization);

    init_obj.register_initializer(initialize_fs2_module);
    init_obj.register_terminator(uninitialize_fs2_module);
    init_obj.set_minimum_library_initialization_level(
        godot::MODULE_INITIALIZATION_LEVEL_SCENE);

    return init_obj.init();
}
