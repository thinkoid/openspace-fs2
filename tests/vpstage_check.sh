#!/bin/sh
#
# The VP staging gate. vpstage extracts every model + map from the pristine
# VP archives through retail's own cfile; check_staging.py then verifies the
# staging manifest three independent ways (hashlib digests, raw
# offset-slices out of the archives bypassing cfile, byte-compare against
# the unpacked install). Finally the slice models are converted from the
# STAGED tree and must produce byte-identical GLB + .tres to a conversion
# from the unpacked install -- the pipeline gives the same answer whichever
# door the data comes in through.
#
#   vpstage_check.sh <vpstage> <pof2glb> <check_staging.py> [search-root]
#
# Needs the pristine VP root (gog/, or $FS2_VP_ROOT) *and* the unpacked
# install (rundir/, or $FS2_GAME_ROOT) next to the search root; skips (77)
# without either.

set -eu

vpstage=$1
pof2glb=$2
checker=$3
search=${4:-}

vproot=${FS2_VP_ROOT:-}
if [ -z "$vproot" ] && [ -n "$search" ] && [ -d "$search/../gog" ]; then
    vproot=$search/../gog
fi
root=${FS2_GAME_ROOT:-}
if [ -z "$root" ] && [ -n "$search" ] && [ -d "$search/../rundir" ]; then
    root=$search/../rundir
fi

if [ -z "$vproot" ] || ! ls "$vproot"/*.vp > /dev/null 2>&1; then
    echo "no VP root found -- set FS2_VP_ROOT; skipping"
    exit 77
fi
if [ -z "$root" ] || [ ! -d "$root/data/models" ] || [ ! -d "$root/data/maps" ]; then
    echo "no unpacked model+maps dir found -- set FS2_GAME_ROOT; skipping"
    exit 77
fi

echo "VP root: $vproot"
echo "game root: $root"

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

"$vpstage" "$vproot" "$tmp/staged" > /dev/null

rc=0
if python3 "$checker" "$tmp/staged/staging.manifest.json" "$root"; then
    :
else
    rc=1
fi

# the same inspection slice as the other gates, converted from both trees
slice="fighter01 science01 capital01"

for m in $slice; do
    mkdir -p "$tmp/from-staged/$m" "$tmp/from-rundir/$m"
    "$pof2glb" "$tmp/staged/data/models/$m.pof" \
        "$tmp/from-staged/$m/$m.glb" > /dev/null
    "$pof2glb" "$root/data/models/$m.pof" \
        "$tmp/from-rundir/$m/$m.glb" > /dev/null

    ok=1
    cmp -s "$tmp/from-staged/$m/$m.glb" "$tmp/from-rundir/$m/$m.glb" || ok=0
    cmp -s "$tmp/from-staged/$m/$m.tres" "$tmp/from-rundir/$m/$m.tres" || ok=0
    if [ $ok = 1 ]; then
        echo "OK: $m converts byte-identically from staged and unpacked"
    else
        echo "FAIL: $m conversion differs between staged and unpacked trees"
        rc=1
    fi
done

exit $rc
