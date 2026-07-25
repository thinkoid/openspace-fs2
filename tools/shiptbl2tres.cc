// -*- mode: c++; -*-
//
// shiptbl2tres: stage the flight parameters out of ships.tbl through
// retail's OWN table parser -- the same authoritative-reader pattern as
// vpstage (docs/godot-migration-plan.md). weapon_init + ship_init run the
// real parse_shiptbl over the real table, and whatever lands in Ship_info
// -- including retail-derived values like max_rotvel = 2*PI/rotation_time
// (ship.cc:588) -- is emitted as a Godot .tres against the
// inspect/ship_params.gd schema, keyed by lowercased POF stem so a
// converted ship finds its own numbers.
//
//   shiptbl2tres <game-root> <out.tres>
//
// Emits the physics_ship_init subset (ship.cc:1292): density (mass = POF
// mass * density), damp/rotdamp, max_vel/max_rear_vel/max_rotvel, the
// accel/decel time constants, and the afterburner set. Nothing is stamped
// with a time: same table, same .tres, byte for byte.

#include <stdio.h>

#include <string>

#include <globalincs/pstypes.hh>
#include <cfile/cfile.hh>
#include <graphics/2d.hh>
#include <io/timer.hh>
#include <localization/localize.hh>
#include <ship/ship.hh>
#include <weapon/weapon.hh>

// weapons.tbl parsing stores laser colors, and ship_init the IFF colors,
// through gr_screen's function pointers; no renderer runs here, so
// do-nothing stubs stand in
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
    if (argc != 3) {
        fprintf(stderr, "usage: shiptbl2tres <game-root> <out.tres>\n");
        return 2;
    }

    // cfile_init derives the root by truncating at the last separator
    char exe_path[CF_MAX_PATHNAME_LENGTH];
    snprintf(exe_path, sizeof(exe_path), "%s/x", argv[1]);
    if (cfile_init(exe_path)) {
        fprintf(stderr, "shiptbl2tres: cfile_init failed for %s\n", argv[1]);
        return 1;
    }

    // the game's own init order (freespace.cc): localization, then
    // weapons.tbl (ships.tbl references weapon names), then ships.tbl.
    // The language is passed explicitly -- lcl_init(-1) asks the os
    // registry, which no tool context has (Int3 on NULL); the timer comes
    // up because weapon parsing touches bmpman's cache bookkeeping; and
    // Fred_running takes FRED's parse-only path -- tables without bitmap
    // loads, the editor's own no-render mode doing exactly this job.
    Fred_running = 1;
    gr_screen.gf_init_color = null_init_color;
    gr_screen.gf_init_alphacolor = null_init_alphacolor;
    timer_init();
    lcl_init(LCL_ENGLISH);
    weapon_init();
    ship_init();

    std::string t;
    t += "[gd_resource type=\"Resource\" script_class=\"ShipParams\" "
         "load_steps=2 format=3]\n\n";
    t += "[ext_resource type=\"Script\" path=\"res://ship_params.gd\" "
         "id=\"1\"]\n\n";
    t += "[resource]\n";
    t += "script = ExtResource(\"1\")\n";
    t += "ships = {\n";

    char buf[512];
    for (int i = 0; i < Num_ship_types; ++i) {
        const ship_info &s = Ship_info[i];

        // key: lowercased POF stem, the converted asset's name
        std::string key = s.pof_file;
        const std::size_t dot = key.rfind('.');
        if (dot != std::string::npos)
            key.resize(dot);
        for (char &c : key)
            c = tolower((unsigned char)c);

        t += "\"" + key + "\": {\n";
        snprintf(buf, sizeof buf, "\"name\": \"%s\",\n", s.name);
        t += buf;
        snprintf(buf, sizeof buf,
                 "\"density\": %.9g,\n\"damp\": %.9g,\n\"rotdamp\": %.9g,\n",
                 s.density, s.damp, s.rotdamp);
        t += buf;
        snprintf(buf, sizeof buf,
                 "\"max_vel\": Vector3(%.9g, %.9g, %.9g),\n"
                 "\"max_rear_vel\": %.9g,\n"
                 "\"max_rotvel\": Vector3(%.9g, %.9g, %.9g),\n",
                 s.max_vel.x, s.max_vel.y, s.max_vel.z, s.max_rear_vel,
                 s.max_rotvel.x, s.max_rotvel.y, s.max_rotvel.z);
        t += buf;
        snprintf(buf, sizeof buf,
                 "\"forward_accel\": %.9g,\n\"forward_decel\": %.9g,\n"
                 "\"slide_accel\": %.9g,\n\"slide_decel\": %.9g,\n",
                 s.forward_accel, s.forward_decel, s.slide_accel,
                 s.slide_decel);
        t += buf;
        snprintf(buf, sizeof buf,
                 "\"afterburner_max_vel\": Vector3(%.9g, %.9g, %.9g),\n"
                 "\"afterburner_forward_accel\": %.9g\n",
                 s.afterburner_max_vel.x, s.afterburner_max_vel.y,
                 s.afterburner_max_vel.z, s.afterburner_forward_accel);
        t += buf;
        t += (i + 1 < Num_ship_types) ? "},\n" : "}\n";
    }
    t += "}\n";

    std::FILE *o = std::fopen(argv[2], "wb");
    if (!o || std::fwrite(t.data(), 1, t.size(), o) != t.size()) {
        fprintf(stderr, "shiptbl2tres: cannot write %s\n", argv[2]);
        if (o)
            std::fclose(o);
        return 1;
    }
    std::fclose(o);

    printf("shiptbl2tres: %d ships -> %s\n", Num_ship_types, argv[2]);
    return 0;
}
