// Effects-art baker for the migration: decode retail .ani flipbooks with the
// port's authoritative anim code (animplay.cc / packunpack.cc -- the same RLE
// walk the game plays) and emit each as an RGBA PNG frame atlas plus a JSON
// sidecar the presentation reads (frame count, fps, grid shape). The
// transparent color (the ani's own xparent RGB, green by default) bakes to
// (0,0,0,0) so the atlas composes under either alpha or additive blending.
// tests/check_ani.py re-decodes every ani with an independent hand-rolled
// Python reader and compares pixel-for-pixel -- the pcx_dump/check_tex
// pattern, pinning decode + palette + transparency against retail.
//
//   ani2png [--aa] <game-root> <out-dir> <name> [<name> ...]
//
// --aa bakes interface art the way GR_AABITMAP reads it: the palette
// INDEX is the alpha (16 levels, scaled x17), the RGB is white -- the
// presenter tints with the HUD color at draw time. The palette's literal
// colors are never shown by retail for these.
//
// Names are bare, extension-less; anim_load appends .ani and cfile locates
// them (data/effects for the combat set). A name with no .ani behind it is
// tried as a still PCX (the laser bodies and glows live as stills beside
// the flipbooks) and bakes as a one-frame atlas with the same sidecar
// shape -- one loader on the scene side. Emits <out-dir>/<lower(name)>.png
// and <out-dir>/<lower(name)>.json.

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include <globalincs/pstypes.hh>
#include <anim/animplay.hh>
#include <anim/packunpack.hh>
#include <cfile/cfile.hh>
#include <graphics/2d.hh>
#include <pcxutils/pcxutils.hh>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

static std::string
lower(std::string s)
{
    for (char &c : s)
        c = (char)tolower((unsigned char)c);
    return s;
}

static bool aa_mode = false;

// one pixel into the atlas: color art through the palette + transparent
// key; --aa art as a white mask with the index for alpha
static void
put_pixel(unsigned char *dst, ubyte idx, const ubyte *pal, ubyte xr, ubyte xg,
          ubyte xb)
{
    if (aa_mode) {
        int a = idx * 17;
        dst[0] = dst[1] = dst[2] = 255;
        dst[3] = (unsigned char)(a > 255 ? 255 : a);
        return;
    }
    const ubyte *rgb = pal + idx * 3;
    if (rgb[0] == xr && rgb[1] == xg && rgb[2] == xb)
        return;                        // stays (0,0,0,0)
    dst[0] = rgb[0];
    dst[1] = rgb[1];
    dst[2] = rgb[2];
    dst[3] = 255;
}

// a still PCX as a one-frame atlas: the retail green key (0,255,0) bakes
// to (0,0,0,0), sidecar says frames 1 / fps 0
static bool
bake_still(const char *name, const std::string &out_dir)
{
    char fn[MAX_FILENAME_LEN];

    int w = 0, h = 0;
    ubyte pal[768];
    strncpy(fn, name, sizeof fn - 1);
    fn[sizeof fn - 1] = 0;
    if (pcx_read_header(fn, &w, &h, pal) != PCX_ERROR_NONE) {
        fprintf(stderr, "ani2png: no ani and no pcx for %s\n", name);
        return false;
    }

    std::vector<ubyte> idx((size_t)w * h);
    strncpy(fn, name, sizeof fn - 1);
    fn[sizeof fn - 1] = 0;
    if (pcx_read_bitmap_8bpp(fn, idx.data(), pal) != PCX_ERROR_NONE) {
        fprintf(stderr, "ani2png: pcx decode failed for %s\n", name);
        return false;
    }

    std::vector<unsigned char> rgba((size_t)w * h * 4, 0);
    for (size_t i = 0; i < idx.size(); i++)
        put_pixel(rgba.data() + i * 4, idx[i], pal, 0, 255, 0);

    const std::string stem = lower(name);
    const std::string png = out_dir + "/" + stem + ".png";
    const std::string json = out_dir + "/" + stem + ".json";

    if (!stbi_write_png(png.c_str(), w, h, 4, rgba.data(), w * 4)) {
        fprintf(stderr, "ani2png: cannot write %s\n", png.c_str());
        return false;
    }

    FILE *o = fopen(json.c_str(), "w");
    if (!o) {
        fprintf(stderr, "ani2png: cannot write %s\n", json.c_str());
        return false;
    }
    fprintf(o,
            "{\"name\": \"%s\", \"width\": %d, \"height\": %d, "
            "\"frames\": 1, \"fps\": 0, \"cols\": 1, \"rows\": 1, "
            "\"xparent\": [0, 255, 0], \"atlas\": \"%s.png\"}\n",
            stem.c_str(), w, h, stem.c_str());
    fclose(o);
    return true;
}

