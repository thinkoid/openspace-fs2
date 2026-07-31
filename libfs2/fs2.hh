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

#include <string>
#include <vector>

#include <globalincs/pstypes.hh>
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
// The triggers land in retail's own control_info counts --
// obj_player_fire_stuff (object.cc:694) does the firing from there.
struct flight_controls_t {
    float pitch, heading, bank;
    float forward;
    bool afterburner;
    bool fire_primary;
    bool fire_secondary;
    bool fire_countermeasure;
    bool target_next;                  // edge -> retail's hud_target_next
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

// One mission object, value-only, as the snapshot reports it. The signature
// is retail's own stable id (object.signature, minted to outlive objnum
// reuse) -- the reconciler keys scene nodes by it. type is retail's OBJ_*:
// ships carry the full record; weapons, fireballs and debris carry the
// kinematic core (class name where they have one, radius always).
struct object_state_t {
    int signature;
    int objnum;
    int type;                          // OBJ_SHIP / OBJ_WEAPON / ...
    float radius;
    char name[32];                     // Ships[].ship_name
    char class_name[32];               // Ship_info[].name
    char pof[32];                      // Ship_info[].pof_file
    int team;
    int arrival_location;              // ARRIVE_* -- nonzero means the game
                                       // relocated the authored position
    bool player;
    bool dying;
    bool afterburner;                  // PF_AFTERBURNER_ON, the sim's truth

    vector pos;
    matrix orient;
    vector vel;
    float hull;                        // current hull strength
    float hull_max;
};

// The discontinuities between two events() drains: objects entering and
// leaving the world, and every new mission-log entry (retail's own record
// -- LOG_WAYPOINTS_DONE, LOG_SHIP_DESTROYED... the sexp predicates read
// the same table).
struct event_t {
    enum kind_t { created, destroyed, log, sound, message };

    kind_t kind;
    int signature;                     // created/destroyed
    char name[32];                     // created/destroyed; sound: the wav
                                       // message: the voice wav ("" = none)

    int log_type;                      // log: the LOG_* constant
    char pname[32], sname[32];         // log; message: pname = the sender
    fix time;                          // log: mission time of the entry

    char text[512];                    // message: the translated line
                                       // (retail's MESSAGE_LENGTH)

    bool has_pos;                      // sound: 3d, at pos
    vector pos;
};

// One directives-gauge line, decoded exactly as training_obj_display
// renders it (missiontraining.cc:198): the event's objective text (with
// retail's [count] suffix), its EVENT_* status, and whether this is a
// bright-green key line (objective_key_text through the token remap).
struct directive_t {
    char text[256];
    int state;                         // 0 unborn / EVENT_CURRENT /
                                       // EVENT_SATISFIED / EVENT_FAILED
    bool key_line;
};

// The lesson's display half, value-only: what the training gauges would
// draw this frame. training_text is empty outside a message's window
// (retail's own text-length timing paces it headless -- the voice load
// fails without a device and message_training_setup falls back);
// training_voice names the wave so the presenter can play it.
struct hud_state_t {
    char training_text[8192];
    char training_voice[64];
    std::vector<directive_t> directives;
    int target_signature;              // the player's current target
                                       // (Player_ai), -1 = none
};

// The boundary object. Slice 1 grew the single-ship flight surface
// (fly_*, the tests/physics_dump.cc call pair); slice 2 grows the WORLD:
// load runs retail's game-path mission chain (arrival cues live), step is
// the headless twin of freespace.cc's frame (game_simulation_frame's sim
// subset -- freespace.cc itself is the game's entry point and stays out
// of the library), snapshot/events are the value-only state out.
struct fs2_t {
    const char *version() const;

    // one ship, no world -- the fly.tscn scene and the flight oracle
    void fly_reset(const flight_params_t &params);
    void fly_step(float dt, const flight_controls_t &controls);
    flight_state_t fly_state() const;

    // the mission world. load boots cfile + tables once per process
    // (game_root = the unpacked install), then runs the level chain for
    // the mission; seed pins the rand stream (retail's own
    // game_level_init(seed) parameter). Returns false on parse failure.
    bool load(const char *game_root, const char *mission, int seed);
    void step(float dt, const flight_controls_t &controls);
    std::vector<object_state_t> snapshot() const;
    std::vector<event_t> events();     // drains
    hud_state_t hud_state() const;

    // mark a key used, by the mission's own name for it ("t", "M",
    // "Tab") -- the sexp key-pressed predicate reads the mark
    void key_mark(const char *key_text);

private:
    physics_info m_pi;
    vector m_pos = vmd_zero_vector;
    matrix m_orient = vmd_identity_matrix;
    bool m_has_afterburner = false;

    bool m_world_live = false;
    bool m_pre_entry = true;           // freespace.cc's Pre_player_entry
                                       // (no header ever declared it; the
                                       // boundary owns the replica)
    bool m_burn_held = false;          // afterburner edge detection
    bool m_target_held = false;        // target-next edge detection
    int m_log_drained = 0;             // mission-log high-water mark
    std::vector<object_state_t> m_known; // last drain's world, for diffs

public:
    fs2_t();
};
