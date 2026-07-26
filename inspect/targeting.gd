# -*- mode: gdscript -*-
#
# Targeting -- the player's target state and cycling, the data half of
# retail's target monitor (hudtarget.cpp, lean). Retail's hud_target_next
# walks OBJ_SHIP objects in objnum order skipping the player; hostile
# cycling filters by team. The `targeted` SEXP (sexp.cc:6528) reads this:
# name match on the current target plus an optional held-for seconds arg
# against Players_target_timestamp -- the ms clock at acquisition.
#
# Ships here are the mission's entries (inert until the AI sliver); the
# scene feeds positions for the range readout. Subsystem targeting is the
# turrets' business, another day.
class_name Targeting
extends RefCounted

var ships: Array = []            # [{name, team}] in mission (objnum) order
var target := ""                 # current target name, "" = none
var since_ms := -1               # acquisition time, the vm ms clock

func setup(entries: Array, player_name: String) -> void:
    for e in entries:
        if e["name"] != player_name:
            ships.append({"name": e["name"], "team": int(e["team"])})

func _cycle(candidates: Array, now_ms: int) -> void:
    if candidates.is_empty():
        return
    var start := -1
    for i in candidates.size():
        if candidates[i]["name"] == target:
            start = i
            break
    var next: Dictionary = candidates[(start + 1) % candidates.size()]
    target = next["name"]
    since_ms = now_ms

func next_target(now_ms: int) -> void:
    _cycle(ships, now_ms)

func next_hostile(player_team: int, now_ms: int) -> void:
    var hostiles := []
    for s in ships:
        if s["team"] != player_team:
            hostiles.append(s)
    _cycle(hostiles, now_ms)

func clear() -> void:
    target = ""
    since_ms = -1

# sexp_targeted's testable core: right name, held long enough. The caller
# handles the ship-will-never-arrive KNOWN_FALSE case (not reachable while
# every ship stands at t=0) and the subsystem arg (stubbed).
func targeted_check(name: String, delay_s: int, now_ms: int) -> bool:
    if target == "" or target.nocasecmp_to(name) != 0:
        return false
    if delay_s > 0 and now_ms < since_ms + delay_s * 1000:
        return false
    return true
