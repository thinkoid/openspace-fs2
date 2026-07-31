#!/usr/bin/env python3
# Independent oracle for ani2png: a from-scratch .ani reader -- header,
# keyframe table, all four RLE packing methods (packunpack.cc is the
# REFERENCE for the format, but this file shares no code with it) -- that
# re-derives every frame's RGBA and compares the tool's PNG atlas
# pixel-for-pixel, plus the sidecar's shape facts. The check_tex pattern:
# retail decodes through its own code in the tool, python re-decodes from
# the spec, the diff must be empty.
#
#   check_ani.py <ani-file> <baked-dir>
#
# <ani-file> is a real path -- the oracle reads bytes directly, no cfile;
# the tool located the same bytes through cfile.

import json
import os
import struct
import sys
from array import array

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from check_tex import read_png

PACKER = 0xEE        # PACKER_CODE (packunpack.hh:20); the header carries a
                     # per-file packer byte but retail unpacks against the
                     # global constant -- asserted equal below
TRANSPARENT = 254    # transparent_code (packunpack.cc:18), also a global


def u16(d, p):
    return struct.unpack_from("<H", d, p)[0]


def read_ani(path):
    with open(path, "rb") as f:
        d = f.read()

    p = 0
    width = u16(d, p)
    p += 2
    version, fps = 0, 30
    xparent = (0, 255, 0)
    if width == 0:
        version = u16(d, p)
        fps = u16(d, p + 2)
        p += 4
        if version >= 2:
            xparent = (d[p], d[p + 1], d[p + 2])
            p += 3
        width = u16(d, p)
        p += 2
    height = u16(d, p)
    frames = u16(d, p + 2)
    packer = d[p + 4]
    p += 5
    pal = d[p:p + 768]
    p += 768
    num_keys = u16(d, p)
    p += 2
    p += num_keys * 6          # (u16 frame, u32 offset) pairs, unused here
    p += 4                     # compressed-data size

    if packer != PACKER:
        raise SystemExit(f"{path}: packer byte {packer:#x}, retail unpacks "
                         f"against {PACKER:#x} -- format drift")

    # sequential decode; the frame buffer persists (delta frames write only
    # changed pixels, TRANSPARENT marks the holes)
    buf = bytearray(width * height)
    out = []
    for _ in range(frames):
        method = d[p]
        p += 1
        size = width * height
        i = 0
        if method in (0, 1):               # Hoffoss RLE (1 = key frame)
            key = method == 1
            while size > 0:
                v = d[p]
                p += 1
                if v != PACKER:
                    if key or v != TRANSPARENT:
                        buf[i] = v
                    i += 1
                    size -= 1
                else:
                    count = d[p]
                    p += 1
                    if count < 2:
                        value = PACKER
                    else:
                        value = d[p]
                        p += 1
                    count += 1
                    if count > size:
                        count = size
                    if key or value != TRANSPARENT:
                        buf[i:i + count] = bytes([value]) * count
                    i += count
                    size -= count
        elif method in (2, 3):             # standard RLE (3 = key frame)
            key = method == 3
            while size > 0:
                v = d[p]
                p += 1
                if not (v & 0x80):
                    if key or v != TRANSPARENT:
                        buf[i] = v
                    i += 1
                    size -= 1
                else:
                    count = v & 0x7F
                    value = d[p]
                    p += 1
                    if key or value != TRANSPARENT:
                        buf[i:i + count] = bytes([value]) * count
                    i += count
                    size -= count
        else:
            raise SystemExit(f"{path}: unknown packing method {method}")
        out.append(bytes(buf))

    return width, height, frames, fps, xparent, pal, out


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: check_ani.py <ani-file> <baked-dir>")
    ani_path, baked = sys.argv[1], sys.argv[2]
    stem = os.path.splitext(os.path.basename(ani_path))[0].lower()

    width, height, frames, fps, xparent, pal, decoded = read_ani(ani_path)

    with open(os.path.join(baked, stem + ".json")) as f:
        side = json.load(f)
    for field, want in (("width", width), ("height", height),
                        ("frames", frames), ("fps", fps),
                        ("xparent", list(xparent))):
        if side[field] != want:
            raise SystemExit(
                f"{stem}: sidecar {field} = {side[field]}, ani says {want}")

    aw, ah, pixels = read_png(os.path.join(baked, stem + ".png"))
    cols, rows = side["cols"], side["rows"]
    if aw != cols * width or ah != rows * height:
        raise SystemExit(f"{stem}: atlas {aw}x{ah}, sidecar grid says "
                         f"{cols * width}x{rows * height}")

    # palette index -> packed RGBA; the transparent color bakes to (0,0,0,0)
    pal32 = []
    for i in range(256):
        rgb = (pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2])
        pal32.append(0 if rgb == xparent else
                     struct.unpack("<I", bytes(rgb) + b"\xff")[0])

    for k in range(frames):
        cell = bytearray()
        cx, cy = (k % cols) * width, (k // cols) * height
        for y in range(height):
            row0 = ((cy + y) * aw + cx) * 4
            cell += pixels[row0:row0 + width * 4]
        got = array("I")
        got.frombytes(bytes(cell))
        want = array("I", map(pal32.__getitem__, decoded[k]))
        if got != want:
            for j in range(len(want)):
                if got[j] != want[j]:
                    raise SystemExit(
                        f"{stem}: frame {k} pixel {j} ({j % width},"
                        f"{j // width}): atlas {got[j]:#010x}, "
                        f"ani says {want[j]:#010x}")

    print(f"OK: {stem} -- {frames} frames {width}x{height} @ {fps} fps agree")
    sys.exit(0)


if __name__ == "__main__":
    main()
