// -*- mode: c++; -*-

#ifndef FREESPACE2_CFILE_CFILEARCHIVE_HH
#define FREESPACE2_CFILE_CFILEARCHIVE_HH

#include <defs.hh>

// The following Cfile_block data is private to cfile.cpp
// DO NOT MOVE the Cfile_block* information to cfile.h / do not extern this
// data
//
#define CFILE_BLOCK_UNUSED 0
#define CFILE_BLOCK_USED 1

struct Cfile_block  {
    int type;         // CFILE_BLOCK_UNUSED, CFILE_BLOCK_USED
    int dir_type;     // directory location
    FILE* fp;         // File pointer if opening an individual file
    const void* data; // Pointer for memory-mapped file access.  NULL if not
                      // mem-mapped.
    bool mem_mapped;  // Flag for memory mapped files (if data is not null and
                      // this is false it means that it's an embedded file)

    size_t data_length; // length of data for mmap
    size_t lib_offset;
    size_t raw_position;
    size_t size; // for packed files

    size_t max_read_len; // max read offset, for special error handling

    const char* source_file;
    int line_num;
};

#define MAX_CFILE_BLOCKS 64
extern Cfile_block Cfile_block_list[MAX_CFILE_BLOCKS];

// Called once to setup the low-level reading code.
void cf_init_lowlevel_read_code (
    CFILE* cfile, size_t lib_offset, size_t size, size_t pos);

#endif // FREESPACE2_CFILE_CFILEARCHIVE_HH
