#pragma once
#ifdef WEBM_DX_CODEC_AUDIO_OPUS
#include "opus/opus.h"
#include "webmdx/AudioTrack.h"
#include "webmdx/IAudioDecoder.h"

namespace wdx {
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
