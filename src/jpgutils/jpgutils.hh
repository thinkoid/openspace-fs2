// -*- mode: c++; -*-

#ifndef FREESPACE2_JPGUTILS_JPGUTILS_HH
#define FREESPACE2_JPGUTILS_JPGUTILS_HH

#include <defs.hh>

#include <cfile/cfile.hh>

#define JPEG_ERROR_INVALID -1
#define JPEG_ERROR_NONE 0
#define JPEG_ERROR_READING 1

// reading
extern int jpeg_read_header (
    const char* real_filename, CFILE* img_cfp = NULL, int* w = 0, int* h = 0,
    int* bpp = 0, ubyte* palette = NULL);
extern int jpeg_read_bitmap (
    const char* real_filename, ubyte* image_data, ubyte* palette,
    int dest_size, int cf_type = CF_TYPE_ANY);

#endif // FREESPACE2_JPGUTILS_JPGUTILS_HH
