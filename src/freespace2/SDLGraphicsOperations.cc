// -*- mode: c++; -*-

#include <defs.hh>
#include <assert/assert.hh>
#include <log/log.hh>

//
//

#include "SDLGraphicsOperations.hh"

namespace {
void setOGLProperties (const fs2::os::ViewPortProperties& props) {
    SDL_GL_ResetAttributes ();

    SDL_GL_SetAttribute (SDL_GL_RED_SIZE, props.pixel_format.red_size);
    SDL_GL_SetAttribute (SDL_GL_GREEN_SIZE, props.pixel_format.green_size);
    SDL_GL_SetAttribute (SDL_GL_BLUE_SIZE, props.pixel_format.blue_size);
    SDL_GL_SetAttribute (SDL_GL_DEPTH_SIZE, props.pixel_format.depth_size);
    SDL_GL_SetAttribute (SDL_GL_STENCIL_SIZE, props.pixel_format.stencil_size);
    SDL_GL_SetAttribute (SDL_GL_DOUBLEBUFFER, 1);
    // disabled due to issues with implementation; may be re-enabled in future
    SDL_GL_SetAttribute (SDL_GL_MULTISAMPLEBUFFERS, 0);
    SDL_GL_SetAttribute (SDL_GL_MULTISAMPLESAMPLES, 0);

    WARNINGF (LOCATION,"  Requested SDL Pixel values = R: %d, G: %d, B: %d, depth: %d, stencil: %d, double-buffer: %d, FSAA: %d",props.pixel_format.red_size, props.pixel_format.green_size,props.pixel_format.blue_size, props.pixel_format.depth_size,props.pixel_format.stencil_size, 1, props.pixel_format.multi_samples);

    SDL_GL_SetAttribute (SDL_GL_ACCELERATED_VISUAL, 1);
    SDL_GL_SetAttribute (
        SDL_GL_CONTEXT_MAJOR_VERSION, props.gl_attributes.major_version);
    SDL_GL_SetAttribute (
        SDL_GL_CONTEXT_MINOR_VERSION, props.gl_attributes.minor_version);

    int profile;
    switch (props.gl_attributes.profile) {
    case fs2::os::OpenGLProfile::Core: profile = SDL_GL_CONTEXT_PROFILE_CORE; break;
    case fs2::os::OpenGLProfile::Compatibility:
        profile = SDL_GL_CONTEXT_PROFILE_COMPATIBILITY;
        break;
    default: ASSERT (0); return;
    }
    SDL_GL_SetAttribute (SDL_GL_CONTEXT_PROFILE_MASK, profile);

    int flags = 0;
    if (props.gl_attributes.flags[fs2::os::OpenGLContextFlags::Debug]) {
        flags |= SDL_GL_CONTEXT_DEBUG_FLAG;
    }
    if (props.gl_attributes.flags[fs2::os::OpenGLContextFlags::ForwardCompatible]) {
        flags |= SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
    }
    SDL_GL_SetAttribute (SDL_GL_CONTEXT_FLAGS, flags);
}
} // namespace

class SDLOpenGLContext : public fs2::os::OpenGLContext {
    SDL_GLContext _glCtx;

public:
    SDLOpenGLContext (SDL_GLContext sdl_gl_context)
        : _glCtx (sdl_gl_context) {}

    ~SDLOpenGLContext () override { SDL_GL_DeleteContext (_glCtx); }

    fs2::os::OpenGLLoadProc getLoaderFunction () override {
        return SDL_GL_GetProcAddress;
    }

    void makeCurrent (SDL_Window* window) {
        SDL_GL_MakeCurrent (window, _glCtx);
    }

    void setSwapInterval (int status) override {
        SDL_GL_SetSwapInterval (status);
    }
};
class SDLWindowViewPort : public fs2::os::Viewport {
    SDL_Window* _window;
    fs2::os::ViewPortProperties _props;

public:
    SDLWindowViewPort (SDL_Window* window, const fs2::os::ViewPortProperties& props)
        : _window (window), _props (props) {
        ASSERTX (window != nullptr, "Invalid window specified");
    }
    ~SDLWindowViewPort () override {
        SDL_DestroyWindow (_window);
        _window = nullptr;
    }

    const fs2::os::ViewPortProperties& getProps () const { return _props; }
    SDL_Window* toSDLWindow () override { return _window; }
    std::pair< uint32_t, uint32_t > getSize () override {
        int width, height;
        SDL_GetWindowSize (_window, &width, &height);

        return std::make_pair (width, height);
    }
    void swapBuffers () override { SDL_GL_SwapWindow (_window); }
    void setState (fs2::os::ViewportState state) override {
        switch (state) {
        case fs2::os::ViewportState::Windowed:
            SDL_SetWindowFullscreen (_window, 0);
            SDL_SetWindowBordered (_window, SDL_FALSE);
            break;
        case fs2::os::ViewportState::Borderless:
            SDL_SetWindowFullscreen (_window, 0);
            SDL_SetWindowBordered (_window, SDL_TRUE);
            break;
        case fs2::os::ViewportState::Fullscreen:
            SDL_SetWindowFullscreen (_window, SDL_WINDOW_FULLSCREEN);
            break;
        default: ASSERT (0); break;
        }
    }
    void minimize () override {
        // lets not minimize if we are in windowed mode
        if (!(SDL_GetWindowFlags (_window) & SDL_WINDOW_FULLSCREEN)) {
            return;
        }

        SDL_MinimizeWindow (_window);
    }

