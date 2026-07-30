# -*- mode: gdscript -*-
#
# Load libfs2 through the REAL engine and prove the boundary round-trips:
# GDExtensionManager.load_extension must accept the build tree's generated
# fs2.gdextension (dlopen + entry symbol + ClassDB registration), and
# FS2.version() must return the exact string vcs_tag stamped at build time
# -- proving the call crossed GDScript -> shim -> fs2_t and back.
#
#   godot --headless --path <repo>/inspect --script check_gdext.gd \
#       -- <fs2.gdextension> <expected-version>
#
# Exit 0 clean, 1 on any failure, 2 on usage.
extends SceneTree

func _init() -> void:
    var args := OS.get_cmdline_user_args()
    if args.size() != 2:
        printerr("usage: -- <fs2.gdextension> <expected-version>")
        quit(2)
        return
    var gdext: String = args[0]
    var expected: String = args[1]

    var status := GDExtensionManager.load_extension(gdext)
    if status != GDExtensionManager.LOAD_STATUS_OK:
        printerr("FAIL: load_extension(%s) -> %d" % [gdext, status])
        quit(1)
        return

    if not ClassDB.class_exists("FS2"):
        printerr("FAIL: extension loaded but class FS2 not registered")
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
