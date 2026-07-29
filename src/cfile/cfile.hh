/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef __CFILE_H__
#define __CFILE_H__

#include <time.h>
#include <globalincs/pstypes.hh>

#define CF_EOF (-1)

#define CF_SEEK_SET (0)
#define CF_SEEK_CUR (1)
#define CF_SEEK_END (2)

typedef struct CFILE
{
    int id; // Index into cfile.cpp specific structure
    int version; // version of this file
} CFILE;

// extra info that can be returned when getting a file listing
typedef struct
{
    time_t write_time;
} file_list_info;

// Includes null terminater, so real length is 31
#define CF_MAX_FILENAME_LENGTH 32
// Includes null terminater, so real length is 255
#define CF_MAX_PATHNAME_LENGTH 256

// Used to check in any directory
#define CF_TYPE_ANY -1

#define CF_TYPE_INVALID 0
// Root must be 1!!
#define CF_TYPE_ROOT 1
#define CF_TYPE_DATA 2
#define CF_TYPE_MAPS 3
#define CF_TYPE_TEXT 4
#define CF_TYPE_MISSIONS 5
#define CF_TYPE_MODELS 6
#define CF_TYPE_TABLES 7
#define CF_TYPE_SOUNDS 8
#define CF_TYPE_SOUNDS_8B22K 9
#define CF_TYPE_SOUNDS_16B11K 10
#define CF_TYPE_VOICE 11
#define CF_TYPE_VOICE_BRIEFINGS 12
#define CF_TYPE_VOICE_CMD_BRIEF 13
#define CF_TYPE_VOICE_DEBRIEFINGS 14
#define CF_TYPE_VOICE_PERSONAS 15
#define CF_TYPE_VOICE_SPECIAL 16
#define CF_TYPE_VOICE_TRAINING 17
#define CF_TYPE_MUSIC 18
#define CF_TYPE_MOVIES 19
#define CF_TYPE_INTERFACE 20
#define CF_TYPE_FONT 21
#define CF_TYPE_EFFECTS 22
#define CF_TYPE_HUD 23
#define CF_TYPE_PLAYER_MAIN 24
#define CF_TYPE_PLAYER_IMAGES_MAIN 25
#define CF_TYPE_CACHE 26
#define CF_TYPE_PLAYERS 27
#define CF_TYPE_SINGLE_PLAYERS 28
#define CF_TYPE_MULTI_PLAYERS 29
#define CF_TYPE_MULTI_CACHE 30
#define CF_TYPE_CONFIG 31
#define CF_TYPE_SQUAD_IMAGES_MAIN 32
#define CF_TYPE_DEMOS 33
#define CF_TYPE_CBANIMS 34
#define CF_TYPE_INTEL_ANIMS 35

// Can be as high as you'd like
#define CF_MAX_PATH_TYPES 36

// TRUE if type is specified and valid
#define CF_TYPE_SPECIFIED(path_type)                                             \
    (((path_type) > CF_TYPE_INVALID) && ((path_type) < CF_MAX_PATH_TYPES))

// #define's for the type parameter in cfopen.
// open file normally
#define CFILE_NORMAL 0
// open file as a memory-mapped file
#define CFILE_MEMORY_MAPPED (1 << 0)

#define CF_SORT_NONE 0
#define CF_SORT_NAME 1
#define CF_SORT_TIME 2

#define cfread_fix(file) (fix) cfread_int(file)
#define cfwrite_fix(i, file) cfwrite_int(i, file)

// callback function used for get_file_list() to filter files to be added to list.  Return 1
// to add file to list, or 0 to not add it.
extern int (*Get_file_list_filter)(char *filename);

// cfile directory. valid after cfile_init() returns successfully
#define CFILE_ROOT_DIRECTORY_LEN 256
extern char Cfile_root_dir[CFILE_ROOT_DIRECTORY_LEN];

// Low-level functions
// Call this once at the beginning of the program
int cfile_init(char *exe_dir, char *cdrom_dir = NULL);