    void restore () override { SDL_RestoreWindow (_window); }
};

SDLGraphicsOperations::SDLGraphicsOperations () {
    WARNINGF (LOCATION, "  Initializing SDL video...");

    // Slight hack to make Mesa advertise S3TC support without libtxc_dxtn
    setenv ("force_s3tc_enable", "true", 1);

    if (SDL_InitSubSystem (SDL_INIT_VIDEO) < 0) {
        ASSERTX (0, "Couldn't init SDL video: %s", SDL_GetError ());
        return;
    }
}
SDLGraphicsOperations::~SDLGraphicsOperations () {
    SDL_QuitSubSystem (SDL_INIT_VIDEO);
}
std::unique_ptr< fs2::os::Viewport >
SDLGraphicsOperations::createViewport (const fs2::os::ViewPortProperties& props) {
    uint32_t windowflags = SDL_WINDOW_SHOWN;
    if (props.enable_opengl) {
        windowflags |= SDL_WINDOW_OPENGL;
        setOGLProperties (props);
    }
    if (props.flags[fs2::os::ViewPortFlags::Borderless]) {
        windowflags |= SDL_WINDOW_BORDERLESS;
    }
    if (props.flags[fs2::os::ViewPortFlags::Fullscreen]) {
        windowflags |= SDL_WINDOW_FULLSCREEN;
    }
    if (props.flags[fs2::os::ViewPortFlags::Resizeable]) {
        windowflags |= SDL_WINDOW_RESIZABLE;
    }

    SDL_Rect bounds;
    if (SDL_GetDisplayBounds (props.display, &bounds) != 0) {
        ERRORF (
            LOCATION, "Failed to get display bounds: %s\n", SDL_GetError ());
        return nullptr;
    }

    int x;
    int y;

    if (bounds.w == (int)props.width && bounds.h == (int)props.height) {
        // If we have the same size as the desktop we explicitly specify 0,0 to
        // make sure that the window borders aren't hidden
        WARNINGF (LOCATION,"SDL: Creating window at %d, %d because window has same size as desktop.",bounds.x, bounds.y);
        x = bounds.x;
        y = bounds.y;
    }
    else {
        x = SDL_WINDOWPOS_CENTERED_DISPLAY (props.display);
        y = SDL_WINDOWPOS_CENTERED_DISPLAY (props.display);
    }

    SDL_Window* window = SDL_CreateWindow (
        props.title.c_str (), x, y, props.width, props.height, windowflags);
    if (window == nullptr) {
        ERRORF (
            LOCATION, "Failed to create SDL Window: %s\n", SDL_GetError ());
        return nullptr;
    }

    return std::unique_ptr< fs2::os::Viewport > (
        new SDLWindowViewPort (window, props));
}
void SDLGraphicsOperations::makeOpenGLContextCurrent (
    fs2::os::Viewport* view, fs2::os::OpenGLContext* ctx) {
    if (view == nullptr && ctx == nullptr) {
        SDL_GL_MakeCurrent (nullptr, nullptr);
        return;
    }

    ASSERTX (
        view != nullptr,
        "Both viewport of context must be valid at this point!");
    ASSERTX (
        ctx != nullptr,
        "Both viewport of context must be valid at this point!");

    auto sdlCtx = reinterpret_cast< SDLOpenGLContext* > (ctx);
    sdlCtx->makeCurrent (view->toSDLWindow ());
}
std::unique_ptr< fs2::os::OpenGLContext >
SDLGraphicsOperations::createOpenGLContext (
    fs2::os::Viewport* viewport, const fs2::os::OpenGLContextAttributes& gl_attrs) {
    auto sdlViewport = reinterpret_cast< SDLWindowViewPort* > (viewport);

    auto props = sdlViewport->getProps ();
    props.gl_attributes = gl_attrs;
    setOGLProperties (props);

    auto ctx = SDL_GL_CreateContext (viewport->toSDLWindow ());

    if (ctx == nullptr) {
        ERRORF (
            LOCATION, "Could not create OpenGL Context: %s\n",
            SDL_GetError ());
        return nullptr;
    }

    int r, g, b, depth, stencil, db, fsaa_samples;
    SDL_GL_GetAttribute (SDL_GL_RED_SIZE, &r);
    SDL_GL_GetAttribute (SDL_GL_GREEN_SIZE, &g);
    SDL_GL_GetAttribute (SDL_GL_BLUE_SIZE, &b);
    SDL_GL_GetAttribute (SDL_GL_DEPTH_SIZE, &depth);
    SDL_GL_GetAttribute (SDL_GL_DOUBLEBUFFER, &db);
    SDL_GL_GetAttribute (SDL_GL_STENCIL_SIZE, &stencil);
    SDL_GL_GetAttribute (SDL_GL_MULTISAMPLESAMPLES, &fsaa_samples);

    WARNINGF (LOCATION,"  Actual SDL Video values    = R: %d, G: %d, B: %d, depth: %d, stencil: %d, double-buffer: %d, FSAA: %d",r, g, b, depth, stencil, db, fsaa_samples);

    return std::unique_ptr< fs2::os::OpenGLContext > (new SDLOpenGLContext (ctx));
}
