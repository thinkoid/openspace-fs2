// -*- mode: c++; -*-

#include <defs.hh>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <shared/types.hh>
#include <cfile/cfile.hh>

static int data_error;
static int no_dir;

unsigned int Total_size = 16; // Start with size of header
unsigned int Num_files = 0;

FILE* fp_out = NULL;
FILE* fp_out_hdr = NULL;

typedef struct vp_header {
    char id[4];
    int version;
    int index_offset;
    int num_files;
} vp_header;

// vp_header Vp_header;

char archive_dat[1024];
char archive_hdr[1024];

#define BLOCK_SIZE (1024 * 1024)
#define VERSION_NUMBER 2;

char tmp_data[BLOCK_SIZE]; // 1 MB

void write_header () {
    int ver = VERSION_NUMBER;

    fseek (fp_out, 0, SEEK_SET);
    fwrite ("VPVP", 1, 4, fp_out);
    fswrite_int (&ver, fp_out);
    fswrite_int ((int*)&Total_size, fp_out);
    fswrite_int ((int*)&Num_files, fp_out);
}

int write_index (char* hf, char* df) {
    FILE *h = NULL, *d = NULL;
    unsigned int i;

    h = fopen (hf, "rb");
    d = fopen (df, "a+b");

    if ((h == NULL) || (d == NULL)) {
        if (h) fclose (h);
        if (d) fclose (d);
        return 0;
    }

    for (i = 0; i < Num_files; i++) {
        fread (tmp_data, 32 + 4 + 4 + 4, 1, h);
        fwrite (tmp_data, 32 + 4 + 4 + 4, 1, d);
    }

    fclose (h);
    fclose (d);

    unlink (hf);

    return 1;
}

void pack_file (
    char* filespec, char* filename, int filesize, _fs_time_t time_write) {
    char path[1024];

    if (strstr (filename, ".vp")) {
        // Don't pack yourself!!
        return;
    }

    if (strstr (filename, ".hdr")) {
        // Don't pack yourself!!
        return;
    }

    if (filesize == 0) {
        // Don't pack 0 length files, screws up directory structure!
        return;
    }

    if (strlen (filename) > 31) {
        printf ("Filename '%s' too long!  Skipping...\n", filename);
        return;
    }

    memset (path, 0, sizeof (path));
    strcpy (path, filename);

    fswrite_int ((int*)&Total_size, fp_out_hdr);
    fswrite_int (&filesize, fp_out_hdr);
    fwrite (&path, 1, 32, fp_out_hdr);
    fswrite_int ((int*)&time_write, fp_out_hdr);

    Total_size += filesize;
    Num_files++;

    printf ("Packing %s%s%s...", filespec, "/", filename);

    sprintf (path, "%s%s%s", filespec, "/", filename);

    FILE* fp = fopen (path, "rb");

    if (fp == NULL) {
        printf ("Error opening '%s'\n", path);
        exit (1);
    }

    int nbytes, nbytes_read = 0;

    do {
        nbytes = fread (tmp_data, 1, BLOCK_SIZE, fp);
        if (nbytes > 0) {
            fwrite (tmp_data, 1, nbytes, fp_out);
            nbytes_read += nbytes;
        }
    } while (nbytes > 0);

    fclose (fp);

    printf (" %d bytes\n", nbytes_read);
}

// This function adds a directory marker to the header file
void add_directory (const char* dirname) {
    char path[256];
    char* pathptr = path;
    char* tmpptr;
    int i = 0;

    strcpy (path, dirname);

    fswrite_int ((int*)&Total_size, fp_out_hdr);
    fswrite_int (&i, fp_out_hdr);

    // strip out any directories that this dir is a subdir of
    while ((tmpptr = strchr (pathptr, '/')) != NULL) {
        pathptr = tmpptr + 1;
    }

    fwrite (pathptr, 1, 32, fp_out_hdr);
    fswrite_int (&i, fp_out_hdr); // timestamp = 0

    Num_files++;
}

