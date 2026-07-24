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

Landed on `godot` (converter half of steps 1, 5–6; the Godot scene is still to
come):

- **`tools/pof2glb.cc`** emits, from one invocation, the `.glb` (geometry,
  hierarchy, textured materials), a `textures/` dir of the ship's maps
  transcoded from PCX to TGA, **and** a `.tres` ship-data resource beside it —
  weapon banks, turrets, thrusters, docks, eyes, paths, subsystems, and the
  shield mesh. `tools/ship_data.gd` is the resource schema (the contract the
  `.tres` targets; the inspection project will keep a copy at its `res://` root).
- **Verification.** `meson test glb-check`, `tres-check` and `tex-check`
  cross-check the outputs against retail oracles (`tests/check_glb.py`,
  `check_tres.py`, `check_tex.py`). All bite: the GLB check hard-fails on
  reversed winding, the `.tres` check hard-fails when any coordinate breaks the
  `(x, y, −z)` map, the texture check hard-fails on any pixel diverging from
  retail's decode. Every `.tres` coordinate is validated by a *second,
  independent* path — the dump un-mirrors X off libpof's memory frame, the
  emitter runs it through `to_godot()` — so agreement pins the axis map end to
  end; every texture pixel is validated against retail's own `pcx_read_bitmap_8bpp`.

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
