// -*- mode: c++; -*-

#ifndef FREESPACE2_GRAPHICS_UNIFORMS_HH
#define FREESPACE2_GRAPHICS_UNIFORMS_HH

#include <defs.hh>

#include "graphics/util/uniform_structs.hh"
#include "material.hh"

/**
 * @file
 *
 * @brief Contains procedures for converting rendering data into their GPU
 * uniform representations
 */

namespace graphics {
namespace uniforms {

void convert_model_material (
    model_uniform_data* data_out, const model_material& material,
    const matrix4& model_transform, const vec3d& scale,
    size_t transform_buffer_offset);
}
} // namespace graphics

#endif // FREESPACE2_GRAPHICS_UNIFORMS_HH
