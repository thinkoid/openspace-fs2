// Point-1/2 oracle: enumerate the mission and campaign files cfile can see
// under a game root (VP archives + loose data tree), parse every sexp tree
// with the retail parser, and dump the aggregate operator vocabulary, one
// operator per line, sorted, to stdout.
//
//   sexp_dump <game-root>
//
// Per-file tree counts go to stderr.  The output must be identical whether
// the missions come from the pristine VPs (gog/) or a loose data tree, and
// must match missions/sexp-used.txt (+ has-time-elapsed, which the 2018
// inventory missed).

#include <stdio.h>
#include <string.h>

#include <set>
#include <string>

#include "pstypes.h"
#include "cfile.h"
#include "parselo.h"
#include "sexp.h"

// Tags whose value is a sexp in retail mission (.fs2) and campaign (.fc2) files
static const char *Sexp_tags[] = {
	"$Formula:", "+Formula:", "$Arrival Cue:", "$Departure Cue:", "$AI Goals:",
};

static std::set<std::string> used_operators;

// --trees: emit each parsed tree as a deterministic pre-order node dump
// (self, then first-subtree deeper, then rest-sibling at the same level).
// Each line records the node's type, subtype, flag bits, and text -- the full
// reader output, so two runs diff byte-for-byte iff the trees are identical.
static bool dump_trees = false;

static void emit_tree(int n, int depth)
{
	if (n < 0)
		return;
	printf("%*s[%d,%d,%08x] %s\n", depth * 2, "",
		SEXP_NODE_TYPE(n), Sexp_nodes[n].subtype,
		(unsigned)(Sexp_nodes[n].type & 0xffff0000), Sexp_nodes[n].text);
	emit_tree(Sexp_nodes[n].first, depth + 1);
	emit_tree(Sexp_nodes[n].rest, depth);
}

static void collect_operators(int n)
{
	if (n < 0)
		return;
	if (SEXP_NODE_TYPE(n) == SEXP_ATOM && Sexp_nodes[n].subtype == SEXP_ATOM_OPERATOR)
		used_operators.insert(Sexp_nodes[n].text);
	collect_operators(Sexp_nodes[n].first);
	collect_operators(Sexp_nodes[n].rest);
}

static int dump_file(char *filename)
{
	read_file_text(filename, CF_TYPE_MISSIONS);
	reset_parse();
	init_sexp();

	// load the mission's sexp variables first, as missionparse does, so
	// @variable atoms in the trees resolve
	char *vars = strstr(Mission_text, "#Sexp_variables");
	if (vars != NULL) {
		Mp = vars + strlen("#Sexp_variables");
		stuff_sexp_variable_list();
	}

	int trees = 0;
	char *scan = Mission_text;
	for (;;) {
		// find the nearest upcoming sexp-introducing tag
		char *next = NULL;
		size_t next_tag_len = 0;
		for (auto tag : Sexp_tags) {
			char *hit = strstr(scan, tag);
			if (hit && (next == NULL || hit < next)) {
				next = hit;
				next_tag_len = strlen(tag);
			}
		}
		if (next == NULL)
			break;

		Mp = next + next_tag_len;
		ignore_white_space();
		if (*Mp == '(') {
			int root = get_sexp_main();
			if (root >= 0) {
				collect_operators(root);
				if (dump_trees) {
					printf("# %s tree %d\n", filename, trees);
					emit_tree(root, 0);
				}
				free_sexp2(root);	// one tree at a time keeps the 2200-node pool ample
				trees++;
			}
		}
		scan = Mp > next ? Mp : next + next_tag_len;
	}

	fprintf(stderr, "%s: %d sexp trees\n", filename, trees);
	return trees;
}

#define MAX_ORACLE_MISSIONS 512

static char mission_arr[MAX_ORACLE_MISSIONS][MAX_FILENAME_LEN];
static char *mission_list[MAX_ORACLE_MISSIONS];

int main(int argc, char *argv[])
{
	const char *game_root = NULL;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--trees"))
			dump_trees = true;
		else
			game_root = argv[i];
	}
	if (game_root == NULL) {
		fprintf(stderr, "usage: sexp_dump [--trees] <game-root>\n");
		return 2;
	}

	char exe_path[CF_MAX_PATHNAME_LENGTH];
	snprintf(exe_path, sizeof(exe_path), "%s/x", game_root);
	if (cfile_init(exe_path)) {
		fprintf(stderr, "cfile_init failed for %s\n", argv[1]);
		return 1;
	}

	int total = 0;
	for (auto pattern : { "*.fs2", "*.fc2" }) {
		int n = cf_get_file_list_preallocated(MAX_ORACLE_MISSIONS, mission_arr,
			mission_list, CF_TYPE_MISSIONS, (char *)pattern, CF_SORT_NAME);
		for (int i = 0; i < n; i++) {
			// listings come back without extensions; read_file_text wants them
			char name[MAX_FILENAME_LEN + 8];
			snprintf(name, sizeof(name), "%s%s", mission_list[i], pattern + 1);
			total += dump_file(name);
		}
	}

	if (!dump_trees)
		for (const std::string &op : used_operators)
			printf("%s\n", op.c_str());

	return total > 0 ? 0 : 1;
}
