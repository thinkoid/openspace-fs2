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
//   sim_dump <game-root> <campaign> campaign <frames> [take_loop]
//   sim_dump <game-root> <mission> warpout <engage-frame> <frames> [abort]
//
// `fire` is `run` with the trigger held and a minimal aim assist: each
// frame it steers toward the nearest hostile using only what crosses the
// boundary (snapshot in, controls out) -- a legitimate consumer, and the
// weapons gate's driver: retail firing, flight, BSP collision, damage and
// destruction all exercise natively, kills appearing in the mission log.
//
// `campaign` loads the named .fc2 (with the pilot's .csg resume), flies
// the CURRENT mission with the fire driver, then runs the mission-end
// pair: debrief (goal states, stage selection, the branch verdict) and
// accept (the .csg save, the advance) -- the campaign-flow gate's driver.
//
// `warpout` flies hands-off and presses the jump key at the engage
// frame, witnessing the departure sequence: each stage once, the warp
// hole's crossing, the departure log entry. With `abort`, a second
// press two seconds in exercises the stage-1 abort and the sim flies on.
//
// All floats %.9g so every float32 round-trips exactly.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mission/missioncampaign.hh>
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

    // campaign mode resolves the mission itself; the others fly argv[2]
    bool campaign = strcmp(argv[3], "campaign") == 0;
    const char *mission = argv[2];

    if (campaign) {
        if (!sim.load_campaign(argv[1], argv[2])) {
            fprintf(stderr, "sim_dump: campaign load failed for %s\n",
                    argv[2]);
            return 1;
        }

        mission = sim.current_mission();
        printf("campaign %s mission '%s'\n", argv[2], mission);
        if (!mission[0]) {
            printf("campaign complete\n");
            return 0;
        }
    }

    if (!sim.load(argv[1], mission, 42)) {
        fprintf(stderr, "sim_dump: load failed for %s\n", mission);
        return 1;
    }

    if (strcmp(argv[3], "layout") == 0) {
        for (const object_state_t &o : sim.snapshot())
            print_state(0, o);

        // the authored sky, one line per element -- world-check pins
        // training-1's sun against the mission text
        for (const backdrop_t &d : sim.backdrop())
            printf("backdrop %s %s glow %s uvec %.9g %.9g %.9g "
                   "scale %.9g %.9g xparent %d rgbi %.9g %.9g %.9g %.9g\n",
                   d.sun ? "sun" : "bitmap", d.name, d.glow[0] ? d.glow : "-",
                   d.orient.uvec.x, d.orient.uvec.y, d.orient.uvec.z,
                   d.scale_x, d.scale_y, int(d.xparent), d.r, d.g, d.b, d.i);
        printf("backdrop stars %d\n", sim.num_stars());
        return 0;
    }

    bool firing = campaign || strcmp(argv[3], "fire") == 0;
    bool warpout_mode = strcmp(argv[3], "warpout") == 0;
    if ((!firing && !warpout_mode && strcmp(argv[3], "run") != 0) ||
        argc < 5 || (warpout_mode && argc < 6)) {
        fprintf(stderr, "sim_dump: unknown mode %s\n", argv[3]);
        return 2;
    }

    int engage = warpout_mode ? atoi(argv[4]) : 0;
    int frames = atoi(argv[warpout_mode ? 5 : 4]);
    int every = (!campaign && !warpout_mode && argc > 5) ? atoi(argv[5]) : 600;
    bool take_loop = campaign && argc > 5 && atoi(argv[5]) != 0;
    bool warp_abort = warpout_mode && argc > 6 &&
                      strcmp(argv[6], "abort") == 0;

    flight_controls_t controls;
    memset(&controls, 0, sizeof(controls));

    const float dt = 1.0f / 60.0f;
    int last_target = -1;
    bool bolt_shown = false;
    bool shield_shown = false;
    bool missile_shown = false;
    bool shockwave_shown = false;
    bool fireball_shown = false;
    bool debris_shown = false;
    bool weapons2_shown = false;
    bool subsys_shown = false;
    int last_stage = 0;
    bool warp_hole_shown = false;
    bool departed_seen = false;
    bool shieldhit_shown = false;

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
            // the pre-entry grace and go ignored. The offset hostile
            // pulse exercises the H binding's chain the same way.
            controls.target_next = frame % 300 == 0;
            controls.target_hostile = frame % 300 == 150;

            // one pulse each, after the entry grace: cycle the primary
            // (bank 1 -> bank 2), cycle the missile bank, target a
            // subsystem -- the weapons gate pins what each did
            controls.cycle_primary = frame == 450;
            controls.cycle_secondary = frame == 456;
            controls.target_subsys = frame == 462;
        }

        if (warpout_mode)
            controls.warp_out = frame == engage ||
                                (warp_abort && frame == engage + 120);

        sim.step(dt, controls);

        // the warpout witnesses -- each stage once, the hole once, the
        // abort or the departure; the warpout gate pins the sequence
        if (warpout_mode && !departed_seen) {
            hud_state_t hs = sim.hud_state();

            if (hs.warpout_stage != last_stage) {
                if (hs.warpout_stage > 0)
                    printf("warpout stage %d\n", hs.warpout_stage);
                else if (!hs.departed)
                    printf("warpout aborted\n");
                last_stage = hs.warpout_stage;
            }

            if (hs.warpout_stage >= 2 && !warp_hole_shown) {
                for (const object_state_t &o : sim.snapshot())
                    if (o.type == OBJ_FIREBALL &&
                        strcmp(o.class_name, "warp") == 0) {
                        printf("warpout hole up ani %s\n", o.pof);
                        warp_hole_shown = true;
                        break;
                    }
            }

            if (hs.departed) {
                printf("warpout departed\n");
                departed_seen = true;   // the loop ends after this
                                        // frame's event drain (the log
                                        // entry crosses there)
            }
        }

        // one-shot art lines, each record kind's first crossing -- the
        // gate pins all four: laser color + tbl size, player shields,
        // the missile's POF, the expanding blast front
        if (firing && (!bolt_shown || !shield_shown || !missile_shown ||
                       !shockwave_shown || !fireball_shown ||
                       !debris_shown)) {
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
                if (!debris_shown && o.type == OBJ_DEBRIS) {
                    printf("art debris pof %s piece %s\n", o.pof, o.piece);
                    debris_shown = true;
                }
                if (!shield_shown && o.player) {
                    printf("art shield '%s' %.9g %.9g %.9g %.9g max %.9g "
                           "icon %s species %d\n",
                           o.name, o.shield[0], o.shield[1], o.shield[2],
                           o.shield[3], o.shield_max,
                           o.shield_icon[0] ? o.shield_icon : "-", o.species);
                    printf("art player energy %.9g/%.9g burner %.9g/%.9g "
                           "gun_speed %.9g\n",
                           o.weapon_energy, o.weapon_energy_max,
                           o.burner_fuel, o.burner_fuel_max,
                           sim.hud_state().primary_speed);

                    // the weapon gauge: every mounted bank, armed flag
                    // and shots-per-pull -- the gate pins the loadout
                    hud_state_t hs = sim.hud_state();
                    printf("art weapons");
                    for (const weapon_bank_t &b : hs.primary_banks)
                        printf(" p '%s' %d %d", b.name, int(b.armed),
                               b.shots);
                    for (const weapon_bank_t &b : hs.secondary_banks)
                        printf(" s '%s' %d %d", b.name, int(b.armed),
                               b.shots);
                    printf("\n");
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

        // the post-pulse witnesses, once each: the weapon gauge after
        // both cycles, and the first targeted subsystem
        if (firing && frame >= 470 && !weapons2_shown) {
            hud_state_t hs = sim.hud_state();
            printf("art weapons2");
            for (const weapon_bank_t &b : hs.primary_banks)
                printf(" p '%s' %d %d", b.name, int(b.armed), b.shots);
            for (const weapon_bank_t &b : hs.secondary_banks)
                printf(" s '%s' %d %d", b.name, int(b.armed), b.shots);
            printf("\n");
            weapons2_shown = true;
        }
        if (firing && !subsys_shown) {
            hud_state_t hs = sim.hud_state();
            if (hs.target_subsys[0]) {
                printf("art subsys '%s'\n", hs.target_subsys);
                subsys_shown = true;
            }
        }

        // the first quadrant dip: incoming fire reached the player's
        // shield -- the shield-range gate's witness that the range
        // shoots back (any driving mode; a hands-off run is the pure
        // incoming-fire case)
        if (!shieldhit_shown) {
            for (const object_state_t &o : sim.snapshot()) {
                if (!o.player || o.shield_max <= 0.0f)
                    continue;
                float qmax = o.shield_max / 4.0f;
                for (int q = 0; q < 4; q++)
                    if (o.shield[q] < qmax - 0.5f) {
                        printf("art shieldhit frame %d q %d %.9g of %.9g\n",
                               frame, q, o.shield[q], qmax);
                        shieldhit_shown = true;
                        break;
                    }
                break;
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
            case event_t::hud_text:
                printf("event %d hud '%s' src %d\n", frame, ev.text,
                       ev.source);
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

        if (departed_seen)
            break;
    }

    // the mission-end pair: everything the debrief screen would show,
    // then the Accept -- .csg written, campaign advanced
    if (campaign) {
        debrief_t d = sim.debrief();

        for (const goal_state_t &g : d.goals)
            printf("goal '%s' type %d status %d invalid %d text '%s'\n",
                   g.name, g.type, g.status, int(g.invalid), g.text);

        // stage text is multi-line prose; the gate pins count, voice and
        // length, not the paragraphs
        int idx = 0;
        for (const debrief_stage_t &s : d.stages)
            printf("stage %d voice %s text %zu rec %zu\n", idx++,
                   s.voice[0] ? s.voice : "-", s.text.size(),
                   s.recommendation.size());

        printf("verdict next '%s' loop %d\n", d.next_mission,
               int(d.loop_offer));
        if (d.loop_offer)
            printf("loop desc '%s' voice %s\n", d.loop_desc.c_str(),
                   d.loop_voice[0] ? d.loop_voice : "-");

        sim.accept(take_loop);
        printf("current '%s'\n", sim.current_mission());

        // the loop bookkeeping, retail's own globals (an oracle tool
        // reads past the boundary on purpose): enabled while inside the
        // detour, cleared when the reentry mission comes up -- the
        // loop-arc gate pins the .csg-carried pair directly
        printf("loop state enabled %d reentry %d\n", Campaign.loop_enabled,
               Campaign.loop_reentry);
    }

    return 0;
}
