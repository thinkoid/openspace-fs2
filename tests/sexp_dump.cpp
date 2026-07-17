// Point-1 oracle: parse the sexps out of retail mission/campaign files and
// dump the operator vocabulary they use, one operator per line, sorted.
//
//   sexp_dump file.fs2 [more files...]
//
// Per-file tree counts go to stderr; the aggregate operator list to stdout.
// Compared against missions/sexp-used.txt (the 2018 inventory).

#include <stdio.h>
#include <string.h>

#include <set>
#include <string>

#include "pstypes.h"
#include "parselo.h"
#include "sexp.h"

// Tags whose value is a sexp in retail mission (.fs2) and campaign (.fc2) files
static const char *Sexp_tags[] = {
	"$Formula:", "+Formula:", "$Arrival Cue:", "$Departure Cue:", "$AI Goals:",
};

static std::set<std::string> used_operators;

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
	read_file_text(filename);
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
				free_sexp2(root);	// one tree at a time keeps the 2200-node pool ample
				trees++;
			}
		}
		scan = Mp > next ? Mp : next + next_tag_len;
	}

	fprintf(stderr, "%s: %d sexp trees\n", filename, trees);
	return trees;
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "usage: sexp_dump <mission files>\n");
		return 2;
	}

	int total = 0;
	for (int i = 1; i < argc; i++)
		total += dump_file(argv[i]);

	for (const std::string &op : used_operators)
		printf("%s\n", op.c_str());

	return total > 0 ? 0 : 1;
}
