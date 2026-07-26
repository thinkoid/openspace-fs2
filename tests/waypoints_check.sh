#!/bin/sh
#
# The waypoint-AI gate: check_waypoints.gd pins inspect/waypoint_ai.gd's
# flying, retail arrival test, LOG_WAYPOINTS_DONE stamping and the
# are-waypoints-done-delay predicate. Skips without godot.
#
#   waypoints_check.sh <check_waypoints.gd> <repo-root>

set -eu

checker=$1
repo=$2

command -v godot > /dev/null 2>&1 || {
    echo "godot not installed; skipping"
    exit 77
}

godot --headless --path "$repo/inspect" --script "$checker"
