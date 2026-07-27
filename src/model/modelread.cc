/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#include <string.h>
#include <ctype.h>

#define MODEL_LIB

#include <cfile/cfile.hh>
#include <model/model.hh>
#include <bmpman/bmpman.hh>
#include <math/floating.hh>
#include <render/3d.hh>
#include <ship/ship.hh>
#include <model/modelsinc.hh>
#include <io/key.hh>
#include <graphics/2d.hh>
#include <render/3dinternal.hh>
#include <globalincs/linklist.hh>
#include <io/timer.hh>
#include <freespace2/freespace.hh> // For flFrameTime
#include <math/fvi.hh>

#define MAX_SUBMODEL_COLLISION_ROT_ANGLE (PI / 6.0f) // max 30 degrees per frame

// info for special polygon lists

polymodel *Polygon_models[MAX_POLYGON_MODELS];

static int model_initted = 0;

// The POF container reader -- read_model_file() and its prop-parsing helpers
// (get_user_prop_value/set_subsystem_info/do_new_subsystem), model_calc_bound_box,
// and their debug globals -- now live in pofparse.cpp (see docs/pof-model.md).
// This file keeps model_load, the Polygon_models table, and post-processing.

static int Model_signature = 0;

// Free up a model, getting rid of all its memory
// Can't be called from outside of model code because more
// than one person might be using this model so we can't free
// it.
static void
model_unload(int modelnum)
{
    int i, j;

    if ((modelnum < 0) || (modelnum > MAX_POLYGON_MODELS)) {
        return;
    }

    polymodel *pm = Polygon_models[modelnum];

    if (!pm) {
        return;
    }

    if (pm->paths) {
        for (i = 0; i < pm->n_paths; i++) {
            for (j = 0; j < pm->paths[i].nverts; j++) {
                if (pm->paths[i].verts[j].turret_ids) {
                    free(pm->paths[i].verts[j].turret_ids);
                }
            }
            if (pm->paths[i].verts) {
                free(pm->paths[i].verts);
            }
        }
        free(pm->paths);
    }

    if (pm->shield.verts) {
        free(pm->shield.verts);
    }

    if (pm->shield.tris) {
        free(pm->shield.tris);
    }

    if (pm->missile_banks) {
        free(pm->missile_banks);
    }

    if (pm->docking_bays) {
        for (i = 0; i < pm->n_docks; i++) {
            if (pm->docking_bays[i].splines) {
                free(pm->docking_bays[i].splines);
            }
        }
        free(pm->docking_bays);
    }

    if (pm->thrusters) {
        free(pm->thrusters);
    }

#ifndef NDEBUG
    if (pm->debug_info) {
        free(pm->debug_info);
    }
#endif

    model_octant_free(pm);

    if (pm->submodel) {
        for (i = 0; i < pm->n_models; i++) {
            if (pm->submodel[i].bsp_data) {
                free(pm->submodel[i].bsp_data);
            }
        }
        free(pm->submodel);
    }

    if (pm->xc) {
        free(pm->xc);
    }

    if (pm->lights) {
        free(pm->lights);
    }

    if (pm->gun_banks) {
        free(pm->gun_banks);
    }

    pm->id = 0;
    memset(pm, 0, sizeof(polymodel));
    free(pm);

    Polygon_models[modelnum] = NULL;
}

void
model_free_all()
{
    int i;

    if (!model_initted) {
        model_init();
        return;
    }

    mprintf(("Freeing all existing models...\n"));

    for (i = 0; i < MAX_POLYGON_MODELS; i++) {
        model_unload(i);
    }
}

void
model_init()
{
    int i;

    if (model_initted) {
        Int3(); // Model_init shouldn't be called twice!
        return;
    }

    for (i = 0; i < MAX_POLYGON_MODELS; i++) {
        Polygon_models[i] = NULL;
    }

    // Init the model caching system
    model_cache_init();

    atexit(model_free_all);
    model_initted = 1;
}

void
print_family_tree(polymodel *obj, int modelnum, char *ident, int islast)
{
    char temp[50];

    if (modelnum < 0)
        return;
    if (obj == NULL)
        return;

    if (strlen(ident) == 0) {
        mprintf((" %s", obj->submodel[modelnum].name));
        sprintf(temp, " ");
    }
    else if (islast) {
        mprintf(("%s\xc0\xc4%s", ident, obj->submodel[modelnum].name));
        sprintf(temp, "%s  ", ident);
    }
    else {
        mprintf(("%s\xc3\xc4%s", ident, obj->submodel[modelnum].name));
        sprintf(temp, "%s\xb3 ", ident);
    }

    mprintf(("\n"));

    int child = obj->submodel[modelnum].first_child;
    while (child > -1) {
        if (obj->submodel[child].next_sibling < 0)
            print_family_tree(obj, child, temp, 1);
        else
            print_family_tree(obj, child, temp, 0);
        child = obj->submodel[child].next_sibling;
    }
}

void
dump_object_tree(polymodel *obj)
{
    print_family_tree(obj, 0, "", 0);
    key_getch();
}

void
create_family_tree(polymodel *obj)
{
    int i;
    for (i = 0; i < obj->n_models; i++) {
        obj->submodel[i].num_children = 0;
        obj->submodel[i].first_child = -1;
        obj->submodel[i].next_sibling = -1;
    }

    for (i = 0; i < obj->n_models; i++) {
        int pn;
        pn = obj->submodel[i].parent;
        if (pn > -1) {
            obj->submodel[pn].num_children++;
            int tmp = obj->submodel[pn].first_child;
            obj->submodel[pn].first_child = i;
            obj->submodel[i].next_sibling = tmp;
        }
    }
}

// used in collision code to check if submode rotates too far
float
get_submodel_delta_angle(submodel_instance_info *sii)
{
    vector diff;
    vm_vec_sub(&diff, (vector *)&sii->angs, (vector *)&sii->prev_angs);

    // find the angle
    float delta_angle = vm_vec_mag(&diff);

    // make sure we get the short way around
    if (delta_angle > PI) {
        delta_angle = (PI2 - delta_angle);
    }

    return delta_angle;
}

// Bind the POF's texture names (recorded by read_model_file) to bitmap-manager
// handles.  This is the consumer side of the texture seam: it is kept here,
// beside model_load, rather than inside the reader, so read_model_file has no
// bmpman dependency.  Logic is unchanged from the old inline ID_TXTR case.
void
model_load_textures(polymodel *pm)
{
    for (int i = 0; i < pm->n_textures; i++) {
        char *tmp_name = pm->texture_file[i];

        if (strstr(tmp_name, "thruster") || strstr(tmp_name, "invisible")) {
            // Don't load textures for thruster animations or invisible textures
            pm->textures[i] = -1;
        }
        else {
            pm->textures[i] = bm_load(tmp_name);
            if (pm->textures[i] < 0) {
                Error(LOCATION,
                      "Couldn't open texture '%s'\nreferenced by model '%s'\n",
                      tmp_name, pm->filename);
            }
        }
        pm->original_textures[i] = pm->textures[i];
    }
}

