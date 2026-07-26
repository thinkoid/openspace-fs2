#!/usr/bin/env python3
# Cross-check mission2tres's event/goal/message/waypoint/ai-goal output
# against an INDEPENDENT read of the .fs2 text -- the tool went through
# retail's parse_main, this reads the #Events/#Goals/#Messages/#Waypoints/
# #Objects/#Wings sections with python. Formulas compare as canonical
# one-line sexp text (single-space separated, strings quoted) -- the same
# normal form mission2tres's own walker emits; the whole corpus keeps every
# $Formula on one line, so the tokenizer never spans lines.
#
# Ship AI goals compare against retail's DECODED form (ai_add_goal_sub_sexp,
# aigoals.cc): op -> AI_GOAL_* mode bit, ai-chase/-guard on a wing name
# become the _WING modes, priority >= 90 bashes to 89, dock/undock/disable/
# disarm carry fixed submodes. Wing $AI Goals apply to every member ship
# AFTER the ship's own (parse_wing_create_ships copies wing goals at
# creation, missionparse.cc:2169). Pure stdlib.
#
#   check_events.py <mission.tres> <mission.fs2>
#
# The .fs2 passed here must be THE FILE CFILE RESOLVED for the tool --
# the game root's data/missions copy, not the loose missions/ checkout:
# the two genuinely differ (the install carries 1.2-patched mission text).
# In the default language retail's lcl_ext_localize never opens the
# externalization table (localize.cc:484) -- an XSTR always resolves to
# its file literal, so the oracle just strips the wrapper.

import re
import struct
import sys


def f32(x):
    return struct.unpack('<f', struct.pack('<f', float(x)))[0]


def xstr(s):
    s = s.strip()
    m = re.fullmatch(r'XSTR\s*\(\s*"(.*)"\s*,\s*-?\d+\s*\)', s, re.S)
    return m.group(1) if m else s


# ---- sexp canonicalization (mirrors mission2tres's sexp_text walker) ----

def sexp_tokens(text):
    for m in re.finditer(r'"[^"]*"|[()]|[^\s()"]+', text):
        yield m.group(0)


def sexp_parse(tokens):
    out = []
    for t in tokens:
        if t == '(':
            out.append(sexp_parse(tokens))
        elif t == ')':
            return out
        else:
            out.append(t)
    return out


def sexp_canon(node):
    if isinstance(node, list):
        return '(' + ' '.join(sexp_canon(e) for e in node) + ')'
    return node


def canon(text):
    tree = sexp_parse(sexp_tokens(text))
    return sexp_canon(tree[0]) if tree else '()'


# ---- the .fs2 side ----

def section(text, name, *enders):
    start = text.index(name)
    end = len(text)
    for e in enders:
        i = text.find(e, start + len(name))
        if i != -1:
            end = min(end, i)
    return text[start:end]


def grab(block, field, default=None):
    m = re.search(re.escape(field) + r'[ \t]*([^\n]*)', block)
    if m is None:
        return default
    return m.group(1).strip()


def balanced(text, start):
    # the sexp beginning at text[start] == '(' -- ends where parens balance,
    # quote-aware
    depth = 0
    i = start
    in_str = False
    while i < len(text):
        c = text[i]
        if in_str:
            if c == '"':
                in_str = False
        elif c == '"':
            in_str = True
        elif c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
        i += 1
    raise ValueError('unbalanced sexp')


def formula_after(block, field):
    i = block.index(field)
    j = block.index('(', i)
    return balanced(block, j)


def multitext(block, field):
    i = block.find(field)
    if i < 0:
        return ''
    j = i + len(field)
    k = block.index('$end_multi_text', j)
    return xstr(block[j:k].strip())


