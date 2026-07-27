/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#define _CFILE_INTERNAL

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h> /* needed for memory mapping of file functions */

#include <filesystem>

#include <globalincs/pstypes.hh>
#include <cfile/cfile.hh>
#include <parse/encrypt.hh>
//#include <osapi/outwnd.hh>
//#include <math/vecmat.hh>
//#include <io/timer.hh>
#include <cfile/cfilesystem.hh>
#include <cfile/cfilearchive.hh>
#include <osapi/osapi.hh>

char Cfile_root_dir[CFILE_ROOT_DIRECTORY_LEN] = "";

// During cfile_init, verify that Pathtypes[n].index == n for each item
// Each path must have a valid parent that can be tracable all the way back to the root
// so that we can create directories when we need to.
//
cf_pathtype Pathtypes[CF_MAX_PATH_TYPES] = {
    // What type this is          Path                             Extensions              Parent type
    { CF_TYPE_INVALID, NULL, NULL, CF_TYPE_INVALID },
    // Root must be index 1!!
    { CF_TYPE_ROOT, "", ".mve", CF_TYPE_ROOT },
    { CF_TYPE_DATA, "data", ".cfg .log .txt", CF_TYPE_ROOT },
    { CF_TYPE_MAPS, "data/maps", ".pcx .ani .tga", CF_TYPE_DATA },
    { CF_TYPE_TEXT, "data/text", ".txt .net", CF_TYPE_DATA },
    { CF_TYPE_MISSIONS, "data/missions", ".fs2 .fc2 .ntl .ssv", CF_TYPE_DATA },
    { CF_TYPE_MODELS, "data/models", ".pof", CF_TYPE_DATA },
    { CF_TYPE_TABLES, "data/tables", ".tbl", CF_TYPE_DATA },
    { CF_TYPE_SOUNDS, "data/sounds", ".wav", CF_TYPE_DATA },
    { CF_TYPE_SOUNDS_8B22K, "data/sounds/8b22k", ".wav", CF_TYPE_SOUNDS },
    { CF_TYPE_SOUNDS_16B11K, "data/sounds/16b11k", ".wav", CF_TYPE_SOUNDS },
    { CF_TYPE_VOICE, "data/voice", "", CF_TYPE_DATA },
    { CF_TYPE_VOICE_BRIEFINGS, "data/voice/briefing", ".wav", CF_TYPE_VOICE },
    { CF_TYPE_VOICE_CMD_BRIEF, "data/voice/command_briefings", ".wav",
      CF_TYPE_VOICE },
    { CF_TYPE_VOICE_DEBRIEFINGS, "data/voice/debriefing", ".wav", CF_TYPE_VOICE },
    { CF_TYPE_VOICE_PERSONAS, "data/voice/personas", ".wav", CF_TYPE_VOICE },
    { CF_TYPE_VOICE_SPECIAL, "data/voice/special", ".wav", CF_TYPE_VOICE },
    { CF_TYPE_VOICE_TRAINING, "data/voice/training", ".wav", CF_TYPE_VOICE },
    { CF_TYPE_MUSIC, "data/music", ".wav", CF_TYPE_VOICE },
    { CF_TYPE_MOVIES, "data/movies", ".mve .msb", CF_TYPE_DATA },
    { CF_TYPE_INTERFACE, "data/interface", ".pcx .ani .tga", CF_TYPE_DATA },
    { CF_TYPE_FONT, "data/fonts", ".vf", CF_TYPE_DATA },
    { CF_TYPE_EFFECTS, "data/effects", ".ani .pcx .neb .tga", CF_TYPE_DATA },
    { CF_TYPE_HUD, "data/hud", ".ani .pcx .tga", CF_TYPE_DATA },
    { CF_TYPE_PLAYER_MAIN, "data/players", "", CF_TYPE_DATA },
    { CF_TYPE_PLAYER_IMAGES_MAIN, "data/players/images", ".pcx",
      CF_TYPE_PLAYER_MAIN },
    { CF_TYPE_CACHE, "data/cache", ".clr .tmp", CF_TYPE_DATA }, //clr=cached color
    { CF_TYPE_PLAYERS, "data/players", ".hcf", CF_TYPE_DATA },
    { CF_TYPE_SINGLE_PLAYERS, "data/players/single", ".plr .csg .css",
      CF_TYPE_PLAYERS },
    { CF_TYPE_MULTI_PLAYERS, "data/players/multi", ".plr", CF_TYPE_DATA },
    { CF_TYPE_MULTI_CACHE, "data/multidata", ".pcx .fs2", CF_TYPE_DATA },
    { CF_TYPE_CONFIG, "data/config", ".cfg", CF_TYPE_DATA },
    { CF_TYPE_SQUAD_IMAGES_MAIN, "data/players/squads", ".pcx", CF_TYPE_DATA },
    { CF_TYPE_DEMOS, "data/demos", ".fsd", CF_TYPE_DATA },
    { CF_TYPE_CBANIMS, "data/cbanims", ".ani", CF_TYPE_DATA },
    { CF_TYPE_INTEL_ANIMS, "data/intelanims", ".ani", CF_TYPE_DATA },
};