// Call this if pack files got added or removed or the
// cdrom changed.  This will refresh the list of filenames
// stored in packfiles and on the cdrom.
void cfile_refresh();

// add an extension to a filename if it doesn't already have it
char *cf_add_ext(const char *filename, const char *ext);

// return CF_TYPE (directory location type) of a CFILE you called cfopen() successfully on.
int cf_get_dir_type(CFILE *cfile);

// Opens the file.  If no path is given, use the extension to look into the
// default path.  If mode is NULL, delete the file.
CFILE *cfopen(const char *filename, const char *mode, int type = CFILE_NORMAL,
              int dir_type = CF_TYPE_ANY, bool localize = false);

// Flush the open file buffer
int cflush(CFILE *cfile);

// version number of opened file.  Will be 0 unless you put something else here after you
// open a file.  Once set, you can use minimum version numbers with the read functions.
void cf_set_version(CFILE *cfile, int version);

// Deletes a file.
void cf_delete(char *filename, int dir_type);

// Same as _access function to read a file's access bits
int cf_access(char *filename, int dir_type, int mode);

// Returns 1 if file exists, 0 if not.
int cf_exist(char *filename, int dir_type);

// ctmpfile() opens a temporary file stream.  File is deleted automatically when closed
CFILE *ctmpfile();

// Closes the file
int cfclose(CFILE *cfile);

// Returns size of file...
int cfilelength(CFILE *fp);

// Reads data
int cfread(void *buf, int elsize, int nelem, CFILE *fp);

// cfwrite() writes to the file
int cfwrite(const void *buf, int elsize, int nelem, CFILE *cfile);

// Reads/writes RLE compressed data.
int cfread_compressed(void *buf, int elsize, int nelem, CFILE *cfile);
int cfwrite_compressed(void *param_buf, int param_elsize, int param_nelem,
                       CFILE *cfile);

// Moves the file pointer
int cfseek(CFILE *fp, int offset, int where);

// Returns current position of file.
int cftell(CFILE *fp);

// cfputc() writes a character to a file
int cfputc(int c, CFILE *cfile);

// cfputs() writes a string to a file
int cfputs(const char *str, CFILE *cfile);

// cfgetc() reads a character to a file
int cfgetc(CFILE *cfile);

// cfgets() reads a string from a file
char *cfgets(char *buf, int n, CFILE *cfile);

// cfeof() Tests for end-of-file on a stream
int cfeof(CFILE *cfile);

// Return the data pointer associated with the CFILE structure (for memory mapped files)
void *cf_returndata(CFILE *cfile);

// get the 2 byte checksum of the passed filename - return 0 if operation failed, 1 if succeeded
int cf_chksum_short(char *filename, ushort *chksum, int max_size = -1,
                    int cf_type = CF_TYPE_ANY);

// get the 2 byte checksum of the passed file - return 0 if operation failed, 1 if succeeded
// NOTE : preserves current file position
int cf_chksum_short(CFILE *file, ushort *chksum, int max_size = -1);

// get the 32 bit CRC checksum of the passed filename - return 0 if operation failed, 1 if succeeded
int cf_chksum_long(char *filename, uint *chksum, int max_size = -1,
                   int cf_type = CF_TYPE_ANY);

// get the 32 bit CRC checksum of the passed file - return 0 if operation failed, 1 if succeeded
// NOTE : preserves current file position
int cf_chksum_long(CFILE *file, uint *chksum, int max_size = -1);

// convenient for misc checksumming purposes ------------------------------------------

// update cur_chksum with the chksum of the new_data of size new_data_size
ushort cf_add_chksum_short(ushort seed, char *buffer, int size);

// update cur_chksum with the chksum of the new_data of size new_data_size
unsigned long cf_add_chksum_long(unsigned long seed, char *buffer, int size);

// convenient for misc checksumming purposes ------------------------------------------

// High-level functions

