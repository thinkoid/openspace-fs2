# The shippable toplevel executables. Sources live under ../src (imported as
# File objects: game_main, archiver_main); declaring the targets here is what
# lands the binaries in builddir/bin, apart from the tools and tests.

# NOTE (wart): the game binary still links two test stubs (game_stubs, exported
# from ../tests) -- gameplay symbols the foundation references but whose owning
# subsystems are not ported yet. This game->tests coupling should dissolve as
# the real subsystems come in and the stubs shrink; it is not the build layout's
# to fix. See tests/gamestubs_gen.cc's header.
fs2 = executable('fs2',
    [game_main, game_stubs],
    include_directories: inc,
    link_with: foundation,
    dependencies: [sdl2_dep, openal_dep, gl_dep])

# deploy the game binary into the sibling rundir on every build (it must be
# a copy: /proc/self/exe resolves symlinks and the game chdirs to the result);
# quietly skipped when no ../rundir exists (e.g. a bare clone)
rundir = meson.project_source_root() / '..' / 'rundir'
custom_target('fs2-deploy',
    input: fs2,
    output: 'fs2-deploy.stamp',
    command: ['sh', '-c', 'test -d ' + rundir + ' && cp -f @INPUT@ ' + rundir + '/fs2; touch @OUTPUT@'],
    build_by_default: true)

cfilearchiver = executable('cfilearchiver',
    archiver_main,
    include_directories: inc)