#define CFILE_STACK_MAX 8

int cfile_inited = 0;
int Cfile_stack_pos = 0;

char Cfile_stack[CFILE_STACK_MAX][128];

Cfile_block Cfile_block_list[MAX_CFILE_BLOCKS];
CFILE Cfile_list[MAX_CFILE_BLOCKS];

char *Cfile_cdrom_dir = NULL;

//
// Function prototypes for internally-called functions
//
int cfget_cfile_block();
CFILE *cf_open_fill_cfblock(FILE *fp, int type);
CFILE *cf_open_packed_cfblock(FILE *fp, int type, int offset, int size);
CFILE *cf_open_mapped_fill_cfblock(int fd, int type);
void cf_chksum_long_init();

void
cfile_close()
{
    cf_free_secondary_filelist();
}

// determine if the given path is in the root directory (/  or  /freespace2  etc)
int
cfile_in_root_dir(char *exe_path)
{
    int token_count = 0;
    char path_copy[2048] = "";
    char *tok;

    // bogus
    if (exe_path == NULL) {
        return 1;
    }

    // copy the path
    memset(path_copy, 0, 2048);
    strncpy(path_copy, exe_path, 2047);

    // count how many components there are in the path
    tok = strtok(path_copy, "/");
    if (tok == NULL) {
        return 1;
    }
    do {
        token_count++;
        tok = strtok(NULL, "/");
    } while (tok != NULL);

    // root directory if the exe name is the only path component
    if (token_count <= 1) {
        return 1;
    }

    // not-root directory
    return 0;
}

// cfile_init() initializes the cfile system.  Called once at application start.
//
// returns:  success ==> 0
//           error   ==> non-zero
//
int
cfile_init(char *exe_dir, char *cdrom_dir)
{
    int i;

    // initialize encryption
    encrypt_init();

    if (!cfile_inited) {
        char buf[128];

        cfile_inited = 1;

        strcpy(buf, exe_dir);
        i = strlen(buf);

        // are we in a root directory?
        if (cfile_in_root_dir(buf)) {
            fprintf(stderr,
                    "Freespace2/Fred2 cannot be run from a root directory!\n");
            return 1;
        }

        while (i--) {
            if (buf[i] == '/') {
                break;
            }
        }

        if (i >= 1) {
            buf[i] = 0;
            cfile_chdir(buf);
        }
        else {
            fprintf(stderr,
                    "Error trying to determine executable root directory!\n");
            return 1;
        }

        // set root directory
        strncpy(Cfile_root_dir, buf, CFILE_ROOT_DIRECTORY_LEN - 1);

        for (i = 0; i < MAX_CFILE_BLOCKS; i++) {
            Cfile_block_list[i].type = CFILE_BLOCK_UNUSED;
        }

        Cfile_cdrom_dir = cdrom_dir;
        cf_build_secondary_filelist(Cfile_cdrom_dir);

        // 32 bit CRC table init
        cf_chksum_long_init();

        atexit(cfile_close);
    }

    return 0;
}

// Call this if pack files got added or removed or the
// cdrom changed.  This will refresh the list of filenames
// stored in packfiles and on the cdrom.
void
cfile_refresh()
{
    cf_build_secondary_filelist(Cfile_cdrom_dir);
}

// Changes to a drive if valid.. 1=A, 2=B, etc
// If flag, then changes to it.
// Returns 0 if not-valid, 1 if valid.
int
cfile_chdrive(int DriveNum, int flag)
{
    // no drive letters on Linux; nothing is ever a valid drive
    (void)DriveNum;
    (void)flag;
    return 0;
}

// push current directory on a 'stack' (so we can restore it) and change the directory
int
cfile_push_chdir(int type)
{
    int e;
    char dir[128];
    char OriginalDirectory[128];
    char *Path;
    char NoDir[] = ".";

    getcwd(OriginalDirectory, 127);
    Assert(Cfile_stack_pos < CFILE_STACK_MAX);
    strcpy(Cfile_stack[Cfile_stack_pos++], OriginalDirectory);

    cf_create_default_path_string(dir, type, NULL);

    Path = dir;

    if (!(*Path)) {
        Path = NoDir;
    }

    // This chdir might get a critical error!
    e = chdir(Path);
    if (e) {
        return 2;
    }

    return 0;
}

int
cfile_chdir(char *dir)
{
    int e;
    char *Path;
    char NoDir[] = ".";

    Path = dir;

    if (!(*Path)) {
        Path = NoDir;
    }

    // This chdir might get a critical error!
    e = chdir(Path);
    if (e) {
        return 2;
    }

    return 0;
}

int
cfile_pop_dir()
{
    Assert(Cfile_stack_pos);
    Cfile_stack_pos--;
    return cfile_chdir(Cfile_stack[Cfile_stack_pos]);
}

