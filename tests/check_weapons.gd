# -*- mode: gdscript -*-
#
# The weapons gate: inspect/weapons.gd's fire cadence, swept collision,
# hull ledger and mission-log queries pinned against retail's contract
# (fire_wait gating, sexp.cc:3314 is-destroyed-delay fix math,
# sexp.cc:3835 hits-left NAN codes). Pure logic, no scene -- the Subach's
# real numbers (450 m/s, 15 damage, 2 s life, 0.2 s wait) over synthetic
# drones. The tunneling case is the reason the collision is swept: one
# 0.1 s step moves a bolt 45 m, clean through a 10 m sphere's interior
# without ever standing inside it.
#
#   godot --headless --path <repo>/inspect --script check_weapons.gd
extends SceneTree

var failed := 0

func check(what: String, got, want) -> void:
    if got != want:
        printerr("FAIL %s: got %s, want %s" % [what, got, want])
        failed += 1

func _init() -> void:
    var W := preload("res://weapons.gd")
    var VM := preload("res://sexp_vm.gd")
    var w = W.new()
    w.setup({"velocity": 450.0, "damage": 15.0,
             "lifetime": 2.0, "fire_wait": 0.2})
    w.add_ship("Drone 01", 10.0, 30.0)          # dies in two hits
    w.add_ship("Drone 02", 10.0, 180.0)
    w.add_ship("Instructor", 10.0, 240.0, true) # invulnerable
    var positions := {
        "Drone 01": Vector3(0, 0, 300),
        "Drone 02": Vector3(0, 0, 2000),        # out of reach
        "Instructor": Vector3(0, 0, -500),
    }

    # the trigger honors fire_wait, and the bolt inherits the parent's
    # velocity on top of its own
    check("first shot fires",
          w.try_fire(1000, Vector3.ZERO, Vector3(0, 0, 1),
                     Vector3(0, 0, 50)), true)
    check("cadence blocks the second", w.try_fire(1100, Vector3.ZERO,
          Vector3(0, 0, 1), Vector3.ZERO), false)
    check("bolt speed inherits parent",
          w.projectiles[0]["vel"], Vector3(0, 0, 500))
    w.projectiles.clear()

    # swept collision: one 0.1 s step carries the bolt from z=280 to
    # z=325, THROUGH the 10 m sphere at z=300 -- endpoints both outside
    w.try_fire(2000, Vector3(0, 0, 280), Vector3(0, 0, 1),
               Vector3.ZERO)
    # (hit details read through .get so a missing hit fails the checks
    # rather than crashing the script before quit())
    var hits: Array = w.step(0.1, 2100, 65536, positions)
    var h: Dictionary = hits[0] if hits.size() > 0 else {}
    check("swept hit through the sphere", hits.size(), 1)
    check("hit names the drone", h.get("name"), "Drone 01")
    check("first hit no kill", h.get("killed"), false)
    check("hull spent", w.ships["Drone 01"]["hull"], 15.0)
    check("hits-left percentage", w.hits_left("Drone 01"), 50)
    check("bolt consumed by the hit", w.projectiles.size(), 0)

    # a wide miss spends nothing and flies on
    w.try_fire(3000, Vector3(500, 0, 280), Vector3(0, 0, 1), Vector3.ZERO)
    check("miss leaves hull alone",
          w.step(0.1, 3100, 65536, positions).size(), 0)
    check("missed bolt flies on", w.projectiles.size(), 1)
    w.projectiles.clear()

    # the second hit kills: destroyed stamped with the kill-time mt_fix
    w.try_fire(4000, Vector3(0, 0, 290), Vector3(0, 0, 1), Vector3.ZERO)
    hits = w.step(0.1, 4100, 5 * 65536, positions)
    h = hits[0] if hits.size() > 0 else {}
    check("second hit kills", h.get("killed"), true)
    check("destroyed stamped", w.destroyed.get("Drone 01"), 5 * 65536)

    # a dead ship stops nothing: the same trajectory now passes through
    w.try_fire(5000, Vector3(0, 0, 290), Vector3(0, 0, 1), Vector3.ZERO)
    check("dead ship is no obstacle",
          w.step(0.1, 5100, 6 * 65536, positions).size(), 0)
    w.projectiles.clear()

    # lifetime: 2 s after launch the bolt expires on its own
    w.try_fire(6000, Vector3(0, 0, 0), Vector3(0, 1, 0), Vector3.ZERO)
    w.step(0.1, 7900, 65536, positions)
    check("bolt alive within lifetime", w.projectiles.size(), 1)
    w.step(0.1, 8100, 65536, positions)
    check("bolt expires at lifetime", w.projectiles.size(), 0)

    # invulnerable: the bolt stops, the hull doesn't move
    w.try_fire(9000, Vector3(0, 0, -480), Vector3(0, 0, -1), Vector3.ZERO)
    hits = w.step(0.1, 9100, 65536, positions)
    h = hits[0] if hits.size() > 0 else {}
    check("invulnerable still hit", hits.size(), 1)
    check("invulnerable never killed", h.get("killed"), false)
    check("invulnerable hull untouched",
          w.ships["Instructor"]["hull"], 240.0)

    # is-destroyed-delay: 0 while any name stands; all down -> 0 inside
    # the delay window, KNOWN_TRUE once i2f(delay) has run past the
    # LATEST kill (retail's fix math, sexp.cc:3334)
    check("partial list not destroyed",
          w.is_destroyed_delay(["Drone 01", "Drone 02"], 0, 10 * 65536), 0)
    check("destroyed, zero delay",
          w.is_destroyed_delay(["Drone 01"], 0, 5 * 65536), VM.KNOWN_TRUE)
    check("delay still running",
          w.is_destroyed_delay(["Drone 01"], 3, 7 * 65536), 0)
    check("delay elapsed",
          w.is_destroyed_delay(["Drone 01"], 3, 8 * 65536), VM.KNOWN_TRUE)
    check("unknown name never destroyed",
          w.is_destroyed_delay(["Ghost"], 0, 99 * 65536), 0)

    # hits-left NAN codes
    check("hits-left of the dead", w.hits_left("Drone 01"),
          VM.NAN_FOREVER)
    check("hits-left of the unknown", w.hits_left("Ghost"), VM.NAN_)
    check("hits-left untouched ship", w.hits_left("Drone 02"), 100)

    if failed == 0:
        print("OK weapons: cadence, swept collision, hull ledger, log queries")
    quit(1 if failed > 0 else 0)
