# Point-1 oracle and unit tests.  stubs.cpp carries temporary definitions for
# game globals the foundation references but whose owning subsystems are not
# ported yet; entries get deleted as the real code comes in.

# The game binary (../bin) links the same two stub files -- it is built against
# unported gameplay too. Exported so bin/meson.build doesn't reach across the
# tree for them; see the wart note there.
game_stubs = files('stubs.cc', 'gamestubs_gen.cc')

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

# pof2glb transcodes each retail PCX map to a TGA; tex-check decodes the same
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
