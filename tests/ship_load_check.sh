#!/bin/sh
#
# The game-side consumption gate: convert EVERY retail model, then one
# headless engine boot assembles each as a Ship (inspect/ship.gd) and
# cross-checks its replay of retail's load-time movement reinterpretation
# against pof_dump --model -- a corpus-wide differential oracle for the one
# piece of retail semantics the Godot side reimplements. Submodel-node
# resolution and the rotator/turret views are proven in the same pass.
#
#   ship_load_check.sh <pof2glb> <pof_dump> <check_ship_load.gd> <repo-root> \
#       [search-root]
#
# Gates on godot AND the unpacked install, like tres_load_check.sh; skips 77.

set -eu

pof2glb=$1
dump=$2
checker=$3
repo=$4
search=${5:-}

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

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

dirs=
for pof in "$root"/data/models/*.pof; do
    m=$(basename "$pof" .pof)
    mkdir -p "$tmp/$m"
    "$pof2glb" "$pof" "$tmp/$m/$m.glb" > /dev/null 2>&1
    "$dump" --model "$root" "$m.pof" > "$tmp/$m/$m.dump" 2>/dev/null
    dirs="$dirs $tmp/$m"
done

# shellcheck disable=SC2086
godot --headless --path "$repo/inspect" --script "$checker" -- $dirs