// flush (delete all files in) the passed directory (by type), return the # of files deleted
// NOTE : WILL NOT DELETE READ-ONLY FILES
int
cfile_flush_dir(int dir_type)
{
    int del_count;

    Assert(CF_TYPE_SPECIFIED(dir_type));

    // attempt to change the directory to the passed type
    if (cfile_push_chdir(dir_type)) {
        return 0;
    }

    // proceed to delete the files
    del_count = 0;
    std::error_code ec, ec2;
    for (std::filesystem::directory_iterator it(".", ec), end; !ec && it != end;
         it.increment(ec)) {
        std::string name = it->path().filename().string();
        if (it->is_regular_file(ec2) && access(name.c_str(), W_OK) == 0) {
            // delete the file (skipping directories and read-only files)
            cf_delete((char *)name.c_str(), dir_type);

            // increment the deleted count
            del_count++;
        }
    }

    // pop the directory back
    cfile_pop_dir();

    // return the # of files deleted
    return del_count;
}

// add the given extention to a filename (or filepath) if it doesn't already have this
// extension.
//    filename = name of filename or filepath to process
//    ext = extension to add.  Must start with the period
//    Returns: new filename or filepath with extension.
char *
cf_add_ext(char *filename, char *ext)
{
    int flen, elen;
    static char path[MAX_PATH_LEN];

    flen = strlen(filename);
    elen = strlen(ext);
    Assert(flen < MAX_PATH_LEN);
    strcpy(path, filename);
    if ((flen < 4) || stricmp(path + flen - elen, ext)) {
        Assert(flen + elen < MAX_PATH_LEN);
        strcat(path, ext);
    }

    return path;
}

// Deletes a file.
void
cf_delete(char *filename, int dir_type)
{
    char longname[MAX_PATH_LEN];

    Assert(CF_TYPE_SPECIFIED(dir_type));

    cf_create_default_path_string(longname, dir_type, filename);

    FILE *fp = fopen(longname, "rb");
    if (fp) {
        // delete the file
        fclose(fp);
        unlink(longname);
    }
}

// Same as _access function to read a file's access bits
int
cf_access(char *filename, int dir_type, int mode)
{
    char longname[MAX_PATH_LEN];

    Assert(CF_TYPE_SPECIFIED(dir_type));

    cf_create_default_path_string(longname, dir_type, filename);

    return access(longname, mode);
}

// Returns 1 if file exists, 0 if not.
int
cf_exist(char *filename, int dir_type)
{
    char longname[MAX_PATH_LEN];

    Assert(CF_TYPE_SPECIFIED(dir_type));

    cf_create_default_path_string(longname, dir_type, filename);

    FILE *fp = fopen(longname, "rb");
    if (fp) {
        fclose(fp);
        return 1;
    }

    return 0;
}

void
cf_attrib(char *filename, int set, int clear, int dir_type)
{
    char longname[MAX_PATH_LEN];

    Assert(CF_TYPE_SPECIFIED(dir_type));

    cf_create_default_path_string(longname, dir_type, filename);

    FILE *fp = fopen(longname, "rb");
    if (fp) {
        fclose(fp);

        // only the Win32 read-only attribute (FILE_ATTRIBUTE_READONLY == 0x1)
        // is meaningful here; map it onto the write permission bits
        struct stat sb;
        if (stat(longname, &sb) == 0) {
            mode_t m = sb.st_mode;
            if (set & 1)
                m &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
            if (clear & 1)
                m |= S_IWUSR;
            chmod(longname, m);
        }
    }
}

int
cf_rename(char *old_name, char *name, int dir_type)
{
    Assert(CF_TYPE_SPECIFIED(dir_type));

    int ret_code;
    char old_longname[_MAX_PATH];
    char new_longname[_MAX_PATH];

    cf_create_default_path_string(old_longname, dir_type, old_name);
    cf_create_default_path_string(new_longname, dir_type, name);

    ret_code = rename(old_longname, new_longname);
    if (ret_code != 0) {
        switch (errno) {
        case EACCES:
            return CF_RENAME_FAIL_ACCESS;
        case ENOENT:
        default:
            return CF_RENAME_FAIL_EXIST;
        }
    }

    return CF_RENAME_SUCCESS;
}

// Creates the directory path if it doesn't exist. Even creates all its
// parent paths.
void
cf_create_directory(int dir_type)
{
    int num_dirs = 0;
    int dir_tree[CF_MAX_PATH_TYPES];
    char longname[MAX_PATH_LEN];

    Assert(CF_TYPE_SPECIFIED(dir_type));

    int current_dir = dir_type;

    do {
        Assert(num_dirs < CF_MAX_PATH_TYPES); // Invalid Pathtypes data?

        dir_tree[num_dirs++] = current_dir;
        current_dir = Pathtypes[current_dir].parent_index;

    } while (current_dir != CF_TYPE_ROOT);

    int i;

    for (i = num_dirs - 1; i >= 0; i--) {
        cf_create_default_path_string(longname, dir_tree[i], NULL);

        if (mkdir(longname, 0755) == 0) {
            mprintf(("CFILE: Created new directory '%s'\n", longname));
        }
    }
}

extern int game_cd_changed();

