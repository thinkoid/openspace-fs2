// -*- mode: c++; -*-

#ifndef FREESPACE2_LIBS_FFMPEG_LIBAVCOMPATIBILITY_HH
#define FREESPACE2_LIBS_FFMPEG_LIBAVCOMPATIBILITY_HH

#include <defs.hh>

// swresample typedefs and macros

// typedefs
typedef AVAudioResampleContext SwrContext;

// Function compatibility
#define swr_alloc avresample_alloc_context
#define swr_init avresample_open
#define swr_free avresample_free
#define swr_convert_frame avresample_convert_frame

#endif // FREESPACE2_LIBS_FFMPEG_LIBAVCOMPATIBILITY_HH
