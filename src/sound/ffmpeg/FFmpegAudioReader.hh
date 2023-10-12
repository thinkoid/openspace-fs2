// -*- mode: c++; -*-

#ifndef FREESPACE2_SOUND_FFMPEG_FFMPEGAUDIOREADER_HH
#define FREESPACE2_SOUND_FFMPEG_FFMPEGAUDIOREADER_HH

#include <defs.hh>

#include "libs/ffmpeg/FFmpegHeaders.hh"

namespace ffmpeg {

class FFmpegAudioReader {
    int _stream_idx = -1;
    AVFormatContext* _format_ctx = nullptr;
    AVCodecContext* _codec_ctx = nullptr;

#if LIBAVCODEC_VERSION_INT <= AV_VERSION_INT(57, 24, 255)
    AVPacket* _currentPacket = nullptr;
#endif

public:
    FFmpegAudioReader (
        AVFormatContext* av_format_context, AVCodecContext* codec_ctx,
        int stream_idx);
    ~FFmpegAudioReader ();

    bool readFrame (AVFrame* decode_frame);
};

} // namespace ffmpeg

#endif // FREESPACE2_SOUND_FFMPEG_FFMPEGAUDIOREADER_HH
