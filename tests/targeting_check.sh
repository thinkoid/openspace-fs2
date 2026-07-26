#!/bin/sh
#
# The targeting gate: check_targeting.gd pins inspect/targeting.gd's
# cycle order, hostile filter and `targeted` held-for semantics against
# a synthetic ship list. Skips without godot.
#
#   targeting_check.sh <check_targeting.gd> <repo-root>

set -eu

checker=$1
repo=$2

command -v godot > /dev/null 2>&1 || {
    echo "godot not installed; skipping"
    exit 77
}

godot --headless --path "$repo/inspect" --script "$checker"
