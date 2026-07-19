# Cluster 04 — Render & models

Subsystems: **graphics**, **render**, **model** (the **POF** parser), **lighting**,
**bmpman**, **palman**, **tgautils**, **pcxutils**, **starfield**, **nebula**,
**particle**. The visual pipeline: file→bitmap→3D transform→rasterizer, plus the
model loader. See [README](README.md) for conventions.

> **Phase-3 target.** `model/modelread.cpp: read_model_file()` is flagged for
> extraction, to be oracled against the sibling **PCS2** POF editor. The carve
> seam is identified below under *THE CARVE SEAM*: the parser produces a fully-
> populated `polymodel` (including the opaque `submodel[i].bsp_data` blobs) and
> nothing else; everything past that is runtime consumer. The BSP bytecode is
> copied verbatim, never rebuilt at load — so the seam runs exactly along
> "copy `bsp_data`" vs "walk `bsp_data`".

---

### model  (~7,050 SLOC master; modelread.cpp ~2,760, modelinterp.cpp ~2,300, modelcollide.cpp ~800, modeloctant.cpp ~330, model.h ~1,000)
- **Purpose:** Load Parallax Object Format (POF) files into an in-memory `polymodel`, then render, collide, and query them at runtime.
- **Entry points:**
  - `model/modelread.cpp: read_model_file()` — the raw POF→`polymodel` chunk parser (the extraction target).
  - `model/modelread.cpp: model_load()` — slot manager: dedup by filename in `Polygon_models[]`, allocs `polymodel`, assigns `Model_signature`-based id, calls `read_model_file()`, then post-processes (destroyed/live-debris links, octant build, radius).
  - `model/modelinterp.cpp: model_render()` → `model_really_render()` → `model_interp_subcall()` → `model_interp_sub()` (BSP walk for drawing).
  - `model/modelcollide.cpp: model_collide()` → `model_collide_sub()` (BSP walk for ray/sphere collision).
  - `model/modeloctant.cpp: model_octant_create()` / `model_which_octant()` — spatial pre-sort of vertices into 8 octants.
- **Core state:**
  - `model.h: struct polymodel` — top-level: `version`, `flags`, `n_models`, `bsp_info *submodel`, detail levels, `mins/maxs/bounding_box`, `mass/center_of_mass/moment_of_inertia`, `w_bank *gun_banks`/`missile_banks`, `dock_bay *docking_bays`, `thruster_bank *thrusters`, `shield_info shield`, `model_path *paths`, `eye view_positions[]`, `insignia ins[]`, `bsp_light *lights`, `model_octant octants[8]`, cross-sections `xc`, `split_plane[]`.
  - `model.h: struct bsp_info` (the submodel) — `name`, `movement_type`/`movement_axis`, `offset` (from parent), `geometric_center`, `rad`, `min/max/bounding_box[8]`, and crucially `int bsp_data_size; ubyte *bsp_data;` (the raw per-submodel BSP blob copied verbatim from disk), plus runtime-only fields: tree links (`parent`/`first_child`/`next_sibling`/`num_children`), `angs`, `blown_off`, `my_replacement`/`i_replace`, live-debris arrays, `submodel_instance_info *sii`, electric-arc effect arrays.
  - `Polygon_models[MAX_POLYGON_MODELS]` global slot table; `Model_signature`.