// cfopen()
//
// parameters:  *filepath ==> name of file to open (may be path+name)
//              *mode     ==> specifies how file should be opened (eg "rb" for read binary)
//                            passing NULL to mode deletes the file if it exists and returns NULL
//               type     ==> one of:    CFILE_NORMAL
//                                       CFILE_MEMORY_MAPPED
//               dir_type  => override extension check, value is one of CF_TYPE* #defines
//
//               NOTE: type parameter is an optional parameter.  The default value is CFILE_NORMAL
//
//
// returns:    success ==> address of CFILE structure
//             error   ==> NULL
//

CFILE *
cfopen(char *file_path, char *mode, int type, int dir_type, bool localize)
{
    char longname[_MAX_PATH];

// nprintf(("CFILE", "CFILE -- trying to open %s\n", file_path ));
// #if !defined(MULTIPLAYER_BETA_BUILD) && !defined(FS2_DEMO)

// we no longer need to do this, and on machines with crappy-ass drivers it can slow things down horribly.
#if 0 
   if ( game_cd_changed() ) {
      cfile_refresh();
   }
#endif

    // Check that all the parameters make sense
    Assert(file_path && strlen(file_path));
    Assert(mode != NULL);

    // Can only open read-only binary files in memory mapped mode.
    if ((type & CFILE_MEMORY_MAPPED) && strcmp(mode, "rb")) {
        Int3();
        return NULL;
    }

    // If in write mode, just try to open the file straight off
    // the harddisk.  No fancy packfile stuff here!

    if (strchr(mode, 'w')) {
        // For write-only files, require a full path or a path type
        if (strpbrk(file_path, "/")) {
            // Full path given?
            strcpy(longname, file_path);
        }
        else {
            // Path type given?
            Assert(dir_type != CF_TYPE_ANY);

            // Create the directory if necessary
            cf_create_directory(dir_type);

            cf_create_default_path_string(longname, dir_type, file_path);
        }
        Assert(!(type & CFILE_MEMORY_MAPPED));

        // JOHN: TODO, you should create the path if it doesn't exist.

        FILE *fp = fopen(longname, mode);
        if (fp) {
            return cf_open_fill_cfblock(fp, dir_type);
        }
        return NULL;
    }

    // Search for file on disk, on cdrom, or in a packfile

    int offset, size;
    char copy_file_path
        [MAX_PATH_LEN]; // FIX change in memory from cf_find_file_location
    strcpy(copy_file_path, file_path);

    if (cf_find_file_location(copy_file_path, dir_type, longname, &size, &offset,
                              localize)) {
        // Fount it, now create a cfile out of it

        if (type & CFILE_MEMORY_MAPPED) {
            // Can't open memory mapped files out of pack files
            if (offset == 0) {
                int fd;

                fd = open(longname, O_RDONLY);

                if (fd != -1) {
                    return cf_open_mapped_fill_cfblock(fd, dir_type);
                }
            }
        }
        else {
            FILE *fp = fopen(longname, "rb");

            if (fp) {
                if (offset) {
                    // Found it in a pack file
                    return cf_open_packed_cfblock(fp, dir_type, offset, size);
                }
                else {
                    // Found it in a normal file
                    return cf_open_fill_cfblock(fp, dir_type);
                }
            }
        }
    }

    return NULL;
}

// ------------------------------------------------------------------------
// ctmpfile()
//
// Open up a temporary file.  A unique name is automatically generated.  The
// file will be automatically deleted when file is closed.
//
// return:     NULL              =>    tmp file could not be opened
//             pointer to CFILE  =>    tmp file successfully opened
//
CFILE *
ctmpfile()
{
    FILE *fp;
    fp = tmpfile();
    if (fp)
        return cf_open_fill_cfblock(fp, 0);
    else
        return NULL;
}

// cfget_cfile_block() will try to find an empty Cfile_block structure in the
// Cfile_block_list[] array and return the index.
//
// returns:   success ==> index in Cfile_block_list[] array
//            failure ==> -1
//
int
cfget_cfile_block()
{
    int i;
    Cfile_block *cb;

    for (i = 0; i < MAX_CFILE_BLOCKS; i++) {
        cb = &Cfile_block_list[i];
        if (cb->type == CFILE_BLOCK_UNUSED) {
            cb->data = NULL;
            cb->fp = NULL;
            cb->fd = -1;
            cb->type = CFILE_BLOCK_USED;
            return i;
        }
    }

    // If we've reached this point, a free Cfile_block could not be found
    nprintf(("Warning", "A free Cfile_block could not be found.\n"));
    Assert(0); // out of free cfile blocks
    return -1;
}

// cfclose() closes the file
//
// returns:   success ==> 0
//            failure ==> EOF
//
int
cfclose(CFILE *cfile)
{
    int result;

    Assert(cfile != NULL);
    Cfile_block *cb;
    Assert(cfile->id >= 0 && cfile->id < MAX_CFILE_BLOCKS);
    cb = &Cfile_block_list[cfile->id];

    result = 0;
    if (cb->data) {
        // close memory mapped file
        result = munmap(cb->data, cb->map_len);
        Assert(result == 0);
        result = close(cb->fd);
        Assert(result == 0); // Ensure file descriptor is closed properly
        cb->fd = -1;
        result = 0;
    }
    else if (cb->fp != NULL) {
        Assert(cb->fp != NULL);
        result = fclose(cb->fp);
    }
    else {
        // VP  do nothing
    }

    cb->type = CFILE_BLOCK_UNUSED;
    return result;
}

