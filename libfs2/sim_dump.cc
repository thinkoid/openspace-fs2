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
//   sim_dump <game-root> <mission> fire <frames> [every]
//
// `fire` is `run` with the trigger held and a minimal aim assist: each
// frame it steers toward the nearest hostile using only what crosses the
// boundary (snapshot in, controls out) -- a legitimate consumer, and the
// weapons gate's driver: retail firing, flight, BSP collision, damage and
// destruction all exercise natively, kills appearing in the mission log.
//
// All floats %.9g so every float32 round-trips exactly.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <object/object.hh>

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

    bool firing = strcmp(argv[3], "fire") == 0;
    if ((!firing && strcmp(argv[3], "run") != 0) || argc < 5) {
        fprintf(stderr, "sim_dump: unknown mode %s\n", argv[3]);
        return 2;
    }

    int frames = atoi(argv[4]);
    int every = argc > 5 ? atoi(argv[5]) : 600;

    flight_controls_t controls;
    memset(&controls, 0, sizeof(controls));

    const float dt = 1.0f / 60.0f;
    int last_target = -1;
    bool bolt_shown = false;
    bool shield_shown = false;
    bool missile_shown = false;
    bool shockwave_shown = false;
    bool fireball_shown = false;

    for (int frame = 1; frame <= frames; frame++) {
        if (firing) {
            // aim assist through the boundary: point the stick at the
            // nearest living hostile ship, hold the trigger when aligned
            std::vector<object_state_t> now = sim.snapshot();

            const object_state_t *me = nullptr;
            for (const object_state_t &o : now)
                if (o.type == OBJ_SHIP && o.player)
                    me = &o;

            const object_state_t *foe = nullptr;
            float best = 0.0f;
            if (me) {
                for (const object_state_t &o : now) {
                    // ships only: fireballs and debris are team-0 records
                    // and an undiscriminating gunner strafes the wreckage
                    if (o.type != OBJ_SHIP || o.player || o.dying)
                        continue;
                    if (o.team == me->team)
                        continue;

                    vector d;
                    vm_vec_sub(&d, const_cast<vector *>(&o.pos),
                               const_cast<vector *>(&me->pos));
                    float dist = vm_vec_mag(&d);
                    if (!foe || dist < best) {
                        foe = &o;
                        best = dist;
                    }
                }
            }

            memset(&controls, 0, sizeof(controls));
            if (me && foe) {
                // convergence sweep: the guns fire from offset points,
                // parallel to the boresight, so an aim locked dead on
                // center straddles a small target forever (the frozen
                // 55.5-hull drone). The aim point rides a slow ~3 m
                // circle around the hull instead; frame-based, so the
                // determinism contract holds.
                vector aim = foe->pos;
                aim.x += 3.0f * sinf(float(frame) * 0.05f);
                aim.y += 3.0f * cosf(float(frame) * 0.05f);

                vector to;
                vm_vec_sub(&to, &aim, const_cast<vector *>(&me->pos));
                vm_vec_normalize_safe(&to);

                // the error, in the local frame: rows dot the direction
                float right = vm_vec_dotprod(
                    &to, const_cast<vector *>(&me->orient.rvec));
                float up = vm_vec_dotprod(
                    &to, const_cast<vector *>(&me->orient.uvec));
                float fwd = vm_vec_dotprod(
                    &to, const_cast<vector *>(&me->orient.fvec));

                // bang-bang with a proportional core; positive pitch noses
                // DOWN (stick-true), so an upward error wants negative
                controls.heading = fmaxf(-1.0f, fminf(1.0f, right * 50.0f));
                controls.pitch = fmaxf(-1.0f, fminf(1.0f, -up * 50.0f));
                controls.fire_primary = fwd > 0.98f;

                // an occasional missile off the rail: heat-seekers
                // self-acquire in the launch cone, and a warhead's blast
                // is the corpus's one reliable shockwave witness
                controls.fire_secondary = fwd > 0.98f && frame % 240 == 0;
            }

            // a periodic target-next pulse: the gunner aims by snapshot,
            // but the press proves the whole targeting chain (virtual
            // stick edge -> hud_target_next -> Player_ai -> hud_state's
            // signature); periodic because the earliest presses land in
            // the pre-entry grace and go ignored
            controls.target_next = frame % 300 == 0;
        }

        sim.step(dt, controls);

        // one-shot art lines, each record kind's first crossing -- the
        // gate pins all four: laser color + tbl size, player shields,
        // the missile's POF, the expanding blast front
        if (firing && (!bolt_shown || !shield_shown || !missile_shown ||
                       !shockwave_shown || !fireball_shown)) {
            for (const object_state_t &o : sim.snapshot()) {
                if (!bolt_shown && o.type == OBJ_WEAPON && !o.pof[0]) {
                    printf("art bolt %s len %.9g r %.9g rgb %d %d %d "
                           "bitmap %s glow %s\n",
                           o.class_name, o.laser_length, o.laser_head_radius,
                           o.laser_rgb[0], o.laser_rgb[1], o.laser_rgb[2],
                           o.laser_bitmap, o.laser_glow);
                    bolt_shown = true;
                }
                if (!missile_shown && o.type == OBJ_WEAPON && o.pof[0]) {
                    printf("art missile %s pof %s\n", o.class_name, o.pof);
                    missile_shown = true;
                }
                if (!shockwave_shown && o.type == OBJ_SHOCKWAVE) {
                    printf("art shockwave frame %d r %.9g\n", frame, o.radius);
                    shockwave_shown = true;
                }
                if (!fireball_shown && o.type == OBJ_FIREBALL) {
                    printf("art fireball %s ani %s\n", o.class_name, o.pof);
                    fireball_shown = true;
                }
                if (!shield_shown && o.player) {
                    printf("art shield '%s' %.9g %.9g %.9g %.9g max %.9g\n",
                           o.name, o.shield[0], o.shield[1], o.shield[2],
                           o.shield[3], o.shield_max);
                    printf("art player energy %.9g/%.9g burner %.9g/%.9g "
                           "gun_speed %.9g\n",
                           o.weapon_energy, o.weapon_energy_max,
                           o.burner_fuel, o.burner_fuel_max,
                           sim.hud_state().primary_speed);
                    shield_shown = true;
                }
            }
        }

        // target acquisitions and losses, recorded as they happen
        {
            int t = sim.hud_state().target_signature;
            if (t != last_target) {
                printf("hud %d target %d\n", frame, t);
                last_target = t;
            }
        }

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
            case event_t::sound:
                if (ev.has_pos)
                    printf("event %d sound %s at %.9g %.9g %.9g\n", frame,
                           ev.name, ev.pos.x, ev.pos.y, ev.pos.z);
                else
                    printf("event %d sound %s\n", frame, ev.name);
                break;
            case event_t::message:
                printf("event %d message '%s' '%s' wave %s\n", frame,
                       ev.pname, ev.text, ev.name[0] ? ev.name : "-");
                break;
            }
        }

        if (frame % every == 0) {
            for (const object_state_t &o : sim.snapshot())
                print_state(frame, o);

            hud_state_t h = sim.hud_state();
            if (h.training_text[0])
                printf("hud %d msg '%s' voice '%s'\n", frame,
                       h.training_text, h.training_voice);
            for (const directive_t &d : h.directives)
                printf("hud %d directive %d key %d '%s'\n", frame, d.state,
                       int(d.key_line), d.text);
        }
    }

    return 0;
}