// rename a file, utilizing the extension to determine where file is.
// successfully renamed the file
#define CF_RENAME_SUCCESS 0
// new name could not be created
#define CF_RENAME_FAIL_ACCESS 1
// old name does not exist
#define CF_RENAME_FAIL_EXIST 2
int cf_rename(char *old_name, char *name, int type = CF_TYPE_ANY);

// changes the attributes of a file
void cf_attrib(char *name, int set, int clear, int type);

// flush (delete all files in) the passed directory (by type), return the # of files deleted
// NOTE : WILL NOT DELETE READ-ONLY FILES
int cfile_flush_dir(int type);

// functions for reading from cfile
// These are all high level, built up from
// cfread.
int cfgetc(CFILE *fp);
char *cfgets(char *buf, size_t n, CFILE *fp);
char cfread_char(CFILE *file, int ver = 0, char deflt = 0);
ubyte cfread_ubyte(CFILE *file, int ver = 0, ubyte deflt = 0);
short cfread_short(CFILE *file, int ver = 0, short deflt = 0);
ushort cfread_ushort(CFILE *file, int ver = 0, ushort deflt = 0);
int cfread_int(CFILE *file, int ver = 0, int deflt = 0);
uint cfread_uint(CFILE *file, int ver = 0, uint deflt = 0);
float cfread_float(CFILE *file, int ver = 0, float deflt = 0.0f);
void cfread_vector(vector *vec, CFILE *file, int ver = 0, vector *deflt = NULL);
void cfread_angles(angles *ang, CFILE *file, int ver = 0, angles *deflt = NULL);

// Reads variable length, null-termined string.   Will only read up
// to n characters.
void cfread_string(char *buf, int n, CFILE *file);
// Read a fixed length that is null-terminatedm, and has the length
// stored in file
void cfread_string_len(char *buf, int n, CFILE *file);

// functions for writing cfiles
int cfwrite_char(char c, CFILE *file);
int cfwrite_float(float f, CFILE *file);
int cfwrite_int(int i, CFILE *file);
int cfwrite_uint(uint i, CFILE *file);
int cfwrite_short(short s, CFILE *file);
int cfwrite_ushort(ushort s, CFILE *file);
int cfwrite_ubyte(ubyte u, CFILE *file);
int cfwrite_vector(vector *vec, CFILE *file);
int cfwrite_angles(angles *ang, CFILE *file);

// writes variable length, null-termined string.
int cfwrite_string(const char *buf, CFILE *file);

// write a fixed length that is null-terminatedm, and has the length
// stored in file
int cfwrite_string_len(char *buf, CFILE *file);

int cf_get_file_list(int max, char **list, int type, const char *filter,
                     int sort = CF_SORT_NONE, file_list_info *info = NULL);
int cf_get_file_list_preallocated(int max, char arr[][MAX_FILENAME_LEN],
                                  char **list, int type, const char *filter,
                                  int sort = CF_SORT_NONE,
                                  file_list_info *info = NULL);
void cf_sort_filenames(int n, char **list, int sort, file_list_info *info = NULL);

// Searches for a file.   Follows all rules and precedence and searches
// CD's and pack files.
// Input:  filespace   - Filename & extension
//         pathtype    - See CF_TYPE_ defines in CFILE.H
// Output: pack_filename - Absolute path and filename of this file.   Could be a packfile or the actual file.
//         size        - File size
//         offset      - Offset into pack file.  0 if not a packfile.
// Returns: If not found returns 0.
int cf_find_file_location(char *filespec, int pathtype, char *pack_filename,
                          int *size, int *offset, bool localize = false);

// Functions to change directories
int cfile_chdir(char *dir);
int cfile_chdrive(int DriveNum, int flag);

// push current directory on a 'stack' (so we can restore it) and change the directory
int cfile_push_chdir(int type);

// restore directory on top of the stack
int cfile_pop_dir();

#endif /* __CFILE_H__ */
