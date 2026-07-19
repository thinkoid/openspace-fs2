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

// ---------------------------------------------------------------------------
// --full: a complete model-geometry dump (no textures, no ancillary runtime
// data).  Everything below reads the *loaded* polymodel plus the raw per-
// submodel bsp_data blob, which retail copies verbatim and never parses at
// load -- so this walker is the load-time-absent half of the model: the actual
// geometry.  Byte layouts are the ones documented in model/modelcollide.cpp
// (mirrored here because modelsinc.h is MODEL_LIB-internal).  This is the
// oracle surface to match pcs2 against, once its parse is done.
// ---------------------------------------------------------------------------

// BSP bytecode opcodes (model/modelsinc.h)
enum { BOP_EOF = 0, BOP_DEFPOINTS = 1, BOP_FLATPOLY = 2, BOP_TMAPPOLY = 3,
       BOP_SORTNORM = 4, BOP_BOUNDBOX = 5 };

// unaligned little-endian reads off the raw blob (retail uses raw casts via w()/
// fl()/vp(); memcpy is the same on x86 without the aliasing/alignment UB)
static int   rd_int(const ubyte *p)   { int v;   memcpy(&v, p, 4); return v; }
static short rd_short(const ubyte *p)  { short v; memcpy(&v, p, 2); return v; }
static float rd_float(const ubyte *p)  { float v; memcpy(&v, p, 4); return v; }
static vector rd_vec(const ubyte *p)
{
	vector v;
	v.x = rd_float(p); v.y = rd_float(p + 4); v.z = rd_float(p + 8);
	return v;
}

// vertex positions defined by the current submodel's OP_DEFPOINTS, referenced
// by index from the poly chunks (exactly as Mc_point_list works in retail)
static std::vector<vector> Bsp_points;

static void bsp_defpoints(const ubyte *p)
{
	int nverts = rd_int(p + 8);
	int offset = rd_int(p + 16);
	const ubyte *normcount = p + 20;
	const ubyte *src = p + offset;

	Bsp_points.assign(nverts, vector());
	printf("      points %d\n", nverts);
	for (int n = 0; n < nverts; n++) {
		vector pos = rd_vec(src);
		int nnorm = normcount[n];
		Bsp_points[n] = pos;
		printf("        v %d %.6g %.6g %.6g norms %d",
			n, pos.x, pos.y, pos.z, nnorm);
		for (int k = 0; k < nnorm; k++) {
			vector nv = rd_vec(src + 12 * (k + 1));
			printf(" [%.6g %.6g %.6g]", nv.x, nv.y, nv.z);
		}
		printf("\n");
		src += 12 * (nnorm + 1);
	}
}

static void bsp_flatpoly(const ubyte *p)
{
	vector norm = rd_vec(p + 8);
	vector cen  = rd_vec(p + 20);
	float  rad  = rd_float(p + 32);
	int    nv   = rd_int(p + 36);
	int r = p[40], g = p[41], b = p[42];
	printf("      flat nv %d normal %.6g %.6g %.6g center %.6g %.6g %.6g rad %.6g rgb %d %d %d\n",
		nv, norm.x, norm.y, norm.z, cen.x, cen.y, cen.z, rad, r, g, b);
	const ubyte *verts = p + 44;			// nv * (short vertnum, short normnum)
	printf("        verts");
	for (int i = 0; i < nv; i++)
		printf(" %d/%d", rd_short(verts + i * 4), rd_short(verts + i * 4 + 2));
	printf("\n");
}

static void bsp_tmappoly(const ubyte *p)
{
	vector norm = rd_vec(p + 8);
	vector cen  = rd_vec(p + 20);
	float  rad  = rd_float(p + 32);
	int    nv   = rd_int(p + 36);
	int    tmap = rd_int(p + 40);
	printf("      tmap nv %d normal %.6g %.6g %.6g center %.6g %.6g %.6g rad %.6g tmap %d\n",
		nv, norm.x, norm.y, norm.z, cen.x, cen.y, cen.z, rad, tmap);
	const ubyte *verts = p + 44;			// nv * model_tmap_vert{short vertnum,normnum; float u,v}
	printf("        verts");
	for (int i = 0; i < nv; i++) {
		const ubyte *v = verts + i * 12;
		printf(" %d/%d(%.6g,%.6g)", rd_short(v), rd_short(v + 2),
			rd_float(v + 4), rd_float(v + 8));
	}
	printf("\n");
}

