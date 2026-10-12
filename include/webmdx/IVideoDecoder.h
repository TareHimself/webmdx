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
         * Releases the frames the decoder is still holding once no more packets will follow.
         * SourceDecoder calls this when Demux() returns Finished, so drain GetFrame() after that.
         */
        virtual void Flush() = 0;
        /**
         * Returns the next decoded frame produced so far, or nullptr if none is ready.
         * A single Decode() call can yield zero, one, or multiple frames (lookahead/reordering),
         * so callers must call this in a loop after each Decode() until it returns nullptr.
         * A frame can come out several Decode() calls after its packet went in, so take its time
         * from IDecodedVideoFrame::GetTime(), not from the packet passed to the latest Decode().
         */
        virtual std::shared_ptr<IDecodedVideoFrame> GetFrame() = 0;

        static std::shared_ptr<IVideoDecoder> Create(const VideoTrack& track);
    };
}
