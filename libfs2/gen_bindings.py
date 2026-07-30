#!/usr/bin/env python3
# Generate the godot-cpp bindings for libfs2, out of tree.
#
#   gen_bindings.py <godot-cpp-dir> <godot-exe> <build-profile> <out-dir>
#
# Dumps extension_api.json from the INSTALLED godot (so the bindings match
# the binary that will load the extension, not whatever the submodule pins),
# trims it through the build profile (a small class closure keeps the
# generated corpus tiny), generates into <out-dir>/gen, and prints the
# generated source list one path per line -- meson's run_command captures it
# at configure time. Everything lands in the build tree; the source tree is
# never written.

import contextlib
import subprocess
import sys

godot_cpp, godot_exe, profile, out_dir = sys.argv[1:5]

sys.path.insert(0, godot_cpp)
import binding_generator  # noqa: E402
from build_profile import generate_trimmed_api  # noqa: E402

subprocess.run(
    [godot_exe, "--headless", "--dump-extension-api"],
    cwd=out_dir, check=True, capture_output=True)

api_path = out_dir + "/extension_api.json"
api = generate_trimmed_api(api_path, profile)

# The 4.7 api exports global constants named after <cstdint> MACROS
# (UINT8_MAX, INT64_MIN, ...); the generator emits them as C++ constants and
# the preprocessor mangles the declarations. The pinned godot-cpp predates
# them, so it carries no skip-list -- filter them here. Nothing is lost:
# those names are macro-shadowed and unusable from C++ anyway.
api["global_constants"] = [
    c for c in api.get("global_constants", [])
    if not (c["name"].endswith(("_MAX", "_MIN"))
            and c["name"].lstrip("U").startswith("INT"))]

# the generator narrates on stdout; keep stdout pure for the file list. It
# appends gen/ to output_dir itself, so the tree lands at <out-dir>/gen.
with contextlib.redirect_stdout(sys.stderr):
    binding_generator._generate_bindings(
        api, api_path,
        use_template_get_node=True, bits="64", precision="single",
        output_dir=out_dir)

# the source list carries .inc entries too; only the .cpp compile
for path in binding_generator._get_file_list(api, out_dir, sources=True):
    if path.endswith(".cpp"):
        print(path)
