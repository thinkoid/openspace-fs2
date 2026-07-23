/*
 * pofparse.cpp -- the POF (Parallax Object File) container reader, carved
 * out of modelread.cpp.  This is the "file -> polymodel" half of the model
 * system: it walks the POF chunk container and fills a polymodel (including the
 * opaque per-submodel bsp_data blobs, which it copies verbatim and never
 * parses).  It carries no game-system dependency -- texture binding was already
 * deferred to model_load_textures() (see docs/pof-model.md); everything here is
 * cfile IO, vecmat, and struct-fill, plus the optional subsystem out-parameter
 * population (do_new_subsystem, which NULL-guards and only writes the caller's
 * model_subsystem array).
 *
 * The runtime consumers of the geometry -- modelinterp.cpp (render),
 * modelcollide.cpp (collision), modeloctant.cpp (spatial sort) -- and
 * model_load's post-processing stay in modelread.cpp.  The function bodies below
 * were moved VERBATIM; this is a relocation, not a rewrite.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define MODEL_LIB
#include <globalincs/pstypes.hh>
#include <cfile/cfile.hh>
#include <model/model.hh>
#include <math/vecmat.hh>
#include <model/modelsinc.hh>

// Anything less than this is considered incompatible.
#define PM_COMPATIBLE_VERSION 1900

// Anything greater than or equal to PM_COMPATIBLE_VERSION and
// whose major version is less than or equal to this is considered
// compatible.
#define PM_OBJFILE_MAJOR_VERSION 21

#ifndef NDEBUG
CFILE *ss_fp; // file pointer used to dump subsystem information
char model_filename[_MAX_PATH]; // temp used to store filename
char debug_name[_MAX_PATH];
int ss_warning_shown; // have we shown the warning dialog concerning the subsystems?
char Global_filename[256];
#endif

// routine to parse out values from a user property field of an object
void
get_user_prop_value(char *buf, char *value)
{
    char *p, *p1, c;

    p = buf;
    while (isspace(*p) || (*p == '=')) // skip white space and equal sign
        p++;
    p1 = p;
    while (!iscntrl(*p1))
        p1++;
    c = *p1;
    *p1 = '\0';
    strcpy(value, p);
    *p1 = c;
}

// funciton to copy model data from one subsystem set to another subsystem set.  This function
// is called when two ships use the same model data, but since the model only gets read in one time,
// the subsystem data is only present in one location.  The ship code will call this routine to fix
// this situation by copying stuff from the source subsystem set to the dest subsystem set.
void
model_copy_subsystems(int n_subsystems, model_subsystem *d_sp,
                      model_subsystem *s_sp)
{
    int i, j;
    model_subsystem *source, *dest;

    for (i = 0; i < n_subsystems; i++) {
        source = &s_sp[i];
        for (j = 0; j < n_subsystems; j++) {
            dest = &d_sp[j];
            if (!stricmp(source->subobj_name, dest->subobj_name)) {
                dest->flags = source->flags;
                dest->subobj_num = source->subobj_num;
                dest->model_num = source->model_num;
                dest->pnt = source->pnt;
                dest->radius = source->radius;
                dest->type = source->type;
                dest->turn_rate = source->turn_rate;
                dest->turret_gun_sobj = source->turret_gun_sobj;

                strcpy(dest->name, source->name);

                if (dest->type == SUBSYSTEM_TURRET) {
                    int nfp;

                    dest->turret_fov = source->turret_fov;
                    dest->turret_num_firing_points =
                        source->turret_num_firing_points;
                    dest->turret_norm = source->turret_norm;
                    dest->turret_matrix = source->turret_matrix;

                    for (nfp = 0; nfp < dest->turret_num_firing_points; nfp++)
                        dest->turret_firing_point[nfp] =
                            source->turret_firing_point[nfp];

                    if (dest->flags & MSS_FLAG_CREWPOINT)
                        strcpy(dest->crewspot, source->crewspot);
                }
                break;
            }
        }
        if (j == n_subsystems)
            Int3(); // get allender -- something is amiss with models
    }
}

// routine to get/set subsystem information
static void
set_subsystem_info(model_subsystem *subsystemp, char *props, char *dname)
{
    char *p;
    char buf[32];
    char lcdname[256];

    if ((p = strstr(props, "$name")) != NULL)
        get_user_prop_value(p + 5, subsystemp->name);
    else
        strcpy(subsystemp->name, dname);

    strcpy(lcdname, dname);
    strlwr(lcdname);

    // check the name for it's specific type
    if (strstr(lcdname, "engine")) {
        subsystemp->type = SUBSYSTEM_ENGINE;
    }
    else if (strstr(lcdname, "radar")) {
        subsystemp->type = SUBSYSTEM_RADAR;
    }
    else if (strstr(lcdname, "turret")) {
        float angle;

        subsystemp->type = SUBSYSTEM_TURRET;
        if ((p = strstr(props, "$fov")) != NULL)
            get_user_prop_value(p + 4, buf); // get the value of the fov
        else
            strcpy(buf, "180");
        angle = ANG_TO_RAD(atoi(buf)) / 2.0f;
        subsystemp->turret_fov = (float)cos(angle);
        subsystemp->turret_num_firing_points = 0;

        if ((p = strstr(props, "$crewspot")) != NULL) {
            subsystemp->flags |= MSS_FLAG_CREWPOINT;
            get_user_prop_value(p + 9, subsystemp->crewspot);
        }
    }
    else if (strstr(lcdname, "navigation")) {
        subsystemp->type = SUBSYSTEM_NAVIGATION;
    }
    else if (strstr(lcdname, "communication")) {
        subsystemp->type = SUBSYSTEM_COMMUNICATION;
    }
    else if (strstr(lcdname, "weapons")) {
        subsystemp->type = SUBSYSTEM_WEAPONS;
    }
    else if (strstr(lcdname, "sensors")) {
        subsystemp->type = SUBSYSTEM_SENSORS;
    }
    else if (strstr(lcdname, "solar")) {
        subsystemp->type = SUBSYSTEM_SOLAR;
    }
    else if (strstr(lcdname, "gas")) {
        subsystemp->type = SUBSYSTEM_GAS_COLLECT;
    }
    else if (strstr(lcdname, "activator")) {
        subsystemp->type = SUBSYSTEM_ACTIVATION;
    }
    else { // If unrecognized type, set to unknown so artist can continue working...
        subsystemp->type = SUBSYSTEM_UNKNOWN;
        mprintf((
            "Warning: Ignoring unrecognized subsystem %s, believed to be in ship %s\n",
            dname, Global_filename));
    }

    // Rotating subsystem
    if ((p = strstr(props, "$rotate")) != NULL) {
        subsystemp->flags |= MSS_FLAG_ROTATES;

        // get time for (a) complete rotation (b) step (c) activation
        float turn_time;
        get_user_prop_value(p + 7, buf);
        turn_time = (float)atof(buf);

        // CASE OF STEPPED ROTATION
        if ((p = strstr(props, "$stepped")) != NULL) {
            subsystemp->stepped_rotation = new (stepped_rotation);
            subsystemp->flags |= MSS_FLAG_STEPPED_ROTATE;

            // get number of steps
            if ((p = strstr(props, "$steps")) != NULL) {
                get_user_prop_value(p + 6, buf);
                subsystemp->stepped_rotation->num_steps = atoi(buf);
            }
            else {
                subsystemp->stepped_rotation->num_steps = 8;
            }

            // get pause time
            if ((p = strstr(props, "$t_paused")) != NULL) {
                get_user_prop_value(p + 9, buf);
                subsystemp->stepped_rotation->t_pause = (float)atof(buf);
            }
            else {
                subsystemp->stepped_rotation->t_pause = 2.0f;
            }

            // get transition time - time to go between steps
            if ((p = strstr(props, "$t_transit")) != NULL) {
                get_user_prop_value(p + 10, buf);
                subsystemp->stepped_rotation->t_transit = (float)atof(buf);
            }
            else {
                subsystemp->stepped_rotation->t_transit = 2.0f;
            }

            // get fraction of time spent in accel
            if ((p = strstr(props, "$fraction_accel")) != NULL) {
                get_user_prop_value(p + 15, buf);
                subsystemp->stepped_rotation->fraction = (float)atof(buf);
                Assert(subsystemp->stepped_rotation->fraction > 0 &&
                       subsystemp->stepped_rotation->fraction < 0.5);
            }
            else {
                subsystemp->stepped_rotation->fraction = 0.3f;
            }

            int num_steps = subsystemp->stepped_rotation->num_steps;
            float t_trans = subsystemp->stepped_rotation->t_transit;
            float fraction = subsystemp->stepped_rotation->fraction;

            subsystemp->stepped_rotation->max_turn_accel =
                PI2 /
                (fraction * (1.0f - fraction) * num_steps * t_trans * t_trans);
            subsystemp->stepped_rotation->max_turn_rate = PI2 /
                                                          ((1.0f - fraction) *
                                                           num_steps * t_trans);
        }

        // CASE OF AI ROTATION
        else if ((p = strstr(props, "$ai")) != NULL) {
            get_user_prop_value(p + 8, buf);
            subsystemp->flags |= MSS_FLAG_AI_ROTATE;

            // get parameters - ie, speed / dist / other ??
            // time to activate
            // condition
        }

        // CASE OF NORMAL CONTINUOUS ROTATION
        else {
            if (fabs(turn_time) < 1) {
                // Warning(LOCATION, "%s has subsystem %s with rotation time less than 1 sec", dname, Global_filename );
                subsystemp->flags &= ~MSS_FLAG_ROTATES;
                subsystemp->turn_rate = 0.0f;
            }
            else {
                subsystemp->turn_rate = PI2 / turn_time;
            }
        }
    }
}

void
do_new_subsystem(int n_subsystems, model_subsystem *slist, int subobj_num,
                 float rad, vector *pnt, char *props, char *subobj_name,
                 int model_num)
{
    int i;
    model_subsystem *subsystemp;

    if (slist == NULL)
        return; // For TestCode, POFView, etc don't bother

    // try to find the name of the subsystem passed here on the list of subsystems currently on the
    // ship.  Assign the values only when the right subsystem is found

    for (i = 0; i < n_subsystems; i++) {
        subsystemp = &slist[i];
        if (!stricmp(subobj_name, subsystemp->subobj_name)) {
            subsystemp->flags = 0;
            subsystemp->subobj_num = subobj_num;
            subsystemp->turret_gun_sobj = -1;
            subsystemp->model_num = model_num;
            subsystemp->pnt =
                *pnt; // use the offset to get the center point of the subsystem
            subsystemp->radius = rad;
            set_subsystem_info(subsystemp, props, subobj_name);
            strcpy(subsystemp->subobj_name, subobj_name); // copy the object name
            return;
        }
    }
#ifndef NDEBUG
    if (!ss_warning_shown) {
        char bname[_MAX_FNAME];

        _splitpath(model_filename, NULL, NULL, bname, NULL);
        Warning(
            LOCATION,
            "A subsystem was found in model %s that does not have a record in ships.tbl.\nA list of subsystems for this ship will be dumped to:\n\ndata\\tables\\%s.subsystems for inclusion\n into ships.tbl.",
            model_filename, bname);
        ss_warning_shown = 1;
    }
    else
#endif
        nprintf(("warning", "Subsystem %s in ships.tbl not found in model!\n",
                 subobj_name));
#ifndef NDEBUG
    if (ss_fp) {
        char tmp_buffer[128];
        sprintf(tmp_buffer, "$Subsystem:\t\t\t%s,1,0.0\n", subobj_name);
        cfputs(tmp_buffer, ss_fp);
    }
#endif
}

void
model_calc_bound_box(vector *box, vector *big_mn, vector *big_mx)
{
    box[0].x = big_mn->x;
    box[0].y = big_mn->y;
    box[0].z = big_mn->z;
    box[1].x = big_mx->x;
    box[1].y = big_mn->y;
    box[1].z = big_mn->z;
    box[2].x = big_mx->x;
    box[2].y = big_mx->y;
    box[2].z = big_mn->z;
    box[3].x = big_mn->x;
    box[3].y = big_mx->y;
    box[3].z = big_mn->z;

    box[4].x = big_mn->x;
    box[4].y = big_mn->y;
    box[4].z = big_mx->z;
    box[5].x = big_mx->x;
    box[5].y = big_mn->y;
    box[5].z = big_mx->z;
    box[6].x = big_mx->x;
    box[6].y = big_mx->y;
    box[6].z = big_mx->z;
    box[7].x = big_mn->x;
    box[7].y = big_mx->y;
    box[7].z = big_mx->z;
}

//      Debug thing so we don't repeatedly show warning messages.
#ifndef NDEBUG
int Bogus_warning_flag_1903 = 0;
#endif

//reads a binary file containing a 3d model
int
read_model_file(polymodel *pm, char *filename, int n_subsystems,
                model_subsystem *subsystems)
{
    CFILE *fp;
    int version;
    int id, len, next_chunk;
    int i, j;

#ifndef NDEBUG
    strcpy(Global_filename, filename);
#endif

    fp = cfopen(filename, "rb");
    if (!fp) {
        Error(LOCATION, "Can't open file <%s>", filename);
        return 0;
    }

    // code to get a filename to write out subsystem information for each model that
    // is read.  This info is essentially debug stuff that is used to help get models
    // into the game quicker
#if 0
        {
                char bname[_MAX_FNAME];

                _splitpath(filename, NULL, NULL, bname, NULL);
                sprintf(debug_name, "%s.subsystems", bname);
                ss_fp = cfopen(debug_name, "wb", CFILE_NORMAL, CF_TYPE_TABLES );
                if ( !ss_fp )   {
                        mprintf(( "Can't open debug file for writing subsystems for %s\n", filename));
                } else {
                        strcpy(model_filename, filename);
                        ss_warning_shown = 0;
                }
        }
#endif

    id = cfread_int(fp);

    if (id != 'OPSP')
        Error(LOCATION, "Bad ID in model file <%s>", filename);

    // Version is major*100+minor
    // So, major = version / 100;
    //     minor = version % 100;
    version = cfread_int(fp);

    //Warning( LOCATION, "POF Version = %d", version );

    if (version < PM_COMPATIBLE_VERSION ||
        (version / 100) > PM_OBJFILE_MAJOR_VERSION) {
        Warning(LOCATION, "Bad version (%d) in model file <%s>", version,
                filename);
        return 0;
    }

    pm->version = version;
    Assert(strlen(filename) < FILENAME_LEN);
    strncpy(pm->filename, filename, FILENAME_LEN);

    memset(&pm->view_positions, 0, sizeof(pm->view_positions));

    // reset insignia counts
    pm->num_ins = 0;

    id = cfread_int(fp);
    len = cfread_int(fp);
    next_chunk = cftell(fp) + len;

    while (!cfeof(fp)) {
        //              mprintf(("Processing chunk <%c%c%c%c>, len = %d\n",id,id>>8,id>>16,id>>24,len));
        //              key_getch();

        switch (id) {
        case ID_OHDR: { //Object header
            //vector v;

            //mprintf(0,"Got chunk OHDR, len=%d\n",len);

#if defined(FREESPACE1_FORMAT)
            pm->n_models = cfread_int(fp);
            //                          mprintf(( "Num models = %d\n", pm->n_models ));
            pm->rad = cfread_float(fp);
            pm->flags = cfread_int(fp); // 1=Allow tiling
#elif defined(FREESPACE2_FORMAT)
            pm->rad = cfread_float(fp);
            pm->flags = cfread_int(fp); // 1=Allow tiling
            pm->n_models = cfread_int(fp);
//                              mprintf(( "Num models = %d\n", pm->n_models ));
#endif

            pm->submodel = (bsp_info *)malloc(sizeof(bsp_info) * pm->n_models);
            Assert(pm->submodel != NULL);
            memset(pm->submodel, 0, sizeof(bsp_info) * pm->n_models);

            //Assert(pm->n_models <= MAX_SUBMODELS);

            cfread_vector(&pm->mins, fp);
            cfread_vector(&pm->maxs, fp);
            model_calc_bound_box(pm->bounding_box, &pm->mins, &pm->maxs);

            pm->n_detail_levels = cfread_int(fp);
            //  mprintf(( "There are %d detail levels\n", pm->n_detail_levels ));
            for (i = 0; i < pm->n_detail_levels; i++) {
                pm->detail[i] = cfread_int(fp);
                pm->detail_depth[i] = 0.0f;
                ///             mprintf(( "Detail level %d is model %d.\n", i, pm->detail[i] ));
            }

            pm->num_debris_objects = cfread_int(fp);
            Assert(pm->num_debris_objects <= MAX_DEBRIS_OBJECTS);
            // mprintf(( "There are %d debris objects\n", pm->num_debris_objects ));
            for (i = 0; i < pm->num_debris_objects; i++) {
                pm->debris_objects[i] = cfread_int(fp);
                // mprintf(( "Debris object %d is model %d.\n", i, pm->debris_objects[i] ));
            }

            if (pm->version >= 1903) {
                if (pm->version >= 2009) {
                    pm->mass = cfread_float(fp);
                    cfread_vector(&pm->center_of_mass, fp);
                    cfread_vector(&pm->moment_of_inertia.rvec, fp);
                    cfread_vector(&pm->moment_of_inertia.uvec, fp);
                    cfread_vector(&pm->moment_of_inertia.fvec, fp);
                }
                else {
                    // old code where mass wasn't based on area, so do the calculation manually

                    float vol_mass = cfread_float(fp);
                    //  Attn: John Slagel:  The following is better done in bspgen.
                    // Convert volume (cubic) to surface area (quadratic) and scale so 100 -> 100
                    float area_mass = (float)pow(vol_mass, 0.6667f) * 4.65f;

                    pm->mass = area_mass;
                    float mass_ratio = vol_mass / area_mass;

                    cfread_vector(&pm->center_of_mass, fp);
                    cfread_vector(&pm->moment_of_inertia.rvec, fp);
                    cfread_vector(&pm->moment_of_inertia.uvec, fp);
                    cfread_vector(&pm->moment_of_inertia.fvec, fp);

                    // John remove this with change to bspgen
                    vm_vec_scale(&pm->moment_of_inertia.rvec, mass_ratio);
                    vm_vec_scale(&pm->moment_of_inertia.uvec, mass_ratio);
                    vm_vec_scale(&pm->moment_of_inertia.fvec, mass_ratio);
                }
            }
            else {
#ifndef NDEBUG
                if (stricmp("fighter04.pof", filename)) {
                    if (Bogus_warning_flag_1903 == 0) {
                        Warning(
                            LOCATION,
                            "Ship %s is old.  Cannot compute mass.\nSetting to 50.0f.  Talk to John.",
                            filename);
                        Bogus_warning_flag_1903 = 1;
                    }
                }
#endif
                pm->mass = 50.0f;
                vm_vec_zero(&pm->center_of_mass);
                vm_set_identity(&pm->moment_of_inertia);
                vm_vec_scale(&pm->moment_of_inertia.rvec, 0.001f);
                vm_vec_scale(&pm->moment_of_inertia.uvec, 0.001f);
                vm_vec_scale(&pm->moment_of_inertia.fvec, 0.001f);
            }

            // read in cross section info
            pm->xc = NULL;
            if (pm->version >= 2014) {
                pm->num_xc = cfread_int(fp);
                if (pm->num_xc > 0) {
                    pm->xc = (cross_section *)malloc(pm->num_xc *
                                                     sizeof(cross_section));
                    for (int i = 0; i < pm->num_xc; i++) {
                        pm->xc[i].z = cfread_float(fp);
                        pm->xc[i].radius = cfread_float(fp);
                    }
                }
            }
            else {
                pm->num_xc = 0;
            }

            if (pm->version >= 2007) {
                pm->num_lights = cfread_int(fp);
                //mprintf(( "Found %d lights!\n", pm->num_lights ));

                pm->lights = (bsp_light *)malloc(sizeof(bsp_light) *
                                                 pm->num_lights);
                for (i = 0; i < pm->num_lights; i++) {
                    cfread_vector(&pm->lights[i].pos, fp);
                    pm->lights[i].type = cfread_int(fp);
                    pm->lights[i].value = 0.0f;
                }
            }
            else {
                pm->num_lights = 0;
                pm->lights = NULL;
            }

            break;
        }

        case ID_SOBJ: { //Subobject header
            int n;
            char *p, props[MAX_PROP_LEN];
            //                          float d;

            //mprintf(0,"Got chunk SOBJ, len=%d\n",len);

            n = cfread_int(fp);

            Assert(n < pm->n_models);

#if defined(FREESPACE2_FORMAT)
            pm->submodel[n].rad = cfread_float(fp); //radius
#endif

            pm->submodel[n].parent = cfread_int(fp);

            //                          cfread_vector(&pm->submodel[n].norm,fp);
            //                          d = cfread_float(fp);
            //                          cfread_vector(&pm->submodel[n].pnt,fp);
            cfread_vector(&pm->submodel[n].offset, fp);

            //                  mprintf(( "Subobj %d, offs = %.1f, %.1f, %.1f\n", n, pm->submodel[n].offset.x, pm->submodel[n].offset.y, pm->submodel[n].offset.z ));

#if defined(FREESPACE1_FORMAT)
            pm->submodel[n].rad = cfread_float(fp); //radius
#endif

            //                          pm->submodel[n].tree_offset = cfread_int(fp);   //offset
            //                          pm->submodel[n].data_offset = cfread_int(fp);   //offset

            cfread_vector(&pm->submodel[n].geometric_center, fp);

            cfread_vector(&pm->submodel[n].min, fp);
            cfread_vector(&pm->submodel[n].max, fp);

            model_calc_bound_box(pm->submodel[n].bounding_box,
                                 &pm->submodel[n].min, &pm->submodel[n].max);

            pm->submodel[n].name[0] = '\0';

            cfread_string_len(pm->submodel[n].name, MAX_NAME_LEN,
                              fp); // get the name
            cfread_string_len(props, MAX_PROP_LEN, fp); // and the user properites

            pm->submodel[n].movement_type = cfread_int(fp);
            pm->submodel[n].movement_axis = cfread_int(fp);

            // change turret movement type to MOVEMENT_TYPE_ROT_SPECIAL
            if (pm->submodel[n].movement_type == MOVEMENT_TYPE_ROT) {
                if (strstr(pm->submodel[n].name, "turret") ||
                    strstr(pm->submodel[n].name, "gun") ||
                    strstr(pm->submodel[n].name, "cannon")) {
                    pm->submodel[n].movement_type = MOVEMENT_TYPE_ROT_SPECIAL;
                }
                else if (strstr(pm->submodel[n].name, "thruster")) {
                    // Int3();
                    pm->submodel[n].movement_type = MOVEMENT_TYPE_NONE;
                    pm->submodel[n].movement_axis = MOVEMENT_AXIS_NONE;
                }
            }

            if (pm->submodel[n].name[0] == '\0') {
                strcpy(pm->submodel[n].name, "unknown object name");
            }

            bool rotating_submodel_has_subsystem = !(
                pm->submodel[n].movement_type == MOVEMENT_TYPE_ROT);
            if ((p = strstr(props, "$special")) != NULL) {
                char type[32];

                get_user_prop_value(p + 9, type);
                if (!stricmp(
                        type,
                        "subsystem")) { // if we have a subsystem, put it into the list!
                    do_new_subsystem(n_subsystems, subsystems, n,
                                     pm->submodel[n].rad, &pm->submodel[n].offset,
                                     props, pm->submodel[n].name, pm->id);
                    rotating_submodel_has_subsystem = true;
                }
                else if (!stricmp(type, "no_rotate")) {
                    // mark those submodels which should not rotate - ie, those with no subsystem
                    pm->submodel[n].movement_type = MOVEMENT_TYPE_NONE;
                    pm->submodel[n].movement_axis = MOVEMENT_AXIS_NONE;
                }
                else {
                    // if submodel rotates (via bspgen), then there is either a subsys or special=no_rotate
                    Assert(pm->submodel[n].movement_type != MOVEMENT_TYPE_ROT);
                }
            }

            if (!rotating_submodel_has_subsystem) {
                nprintf(("Model",
                         "Model %s: Rotating Submodel without subsystem: %s\n",
                         pm->filename, pm->submodel[n].name));

                // mark those submodels which should not rotate - ie, those with no subsystem
                pm->submodel[n].movement_type = MOVEMENT_TYPE_NONE;
                pm->submodel[n].movement_axis = MOVEMENT_AXIS_NONE;
            }

            pm->submodel[n].angs.p = 0.0f;
            pm->submodel[n].angs.b = 0.0f;
            pm->submodel[n].angs.h = 0.0f;

            {
                int nchunks = cfread_int(fp); // Throw away nchunks
                if (nchunks > 0) {
                    Error(LOCATION, "Model '%s' is chunked.  See John or Adam!\n",
                          pm->filename);
                }
            }
            pm->submodel[n].bsp_data_size = cfread_int(fp);
            if (pm->submodel[n].bsp_data_size > 0) {
                pm->submodel[n].bsp_data = (ubyte *)malloc(
                    pm->submodel[n].bsp_data_size);
                cfread(pm->submodel[n].bsp_data, 1, pm->submodel[n].bsp_data_size,
                       fp);
            }
            else {
                pm->submodel[n].bsp_data = NULL;
            }

            if (strstr(pm->submodel[n].name, "thruster"))
                pm->submodel[n].is_thruster = 1;
            else
                pm->submodel[n].is_thruster = 0;

            if (strstr(pm->submodel[n].name, "-destroyed"))
                pm->submodel[n].is_damaged = 1;
            else
                pm->submodel[n].is_damaged = 0;

            //mprintf(( "Submodel %d, name '%s', parent = %d\n", n, pm->submodel[n].name, pm->submodel[n].parent ));
            //key_getch();

            //mprintf(( "Submodel %d, tree offset %d\n", n, pm->submodel[n].tree_offset ));
            //mprintf(( "Submodel %d, data offset %d\n", n, pm->submodel[n].data_offset ));
            //key_getch();

            break;
        }

        case ID_SHLD: {
            int nverts, ntris;

            nverts = cfread_int(fp); // get the number of vertices in the list
            pm->shield.nverts = nverts;
            pm->shield.verts = (shield_vertex *)malloc(nverts *
                                                       sizeof(shield_vertex));
            Assert(pm->shield.verts);
            for (i = 0; i < nverts; i++) // read in the vertex list
                cfread_vector(&(pm->shield.verts[i].pos), fp);

            ntris = cfread_int(
                fp); // get the number of triangles that compose the shield
            pm->shield.ntris = ntris;
            pm->shield.tris = (shield_tri *)malloc(ntris * sizeof(shield_tri));
            Assert(pm->shield.tris);
            for (i = 0; i < ntris; i++) {
                cfread_vector(&(pm->shield.tris[i].norm), fp);
                for (j = 0; j < 3; j++) {
                    pm->shield.tris[i].verts[j] = cfread_int(
                        fp); // read in the indices into the shield_vertex list
                    /*
#ifndef NDEBUG
                                                        if (pm->shield.tris[i].verts[j] >= nverts)
                                                                if (!warning_displayed) {
                                                                        warning_displayed = 1;
                                                                        Warning(LOCATION, "Ship %s has a bogus shield mesh.\nOnly %i vertices, index %i found.\n", filename, nverts, pm->shield.tris[i].verts[j]);
                                                                }
#endif
                                                                */
                }
                for (j = 0; j < 3; j++)
                    pm->shield.tris[i].neighbors[j] = cfread_int(
                        fp); // read in the neighbor indices -- indexes into tri list
            }
            break;

        } break;

        case ID_GPNT:
            pm->n_guns = cfread_int(fp);
            pm->gun_banks = (w_bank *)malloc(sizeof(w_bank) * pm->n_guns);
            Assert(pm->gun_banks != NULL);

            for (i = 0; i < pm->n_guns; i++) {
                w_bank *bank = &pm->gun_banks[i];

                bank->num_slots = cfread_int(fp);
                Assert(bank->num_slots < MAX_SLOTS);
                for (j = 0; j < bank->num_slots; j++) {
                    cfread_vector(&(bank->pnt[j]), fp);
                    cfread_vector(&(bank->norm[j]), fp);
                }
            }
            break;

        case ID_MPNT:
            pm->n_missiles = cfread_int(fp);
            pm->missile_banks = (w_bank *)malloc(sizeof(w_bank) * pm->n_missiles);
            Assert(pm->missile_banks != NULL);

            for (i = 0; i < pm->n_missiles; i++) {
                w_bank *bank = &pm->missile_banks[i];

                bank->num_slots = cfread_int(fp);
                Assert(bank->num_slots < MAX_SLOTS);
                for (j = 0; j < bank->num_slots; j++) {
                    cfread_vector(&(bank->pnt[j]), fp);
                    cfread_vector(&(bank->norm[j]), fp);
                }
            }
            break;

        case ID_DOCK: {
            char props[MAX_PROP_LEN];

            pm->n_docks = cfread_int(fp);
            pm->docking_bays = (dock_bay *)malloc(sizeof(dock_bay) * pm->n_docks);
            Assert(pm->docking_bays != NULL);

            for (i = 0; i < pm->n_docks; i++) {
                char *p;
                dock_bay *bay = &pm->docking_bays[i];

                cfread_string_len(props, MAX_PROP_LEN, fp);
                if ((p = strstr(props, "$name")) != NULL)
                    get_user_prop_value(p + 5, bay->name);
                else
                    sprintf(bay->name, "<unnamed bay %c>", 'A' + i);
                bay->num_spline_paths = cfread_int(fp);
                if (bay->num_spline_paths > 0) {
                    bay->splines = (int *)malloc(sizeof(int) *
                                                 bay->num_spline_paths);
                    for (j = 0; j < bay->num_spline_paths; j++)
                        bay->splines[j] = cfread_int(fp);
                }
                else {
                    bay->splines = NULL;
                }

                // determine what this docking bay can be used for
                if (!strnicmp(bay->name, "cargo", 5))
                    bay->type_flags = DOCK_TYPE_CARGO;
                else
                    bay->type_flags = (DOCK_TYPE_REARM | DOCK_TYPE_GENERIC);

                bay->num_slots = cfread_int(fp);
                Assert(bay->num_slots == 2); // Get Allender if Asserted!
                for (j = 0; j < bay->num_slots; j++) {
                    cfread_vector(&(bay->pnt[j]), fp);
                    cfread_vector(&(bay->norm[j]), fp);
                }
            }
            break;
        }

        case ID_FUEL:
            char props[MAX_PROP_LEN];
            pm->n_thrusters = cfread_int(fp);
            pm->thrusters = (thruster_bank *)malloc(sizeof(thruster_bank) *
                                                    pm->n_thrusters);
            Assert(pm->thrusters != NULL);

            for (i = 0; i < pm->n_thrusters; i++) {
                thruster_bank *bank = &pm->thrusters[i];

                bank->num_slots = cfread_int(fp);

                if (pm->version < 2117) {
                    bank->wash_info_index = -1;
                }
                else {
                    cfread_string_len(props, MAX_PROP_LEN, fp);
                    // look for $engine_subsystem=xxx
                    int length = strlen(props);
                    if (length > 0) {
                        int base_length = strlen("$engine_subsystem=");
                        Assert(strstr((const char *)&props,
                                      "$engine_subsystem=") != NULL);
                        Assert(length > base_length);
                        char *engine_subsys_name = props + base_length;
                        if (engine_subsys_name[0] == '$') {
                            engine_subsys_name++;
                        }

                        nprintf((
                            "wash",
                            "Ship %s with engine wash associated with subsys %s\n",
                            filename, engine_subsys_name));

                        // set wash_info_index to invalid
                        int table_error = 1;
                        bank->wash_info_index = -1;
                        for (int k = 0; k < n_subsystems; k++) {
                            if (0 == stricmp(subsystems[k].subobj_name,
                                             engine_subsys_name)) {
                                bank->wash_info_index =
                                    subsystems[k].engine_wash_index;
                                if (bank->wash_info_index >= 0) {
                                    table_error = 0;
                                }
                                break;
                            }
                        }

                        if ((bank->wash_info_index == -1) && (n_subsystems > 0)) {
                            if (table_error) {
                                Warning(
                                    LOCATION,
                                    "No engine wash table entry in ships.tbl for ship model %s",
                                    filename);
                            }
                            else {
                                Warning(
                                    LOCATION,
                                    "Inconsistent model: Engine wash engine subsystem does not match any ship subsytem names for ship model %s",
                                    filename);
                            }
                        }
                    }
                    else {
                        bank->wash_info_index = -1;
                    }
                }

                for (j = 0; j < bank->num_slots; j++) {
                    cfread_vector(&(bank->pnt[j]), fp);
                    cfread_vector(&(bank->norm[j]), fp);
                    if (pm->version > 2004) {
                        bank->radius[j] = cfread_float(fp);
                        //mprintf(( "Rad = %.2f\n", rad ));
                    }
                    else {
                        bank->radius[j] = 1.0f;
                    }
                }
                //mprintf(( "Num slots = %d\n", bank->num_slots ));
            }
            break;

        case ID_TGUN:
        case ID_TMIS: {
            int n_banks, n_slots, parent;
            model_subsystem *subsystemp;
            int i, j, snum = -1;

            n_banks = cfread_int(fp); // number of turret points
            for (i = 0; i < n_banks; i++) {
                int physical_parent; // who are we attached to?
                parent = cfread_int(fp); // get the turret parent of the object

                physical_parent = cfread_int(
                    fp); // The parent subobj that this is physically attached to

                if (subsystems) {
                    for (snum = 0; snum < n_subsystems; snum++) {
                        subsystemp = &subsystems[snum];

                        if (parent == subsystemp->subobj_num) {
                            cfread_vector(&subsystemp->turret_norm, fp);
                            vm_vector_2_matrix(&subsystemp->turret_matrix,
                                               &subsystemp->turret_norm, NULL,
                                               NULL);

                            n_slots = cfread_int(fp);
                            subsystemp->turret_gun_sobj = physical_parent;
                            Assert(
                                n_slots <
                                MAX_TFP); // only MAX_TFP firing points per model_subsystem
                            for (j = 0; j < n_slots; j++) {
                                cfread_vector(&subsystemp->turret_firing_point[j],
                                              fp);
                            }
                            Assert(n_slots > 0);

                            subsystemp->turret_num_firing_points = n_slots;

                            break;
                        }
                    }
                }

                //turret_gun_sobj

                if ((n_subsystems == 0) || (snum == n_subsystems)) {
                    vector bogus;

                    nprintf((
                        "Warning",
                        "Turret object not found for turret firing point in model %s\n",
                        model_filename));
                    cfread_vector(&bogus, fp);
                    n_slots = cfread_int(fp);
                    for (j = 0; j < n_slots; j++)
                        cfread_vector(&bogus, fp);
                }
            }
            break;
        }

        case ID_SPCL: {
            char name[MAX_NAME_LEN], props[MAX_PROP_LEN], *p;
            int n_specials;
            float radius;
            vector pnt;

            n_specials = cfread_int(
                fp); // get the number of special subobjects we have
            for (i = 0; i < n_specials; i++) {
                // get the next free object of the subobject list.  Flag error if no more room

                cfread_string_len(name, MAX_NAME_LEN,
                                  fp); // get the name of this special polygon

                cfread_string_len(props, MAX_PROP_LEN,
                                  fp); // will definately have properties as well!
                cfread_vector(&pnt, fp);
                radius = cfread_float(fp);

                // check if $Split
                p = strstr(name, "$split");
                if (p != NULL) {
                    pm->split_plane[pm->num_split_plane] = pnt.z;
                    pm->num_split_plane++;
                    Assert(pm->num_split_plane <= MAX_SPLIT_PLANE);
                }
                else if ((p = strstr(props, "$special")) != NULL) {
                    char type[32];

                    get_user_prop_value(p + 9, type);
                    if (!stricmp(
                            type,
                            "subsystem")) // if we have a subsystem, put it into the list!
                        do_new_subsystem(
                            n_subsystems, subsystems, -1, radius, &pnt, props,
                            &name[1],
                            pm->id); // skip the first '$' character of the name
                }
                else if (strstr(name, "$enginelarge") ||
                         strstr(name, "$enginehuge")) {
                    do_new_subsystem(
                        n_subsystems, subsystems, -1, radius, &pnt, props,
                        &name[1],
                        pm->id); // skip the first '$' character of the name
                }
                else {
                    nprintf((
                        "Warning",
                        "Unknown special object type %s while reading model %s\n",
                        name, pm->filename));
                }
            }
            break;
        }

        case ID_TXTR: { //Texture filename list
            int i, n;
            //                          char name_buf[128];

            //mprintf(0,"Got chunk TXTR, len=%d\n",len);

            n = cfread_int(fp);
            pm->n_textures = n;
            // Dont overwrite memory!!
            Assert(n <= MAX_MODEL_TEXTURES);
            //mprintf(0,"  num textures = %d\n",n);
            for (i = 0; i < n; i++) {
                char tmp_name[256];
                cfread_string_len(tmp_name, 127, fp);

                // Record the texture name only.  Binding it to a bitmap-
                // manager handle is deferred to model_load_textures() so the
                // reader carries no bmpman dependency -- the sole game-system
                // coupling read_model_file used to have.  See docs/pof-model.md.
                strncpy(pm->texture_file[i], tmp_name, FILENAME_LEN - 1);
                pm->texture_file[i][FILENAME_LEN - 1] = '\0';
            }

            break;
        }

            /*                  case ID_IDTA:           //Interpreter data
                                //mprintf(0,"Got chunk IDTA, len=%d\n",len);

                                pm->model_data = (ubyte *)malloc(len);
                                pm->model_data_size = len;
                                Assert(pm->model_data != NULL );
                        
                                cfread(pm->model_data,1,len,fp);
                        
                                break;
*/

        case ID_INFO: // don't need to do anything with info stuff

