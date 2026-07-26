// -*- mode: c++; -*-
//
// mission2tres: stage a mission's object layout out of a retail .fs2 file
// through retail's OWN mission parser -- the authoritative-reader pattern
// once more (docs/godot-migration-plan.md). parse_main runs under
// Fred_running, the editor's mode where EVERY mission object is created
// regardless of arrival cues (missionparse.cc:1856) and player starts
// become OBJ_START objects -- so the emitted layout is the mission as FRED
// shows it: all ships, placed. Arrival/departure semantics (which ships
// exist at t=0, when the rest warp in) belong to the events slice.
//
//   mission2tres <game-root> <mission.fs2> <out.tres>
//
// The mission file is looked up through cfile's CF_TYPE_MISSIONS, so both
// packed and loose missions resolve. Per object: name, ship class, POF
// stem (how the scene finds the converted GLB), team, position and
// orientation in FS2's own frame (the scene maps at the visual boundary),
// the player-start flag, and the ship's initial AI orders. After the
// ships: the event list (formulas as canonical one-line sexp text), the
// mission goals, the messages, and the waypoint lists -- the evaluator's
// entire diet. Arrival/departure cues are NOT extracted yet (under
// Fred_running every ship exists at t=0); that refinement belongs with
// wings. Nothing is stamped with a time.

#include <stdio.h>
#include <stdlib.h>

#include <string>

#include <globalincs/pstypes.hh>
#include <cfile/cfile.hh>
#include <graphics/2d.hh>
#include <io/timer.hh>
#include <localization/localize.hh>
#include <mission/missionbriefcommon.hh>
#include <mission/missiongoals.hh>
#include <mission/missionmessage.hh>
#include <mission/missionparse.hh>
#include <parse/sexp.hh>
#include <ship/ai.hh>
#include <ship/aigoals.hh>
#include <object/object.hh>
#include <ship/ship.hh>
#include <weapon/weapon.hh>

// table parsing stores laser and IFF colors through gr_screen's function
// pointers; no renderer runs here (see shiptbl2tres.cc)
static void
null_init_color(color *, int, int, int)
{
}

static void
null_init_alphacolor(color *, int, int, int, int, int)
{
}

// a string escaped for embedding in a .tres double-quoted literal --
// message texts carry newlines (F_MULTITEXT) and may carry quotes. The
// .tres is UTF-8; retail text is cp1252-ish passed through raw, so high
// bytes transcode here (corpus census: exactly ONE such byte in the whole
// install, a 0x92 curly apostrophe in SM2-06 -- cp1252 punctuation gets
// its real codepoint, anything else the latin-1 one).
static std::string
esc(const char *s)
{
    std::string out;
    for (; s && *s; ++s) {
        const unsigned char c = *s;
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"':  out += "\\\""; break;
        case '\n': out += "\\n";  break;
        case '\r':                break;
        default:
            if (c == 0x92) {
                out += "\xe2\x80\x99";        // U+2019
            }
            else if (c >= 0x80) {             // latin-1 codepoint, 2-byte UTF-8
                out += (char)(0xc0 | (c >> 6));
                out += (char)(0x80 | (c & 0x3f));
            }
            else {
                out += (char)c;
            }
            break;
        }
    }
    return out;
}

// compact canonical text of a parsed sexp tree -- one line, single-space
// separated, strings quoted, so the oracle can diff it against its own
// normalization of the mission text. Own walker rather than FRED's
// convert_sexp_to_string (global buffer, 40-column pretty-print reflow);
// the quoting semantics mirror build_sexp_text_string's SEXP_SAVE_MODE:
// SEXP_ATOM_STRING atoms quoted, variable atoms as @name[value].
static void
sexp_text(int node, std::string &out)
{
    out += '(';

    for (bool sp = false; node != -1; node = Sexp_nodes[node].rest, sp = true) {
        if (sp)
            out += ' ';

        if (Sexp_nodes[node].first != -1) {
            sexp_text(Sexp_nodes[node].first, out);
            continue;
        }

        if (Sexp_nodes[node].type & SEXP_FLAG_VARIABLE) {
            const int v = get_index_sexp_variable_name(Sexp_nodes[node].text);
            const bool str = Sexp_nodes[node].subtype == SEXP_ATOM_STRING;
            if (str)
                out += '"';
            out += '@';
            out += Sexp_nodes[node].text;
            out += '[';
            out += v >= 0 ? Sexp_variables[v].text : "?";
            out += ']';
            if (str)
                out += '"';
            continue;
        }

        if (Sexp_nodes[node].subtype == SEXP_ATOM_STRING) {
            out += '"';
            out += Sexp_nodes[node].text;
            out += '"';
            continue;
        }

        out += Sexp_nodes[node].text;
    }

    out += ')';
}

