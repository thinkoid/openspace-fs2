#!/bin/sh
#
# The flight-model gate: physics_dump (retail's physics_sim over a scripted
# control sequence) writes the reference trajectory; check_flight.gd replays
# the same inputs through inspect/flight_model.gd and the trajectories must
# agree within float-width tolerances. Skips without godot.
#
#   flight_check.sh <physics_dump> <check_flight.gd> <repo-root>

set -eu

dump=$1
checker=$2
repo=$3

command -v godot > /dev/null 2>&1 || {
    echo "godot not installed; skipping"
    exit 77
}

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

"$dump" > "$tmp/trace.txt"

godot --headless --path "$repo/inspect" --script "$checker" -- "$tmp/trace.txt"
