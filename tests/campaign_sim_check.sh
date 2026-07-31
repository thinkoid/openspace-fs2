#!/bin/sh
#
# The campaign gate: EVERY install mission simulates 10 hands-off
# sim-seconds in the native world without crashing -- arrivals, dogfight
# AI, turret fire, missiles, engine wash, debris, damage all exercising
# retail's own code through the boundary. Aggregate assertions prove the
# world is ALIVE, not merely quiet: somewhere in the corpus ships arrive
# mid-run, and somewhere the AI scores a kill on its own. One combat
# mission runs twice and must be byte-identical (combat joins the
# determinism contract).
#
#   campaign_sim_check.sh <sim_dump> [search-root]
#
# Needs the unpacked install; skips (77) otherwise.

set -eu

sim=$1
search=${2:-}

root=${FS2_GAME_ROOT:-}
if [ -z "$root" ] && [ -n "$search" ] && [ -d "$search/../rundir" ]; then
    root=$search/../rundir
fi

if [ -z "$root" ] || ! ls "$root"/data/missions/*.fs2 > /dev/null 2>&1; then
    echo "no unpacked install found -- set FS2_GAME_ROOT; skipping"
    exit 77
fi

echo "game root: $root"

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

total=0
crashed=0
arrivals=0
kills=0
chatter=0
for f in "$root"/data/missions/*.fs2; do
    m=$(basename "$f")
    total=$((total + 1))
    if ! "$sim" "$root" "$f" run 600 600 > "$tmp/$m.out" 2> "$tmp/$m.err"; then
        echo "CRASH $m: $(tail -1 "$tmp/$m.err" | cut -c1-90)"
        crashed=$((crashed + 1))
        continue
    fi
    # a ship (named object) entering after the first second = an arrival
    if awk '$1 == "event" && $2 > 60 && $3 == "created" && $4 != "sig"' \
            "$tmp/$m.out" | grep -q .; then
        arrivals=$((arrivals + 1))
    fi
    # LOG_SHIP_DESTROYED entries, all AI's own work (the stick is dead)
    if grep -q "^event [0-9]* log 1 " "$tmp/$m.out"; then
        kills=$((kills + 1))
    fi
    # radio chatter through the Msg_capture seam: sender + translated
    # text + resolved voice wave
    if grep -q "^event [0-9]* message " "$tmp/$m.out"; then
        chatter=$((chatter + 1))
    fi
done

echo "missions: $((total - crashed))/$total clean, $arrivals with arrivals, $kills with AI kills, $chatter with chatter"

rc=0
[ $crashed = 0 ] || rc=1

if [ $arrivals = 0 ]; then
    echo "FAIL: no mid-run arrivals anywhere in the corpus"
    rc=1
fi
if [ $kills = 0 ]; then
    echo "FAIL: no AI kills anywhere in the corpus"
    rc=1
fi
if [ $chatter = 0 ]; then
    echo "FAIL: no radio chatter anywhere in the corpus"
    rc=1
fi
# somewhere a message must carry its voice wave -- the capture happens
# before the deviceless snd_load scrubs wave_info.index; if that ordering
# regresses, text still flows but every wave goes empty
if ! grep -qE "^event [0-9]+ message .* wave [^-]" "$tmp"/*.out; then
    echo "FAIL: no message anywhere carries a voice wave"
    rc=1
fi

# combat determinism, spot-checked on a mission that fights hands-off
combat=$(grep -l "^event [0-9]* log 1 " "$tmp"/*.out 2> /dev/null | head -1)
if [ -n "$combat" ]; then
    m=$(basename "$combat" .out)
    "$sim" "$root" "$root/data/missions/$m" run 600 600 \
        > "$tmp/again.out" 2> /dev/null || true
    if ! cmp -s "$combat" "$tmp/again.out"; then
        echo "FAIL: $m differs across two runs -- combat determinism broken"
        rc=1
    else
        echo "OK: $m combat byte-identical across runs"
    fi
fi

exit $rc