- **Mechanism — chunk reader loop** (`read_model_file`, symbol-anchored):
  - Header: reads `id = cfread_int` and asserts `=='OPSP'`; then `version = cfread_int` (major*100+minor); gate `version < PM_COMPATIBLE_VERSION(1900) || version/100 > PM_OBJFILE_MAJOR_VERSION(21)`.
  - Loop shape: `id = cfread_int; len = cfread_int; next_chunk = cftell(fp)+len; while(!cfeof){ switch(id){…}; cfseek(fp, next_chunk, SEEK_SET); id=cfread_int; len=cfread_int; next_chunk=cftell()+len; }`. **Every chunk is length-prefixed and the loop unconditionally seeks to `next_chunk`**, so unknown chunks are skipped safely (`default: cfseek(fp,len,SEEK_CUR)`), and a chunk body that under-reads is corrected by the seek.
  - Chunk-ID dispatch (IDs are 4-char little-endian FOURCCs, defined reversed in `modelsinc.h`, e.g. `ID_OHDR '2RDH'`, `ID_SOBJ '2JBO'` for the FS2 format vs `'RDHO'/'JBOS'` for FS1, guarded by `FREESPACE2_FORMAT`):
    - `ID_OHDR` (HDR2): radius, flags, `n_models`, allocs `submodel[]`, mins/maxs, detail levels, debris objects, then version-gated physics (`>=2009` reads mass + moment_of_inertia matrix directly; older computes area-mass from volume via `pow(vol,0.6667)*4.65`), cross-sections (`>=2014`), lights (`>=2007`).
    - `ID_SOBJ` (OBJ2): per-submodel `n`, radius, `parent`, `offset`, `geometric_center`, min/max, name + user-properties string; parses `$special=subsystem`/`no_rotate` props to set `movement_type`; **reads `bsp_data_size` then `cfread`s the raw BSP blob into `submodel[n].bsp_data`** (nchunks must be 0 or Error "is chunked").
    - `ID_TXTR`: texture filename list → `bm_load()` each (note: couples parser to bmpman; thruster/invisible names get `-1`).
    - `ID_SHLD`: shield mesh — nverts vectors then ntris of {normal, 3 vert indices, 3 neighbor indices} → `shield_info` (`shield_vertex`, `shield_tri`).
    - `ID_GPNT`/`ID_MPNT`: gun/missile banks (`w_bank`: num_slots × {pnt, norm}).
    - `ID_DOCK`: docking bays (props `$name`, spline path indices, exactly 2 slots).
    - `ID_FUEL`: thruster banks (per-slot pnt/norm/radius; `>=2117` parses `$engine_subsystem=` prop and resolves engine-wash index).
    - `ID_TGUN`/`ID_TMIS`: turret firing points, matched back to `model_subsystem` by `subobj_num` — **note this writes into the caller-supplied `subsystems[]`, a coupling to ship-table data outside the pure file model.**
    - `ID_SPCL`: special points ($split planes, $special=subsystem, $enginelarge/huge) → `do_new_subsystem()`.
    - `ID_PATH`: named spline paths with parent submodel resolution, per-vertex radius + turret guard ids.
    - `ID_EYE`, `ID_INSG` (insignia: verts, offset, faces with per-vertex u/v), `ID_ACEN` (autocenter, sets `PM_FLAG_AUTOCEN`), `ID_INFO` (debug only), `ID_GRID` (no-op).
