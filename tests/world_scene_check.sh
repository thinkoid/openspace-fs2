#!/bin/sh
#
# The reconciler gate: world.tscn boots headless on the NATIVE Training-1
# (libfs2 via FS2_GDEXT), with the Myrmidon converted fresh by pof2glb,
# and check_world_scene.gd asserts the world made it onto the scene tree
# -- ships as nodes, the Instructor's node measurably flying under retail
# AI. Presentation consumes the boundary end to end: pof2glb assets,
# load(), step(), snapshot(), reconciliation.
#
#   world_scene_check.sh <pof2glb> <fs2.gdextension> <checker> <repo-root>
#
# Gates on godot + the unpacked install; skips (77) otherwise.

set -eu

pof2glb=$1
gdext=$2
checker=$3
repo=$4

command -v godot > /dev/null 2>&1 || {
    echo "godot not installed; skipping"
    exit 77
}

root=${FS2_GAME_ROOT:-}
if [ -z "$root" ] && [ -d "$repo/../rundir" ]; then
    root=$repo/../rundir
fi

if [ -z "$root" ] || [ ! -d "$root/data/models" ] || \
   [ -z "$(find "$root/data/missions" -maxdepth 1 -iname 'training-1.fs2' \
           2> /dev/null)" ]; then
    echo "no unpacked install with training-1.fs2 -- set FS2_GAME_ROOT; skipping"
    exit 77
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# the one class Training-1 needs at t=0 (both t=0 ships are Myrmidons)
"$pof2glb" "$root/data/models/fighter2t-05.pof" "$tmp/fighter2t-05.glb" \
    > /dev/null

FS2_GDEXT=$gdext godot --headless --path "$repo/inspect" \
    --script "$checker" -- world training-1.fs2 "$tmp" "$root"
