#!/bin/sh
#
# The effects-art gate: bake EVERY .ani under the install's data/effects
# with ani2png (retail's own anim code decoding), then re-derive each one
# with tests/check_ani.py -- an independent from-scratch python reader --
# and demand pixel-for-pixel agreement plus matching sidecar facts. The
# pcx_dump/check_tex pattern, applied to the flipbooks.
#
# The still PCX art (laser bodies, glows, beam sections) sweeps the same
# way: baked as one-frame atlases, cross-checked against pcx_dump's
# retail decode via the checker's --still mode.
#
#   ani_check.sh <ani2png> <pcx_dump> <checker> <repo-root>
#
# Needs the unpacked install; skips (77) otherwise.

set -eu

tool=$1
pcx_dump=$2
checker=$3
repo=$4

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

# color art (effects) bakes through the palette; interface art (hud)
# bakes --aa -- GR_AABITMAP's reading, index as alpha
sweep_anis() {
    dir=$1
    flag=$2
    n=0
    for f in "$root"/data/"$dir"/*.ani "$root"/data/"$dir"/*.ANI; do
        [ -f "$f" ] || continue
        n=$((n + 1))
        name=$(basename "$f")
        name=${name%.*}            # case-blind strip: 2_BLAST.ANI too
        if ! "$tool" $flag "$root" "$tmp" "$name" > /dev/null 2>&1; then
            echo "FAIL: ani2png could not bake $name"
            rc=1
            continue
        fi
        if ! python3 "$checker" $flag "$f" "$tmp"; then
            rc=1
        fi
    done
    echo "$n $dir anis baked and cross-checked"
}

sweep_stills() {
    dir=$1
    flag=$2
    n=0
    for f in "$root"/data/"$dir"/*.pcx "$root"/data/"$dir"/*.PCX; do
        [ -f "$f" ] || continue
        n=$((n + 1))
        name=$(basename "$f")
        name=${name%.*}
        if ! "$tool" $flag "$root" "$tmp" "$name" > /dev/null 2>&1; then
            echo "FAIL: ani2png could not bake still $name"
            rc=1
            continue
        fi
        if ! "$pcx_dump" "$root" "$tmp" "$name" > /dev/null 2>&1; then
            echo "FAIL: pcx_dump could not decode $name"
            rc=1
            continue
        fi
        stem=$(echo "$name" | tr '[:upper:]' '[:lower:]')
        if ! python3 "$checker" $flag --still "$tmp/$stem.idx" "$tmp"; then
            rc=1
        fi
    done
    echo "$n $dir stills baked and cross-checked"
}

sweep_anis effects ""
sweep_anis hud --aa
sweep_stills effects ""
sweep_stills hud --aa
exit $rc