//returns the number of this model
int
model_load(char *filename, int n_subsystems, model_subsystem *subsystems)
{
    int i, num, arc_idx;

    if (!model_initted)
        model_init();

    //Assert(strlen(filename) <= 12);

    num = -1;

    for (i = 0; i < MAX_POLYGON_MODELS; i++) {
        if (Polygon_models[i]) {
            if (!stricmp(filename, Polygon_models[i]->filename)) {
                // Model already loaded; just return.
                return Polygon_models[i]->id;
            }
        }
        else if (num == -1) {
            // This is the first empty slot
            num = i;
        }
    }

    // No empty slot
    if (num == -1) {
        Error(LOCATION, "Too many models");
        return -1;
    }

    mprintf(("Loading model '%s'\n", filename));

    polymodel *pm = (polymodel *)malloc(sizeof(polymodel));

    Polygon_models[num] = pm;

    memset(pm, 0, sizeof(polymodel));

    pm->n_paths = 0;
    pm->paths = NULL;

    int org_sig = Model_signature;
    Model_signature += MAX_POLYGON_MODELS;
    if (Model_signature < org_sig) {
        Model_signature = 0;
    }
    Assert((Model_signature % MAX_POLYGON_MODELS) == 0);
    pm->id = Model_signature + num;

    if (!read_model_file(pm, filename, n_subsystems, subsystems)) {
        return -1;
    }

    // bind texture names -> bitmap handles (deferred out of the reader; see
    // model_load_textures).  Ordering matches retail: textures were bound during
    // the read, before the post-processing below, and nothing below needs them.
    model_load_textures(pm);

    //mprintf(( "Loading model '%s'\n", filename ));
    //key_getch();

    //=============================
    // Find the destroyed replacement models

    // Set up the default values
    for (i = 0; i < pm->n_models; i++) {
        pm->submodel[i].my_replacement = -1; // assume nothing replaces this
        pm->submodel[i].i_replace = -1; // assume this doesn't replaces anything
    }

    // Search for models that have destroyed versions
    for (i = 0; i < pm->n_models; i++) {
        int j;
        char destroyed_name[128];

        strcpy(destroyed_name, pm->submodel[i].name);
        strcat(destroyed_name, "-destroyed");
        for (j = 0; j < pm->n_models; j++) {
            if (!stricmp(pm->submodel[j].name, destroyed_name)) {
                // mprintf(( "Found destroyed model for '%s'\n", pm->submodel[i].name ));
                pm->submodel[i].my_replacement = j;
                pm->submodel[j].i_replace = i;
            }
        }

        // Search for models with live debris
        // This debris comes from a destroyed subsystem when ship is still alive
        char live_debris_name[128];

        strcpy(live_debris_name, "debris-");
        strcat(live_debris_name, pm->submodel[i].name);

        pm->submodel[i].num_live_debris = 0;
        for (j = 0; j < pm->n_models; j++) {
            // check if current model name is substring of destroyed
            if (strstr(pm->submodel[j].name, live_debris_name)) {
                mprintf(
                    ("Found live debris model for '%s'\n", pm->submodel[i].name));
                Assert(pm->submodel[i].num_live_debris < MAX_LIVE_DEBRIS);
                pm->submodel[i].live_debris[pm->submodel[i].num_live_debris++] = j;
                pm->submodel[j].is_live_debris = 1;
            }
        }
    }

    create_family_tree(pm);
    //	dump_object_tree(pm);

    //==============================
    // Find all the lower detail versions of the hires model
    for (i = 0; i < pm->n_models; i++) {
        int j, l1;
        bsp_info *sm1 = &pm->submodel[i];

        // set all arc types to be default
        for (arc_idx = 0; arc_idx < MAX_ARC_EFFECTS; arc_idx++) {
            sm1->arc_type[arc_idx] = MARC_TYPE_NORMAL;
        }

        sm1->num_details = 0;
        l1 = strlen(sm1->name);

        for (j = 0; j < pm->num_debris_objects; j++) {
            if (i == pm->debris_objects[j]) {
                sm1->is_damaged = 1;
            }
        }

        for (j = 0; j < MAX_MODEL_DETAIL_LEVELS; j++) {
            sm1->details[j] = -1;
        }

        for (j = 0; j < pm->n_models; j++) {
            int k;
            bsp_info *sm2 = &pm->submodel[j];

            if (i == j)
                continue;

            // set all arc types to be default
            for (arc_idx = 0; arc_idx < MAX_ARC_EFFECTS; arc_idx++) {
                sm2->arc_type[arc_idx] = MARC_TYPE_NORMAL;
            }

            // if sm2 is a detail of sm1 and sm1 is a high detail, then add it to sm1's list
            if ((int)strlen(sm2->name) != l1)
                continue;

            int ndiff = 0;
            int first_diff = 0;
            for (k = 0; k < l1; k++) {
                if (sm1->name[k] != sm2->name[k]) {
                    if (ndiff == 0)
                        first_diff = k;
                    ndiff++;
                }
            }
            if (ndiff == 1) { // They only differ by one character!
                int dl1, dl2;
                dl1 = tolower(sm1->name[first_diff]) - 'a';
                dl2 = tolower(sm2->name[first_diff]) - 'a';

                if ((dl1 < 0) || (dl2 < 0) || (dl1 >= MAX_MODEL_DETAIL_LEVELS) ||
                    (dl2 >= MAX_MODEL_DETAIL_LEVELS))
                    continue; // invalid detail levels

                if (dl1 == 0) {
                    dl2--; // Start from 1 up...
                    if (dl2 >= sm1->num_details)
                        sm1->num_details = dl2 + 1;
                    sm1->details[dl2] = j;
                    //mprintf(( "Submodel '%s' is detail level %d of '%s'\n", sm2->name, dl2, sm1->name ));
                }
            }
        }

        for (j = 0; j < sm1->num_details; j++) {
            if (sm1->details[j] == -1) {
                Warning(
                    LOCATION,
                    "Model '%s' could find all detail levels for submodel '%s'",
                    pm->filename, sm1->name);
                sm1->num_details = 0;
            }
        }
    }

    model_octant_create(pm);

    // Find the core_radius... the minimum of
    float rx, ry, rz;
    rx = fl_abs(pm->submodel[pm->detail[0]].max.x -
                pm->submodel[pm->detail[0]].min.x);
    ry = fl_abs(pm->submodel[pm->detail[0]].max.y -
                pm->submodel[pm->detail[0]].min.y);
    rz = fl_abs(pm->submodel[pm->detail[0]].max.z -
                pm->submodel[pm->detail[0]].min.z);

    pm->core_radius = min(rx, min(ry, rz)) / 2.0f;

    for (i = 0; i < pm->n_view_positions; i++) {
        if (pm->view_positions[i].parent == pm->detail[0]) {
            float d = vm_vec_mag(&pm->view_positions[i].pnt);

            d += 0.1f; // Make the eye 1/10th of a meter inside the sphere.

            if (d > pm->core_radius) {
                //mprintf(( "Model %s core radius increased from %.1f to %.1f to fit eye\n", pm->filename, pm->core_radius, d ));
                pm->core_radius = d;
            }
        }
    }

    return pm->id;
}

