#!/bin/sh
#
# The effects-art gate: bake EVERY .ani under the install's data/effects
# with ani2png (retail's own anim code decoding), then re-derive each one
# with tests/check_ani.py -- an independent from-scratch python reader --
# and demand pixel-for-pixel agreement plus matching sidecar facts. The
# pcx_dump/check_tex pattern, applied to the flipbooks.
#
#   ani_check.sh <ani2png> <checker> <repo-root>
#
# Needs the unpacked install; skips (77) otherwise.

set -eu

tool=$1
checker=$2
repo=$3

root=${FS2_GAME_ROOT:-}
if [ -z "$root" ] && [ -d "$repo/../rundir" ]; then
    root=$repo/../rundir
fi

if [ -z "$root" ] || ! ls "$root"/data/effects/*.ani > /dev/null 2>&1; then
    echo "no unpacked install with effects anis -- set FS2_GAME_ROOT; skipping"
    exit 77
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

rc=0
total=0
for f in "$root"/data/effects/*.ani; do
    total=$((total + 1))
    name=$(basename "$f" .ani)
    if ! "$tool" "$root" "$tmp" "$name" > /dev/null 2>&1; then
        echo "FAIL: ani2png could not bake $name"
        rc=1
        continue
    fi
    if ! python3 "$checker" "$f" "$tmp"; then
        rc=1
    fi
done

echo "$total effects anis baked and cross-checked"
exit $rc
