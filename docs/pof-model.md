# The POF model loader — anatomy and carve plan

A deep dive on `code/model/modelread.cpp`, the reader for Volition's POF
(**P**arallax **O**bject **F**ile) 3-D model format. This document delineates
the loader into **container reader** → **geometry (BSP) bytecode** →
**consumer post-processing**, and ends with a concrete plan for separating the
file reader into a standalone unit — the model-side analogue of the SEXP carve
([sexp-vm.md](sexp-vm.md)).

Companion to the survey ([survey/04-render-models.md](survey/04-render-models.md)).
Conventions ([survey/README.md](survey/README.md)): anchors are **symbol-first**;
line hints are against `master` (the working tree) and are hints only — the
symbol is the anchor, git manages staleness. The loader is **retail-faithful**
(the format and code are Volition's, unchanged since the 2002 import), so it is
stable archaeology. The empirical oracle for everything below is
`tests/pof_dump --full <game-root> [model…]`, which parses the real loader's
output over the whole retail corpus (176 models, 0 failures).

---

## 1. What it is

POF is a **Descent-lineage IFF/RIFF-style container**: a flat sequence of
length-prefixed chunks, each tagged by a reversed-endian 4-character code
("FOURCC"). The file opens with the magic `'OPSP'` ("PSPO" reversed → *Parallax
SPec Object*), a 4-byte version, then chunk after chunk until EOF. The reader
(`read_model_file`) walks that sequence once and fills a `polymodel` struct.

The model's actual **geometry is not parsed at load**. Each subobject carries a
pre-compiled **BSP bytecode blob** (`bsp_data`) that the loader copies verbatim
off disk and never interprets. The geometry — vertices, normals, polygons,
and the BSP tree that spatially sorts them — lives entirely inside that blob and
is walked lazily, later, by three separate consumers: the renderer
(`modelinterp.cpp`), the collision detector (`modelcollide.cpp`), and the octant
builder (`modeloctant.cpp`). The blob is compiled offline by a Volition tool
("bspgen", referenced in code comments); the engine is a pure *consumer* of it.

So the format has two layers with a clean seam between them:

- **Container layer** — chunks describing the model's structure and metadata:
  subobject tree, radii/mass/inertia, weapon/dock/thruster/eye points, shield
  mesh, spline paths, insignia, textures. Read eagerly into typed struct fields.
- **Geometry layer** — the opaque per-subobject BSP blobs. Copied, not parsed.

`read_model_file` is the container reader. `pof_dump --full` is the container
reader's output **plus** a faithful re-walk of the geometry layer (the BSP
walk mirrors `model_collide_sub`), giving the complete model in text.

---

## 2. The data model

The populated struct is `polymodel` (`model/model.h: struct polymodel`). The
reader owns and fills it; every downstream system reads from it by
`model_get(num)`.

### 2.1 Top-level — `polymodel`

Grouped by what fills it:

- **Identity/version:** `id`, `version`, `filename`, `flags`
  (`PM_FLAG_ALLOW_TILING`, `PM_FLAG_AUTOCEN`).
- **Subobject tree:** `n_models`, `bsp_info *submodel` (array of size
  `n_models`), `n_detail_levels` + `detail[]`/`detail_depth[]`,
  `num_debris_objects` + `debris_objects[]`.
- **Whole-model bounds/physics:** `mins`/`maxs`/`bounding_box[8]`, `rad`,
  `core_radius` (**computed in `model_load`, not read**), `mass`,
  `center_of_mass`, `moment_of_inertia` (a `matrix`).
- **Hardpoints:** `n_guns`/`gun_banks`, `n_missiles`/`missile_banks` (both
  `w_bank`), `n_docks`/`docking_bays` (`dock_bay`), `n_thrusters`/`thrusters`
  (`thruster_bank`).
- **Misc geometry:** `shield` (`shield_info`), `n_paths`/`paths` (`model_path`),
  `n_view_positions`/`view_positions[]` (`eye`), `num_ins`/`ins[]` (`insignia`),
  `num_xc`/`xc` (`cross_section`), `num_split_plane`/`split_plane[]`,
  `autocenter`.
- **Textures (ancillary):** `n_textures`, `textures[]`/`original_textures[]` —
  bitmap-manager handles, **not** part of the geometry (see §6 on the carve).
- **Runtime-derived (not from file):** `octants[8]`, `lights`/`num_lights`
  (glow points), `debug_info`.

### 2.2 The subobject — `bsp_info`

