# FreeSpace 2 → [Godot](https://godotengine.org/) Migration Plan

> **Strategy:** preserve the retail simulation; replace platform and
> presentation incrementally. Godot is a modern host for the existing game, not
> an excuse to rewrite its behavior. This plan is concrete only as far as we can
> currently see — the far phases are directional, and their shapes get
> *discovered* as we build, not designed here.

## Vision

Move the retail-forward Linux port onto Godot while keeping the character and
behavior of the shipped game:

- retail campaign, mission, AI, and SEXP behavior stay authoritative;
- existing tables and mission data stay usable;
- visual assets move through a reproducible, inspectable conversion pipeline;
- rendering, input, audio, video, and UI become Godot-native, one seam at a time.

The migration always leaves us with a running, testable game. It is a sequence
of narrow substitutions, not a flag-day rewrite.

## Guiding principles

1. **Shipped behavior wins.** The port and its oracles define the baseline,
   including where it differs from later fs2open.
2. **One authoritative simulation.** FS2 owns gameplay state and its own
   timestep; Godot presents. Never two physics or object models.
3. **Convert losslessly before improving.** Preserve all POF semantics before
   changing meshes, materials, collision, or effects.
4. **Automate the pipeline.** Every asset is reproducibly derived from the
   user's own retail archives; no hand-repaired output.
5. **Replace at explicit seams, one authority at a time.** Only one thing
   changes ownership at once, so the previous version stays as its oracle.
6. **Validate every milestone.** A phase is done only when its output can be
   compared against the current engine or a pinned oracle.

## Ownership: what FS2 keeps, what Godot takes

This table is the spine of the whole migration.

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

FS2 advances the simulation and owns timing; Godot copies the resulting
transforms each tick and may interpolate for presentation, but never feeds
altered state back. FS2's variable timestep is *kept* — imposing a fixed dt
would itself be a behavior change. The clock is virtualized only so traces can
be recorded and replayed deterministically.

## The asset pipeline (near horizon — concrete)

### POF is not merely a mesh

GLB/glTF carries visible geometry, hierarchy, pivots, materials, and animation.
Everything else FS2-specific rides in a generated Godot resource beside the GLB:

- submodel hierarchy, origins, pivots; detail levels and debris;
- turret bases, barrels, normals; gun and missile points; thruster points;
- docking points and paths; eye and special points;
- subsystem locations and radii; shield geometry;
- bounding boxes, radii, collision BSP; texture slots and insignia;
- destroyed-submodel relationships.

One converted ship has an explicit shape:

```
GTF Ulysses
├── Ulysses.glb          (hull, detail levels, articulated + debris submodels)
├── textures/
└── UlyssesShipData.tres (weapon points, thrusters, docks, paths,
                          subsystems, shield geometry, retail collision)
```

The converter is a **batch, offline** tool built on the port's authoritative
readers (`modelread` / `pofer` / `libpof`) — never a second POF reader, never
on-the-fly conversion in the game loop. It emits GLB + `.tres` + a manifest
(source archive, path, checksum, converter version, warnings, output paths).

Source is **retail** data from the user's own GOG install — not community
MediaVPs. Generated assets are reproducible build output: gitignored, never
committed, never public; only the converter and its manifests are versioned. A
convenience snapshot of the generated tree is kept off-repo (odroid rsync), not
in git.

### Other formats (later)

| Retail input | Output | Note |
|---|---|---|
| VP archives | staged input | manifest with origin + checksums |
| POF | GLB + `.tres` | lossless semantic conversion |
| PCX/TGA | PNG / lossless | preserve transparency + palette semantics |
| ANI | frames / Godot anim | preserve frame timing + palette behavior |
| WAV/ADPCM | WAV/OGG | validate speech duration (mission pacing) |
| MVE | modern container | keep aspect ratio, timing, audio sync |
| tables, missions | existing FS2 parser | native authoring is a later tooling project |

## First step: single-ship inspection slice

The first deliverable touches none of the hard coupling — no mission, no
simulation, no boundary. It is `pofview` one engine over, plugged into the same
`pof_dump` oracle:

1. convert one representative retail fighter → GLB + `.tres`;
2. load it into a Godot inspection scene;
3. articulate every movable submodel;
4. overlay pivots, bounds, subsystems, hardpoints, thrusters, docks, paths;
5. auto-diff every exported field against `pof_dump`;
6. document coordinate, unit, material, and naming conventions as discovered.

Choose a ship with real structure but not a capital ship's complexity. When this
is trustworthy, scaling to the full retail corpus is mechanical.

**Exit:** the model is visually correct and every relevant POF datum is present,
inspectable, and oracle-checked.

### Slice progress

Landed on `godot` (converter half of steps 1, 5–6, plus the inspection
scene):

- **`tools/pof2glb.cc`** emits, from one invocation, the `.glb` (geometry,
  hierarchy, textured materials), a `textures/` dir of the ship's maps
  transcoded from PCX to PNG, a `.tres` ship-data resource beside it —
  weapon banks, turrets, thrusters, docks, eyes, paths, subsystems, and the
  shield mesh — **and** a `.manifest.json` accounting for the conversion:
  every source file and output with its SHA-256, the converter version, and
  any warnings. `inspect/ship_data.gd` is the resource schema (the contract
  the `.tres` targets), living at the inspection project's `res://` root — the
  single canonical, which the `.tres`'s `ext_resource` reference resolves to.
