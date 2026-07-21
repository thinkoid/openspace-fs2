// Point-3 oracle: load every POF model cfile can see with the retail loader
// (the only authoritative POF implementation) and dump the parsed structure.
// The pinned output is the regression floor for modelread and, later, the
// reference to complete pofer against.
//
//   pof_dump <game-root>

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>

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

// The model name is the only part of this dump that depends on where the data
// came from: the VP archives keep the original mixed case ("BeamSaber.POF")
// while an unpacked install has lowercased filenames.  Fold it so the dump --
// and the order the models come out in -- is identical from either root, which
// is what lets a second implementation reading loose files match this output.
// model_load still gets the name as cfile reported it.
static std::string fold_name(const std::string &s)
{
	std::string r(s);
	for (size_t i = 0; i < r.size(); i++)
		r[i] = tolower((unsigned char)r[i]);
	return r;
}

static void dump_vec(const char *tag, vector *v)
{
	printf("  %s %.6g %.6g %.6g\n", tag, v->x, v->y, v->z);
}

static void dump_model(char *name)
{
	int num = model_load(name, 0, NULL);
	if (num < 0) {
		printf("== %s LOAD-FAILED\n", fold_name(name).c_str());
		return;
	}
	polymodel *pm = model_get(num);

	printf("== %s version %d\n", fold_name(name).c_str(), pm->version);
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
		printf("=== %s LOAD-FAILED\n", fold_name(name).c_str());
		return;
	}
	polymodel *pm = model_get(num);

	printf("=== %s version %d\n", fold_name(name).c_str(), pm->version);
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

// every vertex normal of the current submodel, flattened in vertex order --
// the layout OP_DEFPOINTS stores them in, and what normnum indexes
static std::vector<vector> Bsp_norms;

static void read_defpoints(const ubyte *p)
{
	int nverts = rd_int(p + 8);
	int offset = rd_int(p + 16);
	const ubyte *normcount = p + 20;
	const ubyte *src = p + offset;

	Bsp_points.assign(nverts, vector());
	Bsp_norms.clear();
	for (int n = 0; n < nverts; n++) {
		Bsp_points[n] = rd_vec(src);
		int nnorm = normcount[n];
		for (int k = 0; k < nnorm; k++)
			Bsp_norms.push_back(rd_vec(src + 12 * (k + 1)));
		src += 12 * (nnorm + 1);
	}
}

// ---------------------------------------------------------------------------
// --model: the unified comparison + interchange dump.  The format is specified
// in pofer's doc/model-dump-spec.md and implemented independently there and in
// libpof; the three converge on that text, not on each other's sources.
//
// The rule deciding every field: the dump is the polymodel -- every value this
// loader retains from the file, after its own interpretation, and nothing else.
// Where --geom sorted formatted strings, this sorts polygons by a key computed
// from the data (the complete record, compared as values), so the order is a
// property of the model rather than of a printf format.  All floats print as
// %.9g, which round-trips a float32 exactly; -0 folds to +0 (the same number,
// so a genuine equivalence class -- see the spec for why that is the only
// canonicalisation allowed).
// ---------------------------------------------------------------------------

// A NaN or infinity would poison both the text and the sort key's total
// order; no retail model contains one, so meeting one is corruption and the
// dump must die, not print.
static float canon(float f)
{
	if (!isfinite(f)) {
		fprintf(stderr, "pof_dump: non-finite float in model\n");
		exit(3);
	}
	return f == 0.0f ? 0.0f : f;
}

static void app(std::string &line, const char *fmt, ...)
{
	char buf[512];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	line += buf;
}

static void app_f(std::string &line, float f)
{
	app(line, "%.9g", canon(f));
}

static void app_vec(std::string &line, const vector &v)
{
	app(line, "(%.9g %.9g %.9g)", canon(v.x), canon(v.y), canon(v.z));
}

static void put(const std::string &line)
{
	printf("%s\n", line.c_str());
}

// One polygon, carrying its own sort key.  The key holds every field of the
// line, in the line's field order, as values -- kind, colour, count, face
// normal/center/radius, then each corner.  A double holds an int and a float
// exactly, so one vector compares the mixed record lexicographically.  Keying
// the complete record makes ties free: records equal on it are byte-identical.
struct canon_poly {
	std::vector<double> key;
	std::string line;

