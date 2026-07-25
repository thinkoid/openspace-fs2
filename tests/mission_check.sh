#!/bin/sh
#
# The mission gate: every retail campaign mission through mission2tres
# (retail's parse_main under Fred_running) and cross-checked against an
# independent python read of the .fs2 #Objects text -- name/class/position/
# orientation/player-start as a strict bijection, float32-exact. The
# missions live loose in the workspace's missions/ checkout AND under the
# install's data/missions; the loose checkout is the corpus here.
#
#   mission_check.sh <mission2tres> <check_mission.py> [search-root]
#
# Needs the unpacked install (tables + models for ship_create) and the
# missions/ checkout next to the search root; skips (77) otherwise.

set -eu

tool=$1
checker=$2
search=${3:-}

root=${FS2_GAME_ROOT:-}
if [ -z "$root" ] && [ -n "$search" ] && [ -d "$search/../rundir" ]; then
    root=$search/../rundir
fi
missions=${FS2_MISSIONS:-}
if [ -z "$missions" ] && [ -n "$search" ] && [ -d "$search/../missions" ]; then
    missions=$search/../missions
fi

if [ -z "$root" ] || [ ! -d "$root/data/tables" ]; then
    echo "no unpacked install found -- set FS2_GAME_ROOT; skipping"
    exit 77
fi
if [ -z "$missions" ] || ! ls "$missions"/*.fs2 > /dev/null 2>&1; then
    echo "no missions checkout found -- set FS2_MISSIONS; skipping"
    exit 77
fi

echo "game root: $root"
echo "missions: $missions"

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

total=0
failed=0
for f in "$missions"/*.fs2; do
    m=$(basename "$f")
    total=$((total + 1))
    if ! "$tool" "$root" "$m" "$tmp/$m.tres" > "$tmp/$m.log" 2>&1; then
        echo "FAIL $m: mission2tres ($(tail -1 "$tmp/$m.log"))"
        failed=$((failed + 1))
        continue
    fi
    if ! python3 "$checker" "$tmp/$m.tres" "$f" > "$tmp/$m.out" 2>&1; then
        echo "FAIL $m: $(grep -m1 FAIL "$tmp/$m.out")"
        failed=$((failed + 1))
    fi
done

echo "missions: $((total - failed))/$total clean"
[ $failed = 0 ]
