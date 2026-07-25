#!/usr/bin/env python3
# Cross-check one pof2glb GLB against pof_dump's --model output, which is
# oracle-pinned to retail (tests/oracle, the three-way libpof/pofer/retail
# agreement). Pure two-arg checker -- `check_glb.py <glb> <model-dump>` -- so it
# stays useful by hand on a single model; the meson test's glb_check.sh driver
# runs it over the slice. Checks, in order:
#
#   1. GLB container: magic/version/length, JSON then BIN chunk tags, buffer
#      and every bufferView/accessor within the BIN chunk's bounds.
#   2. Node hierarchy: one node per submodel, names and parent links matching
#      the dump, and node translations matching the dump's offsets through the
#      emitter's axis map (see below).
#   3. Per-mesh triangle count: the GLB's indices/3 equals the dump's
#      Sum(nv - 2) fan triangulation for that submodel.
#   4. Triangle winding vs the stored normals. THIS is the check that earns its
#      keep: the first emitter got winding unanimously backwards (830/830 and
#      1248/1248 triangles disagreeing with their normals). pof2glb emits each
#      fan mirrored because the stored corner order is clockwise-vs-normals;
#      this is the guard that keeps it that way. See tools/pof2glb.cc's mesh()
#      comment and docs/pof-corpus-survey.txt.
#
#      The comparison is FIDELITY, not unanimity: the proxy here is the mean
#      *vertex* normal (the GLB carries no face normals), and retail data has
#      polygons whose smoothed vertex normals point against their own facet --
#      capital01 has exactly 4, all in debris meshes, while the winding itself
#      is corner-order-consistent 2932/2932 against the stored *face* normals
#      (measured both ways). So the expected disagreement count is computed
#      per submodel from the dump with the same proxy and must match exactly;
#      a real winding regression flips the agreeing majority and dies.
#
# Axis map: the dump prints FILE-frame offsets; pof2glb emits (x, y, -z) of
# them (libpof mirrors X on parse, the memory->glTF rotation mirrors it back,
# and only Z is left negated -- the net FILE->glTF is (x, y, -z)). No external
# deps: struct/json/re are all stdlib.
import json, re, struct, sys

def die(msg):
    print(f'FAIL: {msg}'); sys.exit(1)

glb_path, model_path = sys.argv[1], sys.argv[2]

# ---- parse the GLB container ----
raw = open(glb_path, 'rb').read()
magic, version, total = struct.unpack_from('<III', raw, 0)
if magic != 0x46546C67: die('bad magic')
if version != 2: die('bad version')
if total != len(raw): die(f'length field {total} != file size {len(raw)}')

jlen, jtag = struct.unpack_from('<II', raw, 12)
if jtag != 0x4E4F534A: die('first chunk not JSON')
doc = json.loads(raw[20:20 + jlen])

boff = 20 + jlen
blen, btag = struct.unpack_from('<II', raw, boff)
if btag != 0x004E4942: die('second chunk not BIN')
bin_data = raw[boff + 8: boff + 8 + blen]
if len(bin_data) != blen: die('BIN chunk truncated')
if doc['buffers'][0]['byteLength'] > blen: die('buffer overruns BIN chunk')

# ---- accessor/view bounds ----
for i, v in enumerate(doc['bufferViews']):
    if v['byteOffset'] + v['byteLength'] > blen:
        die(f'bufferView {i} out of bounds')
NCOMP = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3}
for i, a in enumerate(doc['accessors']):
    v = doc['bufferViews'][a['bufferView']]
    need = a['count'] * NCOMP[a['type']] * 4
    if need > v['byteLength']:
        die(f'accessor {i} overruns its view')

def acc_data(ix, fmt):
    a = doc['accessors'][ix]
    v = doc['bufferViews'][a['bufferView']]
    n = a['count'] * NCOMP[a['type']]
    return struct.unpack_from(f'<{n}{fmt}', bin_data, v['byteOffset'])

# ---- parse the model dump ----
subs = []   # (name, parent, offset, npolys, ntris, expected_disagree)
cur = None
triple = re.compile(r'\(([-+0-9.eE ]+)\)')

def f32(x):   # the float32 the dump's %.9g decimal denotes, as a double
    return struct.unpack('<f', struct.pack('<f', x))[0]
