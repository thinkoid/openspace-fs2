// Texture oracle for the migration converter: decode retail texture maps with
// the port's authoritative pcx_read_bitmap_8bpp (pcxutils.cc:87) and dump the
// raw 8-bit indices + 768-byte palette. tests/check_tex.py expands these
// through the green colour-key and compares the result to pof2glb's own
// hand-rolled TGA, pinning the converter's decode + key against retail
// pixel-for-pixel -- the same role pof_dump plays for geometry.
//
//   pcx_dump <game-root> <out-dir> <texture-name> [<texture-name> ...]
//
// Names are bare, extension-less TXTR basenames; the pcx reader forces .pcx
// and cfile locates them under data/maps. Emits one <out-dir>/<lower(name)>.idx
// per texture: int32 w, int32 h, then w*h index bytes, then 768 palette bytes,
// all little-endian -- the format tests/check_tex.py reads.

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include <globalincs/pstypes.hh>
#include <cfile/cfile.hh>
#include <pcxutils/pcxutils.hh>

static std::string
lower(std::string s)
{
    for (char &c : s)
        c = (char)tolower((unsigned char)c);
    return s;
}

// The pcx readers take a mutable char* and rewrite its extension in place
// (strchr('.')/strcat(".pcx")), so each call gets a fresh copy of the name.
static bool
dump_one(const char *name, const std::string &out_path)
{
    char fn[MAX_FILENAME_LEN];

    int w = 0, h = 0;
    ubyte pal[768];
    strncpy(fn, name, sizeof fn - 1);
    fn[sizeof fn - 1] = 0;
    if (pcx_read_header(fn, &w, &h, pal) != PCX_ERROR_NONE) {
        fprintf(stderr, "pcx_dump: header failed for %s\n", name);
        return false;
    }

    std::vector< ubyte > idx((size_t)w * h);
    strncpy(fn, name, sizeof fn - 1);
    fn[sizeof fn - 1] = 0;
    if (pcx_read_bitmap_8bpp(fn, idx.data(), pal) != PCX_ERROR_NONE) {
        fprintf(stderr, "pcx_dump: decode failed for %s\n", name);
        return false;
    }

    FILE *o = fopen(out_path.c_str(), "wb");
    if (!o) {
        fprintf(stderr, "pcx_dump: cannot write %s\n", out_path.c_str());
        return false;
    }
    fwrite(&w, sizeof(int), 1, o);
    fwrite(&h, sizeof(int), 1, o);
    fwrite(idx.data(), 1, idx.size(), o);
    fwrite(pal, 1, 768, o);
    fclose(o);
    return true;
}

int
main(int argc, char *argv[])
{
    if (argc < 4) {
        fprintf(stderr,
                "usage: pcx_dump <game-root> <out-dir> <name> [name ...]\n");
        return 2;
    }
    const char *root = argv[1];
    const std::string out_dir = argv[2];

    char exe_path[CF_MAX_PATHNAME_LENGTH];
    snprintf(exe_path, sizeof exe_path, "%s/x", root);
    if (cfile_init(exe_path)) {
        fprintf(stderr, "cfile_init failed for %s\n", root);
        return 1;
    }

    int fail = 0;
    for (int i = 3; i < argc; i++)
        if (!dump_one(argv[i], out_dir + "/" + lower(argv[i]) + ".idx"))
            fail = 1;
    return fail;
}
