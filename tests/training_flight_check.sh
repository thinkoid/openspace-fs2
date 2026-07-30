#!/bin/sh
#
# The Instructor gate: Training-1 runs 60 sim-seconds in the native world
# with a hands-off stick, and retail's OWN AI must fly the Instructor's
# first waypoint path to completion -- proven by the mission log's
# LOG_WAYPOINTS_DONE (type 17) entry, retail's own record, and by the
# Instructor measurably leaving his spawn. The run also executes twice and
# must produce byte-identical output (step determinism; the layout gate
# only pins load determinism).
#
#   training_flight_check.sh <sim_dump> [search-root]
#
# Needs the unpacked install; skips (77) otherwise.

set -eu

sim=$1
search=${2:-}

root=${FS2_GAME_ROOT:-}
if [ -z "$root" ] && [ -n "$search" ] && [ -d "$search/../rundir" ]; then
    root=$search/../rundir
fi

# the on-disk name is mixed-case (Training-1.fs2); cfile resolves either,
# the shell test must not care
if [ -z "$root" ] || \
   [ -z "$(find "$root/data/missions" -maxdepth 1 -iname 'training-1.fs2' \
           2> /dev/null)" ]; then
    echo "no unpacked install with training-1.fs2 -- set FS2_GAME_ROOT; skipping"
    exit 77
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

"$sim" "$root" training-1.fs2 run 3600 3600 > "$tmp/run.txt" 2> /dev/null
"$sim" "$root" training-1.fs2 run 3600 3600 > "$tmp/run2.txt" 2> /dev/null

rc=0

if ! cmp -s "$tmp/run.txt" "$tmp/run2.txt"; then
    echo "FAIL: two runs differ -- step determinism broken"
    rc=1
fi

if grep -q "^event [0-9]* log 17 'Instructor'" "$tmp/run.txt"; then
    echo "OK: $(grep -m1 "log 17 'Instructor'" "$tmp/run.txt")"
else
    echo "FAIL: no LOG_WAYPOINTS_DONE for the Instructor in 60 sim-seconds"
    rc=1
fi

# the Instructor's t=3600 position vs his t=0 spawn (from the layout)
moved=$("$sim" "$root" training-1.fs2 layout 2> /dev/null \
    | grep '^state 0 Instructor' > "$tmp/spawn.txt"; \
    grep '^state 3600 Instructor' "$tmp/run.txt" > "$tmp/end.txt"; \
    paste "$tmp/spawn.txt" "$tmp/end.txt" | awk '{
        for (i = 1; i <= NF; i++) if ($i == "pos") { if (!p1) p1 = i; else p2 = i }
        dx = $(p1+1) - $(p2+1); dy = $(p1+2) - $(p2+2); dz = $(p1+3) - $(p2+3)
        print sqrt(dx*dx + dy*dy + dz*dz)
    }')

if [ -z "$moved" ]; then
    echo "FAIL: could not measure the Instructor's travel"
    rc=1
elif awk "BEGIN { exit !($moved > 50) }"; then
    echo "OK: Instructor flew $moved units under retail AI"
else
    echo "FAIL: Instructor only moved $moved units"
    rc=1
fi

exit $rc
