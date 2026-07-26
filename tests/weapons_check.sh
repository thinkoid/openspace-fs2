#!/bin/sh
#
# The weapons gate: check_weapons.gd pins inspect/weapons.gd's fire
# cadence, swept segment-sphere collision, hull ledger and the
# is-destroyed-delay/hits-left log queries against retail's contract.
# Skips without godot.
#
#   weapons_check.sh <check_weapons.gd> <repo-root>

set -eu

checker=$1
repo=$2

command -v godot > /dev/null 2>&1 || {
    echo "godot not installed; skipping"
    exit 77
}

godot --headless --path "$repo/inspect" --script "$checker"
