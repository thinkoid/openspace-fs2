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

// FREESPACE1 FORMAT
#if defined(FREESPACE1_FORMAT)
// POF file header
#define ID_OHDR 'RDHO'
// Subobject header
#define ID_SOBJ 'JBOS'
#else
// POF file header
#define ID_OHDR '2RDH'
// Subobject header
#define ID_SOBJ '2JBO'
#endif
// Texture filename list
#define ID_TXTR 'RTXT'
// POF file information, like command line, etc
#define ID_INFO 'FNIP'
// Grid information
#define ID_GRID 'DIRG'
// Special object -- like a gun, missile, docking point, etc.
#define ID_SPCL 'LCPS'
// A spline based path
#define ID_PATH 'HTAP'
// gun points
#define ID_GPNT 'TNPG'
// missile points
#define ID_MPNT 'TNPM'
// docking points
#define ID_DOCK 'KCOD'
// turret gun points
#define ID_TGUN 'NUGT'
// turret missile points
#define ID_TMIS 'SIMT'
// thruster points
#define ID_FUEL 'LEUF'
// shield definition
#define ID_SHLD 'DLHS'
// eye information
#define ID_EYE ' EYE'
// insignia information
#define ID_INSG 'GSNI'
// autocentering information
#define ID_ACEN 'NECA'

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
int read_model_file(polymodel *pm, char *filename, int n_subsystems,
                    model_subsystem *subsystems);

void interp_clear_instance();

// 6500 (7x)
#define MAX_POLYGON_VECS 1100
// 6500 (3x)
#define MAX_POLYGON_NORMS 2800

extern vector *Interp_verts[MAX_POLYGON_VECS];

#endif
