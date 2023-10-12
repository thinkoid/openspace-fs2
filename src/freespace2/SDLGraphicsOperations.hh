// -*- mode: c++; -*-

#ifndef FREESPACE2_FREESPACE2_SDLGRAPHICSOPERATIONS_HH
#define FREESPACE2_FREESPACE2_SDLGRAPHICSOPERATIONS_HH

#include <defs.hh>

#include <osapi/osapi.hh>

class SDLGraphicsOperations : public fs2::os::GraphicsOperations {
public:
    SDLGraphicsOperations ();
    ~SDLGraphicsOperations () override;

    std::unique_ptr< fs2::os::OpenGLContext > createOpenGLContext (
        fs2::os::Viewport* viewport,
        const fs2::os::OpenGLContextAttributes& gl_attrs) override;

    void makeOpenGLContextCurrent (
        fs2::os::Viewport* view, fs2::os::OpenGLContext* ctx) override;

    std::unique_ptr< fs2::os::Viewport >
    createViewport (const fs2::os::ViewPortProperties& props) override;
};

#endif // FREESPACE2_FREESPACE2_SDLGRAPHICSOPERATIONS_HH