// a formula index is the head ELEMENT of the top-level list (get_sexp_main
// returns get_sexp()'s start node, past the opening paren) -- serialize it
// as the list it heads, no unwrapping
static std::string
sexp_str(int formula)
{
    std::string s;
    if (formula < 0)
        return "()";
    sexp_text(formula, s);
    return s;
}

// initial AI orders in retail's decoded form (ai_add_goal_sub_sexp,
// aigoals.cc): mode = AI_GOAL_* bit, ship_name carries every target,
// waypoint path names included. ai_clear_ship_goals only resets ai_mode,
// so ship_name/ai_submode hold STALE data for ops that don't write them
// (play-dead, chase-any, undock...) -- read them only for the modes whose
// parser sets them.
static const int target_modes =
    AI_GOAL_WAYPOINTS | AI_GOAL_WAYPOINTS_ONCE |
    AI_GOAL_DESTROY_SUBSYSTEM | AI_GOAL_DISABLE_SHIP |
    AI_GOAL_DISARM_SHIP | AI_GOAL_WARP | AI_GOAL_STAY_STILL |
    AI_GOAL_DOCK | AI_GOAL_CHASE | AI_GOAL_CHASE_WING |
    AI_GOAL_GUARD | AI_GOAL_GUARD_WING | AI_GOAL_EVADE_SHIP |
    AI_GOAL_STAY_NEAR_SHIP | AI_GOAL_IGNORE;

static const int submode_modes =
    AI_GOAL_DOCK | AI_GOAL_UNDOCK |
    AI_GOAL_DISABLE_SHIP | AI_GOAL_DISARM_SHIP;

static void
emit_ai_goals(const ai_goal *goals, std::string &t)
{
    char buf[256];

    t += "\"ai_goals\": [";
    int g = 0;
    for (int k = 0; k < MAX_AI_GOALS; ++k) {
        const ai_goal &goal = goals[k];
        if (goal.ai_mode == AI_GOAL_NONE)
            continue;

        const char *target =
            (goal.ai_mode & target_modes) ? goal.ship_name : NULL;
        const int submode =
            (goal.ai_mode & submode_modes) ? goal.ai_submode : 0;

        snprintf(buf, sizeof buf,
                 "%s{\"mode\": %d, \"submode\": %d, \"priority\": %d, ",
                 g ? ", " : "", goal.ai_mode, submode, goal.priority);
        t += buf;
        t += "\"target\": \"" + esc(target) + "\"}";
        ++g;
    }
    t += "]";
}