// cf_open_fill_cfblock() will fill up a Cfile_block element in the Cfile_block_list[] array
// for the case of a file being opened by cf_open();
//
// returns:   success ==> ptr to CFILE structure.
//            error   ==> NULL
//
CFILE *
cf_open_fill_cfblock(FILE *fp, int type)
{
    int cfile_block_index;

    cfile_block_index = cfget_cfile_block();
    if (cfile_block_index == -1) {
        return NULL;
    }
    else {
        CFILE *cfp;
        Cfile_block *cfbp;
        cfbp = &Cfile_block_list[cfile_block_index];
        cfp = &Cfile_list[cfile_block_index];
        ;
        cfp->id = cfile_block_index;
        cfp->version = 0;
        cfbp->data = NULL;
        cfbp->fp = fp;
        cfbp->dir_type = type;

        cf_init_lowlevel_read_code(cfp, 0, filelength(fileno(fp)));

        return cfp;
    }
}

// cf_open_packed_cfblock() will fill up a Cfile_block element in the Cfile_block_list[] array
// for the case of a file being opened by cf_open();
//
// returns:   success ==> ptr to CFILE structure.
//            error   ==> NULL
//
CFILE *
cf_open_packed_cfblock(FILE *fp, int type, int offset, int size)
{
    // Found it in a pack file
    int cfile_block_index;

    cfile_block_index = cfget_cfile_block();
    if (cfile_block_index == -1) {
        return NULL;
    }
    else {
        CFILE *cfp;
        Cfile_block *cfbp;
        cfbp = &Cfile_block_list[cfile_block_index];

        cfp = &Cfile_list[cfile_block_index];
        cfp->id = cfile_block_index;
        cfp->version = 0;
        cfbp->data = NULL;
        cfbp->fp = fp;
        cfbp->dir_type = type;

        cf_init_lowlevel_read_code(cfp, offset, size);

        return cfp;
    }
}

// cf_open_mapped_fill_cfblock() will fill up a Cfile_block element in the Cfile_block_list[] array
// for the case of a file being opened by cf_open_mapped();
//
// returns:   ptr CFILE structure.
//
CFILE *
cf_open_mapped_fill_cfblock(int fd, int type)
{
    int cfile_block_index;

    cfile_block_index = cfget_cfile_block();
    if (cfile_block_index == -1) {
        close(fd);
        return NULL;
    }
    else {
        CFILE *cfp;
        Cfile_block *cfbp;
        cfbp = &Cfile_block_list[cfile_block_index];

        cfp = &Cfile_list[cfile_block_index];
        cfp->id = cfile_block_index;
        cfbp->fp = NULL;
        cfbp->fd = fd;
        cfbp->dir_type = type;

        cf_init_lowlevel_read_code(cfp, 0, 0);

        cfbp->map_len = (size_t)filelength(fd);
        cfbp->data = mmap(NULL, cfbp->map_len, PROT_READ, MAP_PRIVATE, fd, 0);
        if (cfbp->data == MAP_FAILED) {
            nprintf(("Error", "Could not create file-mapping object.\n"));
            cfbp->data = NULL;
            close(cfbp->fd);
            cfbp->fd = -1;
            cfbp->type = CFILE_BLOCK_UNUSED;
            return NULL;
        }

        Assert(cfbp->data != NULL);
        return cfp;
    }
}

int
cf_get_dir_type(CFILE *cfile)
{
    return Cfile_block_list[cfile->id].dir_type;
}

// cf_returndata() returns the data pointer for a memory-mapped file that is associated
// with the CFILE structure passed as a parameter
//
//

void *
cf_returndata(CFILE *cfile)
{
    Assert(cfile != NULL);
    Cfile_block *cb;
    Assert(cfile->id >= 0 && cfile->id < MAX_CFILE_BLOCKS);
    cb = &Cfile_block_list[cfile->id];
    Assert(cb->data != NULL);
    return cb->data;
}

// version number of opened file.  Will be 0 unless you put something else here after you
// open a file.  Once set, you can use minimum version numbers with the read functions.
void
cf_set_version(CFILE *cfile, int version)
{
    Assert(cfile != NULL);

    cfile->version = version;
}

// routines to read basic data types from CFILE's.  Put here to
// simplify mac/pc reading from cfiles.

float
cfread_float(CFILE *file, int ver, float deflt)
{
    float f;

    if (file->version < ver)
        return deflt;

    if (cfread(&f, sizeof(f), 1, file) != 1)
        return deflt;

    //   i = INTEL_INT(i);       //  hmm, not sure what to do here
    return f;
}

int
cfread_int(CFILE *file, int ver, int deflt)
{
    int i;

    if (file->version < ver)
        return deflt;

    if (cfread(&i, sizeof(i), 1, file) != 1)
        return deflt;

    i = INTEL_INT(i);
    return i;
}

