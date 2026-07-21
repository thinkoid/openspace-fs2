#!/bin/sh
#
# Check pof_dump's output against the pinned oracle in tests/oracle/.
#
#   pof_oracle.sh <pof_dump-binary> <oracle-dir> [search-root]
#
# The game data is not in the repo, so the install comes from $FS2_GAME_ROOT.
# Failing that we look for the workspace's own gog/ and rundir/ next to the
# search root; failing that the test skips (77) rather than failing, because a
# missing install is not a broken loader.
#
# Either install works: the VP archives keep the original mixed-case filenames
# and an unpacked tree has lowercased ones, but the dump folds the name for
# both display and sort order, so the two produce identical bytes.
#
# Three checks, cheapest first:
#   1. the summary dump of all 176 models, compared line by line
#   2. --full for eight models picked for feature coverage -- flat polygons
#      survive in only 6 of the 176 retail models, so two of those are in the
#      sample on purpose, and both POF versions (2116, 2117) are represented
#   3. sha256 of --full over the whole corpus: no diff to read when it breaks,
#      but the only check that covers every model's geometry

set -eu

dump=$1
oracle=$2
search=${3:-}

root=${FS2_GAME_ROOT:-}
if [ -z "$root" ] && [ -n "$search" ]; then
    for candidate in "$search/../gog" "$search/../rundir"; do
        if [ -d "$candidate" ]; then
            root=$candidate
            break
        fi
    done
fi

if [ -z "$root" ]; then
    echo "no game install found -- set FS2_GAME_ROOT; skipping"
    exit 77
fi
if [ ! -d "$root" ]; then
    echo "FS2_GAME_ROOT=$root is not a directory; skipping"
    exit 77
fi

echo "game root: $root"

# Must stay in step with tests/oracle/pof-full-sample.txt; regenerate that file
# whenever this list changes.
sample_models="t-laser.pof cmeasure01.pof spacehunk.pof subspacenode.pof \
cargo03.pof support02.pof fighter2s-03.pof fighter13.pof"

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

rc=0

"$dump" "$root" > "$tmp/summary.txt" 2>/dev/null
if diff -u "$oracle/pof-dump.txt" "$tmp/summary.txt" > "$tmp/summary.diff"; then
    echo "ok   summary      (176 models)"
else
    echo "FAIL summary      $(grep -c '^[-+][^-+]' "$tmp/summary.diff") changed lines"
    head -40 "$tmp/summary.diff"
    rc=1
fi

# word splitting on sample_models is intended
# shellcheck disable=SC2086
"$dump" --full "$root" $sample_models > "$tmp/sample.txt" 2>/dev/null
if diff -u "$oracle/pof-full-sample.txt" "$tmp/sample.txt" > "$tmp/sample.diff"; then
    echo "ok   full sample  (8 models)"
else
    echo "FAIL full sample  $(grep -c '^[-+][^-+]' "$tmp/sample.diff") changed lines"
    head -40 "$tmp/sample.diff"
    rc=1
fi

got=$("$dump" --full "$root" 2>/dev/null | sha256sum | cut -d' ' -f1)
want=$(cat "$oracle/pof-full.sha256")
if [ "$got" = "$want" ]; then
    echo "ok   full corpus  sha256 $got"
else
    echo "FAIL full corpus  sha256"
    echo "       want $want"
    echo "       got  $got"
    echo "     (run pof_dump --full yourself to see where; the sample above"
    echo "      only covers 8 of the 176 models)"
    rc=1
fi

exit $rc
