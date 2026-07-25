#!/usr/bin/env python3
# Verify one vpstage staging manifest three independent ways. Pure stdlib.
#
#   check_staging.py <staging.manifest.json> [rundir-root]
#
#   1. Digests: every staged file's SHA-256 recomputed with hashlib (the
#      independent oracle for the tools' hand-rolled FIPS 180-4), and every
#      archive's digest likewise.
#   2. Raw re-extraction: each member is sliced straight out of its archive
#      at the recorded offset/size and compared byte-for-byte against the
#      staged file -- an extraction check that never touches cfile, so a
#      cfile read bug and a manifest bookkeeping bug cannot cover for each
#      other.
#   3. Differential vs the unpacked install (optional second arg): each
#      staged file must be byte-identical to the loose rundir copy, matched
#      case-insensitively (the unpack and the VP TOC disagree on case).

import hashlib
import json
import os
import sys


def sha256(path):
    with open(path, 'rb') as f:
        return hashlib.sha256(f.read()).hexdigest()


def main():
    mpath = sys.argv[1]
    rundir = sys.argv[2] if len(sys.argv) > 2 else None
    mdir = os.path.dirname(mpath) or '.'
    with open(mpath) as f:
        m = json.load(f)

    bad = 0

    if m.get('tool') != 'vpstage':
        print(f'FAIL: tool is {m.get("tool")!r}, not vpstage')
        bad += 1
    if not m.get('version'):
        print('FAIL: empty version')
        bad += 1
    if not m.get('files'):
        print('FAIL: no files staged')
        bad += 1

    for a in m.get('archives', []):
        if sha256(a['path']) != a['sha256']:
            print(f'FAIL: archive digest mismatch: {a["path"]}')
            bad += 1

    # case-insensitive index of the unpacked install, per data subdir
    loose = {}
    if rundir:
        for sub in ('models', 'maps'):
            d = os.path.join(rundir, 'data', sub)
            for nm in os.listdir(d):
                loose[f'data/{sub}/{nm}'.lower()] = os.path.join(d, nm)

    archives = [open(a['path'], 'rb') for a in m.get('archives', [])]
    sliced = diffed = 0
    for e in m['files']:
        path = os.path.join(mdir, e['path'])
        with open(path, 'rb') as f:
            data = f.read()

        if len(data) != e['size']:
            print(f'FAIL: size mismatch: {e["path"]}')
            bad += 1
            continue
        if hashlib.sha256(data).hexdigest() != e['sha256']:
            print(f'FAIL: digest mismatch: {e["path"]}')
            bad += 1

        if e['archive'] >= 0:
            arc = archives[e['archive']]
            arc.seek(e['offset'])
            if arc.read(e['size']) != data:
                print(f'FAIL: archive slice mismatch: {e["path"]} '
                      f'@{e["offset"]} in '
                      f'{m["archives"][e["archive"]]["path"]}')
                bad += 1
            else:
                sliced += 1

        if rundir:
            lp = loose.get(e['path'].lower())
            if lp is None:
                print(f'FAIL: not in unpacked install: {e["path"]}')
                bad += 1
            else:
                with open(lp, 'rb') as f:
                    if f.read() != data:
                        print(f'FAIL: differs from unpacked: {e["path"]}')
                        bad += 1
                    else:
                        diffed += 1

    if bad == 0:
        print(f'OK: version {m["version"]}, {len(m["files"])} files, '
              f'{len(m.get("archives", []))} archives digest-verified; '
              f'{sliced} raw slices match, {diffed} match the unpacked install')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
