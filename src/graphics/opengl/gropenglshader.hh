// -*- mode: c++; -*-

#ifndef FREESPACE2_GRAPHICS_OPENGL_GROPENGLSHADER_HH
#define FREESPACE2_GRAPHICS_OPENGL_GROPENGLSHADER_HH

#include "defs.hh"

#include "graphics/2d.hh"
#include "graphics/opengl/gropengl.hh"
#include "graphics/material.hh"

#include <string>
#include "glad/glad.h"

#include <boost/container_hash/hash.hpp>

namespace opengl {
// Forward definition to avoid cyclic dependency
class ShaderProgram;
} // namespace opengl

enum shader_stage { SDR_STAGE_VERTEX, SDR_STAGE_FRAGMENT, SDR_STAGE_GEOMETRY };

struct opengl_vert_attrib {
    enum attrib_id {
        POSITION,
        COLOR,
        TEXCOORD,
        NORMAL,
        TANGENT,
        MODEL_ID,
        RADIUS,
        UVEC,
        WORLD_MATRIX,
        NUM_ATTRIBS,
    };

    attrib_id attribute_id;
    std::string name;
    vec4 default_value;
};

inline std::size_t
hash_value (opengl_vert_attrib::attrib_id const& arg) {
    return boost::hash< size_t > ()(static_cast< size_t > (arg));
}

extern std::vector< opengl_vert_attrib > GL_vertex_attrib_info;

struct geometry_sdr_params {
    int input_type;
    int output_type;
    int vertices_out;
};

struct opengl_shader_type_t {
    shader_type type_id;

    const char* vert;
    const char* frag;
    const char* geo;

    std::vector< opengl_vert_attrib::attrib_id > attributes;

    const char* description;
};

struct opengl_shader_variant_t {
    shader_type type_id;

    bool use_geometry_sdr;

    int flag;
    std::string flag_text;

    std::vector< opengl_vert_attrib::attrib_id > attributes;

    const char* description;
};

struct opengl_shader_file_t {
    const char* vert;
    const char* frag;
    const char* geo;

    int flags;

    std::vector< const char* > uniforms;

    std::vector< const char* > attributes;

    const char* description;
};

struct opengl_shader_uniform_reference_t {
    unsigned int flag;

    std::vector< const char* > uniforms;

    std::vector< const char* > attributes;

    std::vector< const char* > uniform_blocks;

    const char* name;
};

struct opengl_shader_uniform_t  {
    std::string text_id;
    GLint location;

    opengl_shader_uniform_t () : location (-1) {}
};

struct opengl_shader_t  {
    std::unique_ptr< opengl::ShaderProgram > program;

    shader_type shader;
    unsigned int flags;
    int flags2;

    opengl_shader_t ();

    opengl_shader_t (opengl_shader_t&& other) noexcept;
    opengl_shader_t& operator= (opengl_shader_t&& other) noexcept;

    opengl_shader_t (const opengl_shader_t&) = delete;
    opengl_shader_t& operator= (const opengl_shader_t&) = delete;
};

extern std::vector< opengl_shader_t > GL_shader;

extern opengl_shader_t* Current_shader;

int gr_opengl_maybe_create_shader (shader_type shader_t, unsigned int flags);
void gr_opengl_recompile_all_shaders (
    const std::function< void(size_t, size_t) >& progress_callback = nullptr);
void opengl_delete_shader (int sdr_handle);
void opengl_shader_set_current (opengl_shader_t* shader_obj = NULL);
void opengl_shader_set_current (int handle);

void opengl_shader_init ();
void opengl_shader_shutdown ();

int opengl_compile_shader (shader_type sdr, uint flags);

GLint opengl_shader_get_attribute (opengl_vert_attrib::attrib_id attribute);

void opengl_program_check_info_log (GLuint program_object);
void opengl_shader_check_info_log (GLuint shader_object);

void opengl_shader_compile_deferred_light_shader ();
void opengl_shader_compile_deferred_light_clear_shader ();

void opengl_shader_compile_passthrough_shader ();

void opengl_shader_set_passthrough (bool textured = true);

void opengl_shader_set_default_material (
    bool textured, bool alpha, vec4* clr, float color_scale,
    uint32_t array_index, const material::clip_plane& clip_plane);

#endif // FREESPACE2_GRAPHICS_OPENGL_GROPENGLSHADER_HH
