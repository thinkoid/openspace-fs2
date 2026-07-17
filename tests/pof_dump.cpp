// Point-3 oracle: load every POF model cfile can see with the retail loader
// (the only authoritative POF implementation) and dump the parsed structure.
// The pinned output is the regression floor for modelread and, later, the
// reference to complete pofer against.
//
//   pof_dump <game-root>

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "pstypes.h"
#include "cfile.h"
#include "model.h"

static std::vector<std::string> full_names;

static int capture_name(char *name_ext)
{
	full_names.push_back(name_ext);
	return 1;
}

static void dump_vec(const char *tag, vector *v)
{
	printf("  %s %.6g %.6g %.6g\n", tag, v->x, v->y, v->z);
}

static void dump_model(char *name)
{
	int num = model_load(name, 0, NULL);
	if (num < 0) {
		printf("== %s LOAD-FAILED\n", name);
		return;
	}
	polymodel *pm = model_get(num);

	printf("== %s version %d\n", name, pm->version);
	printf("  submodels %d details %d debris %d\n",
		pm->n_models, pm->n_detail_levels, pm->num_debris_objects);
	printf("  rad %.6g core_rad %.6g mass %.6g\n", pm->rad, pm->core_radius, pm->mass);
	dump_vec("mins", &pm->mins);
	dump_vec("maxs", &pm->maxs);
	dump_vec("center_of_mass", &pm->center_of_mass);
	printf("  guns %d missiles %d docks %d thrusters %d paths %d eyes %d textures %d shield_tris %d\n",
		pm->n_guns, pm->n_missiles, pm->n_docks, pm->n_thrusters,
		pm->n_paths, pm->n_view_positions, pm->n_textures, pm->shield.ntris);

	for (int i = 0; i < pm->n_models; i++) {
		bsp_info *sm = &pm->submodel[i];
		printf("  sub %2d %-24s parent %2d children %2d rad %.6g bsp %d off %.6g %.6g %.6g\n",
			i, sm->name, sm->parent, sm->num_children, sm->rad,
			sm->bsp_data_size, sm->offset.x, sm->offset.y, sm->offset.z);
	}

	model_free_all();	// keep the 128-slot table empty
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "usage: pof_dump <game-root>\n");
		return 2;
	}

	char exe_path[CF_MAX_PATHNAME_LENGTH];
	snprintf(exe_path, sizeof(exe_path), "%s/x", argv[1]);
	if (cfile_init(exe_path)) {
		fprintf(stderr, "cfile_init failed for %s\n", argv[1]);
		return 1;
	}

	static char arr[1024][MAX_FILENAME_LEN];
	static char *list[1024];
	full_names.clear();
	Get_file_list_filter = capture_name;
	cf_get_file_list_preallocated(1024, arr, list, CF_TYPE_MODELS, (char *)"*", CF_SORT_NONE);

	// deterministic order regardless of VP index order
	std::sort(full_names.begin(), full_names.end());

	int n = 0;
	for (const std::string &name : full_names)
		if (name.size() > 4 && !strcasecmp(name.c_str() + name.size() - 4, ".pof")) {
			dump_model((char *)name.c_str());
			n++;
		}

	fprintf(stderr, "pof_dump: %d models\n", n);
	return n > 0 ? 0 : 1;
}
