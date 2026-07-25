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
// initial velocity (percent of max), hull, and the player-start flag.
// Nothing is stamped with a time.

#include <stdio.h>
#include <stdlib.h>

#include <string>

#include <globalincs/pstypes.hh>
#include <cfile/cfile.hh>
#include <graphics/2d.hh>
#include <io/timer.hh>
#include <localization/localize.hh>
#include <mission/missionbriefcommon.hh>
#include <mission/missionparse.hh>
#include <ship/ai.hh>
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
        // (missionparse.cc:1234)
        snprintf(buf, sizeof buf,
                 "\"player_start\": %s\n}",
                 (o.flags & OF_PLAYER_SHIP) ? "true" : "false");
        t += buf;
        ++n;
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

    printf("mission2tres: \"%s\", %d ships -> %s\n", The_mission.name, n,
           argv[3]);
    return 0;
}