for line in open(model_path):
    m = re.match(r'  sub (\d+) "([^"]+)" parent (-?\d+)', line)
    if m:
        cur = int(m.group(1))
        subs.append([m.group(2), int(m.group(3)), None, 0, 0, 0])
        continue
    m = re.match(r'    radius \S+ offset \((\S+) (\S+) (\S+)\)', line)
    if m and cur is not None and subs[cur][2] is None:
        subs[cur][2] = tuple(float(m.group(i)) for i in (1, 2, 3))
        continue
    # textured polys print "poly tex N", flat-shaded ones "poly flat R G B"
    # (effect models -- warphole, subspacenode, t-laser -- are all-flat, and
    # flat polys turn up inside otherwise-textured debris too)
    m = re.match(r'      poly (?:tex -?\d+|flat \d+ \d+ \d+) nv (\d+)', line)
    if m and cur is not None:
        subs[cur][3] += 1
        subs[cur][4] += int(m.group(1)) - 2
        # Count the fan triangles whose geometric normal opposes the mean
        # vertex normal -- the expected disagreements for this submodel.
        # This REPLAYS the GLB-side computation below bit-for-bit: same
        # frame (file -> glTF (x, y, -z)), same emitted corner order
        # (v0, v[i], v[i-1] -- pof2glb mirrors each fan), same summation
        # order. Anything less exact flips sign on near-degenerate slivers
        # (bomber05 has a tri whose dot sits within an ulp of zero).
        # Line layout: normal (..) center (..) then (pos)(vnorm)[(uv)] per
        # vertex; uv is 2-wide, so 3-float groups are pos/vnorm alternating.
        # The dump's %.9g decimals identify a float32 exactly but parse to a
        # *different* nearest-double -- snap through float32 so the doubles
        # here are the same ones the GLB side unpacks (bomber05's sliver sits
        # at d~1e-13; the ~1e-10 parse skew is enough to flip its sign).
        t = [tuple(f32(float(x)) for x in g.split())
             for g in triple.findall(line) if len(g.split()) == 3]
        gv = [(p[0], p[1], -p[2]) for p in t[2::2]]   # positions, glTF frame
        gn = [(n[0], n[1], -n[2]) for n in t[3::2]]   # vertex normals, ditto
        for i in range(2, len(gv)):
            a, b, c = gv[0], gv[i], gv[i - 1]         # emitted order
            ux, uy, uz = b[0]-a[0], b[1]-a[1], b[2]-a[2]
            vx, vy, vz = c[0]-a[0], c[1]-a[1], c[2]-a[2]
            fx, fy, fz = uy*vz-uz*vy, uz*vx-ux*vz, ux*vy-uy*vx
            mx = sum(n[0] for n in (gn[0], gn[i], gn[i - 1]))
            my = sum(n[1] for n in (gn[0], gn[i], gn[i - 1]))
            mz = sum(n[2] for n in (gn[0], gn[i], gn[i - 1]))
            if fx*mx + fy*my + fz*mz < 0:
                subs[cur][5] += 1

# ---- node hierarchy, names, offsets ----
nodes = doc['nodes']
if len(nodes) != len(subs): die(f'{len(nodes)} nodes vs {len(subs)} subobjects')

child_parent = {}
for pix, n in enumerate(nodes):
    for c in n.get('children', []):
        child_parent[c] = pix

for i, (name, parent, off, npolys, ntris, xdis) in enumerate(subs):
    n = nodes[i]
    if n['name'] != name: die(f'node {i} name {n["name"]} != {name}')
    got_parent = child_parent.get(i, -1)
    if got_parent != parent: die(f'node {i} parent {got_parent} != {parent}')
    # dump offsets are FILE coords; emitted = (x, y, -z)
    want = (off[0], off[1], -off[2])
    t = n.get('translation', [0, 0, 0])
    if any(abs(a - b) > 1e-6 for a, b in zip(t, want)):
        die(f'node {i} translation {t} != {want}')

# ---- per-mesh triangle counts + winding vs normals ----
tot_tris = agree = disagree = tot_expected = 0
for i, (name, parent, off, npolys, ntris, xdis) in enumerate(subs):
    n = nodes[i]
    if npolys == 0:
        if 'mesh' in n: die(f'node {i} has a mesh but no polys in the dump')
        continue
    mesh = doc['meshes'][n['mesh']]
    if mesh['name'] != name: die(f'mesh name mismatch on node {i}')
    got = mesh_disagree = 0
    for prim in mesh['primitives']:
        ix = acc_data(prim['indices'], 'I')
        pos = acc_data(prim['attributes']['POSITION'], 'f')
        nrm = acc_data(prim['attributes']['NORMAL'], 'f')
        got += len(ix) // 3
        for t0 in range(0, len(ix), 3):
            a, b, c = ix[t0], ix[t0 + 1], ix[t0 + 2]
            ax, ay, az = pos[3*a:3*a+3]
            bx, by, bz = pos[3*b:3*b+3]
            cx, cy, cz = pos[3*c:3*c+3]
            ux, uy, uz = bx-ax, by-ay, bz-az
            vx, vy, vz = cx-ax, cy-ay, cz-az
            fx, fy, fz = uy*vz-uz*vy, uz*vx-ux*vz, ux*vy-uy*vx
            # against the mean vertex normal -- same proxy as the dump side
            mx = sum(nrm[3*k] for k in (a, b, c))
            my = sum(nrm[3*k+1] for k in (a, b, c))
            mz = sum(nrm[3*k+2] for k in (a, b, c))
            d = fx*mx + fy*my + fz*mz
            if d > 0: agree += 1
            elif d < 0: mesh_disagree += 1
    if got != ntris: die(f'node {i} ({name}): {got} triangles vs dump {ntris}')
    # fidelity, not unanimity: the emitted disagreements must be exactly the
    # source's own (retail debris carries facet-opposing vertex normals; see
    # header). Any converter winding slip changes these counts and dies here.
    if mesh_disagree != xdis:
        die(f'winding: {name}: {mesh_disagree} triangles disagree with their '
            f'vertex normals, source says {xdis}')
    disagree += mesh_disagree
    tot_expected += xdis
    tot_tris += got

print(f'OK: {len(nodes)} nodes, {tot_tris} triangles, '
      f'winding vs normals: {agree} agree / {disagree} disagree '
      f'(source expects {tot_expected})')
