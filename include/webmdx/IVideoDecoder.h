#pragma once
#include <span>
#include <memory>
#include "IDecodedVideoFrame.h"
#include "VideoTrack.h"

namespace wdx {
    /**
     * Interface for all video decoders
     */
    struct WEBMDX_API  IVideoDecoder {
        public:
        virtual ~IVideoDecoder() = default;
        /**
         * Decodes a video packet
         * @param input
         * @param timestamp
         */
        virtual void Decode(const std::span<std::uint8_t>& input, double timestamp) = 0;

        /**
         * Returns the decoded frame
         */
        virtual std::shared_ptr<IDecodedVideoFrame> GetFrame() = 0;

        static std::shared_ptr<IVideoDecoder> Create(const VideoTrack& track);
    };
}
