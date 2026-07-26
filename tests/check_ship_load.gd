# -*- mode: gdscript -*-
#
# The Ship-assembly oracle: for every converted model, build a Ship
# (inspect/ship.gd) through the real engine and cross-check its replay of
# retail's load-time movement reinterpretation against pof_dump --model's
# printed values -- retail's own C++ answers, via libpof's oracle-pinned
# dump. Also proves every submodel's scene node resolves by name (the
# index->find_child contract the overlays and the game both lean on).
#
#   godot --headless --path <repo>/inspect --script check_ship_load.gd \
#       -- <dir> [<dir> ...]
#
# Each <dir> holds <stem>.glb/.tres/.dump for one model. One engine boot
# checks the whole corpus. Exit 0 clean, 1 on any failure.
extends SceneTree

var bad := 0

func fail(msg: String) -> void:
    printerr("FAIL: " + msg)
    bad += 1

# "  sub N "name" parent P" ... "    movement type T axis A" -> [[T, A], ...]
func dump_movement(path: String) -> Array:
    var out: Array = []
    var expect_movement := false
    for line in FileAccess.get_file_as_string(path).split("\n"):
        if line.begins_with("  sub "):
            expect_movement = true
        elif expect_movement and line.begins_with("    movement type "):
            var parts := line.strip_edges().split(" ")
            out.append(Vector2i(int(parts[2]), int(parts[4])))
            expect_movement = false
    return out

func _init() -> void:
    var args := OS.get_cmdline_user_args()
    if args.is_empty():
        printerr("usage: ... -- <model-dir> [<model-dir> ...]")
        quit(2)
        return

    var ship_script := load("res://ship.gd")
    var models := 0
    var submodels := 0

    for dir in args:
        var stem := dir.get_file()
        var glb := dir + "/" + stem + ".glb"

        var ship = ship_script.new()
        if not ship.load_ship(glb):
            fail(stem + ": Ship.load_ship failed")
            ship.free()
            continue

        var expected: Array = dump_movement(dir + "/" + stem + ".dump")
        if expected.size() != ship.loaded.size():
            fail("%s: %d submodels in dump, %d in ship"
                % [stem, expected.size(), ship.loaded.size()])
            ship.free()
            continue

        for i in expected.size():
            if ship.loaded[i] != expected[i]:
                fail("%s: %s: replay says type %d axis %d, retail loaded type %d axis %d"
                    % [stem, ship.node_names[i], ship.loaded[i].x,
                       ship.loaded[i].y, expected[i].x, expected[i].y])
            if ship.sub_node(i) == null:
                fail("%s: submodel %d (%s) has no scene node"
                    % [stem, i, ship.node_names[i]])
        submodels += expected.size()

        # the derived views stay consistent with the replay
        for r in ship.rotators():
            if r["node"] == null:
                fail(stem + ": rotator without a node")
        for t in ship.turrets():
            if t["base"] == null or t["arm"] == null:
                fail(stem + ": turret base/arm did not resolve")

        # thruster submodels (is_thruster, pofparse.cc:688) are collected
        # and DARK until the engines engage (modelinterp.cc:1192 skips
        # them without MR_SHOW_THRUSTERS); set_thrusters flips the lot
        var expected_thrusters := 0
        for i in expected.size():
            if str(ship.node_names[i]).find("thruster") != -1:
                expected_thrusters += 1
        if ship.thruster_nodes.size() != expected_thrusters:
            fail("%s: %d thruster nodes collected, %d named"
                % [stem, ship.thruster_nodes.size(), expected_thrusters])
        for n in ship.thruster_nodes:
            if n.visible:
                fail(stem + ": thruster visible before engines engage")
        ship.set_thrusters(true)
        for n in ship.thruster_nodes:
            if not n.visible:
                fail(stem + ": set_thrusters(true) left a thruster dark")

        models += 1
        ship.free()

    if bad == 0:
        print("OK: %d models assembled, %d submodels' loaded movement matches retail"
            % [models, submodels])
    quit(1 if bad > 0 else 0)
