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

Still ahead on the training-mission road: targeting, weapons, the
waypoint-AI sliver, and the game HUD (radar, target box, directives
gauge proper) — at which point Training-1 is not just a place but a
lesson. The evaluator itself still wants a differential gate (a retail
event-trace dump against the VM's replay, physics_dump-style) once the
world predicates firm up.

## Where this work lives

- `master` — the retail Linux port, its own line, formatted uniformly at the
  branch point so migration diffs never carry format churn.
- `godot` branch (in this repo) — the migration's early phases: converter,
  inspection project, manifests. They sit next to the port's readers, which they
  depend on.
- Separate repo (later) — when FS2 becomes a library behind a narrow boundary
  (below), the port graduates to a pinned dependency (submodule/subtree).

## Beyond the horizon (directional only)

Deliberately unspecified until the near work forces its hand:

- **FS2 as a simulation library.** A narrow command/snapshot/event boundary lets
  a headless client load a mission, step it, submit input, and read back
  immutable snapshots. The boundary's exact shape is *discovered* here, not
  designed now — no vast abstraction built in advance.
- **Missions rendered by Godot.** A mission is not a `.tscn` on disk. FS2 owns
  the mission and its object list; Godot builds a transient scene tree at runtime
  by instancing per-*type* ship scenes keyed by stable FS2 object IDs, copying
  authoritative transforms each tick.
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
