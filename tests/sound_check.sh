#!/bin/sh
#
# The sound gate: check_sound.gd pins inspect/sound.gd's cfile-style
# case-insensitive wav resolution and real stream loads against the
# unpacked install. Skips without godot or the install.
#
#   sound_check.sh <check_sound.gd> <repo-root>

set -eu

checker=$1
repo=$2

command -v godot > /dev/null 2>&1 || {
    echo "godot not installed; skipping"
    exit 77
}

root=${FS2_GAME_ROOT:-}
if [ -z "$root" ] && [ -d "$repo/../rundir/data/sounds" ]; then
    root=$repo/../rundir
fi
if [ -z "$root" ] || [ ! -d "$root/data/sounds" ]; then
    echo "no unpacked install found -- set FS2_GAME_ROOT; skipping"
    exit 77
fi

FS2_GAME_ROOT=$root godot --headless --path "$repo/inspect" \
    --script "$checker"