#ifndef NDEBUG
            pm->debug_info_size = len;
            pm->debug_info = (char *)malloc(pm->debug_info_size + 1);
            Assert(pm->debug_info != NULL);
            memset(pm->debug_info, 0, len + 1);
            cfread(pm->debug_info, 1, len, fp);
#endif
            break;

        case ID_GRID:
            break;

        case ID_PATH:
            pm->n_paths = cfread_int(fp);
            pm->paths = (model_path *)malloc(sizeof(model_path) * pm->n_paths);
            Assert(pm->paths != NULL);

            for (i = 0; i < pm->n_paths; i++) {
                cfread_string_len(pm->paths[i].name, MAX_NAME_LEN - 1, fp);
                if (pm->version >= 2002) {
                    // store the sub_model name number of the parent
                    cfread_string_len(pm->paths[i].parent_name, MAX_NAME_LEN - 1,
                                      fp);
                    // get rid of leading '$' char in name
                    if (pm->paths[i].parent_name[0] == '$') {
                        char tmpbuf[MAX_NAME_LEN];
                        strcpy(tmpbuf, pm->paths[i].parent_name + 1);
                        strcpy(pm->paths[i].parent_name, tmpbuf);
                    }
                    // store the sub_model index (ie index into pm->submodel) of the parent
                    pm->paths[i].parent_submodel = -1;
                    for (j = 0; j < pm->n_models; j++) {
                        if (!stricmp(pm->submodel[j].name,
                                     pm->paths[i].parent_name)) {
                            pm->paths[i].parent_submodel = j;
                        }
                    }
                }
                else {
                    pm->paths[i].parent_name[0] = 0;
                    pm->paths[i].parent_submodel = -1;
                }
                pm->paths[i].nverts = cfread_int(fp);
                pm->paths[i].verts = (mp_vert *)malloc(sizeof(mp_vert) *
                                                       pm->paths[i].nverts);
                pm->paths[i].goal = pm->paths[i].nverts - 1;
                pm->paths[i].type = MP_TYPE_UNUSED;
                pm->paths[i].value = 0;
                Assert(pm->paths[i].verts != NULL);
                for (j = 0; j < pm->paths[i].nverts; j++) {
                    cfread_vector(&pm->paths[i].verts[j].pos, fp);
                    pm->paths[i].verts[j].radius = cfread_float(fp);

                    { // version 1802 added turret stuff
                        int nturrets, k;

                        nturrets = cfread_int(fp);
                        pm->paths[i].verts[j].nturrets = nturrets;
                        pm->paths[i].verts[j].turret_ids = (int *)malloc(
                            sizeof(int) * nturrets);
                        for (k = 0; k < nturrets; k++)
                            pm->paths[i].verts[j].turret_ids[k] = cfread_int(fp);
                    }
                }
            }
            break;

        case ID_EYE: // an eye position(s)
        {
            int num_eyes, i;

            // all eyes points are stored simply as vectors and their normals.
            // 0th element is used as usual player view position.

            num_eyes = cfread_int(fp);
            pm->n_view_positions = num_eyes;
            Assert(num_eyes < MAX_EYES);
            for (i = 0; i < num_eyes; i++) {
                pm->view_positions[i].parent = cfread_int(fp);
                cfread_vector(&pm->view_positions[i].pnt, fp);
                cfread_vector(&pm->view_positions[i].norm, fp);
            }
        } break;

        case ID_INSG:
            int num_ins, num_verts, num_faces, idx, idx2, idx3;

            // get the # of insignias
            num_ins = cfread_int(fp);
            pm->num_ins = num_ins;

            // read in the insignias
            for (idx = 0; idx < num_ins; idx++) {
                // get the detail level
                pm->ins[idx].detail_level = cfread_int(fp);

                // # of faces
                num_faces = cfread_int(fp);
                pm->ins[idx].num_faces = num_faces;
                Assert(num_faces <= MAX_INS_FACES);

                // # of vertices
                num_verts = cfread_int(fp);
                Assert(num_verts <= MAX_INS_VECS);

                // read in all the vertices
                for (idx2 = 0; idx2 < num_verts; idx2++) {
                    cfread_vector(&pm->ins[idx].vecs[idx2], fp);
                }

                // read in world offset
                cfread_vector(&pm->ins[idx].offset, fp);

                // read in all the faces
                for (idx2 = 0; idx2 < pm->ins[idx].num_faces; idx2++) {
                    // read in 3 vertices
                    for (idx3 = 0; idx3 < 3; idx3++) {
                        pm->ins[idx].faces[idx2][idx3] = cfread_int(fp);
                        pm->ins[idx].u[idx2][idx3] = cfread_float(fp);
                        pm->ins[idx].v[idx2][idx3] = cfread_float(fp);
                    }
                }
            }
            break;

        // autocentering info
        case ID_ACEN:
            cfread_vector(&pm->autocenter, fp);
            pm->flags |= PM_FLAG_AUTOCEN;
            break;

        default:
            mprintf(("Unknown chunk <%c%c%c%c>, len = %d\n", id, id >> 8,
                     id >> 16, id >> 24, len));
            cfseek(fp, len, SEEK_CUR);
            break;
        }
        cfseek(fp, next_chunk, SEEK_SET);

        id = cfread_int(fp);
        len = cfread_int(fp);
        next_chunk = cftell(fp) + len;
    }

#ifndef NDEBUG
    if (ss_fp) {
        int size;

        cfclose(ss_fp);
        ss_fp = cfopen(debug_name, "rb");
        if (ss_fp) {
            size = cfilelength(ss_fp);
            cfclose(ss_fp);
            if (size <= 0) {
                _unlink(debug_name);
            }
        }
    }
#endif

    cfclose(fp);
    // mprintf(("Done processing chunks\n"));
    return 1;
}
