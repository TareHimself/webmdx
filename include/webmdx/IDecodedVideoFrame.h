#pragma once
#include <span>
#include <cstdint>
#include <vector>
#include "export.h"
namespace wdx {

    /**
     * The decoder specific representation of a video frame, must be valid even if the decoder has been destroyed
     */
    struct WEBMDX_API  IDecodedVideoFrame {
        virtual ~IDecodedVideoFrame() = default;
        /**
         * The time, in seconds, of the packet this frame was decoded from
         */
        [[nodiscard]] virtual double GetTime() const = 0;
        virtual void ToRgba(const std::span<std::uint8_t>& frame) = 0;
        virtual void ToNV12(const std::span<std::uint8_t>& y,const std::span<std::uint8_t>& uv) = 0;
    };
}
