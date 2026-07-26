#!/usr/bin/env python3
# Cross-check shiptbl2tres's output against an INDEPENDENT read of the
# table text: the tool went through retail's parse_shiptbl, this goes
# through a python regex per field -- two parsers over the same bytes, so a
# stuffing bug in either is a disagreement here. The one derived value,
# max_rotvel = (2*PI)/rotation_time, is replicated in retail's own float32
# arithmetic (PI = 3.141592654f, pstypes.hh:303), every comparison done on
# float32-snapped values -- exact, no tolerances. Pure stdlib.
#
#   check_shiptbl.py <ship_params.tres> <ships.tbl> <pof-stem> [stem ...]

import re
import struct
import sys


def f32(x):
    return struct.unpack('<f', struct.pack('<f', float(x)))[0]


PI = f32(3.141592654)

# ships.tbl field -> (tres key, arity); rotation time is handled apart
FIELDS = {
    'Density': ('density', 1),
    'Damp': ('damp', 1),
    'Rotdamp': ('rotdamp', 1),
    'Max Velocity': ('max_vel', 3),
    'Rear Velocity': ('max_rear_vel', 1),
    'Forward accel': ('forward_accel', 1),
    'Forward decel': ('forward_decel', 1),
    'Slide accel': ('slide_accel', 1),
    'Slide decel': ('slide_decel', 1),
    'Aburn Max Vel': ('afterburner_max_vel', 3),
    'Aburn For accel': ('afterburner_forward_accel', 1),
    'Hitpoints': ('hull', 1),
}

# weapons.tbl field -> tres key (the ballistics subset the weapons slice
# consumes; entries missing a field, e.g. beams, compare what they have)
WEAPON_FIELDS = {
    'Velocity': 'velocity',
    'Damage': 'damage',
    'Lifetime': 'lifetime',
    'Fire Wait': 'fire_wait',
}


def snd_names(path):
    # sounds.tbl game-sounds section only: index -> wav name (the
    # interface section reuses the same indices)
    text = open(path, encoding='latin-1').read()
    text = text[:text.index('#Game Sounds End')]
    out = {}
    for m in re.finditer(r'\$Name:\s*(\d+)\s+([^\s,]+)\s*,', text):
        out[int(m.group(1))] = m.group(2)
    return out


def tbl_weapons(path, snds):
    text = open(path, encoding='latin-1').read()
    out = {}
    for block in re.split(r'\n\$Name:', text)[1:]:
        name = block[:block.index('\n')].split(';')[0].strip()
        entry = {}
        for field, key in WEAPON_FIELDS.items():
            fm = re.search(r'\$' + re.escape(field) + r':\s*([^;\n]+)', block)
            if fm:
                entry[key] = f32(fm.group(1).split()[0])
        # sound indices resolve through sounds.tbl; -1 and the
        # "none.wav" placeholder both mean silence
        for field, key in (('LaunchSnd', 'launch_snd'),
                           ('ImpactSnd', 'impact_snd')):
            fm = re.search(r'\$' + field + r':\s*(-?\d+)', block)
            if fm:
                wav = snds.get(int(fm.group(1)), '')
                entry[key] = '' if wav.lower() == 'none.wav' else wav
        out[name.lstrip('@')] = entry
    return out


def tres_weapons(path):
    text = open(path).read()
    text = text[text.index('weapons = {'):text.index('\nsounds = {')]
    out = {}
    for m in re.finditer(r'"([^"]+)": \{([^}]*)\}', text):
        entry = {}
        for fm in re.finditer(r'"(\w+)": ([-\d.e+]+)', m.group(2)):
            entry[fm.group(1)] = f32(fm.group(2))
        for fm in re.finditer(r'"(\w+)": "([^"]*)"', m.group(2)):
            entry[fm.group(1)] = fm.group(2)
        out[m.group(1)] = entry
    return out


