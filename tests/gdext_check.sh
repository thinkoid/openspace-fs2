#!/bin/sh
#
# The GDExtension gate: a real headless godot boot loads libfs2.so through a
# throwaway project (project.godot + fs2.gdextension pointing at the built
# library) and check_gdext.gd asserts FS2.version() returns the exact string
# vcs_tag stamped into the build -- the full round-trip GDScript -> shim ->
# fs2_t and back. The tmp project keeps inspect/ untouched until a play
# slice actually needs the extension there.
#
#   gdext_check.sh <libfs2-so> <check_gdext.gd> <repo-root>
#
# Gates on godot being installed, else skip (77). No game data needed.

set -eu

so=$1
checker=$2
repo=$3

command -v godot > /dev/null 2>&1 || {
    echo "godot not installed; skipping"
    exit 77
}

# the same describe vcs_tag runs at build time; the .so was rebuilt by this
# test's dependency, so the two must agree
expected=$(git -C "$repo" describe --always --dirty=+)

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/project.godot" << EOF
config_version=5

[application]
config/name="libfs2 gate"
EOF

cat > "$tmp/fs2.gdextension" << EOF
[configuration]
entry_symbol = "fs2_library_init"
compatibility_minimum = "4.7"

[libraries]
linux.debug.x86_64 = "$so"
linux.release.x86_64 = "$so"
EOF

# godot discovers extensions through .godot/extension_list.cfg, which only
# the editor's import scan writes -- a virgin --script project never loads
# the .gdextension without it (measured: silent, no error). The file is a
# plain list of res:// paths; write it ourselves rather than paying for an
# editor --import pass.
mkdir "$tmp/.godot"
echo 'res://fs2.gdextension' > "$tmp/.godot/extension_list.cfg"

godot --headless --path "$tmp" --script "$checker" -- "$expected"
