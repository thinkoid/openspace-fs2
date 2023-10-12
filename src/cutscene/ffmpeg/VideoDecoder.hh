// -*- mode: c++; -*-

#ifndef FREESPACE2_CUTSCENE_FFMPEG_VIDEODECODER_HH
#define FREESPACE2_CUTSCENE_FFMPEG_VIDEODECODER_HH

#include <defs.hh>

#include "cutscene/ffmpeg/internal.hh"
#include "cutscene/ffmpeg/FFMPEGDecoder.hh"

namespace cutscene {
namespace ffmpeg {
class VideoDecoder : public FFMPEGStreamDecoder< VideoFrame > {
private:
    int m_frameId;
    SwsContext* m_swsCtx;
    AVPixelFormat m_destinationFormat;

    void convertAndPushPicture (const AVFrame* frame);

public:
    explicit VideoDecoder (
        DecoderStatus* status, AVPixelFormat destination_fmt);

    ~VideoDecoder () override;

    void decodePacket (AVPacket* packet) override;

    void finishDecoding () override;

    void flushBuffers () override;
};
} // namespace ffmpeg
} // namespace cutscene

#endif // FREESPACE2_CUTSCENE_FFMPEG_VIDEODECODER_HH
