// -*- mode: c++; -*-

#include <defs.hh>
#include <assert/assert.hh>

#define _CFILE_INTERNAL

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <sstream>
#include <limits>

#include <cfile/cfile.hh>
#include <cfile/cfilearchive.hh>

#define CHECK_POSITION

// Called once to setup the low-level reading code.

void cf_init_lowlevel_read_code (
    CFILE* cfile, size_t lib_offset, size_t size, size_t pos) {
    ASSERT (cfile != NULL);

    Cfile_block* cb;
    ASSERT (cfile->id >= 0 && cfile->id < MAX_CFILE_BLOCKS);
    cb = &Cfile_block_list[cfile->id];

    cb->lib_offset = lib_offset;
    cb->raw_position = pos;
    cb->size = size;

    if (cb->fp) {
        if (cb->lib_offset) { fseek (cb->fp, (long)cb->lib_offset, SEEK_SET); }

#if defined(CHECK_POSITION) && !defined(NDEBUG)
        auto raw_position = ftell (cb->fp) - cb->lib_offset;
        ASSERT (raw_position == cb->raw_position);
#endif
    }
}

// cfeof() Tests for end-of-file on a stream
//
// returns a nonzero value after the first read operation that attempts to read
// past the end of the file. It returns 0 if the current position is not end of
// file. There is no error return.

int cfeof (CFILE* cfile) {
    ASSERT (cfile != NULL);

    Cfile_block* cb;
    ASSERT (cfile->id >= 0 && cfile->id < MAX_CFILE_BLOCKS);
    cb = &Cfile_block_list[cfile->id];

    int result = 0;

#if defined(CHECK_POSITION) && !defined(NDEBUG)
    if (cb->fp) {
        auto raw_position = ftell (cb->fp) - cb->lib_offset;
        ASSERT (raw_position == cb->raw_position);
    }
#endif

    if (cb->raw_position >= cb->size) { result = 1; }
    else {
        result = 0;
    }

    return result;
}

// cftell() returns offset into file
//
// returns:  success ==> offset into the file
// error   ==> -1
//
int cftell (CFILE* cfile) {
    ASSERT (cfile != NULL);
    Cfile_block* cb;
    ASSERT (cfile->id >= 0 && cfile->id < MAX_CFILE_BLOCKS);
    cb = &Cfile_block_list[cfile->id];

#if defined(CHECK_POSITION) && !defined(NDEBUG)
    if (cb->fp) {
        auto raw_position = ftell (cb->fp) - cb->lib_offset;
        ASSERT (raw_position == cb->raw_position);
    }
#endif

    // The rest of the code still uses ints, do an overflow check to detect
    // cases where this fails
    ASSERTX (
        cb->raw_position <=
            static_cast< size_t > (std::numeric_limits< int >::max ()),
        "Integer overflow in cftell, a file is probably too large (but I "
        "don't know which one).");
    return (int)cb->raw_position;
}

// cfseek() moves the file pointer
//
// returns:   success ==> 0
// error   ==> non-zero
//
int cfseek (CFILE* cfile, int offset, int where) {
    ASSERT (cfile != NULL);
    Cfile_block* cb;
    ASSERT (cfile->id >= 0 && cfile->id < MAX_CFILE_BLOCKS);
    cb = &Cfile_block_list[cfile->id];

    // TODO: seek to offset in memory mapped file
    ASSERT (!cb->mem_mapped);

    size_t goal_position;

    switch (where) {
    case CF_SEEK_SET: goal_position = offset + cb->lib_offset; break;
    case CF_SEEK_CUR: {
        goal_position = cb->raw_position + offset + cb->lib_offset;
    } break;
    case CF_SEEK_END:
        goal_position = cb->size + offset + cb->lib_offset;
        break;
    default: ASSERT (0); return 1;
    }

    // Make sure we don't seek beyond the end of the file
    CAP (goal_position, cb->lib_offset, cb->lib_offset + cb->size);

    int result = 0;

    if (cb->fp) {
        // If we have a file pointer we can also seek in that file
        result = fseek (cb->fp, (long)goal_position, SEEK_SET);
        ASSERTX (
            goal_position >= cb->lib_offset,
            "Invalid offset values detected while seeking! Goal "
            "was %zu, lib_offset is %zu.",
            goal_position, cb->lib_offset);
    }
    // If we only have a data pointer this will do all the work
    cb->raw_position = goal_position - cb->lib_offset;
    ASSERTX (
        cb->raw_position <= cb->size, "Invalid raw_position value detected!");

#if defined(CHECK_POSITION) && !defined(NDEBUG)
    if (cb->fp) {
        auto tmp_offset = ftell (cb->fp) - cb->lib_offset;
        ASSERT (tmp_offset == cb->raw_position);
    }
#endif

    return result;
}

// cfread() reads from a file
//
// returns:   returns the number of full elements read
//
//
int cfread (void* buf, int elsize, int nelem, CFILE* cfile) {
    if (!cf_is_valid (cfile)) return 0;

    size_t size = elsize * nelem;

    if (buf == NULL || size <= 0) return 0;

    Cfile_block* cb = &Cfile_block_list[cfile->id];

    if ((cb->raw_position + size) > cb->size) {
        ASSERTX (
            cb->raw_position <= cb->size,
            "Invalid raw_position value detected!");
        size = cb->size - cb->raw_position;
        if (size < 1) { return 0; }
        // mprintf(( "CFILE: EOF encountered in file\n" ));
    }

    if (cb->max_read_len) {
        if (cb->raw_position + size > cb->max_read_len) {
            std::ostringstream s_buf;
            s_buf << "Attempted to read " << size
                  << "-byte(s) beyond length limit";

            throw cfile::max_read_length (s_buf.str ());
        }
    }

    size_t bytes_read;
    if (cb->data != nullptr) {
        // This is a file from memory
        bytes_read = size;
        memcpy (
            buf, reinterpret_cast< const char* > (cb->data) + cb->raw_position,
            size);
    }
    else {
        bytes_read = fread (buf, 1, size, cb->fp);
    }
    if (bytes_read > 0) {
        cb->raw_position += bytes_read;
        ASSERTX (
            cb->raw_position <= cb->size,
            "Invalid raw_position value detected!");
    }

#if defined(CHECK_POSITION) && !defined(NDEBUG)
    if (cb->fp) {
        auto tmp_offset = ftell (cb->fp) - cb->lib_offset;
        ASSERT (tmp_offset == cb->raw_position);
    }
#endif

    return (int)(bytes_read / elsize);
}
