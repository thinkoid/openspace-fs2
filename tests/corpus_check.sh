#!/bin/sh
#
# The corpus gate: EVERY retail model through the converter, cross-checked
# against the retail oracle. Where the slice gates go deep on three ships,
# this goes wide over all 176 -- it is what flushed out the flat-poly parse
# gap, the collinear-sliver float32 snap, and (on its capital01 precursor)
# the jf() truncation and the vertex-normal fidelity story. Per model:
#
#   pof2glb converts (any warning fails, spherec excepted -- see below),
#   check_glb.py     structure/hierarchy/counts/winding-fidelity vs the dump,
#   check_tres.py    every ship-data field vs the dump through the axis map,
#   check_manifest.py every digest via python hashlib.
#
# Deliberately NOT corpus-wide (the slice covers them, and they are dear):
# tex-check (pcx_dump per map), tres-load-check (a godot boot per model),
# and the manifest convert-twice reproducibility compare.
#
#   corpus_check.sh <pof2glb> <pof_dump> <check_glb.py> <check_tres.py> \
#                   <check_manifest.py> [search-root]
#
# spherec is the one sanctioned warning: a retail test model whose
# nbackblue1 map lives in data/effects, not data/maps
# (docs/pof-corpus-survey.txt). Its digests still verify.
#
# Same unpacked-install gate as the slice gates ($FS2_GAME_ROOT or the
# workspace rundir/), else skip (77).

set -eu

pof2glb=$1
dump=$2
check_glb=$3
check_tres=$4
check_manifest=$5
search=${6:-}

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

total=0
failed=0
for pof in "$root"/data/models/*.pof; do
    m=$(basename "$pof" .pof)
    total=$((total + 1))
    mkdir -p "$tmp/$m"

    if ! "$pof2glb" "$pof" "$tmp/$m/$m.glb" > "$tmp/$m/convert.log" 2>&1; then
        echo "FAIL $m: pof2glb"
        failed=$((failed + 1))
        continue
    fi
    "$dump" --model "$root" "$m.pof" > "$tmp/$m/$m.dump" 2>/dev/null

    allow=
    [ "$m" = spherec ] && allow=--allow-warnings

    ok=1
    python3 "$check_glb" "$tmp/$m/$m.glb" "$tmp/$m/$m.dump" \
        > "$tmp/$m/glb.out" 2>&1 || { echo "FAIL $m: $(cat "$tmp/$m/glb.out")"; ok=0; }
    python3 "$check_tres" "$tmp/$m/$m.tres" "$tmp/$m/$m.dump" \
        > "$tmp/$m/tres.out" 2>&1 || { echo "FAIL $m: $(cat "$tmp/$m/tres.out")"; ok=0; }
    python3 "$check_manifest" $allow "$tmp/$m/$m.manifest.json" \
        > "$tmp/$m/man.out" 2>&1 || { echo "FAIL $m: $(cat "$tmp/$m/man.out")"; ok=0; }

    [ $ok = 1 ] || failed=$((failed + 1))
done

echo "corpus: $((total - failed))/$total models clean"
[ $failed = 0 ]
