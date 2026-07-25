#!/bin/sh
#
# Check pof2glb's GLB output for the inspection-slice models against the
# pof_dump --model oracle, via check_glb.py (structure, hierarchy, triangle
# counts, and winding vs normals -- the check that caught the winding flip).
#
#   glb_check.sh <pof2glb-binary> <pof_dump-binary> <check_glb.py> [search-root]
#
# pof2glb reads a LOOSE model file (a filesystem path, not a VP), so unlike the
# pof_dump oracle this needs an *unpacked* install: $FS2_GAME_ROOT/data/models,
# or the workspace's rundir/ next to the search root. Failing that the test
# skips (77) rather than failing -- a missing install is not a broken emitter.

set -eu

pof2glb=$1
dump=$2
checker=$3
search=${4:-}

root=${FS2_GAME_ROOT:-}
if [ -z "$root" ] && [ -n "$search" ] && [ -d "$search/../rundir" ]; then
    root=$search/../rundir
fi

if [ -z "$root" ] || [ ! -d "$root/data/models" ]; then
    echo "no unpacked model dir found -- set FS2_GAME_ROOT to an unpacked install; skipping"
    exit 77
fi

echo "game root: $root"

# The inspection slice (docs/pof-corpus-survey.txt): fighter01 (Ulysses, v2117)
# + science01 (Faustus, v2116, rotating solar panel) -- 15/16 chunk types, both
# POF versions, and real subobject rotation between them.
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
    "$pof2glb" "$pof" "$tmp/$m.glb" > /dev/null
    "$dump" --model "$root" "$m.pof" > "$tmp/$m.dump" 2>/dev/null
    printf '%-12s ' "$m"
    if python3 "$checker" "$tmp/$m.glb" "$tmp/$m.dump"; then
        :   # check_glb.py prints its own OK line
    else
        rc=1
    fi
done

exit $rc
