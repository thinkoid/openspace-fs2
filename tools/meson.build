# Migration tooling (docs/godot-migration-plan.md). Flat, like tests/: one
# meson.build owning the family. A single-file tool is one .cc here; a tool
# that grows into a family of sources earns its own subdir then, not before.

# The POF->GLB converter reads models through libpof (vendored as a submodule
# at subprojects/libpof, acquired in the root meson.build), never through a
# second POF parser. libpof is C++23; the override keeps the game on C++17.
# stb_image_write (third-party/stb) transcodes the maps to TGA; its unused
# static helpers are silenced, and it wants libm.
pof2glb = executable('pof2glb',
    'pof2glb.cc',
    include_directories: include_directories('../third-party/stb'),
    dependencies: [libpof_dep, meson.get_compiler('cpp').find_library('m')],
    cpp_args: ['-Wno-unused-function', '-Wno-unused-parameter'],
    override_options: ['cpp_std=c++23'])
