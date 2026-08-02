#!/bin/sh
#
# The save-codec gate: savejson must round-trip the saves a real campaign
# hop writes -- .plr, .csg, .css -- byte-identically (decode | encode ==
# original), an edited field must survive the trip, and retail itself must
# accept codec output: a second campaign hop resumes from a .plr and .csg
# the codec re-wrote.
#
#   savejson_check.sh <savejson> <sim_dump> <repo-root>
#
# Needs the unpacked install (tables + models); skips (77) otherwise.

set -eu

sj=$1
sim=$2
repo=$3

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

# stage the flow campaign and fly one hop -- the saves this writes are
# the corpus: a .plr with a flown record, the .csg, the .css
mkdir -p "$tmp/xdg-data/fs2/data/missions"
cp "$repo/tests/flow.fc2" "$tmp/xdg-data/fs2/data/missions/"
for m in flow-1 flow-2 flow-loop; do
    cp "$repo/tests/weapons-range.fs2" "$tmp/xdg-data/fs2/data/missions/$m.fs2"
done

"$sim" "$root" flow campaign 3600 > "$tmp/hop1.txt" 2> /dev/null

pdir="$tmp/xdg-data/fs2/data/players/single"

rc=0

for f in "Commander Jameson.plr" \
         "Commander Jameson.flow.csg" \
         "Commander Jameson.flow.css"; do
    if [ ! -f "$pdir/$f" ]; then
        echo "FAIL: hop 1 wrote no $f"
        rc=1
        continue
    fi

    "$sj" "$pdir/$f" > "$tmp/rt.json"
    "$sj" "$tmp/rt.json" "$tmp/rt.bin"

    if cmp -s "$pdir/$f" "$tmp/rt.bin"; then
        echo "OK: $f round-trips byte-identical ($(wc -c < "$pdir/$f") bytes)"
    else
        echo "FAIL: $f round trip differs"
        rc=1
    fi
done

# the edit: change one .plr field in the JSON, encode IN PLACE, and read
# it back through the codec
"$sj" "$pdir/Commander Jameson.plr" > "$tmp/pilot.json"
sed -E 's/"mouse_sensitivity": -?[0-9]+/"mouse_sensitivity": 9/' \
    "$tmp/pilot.json" > "$tmp/edited.json"
"$sj" "$tmp/edited.json" "$pdir/Commander Jameson.plr"

if "$sj" "$pdir/Commander Jameson.plr" \
        | grep -q '"mouse_sensitivity": 9'; then
    echo "OK: edited field survives encode | decode"
else
    echo "FAIL: edited field lost"
    rc=1
fi

# retail acceptance: re-write the .csg through the codec too, then a
# second campaign hop must boot from BOTH codec-written saves and resume
# exactly where the first hop left off (flow-2)
"$sj" "$pdir/Commander Jameson.flow.csg" > "$tmp/csg.json"
"$sj" "$tmp/csg.json" "$pdir/Commander Jameson.flow.csg"

"$sim" "$root" flow campaign 3600 > "$tmp/hop2.txt" 2> /dev/null

if grep -q "^campaign flow mission 'flow-2\.fs2'$" "$tmp/hop2.txt"; then
    echo "OK: retail resumed from codec-written saves"
else
    echo "FAIL: retail rejected codec output: $(grep -m1 '^campaign' "$tmp/hop2.txt" || echo none)"
    rc=1
fi

exit $rc