- **On-disk handling / endianness:** all scalar reads go through `cfile/cfile.cpp: cfread_int/cfread_float/cfread_short/cfread_vector/cfread_string_len`. Ints/shorts pass through `INTEL_INT`/`INTEL_SHORT` (`globalincs/pstypes.h`) which is identity on little-endian and `SWAPINT/SWAPSHORT` otherwise — **so the format is defined as little-endian x86 on disk.** Floats are read raw (a comment flags byteswap as unhandled). Chunk skipping is purely via `len`-prefixed `cfseek`. The per-submodel `bsp_data` is NOT byteswapped on read — it is walked in place later (see BSP below), so BSP internal ints are consumed via the raw-pointer macros `w(p)=*(int*)p`, `fl(p)`, `vp(p)` from `modelsinc.h`.
- **BSP tree (interpretation):** The `bsp_data` blob is a flat, self-relative bytecode with 6 opcodes (`modelsinc.h`): `OP_EOF 0`, `OP_DEFPOINTS 1`, `OP_FLATPOLY 2`, `OP_TMAPPOLY 3`, `OP_SORTNORM 4`, `OP_BOUNDBOX 5`. Each chunk is `[int op][int size]…`; walkers step `p += chunk_size`. `model_collide.cpp` documents the exact byte layouts (headline for oracling against PCS2):
  - DEFPOINTS `+8 nverts, +16 offset, +20 per-vertex normal-count bytes`, vertices packed as vector + N normals.
  - FLATPOLY `+8 normal, +20 center, +32 radius, +36 nverts, +40 rgb, +44 nverts×int vertlist`.
  - TMAPPOLY `+8 normal, +20 center, +32 radius, +36 nverts, +40 tmap_num, +44 nverts×model_tmap_vert(vertnum,u,v)`.
  - SORTNORM `+36 front, +40 back, +44 prelist, +48 postlist, +52 online offsets` (self-relative child offsets — this is the actual BSP recursion; `model_collide_sortnorm` recurses into `p+offset`), plus `+56/+68` bounding box (version `>=2000`) used for ray-box rejection. `model_interp_sub` mirrors this for rendering and adds `OP_BOUNDBOX` frustum/light-box culling.
- **Rare knowledge:** POF is a Descent-lineage IFF-style length-prefixed FOURCC container with reversed 4CC constants; the geometry is a pre-baked BSP bytecode (compiled offline by "bspgen", referenced in comments) rather than raw meshes — the engine never rebuilds it, only walks it. Version gating is pervasive (1900 min; features keyed at 1903/2000/2002/2004/2007/2009/2014/2117), so a faithful parser must branch on `pm->version`. Subsystem/turret/thruster semantics are smuggled in via free-text `$`-prefixed user-property strings parsed with `strstr`.
- **Deps:** `cfile` (all IO + endianness), `bmpman` (`bm_load` inside TXTR), `math/vecmat` (`vector`, `matrix`, `vm_*`), ship-side `model_subsystem`/`do_new_subsystem` (table coupling), `render/3d` (`g3_*` for interp), `lighting` (interp light filters).
- **Extraction seams — THE CARVE SEAM:** The reusable parser is `read_model_file()` plus the struct definitions (`polymodel`, `bsp_info`, `shield_info`, `w_bank`, `dock_bay`, `thruster_bank`, `model_path`, `eye`, `insignia`) and the `cfread_*` primitives. The clean cut line is **"produces a fully-populated `polymodel` (including the opaque `submodel[i].bsp_data` blobs) and nothing else."** Everything past that is runtime consumer: `model_load()`'s post-processing (destroyed-model linking, live-debris, octant build via `modeloctant.cpp`), all of `modelinterp.cpp` (rendering/`g3_*`), all of `modelcollide.cpp` (BSP ray/sphere walk), docking/path/subsystem lookups. Concrete impurities to sever for a pure extraction: (1) the `ID_TXTR` case calls `bm_load()` — a pure parser should record texture *names* and defer binding; (2) `ID_TGUN/ID_TMIS/ID_SPCL/ID_FUEL` write into the caller's `model_subsystem *subsystems` and call `do_new_subsystem()` — that ship-table binding is consumer logic, not file parsing; (3) `#ifndef NDEBUG` subsystem-dump file `ss_fp` (guarded `#if 0`). The BSP bytecode itself is parsed nowhere at load time — it is copied verbatim, so the parser/consumer seam runs exactly along "copy `bsp_data`" vs "walk `bsp_data`". This is the natural oracle boundary against PCS2's POF read/write.
- **Port notes:** `read_model_file` is retail-faithful `[retail]` — same `'OPSP'` magic, same chunk loop, same `ID_ACEN` tail and `cfseek(fp,next_chunk,SEEK_SET)` (verified against `retail:code/model/modelread.cpp: read_model_file()` at retail line ~1197). Master file is shorter overall (2757 vs retail 3437 lines) — trimming is elsewhere in the file, not the parser. `FREESPACE1_FORMAT` path is `#if`-compiled out (`FREESPACE2_FORMAT` active). The old `ID_IDTA` interpreter-data chunk is commented out.

