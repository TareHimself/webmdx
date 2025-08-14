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
        virtual void ToRgba(const std::span<std::uint8_t>& frame) = 0;
    };
}
