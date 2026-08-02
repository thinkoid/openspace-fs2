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

#include <globalincs/pstypes.hh>

#include <gamesnd/gamesnd.hh>
#include <object/object.hh>

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

static flight_controls_t
controls_of(const godot::Dictionary &controls)
{
    flight_controls_t c;

    c.pitch = controls.get("pitch", 0.0);
    c.heading = controls.get("heading", 0.0);
    c.bank = controls.get("bank", 0.0);
    c.forward = controls.get("forward", 0.0);
    c.afterburner = controls.get("afterburner", false);
    c.fire_primary = controls.get("fire_primary", false);
    c.fire_secondary = controls.get("fire_secondary", false);
    c.fire_countermeasure = controls.get("fire_countermeasure", false);
    c.target_next = controls.get("target_next", false);
    c.target_hostile = controls.get("target_hostile", false);
    c.target_escort = controls.get("target_escort", false);
    c.target_subsys = controls.get("target_subsys", false);
    c.cycle_primary = controls.get("cycle_primary", false);
    c.cycle_secondary = controls.get("cycle_secondary", false);
    c.warp_out = controls.get("warp_out", false);

    return c;
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
        godot::ClassDB::bind_method(godot::D_METHOD("load", "game_root",
                                                    "mission", "seed"),
                                    &FS2::load);
        godot::ClassDB::bind_method(godot::D_METHOD("step", "dt", "controls"),
                                    &FS2::step);
        godot::ClassDB::bind_method(godot::D_METHOD("frame"), &FS2::frame);
        godot::ClassDB::bind_method(godot::D_METHOD("snapshot"),
                                    &FS2::snapshot);
        godot::ClassDB::bind_method(godot::D_METHOD("events"), &FS2::events);
        godot::ClassDB::bind_method(godot::D_METHOD("backdrop"),
                                    &FS2::backdrop);
        godot::ClassDB::bind_method(godot::D_METHOD("num_stars"),
                                    &FS2::num_stars);
        godot::ClassDB::bind_method(godot::D_METHOD("hud_state"),
                                    &FS2::hud_state);
        godot::ClassDB::bind_method(godot::D_METHOD("load_campaign",
                                                    "game_root", "name"),
                                    &FS2::load_campaign);
        godot::ClassDB::bind_method(godot::D_METHOD("current_mission"),
                                    &FS2::current_mission);
        godot::ClassDB::bind_method(godot::D_METHOD("debrief"),
                                    &FS2::debrief);
        godot::ClassDB::bind_method(godot::D_METHOD("accept", "take_loop"),
                                    &FS2::accept);
        godot::ClassDB::bind_method(godot::D_METHOD("key_mark", "key_text"),
                                    &FS2::key_mark);
        godot::ClassDB::bind_method(godot::D_METHOD("sound_name", "id"),
                                    &FS2::sound_name);
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
        m_sim.fly_step(float(dt), controls_of(controls));
    }

    // the mission world (slice 2): value-only marshalling, FS2-frame
    bool load(const godot::String &game_root, const godot::String &mission,
              int seed)
    {
        return m_sim.load(game_root.utf8().get_data(),
                          mission.utf8().get_data(), seed);
    }

    void step(double dt, const godot::Dictionary &controls)
    {
        m_sim.step(float(dt), controls_of(controls));
    }

    // the campaign (slice 3): load_campaign + current_mission bracket
    // the mission loads; debrief/accept mirror the debrief screen's
    // lifecycle -- the verdict crosses as one dictionary
    bool load_campaign(const godot::String &game_root,
                       const godot::String &name)
    {
        return m_sim.load_campaign(game_root.utf8().get_data(),
                                   name.utf8().get_data());
    }

    godot::String current_mission() const
    {
        return m_sim.current_mission();
    }

    godot::Dictionary debrief()
    {
        debrief_t d = m_sim.debrief();

        godot::Array goals;
        for (const goal_state_t &g : d.goals) {
            godot::Dictionary e;
            e["name"] = g.name;
            e["text"] = g.text;
            e["type"] = g.type;
            e["status"] = g.status;
            e["invalid"] = g.invalid;
            goals.push_back(e);
        }

        godot::Array stages;
        for (const debrief_stage_t &s : d.stages) {
            godot::Dictionary e;
            e["text"] = s.text.c_str();
            e["recommendation"] = s.recommendation.c_str();
            e["voice"] = s.voice;
            stages.push_back(e);
        }

        godot::Dictionary out;
        out["goals"] = goals;
        out["stages"] = stages;
        out["next_mission"] = d.next_mission;
        out["loop_offer"] = d.loop_offer;
        out["loop_desc"] = d.loop_desc.c_str();
        out["loop_voice"] = d.loop_voice;
        return out;
    }

    void accept(bool take_loop)
    {
        m_sim.accept(take_loop);
    }

    // one record as a dictionary -- snapshot()'s rows, and the created
    // event's birth payload (identity crosses once, there)
    static godot::Dictionary rec_dict(const object_state_t &o)
    {
        godot::Dictionary d;
        d["signature"] = o.signature;
            switch (o.type) {
            case OBJ_WEAPON:    d["type"] = "weapon"; break;
            case OBJ_FIREBALL:  d["type"] = "fireball"; break;
            case OBJ_DEBRIS:    d["type"] = "debris"; break;
            case OBJ_SHOCKWAVE: d["type"] = "shockwave"; break;
            default:            d["type"] = "ship"; break;
            }
            d["radius"] = o.radius;
            d["name"] = o.name;
            d["class"] = o.class_name;
            d["pof"] = o.pof;
            d["team"] = o.team;
            d["player"] = o.player;
            d["dying"] = o.dying;
            d["afterburner"] = o.afterburner;
            d["pos"] = from_vec(o.pos);
            d["rvec"] = from_vec(o.orient.rvec);
            d["uvec"] = from_vec(o.orient.uvec);
            d["fvec"] = from_vec(o.orient.fvec);
            d["vel"] = from_vec(o.vel);
            d["hull"] = o.hull;
            d["hull_max"] = o.hull_max;
            d["max_speed"] = o.max_speed;

            godot::Array shield;
            for (int i = 0; i < 4; i++)
                shield.push_back(o.shield[i]);
            d["shield"] = shield;
            d["shield_max"] = o.shield_max;
            d["weapon_energy"] = o.weapon_energy;
            d["weapon_energy_max"] = o.weapon_energy_max;
            d["burner_fuel"] = o.burner_fuel;
            d["burner_fuel_max"] = o.burner_fuel_max;
            d["species"] = o.species;
            d["shield_icon"] = o.shield_icon;

            // the bolt's art, normalized to Godot's color space
            d["color"] = godot::Color(o.laser_rgb[0] / 255.0f,
                                      o.laser_rgb[1] / 255.0f,
                                      o.laser_rgb[2] / 255.0f);
        d["laser_length"] = o.laser_length;
        d["laser_radius"] = o.laser_head_radius;
        d["laser_bitmap"] = o.laser_bitmap;
        d["laser_glow"] = o.laser_glow;
        d["piece"] = o.piece;

        return d;
    }

    godot::Array snapshot() const
    {
        godot::Array out;

        for (const object_state_t &o : m_sim.snapshot())
            out.push_back(rec_dict(o));

        return out;
    }

    // the packed kinematic core: a handful of Packed*Array crossings per
    // frame instead of a dictionary per object -- retail's `vector` and
    // godot::Vector3 are both three floats, so the copies are straight
    // memcpy. Rows join to birth identity by "sig".
    godot::Dictionary frame() const
    {
        frame_t f = m_sim.frame();
        int n = int(f.sig.size());

        godot::Dictionary out;
        out["n"] = n;

        godot::PackedInt32Array sig;
        sig.resize(n);
        if (n)
            memcpy(sig.ptrw(), f.sig.data(), n * sizeof(int32_t));
        out["sig"] = sig;

        godot::PackedInt32Array flags;
        flags.resize(n);
        if (n)
            memcpy(flags.ptrw(), f.flags.data(), n * sizeof(int32_t));
        out["flags"] = flags;

        auto vecs = [n](const std::vector<vector> &v) {
            godot::PackedVector3Array a;
            a.resize(n);
            if (n)
                memcpy(a.ptrw(), v.data(), n * sizeof(vector));
            return a;
        };
        out["pos"] = vecs(f.pos);
        out["rvec"] = vecs(f.rvec);
        out["uvec"] = vecs(f.uvec);
        out["fvec"] = vecs(f.fvec);
        out["vel"] = vecs(f.vel);

        auto floats = [](const std::vector<float> &v) {
            godot::PackedFloat32Array a;
            a.resize(int(v.size()));
            if (!v.empty())
                memcpy(a.ptrw(), v.data(), v.size() * sizeof(float));
            return a;
        };
        out["hull"] = floats(f.hull);
        out["radius"] = floats(f.radius);
        out["shield"] = floats(f.shield);      // 4 per row

        godot::PackedByteArray rgb;            // 3 per row
        rgb.resize(int(f.rgb.size()));
        if (!f.rgb.empty())
            memcpy(rgb.ptrw(), f.rgb.data(), f.rgb.size());
        out["rgb"] = rgb;

        return out;
    }

    godot::Array events()
    {
        godot::Array out;

        for (const event_t &ev : m_sim.events()) {
            godot::Dictionary d;
            switch (ev.kind) {
            case event_t::created:
                d["kind"] = "created";
                d["signature"] = ev.signature;
                d["name"] = ev.name;
                d["rec"] = rec_dict(ev.birth);   // identity, once
                break;
            case event_t::destroyed:
                d["kind"] = "destroyed";
                d["signature"] = ev.signature;
                d["name"] = ev.name;
                break;
            case event_t::log:
                d["kind"] = "log";
                d["log_type"] = ev.log_type;
                d["pname"] = ev.pname;
                d["sname"] = ev.sname;
                d["time"] = double(ev.time) / 65536.0;
                break;
            case event_t::sound:
                d["kind"] = "sound";
                d["name"] = ev.name;
                if (ev.has_pos)
                    d["pos"] = from_vec(ev.pos);
                break;
            case event_t::message:
                d["kind"] = "message";
                d["who"] = ev.pname;
                d["text"] = ev.text;
                d["wave"] = ev.name;
                break;
            case event_t::hud_text:
                d["kind"] = "hud_text";
                d["text"] = ev.text;
                d["source"] = ev.source;
                break;
            }
            out.push_back(d);
        }

        return out;
    }

    // the mission's authored sky, static per load
    godot::Array backdrop() const
    {
        godot::Array out;

        for (const backdrop_t &d : m_sim.backdrop()) {
            godot::Dictionary e;
            e["sun"] = d.sun;
            e["name"] = d.name;
            e["glow"] = d.glow;
            e["rvec"] = from_vec(d.orient.rvec);
            e["uvec"] = from_vec(d.orient.uvec);
            e["fvec"] = from_vec(d.orient.fvec);
            e["scale_x"] = d.scale_x;
            e["scale_y"] = d.scale_y;
            e["xparent"] = d.xparent;
            e["color"] = godot::Color(d.r, d.g, d.b);
            e["intensity"] = d.i;
            out.push_back(e);
        }

        return out;
    }

    int num_stars() const { return m_sim.num_stars(); }

    godot::Dictionary hud_state() const
    {
        hud_state_t h = m_sim.hud_state();
        godot::Dictionary out;

        out["training_text"] = h.training_text;
        out["training_voice"] = h.training_voice;
        out["primary_speed"] = h.primary_speed;
        out["target_signature"] = h.target_signature;
        out["target_subsys"] = h.target_subsys;
        out["target_subsys_pos"] =
            godot::Vector3(h.target_subsys_pos.x, h.target_subsys_pos.y,
                           h.target_subsys_pos.z);
        out["weapon_energy"] = h.weapon_energy;
        out["weapon_energy_max"] = h.weapon_energy_max;
        out["burner_fuel"] = h.burner_fuel;
        out["burner_fuel_max"] = h.burner_fuel_max;

        godot::Array pb, sb;
        for (const weapon_bank_t &b : h.primary_banks) {
            godot::Dictionary line;
            line["name"] = b.name;
            line["armed"] = b.armed;
            line["shots"] = b.shots;
            pb.push_back(line);
        }
        for (const weapon_bank_t &b : h.secondary_banks) {
            godot::Dictionary line;
            line["name"] = b.name;
            line["armed"] = b.armed;
            line["shots"] = b.shots;
            sb.push_back(line);
        }
        out["primary_banks"] = pb;
        out["secondary_banks"] = sb;

        out["warpout_stage"] = h.warpout_stage;
        out["departed"] = h.departed;

        godot::Array dirs;
        for (const directive_t &d : h.directives) {
            godot::Dictionary line;
            line["text"] = d.text;
            line["state"] = d.state;
            line["key"] = d.key_line;
            dirs.push_back(line);
        }
        out["directives"] = dirs;

        return out;
    }

    void key_mark(const godot::String &key_text)
    {
        m_sim.key_mark(key_text.utf8().get_data());
    }

    // a game sound's wav by Snds[] id (gamesnd.hh's SND_*) -- presentation
    // runs its own loops (engine hum) from the same tables
    godot::String sound_name(int id) const
    {
        if (id < 0 || id >= MAX_GAME_SOUNDS)
            return "";
        return Snds[id].filename;
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
