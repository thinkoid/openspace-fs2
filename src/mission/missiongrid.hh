/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef __MISSIONGRID_H__
#define __MISSIONGRID_H__

#define MAX_GRIDLINE_POINTS 201
#define L_MAX_LINES 128

typedef struct grid
{
    int nrows, ncols;
    vector center;
    matrix gmatrix;
    physics_info physics;
    float square_size;
    float planeD; // D component of plane equation (A, B, C are uvec in gmatrix)
    vector
        gpoints1[MAX_GRIDLINE_POINTS]; // 1 -4 are edge gridpoints for small grid.
    vector gpoints2[MAX_GRIDLINE_POINTS];
    vector gpoints3[MAX_GRIDLINE_POINTS];
    vector gpoints4[MAX_GRIDLINE_POINTS];
    vector gpoints5[MAX_GRIDLINE_POINTS]; // 5-8 are edge gridpoints for large grid.
    vector gpoints6[MAX_GRIDLINE_POINTS];
    vector gpoints7[MAX_GRIDLINE_POINTS];
    vector gpoints8[MAX_GRIDLINE_POINTS];
} grid;

// The FRED grid implementation is gone (missiongrid.cc, removed with the
// Fred_running fold); the briefing map keeps its own brief_* grid code in
// missionbriefcommon.cc, which owns these globals now.
extern grid Global_grid;
extern grid *The_grid;
extern int double_fine_gridlines;

#endif