// mirror of model_collide_sub + model_collide_sortnorm (model/modelcollide.cpp),
// emitting the tree instead of colliding.  SORTNORM offsets are self-relative.
static void bsp_walk(const ubyte *p, int version, int depth)
{
	if (depth > 256) { printf("      !! bsp recursion too deep\n"); return; }

	for (;;) {
		int chunk_type = rd_int(p);
		int chunk_size = rd_int(p + 4);
		if (chunk_type == BOP_EOF)
			return;

		switch (chunk_type) {
		case BOP_DEFPOINTS: bsp_defpoints(p); break;
		case BOP_FLATPOLY:  bsp_flatpoly(p);  break;
		case BOP_TMAPPOLY:  bsp_tmappoly(p);  break;
		case BOP_BOUNDBOX: {
			vector mn = rd_vec(p + 8), mx = rd_vec(p + 20);
			printf("      boundbox min %.6g %.6g %.6g max %.6g %.6g %.6g\n",
				mn.x, mn.y, mn.z, mx.x, mx.y, mx.z);
			break;
		}
		case BOP_SORTNORM: {
			int pre = rd_int(p + 44), back = rd_int(p + 40), on = rd_int(p + 52);
			int front = rd_int(p + 36), post = rd_int(p + 48);
			printf("      sortnorm pre %d back %d on %d front %d post %d\n",
				pre, back, on, front, post);
			// same visit order as model_collide_sortnorm
			if (pre)   bsp_walk(p + pre,   version, depth + 1);
			if (back)  bsp_walk(p + back,  version, depth + 1);
			if (on)    bsp_walk(p + on,    version, depth + 1);
			if (front) bsp_walk(p + front, version, depth + 1);
			if (post)  bsp_walk(p + post,  version, depth + 1);
			break;
		}
		default:
			printf("      !! bad chunk type %d size %d\n", chunk_type, chunk_size);
			return;
		}
		p += chunk_size;
	}
}

