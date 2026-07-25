#!/bin/sh
#
# Check pof2glb's .tres ship data for the inspection-slice models against the
# pof_dump --model oracle, via check_tres.py (weapon/thruster/dock/path points,
# turrets, eyes, shield -- every coordinate through the axis map). The .tres
# rides beside the .glb pof2glb writes, same stem.
#
#   tres_check.sh <pof2glb-binary> <pof_dump-binary> <check_tres.py> [search-root]
#
# Like glb_check.sh this needs an *unpacked* install (pof2glb reads loose model
# files): $FS2_GAME_ROOT/data/models, or the workspace's rundir/ next to the
# search root. Failing that the test skips (77) -- a missing install is not a
# broken emitter.

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

# Same inspection slice as glb_check.sh: fighter01 (Ulysses) + science01
# (Faustus, rotating solar panel) -- both POF versions, real subobject rotation.
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
    # pof2glb writes <stem>.glb and <stem>.tres together; we only check the .tres.
    "$pof2glb" "$pof" "$tmp/$m.glb" > /dev/null
    "$dump" --model "$root" "$m.pof" > "$tmp/$m.dump" 2>/dev/null
    printf '%-12s ' "$m"
    if python3 "$checker" "$tmp/$m.tres" "$tmp/$m.dump"; then
        :   # check_tres.py prints its own OK line
    else
        rc=1
    fi
done

exit $rc
