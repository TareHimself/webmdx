#ifdef WEBMD_CODEC_AUDIO_VORBIS
#include "VorbisDecoder.h"
#include <vorbis/codec.h>
namespace wd {
    VorbisDecoder::VorbisDecoder(const AudioTrack &track) {
        _track = track;
        vorbis_info_init(&_info);
        vorbis_comment_init(&_comment);
    }

    int VorbisDecoder::Decode(const std::span<std::uint8_t> &input, std::vector<float> &pcm, double timestamp) {
        return 0;
    }

    VorbisDecoder::~VorbisDecoder() = default;
}
#endif