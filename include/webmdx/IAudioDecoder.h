#pragma once
#include <span>
#include <memory>
#include <vector>

#include "AudioTrack.h"
#include "Packet.h"

namespace wdx {
    /**
     * Interface for all audio decoders
     */
    struct WEBMDX_API IAudioDecoder {
        virtual ~IAudioDecoder() = default;

        /**
         * Decodes an audio packet into [-1,1] interleaved pcm
         * @param input the raw audio packet
         * @param pcm the resulting pcm data
         * @return the number of samples decoded (total not per channel)
         */
        virtual int Decode(const std::shared_ptr<Packet>& input, std::vector<float> &pcm) = 0;

        virtual void Reset() = 0;

        static std::shared_ptr<IAudioDecoder> Create(const AudioTrack& track);
    };

    std::shared_ptr<IAudioDecoder> makeAudioDecoder(const AudioTrack& track);
}
