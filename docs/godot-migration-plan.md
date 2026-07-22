# FreeSpace 2 → Godot Migration Plan

> **Strategy:** preserve the retail game simulation first; replace its platform
> and presentation layers incrementally. Godot begins as a modern host for the
> existing game, not as an excuse to rewrite its behavior all at once.

## Vision

Move the retail-forward Linux port of FreeSpace 2 onto Godot while retaining
the character and behavior of the shipped game:

- retail campaign, mission, AI, and SEXP behavior remain authoritative;
- existing tables and mission data remain usable;
- visual assets move through a reproducible, inspectable conversion pipeline;
- rendering, input, audio, video, UI, and tooling become Godot-native;
- gameplay subsystems are replaced only when their behavior can be compared
  against the retail implementation.

The migration should always leave us with a running, testable game. It is a
sequence of narrow substitutions, not a flag-day rewrite.

## Guiding principles

1. **Shipped behavior wins.** The existing port and its oracles define the
   baseline, including behavior that differs from later fs2open versions.
2. **One authoritative simulation.** During migration, FS2 owns gameplay state
   and Godot presents it. Do not allow two physics or object models to drift.
3. **Convert losslessly before improving.** Preserve all POF semantics before
   changing meshes, materials, collision, or effects.
4. **Automate the pipeline.** Every retail asset should be reproducibly derived
   from the original VP archives; generated assets should not require hand
   repair.
5. **Replace at explicit seams.** Rendering, sound, input, and time become
   interfaces with well-defined ownership rather than scattered callbacks.
6. **Validate every milestone.** A phase is complete only when its output can be
   compared against the current engine or a pinned oracle.

## Target architecture

**1. Godot host**

Scenes, rendering, materials, UI/HUD, input, audio, video, particles, editor
tooling, and asset import.

↓ _Narrow C++ command, snapshot, and event API (GDExtension)_

**2. FS2 simulation library**

Game sequence, campaign, missions, SEXPs, AI, ships, weapons, objects, retail
physics and collision, tables, timing semantics, and pilot/save state.

↓ _Authoritative retail readers and deterministic converters_

**3. Retail data**

VP, POF, PCX/TGA, ANI, WAV, tables, missions, and MVE are converted into GLB,
textures, Godot resources, audio/video, and FS2 metadata.

Data flows upward from retail assets through deterministic converters. Godot
sends input and commands across the extension boundary; FS2 returns immutable
world snapshots and events for Godot to present.

### Initial ownership

| Area | Authority during migration | Eventual direction |
|---|---|---|
| Ship and weapon motion | FS2 | Keep unless a verified replacement is desirable |
| AI, missions, SEXPs, campaign | FS2 | Keep for retail compatibility |
| Gameplay collision and damage | FS2 | Replace only behind comparison tests |
| Rendering and materials | Godot | Godot-native |
| Window, keyboard, mouse, controller | Godot | Godot-native |
| Sound, music, speech, video | Godot | Godot-native |
| Menus and HUD | FS2 state, Godot presentation | Godot-native scenes and controls |
| Scene organization and tools | Godot | Godot-native |
| Asset interpretation | Existing retail readers | Offline converters/importers |

## Expected effect on codebase size

The migration will probably **increase the codebase temporarily, then make the
hand-maintained runtime moderately smaller**. It is unlikely to produce a tiny
codebase: most of the surviving FS2 source is gameplay rather than obsolete
platform machinery, and preserving retail behavior is one of the project's
goals.

The present Linux port is approximately **146–150 KSLOC**. A useful planning
estimate is:

| State | Estimated hand-maintained code | Change from today | Why |
|---|---:|---:|---|
| Current Linux port | 146–150 KSLOC | baseline | Complete gameplay plus custom platform and presentation layers |
| Peak migration | 175–220 KSLOC | +20–50% | Legacy and Godot paths coexist; adapters, converters, inspectors, and comparison harnesses are added |
| Godot-hosted retail game | 110–140 KSLOC | −5–25% | Legacy renderer, UI drawing, input, audio, video, and OS layers can be removed |
| Mature, selectively modernized game | 100–130 KSLOC | −10–30% | Duplicate adapters and transitional harnesses disappear; some data plumbing becomes Godot resources and tools |

These ranges count C++, GDScript/C#, shaders, import tools, and tests maintained
in the project. They exclude generated GLB files, converted textures/audio,
Godot import cache, vendored Godot source, and `godot-cpp` itself.

### What becomes smaller

Godot can eventually replace substantial engine-facing code:

