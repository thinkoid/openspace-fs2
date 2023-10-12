// -*- mode: c++; -*-

#ifndef FREESPACE2_CUTSCENE_FFMPEG_FFMPEGDECODER_HH
#define FREESPACE2_CUTSCENE_FFMPEG_FFMPEGDECODER_HH

#include <defs.hh>

#include <cutscene/Decoder.hh>

namespace cutscene {
namespace ffmpeg {
struct InputStream;

struct DecoderStatus;

class SubtitleDecoder;

class FFMPEGDecoder : public Decoder {
private:
    std::unique_ptr< InputStream > m_input;

    std::unique_ptr< InputStream > m_subtitleInput;

    std::unique_ptr< DecoderStatus > m_status;

    PlaybackProperties m_properties;

    bool hasExternalSubtitle () const;

    void runSubtitleDecoder (SubtitleDecoder* decoder);

public:
    FFMPEGDecoder ();

    ~FFMPEGDecoder () override;

    bool initialize (
        const std::string& fileName,
        const PlaybackProperties& properties) override;

    MovieProperties getProperties () const override;

    void startDecoding () override;

    bool hasAudio () const override;

    bool hasSubtitles () const override;

    void close () override;
};
} // namespace ffmpeg
} // namespace cutscene

#endif // FREESPACE2_CUTSCENE_FFMPEG_FFMPEGDECODER_HH