---

### graphics  (~10,290 SLOC master)
- **Purpose:** Low-level 2D/rasterization abstraction (`gr_screen` function-pointer vtable) with two live backends: the retail 8bpp/16bpp software rasterizer and a revived OpenGL backend.
- **Entry points:** `graphics/2d.cpp: gr_init()` — selects backend, fills `gr_screen.gf_*` pointers; branches to `gr_soft_init()` (`grsoft.cpp`) or `gr_opengl_init()` (`gropengl.cpp`) when `Cmdline_opengl` set. `graphics/grsoft.cpp: gr_soft_init()` wires `gr_screen.gf_tmapper = grx_tmapper`. Texture-mapper pipeline: `tmapper.cpp: tmapper_setup()` → per-poly `grx_tmapper` → `tmap_scanline` function-pointer → the `tmapscanline*.cpp` inner loops.
- **Core state:** `gr_screen` (global vtable + mode/res). `grsoft.cpp: Soft_buffer` (the 8bpp framebuffer, `ubyte*`), `Soft_palette[768]`. `tmapper.cpp: tmapper_data Tmap` (the whole texture-mapper register file: `FixedScale=65536.0f`, `FixedScale8`, per-scanline `l`/`deltas` gradients, `uv_delta[]`, `DeltaUFrac/DeltaVFrac`). `tmap_scanline_table[TMAP_MAX_SCANLINES]` dispatch by TMAP_FLAG combination. gropengl: `tcache_slot_opengl Textures` cache.
- **Mechanism:** `gr_*` calls indirect through `gr_screen.gf_*`. Software path scan-converts polys into per-flag specialized scanline routines (flat/gouraud/textured/RAMP/RGB/tiled). The mapper builds 16.16 fixed-point u/v gradients per span and walks texels with integer carry arithmetic.
- **Rare knowledge (mining value):** `tmapscanline.cpp` is the pre-GPU affine/perspective texture-mapper inner loop. `tmap_uv_step()` implements the classic x86 trick of stepping a 0.32 fixed-point u/v fraction and using the add-carry to advance whole texels — the *original x87/asm is preserved in comments* (`add ecx,DeltaVFrac / sbb ebp,ebp / adc esi,uv_delta[4*ebp+4]`) and the C notes "perspective mappers ran the FPU in 24-bit precision mode; fistp rounded to nearest, hence lrintf." Perspective correction divides `1/w` once per span (`z_left = 1.0f/one_over_z`). Tiled variants (`tmapscantiled16/32/64/128/256.cpp`) special-case power-of-two textures with mask stepping; there's a hand-specialized "subspace effect" inner loop. `Light_table[256]` precomputes `i/255`.
- **Deps:** `bmpman` (source textures), `palman` (8bpp palette + blend tables), `math/floating` (fixed/float), `io` for surface present.
- **Port notes:** Heavily port-touched. `gropengl.cpp` header comment: "Retail shipped this file as an unwired skeleton: gr_opengl_* stubs" — the OpenGL backend was **revived** and made opt-in via the `-opengl` flag (`cmdline.cpp: opengl_arg`, `Cmdline_opengl`), using SDL2 (`SDL2/SDL_opengl.h`, `SDL_GLContext`). The retail Direct3D (`grd3d`) and Glide (`grglide`) backends are **deleted** — only soft + GL remain; `2d.cpp` calls only `gr_soft_*`/`gr_opengl_*`. Retail names kept as aliases for parity (`lpDibBits` aliases `Soft_buffer`, comment "retail name kept for gr_soft_init"). The asm scanlines were rewritten to portable C with the original asm retained as documentation.

---

