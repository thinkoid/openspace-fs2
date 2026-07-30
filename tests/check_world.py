#!/usr/bin/env python3
# The world gate's cross-check: sim_dump's t=0 snapshot (retail's GAME-path
# parse -- arrival cues live, deferred ships absent) against mission2tres's
# .tres (the FRED view -- every object). The native world must be a subset
# of the FRED world, agreeing exactly on identity (class/pof/team) and
# within mission-check's tolerances on placement (both sides are engine
# placements, but the FRED pass creates all ships and the game pass only
# the cue-satisfied -- dock realignment can differ across that line by
# post-pass float residue).
#
#   check_world.py <layout.txt> <mission.tres>
#
# Exit 0 clean, 1 on any failure.

import re
import sys

TOL_POS = 1e-3
TOL_ORIENT = 1e-4


def fail(msg):
    print("FAIL " + msg)
    global bad
    bad += 1


def parse_tres(path):
    text = open(path, encoding="utf-8").read()
    ships = {}
    for m in re.finditer(
            r'\{\s*"name": "([^"]*)",\s*"ship_class": "([^"]*)",\s*'
            r'"pof": "([^"]*)",\s*"team": (-?\d+),\s*'
            r'"pos": Vector3\(([^)]*)\),\s*'
            r'"rvec": Vector3\(([^)]*)\),\s*'
            r'"uvec": Vector3\(([^)]*)\),\s*'
            r'"fvec": Vector3\(([^)]*)\),\s*'
            r'"player_start": (true|false)', text):
        name = m.group(1)
        vecs = [[float(x) for x in m.group(i).split(",")] for i in (5, 6, 7, 8)]
        ships[name.lower()] = {
            "name": name,
            "class": m.group(2),
            "pof": m.group(3),
            "team": int(m.group(4)),
            "pos": vecs[0],
            "orient": vecs[1] + vecs[2] + vecs[3],
            "player": m.group(9) == "true",
        }
    return ships


def parse_layout(path):
    ships = {}
    for line in open(path, encoding="utf-8"):
        t = line.split()
        if not t or t[0] != "state":
            continue
        # state 0 <name...> sig N class <c...> pof <p> team N player N dying N
        # pos x y z orient r..u..f.. vel x y z hull h/m
        # names/classes contain spaces: split on the keywords
        m = re.match(
            r"state \d+ (.*) sig (-?\d+) class (.*) pof (\S+) team (-?\d+) "
            r"arrival (-?\d+) player (\d) dying (\d) pos (\S+) (\S+) (\S+) "
            r"orient (\S+) (\S+) (\S+) (\S+) (\S+) (\S+) (\S+) (\S+) (\S+) ",
            line)
        if not m:
            continue
        name = m.group(1)
        ships[name.lower()] = {
            "name": name,
            "class": m.group(3),
            "pof": m.group(4).removesuffix(".pof"),
            "team": int(m.group(5)),
            "arrival": int(m.group(6)),
            "player": m.group(7) == "1",
            "pos": [float(m.group(i)) for i in (9, 10, 11)],
            "orient": [float(m.group(i)) for i in range(12, 21)],
        }
    return ships


bad = 0

layout = parse_layout(sys.argv[1])
fred = parse_tres(sys.argv[2])

if not layout:
    fail("empty native layout")

for key, native in layout.items():
    if key not in fred:
        fail("native ship '%s' unknown to the FRED view" % native["name"])
        continue
    ref = fred[key]

    # pof compares case-folded (mission2tres lowercases stems; Ship_info
    # carries the table's own case). player compares one-way: FRED marks
    # EVERY start OF_PLAYER_SHIP, the game path marks exactly the one
    # being flown -- a native player must be a FRED player, not vice versa.
    for field in ("class", "team"):
        if native[field] != ref[field]:
            fail("%s %s: native %r, FRED %r" %
                 (native["name"], field, native[field], ref[field]))
    if native["pof"].lower() != ref["pof"].lower():
        fail("%s pof: native %r, FRED %r" %
             (native["name"], native["pof"], ref["pof"]))
    if native["player"] and not ref["player"]:
        fail("%s: native player, FRED disagrees" % native["name"])

    # a nonzero arrival location (near ship, in front, dock bay, hyperspace
    # warp-in) means the GAME repositions the ship at creation -- the FRED
    # view keeps the authored coordinates, so placement is only comparable
    # for at-location ships
    if native["arrival"] == 0:
        dp = max(abs(a - b) for a, b in zip(native["pos"], ref["pos"]))
        do = max(abs(a - b) for a, b in zip(native["orient"], ref["orient"]))
        if dp > TOL_POS:
            fail("%s pos diverges %g" % (native["name"], dp))
        if do > TOL_ORIENT:
            fail("%s orient diverges %g" % (native["name"], do))

# the player start is never behind an arrival cue -- exactly one, present
players = sum(1 for s in layout.values() if s["player"])
if players != 1:
    fail("expected exactly one native player ship, found %d" % players)

if bad:
    sys.exit(1)

print("OK: %d/%d ships in the native world, identity exact, placement "
      "within tolerance" % (len(layout), len(fred)))
