// -*- mode: c++; -*-

#ifndef FREESPACE2_RENDER_3DINTERNAL_HH
#define FREESPACE2_RENDER_3DINTERNAL_HH

#include <defs.hh>

#include "render/3d.hh"

extern int Canvas_width, Canvas_height; // the actual width & height
extern float Canv_w2, Canv_h2;          // fixed-point width,height/2

extern vec3d Window_scale;
extern int free_point_num;

extern float View_zoom;
extern vec3d View_position, Matrix_scale;
extern matrix View_matrix, Unscaled_matrix;

extern void free_temp_point (vertex* p);
extern vertex**
clip_polygon (vertex** src, vertex** dest, int* nv, ccodes* cc, uint flags);
extern void init_free_points (void);
extern void clip_line (vertex** p0, vertex** p1, ubyte codes_or, uint flags);

extern int G3_count;

extern int G3_user_clip;
extern vec3d G3_user_clip_normal;
extern vec3d G3_user_clip_point;

// Returns TRUE if point is behind user plane
extern int g3_point_behind_user_plane (const vec3d* pnt);

#endif // FREESPACE2_RENDER_3DINTERNAL_HH