- the software renderer and bitmap presentation path;
- SDL window, event, keyboard, mouse, and controller integration;
- OpenAL buffer, source, streaming, and listener management;
- legacy UI widgets and much of the immediate-mode menu/HUD drawing code;
- palette and screen-format machinery that exists to serve the retail renderer;
- video playback and a portion of filesystem/platform glue;
- transitional stubs and test harnesses whose only purpose was bringing the
  original executable up subsystem by subsystem.

### What remains large

The code worth preserving is also the majority of what is left:

- ship, weapon, object, AI, collision, and mission behavior;
- campaign progression and game-sequence rules;
- the parser and complete retail SEXP evaluator;
- retail physics and timing semantics;
- tables, pilot/save compatibility, localization, and gameplay data structures;
- authoritative readers used by the conversion and validation pipeline.

Godot does not make this domain logic disappear. Rewriting it in GDScript would
mostly exchange well-tested C++ for new code of similar size and much greater
risk.

### What adds new code

The migration introduces permanent code that the current port does not need:

- the GDExtension bridge and snapshot/event API;
- deterministic asset converters and manifests;
- custom Godot resources and import support;
- Godot scenes/scripts for menus, briefing, HUD, and game presentation;
- shaders and modern effects;
- cross-engine regression traces and asset inspection tools.

The peak estimate matters operationally. Old code must not be deleted merely to
keep line count down before the replacement reaches its validation gate. Code
size should fall in deliberate steps at the end of phases 4 and 5, when whole
legacy presentation subsystems become removable.

The likely final result is therefore **a somewhat thinner runtime surrounded by
better tooling**, not a dramatic reduction in total project complexity. The
larger benefit is architectural: fewer platform-specific subsystems, clearer
ownership, richer tools, and a much smaller amount of code that this project
alone must maintain as an engine.

## Physics policy

Godot has a capable 3D rigid-body physics system, but that does not make it a
drop-in replacement for FS2 physics.

FS2 ship motion is gameplay: acceleration curves, damping, rotational response,
afterburners, weapon prediction, shockwaves, collision damage, subsystem hits,
AI steering, and mission timing all depend on its current rules. Moving ships
directly to generic rigid bodies would change the feel of the game and could
alter mission outcomes.

During the initial migration:

- FS2 advances the simulation at a fixed timestep;
- FS2 remains authoritative for position, orientation, velocity, collision,
  and damage;
- Godot nodes receive the resulting transforms each simulation tick;
- interpolation may smooth presentation, but may not feed altered state back
  into the simulation.

Godot physics remains useful for secondary or new effects whose exact retail
behavior is immaterial:

- cosmetic debris;
- environmental props;
- particle collisions;
- editor visualization and ray queries;
- experimental broad-phase acceleration;
- new content explicitly designed for the new engine.

Any gameplay-physics replacement requires recorded scenarios, numeric
tolerances, and mission-level regression tests before it can become
authoritative.

## Asset migration

### POF is not merely a mesh

GLB/glTF is the preferred interchange format for visible geometry, materials,
hierarchy, pivots, and animation. FS2-specific semantics must be preserved in
glTF extras or a generated Godot resource alongside the GLB.

A POF conversion must account for:

- submodel hierarchy, origins, and pivot axes;
- detail levels and debris submodels;
- turret bases, barrels, and firing normals;
- primary gun and missile firing points;
- thruster points and normals;
- docking points and approach paths;
- fighter-bay and other AI paths;
- eye points and special points;
- subsystem locations and radii;
- shield geometry;
- bounding boxes and model radii;
- collision BSP data;
- texture slots and insignia geometry;
- destroyed-submodel relationships.

One converted ship should have an explicit shape similar to:

```text
GTF Ulysses
├── Ulysses.glb
│   ├── hull
│   ├── detail levels
│   ├── articulated submodels
│   └── debris submodels
├── textures/
└── UlyssesShipData.tres
    ├── weapon points
    ├── thrusters
    ├── docks and paths
    ├── subsystems
    ├── shield geometry
    └── retail collision data
```

The existing retail `modelread` implementation and `pof_dump` oracle are the
format authority. The converter should share that interpretation rather than
develop a second, subtly different POF reader.

### Other formats

