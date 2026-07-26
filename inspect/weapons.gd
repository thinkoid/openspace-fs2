# -*- mode: gdscript -*-
#
# Weapons -- the player's gun, its projectiles in flight, and the hull
# ledger they spend down: the runtime half of the weapons slice (the data
# half is ship_params.tres ballistics through retail's weapon_init). Pure
# logic in the FS2 frame; the scene feeds ship positions each step and
# renders the bolts.
#
# The collision call, made deliberately: swept segment against the ship's
# POF bounding sphere, NOT retail's model_collide BSP walk. A Subach bolt
# crosses ~7.5 m per 60 Hz frame -- a point-in-sphere test would tunnel
# straight through a drone -- so each step tests the closest approach of
# the bolt's travel segment to the hull sphere. Polygon accuracy belongs
# with the model subsystem if a slice ever needs it; against fighter-sized
# spheres the error is a few meters of generosity.
#
# The destroyed registry keys the mission-log SEXPs exactly as retail's
# missionlog.cc does by LOG_SHIP_DESTROYED time: is-destroyed-delay
# (sexp.cc:3314) wants ALL names destroyed and Missiontime - latest >=
# i2f(delay); hits-left (sexp.cc:3835) is int(100 * hull / initial), NAN
# once the ship is gone. Departure never happens in this world, so the
# KNOWN_FALSE departed branches are unreachable and omitted.
class_name Weapons
extends RefCounted

const VM := preload("res://sexp_vm.gd")   # the SEXP return codes + fix math

var gun := {}                    # {velocity, damage, lifetime, fire_wait}
var ships := {}                  # name -> {radius, hull, max_hull,
                                 #          invulnerable}
var projectiles := []            # [{pos, vel, expire_ms}]
var destroyed := {}              # name -> mt_fix at the kill
var next_fire_ms := 0

func setup(gun_params: Dictionary) -> void:
    gun = gun_params

func add_ship(name: String, radius: float, max_hull: float,
              invulnerable := false) -> void:
    ships[name] = {"radius": radius, "hull": max_hull, "max_hull": max_hull,
                   "invulnerable": invulnerable}

# holds-to-fire at the gun's own cadence (retail fire_wait gates the
# trigger the same way); the bolt inherits the shooter's velocity
func try_fire(now_ms: int, muzzle: Vector3, dir: Vector3,
              parent_vel: Vector3) -> bool:
    if gun.is_empty() or now_ms < next_fire_ms:
        return false
    next_fire_ms = now_ms + int(gun["fire_wait"] * 1000.0)
    projectiles.append({
        "pos": muzzle,
        "vel": dir.normalized() * gun["velocity"] + parent_vel,
        "expire_ms": now_ms + int(gun["lifetime"] * 1000.0),
    })
    return true

# closest approach of the travel segment [a, b] to the sphere center
static func _segment_hits(a: Vector3, b: Vector3, center: Vector3,
                          radius: float) -> bool:
    var ab := b - a
    var t := 0.0
    var len2 := ab.length_squared()
    if len2 > 0.0:
        t = clampf((center - a).dot(ab) / len2, 0.0, 1.0)
    return (a + ab * t - center).length_squared() <= radius * radius

# one sim step: fly every bolt, spend hulls, retire what hit or expired.
# Returns hit records [{name, killed}] for the scene's feedback.
func step(delta: float, now_ms: int, mt_fix: int,
          positions: Dictionary) -> Array:
    var hits := []
    var live := []
    for p in projectiles:
        var from: Vector3 = p["pos"]
        var to: Vector3 = from + p["vel"] * delta
        p["pos"] = to

        var struck := ""
        for name in ships:
            if destroyed.has(name) or not positions.has(name):
                continue
            if _segment_hits(from, to, positions[name],
                             ships[name]["radius"]):
                struck = name
                break

        if struck != "":
            # an invulnerable hull (the Instructor) stops the bolt but
            # spends nothing (OF_INVULNERABLE, shiphit.cc:1687)
            var s: Dictionary = ships[struck]
            var killed := false
            if not s["invulnerable"]:
                s["hull"] -= gun["damage"]
                killed = s["hull"] <= 0.0
                if killed:
                    destroyed[struck] = mt_fix
            hits.append({"name": struck, "killed": killed})
            continue

        if now_ms < p["expire_ms"]:
            live.append(p)
    projectiles = live
    return hits

# ---- the mission-log queries the SEXP ops read ----

# sexp_is_destroyed_delay: 0 while any name still stands, KNOWN_TRUE once
# all are down and the delay has run past the LATEST kill time
func is_destroyed_delay(names: Array, delay_s: int, mt_fix: int) -> int:
    var latest := 0
    for name in names:
        if not destroyed.has(name):
            return 0
        latest = maxi(latest, destroyed[name])
    if mt_fix - latest >= VM.i2f(delay_s):
        return VM.KNOWN_TRUE
    return 0

# sexp_hits_left: percentage of the whole; NAN_FOREVER once destroyed,
# NAN for a name this world never had
func hits_left(name: String) -> int:
    if destroyed.has(name):
        return VM.NAN_FOREVER
    if not ships.has(name):
        return VM.NAN_
    var s: Dictionary = ships[name]
    return int(100.0 * s["hull"] / s["max_hull"])
