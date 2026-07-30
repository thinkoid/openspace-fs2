# -*- mode: gdscript -*-
#
# Load libfs2 through the REAL engine and prove the boundary round-trips:
# the FS2 class must exist (the .gdextension parsed, the .so dlopened, the
# entry symbol ran, ClassDB registration happened) and version() must return
# the exact string vcs_tag stamped at build time -- proving the call crossed
# GDScript -> shim -> fs2_t and back.
#
#   godot --headless --path <tmp-project> --script check_gdext.gd -- <expected-version>
#
# Exit 0 clean, 1 on any failure, 2 on usage.
extends SceneTree

func _init() -> void:
    var args := OS.get_cmdline_user_args()
    if args.size() != 1:
        printerr("usage: -- <expected-version>")
        quit(2)
        return
    var expected: String = args[0]

    if not ClassDB.class_exists("FS2"):
        printerr("FAIL: class FS2 not registered -- extension did not load")
        quit(1)
        return

    var sim = ClassDB.instantiate("FS2")
    if sim == null:
        printerr("FAIL: FS2 instantiation returned null")
        quit(1)
        return

    var got: String = sim.version()
    if got != expected:
        printerr("FAIL: version mismatch: got '%s', expected '%s'" %
                 [got, expected])
        quit(1)
        return

    print("OK: FS2.version() == '%s' round-tripped through the boundary" % got)
    quit(0)