// Get "parent" submodel for live debris submodel
int
model_get_parent_submodel_for_live_debris(int model_num,
                                          int live_debris_model_num)
{
    polymodel *pm = model_get(model_num);

    Assert(pm->submodel[live_debris_model_num].is_live_debris == 1);

    int mn;
    bsp_info *child;

    // Start with the high level of detail hull
    // Check all its children until we find the submodel to which the live debris belongs
    child = &pm->submodel[pm->detail[0]];
    mn = child->first_child;

    while (mn > 0) {
        child = &pm->submodel[mn];

        if (child->num_live_debris > 0) {
            // check all live debris submodels for the current child
            for (int idx = 0; idx < child->num_live_debris; idx++) {
                if (child->live_debris[idx] == live_debris_model_num) {
                    return mn;
                }
            }
            // DKA 5/26/99: can multiple live debris subsystems with each ship
            // NO LONGER TRUE Can only be 1 submodel with live debris
            // Error( LOCATION, "Could not find parent submodel for live debris.  Possible model error");
        }

        // get next child
        mn = child->next_sibling;
    }
    Error(LOCATION, "Could not find parent submodel for live debris");
    return -1;
}

float
model_get_radius(int modelnum)
{
    polymodel *pm;

    pm = model_get(modelnum);

    return pm->rad;
}

float
model_get_core_radius(int modelnum)
{
    polymodel *pm;

    pm = model_get(modelnum);

    return pm->core_radius;
}

float
submodel_get_radius(int modelnum, int submodelnum)
{
    polymodel *pm;

    pm = model_get(modelnum);

    return pm->submodel[submodelnum].rad;
}

polymodel *
model_get(int model_num)
{
    Assert(model_num > -1);

    int num = model_num % MAX_POLYGON_MODELS;

    Assert(num > -1);
    Assert(num < MAX_POLYGON_MODELS);
    Assert(Polygon_models[num]->id == model_num);

    return Polygon_models[num];
}

/*
// Finds the 3d bounding box of a model.  If submodel_num is -1,
// then it starts from the root object.   If inc_children is non-zero, 
// then this will recurse and find the bounding box for all children
// also.
void model_find_bound_box_3d(int model_num,int submodel_num, int inc_children, matrix *orient, vector * pos, vector * box )
{
	polymodel * pm;
	vector to_root_xlat;
	matrix to_root_rotate;
	int n_steps, steps[16];
	int tmp_sobj;
	
	if ( (model_num < 0) || (model_num >= N_polygon_models) ) return;

	pm = &Polygon_models[model_num];

	if ( submodel_num < 0 ) submodel_num = pm->detail[0];

	// traverse up the model tree to a root object.
	// Store this path in n_steps,
	n_steps = 0;
	tmp_sobj = submodel_num;
	while( tmp_sobj > -1 )	{
		steps[n_steps++] = tmp_sobj;
		tmp_sobj = pm->submodel[tmp_sobj].parent;
	}
	
	

//	vm_copy_transpose_matrix(&to_world_rotate, orient );
//	to_world_xlat = *pos;

}
*/

// Returns zero is x1,y1,x2,y2 are valid
// returns 1 for invalid model, 2 for point offscreen.
// note that x1,y1,x2,y2 aren't clipped to 2d screen coordinates!
int
model_find_2d_bound_min(int model_num, matrix *orient, vector *pos, int *x1,
                        int *y1, int *x2, int *y2)
{
    polymodel *po;
    int n_valid_pts;
    int i, x, y, min_x, min_y, max_x, max_y;
    int rval = 0;

    po = model_get(model_num);

    g3_start_instance_matrix(pos, orient);

    n_valid_pts = 0;

    int hull = po->detail[0];

    min_x = min_y = max_x = max_y = 0;

    for (i = 0; i < 8; i++) {
        vertex pt;
        ubyte flags;

        flags = g3_rotate_vertex(&pt, &po->submodel[hull].bounding_box[i]);
        if (!(flags & CC_BEHIND)) {
            g3_project_vertex(&pt);

            if (!(pt.flags & PF_OVERFLOW)) {
                x = fl2i(pt.sx);
                y = fl2i(pt.sy);
                if (n_valid_pts == 0) {
                    min_x = x;
                    min_y = y;
                    max_x = x;
                    max_y = y;
                }
                else {
                    if (x < min_x)
                        min_x = x;
                    if (y < min_y)
                        min_y = y;

                    if (x > max_x)
                        max_x = x;
                    if (y > max_y)
                        max_y = y;
                }
                n_valid_pts++;
            }
        }
    }

    if (n_valid_pts < 8) {
        rval = 2;
    }

    if (x1)
        *x1 = min_x;
    if (y1)
        *y1 = min_y;

    if (x2)
        *x2 = max_x;
    if (y2)
        *y2 = max_y;

    g3_done_instance();

    return rval;
}

// Returns zero is x1,y1,x2,y2 are valid
// returns 1 for invalid model, 2 for point offscreen.
// note that x1,y1,x2,y2 aren't clipped to 2d screen coordinates!
int
submodel_find_2d_bound_min(int model_num, int submodel, matrix *orient,
                           vector *pos, int *x1, int *y1, int *x2, int *y2)
{
    polymodel *po;
    int n_valid_pts;
    int i, x, y, min_x, min_y, max_x, max_y;
    bsp_info *sm;

    po = model_get(model_num);
    if ((submodel < 0) || (submodel >= po->n_models))
        return 1;
    sm = &po->submodel[submodel];

    g3_start_instance_matrix(pos, orient);

    n_valid_pts = 0;

    min_x = min_y = max_x = max_y = 0;

    for (i = 0; i < 8; i++) {
        vertex pt;
        ubyte flags;

        flags = g3_rotate_vertex(&pt, &sm->bounding_box[i]);
        if (!(flags & CC_BEHIND)) {
            g3_project_vertex(&pt);

            if (!(pt.flags & PF_OVERFLOW)) {
                x = fl2i(pt.sx);
                y = fl2i(pt.sy);
                if (n_valid_pts == 0) {
                    min_x = x;
                    min_y = y;
                    max_x = x;
                    max_y = y;
                }
                else {
                    if (x < min_x)
                        min_x = x;
                    if (y < min_y)
                        min_y = y;

                    if (x > max_x)
                        max_x = x;
                    if (y > max_y)
                        max_y = y;
                }
                n_valid_pts++;
            }
        }
    }

    if (n_valid_pts == 0) {
        return 2;
    }

    if (x1)
        *x1 = min_x;
    if (y1)
        *y1 = min_y;

    if (x2)
        *x2 = max_x;
    if (y2)
        *y2 = max_y;

    g3_done_instance();

    return 0;
}

