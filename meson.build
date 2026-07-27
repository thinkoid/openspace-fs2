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

# The tree is split by role: src/ compiles the whole ported corpus into the
# `foundation` library; bin/ declares the shippable toplevel executables that
# link it; tests/ owns its own family. Order matters -- src exports
# `foundation` and the entry-point sources, tests exports `game_stubs`, both
# of which bin consumes, so bin comes last.
subdir('src')
subdir('tests')
subdir('bin')