- **`inspect/`** is the inspection Godot project: `godot --path inspect --
  /abs/ship.glb` loads a converted ship at *runtime* (GLTFDocument +
  ResourceLoader on absolute paths, no import pipeline — what renders is the
  converter's raw output), articulates the `movement_type 1` submodels about
  their mapped axes, and overlays every `.tres` fact on the hull: weapon
  muzzles, turrets, thrusters, docks, eyes, AI paths, subsystem radii, the
  shield mesh, and a bounds/axes display whose "nose −Z" tripod is the
  axis-map eyeball aid. **Visually signed off 2026-07-25** (both slice
  ships): UV origin, materials and articulation confirmed by eye — the
  slice's last open verification, closing queue items 1–5.
- **Verification.** `meson test glb-check`, `tres-check`, `tex-check`,
  `manifest-check` and `tres-load-check` cross-check the outputs against
  independent oracles (`tests/check_glb.py`, `check_tres.py`, `check_tex.py`,
  `check_manifest.py`, `check_tres_load.gd`).
  All bite: the GLB check hard-fails on reversed winding, the `.tres` check
  hard-fails when any coordinate breaks the `(x, y, −z)` map, the texture
  check hard-fails on any pixel diverging from retail's decode, the manifest
  check hard-fails on any digest diverging from python `hashlib`'s, and the
  `.tres` load check hard-fails when the real engine rejects the file or the
  loaded resource breaks the schema. Every `.tres` coordinate is validated by
  a *second, independent* path — the dump un-mirrors X off libpof's memory
  frame, the emitter runs it through `to_godot()` — so agreement pins the
  axis map end to end; every texture pixel is validated against retail's own
  `pcx_read_bitmap_8bpp`.

Semantic facts the emitter had to honor (also at the bite sites in source):

- **Turrets are merged.** Retail keeps one turret per base submodel, last gun/
  missile bank winning, and drops which chunk it came from (`dump.cc`); the
  `.tres` carries that merged form, not libpof's raw banks.
- **Subsystems have no oracle.** Retail converts SPCL points against `ships.tbl`
  and `pof_dump` keeps only `$split` z, so the `.tres` `subsystems` array is
  emitted straight from libpof and is *unverified* against the oracle —
  `check_tres.py` says so rather than pretend coverage.
- **Path parents resolve by name.** A path's parent is a submodel *name*;
  retail drops a leading `$` and takes the last case-insensitive match (−1 if
  none). The `.tres` carries that resolved `sub` index.

Textures (queue item 3):

- **PCX decode is hand-rolled, not linked.** `pof2glb` stays `libpof` + `stb`
  and decodes 256-colour RLE PCX itself rather than linking the foundation's
  `pcxutils` (which would drag in cfile + SDL/OpenAL/GL + the stub apparatus).
  PCX is a frozen format, so the only risk that buys is a decode bug — closed by
  `tex-check`: `tests/pcx_dump` decodes the same maps through retail's
  authoritative `pcx_read_bitmap_8bpp` and `check_tex.py` compares pixel-for-pixel.
- **The green colour-key is dormant on ship maps.** Retail treats a palette
  entry of exactly `(0,255,0)` as transparent (`pcxutils.cc:266-274`), and the
  emitter + checker both replicate it — but *no* slice map, and none of 120
  sampled `data/maps` PCX, actually uses it (it belongs to HUD/effects art). So
  `tex-check` verifies the key never fires *spuriously* (alpha is compared, and
  is 255 everywhere on real data); the key *firing* is proven only by a
  synthetic pixel in the bite test, not by corpus data.
- **Keyword maps are skipped.** Texture names containing `thruster`/`invisible`
  are engine keywords, not files (`modelread.cc:270`); the match is
  case-sensitive as retail's `strstr` is. Those slots get a name-only material,
  no image. Every other name resolves case-insensitively (retail's cfile
  Windows-ism) to a `data/maps/*.pcx`, the model's sibling directory.

Manifest (queue item 4):

- **SHA-256 is hand-rolled, the PCX bargain again.** A frozen algorithm
  (FIPS 180-4) is not worth a dependency; `check_manifest.py` recomputes every
  digest with python's `hashlib` — an independent implementation — so the gate
  is also the oracle for the converter's own. Proven to bite: one flipped bit
  in the initial hash state fails all 18 slice digests.
- **The manifest is deliberately timestamp-free.** Same inputs through the
  same converter produce a byte-identical manifest, so regeneration is
  diffable — `manifest_check.sh` converts each slice model twice and compares
  the manifests byte-for-byte, turning principle 4 ("automate the pipeline")
  into a failing test.
- **The converter version comes from git at build time.** meson `vcs_tag`
  re-runs `git describe --always --dirty=+` on every build, so the recorded
  version can never go stale the way a configure-time constant would; a dirty
  tree is marked with a trailing `+`.
- **Warnings are part of the record.** A map that fails to resolve or decode
  still converts the rest of the ship, but the loss lands in the manifest's
  `warnings` — and the gate fails on any warning, so a slice conversion must
  be lossless. Output paths are relative to the manifest, so a converted tree
  relocates wholesale.
- **The "source archive" slot waits for VP staging.** Conversion currently
  reads loose files, so the manifest records file paths + digests; tying those
  back to the originating `.vp` archive belongs to the later VP-staging layer
  (the "Other formats" table), whose manifest will carry origin + checksums.

Inspection scene (queue item 5):

- **Loading is runtime, and that decided the texture format.** The scene
  loads through `GLTFDocument.append_from_file` + `ResourceLoader`, not the
  editor import pipeline — so it judges the converter's raw output. The cost
  surfaced immediately: Godot's *runtime* glTF path parses external images by
  mimetype and accepts only PNG/JPEG (`gltf_document.cpp:2186`; measured — a
  `.tga` uri imports fine in the editor and errors at runtime). The planned
  TGA→PNG flip stopped being optional; `pof2glb` now writes PNG and
  `check_tex.py` grew a hand-rolled stdlib PNG reader (zlib + chunk walk +
  scanline unfiltering), still independent of stb's writer. Proven to bite:
  one pixel poked through a stdlib re-encode fails at exactly that pixel.
- **POF extras come from the GLB's JSON chunk, not importer metadata.** Node
  extras survive `glTF → scene` only under the import pipeline; at runtime
  the deterministic source is the file itself. The scene reads the JSON chunk
  directly (glTF 2.0 §4.4) — and since `pof2glb` writes one glTF node per
  subobject at the same index, `nodes[i]` *is* submodel `i`, which is also
  how `.tres` submodel indices resolve to scene nodes (by name).
- **Movement axes cross the same map as the geometry.** `movement_axis` is
  POF-frame with Y/Z swapped in the encoding (`model.hh:37`); the scene maps
  `0→(−1,0,0)`, `1→(0,0,−1)`, `2→(0,1,0)` — the axis-map traps compound, and
  the Faustus panels are the live specimen (4 movables across its LODs).
- **Eyes anchor under their parent submodel.** Eye offsets are the one
  `.tres` field that is parent-relative, not model-frame, so their markers
  ride the parent's scene node. Turret fire points likewise anchor under the
  `arm` submodel — but the slice has no turrets, so that branch has never
  drawn (honest gap; the first capship converted is its trial).
- **`tres-load-check` closes the text-only gap.** `tres-check` proves every
  number with a *text* parser; `check_tres_load.gd` (headless
  `godot --script`, `--path` at `inspect/` so `res://ship_data.gd` resolves)
  proves the real VariantParser accepts the file and the loaded resource
  walks like the schema. One engine fact it must absorb: the emitter's
  `%.9g` floats can land as int-looking literals and load as `int`, so
  numeric checks accept both. Proven to bite three ways: a mangled `Vector3`
  literal, a renamed dict key, and a swapped `ext_resource` script all red.

The capship (capital01, GTD Orion — third slice ship):

- **Joining the slice closed the chunk census at 16/16** (ACEN, TMIS,
  destroyed-turret variants, radar dishes — everything the fighter/science
  pair left uncovered) and immediately flushed two latent facts the small
  ships could never tickle.
- **`jf()` silently truncated at 512 bytes.** The Orion's multi-material
  hull feeds its whole primitives list through one `%s`, overflowing the
  stack buffer and cutting the GLB's JSON mid-key — caught by `check_glb.py`
  the first time it parsed the output. `jf` now measures and reformats at
  full length; it never truncates.
- **Winding is checked as fidelity, not unanimity.** Corner order is
  consistent against the stored *face* normals 2932/2932 on the Orion too —
  but retail's smoothed *vertex* normals oppose their own facet on exactly 4
  debris polygons, and vertex normals are all the GLB carries. So
  `check_glb.py` now computes the expected per-mesh disagreement count from
  the dump with the same proxy and demands an exact match — proven to bite
  both ways (a spurious flip and a "fixed" retail flip both die).
- **No POF file stores movement type 2.** Raw census over all 176 models:
  type 1 ×807, inert 0 ×39, never 2. Retail *manufactures* ROT_SPECIAL at
  load by name (`turret*`/`gun*`/`cannon*` promoted, thrusters and
  non-subsystem rotators stripped — `loaded_movement` in libpof's dump
  replicates it). The GLB extras carry the file value; any consumer deciding
  what retail would free-rotate must replay that reclassification. The
  inspection scene knowingly spins every file-type-1 movable anyway — bases
  yaw, arms pitch — because seeing each declared axis move is the point.
- **`-destroyed` turret wrecks hide at load.** They are siblings of the live
  turrets, swapped in on subsystem death; the scene never shows both.

The corpus gate (`meson test corpus-check`):

- **All 176 retail models, every run.** Convert + `check_glb` +
  `check_tres` + `check_manifest` per model, ~35 s. Wide where the slice
  gates are deep; tex/tres-load/reproducibility stay slice-only (a godot
  boot or a pcx_dump sweep per model would cost minutes for coverage the
  slice already pins).
- **What going wide caught, immediately:** the dump prints flat-shaded
  polygons as `poly flat R G B` — the checker's `poly tex` parse missed
  them, so all-flat effect models (warphole, subspacenode, t-laser) and
  flat-bearing debris read as poly-less. And bomber05 carries a collinear
  sliver whose winding dot sits within an ulp of zero: the dump-side replay
  must snap its parsed decimals **through float32** — `%.9g` identifies a
  float32 exactly but parses to a *different* nearest-double, and that
  ~1e-10 skew flips the sign of a d≈1e-13 triangle. The expected-disagree
  computation now replays the GLB side bit-for-bit (same frame, corner
  order, summation order, float32-snapped inputs): 176/176 exact.
- **spherec is the one sanctioned warning.** A retail test model whose
  `nbackblue1` map lives in `data/effects`, not `data/maps` (survey); the
  gate passes `--allow-warnings` for it alone — digests still verify, and
  removing the allowance was the gate's bite (175/176 → red).

VP staging (`tools/vpstage`, `meson test vpstage-check`):

- **Extraction is retail's own.** `vpstage <vp-root> <out>` stages every
  model and map out of the pristine `.vp` archives *through retail's cfile*
  — the authoritative VP reader, so archive precedence and the member
  actually staged are the game's answers, not a reimplementation's. Staged
  names are lowercased (the TOCs carry mixed case — `ast01.POF` — that
  case-insensitive retail never notices and a case-sensitive pipeline
  would). All 864 files (176 models + 688 maps) come from `sparky_fs2.vp`.
- **The manifest's source-archive slot is now filled.**
  `staging.manifest.json` records per file: staged path, size, SHA-256, the
  originating archive (digested once) and the member's offset within it.
  Timestamp-free and sorted — same VPs, same manifest, byte for byte.
- **Verified three independent ways** by `check_staging.py`: hashlib
  recomputes every digest; each member is **sliced raw out of the archive**
  at the recorded offset and byte-compared (an extraction check that never
  touches cfile, so a cfile bug and a bookkeeping bug can't cover for each
  other); and every staged file must byte-match the unpacked install. Then
  the slice ships convert from the staged tree and must produce
  **byte-identical GLB + `.tres`** to a rundir conversion — the pipeline
  gives the same answer whichever door the data comes through. Proven
  bites: a tampered digest, a tampered offset, and a corrupted staged file
  (which trips all three checks at once) all red.
- **SHA-256 moved to `tools/sha256.hh`**, shared by both manifest writers;
  the slice manifest-check pinned the move (digests unchanged).

Game-side consumption (`inspect/ship.gd`, `meson test ship-load-check`):

- **`Ship` assembles a converted ship the way retail loads one.** Where the
  inspection scene deliberately spins every file-declared axis, `Ship`
  replays retail's load-time movement reinterpretation from the GLB extras:
  a type-1 submodel named `turret*`/`gun*`/`cannon*` becomes ROT_SPECIAL
  (AI-driven, never free-spinning), a `thruster*` loses its rotation,
  `$special=no_rotate` opts out, and a rotator that is not a subsystem goes
  inert — including retail's `get_user_prop_value` parsing (case-sensitive
  key match, case-insensitive value compare, C-`isspace` skipping). It
  hides LOD1+/debris/`-destroyed` wrecks and exposes the game-shaped views:
  `rotators()` (loaded ROT, axes in the Godot frame), `turrets()` with
  base/arm nodes resolved, `sub_node()`.
