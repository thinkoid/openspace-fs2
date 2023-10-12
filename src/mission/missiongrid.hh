// -*- mode: c++; -*-

#ifndef FREESPACE2_MISSION_MISSIONGRID_HH
#define FREESPACE2_MISSION_MISSIONGRID_HH

#include <defs.hh>

#include <physics/physics.hh>

#define MAX_GRIDLINE_POINTS 201
#define L_MAX_LINES 128

struct grid  {
    int nrows, ncols;
    vec3d center;
    matrix gmatrix;
    physics_info physics;
    float square_size;
    float
        planeD; // D component of plane equation (A, B, C are uvec in gmatrix)
    vec3d gpoints1[MAX_GRIDLINE_POINTS]; // 1 -4 are edge gridpoints for small
                                         // grid.
    vec3d gpoints2[MAX_GRIDLINE_POINTS];
    vec3d gpoints3[MAX_GRIDLINE_POINTS];
    vec3d gpoints4[MAX_GRIDLINE_POINTS];
    vec3d gpoints5[MAX_GRIDLINE_POINTS]; // 5-8 are edge gridpoints for large
                                         // grid.
    vec3d gpoints6[MAX_GRIDLINE_POINTS];
    vec3d gpoints7[MAX_GRIDLINE_POINTS];
    vec3d gpoints8[MAX_GRIDLINE_POINTS];
};

struct tline  {
    int istart, iend, color;
};

extern grid Global_grid;
extern grid* The_grid;
extern bool double_fine_gridlines;

void grid_read_camera_controls (control_info* ci, float frametime);
void maybe_create_new_grid (
    grid* gridp, vec3d* pos, matrix* orient, int force = 0);
grid* create_grid (
    grid* gridp, vec3d* forward, vec3d* right, vec3d* center, int nrows,
    int ncols, float square_size);
grid* create_default_grid (void);
void render_grid (grid* gridp);
void modify_grid (grid* gridp);
void rpd_line (vec3d* v0, vec3d* v1);
void grid_render_elevation_line (vec3d* pos, grid* gridp);

#endif // FREESPACE2_MISSION_MISSIONGRID_HH
