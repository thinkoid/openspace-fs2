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
    bool target_hostile;               // edge -> the H binding's
                                       // hud_target_next_list (next
                                       // closest hostile)
    bool target_escort;                // edge -> the E binding's
                                       // hud_escort_target_next
    bool target_subsys;                // edge -> the S binding's
                                       // hud_target_next_subobject
    bool cycle_primary;                // edge -> "." (bank 1 -> bank 2
                                       // -> linked, retail's cycle)
    bool cycle_secondary;              // edge -> "/" (next missile bank)
    bool warp_out;                     // edge -> the jump key: retail's
                                       // END_MISSION gates (collision,
                                       // engines) then the staged
                                       // warpout; pressed again during
                                       // stage 1 = the abort (retail
                                       // spells that ESC)
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
// ships carry the full record; weapons, fireballs, debris and shockwaves
// carry the kinematic core (class name where they have one, radius always;
// a shockwave's radius is the LIVE blast front, not the object's static
// outer ceiling). Lasers add their art; missiles carry their POF instead.
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
    float max_speed;                   // ships: phys_info.max_vel.z --
                                       // match-speed's denominator

    float shield[4];                   // ships: retail's quadrants
                                       // (object.shields[]); a quadrant's
                                       // ceiling is shield_max / 4
                                       // (hudshield.cc:250)
    float shield_max;                  // ship_info.shields, the total

    float weapon_energy;               // ships: the EU reserve the primaries
    float weapon_energy_max;           // draw from, and its ceiling
    float burner_fuel;                 // afterburner fuel and capacity
    float burner_fuel_max;
    int species;                       // ships: 0 Terran / 1 Vasudan /
                                       // 2 Shivan -- picks the thruster
                                       // flipbook family (ship.cc:2996)
    char shield_icon[32];              // ships: the HUD shield ani
                                       // (ships.tbl $Shield_icon; "" =
                                       // none, retail's 255 sentinel)

    unsigned char laser_rgb[3];        // lasers: the current cycle color
                                       // (weapon_get_laser_color)
    float laser_length;                // lasers: the bolt's tbl size
    float laser_head_radius;
    char laser_bitmap[32];             // lasers: the body streak's stem
    char laser_glow[32];               // and the head glow's ("" = none)

    char piece[32];                    // debris: the submodel's name in
                                       // the pof -- a hull chunk wears
                                       // the ship's own model, small
                                       // debris wears debris01/02
};

// The per-frame kinematic core, packed for the hot crossing: parallel
// arrays, one row per live object -- sig[i] names the row, and identity
// crossed ONCE at birth (the created event carries the full record), so
// nothing static re-crosses at 60 Hz. flags bits: 1 dying,
// 2 afterburner, 4 player. shield is 4 floats per row (ships; zeros
// elsewhere), rgb 3 bytes per row (a laser's live cycle color), radius
// is the LIVE value (a shockwave's expanding blast front).
struct frame_t {
    std::vector<int> sig;
    std::vector<int> flags;
    std::vector<vector> pos, rvec, uvec, fvec, vel;
    std::vector<float> hull, radius;
    std::vector<float> shield;         // 4 per row
    std::vector<unsigned char> rgb;    // 3 per row
};

// The discontinuities between two events() drains: objects entering and
// leaving the world, and every new mission-log entry (retail's own record
// -- LOG_WAYPOINTS_DONE, LOG_SHIP_DESTROYED... the sexp predicates read
// the same table).
struct event_t {
    enum kind_t { created, destroyed, log, sound, message, hud_text };

    kind_t kind;
    int signature;                     // created/destroyed
    char name[32];                     // created/destroyed; sound: the wav
                                       // message: the voice wav ("" = none)

    object_state_t birth;              // created: the full identity record
                                       // -- the frame() rows carry only
                                       // what changes after this

    int log_type;                      // log: the LOG_* constant
    char pname[32], sname[32];         // log; message: pname = the sender
    fix time;                          // log: mission time of the entry

    char text[512];                    // message: the translated line
                                       // (retail's MESSAGE_LENGTH);
                                       // hud_text: the ticker line