def fs2_events(text):
    sec = section(text, '#Events', '#Goals')
    out = []
    for block in re.split(r'\n\$Formula:', sec)[1:]:
        block = '$Formula:' + block
        out.append({
            'name': grab(block, '+Name:', ''),
            'formula': canon(formula_after(block, '$Formula:')),
            'repeat_count': int(grab(block, '+Repeat Count:', '1')),
            'interval': int(grab(block, '+Interval:', '-1')),
            'score': int(grab(block, '+Score:', '0')),
            'chain_delay': int(grab(block, '+Chained:', '-1')),
            'objective_text': xstr(grab(block, '+Objective:', '')),
            'objective_key_text': xstr(grab(block, '+Objective key:', '')),
        })
    return out


def fs2_goals(text):
    sec = section(text, '#Goals', '#Waypoints', '#Messages')
    types = {'primary': 0, 'secondary': 1, 'bonus': 2}
    out = []
    for block in re.split(r'\n\$Type:', sec)[1:]:
        block = '$Type:' + block
        t = types[grab(block, '$Type:').lower()]
        if re.search(r'^\+Invalid:?\s*$', block, re.M):
            t |= 1 << 16
        out.append({
            'name': grab(block, '+Name:', ''),
            'type': t,
            'score': int(grab(block, '+Score:', '0')),
            'message': multitext(block, '$MessageNew:'),
            'formula': canon(formula_after(block, '$Formula:')),
        })
    return out


def fs2_messages(text):
    sec = section(text, '#Messages', '#Reinforcements')
    out = []
    for block in re.split(r'\n\$Name:', sec)[1:]:
        out.append({
            'name': block[:block.index('\n')].strip(),
            'text': multitext(block, '$MessageNew:'),
            'avi': grab(block, '+AVI Name:', ''),
            'wave': grab(block, '+Wave Name:', ''),
        })
    return out


def fs2_waypoints(text):
    sec = section(text, '#Waypoints', '#Messages')
    out = []
    for block in re.split(r'\n\$Name:', sec)[1:]:
        name = block[:block.index('\n')].strip()
        pts = [[f32(v) for v in triple.split(',')]
               for triple in re.findall(r'\(([^()]*)\)',
                                        formula_after(block, '$List:'))]
        out.append({'name': name, 'points': pts})
    return out


# ---- retail's ai-goal decode, replayed (aigoals.cc ai_add_goal_sub_sexp) --

AI_MODE = {
    'ai-chase': 1 << 1, 'ai-dock': 1 << 2, 'ai-waypoints': 1 << 3,
    'ai-waypoints-once': 1 << 4, 'ai-warp-out': 1 << 5, 'ai-warp': 1 << 5,
    'ai-destroy-subsystem': 1 << 6, 'ai-undock': 1 << 8,
    'ai-chase-wing': 1 << 9, 'ai-guard': 1 << 10, 'ai-disable-ship': 1 << 11,
    'ai-disarm-ship': 1 << 12, 'ai-chase-any': 1 << 13, 'ai-ignore': 1 << 14,
    'ai-guard-wing': 1 << 15, 'ai-evade-ship': 1 << 16,
    'ai-stay-near-ship': 1 << 17, 'ai-keep-safe-distance': 1 << 18,
    'ai-stay-still': 1 << 20, 'ai-play-dead': 1 << 21,
}
NO_TARGET = {'ai-warp-out', 'ai-undock', 'ai-chase-any', 'ai-play-dead',
             'ai-keep-safe-distance'}
SUBMODE = {'ai-dock': 21, 'ai-undock': 30,          # AIS_DOCK_0/AIS_UNDOCK_0
           'ai-disable-ship': -1, 'ai-disarm-ship': -2}  # -ENGINE/-TURRET


def decode_ai_goal(node, wings):
    op = node[0]
    mode = AI_MODE[op]
    target = '' if op in NO_TARGET else node[1].strip('"')
    if op in ('ai-chase', 'ai-guard') and target in wings:
        mode = AI_MODE[op + '-wing']
    prio = min(int(node[-1]), 89)   # PLAYER_PRIORITY_MIN bash, aigoals.cc:891
    return {'mode': mode, 'submode': SUBMODE.get(op, 0),
            'priority': prio, 'target': target}