// Returns zero is x1,y1,x2,y2 are valid
// returns 1 for invalid model, 2 for point offscreen.
// note that x1,y1,x2,y2 aren't clipped to 2d screen coordinates!
int
model_find_2d_bound(int model_num, matrix *orient, vector *pos, int *x1, int *y1,
                    int *x2, int *y2)
{
    float t, w, h;
    vertex pnt;
    ubyte flags;
    polymodel *po;

    po = model_get(model_num);
    float width = po->rad;
    float height = po->rad;

    flags = g3_rotate_vertex(&pnt, pos);

    if (pnt.flags & CC_BEHIND)
        return 2;

    if (!(pnt.flags & PF_PROJECTED))
        g3_project_vertex(&pnt);

    if (pnt.flags & PF_OVERFLOW)
        return 2;

    t = (width * Canv_w2) / pnt.z;
    w = t * Matrix_scale.x;

    t = (height * Canv_h2) / pnt.z;
    h = t * Matrix_scale.y;

    if (x1)
        *x1 = fl2i(pnt.sx - w);
    if (y1)
        *y1 = fl2i(pnt.sy - h);

    if (x2)
        *x2 = fl2i(pnt.sx + w);
    if (y2)
        *y2 = fl2i(pnt.sy + h);

    return 0;
}

// Returns zero is x1,y1,x2,y2 are valid
// returns 2 for point offscreen.
// note that x1,y1,x2,y2 aren't clipped to 2d screen coordinates!
int
subobj_find_2d_bound(float radius, matrix *orient, vector *pos, int *x1, int *y1,
                     int *x2, int *y2)
{
    float t, w, h;
    vertex pnt;
    ubyte flags;

    float width = radius;
    float height = radius;

    flags = g3_rotate_vertex(&pnt, pos);

    if (pnt.flags & CC_BEHIND)
        return 2;

    if (!(pnt.flags & PF_PROJECTED))
        g3_project_vertex(&pnt);

    if (pnt.flags & PF_OVERFLOW)
        return 2;

    t = (width * Canv_w2) / pnt.z;
    w = t * Matrix_scale.x;

    t = (height * Canv_h2) / pnt.z;
    h = t * Matrix_scale.y;

    if (x1)
        *x1 = fl2i(pnt.sx - w);
    if (y1)
        *y1 = fl2i(pnt.sy - h);

    if (x2)
        *x2 = fl2i(pnt.sx + w);
    if (y2)
        *y2 = fl2i(pnt.sy + h);

    return 0;
}

// Given a vector that is in sub_model_num's frame of
// reference, and given the object's orient and position,
// return the vector in the model's frame of reference.
void
model_find_obj_dir(vector *w_vec, vector *m_vec, object *ship_obj,
                   int sub_model_num)
{
    vector tvec, vec;
    matrix m;
    int mn;

    Assert(ship_obj->type == OBJ_SHIP);

    polymodel *pm = model_get(Ships[ship_obj->instance].modelnum);
    vec = *m_vec;
    mn = sub_model_num;

    // instance up the tree for this point
    while ((mn > -1) && (pm->submodel[mn].parent > -1)) {
        vm_angles_2_matrix(&m, &pm->submodel[mn].angs);
        vm_vec_unrotate(&tvec, &vec, &m);
        vec = tvec;

        mn = pm->submodel[mn].parent;
    }

    // now instance for the entire object
    vm_vec_unrotate(w_vec, &vec, &ship_obj->orient);
}

// Given a point (pnt) that is in sub_model_num's frame of
// reference, return the point in in the object's frame of reference
void
model_rot_sub_into_obj(vector *outpnt, vector *mpnt, polymodel *pm,
                       int sub_model_num)
{
    vector pnt;
    vector tpnt;
    matrix m;
    int mn;

    pnt = *mpnt;
    mn = sub_model_num;

    //instance up the tree for this point
    while ((mn > -1) && (pm->submodel[mn].parent > -1)) {
        vm_angles_2_matrix(&m, &pm->submodel[mn].angs);
        vm_transpose_matrix(&m);
        vm_vec_rotate(&tpnt, &pnt, &m);

        vm_vec_add(&pnt, &tpnt, &pm->submodel[mn].offset);

        mn = pm->submodel[mn].parent;
    }

    //now instance for the entire object
    *outpnt = pnt;
}

// Given a rotating submodel, find the ship and world axes or rotatation.
void
model_get_rotating_submodel_axis(vector *model_axis, vector *world_axis,
                                 int modelnum, int submodel_num, object *obj)
{
    polymodel *pm = model_get(modelnum);

    bsp_info *sm = &pm->submodel[submodel_num];
    Assert(sm->movement_type == MOVEMENT_TYPE_ROT);

    if (sm->movement_axis == MOVEMENT_AXIS_X) {
        vm_vec_make(model_axis, 1.0f, 0.0f, 0.0f);
    }
    else if (sm->movement_axis == MOVEMENT_AXIS_Y) {
        vm_vec_make(model_axis, 0.0f, 1.0f, 0.0f);
    }
    else {
        Assert(sm->movement_axis == MOVEMENT_AXIS_Z);
        vm_vec_make(model_axis, 0.0f, 0.0f, 1.0f);
    }

    model_find_obj_dir(world_axis, model_axis, obj, submodel_num);
}

// Does stepped rotation of a submodel
#pragma warning(push)
#pragma warning(disable : 4701)
void
submodel_stepped_rotate(model_subsystem *psub, submodel_instance_info *sii)
{
    Assert(psub->flags & MSS_FLAG_STEPPED_ROTATE);

    if (psub->subobj_num < 0)
        return;

    polymodel *pm = model_get(psub->model_num);
    bsp_info *sm = &pm->submodel[psub->subobj_num];

    if (sm->movement_type != MOVEMENT_TYPE_ROT)
        return;

    // get active rotation time this frame
    int end_stamp = timestamp();
    float rotation_time = 0.001f * (end_stamp - sii->step_zero_timestamp);
    Assert(rotation_time >= 0);

    // save last angles
    sii->prev_angs = sii->angs;

    // float pointer into struct to get angle (either p,b,h)
    float *ang_prev, *ang_next;
    switch (sm->movement_axis) {
    case MOVEMENT_AXIS_X:
        ang_prev = &sii->prev_angs.p;
        ang_next = &sii->angs.p;
        break;

    case MOVEMENT_AXIS_Y:
        ang_prev = &sii->prev_angs.h;
        ang_next = &sii->angs.h;
        break;

    case MOVEMENT_AXIS_Z:
        ang_prev = &sii->prev_angs.b;
        ang_next = &sii->angs.b;
        break;
    }

    // angular displacement of one step
    float step_size = (PI2 / psub->stepped_rotation->num_steps);

    // get time to complete one step, including pause
    float step_time = psub->stepped_rotation->t_transit +
                      psub->stepped_rotation->t_pause;

    // cur_step is step number relative to zero (0 - num_steps)
    // step_offset_time is TIME into current step
    float step_offset_time = (float)fmod(rotation_time, step_time);
    // subtract off fractional step part, round up  (ie, 1.999999 -> 2)
    int cur_step = int(((rotation_time - step_offset_time) / step_time) + 0.5f);
    // mprintf(("cur step %d\n", cur_step));
    // Assert(step_offset_time >= 0);

    if (cur_step >= psub->stepped_rotation->num_steps) {
        // I don;t know why, but removing this line makes it all good.
        // sii->step_zero_timestamp += int(1000.0f * (psub->stepped_rotation->num_steps * step_time) + 0.5f);

        // reset cur_step (use mod to handle physics/ai pause)
        cur_step = cur_step % psub->stepped_rotation->num_steps;
    }

    // get base angle
    *ang_next = cur_step * step_size;

    // determine which phase of rotation we're in
    float coast_start_time = psub->stepped_rotation->fraction *
                             psub->stepped_rotation->t_transit;
    float decel_start_time = psub->stepped_rotation->t_transit *
                             (1.0f - psub->stepped_rotation->fraction);
    float pause_start_time = psub->stepped_rotation->t_transit;

    float start_coast_angle = 0.5f * psub->stepped_rotation->max_turn_accel *
                              coast_start_time * coast_start_time;

    if (step_offset_time < coast_start_time) {
        // do accel
        float accel_time = step_offset_time;
        *ang_next += 0.5f * psub->stepped_rotation->max_turn_accel * accel_time *
                     accel_time;
        sii->cur_turn_rate = psub->stepped_rotation->max_turn_accel * accel_time;
    }
    else if (step_offset_time < decel_start_time) {
        // do coast
        float coast_time = step_offset_time - coast_start_time;
        *ang_next += start_coast_angle +
                     psub->stepped_rotation->max_turn_rate * coast_time;
        sii->cur_turn_rate = psub->stepped_rotation->max_turn_rate;
    }
    else if (step_offset_time < pause_start_time) {
        // do decel
        float time_to_pause = psub->stepped_rotation->t_transit -
                              step_offset_time;
        *ang_next += (step_size - 0.5f * psub->stepped_rotation->max_turn_accel *
                                      time_to_pause * time_to_pause);
        sii->cur_turn_rate = psub->stepped_rotation->max_turn_rate *
                             time_to_pause;
    }
    else {
        // do pause
        *ang_next += step_size;
        sii->cur_turn_rate = 0.0f;
    }
}
#pragma warning(pop)

