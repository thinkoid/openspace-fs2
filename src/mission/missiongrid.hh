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

typedef struct tline
{
    int istart, iend, color;
} tline;

extern grid Global_grid;
extern grid *The_grid;
extern int double_fine_gridlines;

void grid_read_camera_controls(control_info *ci, float frametime);
void maybe_create_new_grid(grid *gridp, vector *pos, matrix *orient,
                           int force = 0);
grid *create_grid(grid *gridp, vector *forward, vector *right, vector *center,
                  int nrows, int ncols, float square_size);
grid *create_default_grid(void);
void render_grid(grid *gridp);
void modify_grid(grid *gridp);
void rpd_line(vector *v0, vector *v1);
void grid_render_elevation_line(vector *pos, grid *gridp);

#endif
