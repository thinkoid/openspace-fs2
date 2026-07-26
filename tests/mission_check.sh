#!/bin/sh
#
# The mission gate: every mission in the install through mission2tres
# (retail's parse_main under Fred_running) and cross-checked against
# independent python reads of the .fs2 text -- check_mission.py for the
# #Objects placement bijection (float32-exact), check_events.py for
# events/goals/messages/waypoints/ai-goals/wings. The corpus is the
# install's OWN data/missions (the bytes cfile resolves for the tool) --
# the loose missions/ checkout genuinely differs (1.2-patched text).
#
#   mission_check.sh <mission2tres> <check_mission.py> [search-root]
#                    [check_events.py]
#
# Needs the unpacked install (tables + models for ship_create);
# skips (77) otherwise.

set -eu

tool=$1
checker=$2
search=${3:-}
events_checker=${4:-}

root=${FS2_GAME_ROOT:-}
if [ -z "$root" ] && [ -n "$search" ] && [ -d "$search/../rundir" ]; then
    root=$search/../rundir
fi

if [ -z "$root" ] || [ ! -d "$root/data/tables" ]; then
    echo "no unpacked install found -- set FS2_GAME_ROOT; skipping"
    exit 77
fi
if ! ls "$root"/data/missions/*.fs2 > /dev/null 2>&1; then
    echo "no missions under $root/data/missions; skipping"
    exit 77
fi

echo "game root: $root"

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

total=0
failed=0
# the corpus plus the repo's own synthetic proving ground (weapons-range
# beside the checkers) -- cfile opens a path with a separator directly,
# so it converts in place, no staging into the install
for f in "$root"/data/missions/*.fs2 "$(dirname "$checker")"/*.fs2; do
    [ -f "$f" ] || continue
    m=$(basename "$f")
    total=$((total + 1))
    if ! "$tool" "$root" "$f" "$tmp/$m.tres" > "$tmp/$m.log" 2>&1; then
        echo "FAIL $m: mission2tres ($(tail -1 "$tmp/$m.log"))"
        failed=$((failed + 1))
        continue
    fi
    if ! python3 "$checker" "$tmp/$m.tres" "$f" > "$tmp/$m.out" 2>&1; then
        echo "FAIL $m: $(grep -m1 FAIL "$tmp/$m.out")"
        failed=$((failed + 1))
        continue
    fi
    if [ -n "$events_checker" ] && \
       ! python3 "$events_checker" "$tmp/$m.tres" "$f" > "$tmp/$m.ev" 2>&1; then
        echo "FAIL $m: $(grep -m1 FAIL "$tmp/$m.ev")"
        failed=$((failed + 1))
    fi
done

echo "missions: $((total - failed))/$total clean"
[ $failed = 0 ]
