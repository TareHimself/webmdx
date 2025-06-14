#pragma once
#include <span>
#include <memory>
#include <vector>

#include "AudioTrack.h"

namespace wd {
    /**
     * Interface for all audio decoders
     */
    struct  IAudioDecoder {
        virtual ~IAudioDecoder() = default;

        /**
         * Decodes an audio packet into [-1,1] interleaved pcm
         * @param input the raw audio packet
         * @param pcm the resulting pcm data
         * @param timestamp
         * @return the number of samples decoded (total not per channel)
         */
        virtual int Decode(const std::span<std::uint8_t> &input, std::vector<float> &pcm, double timestamp) = 0;

        static std::shared_ptr<IAudioDecoder> Create(const AudioTrack& track);
    };

    std::shared_ptr<IAudioDecoder> makeAudioDecoder(const AudioTrack& track);
}
