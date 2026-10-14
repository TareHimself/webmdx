#include "I420Frame.h"
#include <cstring>
#include <libyuv.h>

namespace wdx {
    void I420Frame::CopyPlane(std::vector<std::uint8_t>& plane, int& stride, const DecodedPlane& source) {
        if (source.bytesPerSample == 1) {
            stride = source.stride;
            plane.resize(static_cast<std::size_t>(source.height) * stride);
            std::memcpy(plane.data(), source.data, plane.size());
            return;
        }
        stride = source.stride / 2;
        plane.resize(static_cast<std::size_t>(source.height) * stride);
        libyuv::Convert16To8Plane(
            reinterpret_cast<const std::uint16_t*>(source.data), stride,
            plane.data(), stride,
            1 << (24 - source.bitDepth),
            stride, source.height);
    }

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