// Rotates the angle of a submodel.  Use this so the right unlocked axis
// gets stuffed.
void
submodel_rotate(model_subsystem *psub, submodel_instance_info *sii)
{
    bsp_info *sm;

    if (psub->subobj_num < 0)
        return;

    polymodel *pm = model_get(psub->model_num);
    sm = &pm->submodel[psub->subobj_num];

    if (sm->movement_type != MOVEMENT_TYPE_ROT)
        return;

    // save last angles
    sii->prev_angs = sii->angs;

    // probably send in a calculated desired turn rate
    float diff = sii->desired_turn_rate - sii->cur_turn_rate;

    float final_turn_rate;
    if (diff > 0) {
        final_turn_rate = sii->cur_turn_rate + sii->turn_accel * flFrametime;
        if (final_turn_rate > sii->desired_turn_rate) {
            final_turn_rate = sii->desired_turn_rate;
        }
    }
    else if (diff < 0) {
        final_turn_rate = sii->cur_turn_rate - sii->turn_accel * flFrametime;
        if (final_turn_rate < sii->desired_turn_rate) {
            final_turn_rate = sii->desired_turn_rate;
        }
    }
    else {
        final_turn_rate = sii->desired_turn_rate;
    }

    float delta = (sii->cur_turn_rate + final_turn_rate) * 0.5f * flFrametime;
    sii->cur_turn_rate = final_turn_rate;

    //float delta = psub->turn_rate * flFrametime;

    switch (sm->movement_axis) {
    case MOVEMENT_AXIS_X:
        sii->angs.p += delta;
        if (sii->angs.p > PI2)
            sii->angs.p -= PI2;
        else if (sii->angs.p < 0.0f)
            sii->angs.p += PI2;
        break;
    case MOVEMENT_AXIS_Y:
        sii->angs.h += delta;
        if (sii->angs.h > PI2)
            sii->angs.h -= PI2;
        else if (sii->angs.h < 0.0f)
            sii->angs.h += PI2;
        break;
    case MOVEMENT_AXIS_Z:
        sii->angs.b += delta;
        if (sii->angs.b > PI2)
            sii->angs.b -= PI2;
        else if (sii->angs.b < 0.0f)
            sii->angs.b += PI2;
        break;
    }
}

//=========================================================================
// Make a turret's correct orientation matrix.   This should be done when
// the model is read, but I wasn't sure at what point all the data that I
// needed was read, so I just check a flag and call this routine when
// I determine I need the correct matrix.   In this code, you can't use
// vm_vec_2_matrix or anything, since these turrets could be either
// right handed or left handed.
void
model_make_turrent_matrix(int model_num, model_subsystem *turret)
{
    polymodel *pm;
    vector fvec, uvec, rvec;

    pm = model_get(model_num);
    bsp_info *sm = &pm->submodel[turret->turret_gun_sobj];
    bsp_info *sm_parent = &pm->submodel[turret->subobj_num];

    model_clear_instance(model_num);
    model_find_world_dir(&fvec, &turret->turret_norm, model_num,
                         turret->turret_gun_sobj, &vmd_identity_matrix, NULL);

    sm_parent->angs.h = -PI / 2.0f;
    sm->angs.p = -PI / 2.0f;
    model_find_world_dir(&rvec, &turret->turret_norm, model_num,
                         turret->turret_gun_sobj, &vmd_identity_matrix, NULL);

    sm_parent->angs.h = 0.0f;
    sm->angs.p = -PI / 2.0f;
    model_find_world_dir(&uvec, &turret->turret_norm, model_num,
                         turret->turret_gun_sobj, &vmd_identity_matrix, NULL);

    vm_vec_normalize(&fvec);
    vm_vec_normalize(&rvec);
    vm_vec_normalize(&uvec);

    turret->turret_matrix.fvec = fvec;
    turret->turret_matrix.rvec = rvec;
    turret->turret_matrix.uvec = uvec;

    //	vm_vector_2_matrix(&turret->turret_matrix,&turret->turret_norm,NULL,NULL);

    // HACK!! WARNING!!!
    // I'm doing nothing to verify that this matrix is orthogonal!!
    // In other words, there's no guarantee that the vectors are 90 degrees
    // from each other.
    // I'm not doing this because I don't know how to do it without ruining
    // the handedness of the matrix... however, I'm not too worried about
    // this because I am creating these 3 vectors by making them 90 degrees
    // apart, so this should be close enough.  I think this will start
    // causing weird errors when we view from turrets. -John
    turret->flags |= MSS_FLAG_TURRET_MATRIX;
}