| Retail input | Proposed output | Notes |
|---|---|---|
| VP archives | Staged source tree or direct converter input | Preserve origin and checksums in a manifest |
| POF models | GLB + FS2 metadata resource | Lossless semantic conversion |
| PCX/TGA textures | PNG or lossless modern texture source | Preserve transparency and palette semantics |
| ANI animations | Sprite sheets, frames, or Godot animation resources | Preserve frame timing and palette behavior |
| WAV/ADPCM | WAV/OGG as appropriate | Validate speech duration; it affects mission pacing |
| MVE cutscenes | Modern video container | Keep aspect ratio, timing, and audio synchronization |
| Tables | Parsed FS2 data or generated resources | Avoid prematurely translating away retail vocabulary |
| Missions/campaigns | Existing parser initially | Native authoring support is a later tooling project |

Generated output should include a manifest containing source archive, source
path, checksum, converter version, warnings, and output paths. This makes the
asset tree reproducible and auditable without committing proprietary game data.

## Simulation boundary

The central engineering task is separating the global-state game loop from its
platform and presentation calls. The first API should be deliberately small:

```cpp
bool initialize(const RuntimeConfig& config);
bool load_campaign(std::string_view name);
bool load_mission(std::string_view name);

void submit_input(const InputFrame& input);
void step_simulation(double fixed_dt);

WorldSnapshot get_world_snapshot();
HudSnapshot get_hud_snapshot();
AudioEvents drain_audio_events();
GameEvents drain_game_events();

void dispatch_ui_action(const UiAction& action);
void shutdown();
```

Snapshots should expose stable IDs and value data, not pointers into retail
globals. Commands cross into FS2; immutable snapshots and events cross out.
Rendering must never mutate gameplay state accidentally.

The boundary will require adapters for:

- clocks, timestamps, and fixed-step scheduling;
- input state and control bindings;
- bitmap/model handles;
- sound, music, and speech events;
- game-sequence and UI actions;
- logging, errors, and debug facilities;
- filesystem and user-save locations.

## Migration roadmap

### Phase 0 — Baseline and contracts

**Goal:** make the current game a measurable reference.

- Record representative pilot/menu, briefing, training, and combat flows.
- Pin transform, physics, collision, and mission-event traces.
- Document coordinate systems, handedness, units, and angle conventions.
- Define stable IDs for models, objects, submodels, textures, and sounds.
- Preserve the existing math, VP, POF, and rasterizer oracles.

**Exit gate:** a compact regression corpus can detect behavioral drift without
requiring a person to replay the campaign.

### Phase 1 — One-ship asset vertical slice

**Goal:** prove that POF semantics survive conversion.

- Convert one representative fighter to GLB plus metadata.
- Convert its textures and create matching Godot materials.
- Display articulated submodels in a Godot inspection scene.
- Overlay pivots, bounding volumes, shields, thrusters, weapon points,
  subsystems, docks, and paths.
- Compare every exported field with `pof_dump`.

Choose a model with enough structure to exercise the format, but not a capital
ship complex enough to obscure basic converter errors.

**Exit gate:** the model is visually correct and every relevant POF datum is
present, inspectable, and oracle-checked.

### Phase 2 — Automated retail asset pipeline

**Goal:** convert the complete GOG data set reproducibly.

- Batch-convert all 176 retail POFs.
- Convert textures, animations, audio, and cutscenes.
- Resolve shared texture/material identity consistently.
- Generate asset manifests and machine-readable diagnostics.
- Fail clearly on unsupported or lossy input rather than silently degrading it.
- Add headless conversion tests and representative visual snapshots.

**Exit gate:** a clean checkout plus locally supplied GOG archives can recreate
the complete generated asset tree with no manual edits.

### Phase 3 — FS2 as a simulation library

**Goal:** run retail gameplay without its renderer owning the process.

- Introduce the narrow simulation API.
- Build the gameplay core as a library suitable for GDExtension.
- Move platform time and input behind adapters.
- Replace direct draw and sound calls with snapshots or event queues.
- Make initialization and shutdown repeatable in one process.
- Keep the current executable as a comparison harness while practical.

**Exit gate:** a headless client can load a mission, advance it deterministically,
submit input, and inspect objects and mission events.

### Phase 4 — First mission rendered by Godot

**Goal:** combine converted assets with the live FS2 simulation.

- Instantiate Godot scenes for FS2 objects using stable object IDs.
- Copy authoritative transforms after each fixed simulation step.
- Add presentation interpolation without changing simulation state.
- Implement cameras, starfield, lighting, shields, thrusters, weapons, and basic
  effects.
- Route Godot input to FS2 controls.
- Render a diagnostic HUD from `HudSnapshot` data.

**Exit gate:** a representative mission can be flown in Godot and produces the
same significant mission events and object outcomes as the current port.

