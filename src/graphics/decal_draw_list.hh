// -*- mode: c++; -*-

#ifndef FREESPACE2_GRAPHICS_DECAL_DRAW_LIST_HH
#define FREESPACE2_GRAPHICS_DECAL_DRAW_LIST_HH

#include <defs.hh>

#include <graphics/material.hh>
#include "graphics/util/UniformBuffer.hh"

namespace graphics {

class decal_draw_list {
    struct decal_draw_info {
        decal_material draw_mat;

        size_t uniform_offset;
    };
    std::vector< decal_draw_info > _draws;

    util::UniformBuffer* _buffer = nullptr;

    static bool
    sort_draws (const decal_draw_info& left, const decal_draw_info& right);

public:
    decal_draw_list ();
    ~decal_draw_list ();

    decal_draw_list (const decal_draw_list&) = delete;
    decal_draw_list& operator= (const decal_draw_list&) = delete;

    void add_decal (
        int diffuse_bitmap, int glow_bitmap, int normal_bitmap,
        float decal_timer, const matrix4& transform, float base_alpha);

    void render ();

    static void globalInit ();

    static void globalShutdown ();
};

} // namespace graphics

#endif // FREESPACE2_GRAPHICS_DECAL_DRAW_LIST_HH