	bool operator<(const canon_poly &o) const { return key < o.key; }
};

static void model_append(std::vector<canon_poly> &out, const ubyte *p, bool textured)
{
	vector norm = rd_vec(p + 8);
	vector cen  = rd_vec(p + 20);
	float  rad  = rd_float(p + 32);
	int    nv   = rd_int(p + 36);

	canon_poly poly;
	poly.key.push_back(textured ? BOP_TMAPPOLY : BOP_FLATPOLY);

	if (textured) {
		int tmap = rd_int(p + 40);
		poly.key.push_back(tmap);
		app(poly.line, "      poly tex %d nv %d normal ", tmap, nv);
	} else {
		int r = p[40], g = p[41], b = p[42];
		poly.key.push_back(r);
		poly.key.push_back(g);
		poly.key.push_back(b);
		app(poly.line, "      poly flat %d %d %d nv %d normal ", r, g, b, nv);
	}
	poly.key.push_back(nv);

	const float face[7] = { norm.x, norm.y, norm.z, cen.x, cen.y, cen.z, rad };
	for (int i = 0; i < 7; i++)
		poly.key.push_back(canon(face[i]));

	app_vec(poly.line, norm);
	poly.line += " center ";
	app_vec(poly.line, cen);
	poly.line += " radius ";
	app_f(poly.line, rad);

	const ubyte *verts = p + 44;
	const int stride = textured ? 12 : 4;
	for (int i = 0; i < nv; i++) {
		const ubyte *v = verts + i * stride;
		int vertnum = rd_short(v), normnum = rd_short(v + 2);
		// an index the file got wrong reads as the origin, which is what
		// retail's walkers do with it
		vector pos = (vertnum >= 0 && vertnum < (int)Bsp_points.size())
			? Bsp_points[vertnum] : vector();
		vector nrm = (normnum >= 0 && normnum < (int)Bsp_norms.size())
			? Bsp_norms[normnum] : vector();

		const float corner[6] = { pos.x, pos.y, pos.z, nrm.x, nrm.y, nrm.z };
		for (int k = 0; k < 6; k++)
			poly.key.push_back(canon(corner[k]));

		poly.line += ' ';
		app_vec(poly.line, pos);
		app_vec(poly.line, nrm);

		if (textured) {
			float u = rd_float(v + 4), w = rd_float(v + 8);
			poly.key.push_back(canon(u));
			poly.key.push_back(canon(w));
			app(poly.line, "(%.9g %.9g)", canon(u), canon(w));
		}
	}

	out.push_back(poly);
}

static void model_walk(const ubyte *p, std::vector<canon_poly> &out, int depth)
{
	if (depth > 256)
		return;

	for (;;) {
		int chunk_type = rd_int(p);
		int chunk_size = rd_int(p + 4);
		if (chunk_type == BOP_EOF)
			return;

		switch (chunk_type) {
		case BOP_DEFPOINTS: read_defpoints(p); break;
		case BOP_FLATPOLY:  model_append(out, p, false); break;
		case BOP_TMAPPOLY:  model_append(out, p, true);  break;
		case BOP_BOUNDBOX:  break;
		case BOP_SORTNORM: {
			int pre = rd_int(p + 44), back = rd_int(p + 40), on = rd_int(p + 52);
			int front = rd_int(p + 36), post = rd_int(p + 48);
			if (pre)   model_walk(p + pre,   out, depth + 1);
			if (back)  model_walk(p + back,  out, depth + 1);
			if (on)    model_walk(p + on,    out, depth + 1);
			if (front) model_walk(p + front, out, depth + 1);
			if (post)  model_walk(p + post,  out, depth + 1);
			break;
		}
		default:
			return;
		}
		p += chunk_size;
	}
}

