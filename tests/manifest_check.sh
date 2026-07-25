#!/bin/sh
#
# Check pof2glb's manifest for the inspection-slice models. check_manifest.py
# recomputes every SHA-256 with python hashlib (the independent oracle for the
# converter's hand-rolled FIPS 180-4), requires every listed source and output
# to exist and match, and fails on any recorded warning. On top of that, each
# model is converted TWICE and the two manifests compared byte-for-byte -- the
# pipeline's reproducibility promise (docs/godot-migration-plan.md, "Automate
# the pipeline") made checkable: no timestamps, no nondeterminism.
#
#   manifest_check.sh <pof2glb-binary> <check_manifest.py> [search-root]
#
# Same unpacked-install gate as glb_check.sh/tex_check.sh: needs loose models
# and maps ($FS2_GAME_ROOT or the workspace rundir/), else skip (77).

set -eu

pof2glb=$1
checker=$2
search=${3:-}

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
slice="fighter01 science01 capital01"

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

rc=0
for m in $slice; do
    pof=$root/data/models/$m.pof
    if [ ! -f "$pof" ]; then
        echo "skip $m  (no $pof)"
        continue
    fi

    mkdir -p "$tmp/$m/a" "$tmp/$m/b"
    "$pof2glb" "$pof" "$tmp/$m/a/$m.glb" > /dev/null
    "$pof2glb" "$pof" "$tmp/$m/b/$m.glb" > /dev/null

    echo "$m:"
    if python3 "$checker" "$tmp/$m/a/$m.manifest.json"; then
        :
    else
        rc=1
    fi

    if cmp -s "$tmp/$m/a/$m.manifest.json" "$tmp/$m/b/$m.manifest.json"; then
        echo "OK: regeneration is byte-identical"
    else
        echo "FAIL: two conversions produced differing manifests"
        rc=1
    fi
done

exit $rc