uint
cfread_uint(CFILE *file, int ver, uint deflt)
{
    uint i;

    if (file->version < ver)
        return deflt;

    if (cfread(&i, sizeof(i), 1, file) != 1)
        return deflt;

    i = INTEL_INT(i);
    return i;
}

short
cfread_short(CFILE *file, int ver, short deflt)
{
    short s;

    if (file->version < ver)
        return deflt;

    if (cfread(&s, sizeof(s), 1, file) != 1)
        return deflt;

    s = INTEL_SHORT(s);
    return s;
}

ushort
cfread_ushort(CFILE *file, int ver, ushort deflt)
{
    ushort s;

    if (file->version < ver)
        return deflt;

    if (cfread(&s, sizeof(s), 1, file) != 1)
        return deflt;

    s = INTEL_SHORT(s);
    return s;
}

ubyte
cfread_ubyte(CFILE *file, int ver, ubyte deflt)
{
    ubyte b;

    if (file->version < ver)
        return deflt;

    if (cfread(&b, sizeof(b), 1, file) != 1)
        return deflt;

    return b;
}

void
cfread_vector(vector *vec, CFILE *file, int ver, vector *deflt)
{
    if (file->version < ver) {
        if (deflt)
            *vec = *deflt;
        else
            vec->x = vec->y = vec->z = 0.0f;

        return;
    }

    vec->x = cfread_float(file, ver, deflt ? deflt->x : NULL);
    vec->y = cfread_float(file, ver, deflt ? deflt->y : NULL);
    vec->z = cfread_float(file, ver, deflt ? deflt->z : NULL);
}

void
cfread_angles(angles *ang, CFILE *file, int ver, angles *deflt)
{
    if (file->version < ver) {
        if (deflt)
            *ang = *deflt;
        else
            ang->p = ang->b = ang->h = 0.0f;

        return;
    }

    ang->p = cfread_float(file, ver, deflt ? deflt->p : NULL);
    ang->b = cfread_float(file, ver, deflt ? deflt->b : NULL);
    ang->h = cfread_float(file, ver, deflt ? deflt->h : NULL);
}

char
cfread_char(CFILE *file, int ver, char deflt)
{
    char b;

    if (file->version < ver)
        return deflt;

    if (cfread(&b, sizeof(b), 1, file) != 1)
        return deflt;

    return b;
}

void
cfread_string(char *buf, int n, CFILE *file)
{
    char c;

    do {
        c = cfread_char(file);
        if (n > 0) {
            *buf++ = c;
            n--;
        }
    } while (c != 0);
}

void
cfread_string_len(char *buf, int n, CFILE *file)
{
    int len;
    len = cfread_int(file);
    Assert(len < n);
    if (len)
        cfread(buf, len, 1, file);

    buf[len] = 0;
}

// equivalent write functions of above read functions follow

int
cfwrite_float(float f, CFILE *file)
{
    //   i = INTEL_INT(i);       //  hmm, not sure what to do here
    return cfwrite(&f, sizeof(f), 1, file);
}

int
cfwrite_int(int i, CFILE *file)
{
    i = INTEL_INT(i);
    return cfwrite(&i, sizeof(i), 1, file);
}

int
cfwrite_uint(uint i, CFILE *file)
{
    i = INTEL_INT(i);
    return cfwrite(&i, sizeof(i), 1, file);
}

int
cfwrite_short(short s, CFILE *file)
{
    s = INTEL_SHORT(s);
    return cfwrite(&s, sizeof(s), 1, file);
}

int
cfwrite_ushort(ushort s, CFILE *file)
{
    s = INTEL_SHORT(s);
    return cfwrite(&s, sizeof(s), 1, file);
}

int
cfwrite_ubyte(ubyte b, CFILE *file)
{
    return cfwrite(&b, sizeof(b), 1, file);
}

int
cfwrite_vector(vector *vec, CFILE *file)
{
    if (!cfwrite_float(vec->x, file)) {
        return 0;
    }
    if (!cfwrite_float(vec->y, file)) {
        return 0;
    }
    return cfwrite_float(vec->z, file);
}

int
cfwrite_angles(angles *ang, CFILE *file)
{
    if (!cfwrite_float(ang->p, file)) {
        return 0;
    }
    if (!cfwrite_float(ang->b, file)) {
        return 0;
    }
    return cfwrite_float(ang->h, file);
}

int
cfwrite_char(char b, CFILE *file)
{
    return cfwrite(&b, sizeof(b), 1, file);
}

int
cfwrite_string(char *buf, CFILE *file)
{
    if ((!buf) || (buf && !buf[0])) {
        return cfwrite_char(0, file);
    }
    int len = strlen(buf);
    if (!cfwrite(buf, len, 1, file)) {
        return 0;
    }
    return cfwrite_char(0, file); // write out NULL termination
}

int
cfwrite_string_len(char *buf, CFILE *file)
{
    int len = strlen(buf);

    if (!cfwrite_int(len, file)) {
        return 0;
    }
    if (len) {
        return cfwrite(buf, len, 1, file);
    }

    return 1;
}

