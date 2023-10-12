// -*- mode: c++; -*-

#ifndef FREESPACE2_GRAPHICS_OPENGL_SHADERPROGRAM_HH
#define FREESPACE2_GRAPHICS_OPENGL_SHADERPROGRAM_HH

#include <defs.hh>

#include "gropenglshader.hh"

#include "glad/glad.h"

namespace opengl {

class ShaderProgram;
class ShaderUniforms {
    struct uniform_bind {
        std::string name;

        enum data_type { INT, FLOAT, VEC2, VEC3, VEC4, MATRIX4 };

        uniform_bind::data_type type;
        size_t index;

        int count;
        int tranpose;
    };

    ShaderProgram* _program;

    std::vector< uniform_bind > _uniforms;

    std::vector< int > _uniform_data_ints;
    std::vector< float > _uniform_data_floats;
    std::vector< vec2d > _uniform_data_vec2d;
    std::vector< vec3d > _uniform_data_vec3d;
    std::vector< vec4 > _uniform_data_vec4;
    std::vector< matrix4 > _uniform_data_matrix4;

    std::unordered_map< std::string, size_t > _uniform_lookup;

    std::unordered_map< std::string, GLint > _uniform_locations;

    size_t findUniform (const std::string& name);
    GLint findUniformLocation (const std::string& name);

public:
    explicit ShaderUniforms (ShaderProgram* shaderProgram);

    void setUniformi (const std::string& name, const int value);
    void
    setUniform1iv (const std::string& name, const int count, const int* val);
    void setUniformf (const std::string& name, const float value);
    void setUniform2f (const std::string& name, const float x, const float y);
    void setUniform2f (const std::string& name, const vec2d& val);
    void setUniform3f (
        const std::string& name, const float x, const float y, const float z);
    void setUniform3f (const std::string& name, const vec3d& value);
    void setUniform4f (
        const std::string& name, const float x, const float y, const float z,
        const float w);
    void setUniform4f (const std::string& name, const vec4& val);
    void
    setUniform1fv (const std::string& name, const int count, const float* val);
    void
    setUniform3fv (const std::string& name, const int count, const vec3d* val);
    void
    setUniform4fv (const std::string& name, const int count, const vec4* val);
    void setUniformMatrix4fv (
        const std::string& name, const int count, const matrix4* value);
    void setUniformMatrix4f (const std::string& name, const matrix4& val);
};

enum ShaderStage { STAGE_VERTEX, STAGE_GEOMETRY, STAGE_FRAGMENT };

class ShaderProgram {
    GLuint _program_id;

    std::vector< GLuint > _compiled_shaders;

    std::unordered_map< opengl_vert_attrib::attrib_id, GLint >
        _attribute_locations;

    void freeCompiledShaders ();

public:
    explicit ShaderProgram (const std::string& program_name);
    ~ShaderProgram ();

    ShaderUniforms Uniforms;

    ShaderProgram (const ShaderProgram&) = delete;
    ShaderProgram& operator= (const ShaderProgram&) = delete;

    ShaderProgram (ShaderProgram&& other) noexcept;
    ShaderProgram& operator= (ShaderProgram&& other) noexcept;

    void use ();

    void addShaderCode (
        ShaderStage stage, const std::string& name,
        const std::vector< std::string >& codeParts);

    void linkProgram ();

    void initAttribute (
        const std::string& name, opengl_vert_attrib::attrib_id attr_id,
        const vec4& default_value);

    GLint getAttributeLocation (opengl_vert_attrib::attrib_id attribute);

    GLuint getShaderHandle ();
};

} // namespace opengl

#endif // FREESPACE2_GRAPHICS_OPENGL_SHADERPROGRAM_HH