### render  (~3,000 SLOC master)
- **Purpose:** The 3D transform/clip/project pipeline (`g3_*`) sitting between world geometry and the 2D rasterizer.
- **Entry points:** `render/3dsetup.cpp: g3_start_frame()`/`g3_end_frame()`, `g3_set_view_matrix()`. `render/3dmath.cpp: g3_rotate_vertex()`, `g3_project_vertex()`. `render/3ddraw.cpp: g3_draw_poly()` (→ `gr_tmapper`), `g3_draw_poly_if_facing()`, `g3_draw_bitmap`, `g3_draw_line`, sphere/rod helpers. `render/3dclipper.cpp` (frustum clipping). `render/3dlaser.cpp` (laser/bolt bounded-box billboards).
- **Core state:** view/projection matrices and the vertex free-list in `3dsetup.cpp`; `vertex` struct (screen x/y, w, u/v, r/g/b, codes) from `3d.h`.
- **Mechanism:** rotate world→view, Sutherland-Hodgman clip against frustum planes, perspective-project to screen `vertex`, hand to `gr_screen.gf_tmapper`. Backface cull via projected signed area / normal dot.
- **Rare knowledge:** classic fixed-pipeline software T&L with per-vertex clip outcodes; `g3_draw_poly` allocates temp vertex pointers and only projects lazily.
- **Deps:** `math/vecmat`, `graphics` (tmapper), `lighting`.
- **Port notes:** Mostly retail-intact; consumes whichever backend `gr_screen` selected, so it is backend-agnostic.

---

### bmpman  (~1,830 SLOC master)
- **Purpose:** Central bitmap registry/cache — loads PCX/TGA/ANI, hands out integer handles, lazy-locks/converts to the target bpp.
- **Entry points:** `bmpman/bmpman.cpp: bm_load()` (dispatches by extension to `pcxutils`/`tgautils`), `bm_create()`, `bm_lock()` (bpp/format conversion), `bm_load_animation()`, `bm_unlock`, `bm_release`, `bm_get_info`, `bm_page_in_*`.
- **Core state:** `struct bitmap_entry bm_bitmaps[MAX_BITMAPS(3500)]` — the bitmap slot table (filename, `handle = id*MAX_BITMAPS + slot`, ref count, bpp, w/h, cached `bitmap` data pointer, palette). Handle-vs-slot indirection via signature.
- **Mechanism:** slot dedup by filename; load defers pixel decode until `bm_lock`; converts 8bpp↔16bpp on demand; tracks paging for level loads.
- **Rare knowledge:** the handle scheme (`id*MAX_BITMAPS+slot`) detects stale handles after slot reuse; animation frames occupy contiguous slots.
- **Deps:** `pcxutils`, `tgautils`, `palman`, `cfile`, `graphics` (target format).
- **Port notes:** Modified (mtime recent) — 16bpp/format handling and paging likely revised for the GL backend; retail `MAX_BITMAPS` preserved.

---

### palman  (~675 SLOC master)
- **Purpose:** 8bpp palette manager and color-blend/lookup acceleration tables for the software rasterizer.
- **Entry points:** `palman/palman.cpp: palette_load_table()` (loads a `.pcx`-derived 256-entry palette, scales 6-bit→8-bit), `palman_load_pixels()`, `palman_set_gamma`, blend-table builders.
- **Core state:** globals `gr_palette[256*3]`, `palette_org[256*3]`, `palette_blend_table[NUM_BLEND_TABLES*256*256]` (precomputed pairwise color blends), `palette_lookup[64*64*64]` (RGB→nearest-palette-index inverse cube), `gr_palette_checksum`.
- **Mechanism:** builds a 64³ inverse lookup so any RGB maps to nearest palette index in O(1); blend tables give per-pair alpha compositing without per-pixel search.
- **Rare knowledge:** quintessential 8bpp-era technique — the 262KB inverse-palette cube and 128KB blend tables trade memory for per-pixel speed; palette entries stored 6-bit (VGA DAC era) then upscaled `*255/63`.
- **Deps:** `pcxutils` (palette source), `graphics`.
- **Port notes:** Retail-faithful; only meaningful under the software backend (GL ignores it).

