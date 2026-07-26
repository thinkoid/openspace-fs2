# -*- mode: gdscript -*-
#
# The targeting gate: inspect/targeting.gd's cycling and `targeted`
# semantics pinned against retail's contract (hudtarget.cpp cycle order,
# sexp.cc:6528 name + held-for check). Pure logic, no scene -- a synthetic
# ship list stands in for the mission (the user's inert-objects range,
# distilled).
#
#   godot --headless --path <repo>/inspect --script check_targeting.gd
extends SceneTree

var failed := 0

func check(what: String, got, want) -> void:
    if got != want:
        printerr("FAIL %s: got %s, want %s" % [what, got, want])
        failed += 1

func _init() -> void:
    var T := preload("res://targeting.gd")
    var t = T.new()
    t.setup([
        {"name": "Alpha 1", "team": 0},      # the player: excluded
        {"name": "Instructor", "team": 0},
        {"name": "Drone 01", "team": 1},
        {"name": "Drone 02", "team": 1},
    ], "Alpha 1")

    check("ships exclude player", t.ships.size(), 3)
    check("no target initially", t.target, "")
    check("targeted with no target",
          t.targeted_check("Instructor", 0, 0), false)

    # T cycles mission order, wrapping
    t.next_target(1000)
    check("first cycle", t.target, "Instructor")
    t.next_target(2000)
    check("second cycle", t.target, "Drone 01")
    t.next_target(3000)
    t.next_target(4000)
    check("cycle wraps", t.target, "Instructor")

    # H cycles hostiles only (player team 0)
    t.next_hostile(0, 5000)
    check("hostile skips friendlies", t.target, "Drone 01")
    t.next_hostile(0, 6000)
    check("hostile cycles", t.target, "Drone 02")
    t.next_hostile(0, 7000)
    check("hostile wraps", t.target, "Drone 01")

    # the targeted predicate: name (case-insensitive), held-for delay
    check("targeted name match", t.targeted_check("Drone 01", 0, 7000), true)
    check("targeted case-insensitive",
          t.targeted_check("DRONE 01", 0, 7000), true)
    check("targeted wrong name",
          t.targeted_check("Instructor", 0, 7000), false)
    check("targeted delay not yet",
          t.targeted_check("Drone 01", 2, 8000), false)
    check("targeted delay elapsed",
          t.targeted_check("Drone 01", 2, 9000), true)

    t.clear()
    check("clear drops target", t.target, "")

    if failed == 0:
        print("OK targeting: cycle, hostile filter, targeted semantics")
    quit(1 if failed > 0 else 0)
