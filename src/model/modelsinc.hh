/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef _MODELSINC_H
#define _MODELSINC_H

#include <model/model.hh>

#ifndef MODEL_LIB
#error This should only be used internally by the model library.  See John if you think you need to include this elsewhere.
#endif

#define OP_EOF 0
#define OP_DEFPOINTS 1
#define OP_FLATPOLY 2
#define OP_TMAPPOLY 3
#define OP_SORTNORM 4
#define OP_BOUNDBOX 5

// change header for freespace2
//#define FREESPACE1_FORMAT
#define FREESPACE2_FORMAT
#if defined(FREESPACE1_FORMAT)
#elif defined(FREESPACE2_FORMAT)
#else
#error Neither FREESPACE1_FORMAT or FREESPACE2_FORMAT defined
#endif

// chunk ids spell the byte sequence as it appears in the file (retail
// wrote them as reversed multichar literals: '2RDH' == fourcc("HDR2"))

// FREESPACE1 FORMAT
#if defined(FREESPACE1_FORMAT)
// POF file header
#define ID_OHDR fourcc("OHDR")
// Subobject header
#define ID_SOBJ fourcc("SOBJ")
#else
// POF file header
#define ID_OHDR fourcc("HDR2")
// Subobject header
#define ID_SOBJ fourcc("OBJ2")
#endif
// Texture filename list
#define ID_TXTR fourcc("TXTR")
// POF file information, like command line, etc
#define ID_INFO fourcc("PINF")
// Grid information
#define ID_GRID fourcc("GRID")
// Special object -- like a gun, missile, docking point, etc.
#define ID_SPCL fourcc("SPCL")
// A spline based path
#define ID_PATH fourcc("PATH")
// gun points
#define ID_GPNT fourcc("GPNT")
// missile points
#define ID_MPNT fourcc("MPNT")
// docking points
#define ID_DOCK fourcc("DOCK")
// turret gun points
#define ID_TGUN fourcc("TGUN")
// turret missile points
#define ID_TMIS fourcc("TMIS")
// thruster points
#define ID_FUEL fourcc("FUEL")
// shield definition
#define ID_SHLD fourcc("SHLD")
// eye information
#define ID_EYE fourcc("EYE ")
// insignia information
#define ID_INSG fourcc("INSG")
// autocentering information
#define ID_ACEN fourcc("ACEN")

#define uw(p) (*((uint *)(p)))
#define w(p) (*((int *)(p)))
#define wp(p) ((int *)(p))
#define vp(p) ((vector *)(p))
#define fl(p) (*((float *)(p)))

// Creates the octants for a given polygon model
void model_octant_create(polymodel *pm);

// frees the memory the octants use for a given polygon model
void model_octant_free(polymodel *pm);

void model_calc_bound_box(vector *box, vector *big_mn, vector *big_mx);

// the POF container reader (pofparse.cpp); fills pm, returns 1 on success
int read_model_file(polymodel *pm, const char *filename, int n_subsystems,
                    model_subsystem *subsystems);

void interp_clear_instance();

// 6500 (7x)
#define MAX_POLYGON_VECS 1100
// 6500 (3x)
#define MAX_POLYGON_NORMS 2800

extern vector *Interp_verts[MAX_POLYGON_VECS];

#endif
