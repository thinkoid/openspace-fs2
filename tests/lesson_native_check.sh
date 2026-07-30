#!/bin/sh
#
# The lesson gate, two halves through hud_state:
#
# Training-1: by 12 sim-seconds the Welcome Message -- fired by the
# native SEXP (after retail's own ~5 s goal-eval grace), promoted by the
# training queue, token-translated, scrollback recorded -- is on display
# with its voice wave named. (Directives stay legitimately empty: the
# lesson's directive events are chained behind player progress.)
#
# The synthetic range: its directives born at start, so by 10 sim-seconds
# the gauge carries lines -- a CURRENT objective, a key line, and
# retail's [count] suffix on the multi-drone directive.
#
#   lesson_native_check.sh <sim_dump> <repo-root>
#
# Needs the unpacked install; skips (77) otherwise.

set -eu

sim=$1
repo=$2

root=${FS2_GAME_ROOT:-}
if [ -z "$root" ] && [ -d "$repo/../rundir" ]; then
    root=$repo/../rundir
fi

if [ -z "$root" ] || \
   [ -z "$(find "$root/data/missions" -maxdepth 1 -iname 'training-1.fs2' \
           2> /dev/null)" ]; then
    echo "no unpacked install with training-1.fs2 -- set FS2_GAME_ROOT; skipping"
    exit 77
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

"$sim" "$root" training-1.fs2 run 720 720 > "$tmp/t1.txt" 2> /dev/null
"$sim" "$root" "$repo/tests/weapons-range.fs2" run 600 600 \
    > "$tmp/wr.txt" 2> /dev/null

rc=0

if grep -q "^hud 720 msg '.\+' voice '.\+\.wav'" "$tmp/t1.txt"; then
    echo "OK: $(grep -m1 '^hud 720 msg' "$tmp/t1.txt" | cut -c1-72)..."
else
    echo "FAIL: Training-1 exposed no voiced training message by 12 sim-seconds"
    rc=1
fi

if grep -q "^hud 600 directive 1 key 0" "$tmp/wr.txt" \
   && grep -q "^hud 600 directive . key 1" "$tmp/wr.txt" \
   && grep -q "\[2\]" "$tmp/wr.txt"; then
    echo "OK: range directives -- current, key line, [count] all exposed"
else
    echo "FAIL: range directives incomplete:"
    grep '^hud' "$tmp/wr.txt" || echo "  (none)"
    rc=1
fi

exit $rc
