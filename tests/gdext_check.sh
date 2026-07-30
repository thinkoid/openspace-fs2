#!/bin/sh
#
# The GDExtension gate: a real headless godot boot loads libfs2 through the
# build tree's generated fs2.gdextension (GDExtensionManager.load_extension
# -- the same runtime-loading contract fly.gd uses; nothing is discovered,
# nothing is written into a project) and check_gdext.gd asserts
# FS2.version() returns the exact string vcs_tag stamped into the build --
# the full round trip GDScript -> shim -> fs2_t and back.
#
#   gdext_check.sh <fs2.gdextension> <check_gdext.gd> <repo-root>
#
# Gates on godot being installed, else skip (77). No game data needed.

set -eu

gdext=$1
checker=$2
repo=$3

command -v godot > /dev/null 2>&1 || {
    echo "godot not installed; skipping"
    exit 77
}

# the same describe vcs_tag runs at build time; the .so was rebuilt by this
# test's dependency, so the two must agree
expected=$(git -C "$repo" describe --always --dirty=+)

godot --headless --path "$repo/inspect" --script "$checker" \
    -- "$gdext" "$expected"
