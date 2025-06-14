#pragma once
#ifdef WEBMD_CODEC_AUDIO_VORBIS
#include <vorbis/codec.h>
#include "webmd/IAudioDecoder.h"

namespace wd {
    class VorbisDecoder final : public IAudioDecoder {
    public:
        explicit VorbisDecoder(const AudioTrack& track);
        int Decode(const std::span<std::uint8_t> &input, std::vector<float> &pcm, double timestamp) override;

        ~VorbisDecoder() override;

    private:
        AudioTrack _track{};
        vorbis_info _info{};
        vorbis_comment _comment{};
        vorbis_dsp_state _state{};
        vorbis_block _block{};
    };
}
#endif