// -*- mode: c++; -*-

#ifndef FREESPACE2_GRAPHICS_OPENGL_GROPENGLPOSTPROCESSING_HH
#define FREESPACE2_GRAPHICS_OPENGL_GROPENGLPOSTPROCESSING_HH

#include <defs.hh>

void opengl_post_process_init ();
void opengl_post_process_shutdown ();

void gr_opengl_post_process_set_effect (
    const char* name, int x, const vec3d* rgb);
void gr_opengl_post_process_set_defaults ();
void gr_opengl_post_process_save_zbuffer ();
void gr_opengl_post_process_restore_zbuffer ();
void gr_opengl_post_process_begin ();
void gr_opengl_post_process_end ();
void get_post_process_effect_names (std::vector< std::string >& names);

void opengl_post_shader_header (
    std::stringstream& sflags, shader_type shader_t, int flags);

#endif // FREESPACE2_GRAPHICS_OPENGL_GROPENGLPOSTPROCESSING_HH
