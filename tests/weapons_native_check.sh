#!/bin/sh
#
# The native weapons gate: sim_dump's fire mode on the synthetic range --
# aim assist through the boundary, trigger held -- must produce a KILL
# recorded by retail's own mission log (LOG_SHIP_DESTROYED, type 1, a
# drone by Alpha 1): retail firing cadence, bolt flight, BSP
# model_collide, damage and destruction, end to end natively. Two runs
# must be byte-identical (the fire path joins the determinism contract).
#
#   weapons_native_check.sh <sim_dump> <repo-root>
#
# Needs the unpacked install (tables + models); skips (77) otherwise.

set -eu

sim=$1
repo=$2

root=${FS2_GAME_ROOT:-}
if [ -z "$root" ] && [ -d "$repo/../rundir" ]; then
    root=$repo/../rundir
fi

if [ -z "$root" ] || [ ! -d "$root/data/tables" ]; then
    echo "no unpacked install found -- set FS2_GAME_ROOT; skipping"
    exit 77
fi

mission=$repo/tests/weapons-range.fs2

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

"$sim" "$root" "$mission" fire 3600 3600 > "$tmp/fire.txt" 2> /dev/null
"$sim" "$root" "$mission" fire 3600 3600 > "$tmp/fire2.txt" 2> /dev/null

rc=0

if ! cmp -s "$tmp/fire.txt" "$tmp/fire2.txt"; then
    echo "FAIL: two firing runs differ -- determinism broken"
    rc=1
fi

if grep -q "^event [0-9]* log 1 'Drone [0-9]*' 'Alpha 1'" "$tmp/fire.txt"; then
    echo "OK: $(grep -m1 "log 1 'Drone" "$tmp/fire.txt")"
else
    echo "FAIL: no drone kill by Alpha 1 in 60 sim-seconds"
    rc=1
fi

# the whole range clears and retail's goal evaluation notices: all three
# drones die and the mission goal satisfies, end to end natively. This
# pins the gunner's convergence sweep -- a boresight locked dead on
# center straddles a small target with the parallel gun streams forever
if grep -q "^event [0-9]* log 14 'Clear the range'" "$tmp/fire.txt"; then
    echo "OK: $(grep -m1 "log 14" "$tmp/fire.txt")"
else
    echo "FAIL: mission goal 'Clear the range' never satisfied"
    rc=1
fi

# the targeting chain: fire mode pulses target_next once at frame 60;
# the signature must cross back through hud_state (a broken chain reads
# target -1 forever)
if grep -q "^hud [0-9]* target [0-9]" "$tmp/fire.txt"; then
    echo "OK: $(grep -m1 "^hud [0-9]* target [0-9]" "$tmp/fire.txt")"
else
    echo "FAIL: no target signature ever crossed hud_state"
    rc=1
fi

# the sounds-as-events seam: every shot requests its launch wav, every
# hit its positioned impact wav
if grep -q "^event [0-9]* sound L_Sidearm" "$tmp/fire.txt" \
   && grep -q "^event [0-9]* sound hit_1\.wav at " "$tmp/fire.txt"; then
    echo "OK: $(grep -c '^event [0-9]* sound' "$tmp/fire.txt") sound events (launch + positioned impacts)"
else
    echo "FAIL: firing produced no sound events"
    rc=1
fi

# the art freight, each record kind's first crossing pinned exactly:
# the laser's tbl size + cycle color (through the deviceless
# gr_init_color stub -- a zeroed rgb means the stub went lossy again),
# the player's shield quadrants (390 total, 97.5 each: ships.tbl's
# Myrmidon), the missile's POF, and a live expanding shockwave record
# (the Piranha detonates even on a miss)
if grep -q "^art bolt Subach HL-7 len 10 r 0\.899999976 rgb 245 0 4 bitmap newglo9\.pcx glow 2_laserglow03\.pcx$" \
        "$tmp/fire.txt"; then
    echo "OK: $(grep -m1 '^art bolt' "$tmp/fire.txt")"
else
    echo "FAIL: laser bolt art wrong or missing: $(grep -m1 '^art bolt' "$tmp/fire.txt" || echo none)"
    rc=1
fi

if grep -q "^art shield 'Alpha 1' 97\.5 97\.5 97\.5 97\.5 max 390$" \
        "$tmp/fire.txt"; then
    echo "OK: $(grep -m1 '^art shield' "$tmp/fire.txt")"
else
    echo "FAIL: player shield freight wrong or missing: $(grep -m1 '^art shield' "$tmp/fire.txt" || echo none)"
    rc=1
fi

if grep -q "^art player energy 150/150 burner 320/320 gun_speed 450$" \
        "$tmp/fire.txt"; then
    echo "OK: $(grep -m1 '^art player' "$tmp/fire.txt")"
else
    echo "FAIL: player HUD freight wrong or missing: $(grep -m1 '^art player' "$tmp/fire.txt" || echo none)"
    rc=1
fi

if grep -q "^art missile Piranha pof piranha\.pof$" "$tmp/fire.txt"; then
    echo "OK: $(grep -m1 '^art missile' "$tmp/fire.txt")"
else
    echo "FAIL: missile POF never crossed: $(grep -m1 '^art missile' "$tmp/fire.txt" || echo none)"
    rc=1
fi

if grep -q "^art shockwave frame [0-9]* r [0-9]" "$tmp/fire.txt"; then
    echo "OK: $(grep -m1 '^art shockwave' "$tmp/fire.txt")"
else
    echo "FAIL: no shockwave record ever crossed"
    rc=1
fi

# the fireball names its flipbook (fireball_art_name -> the pof slot):
# the first blast on the range is retail's exp04
if grep -q "^art fireball explosion ani exp04$" "$tmp/fire.txt"; then
    echo "OK: $(grep -m1 '^art fireball' "$tmp/fire.txt")"
else
    echo "FAIL: fireball ani name wrong or missing: $(grep -m1 '^art fireball' "$tmp/fire.txt" || echo none)"
    rc=1
fi

# debris names its source model and submodel piece -- the first chunk
# off a dying drone is the drone's own
if grep -q "^art debris pof Drone01\.pof piece debris01$" "$tmp/fire.txt"; then
    echo "OK: $(grep -m1 '^art debris' "$tmp/fire.txt")"
else
    echo "FAIL: debris identity wrong or missing: $(grep -m1 '^art debris' "$tmp/fire.txt" || echo none)"
    rc=1
fi

exit $rc
