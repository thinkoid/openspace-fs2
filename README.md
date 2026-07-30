# FS2 → Godot migration

> **Status: LIVE — revived 2026-07-30, GDExtension-first.** Mothballed
> 2026-07-27 at the Training-1-playable milestone for the reason recorded
> below; revived along that section's own exit route: the simulation stays
> the port's C++, compiled as a native module, and Godot keeps presentation.
> The retail Linux port on `master` is now the mothballed line — complete,
> playable, and serving as this migration's reference implementation.

This is the `godot` branch of **openspace-fs2**: an experiment in hosting the
retail FreeSpace 2 game on the Godot engine. In nine days (2026-07-18 to
2026-07-27) it reached: every retail model converted and rendered, every
retail mission's data extracted, and **Training Mission 1 flyable as an
actual lesson** — the Instructor flies his waypoints, directives tick off,
messages play in voice, guns fire and score, the radar works.

## Why it was mothballed (2026-07-27)

The migration plan (docs/godot-migration-plan.md) called for FS2 to keep the
simulation as a C++ library with Godot presenting. The implementation drifted
somewhere better and worse: each gameplay slice was *ported into GDScript* —
faster to build, trivially hot-reloadable, and every port oracle-pinned
against retail — but the endpoint of that road is the whole game rewritten in
a dynamically-typed scripting language. The semantics being transcribed
already exist, tested and playable, in C++ on `master`. Paying a language tax
to re-own them was judged not worth it, and the experiment was stopped at a
clean, fully-verified milestone rather than at the wall.

If the line is ever revived, the sane route is **GDExtension-first**: link
the retail subsystems as native modules (the same authoritative-reader move
the converters already make) and keep Godot strictly for presentation — i.e.
the original plan's ownership table, enforced this time. The GDScript ports
below remain valuable either way: they are executable, line-cited,
gate-tested *specifications* of retail behavior.

## The revival (2026-07-30)