static void dump_model_full(char *name)
{
	int num = model_load(name, 0, NULL);
	if (num < 0) {
		printf("=== %s LOAD-FAILED\n", name);
		return;
	}
	polymodel *pm = model_get(num);

	printf("=== %s version %d\n", name, pm->version);
	printf("model:\n");
	printf("  flags 0x%x%s%s\n", pm->flags,
		(pm->flags & PM_FLAG_ALLOW_TILING) ? " tiling" : "",
		(pm->flags & PM_FLAG_AUTOCEN) ? " autocen" : "");
	printf("  rad %.6g core_radius %.6g mass %.6g\n", pm->rad, pm->core_radius, pm->mass);
	dump_vec("mins", &pm->mins);
	dump_vec("maxs", &pm->maxs);
	dump_vec("center_of_mass", &pm->center_of_mass);
	printf("  moment_of_inertia\n");
	for (int r = 0; r < 3; r++)
		printf("    %.6g %.6g %.6g\n",
			pm->moment_of_inertia.a2d[r][0], pm->moment_of_inertia.a2d[r][1],
			pm->moment_of_inertia.a2d[r][2]);
	if (pm->flags & PM_FLAG_AUTOCEN)
		dump_vec("autocenter", &pm->autocenter);
	printf("  detail_levels %d:", pm->n_detail_levels);
	for (int i = 0; i < pm->n_detail_levels; i++)
		printf(" %d(depth %.6g)", pm->detail[i], pm->detail_depth[i]);
	printf("\n");
	printf("  debris_objects %d:", pm->num_debris_objects);
	for (int i = 0; i < pm->num_debris_objects; i++)
		printf(" %d", pm->debris_objects[i]);
	printf("\n");

	printf("submodels %d\n", pm->n_models);
	for (int i = 0; i < pm->n_models; i++) {
		bsp_info *sm = &pm->submodel[i];
		printf("  sub %d \"%s\"\n", i, sm->name);
		printf("    parent %d movement_type %d movement_axis %d\n",
			sm->parent, sm->movement_type, sm->movement_axis);
		printf("    offset %.6g %.6g %.6g\n", sm->offset.x, sm->offset.y, sm->offset.z);
		printf("    geometric_center %.6g %.6g %.6g rad %.6g\n",
			sm->geometric_center.x, sm->geometric_center.y, sm->geometric_center.z, sm->rad);
		printf("    min %.6g %.6g %.6g max %.6g %.6g %.6g\n",
			sm->min.x, sm->min.y, sm->min.z, sm->max.x, sm->max.y, sm->max.z);
		printf("    children num %d first_child %d next_sibling %d\n",
			sm->num_children, sm->first_child, sm->next_sibling);
		printf("    details num %d:", sm->num_details);
		for (int d = 0; d < sm->num_details; d++)
			printf(" %d", sm->details[d]);
		printf("\n");
		printf("    bsp_data_size %d\n", sm->bsp_data_size);
		if (sm->bsp_data && sm->bsp_data_size > 0) {
			printf("    geometry:\n");
			Bsp_points.clear();
			bsp_walk(sm->bsp_data, pm->version, 0);
		}
	}

	printf("guns %d\n", pm->n_guns);
	for (int i = 0; i < pm->n_guns; i++) {
		w_bank *b = &pm->gun_banks[i];
		printf("  bank %d slots %d\n", i, b->num_slots);
		for (int s = 0; s < b->num_slots; s++)
			printf("    pnt %.6g %.6g %.6g norm %.6g %.6g %.6g\n",
				b->pnt[s].x, b->pnt[s].y, b->pnt[s].z,
				b->norm[s].x, b->norm[s].y, b->norm[s].z);
	}
	printf("missiles %d\n", pm->n_missiles);
	for (int i = 0; i < pm->n_missiles; i++) {
		w_bank *b = &pm->missile_banks[i];
		printf("  bank %d slots %d\n", i, b->num_slots);
		for (int s = 0; s < b->num_slots; s++)
			printf("    pnt %.6g %.6g %.6g norm %.6g %.6g %.6g\n",
				b->pnt[s].x, b->pnt[s].y, b->pnt[s].z,
				b->norm[s].x, b->norm[s].y, b->norm[s].z);
	}
	printf("docks %d\n", pm->n_docks);
	for (int i = 0; i < pm->n_docks; i++) {
		dock_bay *d = &pm->docking_bays[i];
		printf("  bay %d \"%s\" type 0x%x slots %d spline_paths %d\n",
			i, d->name, d->type_flags, d->num_slots, d->num_spline_paths);
		for (int s = 0; s < d->num_slots; s++)
			printf("    pnt %.6g %.6g %.6g norm %.6g %.6g %.6g\n",
				d->pnt[s].x, d->pnt[s].y, d->pnt[s].z,
				d->norm[s].x, d->norm[s].y, d->norm[s].z);
	}
	printf("thrusters %d\n", pm->n_thrusters);
	for (int i = 0; i < pm->n_thrusters; i++) {
		thruster_bank *t = &pm->thrusters[i];
		printf("  bank %d slots %d\n", i, t->num_slots);
		for (int s = 0; s < t->num_slots; s++)
			printf("    pnt %.6g %.6g %.6g norm %.6g %.6g %.6g radius %.6g\n",
				t->pnt[s].x, t->pnt[s].y, t->pnt[s].z,
				t->norm[s].x, t->norm[s].y, t->norm[s].z, t->radius[s]);
	}
	printf("eyes %d\n", pm->n_view_positions);
	for (int i = 0; i < pm->n_view_positions; i++) {
		eye *e = &pm->view_positions[i];
		printf("  eye %d parent %d pnt %.6g %.6g %.6g norm %.6g %.6g %.6g\n",
			i, e->parent, e->pnt.x, e->pnt.y, e->pnt.z, e->norm.x, e->norm.y, e->norm.z);
	}

	printf("shield nverts %d ntris %d\n", pm->shield.nverts, pm->shield.ntris);
	for (int i = 0; i < pm->shield.nverts; i++)
		printf("  v %d %.6g %.6g %.6g\n", i,
			pm->shield.verts[i].pos.x, pm->shield.verts[i].pos.y, pm->shield.verts[i].pos.z);
	for (int i = 0; i < pm->shield.ntris; i++) {
		shield_tri *t = &pm->shield.tris[i];
		printf("  t %d verts %d %d %d neighbors %d %d %d norm %.6g %.6g %.6g\n", i,
			t->verts[0], t->verts[1], t->verts[2],
			t->neighbors[0], t->neighbors[1], t->neighbors[2],
			t->norm.x, t->norm.y, t->norm.z);
	}

	printf("paths %d\n", pm->n_paths);
	for (int i = 0; i < pm->n_paths; i++) {
		model_path *pa = &pm->paths[i];
		printf("  path %d \"%s\" parent \"%s\"(%d) type %d value %d goal %d nverts %d\n",
			i, pa->name, pa->parent_name, pa->parent_submodel,
			pa->type, pa->value, pa->goal, pa->nverts);
		for (int v = 0; v < pa->nverts; v++)
			printf("    vert %d %.6g %.6g %.6g radius %.6g\n", v,
				pa->verts[v].pos.x, pa->verts[v].pos.y, pa->verts[v].pos.z, pa->verts[v].radius);
	}

	printf("insignia %d\n", pm->num_ins);
	for (int i = 0; i < pm->num_ins; i++) {
		insignia *in = &pm->ins[i];
		printf("  ins %d detail_level %d num_faces %d offset %.6g %.6g %.6g\n",
			i, in->detail_level, in->num_faces, in->offset.x, in->offset.y, in->offset.z);
		for (int f = 0; f < in->num_faces; f++)
			printf("    face %d verts %d %d %d u %.6g %.6g %.6g v %.6g %.6g %.6g\n", f,
				in->faces[f][0], in->faces[f][1], in->faces[f][2],
				in->u[f][0], in->u[f][1], in->u[f][2],
				in->v[f][0], in->v[f][1], in->v[f][2]);
	}

	int nxc = pm->num_xc < 0 ? 0 : pm->num_xc;	// retail inits num_xc to -1 = none
	printf("cross_sections %d\n", nxc);
	for (int i = 0; i < nxc; i++)
		printf("  xc %d z %.6g radius %.6g\n", i, pm->xc[i].z, pm->xc[i].radius);

	// deliberately omitted: textures, lights (glowpoints), octants, split
	// planes, debug_info -- textures + ancillary runtime data, not model geometry.

	model_free_all();
}