def ai_goal_list(block, field, wings):
    if field not in block:
        return []
    tree = sexp_parse(sexp_tokens(formula_after(block, field)))
    return [decode_ai_goal(g, wings) for g in tree[0][1:]]  # (goals (ai-...))


def fs2_ai_goals(text):
    # ship-OWN goals only: under Fred_running wing goals never reach the
    # members (the copy runs at wing creation, which Fred skips)
    objects = section(text, '#Objects', '#Wings')
    wings = wing_names(text)
    per_ship = {}
    for block in re.split(r'\n\$Name:', objects)[1:]:
        name = block[:block.index('\n')].strip()
        per_ship[name] = ai_goal_list(block, '$AI Goals:', wings)
    return per_ship


def wing_names(text):
    sec = section(text, '#Wings', '#Events')
    return {m.strip() for m in re.findall(r'\$Name:\s*([^\n;]+)', sec)}


def fs2_wings(text):
    sec = section(text, '#Wings', '#Events')
    wings = wing_names(text)
    out = []
    for block in re.split(r'\n\$Name:', sec)[1:]:
        out.append({
            'name': block[:block.index('\n')].strip(),
            'ships': [s.strip('"') for s in re.findall(
                r'"[^"]*"', formula_after(block, '$Ships:'))],
            'num_waves': int(grab(block, '$Waves:')),
            'arrival_delay': int(grab(block, '+Arrival delay:', '0')),
            'arrival_cue': canon(formula_after(block, '$Arrival Cue:')),
            'departure_cue': canon(formula_after(block, '$Departure Cue:')),
            'goals': ai_goal_list(block, '$AI Goals:', wings),
        })
    return out


# ---- the .tres side ----

def tres_arrays(path):
    text = open(path).read()
    arrays = {}
    for m in re.finditer(r'^(\w+) = \[(.*?)\]\n(?=\w+ = |\Z)', text,
                         re.M | re.S):
        arrays[m.group(1)] = m.group(2)
    return text, arrays


def split_dicts(body):
    # top-level {...} spans, brace-depth and quote aware (ship entries nest
    # their ai_goals dicts)
    out = []
    depth = 0
    start = 0
    in_str = False
    for i, c in enumerate(body):
        if c == '"':
            nb = 0
            while i - nb - 1 >= 0 and body[i - nb - 1] == '\\':
                nb += 1
            if nb % 2 == 0:
                in_str = not in_str
        elif in_str:
            continue
        elif c == '{':
            if depth == 0:
                start = i
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                out.append(body[start + 1:i])
    return out


def dict_entries(body):
    out = []
    for block in split_dicts(body):
        entry = {}
        for m in re.finditer(
                r'"(\w+)": (?:"((?:[^"\\]|\\.)*)"|\[(.*?)\]|(-?\w+))',
                block, re.S):
            key, s, arr, word = m.groups()
            if s is not None:
                entry[key] = (s.replace('\\n', '\n').replace('\\"', '"')
                               .replace('\\\\', '\\'))
            elif arr is not None:
                entry[key] = arr
            else:
                entry[key] = word
        out.append(entry)
    return out


def fail(what, name, key, want, got):
    print('FAIL %s "%s" %s:\n  fs2:  %r\n  tres: %r'
          % (what, name, key, want, got))
    return 1


def compare_list(what, want, got, keys, int_keys):
    bad = 0
    if len(want) != len(got):
        print('FAIL %s: count %d (fs2) != %d (tres)'
              % (what, len(want), len(got)))
        return 1
    for w, g in zip(want, got):
        for k in keys:
            a, b = w[k], g.get(k, '')
            if k in int_keys:
                a, b = int(a), int(b)
            if a != b:
                bad += fail(what, w.get('name', '?'), k, a, b)
    return bad


