// -*- mode: c++; -*-

#ifndef FREESPACE2_TGAUTILS_TGAUTILS_HH
#define FREESPACE2_TGAUTILS_TGAUTILS_HH

#include <defs.hh>

#include <cfile/cfile.hh>

// --------------------
//
// Defines
//
// --------------------

#define TARGA_ERROR_NONE 0
#define TARGA_ERROR_READING 1
#define TARGA_ERROR_WRITING 2

// --------------------
//
// Prototypes
//
// --------------------

int targa_read_header (
    const char* filename, CFILE* img_cfp = NULL, int* w = 0, int* h = 0,
    int* bpp = 0, ubyte* palette = NULL);
int targa_read_bitmap (
    const char* filename, ubyte* data, ubyte* palette, int dest_size,
    int cf_type = CF_TYPE_ANY);
int targa_write_bitmap (
    const char* filename, ubyte* data, ubyte* palette, int w, int h, int bpp);

// The following are used by the tools\vani code.
int targa_compress (
    char* out, const char* in, int outsize, int pixsize, int bytecount);
int targa_uncompress (
    ubyte* dst, ubyte* src, int bitmap_width, int bytes_per_pixel);

#endif // FREESPACE2_TGAUTILS_TGAUTILS_HH
