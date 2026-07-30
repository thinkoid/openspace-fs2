#!/bin/sh
#
# The native weapons gate: sim_dump's fire mode on the synthetic range --
# aim assist through the boundary, trigger held -- must produce a KILL
# recorded by retail's own mission log (LOG_SHIP_DESTROYED, type 1, a
# drone by Alpha 1): retail firing cadence, bolt flight, BSP
# model_collide, damage and destruction, end to end natively. Two runs
# must be byte-identical (the fire path joins the determinism contract).
#
#   weapons_native_check.sh <sim_dump> <repo-root>
#
# Needs the unpacked install (tables + models); skips (77) otherwise.

set -eu

sim=$1
repo=$2

root=${FS2_GAME_ROOT:-}
if [ -z "$root" ] && [ -d "$repo/../rundir" ]; then
    root=$repo/../rundir
fi

if [ -z "$root" ] || [ ! -d "$root/data/tables" ]; then
    echo "no unpacked install found -- set FS2_GAME_ROOT; skipping"
    exit 77
fi

mission=$repo/tests/weapons-range.fs2

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

"$sim" "$root" "$mission" fire 3600 3600 > "$tmp/fire.txt" 2> /dev/null
"$sim" "$root" "$mission" fire 3600 3600 > "$tmp/fire2.txt" 2> /dev/null

rc=0

if ! cmp -s "$tmp/fire.txt" "$tmp/fire2.txt"; then
    echo "FAIL: two firing runs differ -- determinism broken"
    rc=1
fi

if grep -q "^event [0-9]* log 1 'Drone [0-9]*' 'Alpha 1'" "$tmp/fire.txt"; then
    echo "OK: $(grep -m1 "log 1 'Drone" "$tmp/fire.txt")"
else
    echo "FAIL: no drone kill by Alpha 1 in 60 sim-seconds"
    rc=1
fi

# the sounds-as-events seam: every shot requests its launch wav, every
# hit its positioned impact wav
if grep -q "^event [0-9]* sound L_Sidearm" "$tmp/fire.txt" \
   && grep -q "^event [0-9]* sound hit_1\.wav at " "$tmp/fire.txt"; then
    echo "OK: $(grep -c '^event [0-9]* sound' "$tmp/fire.txt") sound events (launch + positioned impacts)"
else
    echo "FAIL: firing produced no sound events"
    rc=1
fi

exit $rc