// Tries to move joints so that the turrent points to the point dst.
// turret1 is the angles of the turret, turret2 is the angles of the gun from turret
//	Returns 1 if rotated gun, 0 if no gun to rotate (rotation handled by AI)
int
model_rotate_gun(int model_num, model_subsystem *turret, matrix *orient,
                 angles *turret1, angles *turret2, vector *pos, vector *dst)
{
    polymodel *pm;

    pm = model_get(model_num);
    bsp_info *sm = &pm->submodel[turret->turret_gun_sobj];
    bsp_info *sm_parent = &pm->submodel[turret->subobj_num];

    // Check for a valid turret
    Assert(turret->turret_num_firing_points > 0);

    if (sm_parent == sm) {
        return 0;
    }

    // Build the correct turret matrix if there isn't already one
    if (!(turret->flags & MSS_FLAG_TURRET_MATRIX))
        model_make_turrent_matrix(model_num, turret);

    Assert(turret->flags & MSS_FLAG_TURRET_MATRIX);
//	Assert( sm->movement_axis == MOVEMENT_AXIS_X );				// Gun must be able to change pitch
//	Assert( sm_parent->movement_axis == MOVEMENT_AXIS_Z );	// Parent must be able to change heading

//======================================================
// DEBUG code to draw the normal out of this gun and a circle
// at the gun point.
#if 0
	{
		vector tmp;
		vector tmp1;
		vertex dpnt1, dpnt2;

		model_clear_instance(model_num);
		sm->angs.p = turret2->p;
		sm_parent->angs.h = turret1->h;

		model_find_world_point(&tmp, &vmd_zero_vector, model_num, turret->turret_gun_sobj, orient, pos );
		gr_set_color(255,0,0);
		g3_rotate_vertex( &dpnt1, &tmp );

		gr_set_color(255,0,0);
		g3_draw_sphere(&dpnt1,1.0f);

		vm_vec_copy_scale( &tmp1, &turret->turret_matrix.fvec, 10.0f );
		model_find_world_point(&tmp, &tmp1, model_num, turret->turret_gun_sobj, orient, pos );
		g3_rotate_vertex( &dpnt2, &tmp );

		gr_set_color(0,255,0);
		g3_draw_line(&dpnt1,&dpnt2);
		gr_set_color(0,128,0);
		g3_draw_sphere(&dpnt2,0.2f);

		vm_vec_copy_scale( &tmp1, &turret->turret_matrix.rvec, 10.0f );
		model_find_world_point(&tmp, &tmp1, model_num, turret->turret_gun_sobj, orient, pos );
		g3_rotate_vertex( &dpnt2, &tmp );

		gr_set_color(0,0,255);
		g3_draw_line(&dpnt1,&dpnt2);

		vm_vec_copy_scale( &tmp1, &turret->turret_matrix.uvec, 10.0f );
		model_find_world_point(&tmp, &tmp1, model_num, turret->turret_gun_sobj, orient, pos );
		g3_rotate_vertex( &dpnt2, &tmp );

		gr_set_color(255,0,0);
		g3_draw_line(&dpnt1,&dpnt2);
	}
#endif

    //------------
    // rotate the dest point into the turret gun normal's frame of
    // reference, but not using the turret's angles.
    // Call this vector of_dst
    vector of_dst;
    matrix world_to_turret_matrix; // converts world coordinates to turret's FOR
    vector world_to_turret_translate; // converts world coordinates to turret's FOR
    vector tempv;

    vm_vec_unrotate(&tempv, &sm_parent->offset, orient);
    vm_vec_add(&world_to_turret_translate, pos, &tempv);

    vm_matrix_x_matrix(&world_to_turret_matrix, orient, &turret->turret_matrix);

    vm_vec_sub(&tempv, dst, &world_to_turret_translate);
    vm_vec_rotate(&of_dst, &tempv, &world_to_turret_matrix);

    vm_vec_normalize(&of_dst);

    //------------
    // Find the heading and pitch that the gun needs to turn to
    // by extracting them from the of_dst vector.
    // Call this the desired_angles
    angles desired_angles;

    desired_angles.p = (float)acos(of_dst.z);
    desired_angles.h = PI - atan2_safe(of_dst.x, of_dst.y);
    desired_angles.b = 0.0f;

    //	mprintf(( "Z = %.1f, atan= %.1f\n", of_dst.z, desired_angles.p ));

    //------------
    // Gradually turn the turret towards the desired angles
    float step_size = turret->turret_turning_rate * flFrametime;

    vm_interp_angle(&turret1->h, desired_angles.h, step_size);
    vm_interp_angle(&turret2->p, desired_angles.p, step_size);

    //	turret1->h -= step_size*(key_down_timef(KEY_1)-key_down_timef(KEY_2) );
    //	turret2->p += step_size*(key_down_timef(KEY_3)-key_down_timef(KEY_4) );

    return 1;
}

// Given a point (pnt) that is in sub_model_num's frame of
// reference, and given the object's orient and position,
// return the point in 3-space in outpnt.
void
model_find_world_point(vector *outpnt, vector *mpnt, int model_num,
                       int sub_model_num, matrix *objorient, vector *objpos)
{
    vector pnt;
    vector tpnt;
    matrix m;
    int mn;
    polymodel *pm = model_get(model_num);

    pnt = *mpnt;
    mn = sub_model_num;

    //instance up the tree for this point
    while ((mn > -1) && (pm->submodel[mn].parent > -1)) {
        vm_angles_2_matrix(&m, &pm->submodel[mn].angs);
        vm_vec_unrotate(&tpnt, &pnt, &m);

        vm_vec_add(&pnt, &tpnt, &pm->submodel[mn].offset);

        mn = pm->submodel[mn].parent;
    }

    //now instance for the entire object
    vm_vec_unrotate(outpnt, &pnt, objorient);
    vm_vec_add2(outpnt, objpos);
}

// Given a point in the world RF, find the corresponding point in the model RF.
// This is special purpose code, specific for model collision.
// NOTE - this code ASSUMES submodel is 1 level down from hull (detail[0])
//
// out - point in model RF
// world_pt - point in world RF
// pm - polygon model
// submodel_num - submodel in whose RF we're trying to find the corresponding world point
// orient - orient matrix of ship
// pos - pos vector of ship
void
world_find_model_point(vector *out, vector *world_pt, polymodel *pm,
                       int submodel_num, matrix *orient, vector *pos)
{
    Assert((pm->submodel[submodel_num].parent == pm->detail[0]) ||
           (pm->submodel[submodel_num].parent == -1));

    vector tempv1, tempv2;
    matrix m;

    // get into ship RF
    vm_vec_sub(&tempv1, world_pt, pos);
    vm_vec_rotate(&tempv2, &tempv1, orient);

    if (pm->submodel[submodel_num].parent == -1) {
        *out = tempv2;
        return;
    }

    // put into submodel RF
    vm_vec_sub2(&tempv2, &pm->submodel[submodel_num].offset);
    vm_angles_2_matrix(&m, &pm->submodel[submodel_num].angs);
    vm_vec_rotate(out, &tempv2, &m);
}

// Verify rotating submodel has corresponding ship subsystem -- info in which to store rotation angle
int
rotating_submodel_has_ship_subsys(int submodel, ship *shipp)
{
    model_subsystem *psub;
    ship_subsys *pss;

    int found = 0;

    // Go through all subsystems and look for submodel
    // the subsystems that need it.
    for (pss = GET_FIRST(&shipp->subsys_list);
         pss != END_OF_LIST(&shipp->subsys_list); pss = GET_NEXT(pss)) {
        psub = pss->system_info;
        if (psub->subobj_num == submodel) {
            found = 1;
            break;
        }
    }

    return found;
}

