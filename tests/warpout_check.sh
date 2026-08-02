#!/bin/sh
#
# The warpout gate: the jump key must fly retail's departure, end to end
# natively -- the staged autopilot (read_player_controls' warpout branch:
# ramp to 40, hold through the effect), shipfx's warp hole fireball, the
# stage events through retail's own sequencer queue, and the departure
# recorded by the mission log (LOG_SHIP_DEPART for Alpha 1). The abort
# run proves the stage-1 escape hatch: a second press cancels, no hole,
# no departure, the sim flies on. Two departure runs must be
# byte-identical (the warpout path joins the determinism contract).
#
#   warpout_check.sh <sim_dump> <repo-root>
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

# hermetic pilot: libfs2 boots against the XDG homes, so point both at
# scratch -- the gate must neither read nor write the real Commander
export XDG_DATA_HOME="$tmp/xdg-data" XDG_CONFIG_HOME="$tmp/xdg-config"

"$sim" "$root" "$mission" warpout 300 1500 > "$tmp/warp.txt" 2> /dev/null
"$sim" "$root" "$mission" warpout 300 1500 > "$tmp/warp2.txt" 2> /dev/null
"$sim" "$root" "$mission" warpout 300 900 abort > "$tmp/abort.txt" 2> /dev/null

rc=0

if ! cmp -s "$tmp/warp.txt" "$tmp/warp2.txt"; then
    echo "FAIL: two warpout runs differ -- determinism broken"
    rc=1
fi

# the full staircase, in order: engage, up to speed, through the effect
if [ "$(grep '^warpout stage' "$tmp/warp.txt" | tr '\n' ' ')" = \
     "warpout stage 1 warpout stage 2 warpout stage 3 " ]; then
    echo "OK: stages 1 -> 2 -> 3"
else
    echo "FAIL: stage sequence wrong: $(grep '^warpout stage' "$tmp/warp.txt" | tr '\n' ' ')"
    rc=1
fi

# the subspace notice crosses through the HUD-line seam at stage 2 (the
# hole opens), retail's own wording
if grep -q "^event [0-9]* hud 'Subspace node activated'" "$tmp/warp.txt"; then
    echo "OK: $(grep -m1 "hud 'Subspace" "$tmp/warp.txt")"
else
    echo "FAIL: no 'Subspace node activated' HUD line"
    rc=1
fi

# the warp hole is a live fireball record wearing retail's warp flipbook
if grep -q "^warpout hole up ani WarpMap01$" "$tmp/warp.txt"; then
    echo "OK: $(grep -m1 '^warpout hole' "$tmp/warp.txt")"
else
    echo "FAIL: warp hole wrong or missing: $(grep -m1 '^warpout hole' "$tmp/warp.txt" || echo none)"
    rc=1
fi

# the whoosh at engage and the hole's own positioned open sound
if grep -q "^event [0-9]* sound rev1\.wav" "$tmp/warp.txt" \
   && grep -q "^event [0-9]* sound warp_1\.wav at " "$tmp/warp.txt"; then
    echo "OK: warpout sounds crossed (whoosh + positioned hole)"
else
    echo "FAIL: warpout sounds missing"
    rc=1
fi

# the departure is the mission's own truth: LOG_SHIP_DEPART (type 5)
# for Alpha 1, and the boundary's departed flag saw it
if grep -q "^warpout departed$" "$tmp/warp.txt" \
   && grep -q "^event [0-9]* log 5 'Alpha 1'" "$tmp/warp.txt"; then
    echo "OK: $(grep -m1 "log 5 'Alpha 1'" "$tmp/warp.txt")"
else
    echo "FAIL: player departure never logged"
    rc=1
fi

# the abort: stage 1 entered, the second press cancels -- no stage 2,
# no hole, no departure
if grep -q "^warpout stage 1$" "$tmp/abort.txt" \
   && grep -q "^warpout aborted$" "$tmp/abort.txt" \
   && ! grep -q "^warpout stage 2" "$tmp/abort.txt" \
   && ! grep -q "^warpout departed" "$tmp/abort.txt"; then
    echo "OK: stage-1 abort cancels the sequence"
else
    echo "FAIL: abort path wrong: $(grep '^warpout' "$tmp/abort.txt" | tr '\n' ' ')"
    rc=1
fi

exit $rc
