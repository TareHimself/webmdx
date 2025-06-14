#ifdef WEBM_DX_CODEC_VIDEO_VPX
#include "VpxDecoder.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <thread>
#include "webmdx/errors.h"
#include <libyuv.h>

namespace wdx {
    void VpxFrame::ToRgba(const std::span<std::uint8_t> &frame) {

        if (alphaPlane.empty()) {
            alphaStride = yStride;
            alphaPlane = std::vector<uint8_t>(yPlane.size(), 255);
        }
        libyuv::I420AlphaToABGR(
        yPlane.data(),
       yStride,
       uPlane.data(),
       uStride,
       vPlane.data(),
       vStride,
       alphaPlane.data(),
       alphaStride,
       frame.data(),
       static_cast<int>(width) * 4,
       static_cast<int>(width),
       static_cast<int>(height),
       0
       );
        return;
        libyuv::I420ToRAW(
            yPlane.data(),
            yStride,
            uPlane.data(),
            uStride,
            vPlane.data(),
            vStride,
            frame.data(),
            static_cast<int>(width) * 4,
            static_cast<int>(width),
            static_cast<int>(height));
        // for (int y = 0; y < height; ++y) {
        //     for (int x = 0; x < width; ++x) {
        //         const int y_val = yPlane[y * yStride + x];
        //         const int u_val = uPlane[(y / 2) * uStride + (x / 2)];
        //         const int v_val = vPlane[(y / 2) * vStride + (x / 2)];
        //
        //         // Convert YUV to RGB
        //         const int c = y_val - 16;
        //         const int d = u_val - 128;
        //         const int e = v_val - 128;
        //
        //         int r = (298 * c + 409 * e + 128) >> 8;
        //         int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
        //         int b = (298 * c + 516 * d + 128) >> 8;
        //
        //         const int index = (y * width + x) * 4;
        //         frame[index + 0] = std::clamp(r, 0, 255); // R
        //         frame[index + 1] = std::clamp(g, 0, 255); // G
        //         frame[index + 2] = std::clamp(b, 0, 255); // B
        //         frame[index + 3] = 255; // A
        //     }
        // }
    }

    VpxDecoder::VpxDecoder(const VideoTrack &track) {
        _track = track;
        vpx_codec_dec_cfg_t cfg = {};
        cfg.h = _track.height;
        cfg.w = _track.width;
        cfg.threads = std::thread::hardware_concurrency();

        switch (_track.codec) {
            case VideoCodec::Vpx8: {
                if (vpx_codec_dec_init(&_codec, vpx_codec_vp8_dx(), &cfg, 0)) {
                    throw VideoDecoderError("Failed to create vpx8 decoder");
                }
            }
                break;
            case VideoCodec::Vpx9: {
                if (vpx_codec_dec_init(&_codec, vpx_codec_vp9_dx(), &cfg, 0)) {
                    throw VideoDecoderError("Failed to create vpx9 decoder");
                }
            }
                break;
            default:
                throw VideoDecoderError("This decoder only supports vpx");
                break;
        }
    }

    void VpxDecoder::Decode(const std::span<std::uint8_t> &input, double timestamp) {
        if (const auto decodeResult = vpx_codec_decode(&_codec,input.data(),input.size(), nullptr, 0); decodeResult != VPX_CODEC_OK) {
            throw VideoDecoderError("Failed to decode vpx packet");
        }
    }

    std::shared_ptr<IDecodedVideoFrame> VpxDecoder::GetFrame() {
        vpx_codec_iter_t iter = nullptr;
        const vpx_image_t *frame = vpx_codec_get_frame(&_codec, &iter);

        auto result = std::make_shared<VpxFrame>();
        result->width = frame->d_w;
        result->height = frame->d_h;
        result->yStride = frame->stride[VPX_PLANE_Y];
        result->uStride = frame->stride[VPX_PLANE_U];
        result->vStride = frame->stride[VPX_PLANE_V];

        result->yPlane.resize(result->height * result->yStride);

        auto halfHeight = static_cast<int>(std::floor(static_cast<float>(result->height) / 2));
        result->uPlane.resize(halfHeight * result->uStride);
        result->vPlane.resize(halfHeight * result->vStride);



        memcpy(result->yPlane.data(),frame->planes[VPX_PLANE_Y],result->yPlane.size());
        memcpy(result->uPlane.data(),frame->planes[VPX_PLANE_U],result->uPlane.size());
        memcpy(result->vPlane.data(),frame->planes[VPX_PLANE_V],result->vPlane.size());

        if (auto alphaPlane = frame->planes[VPX_PLANE_ALPHA]) {
            result->alphaStride = frame->stride[VPX_PLANE_ALPHA];
            result->alphaPlane.resize(result->height * result->alphaStride);
            memcpy(result->alphaPlane.data(),alphaPlane,result->alphaPlane.size());
        }
        return result;
    }

    VpxDecoder::~VpxDecoder() {
        vpx_codec_destroy(&_codec);
    }
}
#endif