// main.cpp -- SEXP parse oracle.
//
// Drives the standalone reader (sexp_reader.*) over retail mission files: for
// each `$Formula:` / `...Cue:` token it seeks to the following '(' and parses
// one SEXP, mirroring how missionparse.cpp invokes get_sexp_main(). Reports
// per-mission and aggregate stats; --dump round-trips each tree back to text.
//
// The point: this binary links NO game object files (grep the Makefile). If it
// parses the whole retail campaign clean, the reader carve seam is proven.
#include "sexp_reader.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static char *slurp(const char *path, long *len_out)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc(n + 1);
    long got = (long)fread(buf, 1, n, f);
    fclose(f);
    buf[got] = '\0';
    if (len_out) *len_out = got;
    return buf;
}

static char *earliest(char *a, char *b)
{
    if (!a) return b;
    if (!b) return a;
    return a < b ? a : b;
}

struct stats { int sexps, nodes, maxdepth, fails; };

static stats parse_mission(char *buf, bool dump, const char *name)
{
    stats s = {0, 0, 0, 0};
    init_sexp();

    char *scan = buf;
    while (*scan) {
        char *cue = earliest(strstr(scan, "Formula:"), strstr(scan, "Cue:"));
        if (!cue) break;
        char *paren = strchr(cue, '(');
        if (!paren) break;

        Mp = paren;
        int root = get_sexp_main();
        if (root < 0) {
            s.fails++;
            scan = paren + 1;
            continue;
        }
        s.sexps++;
        int d = sexp_tree_depth(root);
        if (d > s.maxdepth) s.maxdepth = d;

        if (dump) {
            char out[8192];
            out[0] = '\0';
            sexp_round_trip(root, out, sizeof(out));
            printf("  %-20s #%d  depth=%d  %s\n", name, s.sexps, d, out);
        }
        scan = Mp;
    }

    s.nodes = sexp_nodes_used();   // per-mission peak (pool reset each mission)
    return s;
}

int main(int argc, char **argv)
{
    bool dump = false;
    stats total = {0, 0, 0, 0};
    int files = 0, maxnodes = 0;

    printf("%-22s %6s %6s %6s %6s\n", "mission", "sexps", "nodes", "depth", "fail");
    printf("%-22s %6s %6s %6s %6s\n", "----------------------", "-----", "-----", "-----", "----");

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--dump")) { dump = true; continue; }

        long len = 0;
        char *buf = slurp(argv[i], &len);
        if (!buf) { fprintf(stderr, "cannot open %s\n", argv[i]); continue; }

        const char *base = strrchr(argv[i], '/');
        base = base ? base + 1 : argv[i];

        stats s = parse_mission(buf, dump, base);
        free(buf);
        files++;

        printf("%-22s %6d %6d %6d %6d\n", base, s.sexps, s.nodes, s.maxdepth, s.fails);
        total.sexps += s.sexps;
        total.fails += s.fails;
        if (s.maxdepth > total.maxdepth) total.maxdepth = s.maxdepth;
        if (s.nodes > maxnodes) maxnodes = s.nodes;
    }

    printf("%-22s %6s %6s %6s %6s\n", "----------------------", "-----", "-----", "-----", "----");
    printf("%-22s %6d %6s %6d %6d\n", "TOTAL", total.sexps, "", total.maxdepth, total.fails);
    printf("\n%d missions, %d SEXPs parsed, %d parse failures.\n", files, total.sexps, total.fails);
    printf("peak nodes in a single mission: %d / %d pool (%.0f%%).\n",
           maxnodes, MAX_SEXP_NODES, 100.0 * maxnodes / MAX_SEXP_NODES);
    return total.fails ? 1 : 0;
}