static bool
bake_one(const char *name, const std::string &out_dir)
{
    anim *a = anim_load(name, 0);
    if (!a)
        return bake_still(name, out_dir);

    // grid shape: as square-ish as fits under a 4096-pixel atlas edge --
    // exp05 (512x512, 94 frames) lands at 8x12
    int cols = 4096 / a->width;
    if (cols < 1)
        cols = 1;
    if (cols > a->total_frames)
        cols = a->total_frames;
    int rows = (a->total_frames + cols - 1) / cols;

    const int atlas_w = cols * a->width;
    const int atlas_h = rows * a->height;
    std::vector<unsigned char> atlas((size_t)atlas_w * atlas_h * 4, 0);

    // raw palette indices out of retail's own unpack (no translation, 8bpp);
    // the ani's palette + xparent RGB finish the job here
    anim_instance *inst = init_anim_instance(a, 8);

    int frames = 0;
    for (;;) {
        ubyte *f = anim_get_next_raw_buffer(inst, 0, 0, 8);
        if (!f || frames >= a->total_frames)
            break;

        const int cx = (frames % cols) * a->width;
        const int cy = (frames / cols) * a->height;

        for (int y = 0; y < a->height; y++) {
            const ubyte *src = f + (size_t)y * a->width;
            unsigned char *dst =
                atlas.data() + (((size_t)(cy + y) * atlas_w) + cx) * 4;

            for (int x = 0; x < a->width; x++, dst += 4)
                put_pixel(dst, src[x], a->palette, a->xparent_r,
                          a->xparent_g, a->xparent_b);
        }
        frames++;
    }

    bool ok = frames == a->total_frames;
    if (!ok)
        fprintf(stderr, "ani2png: %s delivered %d of %d frames\n", name,
                frames, a->total_frames);

    const std::string stem = lower(name);
    const std::string png = out_dir + "/" + stem + ".png";
    const std::string json = out_dir + "/" + stem + ".json";

    if (ok && !stbi_write_png(png.c_str(), atlas_w, atlas_h, 4, atlas.data(),
                              atlas_w * 4)) {
        fprintf(stderr, "ani2png: cannot write %s\n", png.c_str());
        ok = false;
    }

    if (ok) {
        FILE *o = fopen(json.c_str(), "w");
        if (!o) {
            fprintf(stderr, "ani2png: cannot write %s\n", json.c_str());
            ok = false;
        }
        else {
            fprintf(o,
                    "{\"name\": \"%s\", \"width\": %d, \"height\": %d, "
                    "\"frames\": %d, \"fps\": %d, \"cols\": %d, \"rows\": %d, "
                    "\"xparent\": [%d, %d, %d], \"atlas\": \"%s.png\"}\n",
                    stem.c_str(), a->width, a->height, a->total_frames, a->fps,
                    cols, rows, a->xparent_r, a->xparent_g, a->xparent_b,
                    stem.c_str());
            fclose(o);
        }
    }

    free_anim_instance(inst);
    anim_free(a);
    return ok;
}

int
main(int argc, char *argv[])
{
    if (argc < 4) {
        fprintf(stderr,
                "usage: ani2png <game-root> <out-dir> <name> [name ...]\n");
        return 2;
    }
    int argb = 1;
    if (strcmp(argv[argb], "--aa") == 0) {
        aa_mode = true;
        argb++;
    }
    if (argc < argb + 3) {
        fprintf(stderr,
                "usage: ani2png [--aa] <game-root> <out-dir> <name> ...\n");
        return 2;
    }
    const char *root = argv[argb];
    const std::string out_dir = argv[argb + 1];

    char exe_path[CF_MAX_PATHNAME_LENGTH];
    snprintf(exe_path, sizeof exe_path, "%s/x", root);
    if (cfile_init(exe_path)) {
        fprintf(stderr, "cfile_init failed for %s\n", root);
        return 1;
    }

    // hardware-mode palette path: anim_set_palette builds the identity
    // translation and never reaches palette_find (we bypass translation
    // anyway -- raw indices + the ani's own palette)
    gr_screen.bits_per_pixel = 16;

    int fail = 0;
    for (int i = argb + 2; i < argc; i++)
        if (!bake_one(argv[i], out_dir))
            fail = 1;
    return fail;
}
