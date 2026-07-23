// Point-2 oracle: enumerate every file cfile can see under a game root
// (VP archives + loose data tree), read each end-to-end through the cfile
// API, and print "type-index<TAB>name<TAB>size<TAB>crc32" lines.
//
//   vp_ls <game-root>
//
// Compared between the pristine VP root (gog/) and any unpacked tree, and
// across code changes.
//
// Listings return extensionless names, so full names are captured through
// the retail Get_file_list_filter hook.  The filter spec must be "*": retail
// cf_matches_spec treats "*.*" as extension ".*" and rejects everything.

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include <globalincs/pstypes.hh>
#include <cfile/cfile.hh>

#define MAX_ORACLE_FILES 8192

static char file_arr[MAX_ORACLE_FILES][MAX_FILENAME_LEN];
static char *file_list[MAX_ORACLE_FILES];

static std::vector< std::string > full_names;

static int
capture_name(char *name_ext)
{
    full_names.push_back(name_ext);
    return 1;
}

int
main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "usage: vp_ls <game-root>\n");
        return 2;
    }

    // cfile_init derives the root by truncating at the last separator
    char exe_path[CF_MAX_PATHNAME_LENGTH];
    snprintf(exe_path, sizeof(exe_path), "%s/x", argv[1]);
    if (cfile_init(exe_path)) {
        fprintf(stderr, "cfile_init failed for %s\n", argv[1]);
        return 1;
    }

    int total = 0, failed = 0;
    for (int type = CF_TYPE_ROOT; type < CF_MAX_PATH_TYPES; type++) {
        full_names.clear();
        Get_file_list_filter = capture_name;
        cf_get_file_list_preallocated(MAX_ORACLE_FILES, file_arr, file_list, type,
                                      (char *)"*", CF_SORT_NONE);
        for (const std::string &name : full_names) {
            CFILE *f = cfopen((char *)name.c_str(), (char *)"rb", CFILE_NORMAL,
                              type);
            if (f == NULL) {
                // listing found it but open failed: report and count
                printf("%d\t%s\tOPEN-FAILED\t-\n", type, name.c_str());
                failed++;
                continue;
            }
            int size = cfilelength(f);
            uint crc = 0;
            cf_chksum_long(f, &crc);
            cfclose(f);
            printf("%d\t%s\t%d\t%08x\n", type, name.c_str(), size, crc);
            total++;
        }
    }
    fprintf(stderr, "vp_ls: %d files read, %d failures\n", total, failed);
    return failed ? 1 : 0;
}
