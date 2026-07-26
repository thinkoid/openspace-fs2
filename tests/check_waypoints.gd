# -*- mode: gdscript -*-
#
# The waypoint-AI gate: inspect/waypoint_ai.gd's flying, arrival test,
# log stamping and the are-waypoints-done-delay predicate pinned against
# retail's contract (aicode.cc:4717 arrival = MIN_DIST 5 + sqrt(radius)
# swept over the frame's travel; aicode.cc:4763 log on LAST waypoint
# only; sexp.cc:3543 fix-delay math + the destroyed-after-done quirk;
# missionlog.cc:423 case-insensitive names). Pure logic, no scene.
#
#   godot --headless --path <repo>/inspect --script check_waypoints.gd
extends SceneTree

var failed := 0

func check(what: String, got, want) -> void:
    if got != want:
        printerr("FAIL %s: got %s, want %s" % [what, got, want])
        failed += 1

# run the sim until the ship completes a path or the clock runs out;
# returns the number of steps taken, -1 if it never finished
func fly_until_done(nav, ship: String, max_steps: int,
                    mt_fix: int) -> int:
    for i in max_steps:
        if not nav.step(0.1, mt_fix).is_empty():
            return i
    return -1

func _init() -> void:
    var W := preload("res://waypoint_ai.gd")
    var VM := preload("res://sexp_vm.gd")

    # a dogleg: ahead, off to starboard, then back across -- the turn
    # limiter has to work for the second and third legs
    var nav = W.new()
    nav.set_lists([{"name": "Test path", "points":
        [Vector3(0, 0, 500), Vector3(400, 0, 900), Vector3(-200, 0, 1200)]},
        {"name": "Second path", "points": [Vector3(0, 0, 0)]}])
    nav.register("Runner", Vector3.ZERO, Vector3(0, 0, 1),
                 70.0, 1.5, 16.0)
    nav.register("Statue", Vector3(100, 0, 0), Vector3(0, 0, 1),
                 70.0, 1.5, 16.0)

    check("unknown ship refused",
          nav.command("Ghost", "waypoints-once", "Test path"), false)
    check("unknown path refused",
          nav.command("Runner", "waypoints-once", "No such path"), false)
    check("path name case-insensitive on command",
          nav.command("Runner", "waypoints-once", "TEST PATH"), true)

    # statues don't move; runners visit every waypoint in order and the
    # log stamps ONLY on the last
    var statue_pos: Vector3 = nav.ships["Statue"]["pos"]
    var steps := fly_until_done(nav, "Runner", 2000, 5 * 65536)
    check("path completes", steps >= 0, true)
    check("all waypoints visited", nav.ships["Runner"]["idx"], 3)
    check("one log entry", nav.done.size(), 1)
    check("done stamped with mt_fix",
          nav.done_time("Runner", "Test path"), 5 * 65536)
    check("log names case-insensitive",
          nav.done_time("RUNNER", "test PATH"), 5 * 65536)
    check("statue never moved", nav.ships["Statue"]["pos"], statue_pos)
    check("finished runner cruises", nav.ships["Runner"]["mode"], "cruise")

    # a cruising ship keeps moving straight (retail resets to default
    # behavior; the next add-goal lands within the event cadence)
    var after_done: Vector3 = nav.ships["Runner"]["pos"]
    nav.step(0.1, 5 * 65536)
    check("cruise keeps flying",
          nav.ships["Runner"]["pos"] != after_done, true)

    # add-goal mid-life: a second path flies from its own first point
    check("re-command onto second path",
          nav.command("Runner", "waypoints-once", "Second path"), true)
    check("second path completes",
          fly_until_done(nav, "Runner", 2000, 9 * 65536) >= 0, true)
    check("second log entry",
          nav.done_time("Runner", "Second path"), 9 * 65536)
    check("first entry untouched",
          nav.done_time("Runner", "Test path"), 5 * 65536)

    # stay-still parks the ship
    nav.command("Runner", "stay-still")
    var parked: Vector3 = nav.ships["Runner"]["pos"]
    nav.step(0.1, 9 * 65536)
    check("stay-still parks", nav.ships["Runner"]["pos"], parked)
    check("parked speed reads zero", nav.speed_of("Runner"), 0.0)

    # the predicate: fix-delay math, and retail's quirk -- destruction
    # is only consulted when the log has NO entry (sexp.cc:3562)
    check("done, zero delay",
          nav.are_waypoints_done_delay("Runner", "Test path", 0,
                                       9 * 65536, {}), VM.KNOWN_TRUE)
    check("delay still running",
          nav.are_waypoints_done_delay("Runner", "Test path", 5,
                                       9 * 65536, {}), 0)
    check("delay elapsed",
          nav.are_waypoints_done_delay("Runner", "Test path", 4,
                                       9 * 65536, {}), VM.KNOWN_TRUE)
    check("not done, alive: pending",
          nav.are_waypoints_done_delay("Statue", "Test path", 0,
                                       9 * 65536, {}), 0)
    check("not done, destroyed: never",
          nav.are_waypoints_done_delay("Statue", "Test path", 0,
                                       9 * 65536, {"Statue": 1}),
          VM.KNOWN_FALSE)
    check("done then destroyed stays true (retail quirk)",
          nav.are_waypoints_done_delay("Runner", "Test path", 0,
                                       9 * 65536, {"Runner": 1}),
          VM.KNOWN_TRUE)

    if failed == 0:
        print("OK waypoints: flight, arrival, log, predicate semantics")
    quit(1 if failed > 0 else 0)