void
model_get_rotating_submodel_list(int *submodel_list, int *num_rotating_submodels,
                                 object *objp)
{
    Assert(objp->type == OBJ_SHIP);

    // Check if not currently rotating - then treat as part of superstructure.
    int modelnum = Ships[objp->instance].modelnum;
    polymodel *pm = model_get(modelnum);
    bsp_info *child_submodel;

    *num_rotating_submodels = 0;
    child_submodel = &pm->submodel[pm->detail[0]];

    int i = child_submodel->first_child;
    while (i > -1) {
        child_submodel = &pm->submodel[i];

        // Don't check it or its children if it is destroyed or it is a replacement (non-moving)
        if (!child_submodel->blown_off && (child_submodel->i_replace == -1)) {
            // Only look for submodels that rotate
            if (child_submodel->movement_type == MOVEMENT_TYPE_ROT) {
                // find ship subsys and check submodel rotation is less than max allowed.
                ship *pship = &Ships[objp->instance];
                ship_subsys *subsys;

                for (subsys = GET_FIRST(&pship->subsys_list);
                     subsys != END_OF_LIST(&pship->subsys_list);
                     subsys = GET_NEXT(subsys)) {
                    Assert(subsys->system_info->model_num == modelnum);
                    if (i == subsys->system_info->subobj_num) {
                        // found the correct subsystem - now check delta rotation angle not too large
                        float delta_angle = get_submodel_delta_angle(
                            &subsys->submodel_info_1);
                        if (delta_angle < MAX_SUBMODEL_COLLISION_ROT_ANGLE) {
                            Assert(*num_rotating_submodels <
                                   MAX_ROTATING_SUBMODELS - 1);
                            submodel_list[(*num_rotating_submodels)++] = i;
                        }
                        break;
                    }
                }
            }
        }
        i = child_submodel->next_sibling;
    }

    // error checking
//#define MODEL_CHECK
#ifdef MODEL_CHECK
    ship *pship = &Ships[objp->instance];
    for (int idx = 0; idx < *num_rotating_submodels; idx++) {
        int valid = rotating_submodel_has_ship_subsys(submodel_list[idx], pship);
        //		Assert( valid );
        if (!valid) {
            Warning(LOCATION,
                    "Ship %s has rotating submodel [%s] without ship subsystem\n",
                    pship->ship_name, pm->submodel[submodel_list[idx]].name);
            pm->submodel[submodel_list[idx]].movement_type &= ~MOVEMENT_TYPE_ROT;
            *num_rotating_submodels = 0;
        }
    }
#endif
}

// Given a direction (pnt) that is in sub_model_num's frame of
// reference, and given the object's orient and position,
// return the point in 3-space in outpnt.
void
model_find_world_dir(vector *out_dir, vector *in_dir, int model_num,
                     int sub_model_num, matrix *objorient, vector *objpos)
{
    vector pnt;
    vector tpnt;
    matrix m;
    int mn;
    polymodel *pm = model_get(model_num);

    pnt = *in_dir;
    mn = sub_model_num;

    //instance up the tree for this point
    while ((mn > -1) && (pm->submodel[mn].parent > -1)) {
        vm_angles_2_matrix(&m, &pm->submodel[mn].angs);
        vm_vec_unrotate(&tpnt, &pnt, &m);
        pnt = tpnt;

        mn = pm->submodel[mn].parent;
    }

    //now instance for the entire object
    vm_vec_unrotate(out_dir, &pnt, objorient);
}

// Clears all the submodel instances stored in a model to their defaults.
void
model_clear_instance(int model_num)
{
    polymodel *pm;
    int i;

    pm = model_get(model_num);

    // reset textures to original ones
    for (i = 0; i < pm->n_textures; i++) {
        pm->textures[i] = pm->original_textures[i];
    }

    for (i = 0; i < pm->n_models; i++) {
        bsp_info *sm = &pm->submodel[i];

        if (pm->submodel[i].is_damaged) {
            sm->blown_off = 1;
        }
        else {
            sm->blown_off = 0;
        }
        sm->angs.p = 0.0f;
        sm->angs.b = 0.0f;
        sm->angs.h = 0.0f;

        // set pointer to other ship subsystem info [turn rate, accel, moment, axis, ...]
        sm->sii = NULL;

        sm->num_arcs = 0; // Turn off any electric arcing effects
    }

    for (i = 0; i < pm->num_lights; i++) {
        pm->lights[i].value = 0.0f;
    }

    interp_clear_instance();

    //	if ( keyd_pressed[KEY_1] ) pm->lights[0].value = 1.0f/255.0f;
    //	if ( keyd_pressed[KEY_2] ) pm->lights[1].value = 1.0f/255.0f;
    //	if ( keyd_pressed[KEY_3] ) pm->lights[2].value = 1.0f/255.0f;
    //	if ( keyd_pressed[KEY_4] ) pm->lights[3].value = 1.0f/255.0f;
    //	if ( keyd_pressed[KEY_5] ) pm->lights[4].value = 1.0f/255.0f;
    //	if ( keyd_pressed[KEY_6] ) pm->lights[5].value = 1.0f/255.0f;
}

// initialization during ship set
void
model_clear_instance_info(submodel_instance_info *sii)
{
    sii->blown_off = 0;
    sii->angs.p = 0.0f;
    sii->angs.b = 0.0f;
    sii->angs.h = 0.0f;
    sii->prev_angs.p = 0.0f;
    sii->prev_angs.b = 0.0f;
    sii->prev_angs.h = 0.0f;

    sii->cur_turn_rate = 0.0f;
    sii->desired_turn_rate = 0.0f;
    sii->turn_accel = 0.0f;
}

// initialization during ship set
void
model_set_instance_info(submodel_instance_info *sii, float turn_rate,
                        float turn_accel)
{
    sii->blown_off = 0;
    sii->angs.p = 0.0f;
    sii->angs.b = 0.0f;
    sii->angs.h = 0.0f;
    sii->prev_angs.p = 0.0f;
    sii->prev_angs.b = 0.0f;
    sii->prev_angs.h = 0.0f;

    sii->cur_turn_rate = turn_rate * 0.0f;
    sii->desired_turn_rate = turn_rate;
    sii->turn_accel = turn_accel;
    sii->axis_set = 0;
    sii->step_zero_timestamp = timestamp();
}

// Sets the submodel instance data in a submodel (for all detail levels)
void
model_set_instance(int model_num, int sub_model_num, submodel_instance_info *sii)
{
    int i;
    polymodel *pm;

    pm = model_get(model_num);

    Assert(sub_model_num >= 0);
    Assert(sub_model_num < pm->n_models);

    if (sub_model_num < 0)
        return;
    if (sub_model_num >= pm->n_models)
        return;
    bsp_info *sm = &pm->submodel[sub_model_num];

    // Set the "blown out" flags
    sm->blown_off = sii->blown_off;

    if (sm->blown_off) {
        if (sm->my_replacement > -1) {
            pm->submodel[sm->my_replacement].blown_off = 0;
            pm->submodel[sm->my_replacement].angs = sii->angs;
            pm->submodel[sm->my_replacement].sii = sii;
        }
    }
    else {
        if (sm->my_replacement > -1) {
            pm->submodel[sm->my_replacement].blown_off = 1;
        }
    }

    // Set the angles
    sm->angs = sii->angs;
    sm->sii = sii;

    // For all the detail levels of this submodel, set them also.
    for (i = 0; i < sm->num_details; i++) {
        model_set_instance(model_num, sm->details[i], sii);
    }
}