// case-insensitive: does the model filename (with extension) match one of the
// requested basenames (with or without .pof)?
static bool wanted(const std::string &name, const std::vector<std::string> &filters)
{
	if (filters.empty())
		return true;
	const char *slash = strrchr(name.c_str(), '/');
	const char *base = slash ? slash + 1 : name.c_str();
	for (const std::string &f : filters) {
		if (!strcasecmp(base, f.c_str()))
			return true;
		// also match when the filter omits the .pof extension
		if (f.size() + 4 == name.size() + (base - name.c_str()) &&
		    !strncasecmp(base, f.c_str(), f.size()) &&
		    !strcasecmp(base + f.size(), ".pof"))
			return true;
	}
	return false;
}

int main(int argc, char *argv[])
{
	bool full = false;
	const char *game_root = NULL;
	std::vector<std::string> filters;	// optional: dump only these models
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--full"))
			full = true;
		else if (game_root == NULL)
			game_root = argv[i];
		else
			filters.push_back(argv[i]);
	}
	if (game_root == NULL) {
		fprintf(stderr, "usage: pof_dump [--full] <game-root> [model.pof ...]\n");
		return 2;
	}

	char exe_path[CF_MAX_PATHNAME_LENGTH];
	snprintf(exe_path, sizeof(exe_path), "%s/x", game_root);
	if (cfile_init(exe_path)) {
		fprintf(stderr, "cfile_init failed for %s\n", game_root);
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
		if (name.size() > 4 && !strcasecmp(name.c_str() + name.size() - 4, ".pof") &&
		    wanted(name, filters)) {
			if (full)
				dump_model_full((char *)name.c_str());
			else
				dump_model((char *)name.c_str());
			n++;
		}

	fprintf(stderr, "pof_dump: %d models\n", n);
	return n > 0 ? 0 : 1;
}
