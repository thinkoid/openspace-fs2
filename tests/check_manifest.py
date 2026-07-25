#!/usr/bin/env python3
# Verify one pof2glb manifest. Every digest is recomputed with hashlib, an
# independent SHA-256 implementation -- so this is also the oracle pinning the
# converter's hand-rolled FIPS 180-4 (pof2glb.cc): a mistake there mismatches
# every digest here. Output paths resolve relative to the manifest itself,
# the property that makes a converted tree relocatable.
#
#   check_manifest.py <manifest.json>
#
# A clean conversion has no warnings; any recorded warning is a FAIL here --
# the slice gate demands lossless conversion, while the tool itself only
# records what happened.

import hashlib
import json
import os
import sys


def sha256(path):
    with open(path, 'rb') as f:
        return hashlib.sha256(f.read()).hexdigest()


def main():
    mpath = sys.argv[1]
    mdir = os.path.dirname(mpath) or '.'
    with open(mpath) as f:
        m = json.load(f)

    bad = 0

    if m.get('tool') != 'pof2glb':
        print(f'FAIL: tool is {m.get("tool")!r}, not pof2glb')
        bad += 1
    if not m.get('version'):
        print('FAIL: empty converter version')
        bad += 1

    if not m.get('sources'):
        print('FAIL: no sources recorded')
        bad += 1
    for s in m.get('sources', []):
        if not os.path.exists(s['path']):
            print(f'FAIL: source missing: {s["path"]}')
            bad += 1
        elif sha256(s['path']) != s['sha256']:
            print(f'FAIL: source digest mismatch: {s["path"]}')
            bad += 1

    if not m.get('outputs'):
        print('FAIL: no outputs recorded')
        bad += 1
    for o in m.get('outputs', []):
        path = os.path.join(mdir, o['path'])
        if not os.path.exists(path):
            print(f'FAIL: output missing: {o["path"]}')
            bad += 1
        elif sha256(path) != o['sha256']:
            print(f'FAIL: output digest mismatch: {o["path"]}')
            bad += 1

    for w in m.get('warnings', []):
        print(f'FAIL: conversion warning: {w}')
        bad += 1

    if bad == 0:
        n = len(m['sources']) + len(m['outputs'])
        print(f'OK: version {m["version"]}, {n} digests verified, no warnings')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
