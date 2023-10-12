// -*- mode: c++; -*-

#ifndef FREESPACE2_CUTSCENE_VIDEOPRESENTER_HH
#define FREESPACE2_CUTSCENE_VIDEOPRESENTER_HH

#include <defs.hh>

#include "graphics/2d.hh"
#include <graphics/material.hh>
#include <cutscene/Decoder.hh>

#include <memory>

namespace cutscene {
namespace player {

class VideoPresenter {
    MovieProperties _properties;

    std::array< int, 3 > _planeTextureHandles;
    std::array< std::unique_ptr< uint8_t[] >, 3 > _planeTextureBuffers;

    movie_material _movie_material;
    material _rgb_material; // Material used when a RGB/RGBA movie is played
public:
    explicit VideoPresenter (const MovieProperties& props);

    // Disallow copying
    VideoPresenter (const VideoPresenter&) = delete;
    VideoPresenter& operator= (const VideoPresenter&) = delete;

    virtual ~VideoPresenter ();

    void uploadVideoFrame (const VideoFramePtr& frame);

    void displayFrame (float x1, float y1, float x2, float y2);
};
} // namespace player
} // namespace cutscene

#endif // FREESPACE2_CUTSCENE_VIDEOPRESENTER_HH
