#pragma once
#include "webmdx/IDecodedVideoFrame.h"
#include <cstdint>
#include <vector>

namespace wdx {
    // Planar YUV 4:2:0 frame storage shared by every video decoder backend
    // (dav1d and vpx both produce this layout), with stride-aware conversions
    // implemented once via libyuv instead of duplicated per backend.
    struct I420Frame final : IDecodedVideoFrame {
        std::vector<std::uint8_t> yPlane{};
        std::vector<std::uint8_t> uPlane{};
        std::vector<std::uint8_t> vPlane{};
        std::vector<std::uint8_t> alphaPlane{};
        int width{0};
        int height{0};
        int yStride{0};
        int uStride{0};
        int vStride{0};
        int alphaStride{0};

        void ToRgba(const std::span<std::uint8_t>& frame) override;
        void ToNV12(const std::span<std::uint8_t>& y, const std::span<std::uint8_t>& uv) override;
    };
}