---

### tgautils  (~740 SLOC master)
- **Purpose:** Targa (.tga) reader/writer, 8/16/24/32-bit incl. RLE.
- **Entry points:** `tgautils/tgautils.cpp: targa_read_header()`, `targa_read_bitmap()`, `targa_uncompress()` (RLE decode), `targa_write_bitmap`.
- **Core state:** local `targa_header`; no persistent globals.
- **Mechanism:** parses TGA header, handles RLE packets (`targa_uncompress`) and raw scans, respects `bytes_per_pixel` and origin bit.
- **Rare knowledge:** supports the retail 16bpp 1555 path used for true-color textures; appends `.tga` if extension missing.
- **Deps:** `cfile`, called by `bmpman`.
- **Port notes:** Retail-intact leaf module.

---

### pcxutils  (~640 SLOC master)
- **Purpose:** PCX reader/writer — the retail primary texture/UI format; decodes to 8bpp or expands to 16bpp.
- **Entry points:** `pcxutils/pcxutils.cpp: pcx_read_header()`, `pcx_read_bitmap_8bpp()`, `pcx_read_bitmap_16bpp()`, `pcx_read_bitmap_16bpp_aabitmap()` (anti-aliased font/UI), `pcx_read_bitmap_16bpp_nondark()`, `pcx_write_bitmap` (`pcx_encode_line`/`pcx_encode_byte` RLE).
- **Core state:** `PCXHeader`; palette read from final 768 bytes of file.
- **Mechanism:** RLE run decode; 16bpp variants map palette index through to RGB565/1555, with special "aabitmap" (alpha from luminance) and "nondark" (skip index 0 as transparent) modes.
- **Rare knowledge:** the aabitmap/nondark variants encode alpha/transparency conventions from a paletted source — key era detail for reproducing UI/font blending.
- **Deps:** `cfile`, `palman`, called by `bmpman`.
- **Port notes:** Retail-intact leaf module.

---

### starfield  (~1,800 SLOC master; starfield.cpp ~1,180, supernova.cpp ~330, nebula.cpp ~150)
- **Purpose:** Background rendering — point stars, sun/glow bitmaps, skybox/background bitmap layers, motion-blur star streaks, and supernova flash.
- **Entry points:** `starfield/starfield.cpp: stars_init()`, `stars_load_debris()`, `stars_draw()` (stars + suns + background bitmaps + debris), background bitmap add/parse. `starfield/supernova.cpp: supernova_start()/supernova_process()`. `starfield/nebula.cpp: nebula_init()` (the *old* FS1 full-screen rotating nebula bitmap, distinct from `nebula/neb.cpp`).
- **Core state:** `Stars[MAX_STARS]`, `Num_stars=500`, `Starfield_bitmaps[MAX_STARFIELD_BITMAPS]` (background + `glow_bitmap`), `Nebula_orient` (in starfield/nebula.cpp).
- **Mechanism:** stars are rotated by view delta and drawn as short streaks (velocity-based tails); suns are `g3_draw_bitmap` billboards with glow; background bitmaps are angle-placed textured quads.
- **Rare knowledge:** two unrelated "nebula" systems coexist — `starfield/nebula.cpp` is the legacy FS1 rotating background bitmap; `nebula/neb.cpp` is the FS2 volumetric fog. Star motion-blur length is derived from camera angular velocity.
- **Deps:** `render/3d`, `bmpman`, `starfield/nebula`.
- **Port notes:** Recently modified (background/sun handling); functionally retail. Note the OPEN issue: sun-glow billboard pulses through transparent HUD gaps (tracked in project notes).

---

