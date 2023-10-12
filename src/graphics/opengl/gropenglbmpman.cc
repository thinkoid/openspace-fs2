// -*- mode: c++; -*-

#include <defs.hh>

#include <anim/animplay.hh>
#include <anim/packunpack.hh>
#include <cmdline/cmdline.hh>
#include <ddsutils/ddsutils.hh>
#include <shared/globals.hh>
#include "gropenglbmpman.hh"
#include "gropenglstate.hh"
#include "gropengltexture.hh"
#include <jpgutils/jpgutils.hh>
#include <pcxutils/pcxutils.hh>
#include <pngutils/pngutils.hh>
#include <tgautils/tgautils.hh>

#define BMPMAN_INTERNAL
#include <bmpman/bm_internal.hh>

int get_num_mipmap_levels (int w, int h) {
    int size, levels = 0;

    size = MAX (w, h);

    while (size > 0) {
        size >>= 1;
        levels++;
    }

    return (levels > 1) ? levels : 1;
}

/**
 * Anything API specific to freeing bm data
 */
void gr_opengl_bm_free_data (bitmap_slot* slot, bool release) {
    if (release) opengl_free_texture_slot (slot);

    if ((slot->entry.type == BM_TYPE_RENDER_TARGET_STATIC) ||
        (slot->entry.type == BM_TYPE_RENDER_TARGET_DYNAMIC))
        opengl_kill_render_target (slot);
}

/**
 * API specifics for creating a user bitmap
 */
void gr_opengl_bm_create (bitmap_slot* /*entry*/) {}

/**
 * API specific init instructions
 */
void gr_opengl_bm_init (bitmap_slot* slot) {
    slot->gr_info = new tcache_slot_opengl;
}

/**
 * Specific instructions for setting up the start of a page-in session
 */
void gr_opengl_bm_page_in_start () { opengl_preload_init (); }

extern bool opengl_texture_slot_valid (int n, int handle);

void gr_opengl_bm_save_render_target (int handle) {
    if (Cmdline_no_fbo) { return; }

    bitmap_entry* be = bm_get_entry (handle);
    bitmap* bmp = &be->bm;

    size_t rc = opengl_export_render_target (
        handle, bmp->w, bmp->h, (bmp->true_bpp == 32), be->num_mipmaps,
        (ubyte*)bmp->data);

    if (rc != be->mem_taken) {
        ASSERT (0);
        return;
    }

    dds_save_image (
        bmp->w, bmp->h, bmp->true_bpp, be->num_mipmaps, (ubyte*)bmp->data,
        (bmp->flags & BMP_FLAG_CUBEMAP));
}

int gr_opengl_bm_make_render_target (
    int handle, int* width, int* height, int* bpp, int* mm_lvl, int flags) {
    if (Cmdline_no_fbo) { return 0; }

    if ((flags & BMP_FLAG_CUBEMAP) && (*width != *height)) {
        MIN (*width, *height) = MAX (*width, *height);
    }

    if (opengl_make_render_target (
            handle, width, height, bpp, mm_lvl, flags)) {
        return 1;
    }

    return 0;
}

int gr_opengl_bm_set_render_target (int n, int face) {
    if (Cmdline_no_fbo) { return 0; }

    if (n == -1) {
        opengl_set_render_target (-1);
        return 1;
    }

    auto entry = bm_get_entry (n);

    int is_static = (entry->type == BM_TYPE_RENDER_TARGET_STATIC);

    if (opengl_set_render_target (n, face, is_static)) { return 1; }

    return 0;
}

bool gr_opengl_bm_data (int /*n*/, bitmap* /*bm*/) {
    // Do nothing here
    return true;
}
