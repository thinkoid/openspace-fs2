// -*- mode: c++; -*-

#ifndef FREESPACE2_GRAPHICS_OPENGL_GROPENGLSYNC_HH
#define FREESPACE2_GRAPHICS_OPENGL_GROPENGLSYNC_HH

#include <defs.hh>

#include "graphics/opengl/gropengl.hh"

gr_sync gr_opengl_sync_fence ();
bool gr_opengl_sync_wait (gr_sync sync, uint64_t timeoutns);
void gr_opengl_sync_delete (gr_sync sync);

#endif // FREESPACE2_GRAPHICS_OPENGL_GROPENGLSYNC_HH
