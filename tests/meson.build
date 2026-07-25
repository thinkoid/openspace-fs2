# Point-1 oracle and unit tests.  stubs.cc carries temporary definitions for
# game globals the foundation references but whose owning subsystems are not
# ported yet; entries get deleted as the real code comes in. The stub file
# lists (game_stubs, oracle_stubs) are exported from the root meson.build --
# tools/ builds before this dir and links them too (vpstage).

# sexp.cpp's evaluator half references gameplay code that is not ported yet;
# gamestubs_gen.cpp fills those symbols with traps (see its header comment).
sexp_dump = executable('sexp_dump',
    ['sexp_dump.cc', 'stubs.cc', 'gamestubs_gen.cc', 'freespacestubs_gen.cc'],
    include_directories: inc,
    link_with: foundation,
    dependencies: [sdl2_dep, openal_dep, gl_dep])

vp_ls = executable('vp_ls',
    ['vp_ls.cc', 'stubs.cc', 'gamestubs_gen.cc', 'freespacestubs_gen.cc'],
    include_directories: inc,
    link_with: foundation,
    dependencies: [sdl2_dep, openal_dep, gl_dep])

pof_dump = executable('pof_dump',
    ['pof_dump.cc', 'stubs.cc', 'gamestubs_gen.cc', 'freespacestubs_gen.cc'],
    include_directories: inc,
    link_with: foundation,
    dependencies: [sdl2_dep, openal_dep, gl_dep])

# pof_dump is the POF oracle: retail's loader is the only authoritative reading
# of the format, so its output is what a second implementation gets compared
# against. Pinned in oracle/; the runner skips when no game install is around.
test('pof-oracle', find_program('pof_oracle.sh'),
    args: [
        pof_dump.full_path(),
        meson.current_source_dir() / 'oracle',
        meson.project_source_root(),
    ],
    depends: pof_dump,
    timeout: 300)

# The migration's GLB emitter (../tools/pof2glb) checked against the same
# oracle: emit the inspection-slice models, then cross-check structure,
# hierarchy, triangle counts and winding-vs-normals with check_glb.py. Needs an
# *unpacked* install (pof2glb reads loose files); skips otherwise.
test('glb-check', find_program('glb_check.sh'),
    args: [
        pof2glb.full_path(),
        pof_dump.full_path(),
        meson.current_source_dir() / 'check_glb.py',
        meson.project_source_root(),
    ],
    depends: [pof2glb, pof_dump],
    timeout: 120)

# The same converter's .tres ship-data output (weapon/thruster/dock/path
# points, turrets, eyes, shield), cross-checked against pof_dump --model
# through the axis map by check_tres.py. Separate from glb-check so a mesh
# regression and a ship-data regression fail apart. Same unpacked-install gate.
test('tres-check', find_program('tres_check.sh'),
    args: [
        pof2glb.full_path(),
        pof_dump.full_path(),
        meson.current_source_dir() / 'check_tres.py',
        meson.project_source_root(),
    ],
    depends: [pof2glb, pof_dump],
    timeout: 120)

# pcx_dump is the texture oracle: retail's PCX decoder is authoritative, so its
# 8bpp output pins pof2glb's hand-rolled decoder. Same foundation link as the
# other oracles (it calls pcx_read_bitmap_8bpp through cfile).
pcx_dump = executable('pcx_dump',
    ['pcx_dump.cc', 'stubs.cc', 'gamestubs_gen.cc', 'freespacestubs_gen.cc'],
    include_directories: inc,
    link_with: foundation,
    dependencies: [sdl2_dep, openal_dep, gl_dep])

# pof2glb transcodes each retail PCX map to a PNG; tex-check decodes the same
# maps through pcx_dump (retail's reader) and check_tex.py compares the two
# pixel-for-pixel, including the green colour-key. Makes the hand-rolled decoder
# safe. Same unpacked-install gate as glb-check/tres-check.
test('tex-check', find_program('tex_check.sh'),
    args: [
        pof2glb.full_path(),
        pcx_dump.full_path(),
        meson.current_source_dir() / 'check_tex.py',
        meson.project_source_root(),
    ],
    depends: [pof2glb, pcx_dump],
    timeout: 120)