### Phase 5 — Presentation replacement

**Goal:** move the complete player-facing experience to Godot.

- Rebuild HUD gauges and targeting presentation.
- Rebuild menus, briefing, ship selection, weapon selection, debriefing, and
  campaign navigation.
- Route sound effects, 3D audio, music, and speech through Godot.
- Route cutscenes through Godot video playback.
- Reproduce palette-era effects intentionally with shaders where they contribute
  to the visual identity.
- Add modern resolution, scaling, accessibility, and controller support without
  changing retail defaults invisibly.

**Exit gate:** the retail campaign is playable end to end without the legacy
renderer, audio layer, or window/input code.

### Phase 6 — Selective modernization

**Goal:** simplify or replace legacy internals only where doing so is valuable.

Candidates include:

- Godot-native UI state and save presentation;
- new mission and table authoring tools;
- Godot-assisted collision broad phase;
- native resource representations for tables and missions;
- optional modern materials and effects;
- removal of legacy presentation-only subsystems;
- carefully verified replacement of isolated simulation components.

This phase is intentionally open-ended. Completion of the migration does not
require rewriting sound retail gameplay code merely because it is old.

## Validation strategy

Every conversion or substitution needs at least one of these checks:

| Layer | Validation |
|---|---|
| Binary formats | Field-by-field oracle comparison and source checksum |
| Models | Metadata diff, hierarchy check, bounding-volume overlay |
| Textures/materials | Pixel samples, transparency checks, reference captures |
| Animation | Frame count, duration, loop point, and visual comparison |
| Physics | Per-tick position/orientation/velocity traces |
| Collision | Contact pair, time, point, normal, and damage comparison |
| Missions/SEXPs | Event trace and final mission-state comparison |
| Rendering | Curated image comparisons with documented tolerances |
| Audio/speech | Event, duration, loop, and spatial-placement checks |
| Campaign | Save-state and mission-progression regression runs |

Deterministic trace comparison matters more than matching frame rate. Godot may
render at any suitable rate while FS2 advances on an explicitly controlled
simulation clock.

## Principal risks

### Hidden coupling in the retail game loop

Gameplay code calls rendering, sound, timing, and global state from many places.
The remedy is to extract interfaces incrementally while keeping the current
executable runnable—not to design a vast abstraction layer in advance.

### Lossy model conversion

A ship that looks correct may still have broken turrets, paths, docking, shields,
or subsystem targeting. Debug overlays and `pof_dump` comparisons are mandatory.

### Coordinate and timing drift

Handedness, matrix convention, unit scale, fixed timestep, and interpolation
errors can produce plausible but wrong behavior. Define these contracts before
rendering a mission.

### Accidental gameplay rewrite

Replacing physics, collision, mission parsing, and AI together would eliminate
the reference needed to validate any of them. Only one authority changes at a
time.

### Proprietary retail data

Converters and metadata schemas belong in the source repository; converted GOG
assets do not. The build should discover user-supplied data and reproduce derived
output locally.

### Scope expansion

Modern graphics, new content, and editor tooling are attractive but must not
block the first complete retail campaign. Compatibility comes before optional
enhancement.

## First deliverable

Build a **Godot POF inspection project** around a single fighter.

It should:

1. invoke a converter based on the existing authoritative POF reader;
2. produce one GLB and one FS2 metadata resource;
3. load them into a Godot scene;
4. articulate every movable submodel;
5. display selectable overlays for shields, bounds, subsystems, hardpoints,
   thrusters, docks, and paths;
6. compare exported metadata automatically with the `pof_dump` oracle;
7. document coordinate, unit, material, and naming conventions discovered in
   the process.

This vertical slice tests the asset representation, conversion machinery,
Godot import path, and debugging workflow without committing the project to a
premature rewrite. Once it is trustworthy, scaling it to the complete retail
asset corpus becomes a mechanical next step.

## Definition of success

The migration succeeds when:

- the retail campaign runs from beginning to end in Godot;
- gameplay-significant behavior remains traceably compatible with the current
  Linux port;
- all required assets are generated automatically from user-supplied retail
  data;
- the legacy platform, renderer, input, audio, video, menu, and HUD layers are
  no longer required;
- future work can improve presentation and tooling without reopening the
  original platform-port problem.

The desired result is not merely FreeSpace 2 assets displayed by Godot. It is
the retail FreeSpace 2 game, with its proven simulation intact, living behind a
modern engine boundary that can be improved safely.
