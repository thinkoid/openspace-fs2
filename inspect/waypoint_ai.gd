# -*- mode: gdscript -*-
#
# WaypointAI -- the sliver of retail's AI that Training-1's lesson chain
# gates on: ships fly waypoint paths, stand still, or play dead, and
# completing a path stamps the mission log that
# `are-waypoints-done-delay` reads. Pure logic in the FS2 frame; the
# scene feeds ship parameters and mirrors the motion.
#
# What is retail-exact: the arrival test (aicode.cc:4717 -- within
# MIN_DIST_TO_WAYPOINT_GOAL 5.0 + sqrt(radius) of the waypoint, plus
# the swept segment of this frame's travel, so a fast ship cannot step
# over a waypoint), the LOG_WAYPOINTS_DONE stamp on the LAST waypoint
# only (aicode.cc:4763), case-insensitive log queries
# (missionlog.cc:423 stricmp), and the predicate's fix math + the
# destroyed-only-checked-when-not-done quirk (sexp.cc:3562 -- a ship
# destroyed AFTER finishing its path stays KNOWN_TRUE, retail's own
# comment flags it).
#
# What is approximation, deliberately: the flying itself. Retail turns
# through the full rotational physics (aicode's ai_turn_towards_vector
# stack); here the forward vector rotates toward the waypoint at the
# ship's yaw rate and the hull moves along it at retail's own commanded
# speed: DISTANCE-PROPORTIONAL, dist/5 for small ships (aicode.cc:4687,
# the dot-alignment shaping dropped -- our turn is quick), clipped by
# the ship's max and by cap-waypoint-speed (aicode.cc:4702, only a
# positive cap applies). Ships glide in rather than charging at max
# everywhere -- flat max_vel.z was field-reported as "very animated". Believable motion that hits the same waypoints in the same
# order; the maneuvering polish belongs to a real AI slice if one is
# ever needed. On finishing a path the ship PARKS: retail completes
# the goal and an orderless ship idles, and the next add-goal may be
# MINUTES away, gated on the player's own lesson progress -- a ship
# that kept cruising meanwhile would leave the training area entirely
# (measured: the Instructor did exactly that at 75 m/s).
class_name WaypointAI
extends RefCounted

const VM := preload("res://sexp_vm.gd")   # SEXP codes + fix math
const Weap := preload("res://weapons.gd") # the swept segment test

const MIN_DIST_TO_WAYPOINT_GOAL := 5.0    # aicode.cc:167

var lists := {}          # path name -> [Vector3...] (FS2 frame)
var ships := {}          # name -> {pos, fvec, speed, turn, radius,
                         #          mode, path, idx}
var done := []           # the log: [{ship, path, time}], order kept

func set_lists(waypoint_lists: Array) -> void:
    for w in waypoint_lists:
        lists[w["name"]] = w["points"]

func register(name: String, pos: Vector3, fvec: Vector3, speed: float,
              turn: float, radius: float) -> void:
    ships[name] = {"pos": pos, "fvec": fvec.normalized(), "speed": speed,
                   "turn": turn, "radius": radius, "cap": 0.0,
                   "cur_speed": 0.0, "mode": "still", "path": "", "idx": 0}

# cap-waypoint-speed (sexp.cc:5584): a positive cap limits waypoint
# flight; anything else clears it (retail stores -1 for none)
func set_speed_cap(name: String, cap: float) -> void:
    if ships.has(name):
        ships[name]["cap"] = maxf(cap, 0.0)

func _find_list(path_name: String) -> String:
    for name in lists:
        if name.nocasecmp_to(path_name) == 0:
            return name
    return ""

# an order lands: fly a path from its first point, or hold. Unknown
# ships and paths are refused (the caller logs)
func command(name: String, verb: String, path_name := "") -> bool:
    if not ships.has(name):
        return false
    var s: Dictionary = ships[name]
    match verb:
        "waypoints-once":
            var list := _find_list(path_name)
            if list == "":
                return false
            s["mode"] = "waypoints"
            s["path"] = list
            s["idx"] = 0
        "stay-still", "play-dead":
            s["mode"] = "still"
        _:
            return false
    return true

# one sim step; returns completions [{ship, path}] for the scene's
# feedback. Motion order matches retail's frame: turn, then move, then
# the arrival test against the travel just made.
func step(delta: float, mt_fix: int) -> Array:
    var completions := []
    for name in ships:
        var s: Dictionary = ships[name]
        if s["mode"] == "still":
            continue

        var wp: Vector3 = lists[s["path"]][s["idx"]]

        # yaw-rate-limited turn toward the waypoint
        var desired: Vector3 = (wp - s["pos"]).normalized()
        var angle: float = s["fvec"].angle_to(desired)
        var max_step: float = s["turn"] * delta
        if angle > max_step:
            var axis: Vector3 = s["fvec"].cross(desired)
            if axis.length_squared() < 1e-9:
                axis = Vector3.UP   # dead astern: any perpendicular works
            s["fvec"] = s["fvec"].rotated(axis.normalized(),
                                          max_step).normalized()
        else:
            s["fvec"] = desired

        # retail's commanded speed: dist/5, clipped by max and cap
        var target_speed: float = minf(
            s["pos"].distance_to(wp) / 5.0, s["speed"])
        if s["cap"] > 0.0:
            target_speed = minf(target_speed, s["cap"])
        s["cur_speed"] = target_speed

        var from: Vector3 = s["pos"]
        s["pos"] = from + s["fvec"] * target_speed * delta

        # retail's arrival test, both arms (aicode.cc:4717): raw
        # proximity padded by this frame's travel, or the swept segment
        var near := MIN_DIST_TO_WAYPOINT_GOAL + sqrt(s["radius"])
        var moved: float = (s["pos"] - from).length()
        if s["pos"].distance_to(wp) < near + moved \
                or Weap._segment_hits(from, s["pos"], wp, near):
            s["idx"] += 1
            if s["idx"] >= lists[s["path"]].size():
                done.append({"ship": name, "path": s["path"],
                             "time": mt_fix})
                completions.append({"ship": name, "path": s["path"]})
                s["mode"] = "still"
    return completions

func speed_of(name: String) -> float:
    if not ships.has(name):
        return 0.0
    var s: Dictionary = ships[name]
    return 0.0 if s["mode"] == "still" else s["cur_speed"]

# the mover's velocity vector (the lead indicator aims with it)
func velocity_of(name: String) -> Vector3:
    if not ships.has(name):
        return Vector3.ZERO
    var s: Dictionary = ships[name]
    return Vector3.ZERO if s["mode"] == "still" \
        else s["fvec"] * s["cur_speed"]

# mission_log_get_time for LOG_WAYPOINTS_DONE: first entry matching
# both names, case-insensitive (missionlog.cc:423)
func done_time(ship: String, path: String) -> int:
    for d in done:
        if d["ship"].nocasecmp_to(ship) == 0 \
                and d["path"].nocasecmp_to(path) == 0:
            return d["time"]
    return -1

# sexp_are_waypoints_done_delay (sexp.cc:3543): done + fix delay ->
# KNOWN_TRUE; destroyed is only consulted when the log has NO entry
# (retail's quirk -- destruction after the fact never revokes)
func are_waypoints_done_delay(ship: String, path: String, delay_s: int,
                              mt_fix: int, destroyed: Dictionary) -> int:
    var t := done_time(ship, path)
    if t >= 0:
        if mt_fix - t >= VM.i2f(delay_s):
            return VM.KNOWN_TRUE
        return 0
    for name in destroyed:
        if String(name).nocasecmp_to(ship) == 0:
            return VM.KNOWN_FALSE
    return 0
