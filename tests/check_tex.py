#!/usr/bin/env python3
# Compare pof2glb's transcoded PNG maps against retail's authoritative PCX
# decode. pcx_dump wrote, per texture, an .idx of retail's raw 8-bit indices +
# palette (through pcx_read_bitmap_8bpp). Here we expand those indices through
# the green colour-key (pcxutils.cc:266-274) -- an independent third
# implementation of the rule -- and check the result pixel-for-pixel against
# pof2glb's PNG. Bites on any decode or key divergence. Pure stdlib -- the PNG
# read is hand-rolled on zlib (chunk walk + scanline unfiltering), which keeps
# it independent of stb's writer.
#
#   check_tex.py <textures-dir> <idx-dir> <name> [name ...]

import struct
import sys
import zlib


def read_idx(path):
    with open(path, "rb") as f:
        w, h = struct.unpack("<ii", f.read(8))
        idx = f.read(w * h)
        pal = f.read(768)
    return w, h, idx, pal


def expand(w, h, idx, pal):
    # retail's indices are top-down (org_data row 0 = top); expand to RGBA with
    # the green colour-key: a palette entry of exactly (0,255,0) is transparent.
    out = bytearray(w * h * 4)
    for i, p in enumerate(idx):
        r, g, b = pal[p * 3], pal[p * 3 + 1], pal[p * 3 + 2]
        a = 0 if (r == 0 and g == 255 and b == 0) else 255
        out[i * 4:i * 4 + 4] = bytes((r, g, b, a))
    return bytes(out)


def read_png(path):
    with open(path, "rb") as f:
        d = f.read()
    if d[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit(f"{path}: not a PNG")
    pos, idat = 8, b""
    w = h = None
    while pos < len(d):
        (clen,) = struct.unpack(">I", d[pos:pos + 4])
        ctype = d[pos + 4:pos + 8]
        body = d[pos + 8:pos + 8 + clen]
        pos += 12 + clen  # length + type + body + crc
        if ctype == b"IHDR":
            w, h, depth, color, _, _, interlace = struct.unpack(
                ">IIBBBBB", body)
            if depth != 8 or color != 6 or interlace != 0:
                raise SystemExit(
                    f"{path}: expected 8-bit RGBA non-interlaced, got "
                    f"depth={depth} color={color} interlace={interlace}")
        elif ctype == b"IDAT":
            idat += body
        elif ctype == b"IEND":
            break
    raw = zlib.decompress(idat)
    # Undo the per-scanline filters (PNG spec 4.5.2: 0 none, 1 sub, 2 up,
    # 3 average, 4 paeth); rows are top-down RGBA already, matching retail.
    stride = w * 4
    out = bytearray()
    prev = bytes(stride)
    for row in range(h):
        ft = raw[row * (stride + 1)]
        line = bytearray(raw[row * (stride + 1) + 1:(row + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - 4] if i >= 4 else 0   # left, same channel
            b = prev[i]                        # up
            c = prev[i - 4] if i >= 4 else 0   # upper-left
            if ft == 1:
                line[i] = (line[i] + a) & 0xFF
            elif ft == 2:
                line[i] = (line[i] + b) & 0xFF
            elif ft == 3:
                line[i] = (line[i] + (a + b) // 2) & 0xFF
            elif ft == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 0xFF
        out += line
        prev = line
    return w, h, bytes(out)


def main():
    tex_dir, idx_dir = sys.argv[1], sys.argv[2]
    names = sys.argv[3:]
    ok = True
    for nm in names:
        low = nm.lower()
        w, h, idx, pal = read_idx(f"{idx_dir}/{low}.idx")
        ref = expand(w, h, idx, pal)
        tw, th, got = read_png(f"{tex_dir}/{low}.png")
        if (tw, th) != (w, h):
            print(f"  FAIL {nm}: PNG {tw}x{th} vs retail {w}x{h}")
            ok = False
            continue
        if got != ref:
            for i in range(0, len(ref), 4):
                if got[i:i + 4] != ref[i:i + 4]:
                    px, py = (i // 4) % w, (i // 4) // w
                    print(f"  FAIL {nm}: pixel ({px},{py}) "
                          f"png={tuple(got[i:i + 4])} "
                          f"retail={tuple(ref[i:i + 4])}")
                    break
            ok = False
            continue
        print(f"  OK   {nm}: {w}x{h} matches retail decode")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