# The converter's manifest, verified by re-deriving every SHA-256 with python
# hashlib -- an independent implementation, so this doubles as the oracle for
# pof2glb's hand-rolled digest -- plus a convert-twice byte-compare pinning
# the manifest's reproducibility. Same unpacked-install gate.
test('manifest-check', find_program('manifest_check.sh'),
    args: [
        pof2glb.full_path(),
        meson.current_source_dir() / 'check_manifest.py',
        meson.project_source_root(),
    ],
    depends: [pof2glb],
    timeout: 120)

# The same .tres loaded through the REAL engine (godot --headless --script,
# --path at inspect/ so res://ship_data.gd resolves) and validated against the
# schema by check_tres_load.gd. tres-check proves the numbers with a text
# parser; this proves Godot's VariantParser accepts the file at all. Skips
# without godot on PATH, plus the usual unpacked-install gate.
test('tres-load-check', find_program('tres_load_check.sh'),
    args: [
        pof2glb.full_path(),
        meson.current_source_dir() / 'check_tres_load.gd',
        meson.project_source_root(),
        meson.project_source_root(),
    ],
    depends: [pof2glb],
    timeout: 120)

# physics_dump is the flight-model oracle: retail's physics_sim over a
# scripted control sequence; inspect/flight_model.gd must reproduce the
# trace (flight-check below).
physics_dump = executable('physics_dump',
    ['physics_dump.cc', 'stubs.cc', 'gamestubs_gen.cc', 'freespacestubs_gen.cc'],
    include_directories: inc,
    link_with: foundation,
    dependencies: [sdl2_dep, openal_dep, gl_dep])

# The flight gate: the trace from physics_dump vs flight_model.gd's replay,
# frame by frame within float-width tolerances. Needs no game install --
# the oracle is the ported retail code itself.
test('flight-check', find_program('flight_check.sh'),
    args: [
        physics_dump.full_path(),
        meson.current_source_dir() / 'check_flight.gd',
        meson.project_source_root(),
    ],
    depends: [physics_dump],
    timeout: 120)

# The game-side consumption gate: every model assembled as a Ship
# (inspect/ship.gd) in one headless engine boot; its replay of retail's
# load-time movement reinterpretation is diffed per submodel against
# pof_dump --model, corpus-wide. Skips without godot or the install.
test('ship-load-check', find_program('ship_load_check.sh'),
    args: [
        pof2glb.full_path(),
        pof_dump.full_path(),
        meson.current_source_dir() / 'check_ship_load.gd',
        meson.project_source_root(),
        meson.project_source_root(),
    ],
    depends: [pof2glb, pof_dump],
    timeout: 600)

# The VP staging gate: vpstage extracts models + maps from the pristine VPs
# via retail's cfile; check_staging.py verifies digests (hashlib), raw
# offset-slices from the archives (bypassing cfile), and byte-equality with
# the unpacked install; then the slice converts identically from both trees.
test('vpstage-check', find_program('vpstage_check.sh'),
    args: [
        vpstage.full_path(),
        pof2glb.full_path(),
        meson.current_source_dir() / 'check_staging.py',
        meson.project_source_root(),
    ],
    depends: [vpstage, pof2glb],
    timeout: 300)

# The corpus gate: every retail model (176) through pof2glb, cross-checked
# with check_glb + check_tres + check_manifest. Wide where the slice gates
# are deep; ~40s. spherec's known missing-map warning is the one allowance.
test('corpus-check', find_program('corpus_check.sh'),
    args: [
        pof2glb.full_path(),
        pof_dump.full_path(),
        meson.current_source_dir() / 'check_glb.py',
        meson.current_source_dir() / 'check_tres.py',
        meson.current_source_dir() / 'check_manifest.py',
        meson.project_source_root(),
    ],
    depends: [pof2glb, pof_dump],
    timeout: 600)

math_test = executable('math_test',
    ['math_test.cc', 'stubs.cc', 'gamestubs_gen.cc', 'freespacestubs_gen.cc'],
    include_directories: inc,
    link_with: foundation,
    dependencies: [sdl2_dep, openal_dep, gl_dep])

first_pixels = executable('first_pixels',
    ['first_pixels.cc', 'stubs.cc', 'gamestubs_gen.cc', 'freespacestubs_gen.cc'],
    include_directories: inc,
    link_with: foundation,
    dependencies: [sdl2_dep, openal_dep, gl_dep])

test('math', math_test)

tmap_test = executable('tmap_test',
    ['tmap_test.cc', 'stubs.cc', 'gamestubs_gen.cc', 'freespacestubs_gen.cc'],
    include_directories: inc,
    link_with: foundation,
    dependencies: [sdl2_dep, openal_dep, gl_dep])
