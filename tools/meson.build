# Migration tooling (docs/godot-migration-plan.md). Flat, like tests/: one
# meson.build owning the family. A single-file tool is one .cc here; a tool
# that grows into a family of sources earns its own subdir then, not before.

# The manifest records the converter version; vcs_tag re-runs git describe at
# build time (--always: the repo carries no tags, fall back to the hash), so
# the header can never go stale the way a configure-time value would.
pof2glb_version = vcs_tag(
    command: ['git', 'describe', '--always', '--dirty=+'],
    input: 'pof2glb_version.hh.in',
    output: 'pof2glb_version.hh')

# The POF->GLB converter reads models through libpof (vendored as a submodule
# at subprojects/libpof, acquired in the root meson.build), never through a
# second POF parser. libpof is C++23; the override keeps the game on C++17.
# stb_image_write (third-party/stb) transcodes the maps to TGA; its unused
# static helpers are silenced, and it wants libm.
pof2glb = executable('pof2glb',
    ['pof2glb.cc', pof2glb_version],
    include_directories: include_directories('../third-party/stb'),
    dependencies: [libpof_dep, meson.get_compiler('cpp').find_library('m')],
    cpp_args: ['-Wno-unused-function', '-Wno-unused-parameter'],
    override_options: ['cpp_std=c++23'])

# vpstage extracts the conversion inputs (models + maps) from the retail VP
# archives into a loose staging tree + staging.manifest.json (origin archive,
# offsets, digests). The extraction goes through retail's own cfile -- the
# authoritative VP reader -- so it links the foundation and the stub
# apparatus exactly like the oracle dumpers in ../tests do.
vpstage = executable('vpstage',
    ['vpstage.cc', pof2glb_version] + oracle_stubs,
    include_directories: inc,
    link_with: foundation,
    dependencies: [sdl2_dep, openal_dep, gl_dep])

# ani2png bakes the retail effects flipbooks (.ani -> RGBA PNG atlas + JSON
# sidecar) through retail's own anim code -- the authoritative decoder, like
# vpstage's cfile -- with stb writing the PNG like pof2glb's textures.
ani2png = executable('ani2png',
    ['ani2png.cc'] + oracle_stubs,
    include_directories: [inc, include_directories('../third-party/stb')],
    link_with: foundation,
    dependencies: [sdl2_dep, openal_dep, gl_dep],
    cpp_args: ['-Wno-unused-function'])

# shiptbl2tres runs retail's weapon_init + ship_init (the real parse_shiptbl)
# and emits the flight-parameter subset of Ship_info as a Godot .tres --
# ships.tbl read by its one authoritative reader, like vpstage's cfile.
shiptbl2tres = executable('shiptbl2tres',
    ['shiptbl2tres.cc'] + oracle_stubs,
    include_directories: inc,
    link_with: foundation,
    dependencies: [sdl2_dep, openal_dep, gl_dep])

# mission2tres runs retail's parse_main over a .fs2 under Fred_running (all
# objects created, cues ignored, player starts = OBJ_START) and emits the
# mission's ship layout for the Godot side.
mission2tres = executable('mission2tres',
    ['mission2tres.cc'] + oracle_stubs,
    include_directories: inc,
    link_with: foundation,
    dependencies: [sdl2_dep, openal_dep, gl_dep])
