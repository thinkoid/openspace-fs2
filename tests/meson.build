# Point-1 oracle and unit tests.  stubs.cpp carries temporary definitions for
# game globals the foundation references but whose owning subsystems are not
# ported yet; entries get deleted as the real code comes in.

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
