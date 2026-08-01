#!/bin/sh
#
# The packed-boundary gate: frame()'s parallel rows must equal
# snapshot()'s records field for field, same frame, exact -- ten probes
# across a live mission. snapshot() is the oracle (every native gate
# pins it); this keeps the hot packed crossing honest against it.
#
#   frame_eq_check.sh <gdext> <checker.gd> <repo-root>
#
# Needs godot + the unpacked install; skips (77) otherwise.

set -eu

gdext=$1
checker=$2
repo=$3

command -v godot > /dev/null 2>&1 || {
    echo "godot not installed; skipping"
    exit 77
}
[ -f "$gdext" ] || {
    echo "no fs2.gdextension ($gdext); skipping"
    exit 77
}

root=${FS2_GAME_ROOT:-}
if [ -z "$root" ] && [ -d "$repo/../rundir" ]; then
    root=$repo/../rundir
fi
if [ -z "$root" ] || [ ! -d "$root/data/tables" ]; then
    echo "no unpacked install found -- set FS2_GAME_ROOT; skipping"
    exit 77
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# hermetic pilot: libfs2 boots against the XDG homes, so point both at
# scratch -- the gate must neither read nor write the real Commander
export XDG_DATA_HOME="$tmp/xdg-data" XDG_CONFIG_HOME="$tmp/xdg-config"

FS2_GDEXT=$gdext FS2_GAME_ROOT=$root \
    godot --headless --script "$checker" > "$tmp/out.txt" 2>&1 || {
    cat "$tmp/out.txt"
    exit 1
}
grep -q "^frame-eq: OK$" "$tmp/out.txt" || {
    cat "$tmp/out.txt"
    exit 1
}
echo "OK: frame() rows == snapshot() records, 10 probes"