// Get the filelength
int
cfilelength(CFILE *cfile)
{
    Assert(cfile != NULL);
    Cfile_block *cb;
    Assert(cfile->id >= 0 && cfile->id < MAX_CFILE_BLOCKS);
    cb = &Cfile_block_list[cfile->id];

    // TODO: return length of memory mapped file
    Assert(!cb->data);

    Assert(cb->fp != NULL);

    // cb->size gets set at cfopen
    return cb->size;
}

// cfwrite() writes to the file
//
// returns:   number of full elements actually written
//
//
int
cfwrite(void *buf, int elsize, int nelem, CFILE *cfile)
{
    Assert(cfile != NULL);
    Assert(buf != NULL);
    Assert(elsize > 0);
    Assert(nelem > 0);

    Cfile_block *cb;
    Assert(cfile->id >= 0 && cfile->id < MAX_CFILE_BLOCKS);
    cb = &Cfile_block_list[cfile->id];

    int result = 0;

    // cfwrite() not supported for memory-mapped files
    Assert(!cb->data);

    Assert(cb->fp != NULL);
    Assert(cb->lib_offset == 0);
    result = fwrite(buf, elsize, nelem, cb->fp);

    return result;
}

// cfputc() writes a character to a file
//
// returns:   success ==> returns character written
//            error   ==> EOF
//
int
cfputc(int c, CFILE *cfile)
{
    int result;

    Assert(cfile != NULL);
    Cfile_block *cb;
    Assert(cfile->id >= 0 && cfile->id < MAX_CFILE_BLOCKS);
    cb = &Cfile_block_list[cfile->id];

    result = 0;
    // cfputc() not supported for memory-mapped files
    Assert(!cb->data);

    Assert(cb->fp != NULL);
    result = fputc(c, cb->fp);

    return result;
}

// cfgetc() reads a character from a file
//
// returns:   success ==> returns character read
//            error   ==> EOF
//
int
cfgetc(CFILE *cfile)
{
    Assert(cfile != NULL);

    char tmp;

    int result = cfread(&tmp, 1, 1, cfile);
    if (result == 1) {
        result = char(tmp);
    }
    else {
        result = CF_EOF;
    }

    return result;
}

// cfgets() reads a string from a file
//
// returns:   success ==> returns pointer to string
//            error   ==> NULL
//
char *
cfgets(char *buf, int n, CFILE *cfile)
{
    Assert(cfile != NULL);
    Assert(buf != NULL);
    Assert(n > 0);

    char *t = buf;
    int i, c;

    for (i = 0; i < n - 1; i++) {
        do {
            char tmp_c;

            int ret = cfread(&tmp_c, 1, 1, cfile);
            if (ret != 1) {
                *buf = 0;
                if (buf > t) {
                    return t;
                }
                else {
                    return NULL;
                }
            }
            c = int(tmp_c);
        } while (c == 13);
        *buf++ = char(c);
        if (c == '\n')
            break;
    }
    *buf++ = 0;

    return t;
}

// cfputs() writes a string to a file
//
// returns:   success ==> non-negative value
//            error   ==> EOF
//
int
cfputs(char *str, CFILE *cfile)
{
    Assert(cfile != NULL);
    Assert(str != NULL);

    Cfile_block *cb;
    Assert(cfile->id >= 0 && cfile->id < MAX_CFILE_BLOCKS);
    cb = &Cfile_block_list[cfile->id];

    int result;

    result = 0;
    // cfputs() not supported for memory-mapped files
    Assert(!cb->data);
    Assert(cb->fp != NULL);
    result = fputs(str, cb->fp);

    return result;
}

// 16 and 32 bit checksum stuff ----------------------------------------------------------

// CRC code for mission validation.  given to us by Kevin Bentley on 7/20/98.   Some sort of
// checksumming code that he wrote a while ago.
#define CRC32_POLYNOMIAL 0xEDB88320L
unsigned long CRCTable[256];

#define CF_CHKSUM_SAMPLE_SIZE 512

// update cur_chksum with the chksum of the new_data of size new_data_size
ushort
cf_add_chksum_short(ushort seed, char *buffer, int size)
{
    ubyte *ptr = (ubyte *)buffer;
    unsigned int sum1, sum2;

    sum1 = sum2 = (int)(seed);

    while (size--) {
        sum1 += *ptr++;
        if (sum1 >= 255)
            sum1 -= 255;
        sum2 += sum1;
    }
    sum2 %= 255;

    return (unsigned short)((sum1 << 8) + sum2);
}

// update cur_chksum with the chksum of the new_data of size new_data_size
unsigned long
cf_add_chksum_long(unsigned long seed, char *buffer, int size)
{
    unsigned long crc;
    unsigned char *p;
    unsigned long temp1;
    unsigned long temp2;

    p = (unsigned char *)buffer;
    crc = seed;

    while (size-- != 0) {
        temp1 = (crc >> 8) & 0x00FFFFFFL;
        temp2 = CRCTable[((int)crc ^ *p++) & 0xff];
        crc = temp1 ^ temp2;
    }

    return crc;
}

