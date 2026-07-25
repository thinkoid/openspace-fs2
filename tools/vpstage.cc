// -*- mode: c++; -*-
//
// vpstage: stage the conversion pipeline's inputs out of the retail VP
// archives (docs/godot-migration-plan.md, "The asset pipeline"). The
// extraction goes through retail's own cfile -- the authoritative VP reader,
// the same one the game plays from -- so precedence between archives and the
// member actually staged are retail's answers, not a reimplementation's.
//
//   vpstage <game-root> <out-dir>
//
// Stages every model (*.pof) and map (*.pcx) into <out-dir>/data/{models,maps}
// and writes <out-dir>/staging.manifest.json recording, per file: staged
// path, size, SHA-256, the originating archive and the member's offset in it
// -- the "source archive" slot the conversion manifest deferred. Archives are
// digested once each. Entries are sorted by path, and nothing is stamped with
// a time: same VPs, same manifest, byte for byte.

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include <globalincs/pstypes.hh>
#include <cfile/cfile.hh>

#include "pof2glb_version.hh"
#include "sha256.hh"

#define MAX_STAGE_FILES 8192

static char file_arr[MAX_STAGE_FILES][MAX_FILENAME_LEN];
static char *file_list[MAX_STAGE_FILES];

static std::vector< std::string > full_names;

static int
capture_name(char *name_ext)
{
    full_names.push_back(name_ext);
    return 1;
}

// JSON string escape for the paths that land in the manifest
static std::string
jstr(const std::string &s)
{
    std::string out;
    for (char c : s) {
        if (c == '"' || c == '\\')
            out += '\\';
        out += c;
    }
    return out;
}

struct staged_t
{
    std::string path;      // manifest-relative, "data/models/x.pof"
    int         size;
    std::string sha256;
    int         archive;   // index into the archives list, -1 if loose
    int         offset;    // member offset in the archive, 0 if loose
};

static bool
ends_with_ci(const std::string &s, const char *suffix)
{
    const std::size_t n = strlen(suffix);
    if (s.size() < n)
        return false;
    for (std::size_t i = 0; i < n; ++i)
        if (tolower((unsigned char)s[s.size() - n + i]) != suffix[i])
            return false;
    return true;
}

int
main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "usage: vpstage <game-root> <out-dir>\n");
        return 2;
    }
    const std::string out = argv[2];

    // cfile_init derives the root by truncating at the last separator
    char exe_path[CF_MAX_PATHNAME_LENGTH];
    snprintf(exe_path, sizeof(exe_path), "%s/x", argv[1]);
    if (cfile_init(exe_path)) {
        fprintf(stderr, "vpstage: cfile_init failed for %s\n", argv[1]);
        return 1;
    }

    mkdir(out.c_str(), 0755);                      // EEXIST is fine
    mkdir((out + "/data").c_str(), 0755);

    std::vector< std::string >   archives;         // pack paths, first-seen order
    std::map< std::string, int > archive_ix;
    std::vector< staged_t >      staged;

    struct family_t { int type; const char *subdir; const char *ext; };
    const family_t families[] = {
        { CF_TYPE_MODELS, "models", ".pof" },
        { CF_TYPE_MAPS,   "maps",   ".pcx" },
    };

    int failed = 0;
    for (const family_t &fam : families) {
        mkdir((out + "/data/" + fam.subdir).c_str(), 0755);

        full_names.clear();
        Get_file_list_filter = capture_name;
        cf_get_file_list_preallocated(MAX_STAGE_FILES, file_arr, file_list,
                                      fam.type, (char *)"*", CF_SORT_NONE);

        for (const std::string &name : full_names) {
            if (!ends_with_ci(name, fam.ext))
                continue;

            CFILE *f = cfopen((char *)name.c_str(), (char *)"rb",
                              CFILE_NORMAL, fam.type);
            if (!f) {
                fprintf(stderr, "vpstage: cannot open %s\n", name.c_str());
                ++failed;
                continue;
            }
            const int sz = cfilelength(f);
            std::vector< std::uint8_t > buf(sz > 0 ? sz : 0);
            if (sz > 0 && cfread(buf.data(), 1, sz, f) != sz) {
                fprintf(stderr, "vpstage: short read on %s\n", name.c_str());
                cfclose(f);
                ++failed;
                continue;
            }
            cfclose(f);

            // Staged names are lowercased: the VP TOCs carry mixed case
            // ("ast01.POF") that retail's case-insensitive cfile never
            // notices, but a case-sensitive pipeline downstream would. One
            // canonical case also keeps the manifest diffable across VPs
            // that only disagree in case.
            std::string lower = name;
            for (char &c : lower)
                c = tolower((unsigned char)c);

            const std::string rel =
                std::string("data/") + fam.subdir + "/" + lower;
            std::FILE *o = std::fopen((out + "/" + rel).c_str(), "wb");
            if (!o || (sz > 0 &&
                       std::fwrite(buf.data(), 1, sz, o) != (size_t)sz)) {
                fprintf(stderr, "vpstage: cannot write %s\n", rel.c_str());
                if (o)
                    std::fclose(o);
                ++failed;
                continue;
            }
            std::fclose(o);

            // origin: which pack won the retail precedence, and where in it
            char pack[MAX_PATH_LEN];
            int  loc_size = 0, loc_offset = 0;
            int  archive = -1;
            if (cf_find_file_location((char *)name.c_str(), fam.type, pack,
                                      &loc_size, &loc_offset) &&
                loc_offset > 0) {
                const std::string p = pack;
                if (auto it = archive_ix.find(p); it != archive_ix.end())
                    archive = it->second;
                else {
                    archive = (int)archives.size();
                    archive_ix[p] = archive;
                    archives.push_back(p);
                }
            }

            staged.push_back({ rel, sz, sha::sha256_hex(buf.data(), buf.size()),
                               archive, loc_offset });
        }
    }

    std::sort(staged.begin(), staged.end(),
              [](const staged_t &a, const staged_t &b) {
                  return a.path < b.path;
              });

    std::string j = "{\"tool\":\"vpstage\",\"version\":\"" POF2GLB_VERSION
        "\",\"archives\":[";
    for (const std::string &a : archives) {
        j += "{\"path\":\"" + jstr(a) + "\",\"sha256\":\"" +
            sha::sha256_file(a) + "\"},";
    }
    if (j.back() == ',')
        j.pop_back();
    j += "],\"files\":[";
    for (const staged_t &s : staged) {
        char buf[128];
        snprintf(buf, sizeof buf, "\",\"size\":%d,\"sha256\":\"", s.size);
        j += "{\"path\":\"" + jstr(s.path) + buf + s.sha256 + "\"";
        snprintf(buf, sizeof buf, ",\"archive\":%d,\"offset\":%d},",
                 s.archive, s.offset);
        j += buf;
    }
    if (j.back() == ',')
        j.pop_back();
    j += "]}\n";

    const std::string mpath = out + "/staging.manifest.json";
    std::FILE *m = std::fopen(mpath.c_str(), "wb");
    if (!m || std::fwrite(j.data(), 1, j.size(), m) != j.size()) {
        fprintf(stderr, "vpstage: cannot write %s\n", mpath.c_str());
        if (m)
            std::fclose(m);
        return 1;
    }
    std::fclose(m);

    printf("vpstage: %zu files staged from %zu archives -> %s%s\n",
           staged.size(), archives.size(), out.c_str(),
           failed ? " (WITH FAILURES)" : "");
    return failed ? 1 : 0;
}