- **The replay is oracle-pinned corpus-wide.** One headless engine boot
  assembles all 176 models and diffs the replayed `(type, axis)` per
  submodel against `pof_dump --model`'s loaded values — retail's own C++
  answers: **2903/2903 submodels match**, and every submodel's scene node
  resolves by name (no glTF-name sanitization losses anywhere in the
  corpus). Proven bites: breaking the turret promotion or the
  `$special=subsystem` compare each go red with per-submodel diagnostics.
- **HONEST GAP:** no retail model carries `$special=no_rotate` (88 use
  `$special=subsystem`), so that branch is faithful to pofparse but pinned
  by no data.

Flight (`inspect/flight_model.gd`, `inspect/fly.tscn`,
`meson test flight-check`) — first waypoint toward flying the first
training mission:

- **The flight feel is retail's own integrator, not an approximation.**
  `flight_model.gd` ports `physics_sim` + `physics_read_flying_controls`
  (and the vecmat primitives they lean on: `apply_physics`,
  `velocity_ramp` with its close-to-goal hack, `sincos_2_matrix`,
  `vm_orthogonalize_matrix`'s exact cross order, BANK_WHEN_TURN's coupled
  `delta_bank`) function-for-function, same names, same phase structure.
  Scope is the `PF_ACCELERATES` path — no slide/afterburner/reduced-damp/
  shockwave/warp until a slice needs them. The model flies in FS2's own
  frame; only the visual transform crosses to Godot, through the same
  `(x, y, −z)` map as the geometry.
- **Oracle-pinned by trace diff.** `tests/physics_dump` runs the ported
  retail C++ over a scripted 540-frame flight (throttle ramp, pitch pulse,
  hard auto-banking turn, roll, reverse, coast) and prints the full state
  `%.9g`; `check_flight.gd` replays the same inputs through the GDScript
  port. Measured divergence: position 2.3e-4 world units, orientation
  6e-7, velocity 9e-5 — pure float32-vs-double residue, 200× under the
  tolerances, while the bites (rotdamp 0.35→0.36; flipped BANK_WHEN_TURN
  sign) blow through at 26× and 3000× over. The tolerance band separates
  float drift from semantic error by four orders of magnitude.
- **`fly.tscn` is the first flyable scene.** A `Ship` under keyboard
  control (arrows/Q/E/A/Z, stick-style pitch), chase camera, deterministic
  starfield for motion parallax. The Faustus flies with 1 free rotator —
  retail's loaded view through `Ship.rotators()` — where the inspection
  scene shows 4 file-view movables: the reclassification, live.
- **Parameters are a synthetic fighter** (`FlightModel.FIGHTER`, mirrored
  in `physics_dump.cc` — the contract is the pair) *for the oracle*; real
  ships fly on ships.tbl numbers (next block).

ships.tbl (`tools/shiptbl2tres`, `meson test shiptbl-check`):

- **The table is read by its one authoritative reader.** `shiptbl2tres`
  runs retail's `weapon_init` + `ship_init` (the real `parse_shiptbl`) and
  emits the `physics_ship_init` subset of all 113 `Ship_info` entries as a
  `ship_params.tres` (`inspect/ship_params.gd` schema), keyed by lowercased
  POF stem — including retail-derived values like
  `max_rotvel = 2π/rotation_time`. Getting the parse to run outside the
  game took three measured facts: `lcl_init(-1)` Int3s without an OS
  registry (pass `LCL_ENGLISH`), weapon parsing touches bmpman's
  timer-based bookkeeping (`timer_init`), and `Fred_running = 1` takes the
  editor's own parse-only path (no bitmap loads) with two `gr_screen`
  color-storing function pointers stubbed to no-ops.
