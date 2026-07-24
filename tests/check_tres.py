#!/usr/bin/env python3
# Cross-check one pof2glb .tres (ship data) against pof_dump's --model output,
# which is oracle-pinned to retail (tests/oracle, the three-way libpof/pofer/
# retail agreement). Pure two-arg checker -- `check_tres.py <ship.tres>
# <model-dump>` -- so it stays useful by hand on a single model; the meson
# test's tres_check.sh driver runs it over the slice.
#
# The two sides reach every coordinate by DIFFERENT paths: the dump un-mirrors
# X straight off libpof's memory frame (app_vec in dump.cc), the .tres runs the
# memory frame through pof2glb's to_godot(). Agreement therefore validates the
# axis map end to end -- the role check_glb.py plays for the mesh. THE MAP: a
# dump FILE-frame vector (x, y, z) must appear in the .tres as (x, y, -z).
#
# Checked against the oracle: header scalars, gun/missile banks, turrets,
# thrusters, docks, eyes, paths, shield verts + tris. NOT checked: the
# `subsystems` array -- retail converts SPCL points against ships.tbl and the
# dump drops them (dump.cc), so they are emitted-but-unverified here and
# reported as such rather than silently passed. No external deps: re/sys only.
import re, sys

TOL = 1e-4

def die(msg):
    print(f'FAIL: {msg}'); sys.exit(1)

tres_path, model_path = sys.argv[1], sys.argv[2]

# ---- number/vector extraction from Godot literals --------------------
# The type identifiers carry digits of their own (Vector3, PackedInt32Array),
# so strip them before pulling the actual numbers out.
FLOAT = re.compile(r'-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?')
TYPENAME = re.compile(r'Packed(?:Vector3|Int32|Float32)Array|Vector3')

def nums(s):
    return [float(x) for x in FLOAT.findall(TYPENAME.sub(' ', s))]

def triples(s):
    f = nums(s)
    if len(f) % 3:
        die(f'coordinate list not a multiple of 3: {s[:60]}')
    return [tuple(f[i:i+3]) for i in range(0, len(f), 3)]

def close(a, b):
    return all(abs(x - y) <= TOL for x, y in zip(a, b))

# map a dump FILE-frame vector to the .tres's Godot frame: (x, y, -z)
def mapped(v):
    return (v[0], v[1], -v[2])

def want(dump_vecs):
    return [mapped(v) for v in dump_vecs]

def match_vecs(what, got, dump):
    exp = want(dump)
    if len(got) != len(exp):
        die(f'{what}: {len(got)} vectors vs oracle {len(exp)}')
    for i, (g, e) in enumerate(zip(got, exp)):
        if not close(g, e):
            die(f'{what}[{i}]: {g} != mapped oracle {e}')

# ---- parse the .tres -------------------------------------------------
# Every field is emitted on its own line as `name = <literal>`; dict-array
# fields hold flat {...} objects (no nested braces), so a {...} split is safe.
fields = {}
for line in open(tres_path):
    m = re.match(r'(\w+) = (.*)', line.rstrip('\n'))
    if m:
        fields[m.group(1)] = m.group(2)

def field(name):
    if name not in fields:
        die(f'.tres missing field {name}')
    return fields[name]

def dicts(name):
    return re.findall(r'\{[^{}]*\}', field(name))

def dval(d, key):
    # the substring after "key": up to the next `, "` or closing brace
    m = re.search(r'"' + key + r'":\s*(.*?)(?:,\s*"|\}$)', d)
    if not m:
        die(f'dict missing key {key}: {d[:80]}')
    return m.group(1)

# ---- parse the model dump -------------------------------------------
lines = open(model_path).read().splitlines()

def find(rx, start=0):
    for i in range(start, len(lines)):
        m = re.match(rx, lines[i])
        if m:
            return i, m
    die(f'oracle line not found: /{rx}/')

def vecs_on(line):
    return [tuple(g) for g in triples(line)]

# ---- header scalars --------------------------------------------------
_, m = find(r'radius (\S+)')
if abs(float(m.group(1)) - float(nums(field('radius'))[0])) > TOL:
    die('radius mismatch')

_, m = find(r'mass (\S+)')
if abs(float(m.group(1)) - float(nums(field('mass'))[0])) > TOL:
    die('mass mismatch')

_, m = find(r'mass_center \((\S+) (\S+) (\S+)\)')
mc_dump = mapped(tuple(float(m.group(i)) for i in (1, 2, 3)))
if not close(nums(field('mass_center')), mc_dump):
    die(f'mass_center {nums(field("mass_center"))} != mapped {mc_dump}')

_, m = find(r'details \d+(.*)')
if [int(x) for x in m.group(1).split()] != [int(x) for x in nums(field('detail_levels'))]:
    die('detail_levels mismatch')
