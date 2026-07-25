#!/bin/sh
#
# Check pof2glb's transcoded PNG maps for the inspection-slice models against
# retail's authoritative PCX decode, via check_tex.py. pof2glb hand-rolls its
# PCX decoder (it stays libpof + stb, never links the foundation); this gate is
# what makes that safe -- pcx_dump decodes the same maps through retail's
# pcx_read_bitmap_8bpp and the pixels must agree.
#
#   tex_check.sh <pof2glb-binary> <pcx_dump-binary> <check_tex.py> [search-root]
#
# Like glb_check.sh/tres_check.sh this needs an *unpacked* install: pof2glb
# reads loose model + map files. Uses $FS2_GAME_ROOT, or the workspace's
# rundir/ next to the search root. Failing that the test skips (77).

set -eu

pof2glb=$1
pcxdump=$2
checker=$3
search=${4:-}

root=${FS2_GAME_ROOT:-}
if [ -z "$root" ] && [ -n "$search" ] && [ -d "$search/../rundir" ]; then
    root=$search/../rundir
fi

if [ -z "$root" ] || [ ! -d "$root/data/models" ] || [ ! -d "$root/data/maps" ]; then
    echo "no unpacked model+maps dir found -- set FS2_GAME_ROOT; skipping"
    exit 77
fi

echo "game root: $root"

# Same inspection slice as the other gates: fighter01 (Ulysses), science01
# (Faustus) and capital01 (GTD Orion, the turreted capship). All carry a
# keyword map plus real PCX maps.
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

    mkdir -p "$tmp/$m/idx"
    # pof2glb writes <stem>.glb + <stem>.tres + textures/<map>.png together.
    "$pof2glb" "$pof" "$tmp/$m/$m.glb" > /dev/null

    # The maps retail would actually load: the TXTR list minus the thruster/
    # invisible keywords (modelread.cc:270, case-sensitive as retail's strstr),
    # folded and de-duplicated -- exactly the set pof2glb transcoded.
    names=$("$pof2glb" --summary "$pof" | grep '^textures' \
            | grep -o '"[^"]*"' | tr -d '"' \
            | grep -v thruster | grep -v invisible \
            | tr 'A-Z' 'a-z' | sort -u)
    if [ -z "$names" ]; then
        echo "$m: no non-keyword textures"
        continue
    fi

    # shellcheck disable=SC2086
    "$pcxdump" "$root" "$tmp/$m/idx" $names
    echo "$m:"
    # shellcheck disable=SC2086
    if python3 "$checker" "$tmp/$m/textures" "$tmp/$m/idx" $names; then
        :
    else
        rc=1
    fi
done

exit $rc
