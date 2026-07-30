// sim_dump drives fs2_t with no engine in the room -- the boundary API
// linked directly, physics_dump's grown-up sibling. The world gates run
// it: `layout` prints the t=0 snapshot (diffed against mission2tres's
// FRED-view .tres), `run` steps the sim with a hands-off stick and prints
// every event plus periodic ship states (the Instructor provably flies
// his waypoints under retail AI). Output is deterministic for a given
// seed -- the determinism gate diffs two runs byte-for-byte.
//
//   sim_dump <game-root> <mission> layout
//   sim_dump <game-root> <mission> run <frames> [every]
//
// All floats %.9g so every float32 round-trips exactly.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fs2.hh"

static void
print_state(int frame, const object_state_t &o)
{
    printf("state %d %s sig %d class %s pof %s team %d arrival %d "
           "player %d dying %d "
           "pos %.9g %.9g %.9g "
           "orient %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g "
           "vel %.9g %.9g %.9g hull %.9g/%.9g\n",
           frame, o.name, o.signature, o.class_name, o.pof, o.team,
           o.arrival_location, int(o.player), int(o.dying),
           o.pos.x, o.pos.y, o.pos.z,
           o.orient.rvec.x, o.orient.rvec.y, o.orient.rvec.z,
           o.orient.uvec.x, o.orient.uvec.y, o.orient.uvec.z,
           o.orient.fvec.x, o.orient.fvec.y, o.orient.fvec.z,
           o.vel.x, o.vel.y, o.vel.z, o.hull, o.hull_max);
}

int
main(int argc, char *argv[])
{
    if (argc < 4) {
        fprintf(stderr,
                "usage: sim_dump <game-root> <mission> layout\n"
                "       sim_dump <game-root> <mission> run <frames> [every]\n");
        return 2;
    }

    fs2_t sim;

    if (!sim.load(argv[1], argv[2], 42)) {
        fprintf(stderr, "sim_dump: load failed for %s\n", argv[2]);
        return 1;
    }

    if (strcmp(argv[3], "layout") == 0) {
        for (const object_state_t &o : sim.snapshot())
            print_state(0, o);
        return 0;
    }

    if (strcmp(argv[3], "run") != 0 || argc < 5) {
        fprintf(stderr, "sim_dump: unknown mode %s\n", argv[3]);
        return 2;
    }

    int frames = atoi(argv[4]);
    int every = argc > 5 ? atoi(argv[5]) : 600;

    flight_controls_t hands_off;
    memset(&hands_off, 0, sizeof(hands_off));

    const float dt = 1.0f / 60.0f;

    for (int frame = 1; frame <= frames; frame++) {
        sim.step(dt, hands_off);

        for (const event_t &ev : sim.events()) {
            switch (ev.kind) {
            case event_t::created:
                printf("event %d created %s sig %d\n", frame, ev.name,
                       ev.signature);
                break;
            case event_t::destroyed:
                printf("event %d destroyed %s sig %d\n", frame, ev.name,
                       ev.signature);
                break;
            case event_t::log:
                printf("event %d log %d '%s' '%s' t %.3f\n", frame,
                       ev.log_type, ev.pname, ev.sname,
                       double(ev.time) / 65536.0);
                break;
            }
        }

        if (frame % every == 0)
            for (const object_state_t &o : sim.snapshot())
                print_state(frame, o);
    }

    return 0;
}