def tres_sounds(path):
    text = open(path).read()
    text = text[text.index('\nsounds = {'):]
    return dict(re.findall(r'"(\w+)": "([^"]*)"', text))


def tbl_entries(path):
    # the ship classes only: the "#Default Player Ship" section up top also
    # carries a $Name and must not match
    text = open(path, encoding='latin-1').read()
    text = text[text.index('#Ship Classes'):]
    out = {}
    for block in re.split(r'\n\$Name:', text)[1:]:
        m = re.search(r'\$POF file:\s*([^\s;]+)', block)
        if not m:
            continue
        stem = m.group(1).rsplit('.', 1)[0].lower()
        entry = {}
        for field, (key, arity) in FIELDS.items():
            fm = re.search(r'[$+]' + re.escape(field) + r':\s*([^;\n]+)',
                           block)
            if not fm:
                continue
            vals = [f32(v) for v in fm.group(1).replace(',', ' ').split()]
            entry[key] = vals[0] if arity == 1 else vals
        rm = re.search(r'\$Rotation time:\s*([^;\n]+)', block)
        if rm:
            ts = [f32(v) for v in rm.group(1).replace(',', ' ').split()]
            entry['max_rotvel'] = [f32(f32(2 * PI) / t) for t in ts]
        out[stem] = entry
    return out


def tres_entries(path):
    text = open(path).read()
    out = {}
    for m in re.finditer(r'"([a-z0-9_-]+)": \{\n((?:[^}]|\n)*?)\n\}', text):
        stem, body = m.group(1), m.group(2)
        entry = {}
        for fm in re.finditer(r'"(\w+)": (?:Vector3\(([^)]*)\)|([-\d.e+]+))',
                              body):
            key, vec, scalar = fm.groups()
            if vec is not None:
                entry[key] = [f32(v) for v in vec.split(',')]
            else:
                entry[key] = f32(scalar)
        out[stem] = entry
    return out


def main():
    tres, tbl = tres_entries(sys.argv[1]), tbl_entries(sys.argv[2])
    ok = True
    for stem in sys.argv[3:]:
        if stem not in tres:
            print(f'  FAIL {stem}: not in the .tres')
            ok = False
            continue
        if stem not in tbl:
            print(f'  FAIL {stem}: not found in ships.tbl')
            ok = False
            continue
        bad = 0
        for key, want in tbl[stem].items():
            got = tres[stem].get(key)
            if got != want:
                print(f'  FAIL {stem}.{key}: tres {got} vs tbl {want}')
                bad += 1
        if bad:
            ok = False
        else:
            print(f'  OK   {stem}: {len(tbl[stem])} fields agree')

    # the weapons dict against weapons.tbl, sitting beside ships.tbl;
    # sound indices resolve through sounds.tbl beside them both
    import os
    tables = os.path.dirname(sys.argv[2])
    snds = snd_names(os.path.join(tables, 'sounds.tbl'))
    wtbl = tbl_weapons(os.path.join(tables, 'weapons.tbl'), snds)
    wtres = tres_weapons(sys.argv[1])
    checked = 0
    for name, got in wtres.items():
        want = wtbl.get(name.lstrip('@'))
        if want is None:
            print(f'  FAIL weapon {name}: not found in weapons.tbl')
            ok = False
            continue
        for key, w in want.items():
            if got.get(key) != w:
                print(f'  FAIL weapon {name}.{key}: '
                      f'tres {got.get(key)} vs tbl {w}')
                ok = False
        checked += 1
    print(f'  OK   {checked} weapons checked')

    # the effects dict: the fighter explosion pair by retail's indices
    want = {'ship_explode_1': snds.get(7, ''), 'ship_explode_2':
            snds.get(49, '')}
    got = tres_sounds(sys.argv[1])
    if got != want:
        print(f'  FAIL sounds: tres {got} vs tbl {want}')
        ok = False
    else:
        print(f'  OK   effects dict agrees')
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
