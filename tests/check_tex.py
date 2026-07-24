#!/usr/bin/env python3
# Compare pof2glb's transcoded TGA maps against retail's authoritative PCX
# decode. pcx_dump wrote, per texture, an .idx of retail's raw 8-bit indices +
# palette (through pcx_read_bitmap_8bpp). Here we expand those indices through
# the green colour-key (pcxutils.cc:266-274) -- an independent third
# implementation of the rule -- and check the result pixel-for-pixel against
# pof2glb's TGA. Bites on any decode or key divergence. Pure stdlib.
#
#   check_tex.py <textures-dir> <idx-dir> <name> [name ...]

import struct
import sys


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


def read_tga(path):
    with open(path, "rb") as f:
        d = f.read()
    idlen, cmaptype, imgtype = d[0], d[1], d[2]
    w = d[12] | (d[13] << 8)
    h = d[14] | (d[15] << 8)
    bpp = d[16]
    desc = d[17]
    if imgtype != 2 or bpp != 32:
        raise SystemExit(
            f"{path}: expected uncompressed 32-bit TGA, got "
            f"imgtype={imgtype} bpp={bpp}")
    off = 18 + idlen  # truecolor: no colour map to skip
    px = d[off:off + w * h * 4]
    # TGA truecolor bytes are BGRA; rows run bottom-up unless descriptor bit 5
    # (0x20) marks top-down. Normalise to top-down RGBA to match retail.
    top_down = bool(desc & 0x20)
    rows = []
    for row in range(h):
        src = row if top_down else (h - 1 - row)
        line = px[src * w * 4:(src + 1) * w * 4]
        conv = bytearray(w * 4)
        for x in range(w):
            b, g, r, a = line[x * 4:x * 4 + 4]
            conv[x * 4:x * 4 + 4] = bytes((r, g, b, a))
        rows.append(bytes(conv))
    return w, h, b"".join(rows)


def main():
    tex_dir, idx_dir = sys.argv[1], sys.argv[2]
    names = sys.argv[3:]
    ok = True
    for nm in names:
        low = nm.lower()
        w, h, idx, pal = read_idx(f"{idx_dir}/{low}.idx")
        ref = expand(w, h, idx, pal)
        tw, th, got = read_tga(f"{tex_dir}/{low}.tga")
        if (tw, th) != (w, h):
            print(f"  FAIL {nm}: TGA {tw}x{th} vs retail {w}x{h}")
            ok = False
            continue
        if got != ref:
            for i in range(0, len(ref), 4):
                if got[i:i + 4] != ref[i:i + 4]:
                    px, py = (i // 4) % w, (i // 4) // w
                    print(f"  FAIL {nm}: pixel ({px},{py}) "
                          f"tga={tuple(got[i:i + 4])} "
                          f"retail={tuple(ref[i:i + 4])}")
                    break
            ok = False
            continue
        print(f"  OK   {nm}: {w}x{h} matches retail decode")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
