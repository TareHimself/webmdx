#include "I420Frame.h"
#include <libyuv.h>

namespace wdx {
    void I420Frame::ToRgba(const std::span<std::uint8_t> &frame) {
        if (alphaPlane.empty()) {
            alphaStride = yStride;
            alphaPlane = std::vector<uint8_t>(yPlane.size(), 255);
        }

        libyuv::I420AlphaToABGR(
            yPlane.data(), yStride,
            uPlane.data(), uStride,
            vPlane.data(), vStride,
            alphaPlane.data(), alphaStride,
            frame.data(), width * 4,
            width, height,
            0);
    }

    void I420Frame::ToNV12(const std::span<std::uint8_t>& y, const std::span<std::uint8_t>& uv) {
        libyuv::I420ToNV12(
            yPlane.data(), yStride,
            uPlane.data(), uStride,
            vPlane.data(), vStride,
            y.data(), width,
            uv.data(), width,
            width, height);
    }
}