_, m = find(r'debris \d+(.*)')
if [int(x) for x in m.group(1).split()] != [int(x) for x in nums(field('debris_pieces'))]:
    die('debris_pieces mismatch')

# ---- weapon banks ----------------------------------------------------
def check_banks(dump_kw, tres_field):
    i, m = find(rf'{dump_kw} (\d+)')
    nbanks = int(m.group(1))
    banks = dicts(tres_field)
    if len(banks) != nbanks:
        die(f'{tres_field}: {len(banks)} banks vs oracle {nbanks}')
    for b in range(nbanks):
        i, m = find(r'  bank (\d+) slots (\d+)', i + 1)
        nslots = int(m.group(2))
        dpts, dnorms = [], []
        for _ in range(nslots):
            i, sm = find(r'    slot \d+ point \((\S+) (\S+) (\S+)\) '
                         r'normal \((\S+) (\S+) (\S+)\)', i + 1)
            dpts.append(tuple(float(sm.group(k)) for k in (1, 2, 3)))
            dnorms.append(tuple(float(sm.group(k)) for k in (4, 5, 6)))
        match_vecs(f'{tres_field}[{b}].points', triples(dval(banks[b], 'points')), dpts)
        match_vecs(f'{tres_field}[{b}].normals', triples(dval(banks[b], 'normals')), dnorms)

check_banks('guns', 'gun_banks')
check_banks('missiles', 'missile_banks')

# ---- turrets ---------------------------------------------------------
i, m = find(r'turrets (\d+)')
nturr = int(m.group(1))
turr = dicts('turrets')
if len(turr) != nturr:
    die(f'turrets: {len(turr)} vs oracle {nturr}')
for t in range(nturr):
    i, tm = find(r'  turret sub (-?\d+) arm (-?\d+) normal '
                 r'\((\S+) (\S+) (\S+)\) points (\d+)(.*)', i + 1)
    base, arm = int(tm.group(1)), int(tm.group(2))
    dnorm = tuple(float(tm.group(k)) for k in (3, 4, 5))
    dpts = vecs_on(tm.group(7)) if tm.group(7).strip() else []
    d = turr[t]
    if int(nums(dval(d, 'base'))[0]) != base or int(nums(dval(d, 'arm'))[0]) != arm:
        die(f'turret[{t}] base/arm mismatch')
    if not close(nums(dval(d, 'normal')), mapped(dnorm)):
        die(f'turret[{t}] normal mismatch')
    match_vecs(f'turret[{t}].fire_points', triples(dval(d, 'fire_points')), dpts)

# ---- docks -----------------------------------------------------------
i, m = find(r'docks (\d+)')
ndock = int(m.group(1))
docks = dicts('docks')
if len(docks) != ndock:
    die(f'docks: {len(docks)} vs oracle {ndock}')
for dk in range(ndock):
    i, dm = find(r'  dock \d+ "[^"]*" paths (\d+)((?: \d+)*) slots (\d+)', i + 1)
    paths = [int(x) for x in dm.group(2).split()]
    nslots = int(dm.group(3))
    if [int(x) for x in nums(dval(docks[dk], 'paths'))] != paths:
        die(f'dock[{dk}] paths {nums(dval(docks[dk], "paths"))} != oracle {paths}')
    dpts, dnorms = [], []
    for _ in range(nslots):
        i, sm = find(r'    slot \d+ point \((\S+) (\S+) (\S+)\) '
                     r'normal \((\S+) (\S+) (\S+)\)', i + 1)
        dpts.append(tuple(float(sm.group(k)) for k in (1, 2, 3)))
        dnorms.append(tuple(float(sm.group(k)) for k in (4, 5, 6)))
    match_vecs(f'dock[{dk}].points', triples(dval(docks[dk], 'points')), dpts)
    match_vecs(f'dock[{dk}].normals', triples(dval(docks[dk], 'normals')), dnorms)

# ---- thrusters -------------------------------------------------------
i, m = find(r'thrusters (\d+)')
nthr = int(m.group(1))
thr = dicts('thrusters')
if len(thr) != nthr:
    die(f'thrusters: {len(thr)} vs oracle {nthr}')