- **Two parsers over the same bytes.** `check_shiptbl.py` re-reads the
  table text independently (python regex per field, the 2π/rotation_time
  derivation replicated in retail's float32 arithmetic, `PI =
  3.141592654f`) and the slice ships' fields must agree **exactly** on
  float32-snapped values — no tolerances. Plus a run-twice byte-compare.
  Proven bites: a tampered `rotdamp` and a 5th-decimal change to python's
  PI both red. (First bite attempt was a lesson: perturbations below
  float32 resolution don't perturb.)
- **The fly scene uses the real numbers.** `fly.gd` picks up
  `ship_params.tres` beside the GLB and the HUD names the ship; the
  synthetic FIGHTER remains the fallback and the flight-oracle contract.
  The real Ulysses corrected the synthetic guesses: `max_vel` 70 (not 65),
  `max_rear_vel` **0** — it cannot reverse — and yaw is its fastest axis.
  5 retail ships carry lateral `max_vel` (slide); `FlightModel` zeroes
  those with an honest warning until a slide slice earns its keep.

Missions (`tools/mission2tres`, `meson test mission-check`,
`inspect/mission.tscn`):

- **A mission read by its one authoritative reader.** `mission2tres` runs
  retail's `parse_main` under `Fred_running` — the editor's mode, where
  every mission object is created regardless of arrival cues and player
  starts carry `OF_PLAYER_SHIP` — and emits the ship layout
  (name/class/POF stem/team/position/orientation/player-start) as a
  `MissionData` `.tres`. The init recipe grew three entries beyond
  shiptbl2tres's: `obj_init` (ship_create walks `obj_used_list`),
  `ai_init` + `ai_level_init` (each ship claims an AI slot), and FRED's
  own `mission_brief_common_init` (the briefing parser stuffs into
  preallocated buffers). One foundation change:
  `debug_int3` honors `FS2_INT3_CONTINUE` — retail's `Int3()` was a
  *continuable* breakpoint, and `ship_make_create_time_unique` trips a
  diagnostic one legitimately when 50+ ships materialize in one
  millisecond, exactly as real FRED does and survives. The game keeps
  fail-fast. (Retail bug noted in passing: `show_ship_subsys_count`
  indexes `Ships[objp->type]` where it means `objp->instance` — harmless,
  it only feeds a high-water stat.)
- **All 41 campaign missions, gated.** `check_mission.py` re-reads each
  `.fs2`'s `#Objects` independently and demands a strict bijection with
  the `.tres`. Tolerances are tight but not zero, for a measured reason:
  the `.tres` carries the **engine's** placement, and retail post-passes
  the parse — initially-docked ships realign to dock-point geometry,
  matrices re-orthogonalize — drifting ≤3e-5 position / ≤4e-6 orientation
  from the text in 11 of 41 missions. The gate sits ~30× above that
  drift and orders below any real transform error. Bites: a shifted
  position and a flipped player-start flag both red.
- **`-- mission <tres>` is the third mode.** `mission.tscn` spawns every
  ship from its converted GLB (by POF stem, beside the mission `.tres`),
  puts the player in the player-start ship under `FlightModel` with
  ships.tbl parameters, chase/cockpit camera, and holds the rest at
  station. Training-1: you are Alpha 1 in a GTF Myrmidon
  (`fighter2t-05`), the Instructor off your port bow, four GTDR Amazon
  drones waiting downrange. No AI, no events yet — every ship the mission
  knows is present (FRED's view); arrival timing belongs to the events
  slice.

**Facts established by the events/SEXP slice (the extraction + evaluator
core):**

- The roadmap reordered itself when Training-1's own operator census was
  taken (`is-destroyed-delay` ×9, `targeted` ×3, `key-pressed` naming only
  `t`/`M`/`Tab`): events core → targeting → weapons → waypoint-AI sliver →
  game HUD, each slice feeding the next. Testing targeting/weapons can use
  a synthetic mission with inert objects.
- `mission2tres` now emits the evaluator's entire diet: events (formulas
  as canonical one-line sexp text from its own `Sexp_nodes` walker), goals,
  messages, waypoint lists, per-ship AI goals, and wings. Traps that shaped
  it: ship `$AI Goals` sexps are CONSUMED at creation (emit retail's
  decoded `Ai_info` form; `ai_clear_ship_goals` resets only `ai_mode`, so
  stale `ship_name`/`submode` must be masked by mode); wing goals never
  reach members under `Fred_running` (the copy runs at wing creation,
  which FRED skips — they ride the `wings` array with the kept
  arrival/departure cue sexps).
- The oracle corpus is the INSTALL's `data/missions` (90 files) — the
  loose `missions/` checkout genuinely differs (1.2-patched text; the
  "double spaces" that looked like tstrings.tbl lookups were a different
  source file — in the default language `lcl_ext_localize` never opens
  the table, localize.cc:484). Retail cuts every line at `;` before
  parsing and folds ß→ss; the 1.2-added missions are CRLF with multi-line
  `$Orientation:`; the whole corpus holds exactly ONE high byte (cp1252
  0x92 in SM2-06, transcoded to UTF-8 by `esc()`). `check_events.py`
  replays the ai-goal decode (mode bits, wing-name promotion to `_WING`
  modes, priority bash ≥90→89, dock/undock/disable/disarm submodes).
  Gate: mission-check now runs both checkers over all 90, 8 bite axes
  proven.
- `inspect/sexp_vm.gd` is retail's evaluator ported function-for-function
  (eval_sexp's KNOWN_TRUE/FALSE caching on cons cells, sexp_and/or/not
  propagation, eval_when side-effect actions, mission_process_event's
  chain/repeat/interval and the OVERLOADED event timestamp — ms deadline
  while repeating, `(int)Missiontime` fix once done, which
  is-event-true-delay does fix math on; bug-compat). The ms clock starts
  at a nonzero epoch: `timestamp_elapsed(0)` is never-elapsed and a zero
  clock wedges `Mission_goal_timestamp`. Comparisons/arithmetic live in
  the VM; world predicates (key-pressed, distance, facing, training-speed
  context, training-msg) in mission.gd; unimplemented ops log ONCE and
  default false (predicates) or true (actions) — the mission logs its own
  TODO list: `are-waypoints-done-delay` (AI sliver), `is-destroyed-delay`
  (weapons), `targeted` (targeting), `special-check` (training watchdog).
- Sensky SPEAKS: training messages render top-center (wrapped, Iosevka),
  directives with `$key$` tokens substituted through the BINDINGS table
  (T/M/Tab; warp-out remapped Shift+Super+J — the WM eats Alt+J), so the
  Instructor names the player's real keys, remaps included.

**Facts established by the targeting slice:**

- `inspect/targeting.gd` owns the mechanism: mission-order cycling (T),
  hostile-filtered cycling (H — the HUD toggle moved to F1 to free
  retail's key), and `targeted`'s name + held-for semantics
  (sexp.cc:6528). The `targeted` predicate is REAL now; the mission
  scene draws the target monitor's data half (name/class/range/speed/
  hull, lower-left) and brackets the target in the view via
  `unproject_position`. Match speed (M) holds station — targets are
  inert until the AI sliver.
- Gate: `targeting-check`, headless over a synthetic ship list (the
  inert-objects range, distilled) — cycle order, wraparound, hostile
  filter, case-insensitive name, held-for delay; bitten. Suite 14 gates.
- Live verification technique: the capture hook can PRESS KEYS through
  the real input path (`Input.parse_input_event` with an
  `InputEventKey`) before the frame capture — the synthetic T press
  acquired the Instructor on screen.

**Facts established by the weapons data half:** shiptbl2tres now emits
per-ship `hull` ($Hitpoints via `initial_hull_strength`) and a `weapons`
dict (velocity/damage/lifetime/fire_wait from `Weapon_info[]`, retail's
weapon_init already running in the tool); shiptbl-check compares both
against independent tbl reads, bitten. Subach HL-7: 450 m/s, 15 damage,
2 s lifetime, 0.2 s cadence — the runtime half (projectile sim,
collision, hull damage, is-destroyed-delay, the synthetic range
mission) consumes this next.

**Facts established by the weapons runtime half:**

- `inspect/weapons.gd` owns the mechanism: LCtrl holds-to-fire at the
  gun's `fire_wait` cadence, bolts inherit the shooter's velocity and
  fly ballistically for `lifetime` seconds; the scene mirrors them with
  a pooled unshaded mesh. The player's gun is the Subach by name
  (loadout/$Primary Banks extraction is a refinement).
- The collision call, made deliberately: swept segment against the
  ship's POF bounding sphere, NOT retail's model_collide BSP walk — a
  Subach bolt crosses ~7.5 m per 60 Hz frame, so a point test would
  tunnel; the swept test is exact against the sphere. Polygon accuracy
  waits for a slice that needs it.
- Kills stamp a destroyed registry with `mt_fix`, retail's mission-log
  shape: `is-destroyed-delay` (sexp.cc:3314 — ALL names down and
  `Missiontime - latest >= i2f(delay)` → KNOWN_TRUE) and `hits-left`
  (sexp.cc:3835 — `int(100*hull/initial)`, NAN_FOREVER once dead, NAN
  for a name the world never had) are REAL now; both left the stub
  list. A dead ship leaves the scene, the target cycle, and `distance`
  goes NAN for it (retail's failed ship_name_lookup).
- `invulnerable` (+Flags → OF_INVULNERABLE at ship creation,
  missionparse.cc:1275) now rides the .tres per ship: the bolt stops,
  the hull doesn't move (shiphit.cc:1687). Without it the player could
  shoot Training-1's Instructor dead. check_mission.py verifies the
  flag corpus-wide.
- `tests/weapons-range.fs2` is the proving ground: a retail-legal
  mission (Training-1's own dialect — Myrmidon + Subach vs three GTDR
  Amazon drones, 150 hull each) exercising hits-left, per-drone
  directives, and the delayed range-clear. cfile opens a path
  containing a separator directly (cfilesystem.cc:596), so mission2tres
  converts it in place from the repo — no staging into the install; the
  mission-check gate now sweeps it with the corpus (91/91).
- Gate: `weapons-check`, headless — cadence, the swept-vs-tunneling
  case (one 0.1 s step through a 10 m sphere), hull ledger, kill
  stamping, lifetime expiry, invulnerability, and the fix-math delay
  boundary; bitten both ways (endpoint-only collision: 14 fails;
  `>=`→`>`: exact boundary fails). Suite 15 gates.
- Live proof: a synthetic gunnery hook (target hostiles, aim by basis,
  hold LCtrl via `parse_input_event`) cleared the whole range on
  camera — hull % draining in the target box, directives going [done],
  `goal COMPLETE: Clear the range`.

**Facts established by the waypoint-AI sliver:**

- `inspect/waypoint_ai.gd` owns the mechanism: ships fly waypoint
  paths, stand still, or play dead; completing a path stamps a
  LOG_WAYPOINTS_DONE-shaped log (LAST waypoint only, aicode.cc:4763)
  that `are-waypoints-done-delay` reads. Retail-exact: the arrival test
  (aicode.cc:4717 — within MIN_DIST 5.0 + sqrt(radius), padded by the
  frame's travel, plus the swept segment so no ship steps over a
  waypoint), case-insensitive log names (missionlog.cc:423), the
  predicate's fix math and its destroyed-only-checked-when-not-done
  quirk (sexp.cc:3562, retail's own comment flags it). Deliberate
  approximation: the flying itself — the forward vector turns at the
  ship's table yaw rate toward the waypoint at table cruise speed
  (max_vel.z) instead of retail's full rotational-physics stack; same
  waypoints, same order, believable motion. On completion the ship
  PARKS — the original cut had it cruise straight on the assumption
  the next add-goal lands within the event cadence, but Training-1
  gates the Instructor's next path on the PLAYER's lesson progress
  (minutes, not milliseconds) and he left the training area at
  75 m/s. Field-reported, fixed, and pinned in the gate: a finished
  ship stays put.
- `add-goal` is REAL for the sliver's verbs: ai-waypoints-once /
  ai-waypoints start a path, ai-stay-still / ai-play-dead park,
  ai-guard degrades to stay-still (logged once), the rest no-op
  (logged once). Initial $AI Goals from the .tres drive ships at
  spawn — highest-priority goal wins (retail keeps prioritized slots;
  this world flies one order at a time).
- Movers update their mission entry's position in place, so distance,
  the weapons collision sweep, and the target box (live speed now) all
  read fresh positions; a kill also removes the ship from the AI.
- Gate: `waypoints-check`, headless — a dogleg path (the turn limiter
  has to work), arrival, single log stamp, re-command mid-life,
  stay-still, predicate semantics; bitten three ways (reversed turn:
  12 fails; stamp-on-every-waypoint; `>=`→`>` fix boundary). BITE
  LESSON: a negative MIN_DIST perturbation does NOT bite — the swept
  test squares the radius, and the frame-travel padding makes arrival
  robust by design; bite the log shape instead. Suite 16 gates.
- Live proof in Training-1 at 5x clock: "waypoints done: Instructor,
  Waypoint path 1" on the console, the Instructor visibly departed,
  the directives chain advanced. The remaining stub log is down to
  `special-check` and `ship-guardian`.

**Facts established by the game-HUD slice:**

- `inspect/radar.gd` is the radar gauge: the data path is retail's
  radar_plot_object exactly — contact rotated into the player frame,
  radial fraction `acos(z/dist)/pi` (ahead center, beam half-radius,
  astern rim, radar.cc:327), bearing from x/y, rim clip at radius − 5
  (radar.cc:351), screen y inverted, blips beyond the gun's reach
  drawn dim (radar.cc:376), range filter RR_INFINITY (retail's
  default, hudconfig.cc:1687). Retail quirk kept: a contact EXACTLY
  astern has indeterminate bearing and plots at the CENTER
  (the zdist < 0.01 arm, radar.cc:335). The art is lean vectors
  (rim, beam ring, crosshair, colored rects; white box = current
  target), not bitmap blips.
- The lead indicator: aim point = target + velocity × time-of-flight
  (dist / bolt speed, one iteration), shown for movers only, fed by
  the AI's velocity_of. The directives gauge carries its retail title
  and hides when empty.
- Gate: `radar-check`, headless — the cardinal projection contract,
  the acos fraction (45° = quarter radius, not linear), bearing
  splitting, distance invariance, the dead-astern quirk, lead-point
  math; bitten both ways (y-bearing flip: 2 fails; half-angle
  fraction: 6 fails). Suite 17 gates.
- KNOWN GAP (next nibble): `+Initial Hull` is a PERCENTAGE of table
  hitpoints applied at ship creation — not extracted; the weapons
  ledger starts every ship at full table hull. Every Training-1 and
  range ship declares 100, so nothing bites yet; extract
  `Objects[i].hull_strength` (already percent-applied by retail)
  when a mission needs it.

**Facts established by the sound slice:**

- The extraction chain stays authoritative: shiptbl2tres runs
  gamesnd_parse_soundstbl (pure parsing, no device) before weapon_init
  — retail's own order — so weapon sound INDICES resolve to filenames
  through retail's own Snds[]: per-weapon `launch_snd`/`impact_snd`
  (−1 and the "none.wav" placeholder both emit ""), plus a `sounds`
  dict with the fighter explosion pair (SND_SHIP_EXPLODE_1/2 = boom_3
  / boom_1; retail picks by object-index parity, ship.cc:2804 — this
  world alternates per kill). shiptbl-check verifies all of it against
  independent python reads of weapons.tbl + sounds.tbl (game-sounds
  section only — the interface section reuses the same indices);
  bitten.
- `inspect/sound.gd` (SoundBank) plays the install's own wavs
  straight: message voice on a dedicated channel (a new line cuts the
  old), effects round-robin over a small pool, names resolved
  case-insensitively over the voice trees + sounds dirs (cfile's
  Windows-ism again — table and mission names disagree with on-disk
  case throughout). Corpus census: the entire voice + effects set is
  plain 8/16-bit Microsoft PCM, which AudioStreamWAV.load_from_file
  eats directly — no transcoding, no staging. Unresolvable names log
  once and stay silent.
- The mission scene takes an optional game-root argument (or
  FS2_GAME_ROOT) for the wav tree; without it the mission runs silent
  with one log line. Voice fires when a training message's text
  appears; the gun plays launch per shot, impact per hit, boom per
  kill.
- Gate: `sound-check` — index coverage, case-insensitive loads of the
  wavs the missions actually name, polite silence modes; bitten
  (dropping the case-fold: 6 fails). Suite 18 gates.

**Facts established by the controls-and-pace pass (field reports from
the first real playthroughs):**

- Afterburner is REAL: retail's exact PF_AFTERBURNER_ON branch in the
  flight model (physics.cc:601/626/716 — the burner floors the stick,
  swaps the goal to afterburner_max_vel and the accel ramp to the
  burner's constant), flag-gated so the oracle-pinned path is
  IDENTICAL with the flag off (flight-check's replay carries no
  afterburner; its new semantic checks cover engage-reaches-AB-speed
  and tankless-never-engages, bitten via the goal swap). Tab holds it.
  Myrmidon: 75 cruise / 135 burner.
- Throttle keys: `\` max, Backspace zero (retail's bindings); M now
  matches the TARGET's current speed (one-shot throttle set; retail's
  continuous match toggle is a refinement).
- The Instructor's "very animated pace": waypoint flight commanded a
  flat max_vel.z; retail commands DISTANCE-PROPORTIONAL speed —
  dist/5 for small ships (aicode.cc:4687), clipped by the max and by
  cap-waypoint-speed (aicode.cc:4702, positive caps only, retail
  stores -1 for none). Both now ported; `cap-waypoint-speed` left the
  stub list (Training-1's Catch Up event caps him at 55). Ships glide
  in instead of charging; the target box and lead indicator read the
  live commanded speed. Bitten (divisor perturbation).

**Facts established by the match-mode and engine-glow pass (field
reports):**

- Match speed is a MODE, not a one-shot: while on, the throttle tracks
  the target's CURRENT speed every frame (the Instructor decelerates
  into his waypoints — a one-shot match drifts within seconds). M
  toggles it, any manual throttle input (A/Z/0/\/Backspace) cancels
  it, and the HUD's engine readout shows "match" while active.
- Thruster submodels are engine-gated, retail's own rule: is_thruster
  = name contains "thruster" (pofparse.cc:688), and the renderer skips
  them entirely without MR_SHOW_THRUSTERS (modelinterp.cc:1192), which
  ships pass only with the engine machinery live. Ship collects them
  hidden at load; set_thrusters() flips the lot. Player glow follows
  throttle/burner, movers' glow follows their commanded speed, parked
  and killed ships go dark. (Retail also scales the cone by
  forward_thrust with a 0.1 stub at idle — the scale refinement can
  ride a later polish pass; the field request was gone-at-zero.)
- Pinned corpus-wide in ship-load-check: all 176 models collect
  exactly their named thruster submodels, dark at load, all lit on
  set_thrusters(true); bitten (skipping the hide went red).

The training-mission road is BUILT: layout, flight, events, targeting,
weapons, waypoint AI, the HUD, and sound all live — Training-1 runs as
a lesson with Sensky's voice (stub log: special-check + ship-guardian
only). What remains is polish and the evaluator differential gate (a
retail event-trace dump against the VM's replay, physics_dump-style)
once wanted.

## Second step: the GDExtension boundary (`libfs2`)

Designed 2026-07-30, after the revival and the reunification merge. This is
the "FS2 as a simulation library" horizon item made concrete — the ownership
table enforced by an ABI. The GDScript era proved the semantics and built the
gates; this step moves execution back into retail's own machine code and
demotes the ports to oracles.

### The library

A GDExtension is a shared library Godot dlopens, described by a small
`.gdextension` file; the library registers classes with ClassDB and they
appear to GDScript as native types. Ours:

```
libfs2.so  =  foundation (the retail corpus, unchanged)
            + the tool-proven stub family (gr_*, snd as event capture, ...)
            + fs2_t, the boundary object          (engine-agnostic C++)
            + one shim TU registering class FS2   (the only file that
                                                   includes a Godot header)
```

The founding fact: **the tools already prove the sim runs headless.**
`mission2tres` and `shiptbl2tres` link all of foundation, stub the graphics
vtable, and run retail's real init and parse chains to completion. The
boundary is the same trick with a longer leash — keep calling the frame
chain instead of stopping after the parse.

Two layers, two audiences:

- **`fs2_t`** (in libfs2 proper) — a plain struct owning the world:
  `load()`, `step()`, `snapshot()`, `events()`, ordinary C++ types
  throughout, no Godot header anywhere. The library compiles without Godot
  existing, so `sim_dump` and the oracle gates link it directly — the
  boundary API is tested without an engine in the room.
- **`FS2`** (the shim TU) — the ClassDB-registered wrapper: holds an
  `fs2_t`, forwards the four calls, marshals structs to Variants.
  Deliberately boring; pure translation, no logic. GDScript reads
  `var sim := FS2.new()`; the sim side reads `fs2_t sim; sim.step(dt)` —
  each world in its own dialect, one page of glue between them.

### The API and the two flows

```
fs2_t
    load(game_root, mission, seed)   // cfile at root, tables, GAME-path parse
    step(dt, commands)               // one retail frame, minus presentation
    snapshot()                       // value-only world state
    events()                         // discontinuities, drained per call
```

Godot owns the call cadence: the glue scene steps the sim from
`_physics_process`. But **dt is an input, not a policy** — the plan keeps
retail's variable timestep, so the sim accepts whatever dt arrives exactly
as retail accepted flFrametime. In play that is Godot's steady physics
tick; in gates it is a recorded dt sequence replayed exactly. `seed` pins
the rand stream at load (the sexp caching is tuned so tight a re-rolled
rand breaks the campaign); determinism is a boundary parameter, never a
retail modification.

No callbacks from sim into engine, ever — the sim is called, answers,
returns. The one-way rule of the ownership table, enforced by the ABI:
libfs2 does not link against Godot at all.

- **Commands in — input inversion at the device layer.** Rather than
  rewriting playercontrol, the boundary writes retail's own key/joystick
  state before stepping: a virtual stick. Retail's control code runs
  unmodified, reading state it believes came from hardware. The GDScript
  bindings table survives as the Godot-side mapping from real keys to
  boundary commands.
- **Snapshots out — keyed by `object.signature`.** Retail already carries
  a stable id minted to outlive objnum reuse; the snapshot uses it as-is.
  Per object: signature, class, position, orientation, velocity, hull,
  submodel angles (turrets, rotators). A retail mission is a few hundred
  objects; a full snapshot is kilobytes per tick.
- **Events out — the discontinuities.** Object created (Godot instances
  the ship scene for that class), object destroyed, message spoken,
  directive changed, **sound requested** — the `snd_play` stubs stop
  being silent and become recorders, so audio is an event stream Godot
  plays through its own mixer.

### What native linkage buys

The GDScript line stopped at Training-1 because every subsystem had to be
transcribed. The native sim gets retail's implementations by linkage:

- **Mission loading runs the GAME path, not FRED's** — arrival cues,
  departure, reinforcements, waves all just work; `mission_eval_arrivals`
  is in foundation. (The reunification's narrow `Fred_running` stays the
  tools' private mode; the sim never sets it.)
- **Collision upgrades itself**: the swept-sphere approximation retires in
  favor of retail's real BSP `model_collide` — fidelity rises by deleting
  code.
- **The SEXP VM is the real evaluator** — `mission_process_event`, the
  KNOWN_* caching, the timestamp overloads, all of it.
- The mothball README's "not crossed" list (missiles, shields, subsystem
  damage, dogfight AI, energy management, the ~100 unported operators)
  stops being a porting backlog and becomes a **stubbing audit**: the code
  is present; the per-subsystem question is only "does it touch
  gr/snd/UI, and is that seam stubbed or evented?"

### The Godot side

`inspect/` pivots from simulating to presenting. The asset pipeline is
untouched — pof2glb's GLB + `.tres` per class — and `mission.gd` becomes a
reconciler: on `object created`, instance the ship scene for that class
keyed by signature; each tick, apply transforms and submodel angles from
the snapshot; on `destroyed`, free it (the "Missions rendered by Godot"
horizon item, verbatim). The HUD gauges keep their drawing code; their
data now comes from the snapshot. Each GDScript sim file —
`flight_model.gd`, `weapons.gd`, `waypoint_ai.gd`, `sexp_vm.gd` — retires
when its native counterpart drives the same gauge.

### Verification — the gates invert

The retired ports are not deleted; they are demoted to oracles:

- **flight-check inverts.** Today it diffs `flight_model.gd`'s replay
  against `physics_dump` (retail's integrator). The native sim IS that
  integrator, so the gate diffs the GDScript port against the boundary's
  snapshot trace — same trace format, same tolerances, roles swapped.
- **Per-slice retirement gates.** Before a GDScript sim file dies, run the
  mission under both sims and diff the observable record — positions per
  tick, event timing, directive transitions. The port was pinned against
  retail; the native sim must reproduce what the pin certified.
- **The ultimate oracle is the port itself.** The same C++ compiles into
  `fs2` and into libfs2; fed the same mission, seed, and dt sequence, the
  two must produce matching sim traces. A `sim_dump` tool (physics_dump's
  grown-up sibling, linking `fs2_t` directly) makes that a meson gate.

### Build: godot-cpp, vendored

Settled 2026-07-30: **godot-cpp**, vendored as a submodule beside libpof,
built by our meson directly — it is source files plus a python binding
generator over `extension_api.json`; a `custom_target` runs the generator
at configure time. No SCons, no CMake. The api json can be dumped from the
installed godot (`godot --dump-extension-api`) so bindings match the real
4.7.1. The raw C interface was considered and declined: it would mean
hand-writing the object model godot-cpp generates — CFront run in reverse,
visible plumbing rather than visible mechanism, and none of it ours.

### Phasing — one gate per bite

1. **Slice 0, skeleton.** Submodule + meson target + `.gdextension`;
   Godot loads libfs2 and a `version()` method returns the vcs_tag
   string. Gate: a headless script asserts the round-trip.
2. **Slice 1, flight.** One ship, native physics; commands in, transform
   out; `fly.tscn` switches to the native sim. Gate: inverted
   flight-check. Retires `flight_model.gd`.
3. **Slice 2, the world.** Game-path mission load, object list,
   snapshots; arrival cues live for the first time on this branch.
   Training-1's layout appears from the native sim; retail AI flies the
   Instructor. Retires `waypoint_ai.gd`.
4. **Slice 3, weapons + collision.** Retail `model_collide`, hull ledger
   native. Retires `weapons.gd`.
5. **Slice 4, events + HUD data.** Native SEXP processing, directives,
   messages, sounds-as-events. Retires `sexp_vm.gd` — the biggest
   transcription, replaced by the original.
6. **Then the frontier the GDScript line never reached** — arrival/
   departure missions, dogfight AI, shields, missiles — mostly by
   auditing stubs rather than writing systems.

**Exit:** Training-1 playable again, the same lesson as the mothball
milestone — but `inspect/` reduced to presentation and every game
semantic executing in the same machine code the retail port ships. The
ownership table, enforced this time.

**Status (2026-07-30, the day the plan was written):** slices 0–5 landed
and gated in one run — version round-trip, bit-exact flight, the world
(91/91 layouts, retail AI flying), the reconciler scene, native weapons
(a logged kill), the lesson (directives + voiced training messages +
key marks), and the frontier audit: **90/90 campaign missions simulate
hands-off natively** (six missing descriptions, zero missing systems).
The GDScript sims are formally retired to specs (banners in each, gates
still pinning); `mission.tscn` is the superseded era scene pending
fold-in. Open: presentation polish (regular-message exposure,
sounds-as-events, fireball/debris/warp visuals, the world scene's combat
HUD), and the campaign-flow horizon.

**Status (2026-07-31):** the polish menu landed — radio chatter through
the `Msg_capture` seam (37/90 campaign missions speak in their first ten
gate-seconds; the wave captured before the deviceless snd_load scrubs
it), the combat HUD (bracket, target monitor, radar — `radar.gd`'s art
back in service over native data, `hud_state` carrying the target
signature), and warp flashes (`fireball_is_warp` tags the records; the
positioned warp sound was already crossing). `mission.tscn`/`mission.gd`
FOLDED IN — deleted with the `-- mission` mode; `world.tscn` is the
lesson and the battlefield both. Two hunted bugs died on the way: the
"zombie drones" (a tool artifact — the aim bot's parallel gun streams
straddling a small target; fixed with a convergence sweep, and the
weapons gate now pins the mission goal), and the flaky
`is_valid_matrix` abort (retail's asteroid_create builds orientation
from stack garbage outside GM_NORMAL — load() now sets the mode before
the chain, valgrind-clean). Suite 26/26. Open: the campaign-flow
horizon (goals → debrief → next mission).

**Status (2026-07-31, afternoon):** the combat-visuals pass, chosen
over campaign flow on the argument that the user is the differential
oracle for this lane. Freight (`3633b48e3`): lasers cross tbl size +
live cycle color (the deviceless `gr_init_color` stub was LOSSY —
it dropped the rgb payload; it now records the data half exactly),
missiles cross their POF, ships cross shield quadrants + ceiling and
weapon-energy/burner-fuel, `OBJ_SHOCKWAVE` joins the snapshot with the
LIVE blast radius (`Shockwaves[].radius`), `hud_state` carries the
primary's muzzle speed. The synthetic range arms a Piranha (the
fighter-mountable shockwave carrier — detonates even on a miss) and
the fire bot ripples secondaries; the weapons gate pins all six art
crossings exactly, every pin bitten red. Art (`1b234f45d`): colored
cycling bolts, missiles as models, muzzle flashes at the birth
position, explosions as flash-core + embers + dying light timed by the
fireball record, shockwave rings riding the front, shield shimmer on
quadrant drops. HUD (`3f1d02303`): target view (own-world SubViewport,
model turning), shield/hull icons in retail's display order
(`Quadrant_xlate {1,0,2,3}` over `get_quadrant`'s octants), lead
indicator on relative motion, gun/burn gauges, RMB/Space secondary
trigger. Suite 26/26 throughout. Open: campaign flow (goals → debrief
→ next mission); field verdict on the new art pending.

**Status (2026-08-01):** campaign flow, end to end. Foundation
(`ea21ed513`): persistence went Linux-native — cfile grew its second
root (root 0 = `$XDG_DATA_HOME/fs2`, the write root; the retail tree
rides the old CD-ROM slot, so the data home shadows it = the mod
mechanism for free), the registry shim moved to `~/.config/fs2`, and
The Pilot boots with the library: Commander Jameson, read from the
`.plr` or inducted fresh. Shaken loose: retail's `MAX_PATH_LEN 128`
stack smash (bumped 256), the never-initialized anim free list
(`anim_init` joins boot — the pilot's default HUD config activates the
talking-head gauge headless), the void `game_get_default_skill_level`
trap. Boundary (`39643c5dd`): slice 3 — `load_campaign` (.fc2 +
`.csg` resume), `current_mission`, `debrief()` (retail's debrief-entry
order: fail incomplete → store goals/events → eval branch →
scoring_level_close → stage formulas), `accept()` (grants, `.csg`
save, advance, side-loop steer). The campaign-flow gate proves the arc
across three processes on a synthetic two-hop campaign staged through
the root-0 shadow, plus the retail Training-1 fail/repeat/no-save
smoke; perturbed red, restored green. Presenter (`6b9797f88`): `world
campaign <name>` form, Alt-J → debrief overlay (sim frozen, goals +
stages + verdict), Enter → accept + next mission loaded in place
(`_reset_world` strips ship nodes / art / sky, caches survive).
Suite 28/28. Open: promotion/badge/traitor debrief stages (parked,
presentation-fed), red-alert stat carry, the mission-loop brief in the
scene (boundary carries the offer; L is wired but unflown), warp-out
as a real departure (Alt-J currently ends the mission directly).

**Status (2026-08-01, afternoon):** the cockpit-calibration round
(H hostile / E escort / S subsystem / "." and "/" weapon cycles, all
retail's own keycontrol bodies through boundary control edges; the
target monitor + weapon gauge in the HUD rectangle's corners; the
subsystem frame; tests/subsys-range.fs2 with a GTFR Triton), then the
god-script split: world.gd (1866 lines, 42% of the scene code) cut to
a 734-line orchestrator plus four passive modules on the radar.gd
pattern -- fx.gd (transient art + flipbook cache), sky.gd (lights /
starfield / backdrop), hud.gd (every 2D pixel incl. the debrief
overlay), sound.gd grown (hum, positioned attenuation, exit cleanup).
Move-only commits, bodies verbatim, world-scene gate + probe shots
identical after each cut. THEN the boundary contract, measured and landed
(d1dd78c5c + 6320e5ede): identity crosses once at birth (the created
event carries the full record), frame() carries kinematics as packed
parallel arrays keyed by sig. Bench (GDScript side, live furball):
dictionaries ~11.6 us/object/frame; packed 22 us marshal + 3.4 us walk
at 15 objects -- 6.7x, ~15x marginal (a 100-object brawl: ~1.2 ms ->
<0.1 ms). snapshot() stays as the oracle path; frame-eq-check (29th
gate) pins the packed rows against it field for field. Events/config/
debrief stay dictionaries -- low volume, ergonomics win.

**Status (2026-08-02):** warp-out is a flown departure, not a debug key
(2b1d750ca + 0659f3a00). The jump key (Shift-Super-J, or retail's
Alt-J) crosses as the warp_out control edge and the boundary flies
retail's whole sequence: keycontrol's END_MISSION gates (collision
predict, engine strength), the staged autopilot via retail's own
read_player_controls (ramp to 40 km/s, hold through the effect),
shipfx's WarpMap01 hole as a live fireball record (the presenter's warp
art already knew it), stage events travelling retail's own sequencer
queue -- drained per step into freespacestubs' game_process_event,
which now carries freespace.cc's player-warpout event arc (sim
substance; camera/viewer stay out). A second press during stage 1
aborts (retail's ESC semantics). The mission ends when LOG_SHIP_DEPART
lands: hud_state carries departed (+ warpout_stage), campaign mode
enters the debrief, a lone mission ends the flight. New alongside: the
HUD-line capture seam (Hud_msg_capture at HUD_printf_line, beside
Msg_capture/Snd_capture) -- every retail HUD ticker line now crosses
as a hud_text event and joins the chatter window ('Subspace node
activated', warnings, lock feedback). warpout-check (30th gate) pins
the stage staircase, the hole, both sounds, the departure log and the
abort; bite-proven (severed stage-3 transition -> red). Remaining from
the queue: loop-brief UI flown in anger, shields on the HUD, then the
campaign itself.

**Status (2026-08-02, later):** the loop brief, flown (9f4441f70 +
11b2c96f0). Reading accept(take_loop) against retail's
loop_brief_button_pressed found the real bug: the reentry bookmark
(Campaign.loop_reentry = next_mission before the steer) was never
saved, so the loop never officially closed -- the pair rides the .csg
and mission_campaign_next_mission clears loop_enabled only at the
reentry mission. Fixed and pinned directly: sim_dump's campaign
epilogue prints retail's own loop globals, and flow.fc2 grew a
retail-shaped side loop off flow-1 (loop text + brief voice; retail's
own campaign authors text only). The flow gate's new loop arc proves
accept-detour / resume-inside-the-loop / rejoin-at-reentry across
process boundaries, 'enabled 1 reentry 2' inside and 'enabled 0' after
-- bite-proven against the severed reentry line. debrief_t carries
loop_voice. Presenter goes two-phase, retail's order: Enter accepts
INTO the loop-brief overlay (prose alone + voice), L flies the
optional mission, Enter declines onto the main line; headless probe
walks warpout -> Enter -> L on synthetic keys and lands in flow-loop.
Remaining: shields on the HUD, then the campaign itself (its two SOC
loops now reachable for real).

**Status (2026-08-02, evening):** tools/savejson (8dcbfe8d2) -- the
pilot-save codec: .plr / .csg / .css to JSON and back, byte-faithful,
so a save can be JSON-ified, hand-edited, re-encoded. Standalone
(headers only; the raw-struct blocks come from the game's own headers
so the codec tracks the compiled layout). Found along the way:
pstypes' fix is typedef long -- 8 bytes here, not retail's 4 -- making
the .plr's raw mission_time_limit dump 64-bit in this build's saves.
savejson-check (31st gate): a real hop's three saves round-trip
byte-identical, an edit survives, and retail resumes from
codec-written saves.

**Status (2026-08-02, shields):** the shield gauge (c7bd94502) -- four
arcs around a ring, nose up, fading per quadrant (retail's convention,
shield.cc:824: 0 right / 1 front / 2 rear / 3 left), own ship
bottom-center with hull integrity inside, the target's in miniature
beside the monitor in target color; live target quadrants join
target_rec. Bug fixed underneath: GLB-less box stand-ins carried
is_ship=false and fell off radar/monitor/gauge -- kind now says what
an entry IS, is_ship only whether ship art loaded. The proving ground
is tests/shield-range.fs2: weapons-range with the drones alive,
chasing, and armed with real Subach -- retail's Training gun does
LITERALLY ZERO damage (hit sounds, no drain; the range's first draft
taught it). sim_dump's shieldhit witness + two new weapons-gate pins
(quadrant dips, hull holds behind it). Next: the campaign itself.

**Status (2026-08-02, panorama sky):** the deep background is now
NASA's Deep Star Maps 2020 (SVS 4851, galactic-coordinate 4k EXR,
public domain, inspect/assets/ with credits in its README) as a
PanoramaSkyMaterial -- replacing the procedural starfield AND retail's
1998 nebula/planet backdrop bitmaps, a deliberate aesthetic call:
every mission shares one galaxy, the suns still carry the per-mission
lighting mood, and volumetric neb2 fog is a separate untouched system.
The sky feeds ambient (30% sky / 70% lifted constant -- pure-sky
ambient would be DARKER than the old flat gray), filmic tonemap +
glow; suns keep their stars.tbl lights and additive billboards.
sky.gd roughly halved (starfield, patch branch, and _sky_quad's
non-billboard half deleted). EXR loads at runtime like all art here
(no import pipeline); 145 ms/boot, noise for the gates. Suite 31/31.

**Status (2026-08-05): MOTHBALLED, mid-lane and on purpose.** Six days
after the revival the second step is essentially delivered: the retail
simulation runs behind `libfs2` with Godot presenting, all 41 campaign
missions simulate, the campaign advances mission to mission on real
outcomes, and 31 gates hold it. What remains before a person can
*complete* the campaign here is four specific things -- player death,
red-alert missions, the parked promotion/badge/traitor debrief stages,
and the briefing chain -- and the user's call was that the retail port
should answer, by being played to the end, which of them actually matter
and in what shape. So `master` becomes the live line again and this
branch waits, clean, at 31/31.

The full census with verified `file:line` anchors is `notes.txt` at the
branch tip, and the restart brief is the README's "Restarting this
branch" section. Two findings from this branch travelled to `master` on
the way out, both retail bugs in shared sources and neither related to
the migration: `vm_matrix_to_rot_axis_and_angle`'s degenerate-axis
reciprocal (aborts SM2-02) and `approach()`'s assert on a denormal
`theta_goal`. Nothing else crossed -- the remaining `src/` delta is the
`Fred_running` resurrection for the extraction tools, the three capture
seams, and the XDG/cfile work, none of which `master` wants.

The shape of the two biggest remaining pieces is already known, which is
most of why stopping here is cheap: **death is the warp-out slice again**
(retail's own events already fire inside the sim; the stubs'
`game_process_event` swallows them; `read_player_controls` honours
`GM_DEAD` unaided), and **red alert is the `game_do_training_checks`
defect again** (the state machine advances only from inside a HUD gauge
painter, so headless it never advances at all). Both have worked
examples in this branch's history to copy.

## Where this work lives

- `master` — the retail Linux port; mothballed 2026-07-30 at its
  survey-complete milestone, serving as the migration's reference
  implementation. **Live again 2026-08-05** (see the mothball status
  block above): the campaign playthrough happens there. The reunification
  merge (2026-07-30) carried its whole fix + survey campaign into this
  branch.
- `godot` branch (in this repo) — the migration: converters, the Godot
  presenter, manifests, and the libfs2 boundary. They sit next to the
  port's readers, which they depend on. **Mothballed 2026-08-05** at
  31/31 with its restart brief in the README.
- Separate repo (later) — when FS2 becomes a library behind a narrow boundary
  (below), the port graduates to a pinned dependency (submodule/subtree).

## Beyond the horizon (directional only)

Deliberately unspecified until the near work forces its hand:

- **FS2 as a simulation library.** GRADUATED — this is now the second step
  above (`libfs2`), designed after the GDScript era discovered the
  boundary's real shape, exactly as this bullet predicted it would be.
- **Missions rendered by Godot.** A mission is not a `.tscn` on disk. FS2 owns
  the mission and its object list; Godot builds a transient scene tree at runtime
  by instancing per-*type* ship scenes keyed by stable FS2 object IDs, copying
  authoritative transforms each tick. (The second step's reconciler is this,
  implemented.)
- **Presentation replacement.** HUD, menus, briefing/debriefing, audio, and
  video move to Godot once the simulation runs behind the boundary.
- **Selective modernization.** Only where it clearly pays — never rewriting
  sound retail gameplay merely because it is old.

## Validation

Every conversion or substitution needs an oracle. Near-term that is
field-by-field `pof_dump` comparison plus source checksums and manifests. As
later layers come online: metadata and hierarchy diffs with bounding overlays
for models; per-tick transform traces for physics; event-trace and final-state
comparison for missions and SEXPs; curated image comparisons with documented
tolerances for rendering. Deterministic trace comparison matters more than
matching frame rate — Godot may render at any rate while FS2 advances on its own
clock.

## Definition of success

The retail campaign runs end to end in Godot; gameplay-significant behavior stays
traceably compatible with the port; all assets regenerate from user-supplied
retail data; the legacy platform, renderer, input, audio, video, menu, and HUD
layers are no longer required. The goal is not FreeSpace 2 assets shown by
Godot — it is the retail FreeSpace 2 game, its proven simulation intact, living
behind a modern engine boundary that can be improved safely.