void pack_directory (char* filespec) {
    char tmp[512];
    char tmp1[512];
    char* ts;

    // strip trailing slash
    ts = filespec + (strlen (filespec) - 1);
    while ((*ts == '/') && (ts > filespec)) *ts = '\0';

    strcpy (tmp1, filespec);

    add_directory (filespec);
    strcat (tmp1, "/*.*");

    printf ("In dir '%s'\n", tmp1);

    DIR* dirp;
    struct dirent* dir;

    dirp = opendir (filespec);
    if (dirp) {
        while ((dir = readdir (dirp)) != NULL) {
            char fn[PATH_MAX];
            snprintf (fn, PATH_MAX - 1, "%s/%s", filespec, dir->d_name);
            fn[PATH_MAX - 1] = 0;

            struct stat buf;
            if (stat (fn, &buf) == -1) { continue; }

            if ((strcmp (dir->d_name, ".") == 0) ||
                (strcmp (dir->d_name, "..") == 0)) {
                continue;
            }

            if ((strcmp (dir->d_name, ".svn") == 0)) { continue; }

            if (S_ISDIR (buf.st_mode)) {
                strcpy (tmp, filespec);
                strcat (tmp, "/");
                strcat (tmp, dir->d_name);
                pack_directory (tmp);
            }
            else {
                pack_file (filespec, dir->d_name, buf.st_size, buf.st_mtime);
            }
        }
        closedir (dirp);
    }
    else {
        printf ("Error: Source directory does not exist!\n");
        no_dir = 1;
    }
    add_directory ("..");
}

int verify_directory (char* filespec) {
    char* ts;
    char* dd;

    // strip trailing '/'
    ts = filespec + (strlen (filespec) - 1);
    while ((*ts == '/') && (ts > filespec)) *ts = '\0';

    // make sure last directory is named "data", ignoring case
    dd = filespec + (strlen (filespec) - 4);
    if (strcasecmp (dd, "data")) data_error = 1;

    return data_error;
}

void print_instructions () {
    printf ("Creates a vp archive out of a FreeSpace data tree.\n\n");
    printf ("Usage:     cfilearchiver archive_name src_dir\n");
    printf ("Example:   cfilearchiver freespace /tmp/freespace/data\n\n");
    printf (
        "Creates an archive named freespace out of the freespace data tree\n");
    printf (
        "For information about the FS2 directory structure, please consult\n");
    printf ("http://www.hard-light.net/wiki/index.php/FS2_Data_Structure\n");
    exit (0);
}

// we end up #include'ing SDL.h which on Windows and Mac will redefine main()
// which is something that we don't want since we don't actually link against
// SDL, this solves the problem...
#ifdef main
#undef main
#endif

int main (int argc, char* argv[]) {
    char archive[1024];
    char* p;

    if (argc < 3) { print_instructions (); }

    strcpy (archive, argv[1]);
    p = strchr (archive, '.');
    if (p) *p = 0; // remove extension

    strcpy (archive_dat, archive);
    strcat (archive_dat, ".vp");

    strcpy (archive_hdr, archive);
    strcat (archive_hdr, ".hdr");

    fp_out = fopen (archive_dat, "wb");
    if (!fp_out) {
        printf ("Couldn't open '%s'!\n", archive_dat);
        exit (1);
    }

    fp_out_hdr = fopen (archive_hdr, "wb");
    if (!fp_out_hdr) {
        printf ("Couldn't open '%s'!\n", archive_hdr);
        exit (2);
    }

    if (verify_directory (argv[2]) != 0) {
        printf (
            "Warning! Last directory must be named \"data\" (not case "
            "sensitive)\n");
        exit (3);
    }

    write_header ();

    pack_directory (argv[2]);

    // in case the directory doesn't exist
    if (no_dir) exit (4);

    write_header ();

    fclose (fp_out);
    fclose (fp_out_hdr);

    printf ("Data files written, appending index...\n");

    if (!write_index (archive_hdr, archive_dat)) {
        printf ("Error appending index!\n");
        return 1;
    }

    printf ("%d total KB.\n", Total_size / 1024);
    return 0;
}