// Finds a point on the rotation axis of a submodel, used in collision, generally find rotational velocity
void
model_init_submodel_axis_pt(submodel_instance_info *sii, int model_num,
                            int submodel_num)
{
    vector axis;
    vector *mpoint1, *mpoint2;
    vector p1, v1, p2, v2, int1;

    polymodel *pm = model_get(model_num);
    Assert(pm->submodel[submodel_num].movement_type == MOVEMENT_TYPE_ROT);
    Assert(sii);

    mpoint1 = NULL;
    mpoint2 = NULL;

    // find 2 fixed points in submodel RF
    // these will be rotated to about the axis an angle of 0 and PI and we'll find the intersection of the
    // two lines to find a point on the axis
    if (pm->submodel[submodel_num].movement_axis == MOVEMENT_AXIS_X) {
        axis = vmd_x_vector;
        mpoint1 = &vmd_y_vector;
        mpoint2 = &vmd_z_vector;
    }
    else if (pm->submodel[submodel_num].movement_axis == MOVEMENT_AXIS_Y) {
        mpoint1 = &vmd_x_vector;
        axis =
            vmd_z_vector; // rotation about y is a change in heading (p,b,h), so we need z
        mpoint2 = &vmd_z_vector;
    }
    else if (pm->submodel[submodel_num].movement_axis == MOVEMENT_AXIS_Z) {
        mpoint1 = &vmd_x_vector;
        mpoint2 = &vmd_y_vector;
        axis =
            vmd_y_vector; // rotation about z is a change in bank (p,b,h), so we need y
    }
    else {
        // must be one of these axes or submodel_rot_hit is incorrectly set
        Int3();
    }

    // copy submodel angs
    angles copy_angs = pm->submodel[submodel_num].angs;

    // find two points rotated into model RF when angs set to 0
    vm_vec_copy_scale((vector *)&pm->submodel[submodel_num].angs, &axis, 0.0f);
    model_find_world_point(&p1, mpoint1, model_num, submodel_num,
                           &vmd_identity_matrix, &vmd_zero_vector);
    model_find_world_point(&p2, mpoint2, model_num, submodel_num,
                           &vmd_identity_matrix, &vmd_zero_vector);

    // find two points rotated into model RF when angs set to PI
    vm_vec_copy_scale((vector *)&pm->submodel[submodel_num].angs, &axis, PI);
    model_find_world_point(&v1, mpoint1, model_num, submodel_num,
                           &vmd_identity_matrix, &vmd_zero_vector);
    model_find_world_point(&v2, mpoint2, model_num, submodel_num,
                           &vmd_identity_matrix, &vmd_zero_vector);

    // reset submodel angs
    pm->submodel[submodel_num].angs = copy_angs;

    // find direction vectors of the two lines
    vm_vec_sub2(&v1, &p1);
    vm_vec_sub2(&v2, &p2);

    // find the intersection of the two lines
    float s, t;
    fvi_two_lines_in_3space(&p1, &v1, &p2, &v2, &s, &t);

    // find the actual intersection points
    vm_vec_scale_add(&int1, &p1, &v1, s);

    // set flag to init
    sii->pt_on_axis = int1;
    sii->axis_set = 1;
}

// Adds an electrical arcing effect to a submodel
void
model_add_arc(int model_num, int sub_model_num, vector *v1, vector *v2,
              int arc_type)
{
    polymodel *pm;

    pm = model_get(model_num);

    if (sub_model_num == -1) {
        sub_model_num = pm->detail[0];
    }

    Assert(sub_model_num >= 0);
    Assert(sub_model_num < pm->n_models);

    if (sub_model_num < 0)
        return;
    if (sub_model_num >= pm->n_models)
        return;
    bsp_info *sm = &pm->submodel[sub_model_num];

    if (sm->num_arcs < MAX_ARC_EFFECTS) {
        sm->arc_type[sm->num_arcs] = (ubyte)arc_type;
        sm->arc_pts[sm->num_arcs][0] = *v1;
        sm->arc_pts[sm->num_arcs][1] = *v2;
        sm->num_arcs++;
    }
}

// function to return an index into the docking_bays array which matches the criteria passed
// to this function.  dock_type is one of the DOCK_TYPE_XXX defines in model.h
int
model_find_dock_index(int modelnum, int dock_type)
{
    int i;

    polymodel *pm;

    pm = model_get(modelnum);

    // no docking points -- return -1
    if (pm->n_docks <= 0)
        return -1;

    for (i = 0; i < pm->n_docks; i++) {
        if (dock_type & pm->docking_bays[i].type_flags)
            return i;
    }

    return -1;
}

int
model_get_dock_index_type(int modelnum, int index)
{
    polymodel *pm = model_get(modelnum);

    return pm->docking_bays[index].type_flags;
}

// get all the different docking point types on a model
int
model_get_dock_types(int modelnum)
{
    int i, type = 0;
    polymodel *pm;

    pm = model_get(modelnum);
    for (i = 0; i < pm->n_docks; i++)
        type |= pm->docking_bays[i].type_flags;

    return type;
}

// function to return an index into the docking_bays array which matches the string passed
// Fred uses strings to identify docking positions.  This functin also accepts generic strings
// so that a desginer doesn't have to know exact names if building a mission from hand.
int
model_find_dock_name_index(int modelnum, char *name)
{
    int i;
    polymodel *pm;

    pm = model_get(modelnum);
    if (pm->n_docks <= 0)
        return -1;

    // check the generic names and call previous function to find first dock point of
    // the specified type
    if (!stricmp(name, "cargo"))
        return model_find_dock_index(modelnum, DOCK_TYPE_CARGO);
    else if (!stricmp(name, "rearm"))
        return model_find_dock_index(modelnum, DOCK_TYPE_REARM);
    else if (!stricmp(name, "generic"))
        return model_find_dock_index(modelnum, DOCK_TYPE_GENERIC);

    for (i = 0; i < pm->n_docks; i++) {
        if (!stricmp(pm->docking_bays[i].name, name))
            return i;
    }

    // if we get here, name wasn't found -- return -1 and hope for the best
    return -1;
}

// returns the actual name of a docking point on a model, needed by Fred.
char *
model_get_dock_name(int modelnum, int index)
{
    polymodel *pm;

    pm = model_get(modelnum);
    Assert((index >= 0) && (index < pm->n_docks));
    return pm->docking_bays[index].name;
}

int
model_get_num_dock_points(int modelnum)
{
    polymodel *pm;

    pm = model_get(modelnum);
    return pm->n_docks;
}
