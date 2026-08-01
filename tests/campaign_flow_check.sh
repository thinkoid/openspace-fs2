#!/bin/sh
#
# The campaign-flow gate: the synthetic two-hop campaign (flow.fc2 -- the
# range mission twice, advance gated on the "Clear the range" goal)
# played end to end natively across three PROCESSES:
#
#   hop 1  starts fresh at flow-1, clears the range, and the goal-driven
#          branch must pick flow-2; accept writes the .csg
#   hop 2  a new process must RESUME at flow-2 (the .csg round-trip),
#          clear it, and hit end-of-campaign
#   hop 3  a third process must find the campaign complete
#
# The campaign files are staged into the scratch XDG data home -- cfile's
# root-0 shadow carries them, proving the layered-roots mechanism along
# the way. A retail smoke closes: hands-off Training-1 must FAIL its goal,
# select the failure debrief stage, and branch back onto itself WITHOUT
# writing a .csg (retail's repeat-erases-the-record semantics).
#
#   campaign_flow_check.sh <sim_dump> <repo-root>
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

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# hermetic pilot: libfs2 boots against the XDG homes, so point both at
# scratch -- the gate must neither read nor write the real Commander
export XDG_DATA_HOME="$tmp/xdg-data" XDG_CONFIG_HOME="$tmp/xdg-config"

# stage the campaign into the data home: root 0 shadows the retail tree
mkdir -p "$tmp/xdg-data/fs2/data/missions"
cp "$repo/tests/flow.fc2" "$tmp/xdg-data/fs2/data/missions/"
cp "$repo/tests/weapons-range.fs2" "$tmp/xdg-data/fs2/data/missions/flow-1.fs2"
cp "$repo/tests/weapons-range.fs2" "$tmp/xdg-data/fs2/data/missions/flow-2.fs2"

csg="$tmp/xdg-data/fs2/data/players/single/Commander Jameson.flow.csg"

rc=0

# ---- hop 1: fresh start, clear the range, branch on the goal
"$sim" "$root" flow campaign 3600 > "$tmp/hop1.txt" 2> /dev/null

if grep -q "^campaign flow mission 'flow-1\.fs2'$" "$tmp/hop1.txt"; then
    echo "OK: hop 1 starts at flow-1"
else
    echo "FAIL: hop 1 did not start at flow-1: $(grep -m1 '^campaign' "$tmp/hop1.txt" || echo none)"
    rc=1
fi

if grep -q "^goal 'Clear the range' type 0 status 1 invalid 0" "$tmp/hop1.txt"; then
    echo "OK: $(grep -m1 '^goal' "$tmp/hop1.txt")"
else
    echo "FAIL: range goal not complete in hop 1: $(grep -m1 '^goal' "$tmp/hop1.txt" || echo none)"
    rc=1
fi

if grep -q "^verdict next 'flow-2\.fs2' loop 0$" "$tmp/hop1.txt" \
   && grep -q "^current 'flow-2\.fs2'$" "$tmp/hop1.txt"; then
    echo "OK: goal-driven branch picked flow-2"
else
    echo "FAIL: branch verdict wrong: $(grep -m1 '^verdict' "$tmp/hop1.txt" || echo none)"
    rc=1
fi

if [ -f "$csg" ]; then
    echo "OK: campaign save written ($(basename "$csg"))"
else
    echo "FAIL: no .csg after hop 1 accept"
    rc=1
fi

# ---- hop 2: a new process resumes from the .csg, finishes the campaign
"$sim" "$root" flow campaign 3600 > "$tmp/hop2.txt" 2> /dev/null

if grep -q "^campaign flow mission 'flow-2\.fs2'$" "$tmp/hop2.txt"; then
    echo "OK: hop 2 resumed at flow-2 (.csg round-trip)"
else
    echo "FAIL: hop 2 did not resume at flow-2: $(grep -m1 '^campaign' "$tmp/hop2.txt" || echo none)"
    rc=1
fi

if grep -q "^verdict next '' loop 0$" "$tmp/hop2.txt" \
   && grep -q "^current ''$" "$tmp/hop2.txt"; then
    echo "OK: end-of-campaign reached"
else
    echo "FAIL: campaign did not end after flow-2: $(grep -m1 '^verdict' "$tmp/hop2.txt" || echo none)"
    rc=1
fi

# ---- hop 3: the finished campaign stays finished
"$sim" "$root" flow campaign 3600 > "$tmp/hop3.txt" 2> /dev/null

if grep -q "^campaign complete$" "$tmp/hop3.txt"; then
    echo "OK: hop 3 reports campaign complete"
else
    echo "FAIL: completed campaign restarted: $(grep -m1 '^campaign' "$tmp/hop3.txt" || echo none)"
    rc=1
fi

# ---- retail smoke: hands-off Training-1 fails its goal, repeats, and
# the repeat path writes NO save
"$sim" "$root" FreeSpace2 campaign 600 > "$tmp/retail.txt" 2> /dev/null

if grep -q "^campaign FreeSpace2 mission 'Training-1\.fs2'$" "$tmp/retail.txt" \
   && grep -q "^goal 'Flight Training' type 0 status 0" "$tmp/retail.txt" \
   && grep -q "^verdict next 'Training-1\.fs2' loop 0$" "$tmp/retail.txt"; then
    echo "OK: retail Training-1 fails hands-off and repeats"
else
    echo "FAIL: retail campaign smoke wrong:"
    grep -E "^campaign|^goal|^verdict" "$tmp/retail.txt" || echo "  (no output)"
    rc=1
fi

if grep -q "^stage 0 voice TR1_DB_02\.wav" "$tmp/retail.txt"; then
    echo "OK: $(grep -m1 '^stage 0' "$tmp/retail.txt")"
else
    echo "FAIL: failure debrief stage not selected: $(grep -m1 '^stage' "$tmp/retail.txt" || echo none)"
    rc=1
fi

if [ ! -f "$tmp/xdg-data/fs2/data/players/single/Commander Jameson.FreeSpace2.csg" ]; then
    echo "OK: repeat branch wrote no .csg (retail's erase-the-record semantics)"
else
    echo "FAIL: repeat branch wrote a campaign save"
    rc=1
fi

exit $rc