One per submodel (`model/model.h: struct bsp_info`). File-sourced fields:
`name`, `movement_type`/`movement_axis` (with turret/thruster name-heuristics
applied at read time), `offset` (from parent), `geometric_center`, `rad`,
`min`/`max`/`bounding_box[8]`, and the two that matter most:

```c
int    bsp_data_size;   // byte length of the BSP blob
ubyte *bsp_data;        // the opaque pre-compiled geometry (copied verbatim)
```

Tree links (`parent`, `first_child`, `next_sibling`, `num_children`) and detail
links (`num_details`, `details[]`) are **stitched in `model_load`, not read from
file** — the file gives only each subobject's `parent` index; the loader derives
the rest. Remaining fields (`angs`, `blown_off`, `my_replacement`/`i_replace`,
live-debris arrays, `sii`, arc-effect arrays) are runtime instance state.

### 2.3 The geometry blob — the BSP layer

`bsp_data` is a flat, self-relative bytecode: a sequence of `[int op][int size]…`
chunks with six opcodes (`model/modelsinc.h`): `OP_EOF 0`, `OP_DEFPOINTS 1`,
`OP_FLATPOLY 2`, `OP_TMAPPOLY 3`, `OP_SORTNORM 4`, `OP_BOUNDBOX 5`. It is walked,
never rebuilt. Byte layouts in §4.

---

## 3. The container reader — `read_model_file()` (modelread.cpp:531)

Signature: `read_model_file(polymodel *pm, char *filename, int n_subsystems,
model_subsystem *subsystems)`. Called only from `model_load` (modelread.cpp:1338),
which allocates and zeroes the `polymodel`, assigns `id`/signature, then hands it
here to fill.

### 3.1 The container walk

```c
id = cfread_int(fp);                       // 'OPSP' magic, else Error
version = cfread_int(fp);                  // major*100 + minor
if (version < PM_COMPATIBLE_VERSION || version/100 > PM_OBJFILE_MAJOR_VERSION)
    return 0;                              // 1900..2199 accepted
id  = cfread_int(fp); len = cfread_int(fp);
next_chunk = cftell(fp) + len;
while (!cfeof(fp)) {
    switch (id) { … per-chunk cases … }
    cfseek(fp, next_chunk, SEEK_SET);      // len-prefixed skip: unknown chunks are free
    id = cfread_int(fp); len = cfread_int(fp);
    next_chunk = cftell(fp) + len;
}
```