static void dump_model_model(char *name)
{
	// Pass 1: retail keeps turret data (ID_TGUN/ID_TMIS) in the caller's
	// subsystem list, not in the polymodel, and matches entries by submodel
	// name -- the game primes that list from ships.tbl.  Here the model's own
	// submodel names prime it, so the first load only harvests them.
	int num = model_load(name, 0, NULL);
	if (num < 0) {
		printf("model %s LOAD-FAILED\n", fold_name(name).c_str());
		return;
	}
	polymodel *pm = model_get(num);

	std::vector<std::string> subnames;
	for (int i = 0; i < pm->n_models; i++)
		subnames.push_back(pm->submodel[i].name);
	model_free_all();

	std::vector<model_subsystem> subs(subnames.size());
	memset(subs.data(), 0, subs.size() * sizeof(model_subsystem));
	for (size_t i = 0; i < subnames.size(); i++) {
		strncpy(subs[i].subobj_name, subnames[i].c_str(), MAX_NAME_LEN - 1);
		subs[i].subobj_num = -3;	// no ID_TGUN parent can match this
		subs[i].turret_gun_sobj = -1;
	}

	num = model_load(name, (int)subs.size(), subs.data());
	if (num < 0) {
		printf("model %s LOAD-FAILED\n", fold_name(name).c_str());
		return;
	}
	pm = model_get(num);

	// An ID_SPCL point whose name matches a submodel's would overwrite that
	// entry's subobj_num with -1 and could eat a turret arriving later in the
	// chunk stream.  No retail model has such a collision (measured); a file
	// that does must fail loudly, not dump a turret short.
	for (size_t i = 0; i < subs.size(); i++)
		if (subs[i].subobj_num == -1) {
			fprintf(stderr, "pof_dump: SPCL name collides with submodel "
				"\"%s\" in %s\n", subs[i].subobj_name, name);
			exit(3);
		}

	std::string line;

	printf("model %s version %d\n", fold_name(name).c_str(), pm->version);
	printf("flags %d\n", pm->flags);
	printf("radius %.9g\n", canon(pm->rad));

	line = "min ";       app_vec(line, pm->mins);           put(line); line.clear();
	line = "max ";       app_vec(line, pm->maxs);           put(line); line.clear();
	line = "mass ";      app_f(line, pm->mass);             put(line); line.clear();
	line = "mass_center "; app_vec(line, pm->center_of_mass); put(line); line.clear();

	line = "moi ";
	app_vec(line, pm->moment_of_inertia.rvec); line += ' ';
	app_vec(line, pm->moment_of_inertia.uvec); line += ' ';
	app_vec(line, pm->moment_of_inertia.fvec);
	put(line); line.clear();

	if (pm->flags & PM_FLAG_AUTOCEN) {
		line = "autocenter ";
		app_vec(line, pm->autocenter);
		put(line); line.clear();
	}

	printf("details %d", pm->n_detail_levels);
	for (int i = 0; i < pm->n_detail_levels; i++)
		printf(" %d", pm->detail[i]);
	printf("\n");

	printf("debris %d", pm->num_debris_objects);
	for (int i = 0; i < pm->num_debris_objects; i++)
		printf(" %d", pm->debris_objects[i]);
	printf("\n");

	int nxc = pm->num_xc < 0 ? 0 : pm->num_xc;	// retail inits num_xc to -1
	printf("cross_sections %d\n", nxc);
	for (int i = 0; i < nxc; i++) {
		app(line, "  xc %d depth ", i);
		app_f(line, pm->xc[i].z);
		line += " radius ";
		app_f(line, pm->xc[i].radius);
		put(line); line.clear();
	}

	printf("lights %d\n", pm->num_lights);
	for (int i = 0; i < pm->num_lights; i++) {
		app(line, "  light %d ", i);
		app_vec(line, pm->lights[i].pos);
		app(line, " type %d", pm->lights[i].type);
		put(line); line.clear();
	}

	printf("split_planes %d", pm->num_split_plane);
	for (int i = 0; i < pm->num_split_plane; i++)
		printf(" %.9g", canon(pm->split_plane[i]));
	printf("\n");

	printf("textures %d\n", pm->n_textures);
	for (int i = 0; i < pm->n_textures; i++)
		printf("  tex %d \"%s\"\n", i, pm->texture_file[i]);

	printf("submodels %d\n", pm->n_models);
	for (int i = 0; i < pm->n_models; i++) {
		bsp_info *sm = &pm->submodel[i];

		printf("  sub %d \"%s\" parent %d\n", i, sm->name, sm->parent);
		printf("    movement type %d axis %d\n",
			sm->movement_type, sm->movement_axis);

		line = "    radius ";  app_f(line, sm->rad);
		line += " offset ";    app_vec(line, sm->offset);
		line += " center ";    app_vec(line, sm->geometric_center);
		put(line); line.clear();

		line = "    min ";     app_vec(line, sm->min);
		line += " max ";       app_vec(line, sm->max);
		put(line); line.clear();

		std::vector<canon_poly> polys;
		if (sm->bsp_data && sm->bsp_data_size > 0) {
			Bsp_points.clear();
			Bsp_norms.clear();
			model_walk(sm->bsp_data, polys, 0);
		}
		std::sort(polys.begin(), polys.end());
		printf("    polys %d\n", (int)polys.size());
		for (const canon_poly &poly : polys)
			put(poly.line);
	}

	printf("guns %d\n", pm->n_guns);
	for (int i = 0; i < pm->n_guns; i++) {
		w_bank *b = &pm->gun_banks[i];
		printf("  bank %d slots %d\n", i, b->num_slots);
		for (int s = 0; s < b->num_slots; s++) {
			app(line, "    slot %d point ", s);
			app_vec(line, b->pnt[s]);
			line += " normal ";
			app_vec(line, b->norm[s]);
			put(line); line.clear();
		}
	}

	printf("missiles %d\n", pm->n_missiles);
	for (int i = 0; i < pm->n_missiles; i++) {
		w_bank *b = &pm->missile_banks[i];
		printf("  bank %d slots %d\n", i, b->num_slots);
		for (int s = 0; s < b->num_slots; s++) {
			app(line, "    slot %d point ", s);
			app_vec(line, b->pnt[s]);
			line += " normal ";
			app_vec(line, b->norm[s]);
			put(line); line.clear();
		}
	}

	// Emitted in base-submodel order, which is the order the primed list was
	// built in; retail does not retain whether a turret came from ID_TGUN or
	// ID_TMIS, so neither does the dump.
	int n_turrets = 0;
	for (size_t i = 0; i < subs.size(); i++)
		if (subs[i].turret_num_firing_points > 0)
			n_turrets++;
	printf("turrets %d\n", n_turrets);
	for (size_t i = 0; i < subs.size(); i++) {
		model_subsystem *ss = &subs[i];
		if (ss->turret_num_firing_points <= 0)
			continue;
		app(line, "  turret sub %d arm %d normal ",
			ss->subobj_num, ss->turret_gun_sobj);
		app_vec(line, ss->turret_norm);
		app(line, " points %d", ss->turret_num_firing_points);
		for (int s = 0; s < ss->turret_num_firing_points; s++) {
			line += ' ';
			app_vec(line, ss->turret_firing_point[s]);
		}
		put(line); line.clear();
	}

	printf("docks %d\n", pm->n_docks);
	for (int i = 0; i < pm->n_docks; i++) {
		dock_bay *d = &pm->docking_bays[i];
		app(line, "  dock %d \"%s\" paths %d", i, d->name, d->num_spline_paths);
		for (int s = 0; s < d->num_spline_paths; s++)
			app(line, " %d", d->splines[s]);
		app(line, " slots %d", d->num_slots);
		put(line); line.clear();
		for (int s = 0; s < d->num_slots; s++) {
			app(line, "    slot %d point ", s);
			app_vec(line, d->pnt[s]);
			line += " normal ";
			app_vec(line, d->norm[s]);
			put(line); line.clear();
		}
	}

	printf("thrusters %d\n", pm->n_thrusters);
	for (int i = 0; i < pm->n_thrusters; i++) {
		thruster_bank *t = &pm->thrusters[i];
		printf("  bank %d slots %d\n", i, t->num_slots);
		for (int s = 0; s < t->num_slots; s++) {
			app(line, "    slot %d point ", s);
			app_vec(line, t->pnt[s]);
			line += " normal ";
			app_vec(line, t->norm[s]);
			line += " radius ";
			app_f(line, t->radius[s]);
			put(line); line.clear();
		}
	}

	printf("eyes %d\n", pm->n_view_positions);
	for (int i = 0; i < pm->n_view_positions; i++) {
		eye *e = &pm->view_positions[i];
		app(line, "  eye %d parent %d point ", i, e->parent);
		app_vec(line, e->pnt);
		line += " normal ";
		app_vec(line, e->norm);
		put(line); line.clear();
	}

	printf("shield verts %d tris %d\n", pm->shield.nverts, pm->shield.ntris);
	for (int i = 0; i < pm->shield.nverts; i++) {
		app(line, "  v %d ", i);
		app_vec(line, pm->shield.verts[i].pos);
		put(line); line.clear();
	}
	for (int i = 0; i < pm->shield.ntris; i++) {
		shield_tri *t = &pm->shield.tris[i];
		app(line, "  tri %d normal ", i);
		app_vec(line, t->norm);
		app(line, " verts %d %d %d neighbors %d %d %d",
			t->verts[0], t->verts[1], t->verts[2],
			t->neighbors[0], t->neighbors[1], t->neighbors[2]);
		put(line); line.clear();
	}

	printf("paths %d\n", pm->n_paths);
	for (int i = 0; i < pm->n_paths; i++) {
		model_path *pa = &pm->paths[i];
		printf("  path %d \"%s\" parent \"%s\" sub %d verts %d\n",
			i, pa->name, pa->parent_name, pa->parent_submodel, pa->nverts);
		for (int v = 0; v < pa->nverts; v++) {
			app(line, "    vert %d ", v);
			app_vec(line, pa->verts[v].pos);
			line += " radius ";
			app_f(line, pa->verts[v].radius);
			app(line, " turrets %d", pa->verts[v].nturrets);
			for (int k = 0; k < pa->verts[v].nturrets; k++)
				app(line, " %d", pa->verts[v].turret_ids[k]);
			put(line); line.clear();
		}
	}

	// Insignia faces resolve their vertex indices to coordinates: retail does
	// not retain the insignia vertex count, so the table itself cannot be
	// dumped faithfully -- same projection rule as the polygons.
	printf("insignia %d\n", pm->num_ins);
	for (int i = 0; i < pm->num_ins; i++) {
		insignia *in = &pm->ins[i];
		app(line, "  ins %d detail %d offset ", i, in->detail_level);
		app_vec(line, in->offset);
		app(line, " faces %d", in->num_faces);
		put(line); line.clear();
		for (int f = 0; f < in->num_faces; f++) {
			app(line, "    face %d", f);
			for (int k = 0; k < 3; k++) {
				int idx = in->faces[f][k];
				vector pos = (idx >= 0 && idx < MAX_INS_VECS)
					? in->vecs[idx] : vector();
				app(line, " (%.9g %.9g %.9g %.9g %.9g)",
					canon(pos.x), canon(pos.y), canon(pos.z),
					canon(in->u[f][k]), canon(in->v[f][k]));
			}
			put(line); line.clear();
		}
	}

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
	bool model = false;
	const char *game_root = NULL;
	std::vector<std::string> filters;	// optional: dump only these models
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--full"))
			full = true;
		else if (!strcmp(argv[i], "--model"))
			model = true;
		else if (game_root == NULL)
			game_root = argv[i];
		else
			filters.push_back(argv[i]);
	}
	if (game_root == NULL) {
		fprintf(stderr, "usage: pof_dump [--full|--model] <game-root> [model.pof ...]\n");
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

	// deterministic order regardless of VP index order or filename case
	std::sort(full_names.begin(), full_names.end(),
		[](const std::string &a, const std::string &b) {
			return fold_name(a) < fold_name(b);
		});

	int n = 0;
	for (const std::string &name : full_names)
		if (name.size() > 4 && !strcasecmp(name.c_str() + name.size() - 4, ".pof") &&
		    wanted(name, filters)) {
			if (model)
				dump_model_model((char *)name.c_str());
			else if (full)
				dump_model_full((char *)name.c_str());
			else
				dump_model((char *)name.c_str());
			n++;
		}

	fprintf(stderr, "pof_dump: %d models\n", n);
	return n > 0 ? 0 : 1;
}
