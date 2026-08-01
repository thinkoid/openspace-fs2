#!/bin/sh
#
# The world gate: every install mission (plus the synthetic range) loaded
# by the NATIVE boundary (sim_dump layout -- retail's game-path parse,
# arrival cues live) and cross-checked against mission2tres's FRED view by
# check_world.py: native subset of FRED, identity exact, placement within
# tolerance, player start present. Each mission also loads TWICE and the
# layouts must be byte-identical -- the determinism the boundary promises
# (fixed seed, virtual clock) as a failing test.
#
#   world_check.sh <sim_dump> <mission2tres> <check_world.py> [search-root]
#
# Needs the unpacked install; skips (77) otherwise.

set -eu

sim=$1
m2t=$2
checker=$3
search=${4:-}

root=${FS2_GAME_ROOT:-}
if [ -z "$root" ] && [ -n "$search" ] && [ -d "$search/../rundir" ]; then
    root=$search/../rundir
fi

if [ -z "$root" ] || [ ! -d "$root/data/tables" ]; then
    echo "no unpacked install found -- set FS2_GAME_ROOT; skipping"
    exit 77
fi
if ! ls "$root"/data/missions/*.fs2 > /dev/null 2>&1; then
    echo "no missions under $root/data/missions; skipping"
    exit 77
fi

echo "game root: $root"

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# hermetic pilot: libfs2 boots against the XDG homes, so point both at
# scratch -- the gate must neither read nor write the real Commander
export XDG_DATA_HOME="$tmp/xdg-data" XDG_CONFIG_HOME="$tmp/xdg-config"

total=0
failed=0
for f in "$root"/data/missions/*.fs2 "$(dirname "$checker")"/*.fs2; do
    [ -f "$f" ] || continue
    m=$(basename "$f")
    total=$((total + 1))

    if ! "$m2t" "$root" "$f" "$tmp/$m.tres" > "$tmp/$m.m2t.log" 2>&1; then
        echo "FAIL $m: mission2tres ($(tail -1 "$tmp/$m.m2t.log"))"
        failed=$((failed + 1))
        continue
    fi
    if ! "$sim" "$root" "$f" layout > "$tmp/$m.native" 2> "$tmp/$m.log"; then
        echo "FAIL $m: sim_dump ($(tail -1 "$tmp/$m.log"))"
        failed=$((failed + 1))
        continue
    fi
    "$sim" "$root" "$f" layout > "$tmp/$m.native2" 2> /dev/null || true
    if ! cmp -s "$tmp/$m.native" "$tmp/$m.native2"; then
        echo "FAIL $m: two loads differ -- determinism broken"
        failed=$((failed + 1))
        continue
    fi
    if ! python3 "$checker" "$tmp/$m.native" "$tmp/$m.tres" \
            > "$tmp/$m.out" 2>&1; then
        echo "FAIL $m: $(grep -m1 FAIL "$tmp/$m.out")"
        failed=$((failed + 1))
    fi
done

echo "missions: $((total - failed))/$total clean"
[ $failed = 0 ] || exit 1

# the authored sky crosses: training-1's sun with its glow, the RGBI
# light stars.tbl gives it, and the mission's own star count -- pinned
# exactly (bites: dropped stars_init zeroes the RGBI; a dead parse
# loses the sun)
t1=$(find "$root/data/missions" -maxdepth 1 -iname 'training-1.fs2' | head -1)
if [ -n "$t1" ]; then
    "$sim" "$root" "$t1" layout > "$tmp/t1.backdrop" 2> /dev/null || true
    if grep -q "^backdrop sun SunGold glow SunglowGold uvec 0\.38340795 0\.898794293 -0\.212525621 scale 2 2 xparent 1 rgbi 0\.74000001 0\.75999999 0\.439999998 1$" \
            "$tmp/t1.backdrop" \
       && grep -q "^backdrop stars 880$" "$tmp/t1.backdrop"; then
        echo "OK: $(grep -c '^backdrop bitmap' "$tmp/t1.backdrop") backdrop patches + SunGold + 880 stars"
    else
        echo "FAIL: training-1 backdrop wrong or missing: $(grep -m1 '^backdrop' "$tmp/t1.backdrop" || echo none)"
        exit 1
    fi
fi
exit 0
