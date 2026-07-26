#!/usr/bin/env python3
# Cross-check mission2tres's output against an INDEPENDENT read of the .fs2
# text: the tool went through retail's parse_main under Fred_running, this
# reads the #Objects section with python regexes -- two parsers over the
# same bytes. The comparison is a bijection: every .tres ship must be an
# object entry with matching class, position, orientation and player-start
# flag, and every #Objects entry must land in the .tres.
#
# Values compare within TIGHT tolerances rather than exactly: the .tres
# carries the ENGINE's placement, and retail applies geometric post-passes
# after parsing -- initially-docked ships realign to dock-point geometry,
# matrices re-orthogonalize -- that drift a handful of float32 ulps from
# the text (measured <= 3e-5 position, <= 4e-6 orientation across the
# campaign; 11 of 41 missions have at least one such ship). The tolerances
# sit ~30x above the measured drift and orders of magnitude below any real
# transform error. Pure stdlib.
#
#   check_mission.py <mission.tres> <mission.fs2>

import re
import struct
import sys

TOL_POS = 1e-3     # world units; engine dock realignment drifts ~1e-5
TOL_ORIENT = 1e-4  # basis component; re-orthogonalization drifts ~1e-6


def f32(x):
    return struct.unpack('<f', struct.pack('<f', float(x)))[0]


def fs2_objects(path):
    text = open(path, encoding='latin-1').read().replace('\r\n', '\n')
    # retail cuts every line at ';' before parsing (read_file_text) -- the
    # install's copies carry ";! Object #N" tags after ship names
    text = '\n'.join(l.split(';', 1)[0] for l in text.split('\n'))
    text = text[text.index('#Objects'):text.index('#Wings')]
    num = r'-?[\d.]+(?:[eE]-?\d+)?'
    out = {}
    for block in re.split(r'\n\$Name:', text)[1:]:
        name = block[:block.index('\n')].strip()
        cls = re.search(r'\$Class:\s*([^\n]+)', block).group(1).strip()
        # the 1.2-added missions (G-*, M-*, MD*, MT*) wrap $Orientation:
        # across lines -- scan fields to the next '$', then pull the floats
        loc = [f32(v) for v in re.findall(
            num, re.search(r'\$Location:\s*([^$]+)', block).group(1))[:3]]
        ori = [f32(v) for v in re.findall(
            num, re.search(r'\$Orientation:\s*([^$]+)', block).group(1))[:9]]
        flags = re.search(r'\+Flags:\s*\(([^)]*)\)', block)
        player = flags is not None and 'player-start' in flags.group(1)
        invuln = flags is not None and 'invulnerable' in flags.group(1)
        out[name] = {
            'class': cls, 'pos': loc,
            'rvec': ori[0:3], 'uvec': ori[3:6], 'fvec': ori[6:9],
            'player': player, 'invulnerable': invuln,
        }
    return out


def tres_ships(path):
    text = open(path).read()
    # only the ships array -- the .tres carries wings/events/goals/messages/
    # waypoints arrays after it (this gate checks placement; check_events.py
    # owns the rest). Each ship entry ends at its ai_goals bracket.
    text = text[text.index('ships = ['):text.index('\nwings = [')]
    out = {}
    for block in re.split(r'[{,] \{|^ships = \[\{', text, flags=re.M)[1:]:
        entry = {}
        for m in re.finditer(
                r'"(\w+)": (?:Vector3\(([^)]*)\)|"([^"]*)"|(\w+))', block):
            key, vec, s, word = m.groups()
            if vec is not None:
                entry[key] = [f32(v) for v in vec.split(',')]
            elif s is not None:
                entry[key] = s
            else:
                entry[key] = word
        if 'name' in entry:
            out[entry['name']] = entry
    return out


def main():
    tres, fs2 = tres_ships(sys.argv[1]), fs2_objects(sys.argv[2])
    ok = True

    for name, e in fs2.items():
        t = tres.get(name)
        if t is None:
            print(f'  FAIL {name}: in #Objects, not in .tres')
            ok = False
            continue
        if t['ship_class'] != e['class']:
            print(f'  FAIL {name}: class {t["ship_class"]!r} vs {e["class"]!r}')
            ok = False
        for key in ('pos', 'rvec', 'uvec', 'fvec'):
            tol = TOL_POS if key == 'pos' else TOL_ORIENT
            if any(abs(a - b) > tol for a, b in zip(t[key], e[key])):
                print(f'  FAIL {name}.{key}: tres {t[key]} vs fs2 {e[key]}')
                ok = False
        if (t['player_start'] == 'true') != e['player']:
            print(f'  FAIL {name}: player_start {t["player_start"]} '
                  f'vs flags {e["player"]}')
            ok = False
        if (t['invulnerable'] == 'true') != e['invulnerable']:
            print(f'  FAIL {name}: invulnerable {t["invulnerable"]} '
                  f'vs flags {e["invulnerable"]}')
            ok = False

    for name in tres:
        if name not in fs2:
            print(f'  FAIL {name}: in .tres, not in #Objects')
            ok = False

    if ok:
        nplayers = sum(1 for e in fs2.values() if e['player'])
        print(f'  OK   {len(fs2)} objects match, {nplayers} player start(s)')
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