The crossroads assessment that reopened the branch weighed four directions —
Godot via GDExtension, an art revamp, continued incremental modernization of
the port, a fresh rewrite on a modern engine — and landed on the first, for
reasons the mothball note already implies: the port's cleaned C++ is the
payload, not the alternative; a rewrite would re-earn robustness already
owned; chipping at the port has no exit condition and its remaining bulk
(renderer, sound, platform glue) is what any engine future discards; and the
art ambitions are cheap here (GLB is Godot's native format) and expensive
everywhere else. The full argument is in master's `docs/notes.md`,
"Crossroads" entry.

The plan, in order:

1. **Reunify with master.** Since the split, master replayed the entire
   normalization (at tab width 3, not this branch's 8) and ran the full
   fix + warning-survey campaign — 32 commits, warnings 4,756 → 15, real
   retail bugs found and fixed. Merge master in, resolving shared sources
   to master's side wholesale; this branch's additions (`tools/`,
   `inspect/`, the test checkers, `subprojects/libpof`, the docs) carry
   over; the few hybrid files (meson wiring, `debug_int3`'s
   `FS2_INT3_CONTINUE`) re-merge by hand. The 18-gate suite re-proves the
   result before anything else moves.
2. **The GDExtension boundary.** The ownership table, enforced this time:
   retail subsystems compiled as a native module, value-only snapshots out,
   Godot presents. The GDScript ports below retire slice by slice into what
   they already are — executable, line-cited specifications — and their
   gates become the boundary's differential oracle: the GDExtension must
   produce what the GDScript port produced, which was pinned against retail.
3. **The art track rides along.** Retail assets (through `pof2glb`) stay
   the working fleet while the boundary stabilizes — holding assets constant
   keeps the oracles pointed at the boundary code, not at new meshes. The
   ship-design-language line (`docs/ship-design-language.md`) starts when a
   new ship is just a new GLB beside the old ones.

## What was built

Three layers, and the seam between FS2 and Godot sits at **build time and
test time — never at runtime**:

```
 retail data ──► C++ converters (retail's OWN parsers) ──► .glb / .tres
                                                              │
                                                              ▼
             Godot runtime (pure GDScript, inspect/) ◄── loads at runtime
                                                              │
 retail C++ oracles (pof_dump, physics_dump …) ◄── 18 gates ──┘
```

### The converters (`tools/`, C++ — the authoritative readers)

Each tool links retail's *actual* parser, so format knowledge is transcribed,
never re-derived. All output is deterministic (no timestamps): same input,
same bytes.

- **`vpstage`** — stages models/maps out of the VP archives through retail's
  own `cfile` (archive precedence is retail's answer), with a SHA-256
  manifest per file.
- **`pof2glb`** — POF → GLB + `textures/` (PCX→PNG) + a `.tres` carrying
  everything glTF can't: weapon points, turrets, thrusters, docks, paths,
  subsystems, shield mesh. Reads through libpof (subprojects/, oracle-pinned
  byte-identical to retail's loader). Plus a `.manifest.json` of digests.
- **`mission2tres`** — runs retail's `parse_main` under `Fred_running` (every
  object created, arrival cues aside) and emits the mission: objects with
  placement, events/goals as canonical one-line sexp text, messages,
  waypoint lists. The evaluator's entire diet.
- **`shiptbl2tres`** — runs retail's table chain (`gamesnd_parse_soundstbl`,
  `weapon_init`, `ship_init`) and emits flight parameters, weapon data with
  sound names, and effect sounds — `ship_params.tres`.

### The Godot runtime (`inspect/`, pure GDScript)

No C++ executes at runtime. Every behavioral port cites the retail source
line it transcribes (`aicode.cc:4687`, `radar.cc:335`, …).

- **`inspect.gd`** — model viewer: loads converter output *at runtime*
  (GLTFDocument + ResourceLoader, no import pipeline — what renders is the
  raw output), articulates movable submodels, overlays every `.tres` fact.
- **`ship.gd`** — retail's load-time model assembly: detail levels, debris,
  turret/thruster movement reinterpretation, engine-gated thruster glow.
- **`flight_model.gd`** — retail's flight integrator (`physics.cc`):
  damping, rotational velocity, afterburner. Oracle-pinned frame-by-frame
  against `physics_dump` (the port's own C++ physics, same inputs).
- **`sexp_vm.gd`** — retail's SEXP evaluator ported function-for-function
  (`eval_sexp`, `sexp_and/or/not`, `eval_when`, `mission_process_event`)
  over cons cells mirroring `Sexp_nodes`, including the `KNOWN_*` subtree
  cauterization, NAN semantics, chain delays, and retail's
  timestamp-overload bug-compatibilities. Operators that touch the world
  dispatch to the mission scene; an unimplemented op logs once and
  evaluates false — a running mission prints its own TODO list.
- **`mission.gd`** — the mission scene and the VM's "world": spawns the
  layout as Ships, runs the player under FlightModel, answers the sexp
  vocabulary from live state (`is-destroyed-delay`, `hits-left`,
  `are-waypoints-done-delay`, `key-pressed`, `add-goal`, …), renders HUD,
  directives, training messages, target monitor, radar.
- **`weapons.gd`** — gun cadence, swept segment-vs-bounding-sphere
  collision (deliberate simplification of retail's BSP `model_collide`),
  hull ledger, the destroyed registry.
- **`waypoint_ai.gd`** — retail waypoint flight: distance-proportional
  commanded speed (`dist/5`), retail's two-armed arrival test, the
  mission-log stamp on the *last* waypoint only, park on completion.
- **`targeting.gd`**, **`radar.gd`** (retail's blip projection, quirks
  included: a contact exactly astern plots at center), **`sound.gd`**
  (voice/effect playback straight from the install's WAVs).
- **Schemas:** `ship_data.gd`, `ship_params.gd`, `mission_data.gd` — the
  `.tres` contracts.

### Verification (`tests/` — 18 meson gates)

The house differential-oracle doctrine: every port is diffed against retail's
own C++ answering the same question, and every gate was *proven to bite* by
perturbing the subject and watching it go red.

- Corpus-wide: `corpus-check` (all 176 retail models through the converter
  vs `pof_dump`), `mission-check` (all install missions + the synthetic one
  through `mission2tres` vs independent Python reads), `ship-load-check`
  (every model assembled in a real Godot boot, movement replay vs the dump).
- Per-slice: `flight-check` (physics trace diff), `weapons-check`,
  `waypoints-check`, `radar-check`, `sound-check`, `targeting-check`,
  `shiptbl-check`, `glb/tres/tex/manifest/tres-load/vpstage-check`,
  `pof-oracle`.

## What works right now

- **All 176 retail models** convert clean and render — geometry, hierarchy,
  textures, articulation, and every POF datum inspectable as an overlay.
- **All install missions (91/91 with the synthetic range)** extract through
  retail's parser: placement float32-exact, events/goals/messages/waypoints
  verified against the mission text.
- **Training Mission 1 is playable as a lesson**: mouse flight with pointer
  capture, the Instructor flies his waypoint paths and parks, directives
  and training messages advance with voice, guns fire with sound and score
  hits, radar/lead-indicator/reticle/target-monitor HUD.
- **`tests/weapons-range.fs2`** — a synthetic live-fire mission in retail's
  dialect (three hostile drones, hull/destruction events) as the weapons
  proving ground.

Not crossed (recorded, not started): missiles, shields, subsystem damage,
dogfight AI, ship arrival/departure, loadout, energy management, briefings,
menus, campaign flow — and the ~100 sexp operators that sit on top of those
subsystems.

## Recipes

Prerequisites: meson + ninja, a C++17 toolchain, SDL2/OpenAL/GL/zlib,
Python 3, **Godot 4.x** (developed on 4.7.1), and your own retail FreeSpace 2
data (GOG) unpacked into a sibling run directory — the recipes below assume
`../rundir` with `data/{models,maps,tables,missions}` populated.

### Build the toolchain

```
git submodule update --init          # libpof, under subprojects/
meson setup build
ninja -C build
```

### Convert and view a model

```
mkdir -p /tmp/models
./build/tools/pof2glb ../rundir/data/models/fighter01.pof /tmp/models/fighter01.glb

godot --path inspect -- /tmp/models/fighter01.glb        # inspection viewer
godot --path inspect -- fly /tmp/models/fighter01.glb    # fly it
```

The converter drops the `.tres`, `textures/` and manifest beside the GLB.
Viewer keys: `1`–`0` toggle overlays (guns, missiles, turrets, thrusters,
docks, eyes, paths, subsystems, shield, bounds/axes), `D` detail level,
`M` spin, `R` reset, `H` help; left-drag orbits, wheel zooms. Any of the
176 retail POFs works — fighters through capital ships.

### Play Training Mission 1

Everything lives in one directory; the mission scene finds the GLBs and
`ship_params.tres` beside the mission `.tres` by POF stem. Training-1 needs
three classes: GTF Myrmidon (you, `fighter2t-05`), GTF Ulysses (the
Instructor, `fighter01`), GTDR Amazon (`drone01`).

```
mkdir -p /tmp/t1
./build/tools/mission2tres ../rundir training-1.fs2 /tmp/t1/training-1.tres

for m in fighter2t-05 fighter01 drone01; do
    ./build/tools/pof2glb ../rundir/data/models/$m.pof /tmp/t1/$m.glb
done
./build/tools/shiptbl2tres ../rundir /tmp/t1/ship_params.tres

godot --path inspect -- mission /tmp/t1/training-1.tres "$PWD/../rundir"
```

The trailing game root (or `FS2_GAME_ROOT`) is where the WAVs come from —
message voice, gun, impacts, explosions. Without it the mission runs silent.

Controls: **mouse** steers (captured in-window; click to re-grab), **M1**
fires, `A`/`Z` throttle up/down, `\` full, `Backspace` zero, `Tab`
afterburner, `Q`/`E` roll, `T` target next / `H` next hostile, `M`
match target speed (a mode; throttle keys cancel), `V` cockpit/chase,
`R` reset, `F1` HUD, `Esc` quits.

Follow the lesson: the directives gauge top-left is the Instructor's
curriculum, and the events only advance when you do what he says (the
`key-pressed` sexp is watching your actual keys).

The live-fire range works the same way from the same directory:

```
./build/tools/mission2tres ../rundir "$PWD/tests/weapons-range.fs2" /tmp/t1/weapons-range.tres
godot --path inspect -- mission /tmp/t1/weapons-range.tres "$PWD/../rundir"
```

(The mission path must be absolute — a name without a directory is resolved
through retail's `cfile` against the install, and a relative path resolves
against cfile's roots, not your shell's working directory.)

### Run the gates

```
meson test -C build            # finds ../rundir on its own; or set FS2_GAME_ROOT
```

Gates that need the install skip cleanly (exit 77) without it.

## Where the knowledge lives

The findings outlive the vehicle — most are engine-agnostic retail-semantics
censuses, useful to any future frontend:

- **docs/godot-migration-plan.md** — the plan, and per-slice "facts" blocks
  recording every retail behavior discovered while porting (waypoint arrival
  math, radar projection quirks, sexp timestamp overloads, thruster rules…),
  each with its retail `file:line` citation.
- **docs/sexp-vm.md** — the SEXP evaluator analysis.
- **docs/pof-model.md**, **docs/pof-corpus-survey.txt** — the POF format and
  what the retail corpus actually exercises.
- **docs/ship-design-language.md** — a design direction for future original
  ships (thermodynamically honest hulls); engine-agnostic by construction.

## Legal

The source is Volition's, under the terms of its 2002 release: it may not be
sold or commercially exploited. FreeSpace 2 and its assets are the property
of their respective owners. This repository contains no game data.
