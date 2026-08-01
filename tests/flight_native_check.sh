#!/bin/sh
#
# The inverted flight gate's driver: run physics_dump (retail's integrator,
# the oracle) for the trace, then replay it through the native boundary via
# check_flight_native.gd and demand bit-exact agreement -- same machine
# code on both sides, so the only thing that can differ is the boundary.
#
#   flight_native_check.sh <physics_dump> <fs2.gdextension> <checker> <repo-root>
#
# Gates on godot being installed, else skip (77). No game data needed.

set -eu

dump=$1
gdext=$2
checker=$3
repo=$4

command -v godot > /dev/null 2>&1 || {
    echo "godot not installed; skipping"
    exit 77
}

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# hermetic pilot: libfs2 boots against the XDG homes, so point both at
# scratch -- the gate must neither read nor write the real Commander
export XDG_DATA_HOME="$tmp/xdg-data" XDG_CONFIG_HOME="$tmp/xdg-config"

"$dump" > "$tmp/trace.txt"

godot --headless --path "$repo/inspect" --script "$checker" \
    -- "$gdext" "$tmp/trace.txt"
