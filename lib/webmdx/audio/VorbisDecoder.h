#pragma once
#ifdef WEBM_DX_CODEC_AUDIO_VORBIS
#include <vorbis/codec.h>
#include "webmdx/AudioTrack.h"
#include "webmdx/IAudioDecoder.h"

namespace wdx {
    class VorbisDecoder final : public IAudioDecoder {
    public:
        explicit VorbisDecoder(const AudioTrack& track);

        int Decode(const std::shared_ptr<Packet>& input, std::vector<float> &pcm) override;

        ~VorbisDecoder() override;
        void Reset() override;

    private:
        AudioTrack _track{};
        vorbis_info _info{};
        vorbis_comment _comment{};
        vorbis_dsp_state _state{};
        vorbis_block _block{};
    };
}
#endif
