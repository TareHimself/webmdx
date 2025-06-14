#pragma once
#ifdef WEBMD_CODEC_AUDIO_OPUS
#include "opus/opus.h"
#include "webmd/AudioTrack.h"
#include "webmd/IAudioDecoder.h"

namespace wd {
    class OpusDecoder final : public IAudioDecoder {
    public:
        explicit OpusDecoder(const AudioTrack& track);

        int Decode(const std::span<std::uint8_t> &input, std::vector<float> &pcm, double timestamp) override;

        ~OpusDecoder() override;
    private:
        ::OpusDecoder * _decoder{};
        AudioTrack _track{};
    };
}
#endif
