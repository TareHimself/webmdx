#pragma once
#include <span>
#include <memory>
#include "IDecodedVideoFrame.h"
#include "Packet.h"
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
         */
        virtual void Decode(const std::shared_ptr<Packet>& input) = 0;

        virtual void Reset() = 0;
        /**
         * Returns the next decoded frame produced so far, or nullptr if none is ready.
         * A single Decode() call can yield zero, one, or multiple frames (lookahead/reordering),
         * so callers must call this in a loop after each Decode() until it returns nullptr.
         */
        virtual std::shared_ptr<IDecodedVideoFrame> GetFrame() = 0;

        static std::shared_ptr<IVideoDecoder> Create(const VideoTrack& track);
    };
}