    int source;                        // hud_text: HUD_SOURCE_* (color
                                       // class at the commit point)

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

// One sky element, as the mission authored it and retail's own structures
// hold it: a sun (drawn at its direction, lighting the world in its RGBI)
// or a background bitmap patch. orient is the instance's angle matrix --
// the element sits along its uvec (retail rotates (0,1,0) by it); scale
// is retail's angular scale (a sun's angular radius is 0.05 * scale_x,
// stars_draw_sun; a patch spans 10 degrees * scale, 3ddraw.cc's
// p_phi/p_theta). xparent tells the blend: stars.tbl $BitmapX green-key
// art draws with alpha, plain $Bitmap intensity art draws additive.
struct backdrop_t {
    bool sun;
    char name[32];
    char glow[32];                     // suns: the glow bitmap ("" = none)
    matrix orient;
    float scale_x, scale_y;
    int div_x, div_y;
    bool xparent;
    float r, g, b, i;                  // suns: the directional light
};

// One mission goal as missiongoals.cc holds it: FRED's name for it, the
// primary/secondary/bonus class, the GOAL_* status after the mission
// (0 failed / 1 complete / 2 incomplete), and the briefing-room text.
// invalid mirrors the INVALID_GOAL bit (a goal the mission withdrew).
struct goal_state_t {
    char name[32];                     // NAME_LENGTH
    char text[128];                    // MAX_GOAL_TEXT
    int type;                          // PRIMARY/SECONDARY/BONUS_GOAL
    int status;                        // GOAL_FAILED/COMPLETE/INCOMPLETE
    bool invalid;
};

// One debrief stage the formulas selected, in authored order: the
// FRED-authored paragraph, the recommendation shown on request, and the
// voice wave. Promotion/badge stages are presentation-fed (rank art,
// medal bitmaps) and stay out; so does the traitor debriefing.
struct debrief_stage_t {
    std::string text;
    std::string recommendation;
    char voice[32];
};

// The mission-end verdict -- what retail's debrief screen would show and
// what the campaign decided. next_mission is the branch formula's pick
// ("" outside a campaign, or when the campaign just completed);
// loop_offer flags the optional side-loop solicitation (accept(true)
// takes it), loop_desc its popup text.
struct debrief_t {
    std::vector<goal_state_t> goals;
    std::vector<debrief_stage_t> stages;
    char next_mission[32];
    bool loop_offer;
    std::string loop_desc;
    char loop_voice[32];               // the loop brief's voice wave
                                       // ("" = none -- retail's own
                                       // campaign authors no loop voice)
};

// The lesson's display half, value-only: what the training gauges would
// draw this frame. training_text is empty outside a message's window
// (retail's own text-length timing paces it headless -- the voice load
// fails without a device and message_training_setup falls back);
// training_voice names the wave so the presenter can play it.
// One weapon-gauge line: a mounted bank's tbl name and whether the next
// trigger pull fires it -- primaries arm the selected bank, or every
// bank when linked; secondaries arm the selected bank alone. shots is
// the missiles-per-pull count (2 under dual fire, from the SAME bank --
// ship_fire_secondary's two-slot arm); primaries always 1.
struct weapon_bank_t {
    char name[32];
    bool armed;
    int shots;
};

struct hud_state_t {
    char training_text[8192];
    char training_voice[64];
    std::vector<directive_t> directives;
    float primary_speed;               // the selected primary's muzzle
                                       // speed -- the lead indicator's
                                       // one input the scene lacks
    int target_signature;              // the player's current target
                                       // (Player_ai), -1 = none
    char target_subsys[32];            // the targeted subsystem on it
                                       // (system_info->name, "" = none)
    vector target_subsys_pos;          // its world position (FS2 frame;
                                       // valid when target_subsys set)
    float weapon_energy;               // the player's gun reserve and
    float weapon_energy_max;           // ceiling, afterburner fuel and
    float burner_fuel;                 // capacity -- HUD freight, so the
    float burner_fuel_max;             // packed frame() stays uniform
    std::vector<weapon_bank_t> primary_banks;
    std::vector<weapon_bank_t> secondary_banks;
    int warpout_stage;                 // 0 = flying normally; 1..3 =
                                       // retail's PCM_WARPOUT stages
                                       // (the autopilot owns the stick)
    bool departed;                     // the player's LOG_SHIP_DEPART is
                                       // on the mission log -- warped
                                       // out, the mission is over
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
    frame_t frame() const;             // the packed kinematic core --
                                       // snapshot() stays for the oracle
                                       // tools; the presenter reads this
    std::vector<event_t> events();     // drains
    hud_state_t hud_state() const;
    std::vector<backdrop_t> backdrop() const;   // static per mission
    int num_stars() const;             // the mission's $Num stars

    // mark a key used, by the mission's own name for it ("t", "M",
    // "Tab") -- the sexp key-pressed predicate reads the mark
    void key_mark(const char *key_text);

    // the campaign. load_campaign reads the .fc2 plus the pilot's .csg
    // resume and enters campaign mode: every load() after it plays under
    // GM_CAMPAIGN_MODE, and current_mission() names what to fly next
    // ("" = the campaign is complete). debrief() is retail's
    // debrief-entry sequence, sim half only -- fail incomplete goals,
    // store the goal/event record, evaluate the branch formula and the
    // stage formulas -- called once, while the world is still live.
    // accept() is the debrief screen's Accept: commits the mission
    // (grants, completion mark, the .csg save) and advances the
    // campaign; take_loop steers into the offered side loop.
    bool load_campaign(const char *game_root, const char *name);
    const char *current_mission() const;
    debrief_t debrief();
    void accept(bool take_loop = false);

private:
    physics_info m_pi;
    vector m_pos = vmd_zero_vector;
    matrix m_orient = vmd_identity_matrix;
    bool m_has_afterburner = false;

    bool m_world_live = false;
    bool m_campaign = false;           // load_campaign() succeeded
    bool m_campaign_over = false;      // accept() hit next_mission == -1
    bool m_pre_entry = true;           // freespace.cc's Pre_player_entry
                                       // (no header ever declared it; the
                                       // boundary owns the replica)
    bool m_burn_held = false;          // afterburner edge detection
    bool m_target_held = false;        // target-next edge detection
    bool m_hostile_held = false;       // target-hostile edge detection
    bool m_escort_held = false;        // target-escort edge detection
    bool m_subsys_held = false;        // target-subsystem edge detection
    bool m_cycle_p_held = false;       // primary-cycle edge detection
    bool m_cycle_s_held = false;       // secondary-cycle edge detection
    bool m_warp_held = false;          // jump-key edge detection
    int m_log_drained = 0;             // mission-log high-water mark

    // the last drain's membership, signature + the name captured at
    // birth (the destroyed event's only payload) -- the full records
    // stopped being retained when the diff went signatures-only
    struct known_t {
        int signature;
        char name[32];
    };
    std::vector<known_t> m_known;

public:
    fs2_t();
};