The `cfseek(next_chunk)` after every case is the key robustness property: each
chunk is length-prefixed, so a chunk the reader doesn't understand (or doesn't
fully consume) is skipped exactly. The `default:` case just `mprintf`s and seeks
past. All scalar reads go through `cfile`'s `cfread_int/float/short/vector/
string_len`, which apply `INTEL_INT`/`INTEL_SHORT` byteswaps — **the on-disk
format is defined as little-endian x86** (see §6 on floats).

### 3.2 The chunks

Reversed-4CC constants in `model/modelsinc.h`. In retail read order:

- **`ID_OHDR` ("HDR2") — object header.** `rad`, `flags`, `n_models`; **allocs
  `submodel[]`**; `mins`/`maxs` (→ `model_calc_bound_box`); detail levels; debris
  objects; then **version-gated physics**: `>=2009` reads `mass` +
  `center_of_mass` + the three `moment_of_inertia` rows directly; `1903..2008`
  reads a *volume* mass and converts to area-mass (`pow(vol,0.6667)*4.65`,
  scaling inertia by `vol/area`); `<1903` hardcodes `mass=50`, identity inertia.
  Cross-sections at `>=2014`; **lights at `>=2007`** (glow points — ancillary).
- **`ID_SOBJ` ("OBJ2") — one subobject.** `n` (index), `rad`, `parent`,
  `offset`, `geometric_center`, `min`/`max` (→ bound box), `name` +
  user-property string. Applies **name/prop heuristics**: `movement_type ==
  MOVEMENT_TYPE_ROT` + name contains "turret"/"gun"/"cannon" → `ROT_SPECIAL`;
  "thruster" → `NONE`; a `$special=subsystem` prop calls `do_new_subsystem`
  (**consumer coupling**, §6); `$special=no_rotate` forces `NONE`; sets
  `is_thruster`/`is_damaged` from the name. Then: reads `nchunks` (must be 0, else
  `Error "is chunked"`), reads `bsp_data_size`, and **`malloc`s + `cfread`s the
  raw BSP blob** — the one place the geometry layer touches disk, copied verbatim.
- **`ID_SHLD` ("SHLD") — shield mesh.** `nverts` shield vertices (positions),
  then `ntris` triangles: each a `norm`, 3 vertex indices, 3 neighbor-triangle
  indices. A self-contained little mesh with adjacency.
- **`ID_GPNT`/`ID_MPNT` ("GPNT"/"MPNT") — gun/missile points.** `n` banks, each
  `num_slots` × (`pnt`, `norm`) into a `w_bank`.
- **`ID_DOCK` ("DOCK") — docking bays.** Per bay: a property string (→ `$name`,
  else `<unnamed bay X>`), spline-path indices, `type_flags` from a "cargo" name
  prefix (`DOCK_TYPE_CARGO` vs `REARM|GENERIC`), then exactly 2 slots
  (`pnt`/`norm`).
- **`ID_FUEL` ("FUEL") — thrusters.** Per bank: `num_slots`; `>=2117` reads an
  `$engine_subsystem=` prop and resolves it against `subsystems[]` to a
  `wash_info_index` (**consumer coupling**); each slot `pnt`/`norm` + a
  `radius` at `>2004` (else 1.0).
- **`ID_TGUN`/`ID_TMIS` ("TGUN"/"TMIS") — turret firing points.** Reads turret
  banks and **writes them straight into the caller's `subsystems[]`** (turret
  norm/matrix, firing points, `turret_gun_sobj`). Pure consumer coupling: if
  `subsystems` is NULL or the parent isn't found, the data is read into a
  throwaway `bogus` and discarded.
- **`ID_SPCL` ("SPCL") — special points.** Named points with props: `$split`
  planes (→ `split_plane[]`), `$special=subsystem` and `$enginelarge/$enginehuge`
  (→ `do_new_subsystem`). Consumer coupling again, gated on the name/prop.
- **`ID_TXTR` ("TXTR") — texture list.** `n` filenames; each **`bm_load`ed** into
  a bitmap handle (thruster/invisible names → -1). **The heaviest consumer
  coupling** — the file only names textures; binding them is a bitmap-manager
  concern (§6).
- **`ID_INFO` ("INFO") — debug info.** Kept only in debug builds
  (`debug_info`); the `ID_IDTA` interpreter-data case is `#if 0`.
- **`ID_GRID` ("GRID")** — ignored.
- **`ID_PATH` ("PATH") — spline paths.** Per path: `name`; `>=2002` a
  `parent_name` (leading `$` stripped) resolved to a `parent_submodel` index by
  name match; then `nverts` × (`pos`, `radius`, and a per-vertex turret-id list).
- **`ID_EYE` ("EYE") — view positions.** `num_eyes` × (`parent`, `pnt`, `norm`);
  element 0 is the player cockpit view.
- **`ID_INSG` ("INSG") — insignia decals.** Per insignia: `detail_level`,
  `num_faces`, `num_verts` vertices, world `offset`, then per face 3×
  (vertex-index, u, v).
- **`ID_ACEN` ("ACEN") — autocenter.** One `vector`; sets `PM_FLAG_AUTOCEN`.

When the loop exits, the `polymodel` is fully populated except for the derived
fields computed by `model_load` (§5). `read_model_file` returns 1.

---

## 4. The geometry (BSP) bytecode — the copy-vs-walk seam

Each `submodel[i].bsp_data` blob is a self-relative bytecode of `[int op]
[int size]…` chunks. The canonical walkers are `model_collide_sub` +
`model_collide_sortnorm` (`modelcollide.cpp`), which document the exact byte
offsets; `pof_dump --full` mirrors them (the raw-pointer macros `w()/fl()/vp()`
in `modelsinc.h` are `MODEL_LIB`-internal, so the dumper re-derives the reads).

**Driver** (`model_collide_sub`): read `op = w(p)`, `size = w(p+4)`; dispatch;
`p += size`; stop at `OP_EOF`. This is a *linear* scan of one block; the tree
structure comes from `OP_SORTNORM` chunks whose offsets point to other blocks.

- **`OP_DEFPOINTS`** — the vertex pool. `+8` nverts, `+16` offset to the packed
  vertex data, `+20` a byte per vertex giving its normal count. At `p+offset`,
  each vertex is one `vector` position followed by `normcount[n]` normal
  `vector`s (so the next vertex is `12*(normcount[n]+1)` bytes on). Polys index
  into this list by vertex number.
