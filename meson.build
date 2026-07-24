project('fs2', 'cpp',
    version: '0.1',
    default_options: ['cpp_std=c++17', 'warning_level=1', 'buildtype=debug'])

# Retail members shadowing their type names (angles angles; etc.) — renames
# happen when each subsystem is ported, not wholesale.
add_project_arguments('-Wno-changes-meaning', language: 'cpp')

sdl2_dep = dependency('sdl2')
openal_dep = dependency('openal')
gl_dep = dependency('gl')

# Sources use subdirectory-qualified angle includes (e.g. <parse/parselo.hh>);
# -Isrc makes each subsystem addressable by its directory name.
inc = include_directories('src')

# The POF->GLB converter reads models through libpof (vendored as a submodule
# at subprojects/libpof, RELATIVE url so it resolves on both github and odroid).
# Acquired here; consumed in tools/. The C++23 subproject does not disturb the
# game's C++17 -- tools/ overrides cpp_std on the one target that needs it.
subproject('libpof')
libpof_dep = dependency('libpof')

# The tree is split by role: src/ compiles the whole ported corpus into the
# `foundation` library; bin/ declares the shippable toplevel executables that
# link it; tools/ and tests/ own their own families. Order matters -- src
# exports `foundation` and the entry-point sources, tests exports `game_stubs`,
# both of which bin consumes, so bin comes last.
subdir('src')
subdir('tests')
subdir('tools')
subdir('bin')
