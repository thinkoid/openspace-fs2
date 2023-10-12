// -*- mode: c++; -*-

#ifndef FREESPACE2_CUTSCENE_FFMPEG_SUBTITLEDECODER_HH
#define FREESPACE2_CUTSCENE_FFMPEG_SUBTITLEDECODER_HH

#include <defs.hh>

#include "cutscene/ffmpeg/internal.hh"
#include "cutscene/ffmpeg/FFMPEGDecoder.hh"

namespace cutscene {
namespace ffmpeg {

class SubtitleDecoder : public FFMPEGStreamDecoder< SubtitleFrame > {
public:
    explicit SubtitleDecoder (DecoderStatus* status);

    ~SubtitleDecoder () override;

    void decodePacket (AVPacket* packet) override;

    void finishDecoding () override;
    void pushSubtitleFrame (AVPacket* subtitle, AVSubtitle* pSubtitle);
    void flushBuffers () override;
};

} // namespace ffmpeg
} // namespace cutscene

#endif // FREESPACE2_CUTSCENE_FFMPEG_SUBTITLEDECODER_HH