void
cf_chksum_long_init()
{
    int i, j;
    unsigned long crc;

    for (i = 0; i <= 255; i++) {
        crc = i;
        for (j = 8; j > 0; j--) {
            if (crc & 1)
                crc = (crc >> 1) ^ CRC32_POLYNOMIAL;
            else
                crc >>= 1;
        }
        CRCTable[i] = crc;
    }
}

// single function convenient to use for both short and long checksums
// NOTE : only one of chk_short or chk_long must be non-NULL (indicating which checksum to perform)
int
cf_chksum_do(CFILE *cfile, ushort *chk_short, uint *chk_long, int max_size)
{
    char cf_buffer[CF_CHKSUM_SAMPLE_SIZE];
    int is_long;
    int cf_len = 0;
    int cf_total;
    int read_size;

    // determine whether we're doing a short or long checksum
    is_long = 0;
    if (chk_short) {
        Assert(!chk_long);
        *chk_short = 0;
    }
    else {
        Assert(chk_long);
        is_long = 1;
        *chk_long = 0;
    }

    // if max_size is -1, set it to be the size of the file
    if (max_size < 0) {
        cfseek(cfile, 0, SEEK_SET);
        max_size = cfilelength(cfile);
    }

    cf_total = 0;
    do {
        // determine how much we want to read
        if ((max_size - cf_total) >= CF_CHKSUM_SAMPLE_SIZE) {
            read_size = CF_CHKSUM_SAMPLE_SIZE;
        }
        else {
            read_size = max_size - cf_total;
        }

        // read in some buffer
        cf_len = cfread(cf_buffer, 1, read_size, cfile);

        // total we've read so far
        cf_total += cf_len;

        // add the checksum
        if (cf_len > 0) {
            // do the proper short or long checksum
            if (is_long) {
                *chk_long = cf_add_chksum_long(*chk_long, cf_buffer, cf_len);
            }
            else {
                *chk_short = cf_add_chksum_short(*chk_short, cf_buffer, cf_len);
            }
        }
    } while ((cf_len > 0) && (cf_total < max_size));

    return 1;
}

// get the 2 byte checksum of the passed filename - return 0 if operation failed, 1 if succeeded
int
cf_chksum_short(char *filename, ushort *chksum, int max_size, int cf_type)
{
    int ret_val;
    CFILE *cfile = NULL;

    // zero the checksum
    *chksum = 0;

    // attempt to open the file
    cfile = cfopen(filename, "rt", CFILE_NORMAL, cf_type);
    if (cfile == NULL) {
        return 0;
    }

    // call the overloaded cf_chksum function()
    ret_val = cf_chksum_do(cfile, chksum, NULL, max_size);

    // close the file down
    cfclose(cfile);
    cfile = NULL;

    // return the result
    return ret_val;
}

// get the 2 byte checksum of the passed file - return 0 if operation failed, 1 if succeeded
// NOTE : preserves current file position
int
cf_chksum_short(CFILE *file, ushort *chksum, int max_size)
{
    int ret_code;
    int start_pos;

    // Returns current position of file.
    start_pos = cftell(file);
    if (start_pos == -1) {
        return 0;
    }

    // move to the beginning of the file
    if (cfseek(file, 0, CF_SEEK_SET)) {
        return 0;
    }
    ret_code = cf_chksum_do(file, chksum, NULL, max_size);
    // move back to the start position
    cfseek(file, start_pos, CF_SEEK_SET);

    return ret_code;
}

// get the 32 bit CRC checksum of the passed filename - return 0 if operation failed, 1 if succeeded
int
cf_chksum_long(char *filename, uint *chksum, int max_size, int cf_type)
{
    int ret_val;
    CFILE *cfile = NULL;

    // zero the checksum
    *chksum = 0;

    // attempt to open the file
    cfile = cfopen(filename, "rt", CFILE_NORMAL, cf_type);
    if (cfile == NULL) {
        return 0;
    }

    // call the overloaded cf_chksum function()
    ret_val = cf_chksum_do(cfile, NULL, chksum, max_size);

    // close the file down
    cfclose(cfile);
    cfile = NULL;

    // return the result
    return ret_val;
}

// get the 32 bit CRC checksum of the passed file - return 0 if operation failed, 1 if succeeded
// NOTE : preserves current file position
int
cf_chksum_long(CFILE *file, uint *chksum, int max_size)
{
    int ret_code;
    int start_pos;

    // Returns current position of file.
    start_pos = cftell(file);
    if (start_pos == -1) {
        return 0;
    }

    // move to the beginning of the file
    if (cfseek(file, 0, CF_SEEK_SET)) {
        return 0;
    }
    ret_code = cf_chksum_do(file, NULL, chksum, max_size);
    // move back to the start position
    cfseek(file, start_pos, CF_SEEK_SET);

    return ret_code;
}

// Flush the open file buffer
//
// exit: 0 - success
//       1 - failure
int
cflush(CFILE *cfile)
{
    Assert(cfile != NULL);
    Cfile_block *cb;
    Assert(cfile->id >= 0 && cfile->id < MAX_CFILE_BLOCKS);
    cb = &Cfile_block_list[cfile->id];

    // not supported for memory mapped files
    Assert(!cb->data);

    Assert(cb->fp != NULL);
    return fflush(cb->fp);
}