### nebula  (~2,930 SLOC master; neb.cpp ~1,650, neblightning.cpp ~1,280)
- **Purpose:** FS2 volumetric nebula — screen-space fog, "poof" cloud puffs, per-object fog distances, and intra-nebula lightning ("bolts").
- **Entry points:** `nebula/neb.cpp: neb2_render_setup()`, `neb2_render_poofs()`, `neb2_get_fog_intensity()/neb2_get_fog_values()`, `neb2_page_in`. Render mode select `Neb2_render_mode` (NONE/POF/POLY/HTL). `nebula/neblightning.cpp: nebl_process()`, `nebl_bolt()` (procedural lightning generation).
- **Core state:** `Neb2_render_mode`, `Neb2_poofs[MAX_NEB2_POOFS]`/`Neb2_poof_filenames`, `Neb2_bitmap[]`, `Neb2_texture_name`; lightning bolt pools in neblightning.cpp.
- **Mechanism:** fog computed per-object by distance; poofs are camera-relative animated billboards distributed in a moving grid around the viewer; a full-screen approach samples a nebula texture. Lightning recursively subdivides a segment with random perpendicular jitter, then renders glowing line strips.
- **Rare knowledge:** the "poof" system fakes volumetric density with a scrolling 3D lattice of alpha billboards recycled as the camera moves — cheap volumetric fog without a voxel field. Multiple render modes reflect hardware-era fallbacks.
- **Deps:** `render/3d`, `bmpman`, `starfield`, `particle`, `lighting`.
- **Port notes:** neb.cpp recently modified; render-mode plumbing likely adjusted for the GL backend.

---

### particle  (~625 SLOC master)
- **Purpose:** Lightweight billboard particle system (fire, smoke, sparks, debris trails) with optional animated-bitmap particles and emitters.
- **Entry points:** `particle/particle.cpp: particle_init()`, `particle_create()` (two overloads: `particle_info*` and positional), `particle_emit()` (`particle_emitter`), `particle_move_all()`, `particle_render_all()`, `particle_kill_all()`, `particle_page_in()`.
- **Core state:** `struct particle` (pos, vel, age/max_life, radius, type, optional `bitmap`/attached objnum) in a dynamic pool; `struct particle_emitter`.
- **Mechanism:** each frame integrates position/age, culls dead, then batch-renders as `g3_draw_bitmap` billboards scaled by age; PARTICLE_BITMAP/ANIM types cycle animation frames.
- **Rare knowledge:** particles can be attached to an object (follow its motion); size interpolates over lifetime; a single global pool, no per-emitter allocation.
- **Deps:** `render/3d`, `bmpman`, `object` (attach).
- **Port notes:** Retail-intact leaf module.

---

### lighting  (~860 SLOC master)
- **Purpose:** Software dynamic-lighting model feeding the texture mapper — directional + point lights with per-submodel filtering.
- **Entry points:** `lighting/lighting.cpp: light_reset()`, `light_add_directional()`, `light_add_point()`/`light_add_point_unique()`, `light_rotate_all()` (`light_rotate`), `light_set_ambient`, and the `light_filter_push_box`/`light_filter_pop` stack used by `modelinterp.cpp` BSP boundboxes, plus `light_apply_rgb`/gouraud application per vertex.
- **Core state:** `struct light Lights[MAX_LIGHTS]`, `Num_lights`, a filtered active-light sublist, ambient level.
- **Mechanism:** lights are transformed into model space once (`light_rotate`), then a bounding-box filter (`light_filter_push_box`) prunes lights per BSP node so only nearby lights are evaluated in the inner vertex-lighting loop; results become per-vertex RAMP (intensity) or RGB colors consumed by the tmapper.
- **Rare knowledge:** the push/pop light-filter stack keyed off `OP_BOUNDBOX` chunks is the mechanism that makes per-vertex software lighting affordable — it is the direct partner of the BSP walk in `model_interp_sub`.
- **Deps:** `math/vecmat`, consumed by `render` and `model/modelinterp`.
- **Port notes:** Retail-faithful; RGB path matters more with the GL backend but the software RAMP path is intact.
