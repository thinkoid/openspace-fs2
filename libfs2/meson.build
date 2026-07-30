# libfs2 -- the GDExtension boundary (docs/godot-migration-plan.md, "Second
# step"): foundation + the oracle stubs + fs2_t + the FS2 shim, compiled into
# the shared library Godot dlopens. Bindings are generated against the API
# dumped from the INSTALLED godot, so they match the binary that will load
# the extension; without a godot on PATH the whole family is skipped (the
# gate skips 77 alongside).

godot_exe = find_program('godot', required: false)
libfs2_so = disabler()

libfs2_version = vcs_tag(
    command: ['git', 'describe', '--always', '--dirty=+'],
    input: 'version.hh.in',
    output: 'version.hh')

# sim_dump needs no engine at all -- fs2_t linked directly (the boundary
# API is tested without Godot in the room); the world gates drive it.
sim_dump = executable('sim_dump',
    ['sim_dump.cc', 'fs2.cc', libfs2_version] + oracle_stubs,
    include_directories: inc,
    link_with: foundation,
    dependencies: [sdl2_dep, openal_dep, gl_dep])

if godot_exe.found()
    py3 = find_program('python3')
    godot_cpp = meson.project_source_root() / 'third-party/godot-cpp'

    # Configure-time generation: dump the api, trim it through the build
    # profile, generate, and capture the source list from stdout. Reruns on
    # reconfigure only -- the generated corpus depends on the installed godot
    # and the profile, neither of which ninja tracks.
    gen = run_command(py3, files('gen_bindings.py'),
        godot_cpp, godot_exe.full_path(), files('build_profile.json'),
        meson.current_build_dir(), check: true)
    gen_src = gen.stdout().strip().split('\n')

    # godot-cpp compiles as its own static library: handwritten core (the
    # stable 32-file set below) + the generated bindings. Third-party code
    # carries third-party warnings -- silenced (-w) so the port's warning
    # ledger stays meaningful.
    godot_cpp_args = [
        '-DGDEXTENSION', '-DTHREADS_ENABLED', '-DDEBUG_ENABLED', '-w',
        '-I' + godot_cpp / 'include',
        '-I' + godot_cpp / 'gdextension',
        '-I' + meson.current_build_dir() / 'gen/include']

    godot_cpp_handwritten = files(
        '../third-party/godot-cpp/src/classes/editor_plugin_registration.cpp',
        '../third-party/godot-cpp/src/classes/low_level.cpp',
        '../third-party/godot-cpp/src/classes/wrapped.cpp',
        '../third-party/godot-cpp/src/core/class_db.cpp',
        '../third-party/godot-cpp/src/core/error_macros.cpp',
        '../third-party/godot-cpp/src/core/memory.cpp',
        '../third-party/godot-cpp/src/core/method_bind.cpp',
        '../third-party/godot-cpp/src/core/object.cpp',
        '../third-party/godot-cpp/src/core/print_string.cpp',
        '../third-party/godot-cpp/src/godot.cpp',
        '../third-party/godot-cpp/src/variant/aabb.cpp',
        '../third-party/godot-cpp/src/variant/basis.cpp',
        '../third-party/godot-cpp/src/variant/callable_custom.cpp',
        '../third-party/godot-cpp/src/variant/callable_method_pointer.cpp',
        '../third-party/godot-cpp/src/variant/char_string.cpp',
        '../third-party/godot-cpp/src/variant/color.cpp',
        '../third-party/godot-cpp/src/variant/packed_arrays.cpp',
        '../third-party/godot-cpp/src/variant/plane.cpp',
        '../third-party/godot-cpp/src/variant/projection.cpp',
        '../third-party/godot-cpp/src/variant/quaternion.cpp',
        '../third-party/godot-cpp/src/variant/rect2.cpp',
        '../third-party/godot-cpp/src/variant/rect2i.cpp',
        '../third-party/godot-cpp/src/variant/transform2d.cpp',
        '../third-party/godot-cpp/src/variant/transform3d.cpp',
        '../third-party/godot-cpp/src/variant/variant.cpp',
        '../third-party/godot-cpp/src/variant/variant_internal.cpp',
        '../third-party/godot-cpp/src/variant/vector2.cpp',
        '../third-party/godot-cpp/src/variant/vector2i.cpp',
        '../third-party/godot-cpp/src/variant/vector3.cpp',
        '../third-party/godot-cpp/src/variant/vector3i.cpp',
        '../third-party/godot-cpp/src/variant/vector4.cpp',
        '../third-party/godot-cpp/src/variant/vector4i.cpp')

    godot_cpp_lib = static_library('godot-cpp',
        godot_cpp_handwritten + gen_src,
        cpp_args: godot_cpp_args)

    # The .so: fs2_t + the shim, the WHOLE foundation (link_whole surfaces
    # any missing or duplicate symbol now and runs every static ctor at
    # dlopen -- the link risk retires in slice 0, not later), and the same
    # oracle stubs the tools prove out. --no-undefined makes completeness a
    # link-time fact instead of a dlopen surprise.
    libfs2_so = shared_library('fs2',
        ['fs2.cc', 'extension.cc', libfs2_version] + oracle_stubs,
        cpp_args: godot_cpp_args,
        include_directories: inc,
        link_whole: foundation,
        link_with: godot_cpp_lib,
        dependencies: [sdl2_dep, openal_dep, gl_dep],
        link_args: ['-Wl,--no-undefined'])

    # The runtime loading contract: consumers (fly.gd, the gates) hand this
    # file to GDExtensionManager.load_extension. It lives in the build tree
    # with the .so's absolute path baked in -- the inspect/ project is never
    # written to.
    gdext_conf = configuration_data()
    gdext_conf.set('SO_PATH', libfs2_so.full_path())
    libfs2_gdext = configure_file(
        input: 'fs2.gdextension.in',
        output: 'fs2.gdextension',
        configuration: gdext_conf)
endif