def main():
    tres_path, fs2_path = sys.argv[1], sys.argv[2]
    # cp1252 mirrors the tool's esc() transcode (the corpus holds exactly
    # one high byte); retail also folds 0xdf to "ss" at read time, and the
    # 1.2-added missions are CRLF files
    text = open(fs2_path, encoding='cp1252', errors='replace').read()
    text = text.replace('\r\n', '\n').replace('ß', 'ss')
    # retail cuts every line at ';' before any parsing (read_file_text ->
    # strip_comments_fred; NOT quote-aware) -- the install's files carry
    # ";! Object #N" tags and stale comment tails that must fall away here
    # exactly as they do there
    text = '\n'.join(l.split(';', 1)[0] for l in text.split('\n'))
    _, arrays = tres_arrays(tres_path)

    bad = 0

    bad += compare_list(
        'event', fs2_events(text), dict_entries(arrays['events']),
        ['name', 'formula', 'repeat_count', 'interval', 'score',
         'chain_delay', 'objective_text', 'objective_key_text'],
        {'repeat_count', 'interval', 'score', 'chain_delay'})

    bad += compare_list(
        'goal', fs2_goals(text), dict_entries(arrays['goals']),
        ['name', 'type', 'score', 'message', 'formula'],
        {'type', 'score'})

    bad += compare_list(
        'message', fs2_messages(text), dict_entries(arrays['messages']),
        ['name', 'text', 'avi', 'wave'], set())

    want_wp = fs2_waypoints(text)
    got_wp = dict_entries(arrays['waypoints'])
    if len(want_wp) != len(got_wp):
        print('FAIL waypoints: count %d != %d' % (len(want_wp), len(got_wp)))
        bad += 1
    else:
        for w, g in zip(want_wp, got_wp):
            pts = [[f32(v) for v in m.group(1).split(',')]
                   for m in re.finditer(r'Vector3\(([^)]*)\)', g['points'])]
            if w['name'] != g['name'] or w['points'] != pts:
                bad += fail('waypoints', w['name'], 'points',
                            w['points'], pts)

    def check_goals(what, name, want, got):
        got = [{'mode': int(g['mode']), 'submode': int(g['submode']),
                'priority': int(g['priority']), 'target': g['target']}
               for g in got]
        return fail(what, name, 'goals', want, got) if want != got else 0

    want_ai = fs2_ai_goals(text)
    for ship in dict_entries(arrays['ships']):
        bad += check_goals('ai_goals', ship['name'],
                           want_ai.get(ship['name'], []),
                           dict_entries(ship.get('ai_goals', '')))

    want_wings = fs2_wings(text)
    got_wings = dict_entries(arrays['wings'])
    if len(want_wings) != len(got_wings):
        print('FAIL wings: count %d != %d'
              % (len(want_wings), len(got_wings)))
        bad += 1
    else:
        for w, g in zip(want_wings, got_wings):
            for k in ('name', 'arrival_cue', 'departure_cue'):
                if w[k] != g[k]:
                    bad += fail('wing', w['name'], k, w[k], g[k])
            for k in ('num_waves', 'arrival_delay'):
                if w[k] != int(g[k]):
                    bad += fail('wing', w['name'], k, w[k], g[k])
            ships = re.findall(r'"([^"]*)"', g['ships'])
            if w['ships'] != ships:
                bad += fail('wing', w['name'], 'ships', w['ships'], ships)
            bad += check_goals('wing', w['name'], w['goals'],
                               dict_entries(g.get('ai_goals', '')))

    if bad:
        sys.exit(1)
    print('OK %d events, %d goals, %d messages, %d waypoint lists'
          % (len(fs2_events(text)), len(fs2_goals(text)),
             len(fs2_messages(text)), len(want_wp)))


if __name__ == '__main__':
    main()
