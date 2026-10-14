#pragma once
#include "webmdx/IDecodedVideoFrame.h"
#include <cstdint>
#include <vector>

namespace wdx {
    // A plane as a decoder hands it over, with its stride in bytes
    struct DecodedPlane {
        const std::uint8_t* data{nullptr};
        int stride{0};
        int height{0};
        int bytesPerSample{1};
        int bitDepth{8};
    };

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
        double time{0};

        [[nodiscard]] double GetTime() const override { return time; }
        void ToRgba(const std::span<std::uint8_t>& frame) override;
        void ToNV12(const std::span<std::uint8_t>& y, const std::span<std::uint8_t>& uv) override;

        // Stores a plane as 8 bit samples, dropping the low bits of deeper ones
        static void CopyPlane(std::vector<std::uint8_t>& plane, int& stride, const DecodedPlane& source);
    };
}
