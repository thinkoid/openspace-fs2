# FS2 → Godot migration

> **Status: MOTHBALLED 2026-08-05 — a second time, and at a much higher
> water mark.** The retail campaign now *simulates* end to end behind a
> native boundary and most of it is flyable, but the remaining gaps are
> campaign-completion gaps (see **Restarting this branch**), and the way to
> know which of them matter is to finish the campaign on the retail port
> first. `master` is the live line again; this branch keeps every gate
> green and waits. Nothing here is blocked or broken — it is parked on
> purpose, mid-lane, with the next four moves written down.
>
> Previously mothballed 2026-07-27 at the Training-1-playable milestone
> (reason below), revived 2026-07-30 along that section's own exit route:
> the simulation stays the port's C++ compiled as a native module, and
> Godot keeps presentation. That revival is what the six slices below
> delivered.

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

Two eras. The **GDScript era** (2026-07-18 → 07-27) put the seam at build
and test time only — converters ahead of time, pure GDScript at runtime.
The **GDExtension era** (07-30 → 08-05) moved the seam to a runtime
boundary: retail's own C++ simulation, linked as a native Godot module,
with Godot presenting. The converters survived the change unaltered; the
GDScript gameplay ports were retired into specifications.

```
 retail data ──► C++ converters (retail's OWN parsers) ──► .glb / .tres
                                                              │
                                                              ▼
   libfs2.so:  foundation + fs2_t (engine-agnostic)      art loaded at
               + FS2 shim (the one godot-including TU)     runtime by
                          │         ▲                          │
              load/step/  │         │  controls                ▼
              frame/      ▼         │              Godot presenter (inspect/,
              events/hud_state ─────┴────────────► GDScript: reconcile, render,
                          │                        HUD, sound, campaign flow)
                          │
 sim_dump (engine-free driver) ◄── 31 meson gates ──► retail C++ oracles
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

### The boundary (`libfs2/`, C++ — the simulation itself)

`libfs2.so` is the whole retail foundation linked whole, plus:

- **`fs2_t`** (`fs2.cc`/`fs2.hh`) — engine-agnostic, no Godot headers.
  `load(root, mission, seed)` replicates retail's level chain;
  `step(dt, controls)` is a headless twin of `game_frame` +
  `game_simulation_frame` driving retail's own stick through
  `read_player_controls`' seam; `frame()` crosses per-frame kinematics as
  packed parallel arrays keyed by object signature (6.7× the dictionary
  form it replaced); `snapshot()` is the same data as records and is now
  the *oracle path* only; `events()` carries births, deaths, mission-log
  drain, sounds, radio messages and HUD ticker lines; `hud_state()` the
  lesson/combat freight; `debrief()`/`accept()` the campaign flow.
- **`extension.cc`** — the single Godot-including translation unit, the
  `FS2` shim. Include order is load-bearing: Godot headers first,
  `fs2.hh` last (retail's `PI` macro).
- **`sim_dump.cc`** — an engine-free driver linking `fs2_t` directly.
  Builds without Godot, and is the ultimate oracle: whatever it prints,
  the presenter must agree with.
- Capture seams in retail's own sources, each mirroring the next:
  `Snd_capture` (`sound.cc`), `Msg_capture` (`missionmessage.cc`),
  `Hud_msg_capture` (`hudmessage.cc`). Null in the game binary.

### The Godot presenter (`inspect/`, GDScript)

Presentation only. `world.gd` is the orchestrator — frame loop, snapshot
reconciliation, input, camera, campaign — pushing state into passive
modules that never call back: `fx.gd` (transient art, flipbook cache),
`sky.gd`, `hud.gd` (every 2D pixel, debrief overlay included), `sound.gd`,
`radar.gd`, `ship.gd` (retail's load-time model assembly).

The GDScript gameplay ports of the first era — `flight_model.gd`,
`sexp_vm.gd`, `weapons.gd`, `waypoint_ai.gd`, `targeting.gd` — are
**retired but kept**, each bannered with the `libfs2` successor that
overtook it and the gate that pins it. They are executable, line-cited,
gate-tested specifications of retail behavior, and `flight_model.gd`'s
constants still feed the native flight gate.

### Verification (`tests/` — 31 meson gates)

The house differential-oracle doctrine: every port is diffed against retail's
own C++ answering the same question, and every gate was *proven to bite* by
perturbing the subject and watching it go red.

- Corpus-wide: `corpus-check` (all 176 retail models through the converter
  vs `pof_dump`), `mission-check` (all install missions + the synthetic one
  through `mission2tres` vs independent Python reads), `ship-load-check`
  (every model assembled in a real Godot boot, movement replay vs the dump).
- Per-slice, GDScript era: `flight-check`, `weapons-check`,
  `waypoints-check`, `radar-check`, `sound-check`, `targeting-check`,
  `shiptbl-check`, `glb/tres/tex/manifest/tres-load/vpstage-check`,
  `pof-oracle`, `ani-check`.
- Per-slice, boundary era: `gdext-check` (the extension loads and
  round-trips), `flight-native-check` (540-frame trace through the
  boundary, tolerance **zero**), `world-check`, `training-flight-check`,
  `weapons-native-check`, `lesson-native-check`, `world-scene-check`,
  `frame-eq-check` (packed rows == snapshot records, exactly),
  `campaign-sim-check` (all 90 install missions, 3600 frames each),
  `campaign-flow-check`, `warpout-check`, `savejson-check`.

## What works right now

- **All 176 retail models** convert clean and render — geometry, hierarchy,
  textures, articulation, and every POF datum inspectable as an overlay.
- **All install missions (91/91 with the synthetic range)** extract through
  retail's parser: placement float32-exact, events/goals/messages/waypoints
  verified against the mission text.
- **The whole retail campaign simulates natively.** All 41 missions of
  `FreeSpace2.fc2` fly hands-off through `sim_dump` for 60 s of sim; all 90
  install missions survive 3600 frames in the gate. Dogfight AI, turret
  flak, missiles, shockwaves, wash, shields, subsystem damage, debris,
  arrivals and departures are all retail's own code running behind the
  boundary — nothing reimplemented.
- **Flyable, with the mission around you**: mouse flight, guns and
  missiles, targeting (hostile / escort / subsystem / weapon-bank cycling),
  shields, afterburner, match-speed, retail's warp-out staircase as a real
  departure, radio chatter in voice, training lessons with directives, and
  a functional jet-style HUD (speed tape, shield gauge, off-screen target
  chevron, closure rate, velocity vector marker).
- **Campaign flow, end to end**: goals → debrief overlay → Accept → the
  branch formula's next mission loaded in place, side loops offered and
  closed, progress saved to a real `.csg` and resumed from it. One pilot,
  "Commander Jameson". Persistence is XDG (`~/.local/share/fs2`,
  `~/.config/fs2`), with the data home shadowing the retail tree — which
  makes it a free mod mechanism.
- **Retail's own art plays**: explosion/warp/shockwave flipbooks decoded
  from `.ani` by `tools/ani2png`, laser streaks drawn retail's way, species
  thrusters, debris carved from the ship's own model, and a NASA Deep Star
  Maps panorama sky.
- **`tools/savejson`** — `.plr`/`.csg`/`.css` ⇄ JSON, byte-faithful round
  trip, for editing saves by hand.

Not crossed — and these are exactly the restart queue below: player death,
red-alert missions, promotion/medal debrief stages, and the whole briefing
chain (command brief, mission brief, ship and weapon select).

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

### Fly a mission (the boundary — this is the live path)

The simulation is native, so no mission `.tres` is involved: the presenter
hands the mission *name* to `libfs2`, which parses and runs it exactly as
the game would. What the assets directory supplies is **art only** — a GLB
per ship class by POF stem, `effects/` for the flipbooks.

Bake the whole fleet once — 176 models and the effect art. Neither tool
has a sweep flag, so the bake is a loop (verified as written, 2026-08-05):

```
mkdir -p build/glb/effects
for p in ../rundir/data/models/*.pof; do
    ./build/tools/pof2glb "$p" "build/glb/$(basename "${p%.pof}").glb"
done

# every flipbook AND every still -- a missing still falls back silently
# to a primitive, so bake both or the art lies about itself
find ../rundir/data/effects -maxdepth 1 \( -name '*.ani' -o -name '*.pcx' \) \
     -printf '%f\n' | sed 's/\.[^.]*$//' |
     xargs ./build/tools/ani2png ../rundir build/glb/effects
```

Two shell traps, both paid for twice now: **do not** build the name list
with `$(ls ...)` — the interactive `ls` alias corrupts the substitution —
and note that under zsh an unquoted `$names` does **not** word-split, so
the whole list arrives as one argument. `find | xargs` sidesteps both.

Then fly, with **absolute paths everywhere**:

```
godot --path "$PWD/inspect" -- world "$PWD/../rundir/data/missions/training-1.fs2" \
      "$PWD/build/glb" "$PWD/../rundir"
```

The three arguments are the mission, the art directory, and the game root
(WAVs, tables, everything `cfile` serves). A class with no GLB renders as
an honest gray box rather than vanishing — which also means missing art can
*masquerade* as working code, so check the pantry first when something
"doesn't look right".

Controls: **mouse** steers (captured in-window; click to re-grab), **M1** /
`LCtrl` guns, **M2** / `Space` missiles, `A`/`Z` throttle, `\` full,
`Backspace` zero, `Tab` afterburner, `T` target next, `H` next hostile,
`E` next escort, `S` next subsystem, `.` / `/` cycle primary / secondary
bank, `M` match target speed (a mode; throttle keys cancel), `V`
cockpit/chase, `Shift-Super-J` (or `Alt-J`) warp out, `Esc` quits.

### Fly the campaign

```
godot --path "$PWD/inspect" -- world campaign FreeSpace2 \
      "$PWD/build/glb" "$PWD/../rundir"
```

Campaign mode reads (and writes) the real pilot in `~/.local/share/fs2`,
resumes wherever the `.csg` left off, and ends each mission into the
debrief overlay — `Enter` accepts and loads the next mission in place, `L`
takes an offered side loop. Warp out to end a mission the intended way.

(Mission paths must be absolute. A bare name is resolved through retail's
`cfile` against the install; a *relative* path resolves against cfile's
roots rather than your shell's working directory — and `godot --path`
chdirs the engine into the project before `_ready`, which has produced this
same confusion three separate times.)

### The engine-free oracle

`sim_dump` links `fs2_t` directly and needs no Godot at all — it is the
fastest way to ask what the simulation does:

```
./build/libfs2/sim_dump ../rundir SM2-02.fs2 run 3600 1800
```

Modes: `run` (hands-off), `fire` (an aim-assist bot), `warpout`,
`campaign`.

### Run the gates

```
meson test -C build            # finds ../rundir on its own; or set FS2_GAME_ROOT
```

Gates that need the install skip cleanly (exit 77) without it.

## Restarting this branch

Mothballed 2026-08-05 with the tree clean, everything pushed, and the
suite at **31/31**. Nothing is half-applied; `meson test -C build` should
be green on the first try, and if it is not, that is news.

**Why it stopped here rather than anywhere else.** The campaign simulates
and most of it flies, but four things stand between that and a campaign a
person can actually complete. Which of them matter, and how much, is a
question the retail port answers better — so the campaign gets finished on
`master` first, and this branch restarts with that experience in hand.

The four, in the order they bite. The full census, with verified
`file:line` anchors for every claim, is in **`notes.txt`** at the branch
tip (compilation-mode; open it in Emacs and walk the anchors).

1. **Player death has no path.** `world.gd`'s only mission-end test is
   `departed`; a killed player leaves the world running. Retail's death
   arc is *already flying inside the sim* — `shiphit.cc` posts
   `GS_EVENT_DEATH_DIED`, `ship.cc` posts `GS_EVENT_DEATH_BLEW_UP` — and
   `tests/freespacestubs_gen.cc`'s `game_process_event` swallows both. The
   sim half of retail's handlers is twelve lines, and `read_player_controls`
   already zeroes the stick under `GM_DEAD` on its own. **This is the
   warp-out slice again, in the same shape**, and warp-out is the worked
   example to copy. The one genuinely new thing is `popupdead`'s Restart
   choice: the boundary has no mission-restart entry point at all.
2. **Red alert is unreachable headless — 7 of the 41 campaign missions**
   (SM1-02, SM1-03, sm3-02, SM3-03, SM3-08, sm3-09, sm3-10). The operator
   works and arms the countdown, but the state machine only *advances*
   from inside a HUD gauge painter (`hud_maybe_display_red_alert`), so
   headless the countdown is armed and never checked and the mission runs
   on forever. **This is the `game_do_training_checks` defect exactly** —
   a maintainer living in a presentation call — and it wants the same
   treatment: relocate the sim half verbatim into a pure-sim file, called
   from both `game_frame` and `step()`. Carrying hull, subsystems and
   loadout across the hop (`red_alert_bash_wingman_status`) is a second,
   separate piece.
3. **Promotion / badge / traitor debrief stages**, parked on purpose in
   `fs2.cc`'s stage-formula loop. Note that `scoring_level_close` already
   runs, so rank and medal progression *is* happening in the pilot file —
   nothing announces it.
4. **The briefing chain** — command brief, mission brief, ship select,
   weapon select. Nothing broken; simply not built. Missions fly with the
   loadout FRED authored. Largest of the four by a wide margin, least
   blocking, and the only place a player ever chooses anything.

**What travelled to `master` when this was parked:** the two `vecmat.cc`
fixes found here (the degenerate rot-axis reciprocal that aborts SM2-02,
and the `approach()` assert that a denormal `theta_goal` trips). Both are
retail bugs in shared sources, both reachable in ordinary play, and
neither had anything to do with the migration. Nothing else crossed —
the rest of this branch's `src/` delta is the `Fred_running` resurrection
for the extraction tools, the capture seams, and XDG/cfile changes, all
of which `master` deliberately does not want.

**Other things worth knowing before touching anything:**

- The gates invert the usual direction: the retired GDScript ports are
  now *oracles*, and `sim_dump` (which links `fs2_t` with no engine) is
  the ultimate one. When the presenter and `sim_dump` disagree, the
  presenter is wrong.
- `snapshot()` is no longer on the presenter's hot path — `frame()` is.
  `snapshot()` exists to keep the oracles honest, and `frame-eq-check`
  pins the two against each other exactly.
- Windowed Godot probes on an occupied desktop get vsync-throttled to a
  standstill (`window_set_vsync_mode(VSYNC_DISABLED)` in `_initialize`)
  and **steal the user's real keystrokes**. Suspect a probe-vs-reality
  mismatch before suspecting code.
- Schedule synthetic keys on `Engine.get_physics_frames()`, not `_process`
  counts: with vsync off, render outruns physics and presses land inside
  retail's pre-entry grace.
- Perturb only *committed* subjects when proving a gate bites. Reverting a
  perturbation with `git checkout` has already eaten uncommitted real work
  once.
- Reach for **valgrind first** on any nondeterministic memory suspicion.
  Statistical hunting failed on the asteroid flake; one valgrind run found
  it with 116 errors all rooted at the same line.

## Where the knowledge lives

The findings outlive the vehicle — most are engine-agnostic retail-semantics
censuses, useful to any future frontend:

- **docs/godot-migration-plan.md** — the plan, and per-slice "facts" blocks
  recording every retail behavior discovered while porting (waypoint arrival
  math, radar projection quirks, sexp timestamp overloads, thruster rules…),
  each with its retail `file:line` citation. The dated status blocks at the
  end are the day-by-day record of the GDExtension era, ending in the
  mothball block.
- **notes.txt** — the branch's rolling compilation-mode notes, frozen at
  the mothball on the campaign-completion census (the four gaps above).
  Earlier notes rotate into git history rather than accumulating; the
  2026-08-02 retail-campaign flight sweep is at `2db2903cd`.
- **docs/itches.md** — the redesign urges that surfaced mid-work and were
  written down instead of scratched.
- **docs/sexp-vm.md** — the SEXP evaluator analysis.
- **docs/pof-model.md**, **docs/pof-corpus-survey.txt** — the POF format and
  what the retail corpus actually exercises.
- **docs/ship-design-language.md** — a design direction for future original
  ships (thermodynamically honest hulls); engine-agnostic by construction.

## Legal

The source is Volition's, under the terms of its 2002 release: it may not be
sold or commercially exploited. FreeSpace 2 and its assets are the property
of their respective owners. This repository contains no game data.
