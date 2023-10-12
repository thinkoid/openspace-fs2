// -*- mode: c++; -*-

#include <defs.hh>

#include <assert/assert.hh>
#include "freespace2/freespace.hh"
#include <gamesequence/gamesequence.hh>
#include <log/log.hh>
#include <osapi/osapi.hh>
#include <parse/parselo.hh>
#include <shared/types.hh>

#include <fcntl.h>
#include <sys/stat.h>

#include <mutex>
#include <filesystem>
namespace fs = std::filesystem;

namespace fs2 {
namespace os {

namespace {

bool is_active = false;

fs2::os::Viewport* main_viewport = 0;
SDL_Window*   main_window   = 0;

bool
default_window_handler (const SDL_Event& e) {
    ASSERT (main_window);

    if (fs2::os::events::isWindowEvent (e, main_window)) {
        switch (e.window.event) {
        case SDL_WINDOWEVENT_MINIMIZED:
        case SDL_WINDOWEVENT_FOCUS_LOST: {
            if (is_active) {
                game_pause ();
                is_active = false;
            }
            break;
        }
        case SDL_WINDOWEVENT_MAXIMIZED:
        case SDL_WINDOWEVENT_RESTORED:
        case SDL_WINDOWEVENT_FOCUS_GAINED: {
            if (!is_active) {
                game_unpause ();
                is_active = true;
            }
            break;
        }
        case SDL_WINDOWEVENT_CLOSE:
            gameseq_post_event (GS_EVENT_QUIT_GAME);
            break;
        }

        gr_activate (is_active);

        return true;
    }

    return false;
}

bool
default_quit_handler (const SDL_Event&) {
    gameseq_post_event (GS_EVENT_QUIT_GAME);
    return true;
}

} // namespace

////////////////////////////////////////////////////////////////////////

namespace events {

namespace {

// TODO: redo this shit
ListenerIdentifier nextListenerIdentifier;

struct EventListenerData {
    ListenerIdentifier identifier;
    Listener listener;

    uint32_t type;
    int weight;

    bool operator< (const EventListenerData& other) const {
        if (type < other.type) { return true; }
        if (type > other.type) { return false; }

        // Type is the same
        return weight < other.weight;
    }
};

bool compare_type (
    const EventListenerData& left, const EventListenerData& right) {
    return left.type < right.type;
}

std::vector< EventListenerData > eventListeners;

} // anonymous

ListenerIdentifier
addEventListener (SDL_EventType type, int weight, const Listener& listener) {
    ASSERTX (listener, "Invalid event handler passed!");

    EventListenerData data;
    data.identifier = ++nextListenerIdentifier;
    data.listener = listener;

    data.weight = weight;
    data.type = static_cast< uint32_t > (type);

    eventListeners.push_back (data);

    // This is suboptimal for runtime but we will iterate that vector often so
    // cache hits are more important
    // TODO: redo this hack
    std::sort (eventListeners.begin (), eventListeners.end ());

    return data.identifier;
}

bool
removeEventListener (ListenerIdentifier identifier) {
    auto endIter = end (eventListeners);
    for (auto iter = begin (eventListeners); iter != endIter; ++iter) {
        if (iter->identifier == identifier) {
            eventListeners.erase (iter);
            return true;
        }
    }

    return false;
}

bool
isWindowEvent (const SDL_Event& e, SDL_Window* window) {
    auto mainId = SDL_GetWindowID (window);

    switch (e.type) {
    case SDL_WINDOWEVENT: return mainId == e.window.windowID;
    case SDL_KEYDOWN:
    case SDL_KEYUP: return mainId == e.key.windowID;
    case SDL_TEXTEDITING: return mainId == e.edit.windowID;
    case SDL_TEXTINPUT: return mainId == e.text.windowID;
    case SDL_MOUSEMOTION: return mainId == e.motion.windowID;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP: return mainId == e.button.windowID;
    case SDL_MOUSEWHEEL: return mainId == e.wheel.windowID;
    default:
        // Event doesn't have a window ID
        return true;
    }
}

namespace {

std::vector< SDL_Event > event_buffer;

static void
process_event (const SDL_Event& event) {
    using namespace fs2::os::events;

    EventListenerData data;
    data.type = event.type;

    auto iter = std::lower_bound (
        eventListeners.begin (), eventListeners.end (), data, compare_type);

    if (iter != eventListeners.end ()) {
        //
        // The vector contains all event listeners, the listeners are sorted
        // for type and weight -> iterating through all listeners will yield
        // them in increasing weight order but we can only do this until we have
        // reached the end of the vector or the type has changed:
        //
        for (; iter != eventListeners.end () && iter->type == event.type;
             ++iter) {
            if (iter->listener (event)) {
                // Listener has handled the event
                break;
            }
        }
    }
}

} // anonymous

// TODO: re-api
void
buffer_all () {
    SDL_Event event;

    while (SDL_PollEvent (&event)) {
        // Add event to buffer
        event_buffer.push_back (event);
    }
}

void
process_all () {
    //
    // consume buffered events ...
    //
    for (const auto& event : event_buffer)
        process_event (event);

    event_buffer.clear ();

    //
    // ... and pending events.
    //
    for (SDL_Event event; SDL_PollEvent (&event);)
        process_event (event);
}

} // namespace events

////////////////////////////////////////////////////////////////////////

std::vector< std::unique_ptr< fs2::os::Viewport > > viewports;

Viewport*
addViewport (std::unique_ptr< Viewport >&& viewport) {
    auto port = viewport.get ();
    viewports.push_back (std::move (viewport));
    return port;
}

void
setMainViewPort (Viewport* mainView) {
    main_viewport = mainView;
    main_window = mainView->toSDLWindow ();
}

SDL_Window*
getSDLMainWindow () {
    return main_window;
}

Viewport*
getMainViewport () {
    return main_viewport;
}

void
closeAllViewports () {
    viewports.clear ();
}

////////////////////////////////////////////////////////////////////////

namespace {

fs::path config_path;

void init_config_path () {
    config_path = fs::path (SDL_GetPrefPath ("Volition", "Freespace2"));
}

void
do_init () {
    SDL_version v;
    SDL_GetVersion (&v);

    II << "SDL v" << v.major << "." << v.minor << "." <<  v.patch;

    if (SDL_Init (SDL_INIT_EVENTS) < 0) {
        EE << "SDL initialization failed : " << SDL_GetError ();
        exit (1);
    }

    LISTEN (
        SDL_WINDOWEVENT, fs2::os::events::DEFAULT_LISTENER_WEIGHT,
        default_window_handler);

    LISTEN (
        SDL_QUIT, fs2::os::events::DEFAULT_LISTENER_WEIGHT, default_quit_handler);
}

} // anonymous

fs::path
get_config_path () {
    static std::once_flag init_flag;
    std::call_once (init_flag, init_config_path);
    return config_path;
}

fs::path
get_config_path (const fs::path& suffix) {
    return get_config_path () /= suffix;
}

void init () {
    static std::once_flag init_flag;
    std::call_once (init_flag, do_init);
}

void title (const char* s) {
    ASSERT (main_window);
    SDL_SetWindowTitle (main_window, s);
}

const char* title () {
    ASSERT (main_window);
    return SDL_GetWindowTitle (main_window);
}

void fini () {
    fs2::os::closeAllViewports ();
    SDL_Quit ();
}

bool is_foreground () { return is_active; }

} // namespace os
} // namespace fs2


