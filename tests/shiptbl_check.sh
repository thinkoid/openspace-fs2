#!/bin/sh
#
# The ships.tbl gate: shiptbl2tres extracts the flight parameters through
# retail's own parse_shiptbl; check_shiptbl.py re-reads the same table text
# with an independent python parse (incl. the 2*PI/rotation_time derivation
# in retail's float32 arithmetic) and the slice ships' fields must agree
# exactly. The tool also runs twice and the outputs compare byte-for-byte
# -- the pipeline's reproducibility promise again.
#
#   shiptbl_check.sh <shiptbl2tres> <check_shiptbl.py> [search-root]
#
# Needs the unpacked install's data/tables; skips (77) without it.

set -eu

tool=$1
checker=$2
search=${3:-}

root=${FS2_GAME_ROOT:-}
if [ -z "$root" ] && [ -n "$search" ] && [ -d "$search/../rundir" ]; then
    root=$search/../rundir
fi

if [ -z "$root" ] || [ ! -f "$root/data/tables/ships.tbl" ]; then
    echo "no unpacked ships.tbl found -- set FS2_GAME_ROOT; skipping"
    exit 77
fi

echo "game root: $root"

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

"$tool" "$root" "$tmp/a.tres" > /dev/null
"$tool" "$root" "$tmp/b.tres" > /dev/null

rc=0
# the same inspection slice as the other gates
if python3 "$checker" "$tmp/a.tres" "$root/data/tables/ships.tbl" \
        fighter01 science01 capital01; then
    :
else
    rc=1
fi

if cmp -s "$tmp/a.tres" "$tmp/b.tres"; then
    echo "OK: extraction is byte-identical across runs"
else
    echo "FAIL: two extractions differ"
    rc=1
fi

exit $rc