- **`OP_FLATPOLY`** — an untextured (flat-shaded) polygon. `+8` normal, `+20`
  center, `+32` radius, `+36` nverts, `+40..42` rgb (+`+43` pad), `+44` a
  `nverts`-long list of `(short vertnum, short normnum)` pairs.
- **`OP_TMAPPOLY`** — a textured polygon. `+8` normal, `+20` center, `+32`
  radius, `+36` nverts, `+40` texture index, `+44` a `nverts`-long list of
  `model_tmap_vert{short vertnum; short normnum; float u,v}` (12 bytes each).
- **`OP_SORTNORM`** — a BSP node (the recursion). `+36` front, `+40` back, `+44`
  prelist, `+48` postlist, `+52` online — each a **self-relative** offset (0 =
  absent) to a child block, walked in the order **pre, back, on, front, post**.
  At `>=2000` a bounding box at `+56`/`+68` is used for ray/frustum rejection.
- **`OP_BOUNDBOX`** — a `min`/`max` box at `+8`/`+20`, used by consumers to cull
  (collision ray-box, render frustum + light-filter push/pop).

Because the SORTNORM children are reached only through offsets and each block
ends in `OP_EOF`, the tree partitions the polygons — a faithful walk visits each
leaf exactly once. `pof_dump --full` emits DEFPOINTS vertices (with their
normals), every FLAT/TMAP face (vertnum/normnum, rgb or tmap+uv), the SORTNORM
structure, and BOUNDBOXes, in engine visit order.

**This is the carve seam.** Everything in §3 that fills typed struct fields is
the *reader*; the `bsp_data` blob is *copied, never parsed*. The parser/consumer
line runs exactly along "copy `bsp_data`" (reader) vs "walk `bsp_data`"
(renderer/collision/octant). It is also the natural oracle boundary against
pcs2, which reads and rewrites this same bytecode.

---

## 5. Consumer post-processing — `model_load()` after the read

Once `read_model_file` returns, `model_load` derives everything the file does
*not* store directly (`modelread.cpp:1390`+):

- **Destroyed-model links:** for each submodel, find a `<name>-destroyed`
  sibling → set `my_replacement`/`i_replace`.
- **Live-debris links:** find `debris-<name>` siblings → fill `live_debris[]` /
  `is_live_debris`.
- **Family tree:** `create_family_tree(pm)` walks the flat `parent` indices and
  stitches `first_child`/`next_sibling`/`num_children` into a real tree.
- **Detail levels:** submodels whose names differ by exactly one character in the
  detail-letter position are linked as lower-detail mirrors (`details[]`).
- **Octants:** `model_octant_create(pm)` — spatial acceleration structure built
  by walking the BSP (a geometry *consumer*).
- **`core_radius`:** half the smallest hull bounding-box dimension, enlarged to
  contain any cockpit eye point.

None of this reads the file; it all derives from the already-populated struct.
For a pure file reader, this is consumer logic that stays behind.

---

## 6. Rare knowledge / gotchas

- **Floats are not byteswapped.** `cfread_int`/`cfread_short` apply
  `INTEL_*` swaps, but `cfread_float` reads raw (a code comment flags byteswap as
  unhandled). The BSP blob is not swapped at all — it is walked in place via
  raw pointer casts. So the format is **hard little-endian**; a big-endian port
  would have to swap floats and the entire BSP bytecode by hand.
- **Version gating is pervasive.** Min 1900; features key at
  1903/2000/2002/2004/2007/2009/2014/2117. A faithful reader must branch on
  `pm->version` (mass formula, cross-sections, lights, path parents, thruster
  radius, engine-wash props, SORTNORM bounding boxes). The retail campaign
  corpus is almost all 2116/2117.
- **Semantics smuggled through free-text props.** Subsystems, turrets,
  no-rotate, split planes, dock names, engine-wash bindings are all encoded as
  `$`-prefixed strings in per-chunk property fields, parsed with `strstr` +
  `get_user_prop_value`. The geometry format proper knows nothing about ships.
- **Name heuristics mutate movement type at read time** (turret/gun/cannon →
  `ROT_SPECIAL`, thruster → `NONE`). A reader that dropped these would change
  behavior even though it "only reads the file".
- **`nchunks` must be 0.** SOBJ reads a legacy chunk count and `Error`s if
  nonzero ("is chunked. See John or Adam!") — a dead multi-BSP-per-subobject
  path never used by retail data.
- **The blob is compiled, not authored.** Geometry is pre-baked BSP bytecode
  from "bspgen"; the engine only ever walks it. This is why the reader can copy
  it opaquely and why pcs2 (an editor) needs its own BSP compiler to write POFs.