for t in range(nthr):
    i, bm = find(r'  bank \d+ slots (\d+)', i + 1)
    nslots = int(bm.group(1))
    dpts, dnorms, drad = [], [], []
    for _ in range(nslots):
        i, sm = find(r'    slot \d+ point \((\S+) (\S+) (\S+)\) '
                     r'normal \((\S+) (\S+) (\S+)\) radius (\S+)', i + 1)
        dpts.append(tuple(float(sm.group(k)) for k in (1, 2, 3)))
        dnorms.append(tuple(float(sm.group(k)) for k in (4, 5, 6)))
        drad.append(float(sm.group(7)))
    match_vecs(f'thruster[{t}].points', triples(dval(thr[t], 'points')), dpts)
    match_vecs(f'thruster[{t}].normals', triples(dval(thr[t], 'normals')), dnorms)
    grad = nums(dval(thr[t], 'radii'))
    if len(grad) != len(drad) or not close(grad, drad):
        die(f'thruster[{t}] radii {grad} != oracle {drad}')

# ---- eyes ------------------------------------------------------------
i, m = find(r'eyes (\d+)')
neye = int(m.group(1))
eyes = dicts('eyes')
if len(eyes) != neye:
    die(f'eyes: {len(eyes)} vs oracle {neye}')
for e in range(neye):
    i, em = find(r'  eye \d+ parent (-?\d+) point \((\S+) (\S+) (\S+)\) '
                 r'normal \((\S+) (\S+) (\S+)\)', i + 1)
    if int(nums(dval(eyes[e], 'parent'))[0]) != int(em.group(1)):
        die(f'eye[{e}] parent mismatch')
    if not close(nums(dval(eyes[e], 'point')),
                 mapped(tuple(float(em.group(k)) for k in (2, 3, 4)))):
        die(f'eye[{e}] point mismatch')
    if not close(nums(dval(eyes[e], 'normal')),
                 mapped(tuple(float(em.group(k)) for k in (5, 6, 7)))):
        die(f'eye[{e}] normal mismatch')

# ---- shield ----------------------------------------------------------
i, m = find(r'shield verts (\d+) tris (\d+)')
nv, nt = int(m.group(1)), int(m.group(2))
dverts = []
for _ in range(nv):
    i, vm = find(r'  v \d+ \((\S+) (\S+) (\S+)\)', i + 1)
    dverts.append(tuple(float(vm.group(k)) for k in (1, 2, 3)))
match_vecs('shield_verts', triples(field('shield_verts')), dverts)

stris = dicts('shield_tris')
if len(stris) != nt:
    die(f'shield_tris: {len(stris)} vs oracle {nt}')
for t in range(nt):
    i, tm = find(r'  tri \d+ normal \((\S+) (\S+) (\S+)\) '
                 r'verts (\d+) (\d+) (\d+) neighbors (\d+) (\d+) (\d+)', i + 1)
    d = stris[t]
    if not close(nums(dval(d, 'normal')),
                 mapped(tuple(float(tm.group(k)) for k in (1, 2, 3)))):
        die(f'shield_tri[{t}] normal mismatch')
    if [int(x) for x in nums(dval(d, 'verts'))] != [int(tm.group(k)) for k in (4, 5, 6)]:
        die(f'shield_tri[{t}] verts mismatch')
    if [int(x) for x in nums(dval(d, 'neighbors'))] != [int(tm.group(k)) for k in (7, 8, 9)]:
        die(f'shield_tri[{t}] neighbors mismatch')

# ---- paths -----------------------------------------------------------
i, m = find(r'paths (\d+)')
npath = int(m.group(1))
paths = dicts('paths')
if len(paths) != npath:
    die(f'paths: {len(paths)} vs oracle {npath}')
for p in range(npath):
    i, pm = find(r'  path \d+ "[^"]*" parent "[^"]*" sub (-?\d+) verts (\d+)', i + 1)
    sub, nvert = int(pm.group(1)), int(pm.group(2))
    if int(nums(dval(paths[p], 'sub'))[0]) != sub:
        die(f'path[{p}] sub {nums(dval(paths[p], "sub"))} != oracle {sub}')
    dpts, drad = [], []
    for _ in range(nvert):
        i, vm = find(r'    vert \d+ \((\S+) (\S+) (\S+)\) radius (\S+)', i + 1)
        dpts.append(tuple(float(vm.group(k)) for k in (1, 2, 3)))
        drad.append(float(vm.group(4)))
    match_vecs(f'path[{p}].points', triples(dval(paths[p], 'points')), dpts)
    grad = nums(dval(paths[p], 'radii'))
    if len(grad) != len(drad) or not close(grad, drad):
        die(f'path[{p}] radii mismatch')

# ---- subsystems: emitted, not oracle-checkable -----------------------
nsub = len(dicts('subsystems'))

print(f'OK: banks {len(dicts("gun_banks"))}g/{len(dicts("missile_banks"))}m, '
      f'{nturr} turrets, {nthr} thrusters, {ndock} docks, {neye} eyes, '
      f'{npath} paths, shield {nv}v/{nt}t vs oracle; '
      f'{nsub} subsystems emitted (no oracle -- SPCL dropped by retail)')
