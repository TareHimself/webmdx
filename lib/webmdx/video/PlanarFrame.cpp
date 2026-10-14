#include "PlanarFrame.h"
#include <cstring>
#include <libyuv.h>

namespace wdx {
    void PlanarFrame::CopyPlane(std::vector<std::uint8_t>& plane, int& stride, const DecodedPlane& source) {
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

    void PlanarFrame::ToRgba(const std::span<std::uint8_t> &frame) {
        if (alphaPlane.empty()) {
            alphaStride = yStride;
            alphaPlane = std::vector<uint8_t>(yPlane.size(), 255);
        }

        const auto convert = [&](const auto& convertPlanes) {
            convertPlanes(
                yPlane.data(), yStride,
                uPlane.data(), uStride,
                vPlane.data(), vStride,
                alphaPlane.data(), alphaStride,
                frame.data(), width * 4,
                width, height,
                0);
        };
        switch (layout) {
        case ChromaLayout::I420:
            convert(libyuv::I420AlphaToABGR);
            break;
        case ChromaLayout::I422:
            convert(libyuv::I422AlphaToABGR);
            break;
        case ChromaLayout::I444:
            convert(libyuv::I444AlphaToABGR);
            break;
        }
    }

    void PlanarFrame::ToNV12(const std::span<std::uint8_t>& y, const std::span<std::uint8_t>& uv) {
        switch (layout) {
        case ChromaLayout::I420:
            libyuv::I420ToNV12(
                yPlane.data(), yStride,
                uPlane.data(), uStride,
                vPlane.data(), vStride,
                y.data(), width,
                uv.data(), width,
                width, height);
            break;
        case ChromaLayout::I422: {
            // libyuv has no I422ToNV12, so halve the chroma rows first
            const auto chromaWidth = (width + 1) / 2;
            const auto chromaHeight = (height + 1) / 2;
            std::vector<std::uint8_t> u(static_cast<std::size_t>(chromaWidth) * chromaHeight);
            std::vector<std::uint8_t> v(u.size());
            libyuv::I422ToI420(
                yPlane.data(), yStride,
                uPlane.data(), uStride,
                vPlane.data(), vStride,
                y.data(), width,
                u.data(), chromaWidth,
                v.data(), chromaWidth,
                width, height);
            libyuv::MergeUVPlane(
                u.data(), chromaWidth,
                v.data(), chromaWidth,
                uv.data(), width,
                chromaWidth, chromaHeight);
            break;
        }
        case ChromaLayout::I444:
            libyuv::I444ToNV12(
                yPlane.data(), yStride,
                uPlane.data(), uStride,
                vPlane.data(), vStride,
                y.data(), width,
                uv.data(), width,
                width, height);
            break;
        }
    }
}