---

## 7. Carve plan — separating the file reader

### 7.1 What the reader *is* (the movable set)

`read_model_file` plus the struct definitions (`polymodel`, `bsp_info`,
`shield_info`, `w_bank`, `dock_bay`, `thruster_bank`, `model_path`, `eye`,
`insignia`, `cross_section`) and the `cfread_*` primitives. The clean cut line,
as with SEXP: **"produces a fully-populated `polymodel` (including the opaque
`bsp_data` blobs) and nothing else."** Everything past that — `model_load`'s
post-processing (§5), all of `modelinterp.cpp`, `modelcollide.cpp`,
`modeloctant.cpp` — is a geometry/tree consumer.

### 7.2 What it depends on (and how to sever) — the hard part

Unlike the SEXP reader (which touched *zero* game state), `read_model_file` has
three real couplings into game systems, all inside otherwise-pure chunk cases:

1. **Textures — `ID_TXTR` calls `bm_load`.** The file only names textures; a
   pure reader should record the **names** (into `polymodel`) and defer binding.
   Sever: split texture *naming* (reader) from texture *binding* (a post-load
   consumer step, or a `bm_load` seam function the caller supplies).
2. **Subsystem binding — `ID_SOBJ`/`ID_SPCL`/`ID_FUEL` call `do_new_subsystem`
   and write into the caller's `model_subsystem *subsystems`.** This ship-table
   binding is consumer logic, not file parsing. Sever: the reader can parse the
   `$special`/`$engine_subsystem` props into the struct (or a neutral list) and
   leave subsystem binding to the caller.
3. **Turret firing points — `ID_TGUN`/`ID_TMIS` write into `subsystems[]`.** Same
   shape: the reader produces the raw turret data; binding is the caller's.

These are exactly the seams to define. The reader's other dependencies are
benign: `cfile` (the lexer/IO layer, shared), the `vecmat` helpers
(`model_calc_bound_box`, `vm_*`), and `Error`/`Warning`/`Assert`.

### 7.3 Proposed file layout (retail-idiomatic, minimal churn)

Match the existing style (lowercase filenames, `char*`, C-ish idiom; a faithful
lift, not a modern rewrite):

- `model/modelread.cpp` — keeps `model_load` and the §5 post-processing, plus the
  system's table (`Polygon_models[]`, `model_get`, etc.).
- `model/model_reader.cpp` — **the carved unit**: `read_model_file` + the chunk
  cases, producing a populated `polymodel`. Texture/subsystem/turret bindings
  reached through a small seam (caller-supplied, or deferred to a post step).

The seam surface is narrower than it looks: three callbacks (or a deferred
data list) for the three couplings above; everything else is plain struct-fill.

### 7.4 The oracle (already built)

`tests/pof_dump --full <game-root> [model…]` is the model-side analogue of
`sexp_dump --trees`: it runs the **real** loader (foundation-linked) and dumps
the whole model plus the walked BSP geometry, deterministically. Over the retail
corpus it is **176 models, 575,866 lines, zero load failures, zero bad chunks**.

For the carve, this is the before/after diff: relocate the reader, then
`pof_dump --full` must produce byte-identical output over the whole corpus.
Later, when pcs2's parse is ready, the same dump becomes the cross-implementation
oracle — harmonize the two tools' output and diff.

### 7.5 Stages

0. **Delineate** — this document. *(done)*
1. **Sever the three seams** — turn `bm_load`/`do_new_subsystem`/turret writes
   into caller-supplied seam calls or a deferred post-load step, *in place* in
   `modelread.cpp`, verifying `pof_dump --full` stays byte-identical. Reversible;
   no file moved yet. This is the real work — the SEXP carve had no equivalent.
2. **Physical split** — move `read_model_file` + chunk cases into
   `model/model_reader.cpp`, add to the meson target, verify the game builds and
   `pof_dump --full` is byte-identical before/after.
3. **Cross-oracle against pcs2** — once pcs2 parses POFs, harmonize the dump
   formats and diff the two independent readers over the corpus.

Stage 1 is the decision point: it changes how `modelread.cpp` binds textures and
subsystems (living code touched by the game), so it wants the same care the SEXP
stage-2 split got — one seam at a time, oracle green after each.

---
*Status: delineation (stage 0) complete. The reader is understood end-to-end and
the oracle (`pof_dump --full`) is built and green on the full retail corpus.
Stage 1 (seam severing) is next; the physical split and pcs2 cross-oracle follow.*
