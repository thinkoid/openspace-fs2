#!/bin/sh
#
# Load pof2glb's .tres output for the inspection-slice models through the real
# engine: check_tres_load.gd runs under `godot --headless --script` with
# --path pointed at inspect/ (so the .tres's res://ship_data.gd reference
# resolves) and validates every schema field. Closes the gap tres-check leaves
# open -- that gate cross-checks the numbers with a *text* parser and never
# proves Godot itself accepts the file.
#
#   tres_load_check.sh <pof2glb-binary> <check_tres_load.gd> <repo-root> [search-root]
#
# Gates on godot being installed AND on the same unpacked install as the
# sibling gates ($FS2_GAME_ROOT or the workspace rundir/), else skip (77).

set -eu

pof2glb=$1
checker=$2
repo=$3
search=${4:-}

command -v godot > /dev/null 2>&1 || {
    echo "godot not installed; skipping"
    exit 77
}

root=${FS2_GAME_ROOT:-}
if [ -z "$root" ] && [ -n "$search" ] && [ -d "$search/../rundir" ]; then
    root=$search/../rundir
fi

if [ -z "$root" ] || [ ! -d "$root/data/models" ] || [ ! -d "$root/data/maps" ]; then
    echo "no unpacked model+maps dir found -- set FS2_GAME_ROOT; skipping"
    exit 77
fi

echo "game root: $root"

# the same inspection slice as the other gates
slice="fighter01 science01"

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

rc=0
for m in $slice; do
    pof=$root/data/models/$m.pof
    if [ ! -f "$pof" ]; then
        echo "skip $m  (no $pof)"
        continue
    fi

    mkdir -p "$tmp/$m"
    "$pof2glb" "$pof" "$tmp/$m/$m.glb" > /dev/null

    echo "$m:"
    if godot --headless --path "$repo/inspect" --script "$checker" \
            -- "$tmp/$m/$m.tres"; then
        :
    else
        rc=1
    fi
done

exit $rc
