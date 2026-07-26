#!/bin/sh
#
# The radar gate: check_radar.gd pins inspect/radar.gd's blip
# projection (retail radar.cc math, quirks included) and the lead
# indicator's aim point. Skips without godot.
#
#   radar_check.sh <check_radar.gd> <repo-root>

set -eu

checker=$1
repo=$2

command -v godot > /dev/null 2>&1 || {
    echo "godot not installed; skipping"
    exit 77
}

godot --headless --path "$repo/inspect" --script "$checker"