int
main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr,
                "usage: mission2tres <game-root> <mission.fs2> <out.tres>\n");
        return 2;
    }

    char exe_path[CF_MAX_PATHNAME_LENGTH];
    snprintf(exe_path, sizeof(exe_path), "%s/x", argv[1]);
    if (cfile_init(exe_path)) {
        fprintf(stderr, "mission2tres: cfile_init failed for %s\n", argv[1]);
        return 1;
    }

    // the shiptbl2tres recipe: explicit language, timer for bmpman
    // bookkeeping, FRED's parse-only mode, stubbed color stores. Int3
    // reverts to retail's continuable semantics -- bulk ship creation
    // trips a diagnostic one in ship_make_create_time_unique (see
    // debug.cc), exactly as real FRED does and survives.
    setenv("FS2_INT3_CONTINUE", "1", 1);
    Fred_running = 1;
    gr_screen.gf_init_color = null_init_color;
    gr_screen.gf_init_alphacolor = null_init_alphacolor;
    timer_init();
    lcl_init(LCL_ENGLISH);
    obj_init();       // ship_create walks obj_used_list
    ai_init();        // ai.tbl; ship_create claims an AI slot per ship
    ai_level_init();  // resets the slot free-list
    weapon_init();
    ship_init();
    // under Fred_running the briefing parser stuffs into preallocated
    // buffers; this is FRED's own allocator for them
    mission_brief_common_init();

    if (parse_main(argv[2]) != 0) {
        fprintf(stderr, "mission2tres: parse failed for %s\n", argv[2]);
        return 1;
    }

    std::string t;
    t += "[gd_resource type=\"Resource\" script_class=\"MissionData\" "
         "load_steps=2 format=3]\n\n";
    t += "[ext_resource type=\"Script\" path=\"res://mission_data.gd\" "
         "id=\"1\"]\n\n";
    t += "[resource]\n";
    t += "script = ExtResource(\"1\")\n";

    char buf[512];
    snprintf(buf, sizeof buf, "mission_name = \"%s\"\n", The_mission.name);
    t += buf;
    t += "ships = [";

    int n = 0;
    for (int i = 0; i < num_objects; ++i) {
        const object &o = Objects[i];
        if (o.type != OBJ_SHIP && o.type != OBJ_START)
            continue;

        const ship &sh = Ships[o.instance];
        const ship_info &si = Ship_info[sh.ship_info_index];

        std::string stem = si.pof_file;
        const std::size_t dot = stem.rfind('.');
        if (dot != std::string::npos)
            stem.resize(dot);
        for (char &c : stem)
            c = tolower((unsigned char)c);

        t += n ? ", {\n" : "{\n";
        snprintf(buf, sizeof buf,
                 "\"name\": \"%s\",\n\"ship_class\": \"%s\",\n"
                 "\"pof\": \"%s\",\n\"team\": %d,\n",
                 sh.ship_name, si.name, stem.c_str(), sh.team);
        t += buf;
        snprintf(buf, sizeof buf, "\"pos\": Vector3(%.9g, %.9g, %.9g),\n",
                 o.pos.x, o.pos.y, o.pos.z);
        t += buf;
        snprintf(buf, sizeof buf,
                 "\"rvec\": Vector3(%.9g, %.9g, %.9g),\n"
                 "\"uvec\": Vector3(%.9g, %.9g, %.9g),\n"
                 "\"fvec\": Vector3(%.9g, %.9g, %.9g),\n",
                 o.orient.rvec.x, o.orient.rvec.y, o.orient.rvec.z,
                 o.orient.uvec.x, o.orient.uvec.y, o.orient.uvec.z,
                 o.orient.fvec.x, o.orient.fvec.y, o.orient.fvec.z);
        t += buf;
        // player starts carry OF_PLAYER_SHIP under Fred_running
        // (missionparse.cc:1234); "invulnerable" lands on the object at
        // ship creation (missionparse.cc:1275) -- the weapons slice must
        // honor it or the player can shoot the Instructor dead
        snprintf(buf, sizeof buf,
                 "\"player_start\": %s,\n\"invulnerable\": %s,\n",
                 (o.flags & OF_PLAYER_SHIP) ? "true" : "false",
                 (o.flags & OF_INVULNERABLE) ? "true" : "false");
        t += buf;

        // the ship's OWN initial orders ($AI Goals sexps are CONSUMED at
        // ship creation, missionparse.cc:1303 -- ai_add_ship_goal_sexp,
        // nodes freed). Wing orders do NOT reach members under
        // Fred_running (the copy runs at wing creation, which Fred skips,
        // missionparse.cc:2520) -- they ride the `wings` array below.
        emit_ai_goals(Ai_info[sh.ai_index].goals, t);
        t += "\n}";
        ++n;
    }
    t += "]\n";

    // wings: the members (ship_index is filled under Fred_running,
    // missionparse.cc:2475), the wing's initial orders (decoded into
    // wingp->ai_goals by ai_add_wing_goal_sexp; retail hands them to each
    // member at wing creation), and the arrival/departure machinery the
    // events engine will need -- those cue sexps are parsed and KEPT.
    t += "wings = [";
    for (int i = 0; i < num_wings; ++i) {
        const wing &w = Wings[i];
        t += i ? ", {\n" : "{\n";
        t += "\"name\": \"" + esc(w.name) + "\",\n";
        t += "\"ships\": [";
        for (int k = 0; k < w.wave_count; ++k) {
            if (w.ship_index[k] < 0)
                continue;
            t += k ? ", \"" : "\"";
            t += esc(Ships[w.ship_index[k]].ship_name) + "\"";
        }
        t += "],\n";
        snprintf(buf, sizeof buf,
                 "\"num_waves\": %d,\n\"arrival_delay\": %d,\n",
                 w.num_waves, w.arrival_delay);
        t += buf;
        t += "\"arrival_cue\": \"" + esc(sexp_str(w.arrival_cue).c_str())
             + "\",\n";
        t += "\"departure_cue\": \"" + esc(sexp_str(w.departure_cue).c_str())
             + "\",\n";
        emit_ai_goals(w.ai_goals, t);
        t += "\n}";
    }
    t += "]\n";

    t += "events = [";
    for (int i = 0; i < Num_mission_events; ++i) {
        const mission_event &e = Mission_events[i];
        t += i ? ", {\n" : "{\n";
        t += "\"name\": \"" + esc(e.name) + "\",\n";
        t += "\"formula\": \"" + esc(sexp_str(e.formula).c_str()) + "\",\n";
        snprintf(buf, sizeof buf,
                 "\"repeat_count\": %d,\n\"interval\": %d,\n"
                 "\"score\": %d,\n\"chain_delay\": %d,\n",
                 e.repeat_count, e.interval, e.score, e.chain_delay);
        t += buf;
        // objective text is the directives gauge line; the key text still
        // carries retail's $KEY$ tokens -- the display layer substitutes
        // through the bindings table
        t += "\"objective_text\": \"" + esc(e.objective_text) + "\",\n";
        t += "\"objective_key_text\": \"" + esc(e.objective_key_text) + "\"\n}";
    }
    t += "]\n";

    t += "goals = [";
    for (int i = 0; i < Num_goals; ++i) {
        const mission_goal &g = Mission_goals[i];
        t += i ? ", {\n" : "{\n";
        t += "\"name\": \"" + esc(g.name) + "\",\n";
        snprintf(buf, sizeof buf, "\"type\": %d,\n\"score\": %d,\n",
                 g.type, g.score);
        t += buf;
        t += "\"message\": \"" + esc(g.message) + "\",\n";
        t += "\"formula\": \"" + esc(sexp_str(g.formula).c_str()) + "\"\n}";
    }
    t += "]\n";

    // no messages.tbl runs here, so Num_builtin_messages is 0 and the
    // mission's own messages fill Messages[] from the start; avi/wave are
    // names under Fred_running (message_parse strdups them)
    t += "messages = [";
    for (int i = Num_builtin_messages; i < Num_messages; ++i) {
        const MissionMessage &m = Messages[i];
        t += i > Num_builtin_messages ? ", {\n" : "{\n";
        t += "\"name\": \"" + esc(m.name) + "\",\n";
        t += "\"text\": \"" + esc(m.message) + "\",\n";
        t += "\"avi\": \"" + esc(m.avi_info.name) + "\",\n";
        t += "\"wave\": \"" + esc(m.wave_info.name) + "\"\n}";
    }
    t += "]\n";

    t += "waypoints = [";
    for (int i = 0; i < Num_waypoint_lists; ++i) {
        const waypoint_list &w = Waypoint_lists[i];
        t += i ? ", {\n" : "{\n";
        t += "\"name\": \"" + esc(w.name) + "\",\n\"points\": [";
        for (int k = 0; k < w.count; ++k) {
            snprintf(buf, sizeof buf, "%sVector3(%.9g, %.9g, %.9g)",
                     k ? ", " : "", w.waypoints[k].x, w.waypoints[k].y,
                     w.waypoints[k].z);
            t += buf;
        }
        t += "]\n}";
    }
    t += "]\n";

    std::FILE *o = std::fopen(argv[3], "wb");
    if (!o || std::fwrite(t.data(), 1, t.size(), o) != t.size()) {
        fprintf(stderr, "mission2tres: cannot write %s\n", argv[3]);
        if (o)
            std::fclose(o);
        return 1;
    }
    std::fclose(o);

    printf("mission2tres: \"%s\", %d ships, %d events, %d goals, "
           "%d messages, %d waypoint lists -> %s\n",
           The_mission.name, n, Num_mission_events, Num_goals,
           Num_messages - Num_builtin_messages, Num_waypoint_lists, argv[3]);
    return 0;
}
