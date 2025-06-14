#ifdef WEBMD_CODEC_VIDEO_VPX
#include "VpxDecoder.h"
#include <algorithm>
#include <cstring>
#include <thread>
#include "webmd/errors.h"
#include <libyuv.h>

namespace wd {
    void VpxFrame::ToRgba(std::vector<std::uint8_t> &frame) {
        auto targetFrameSize = width * height * 4;
        if (frame.size() != targetFrameSize) {
            frame.resize(targetFrameSize); // RGBA = 4 bytes per pixel
        }
        libyuv::I420ToRGBA(yPlane.data(),yStride,uPlane.data(),uStride,vPlane.data(),vStride,frame.data(),width * 4,width,height);
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
        result->yStride = frame->stride[0];
        result->uStride = frame->stride[1];
        result->vStride = frame->stride[2];
        result->yPlane.reserve(result->height * result->yStride);
        result->uPlane.reserve(((result->height + 1) / 2) * result->uStride);
        result->vPlane.reserve(((result->height + 1) / 2) * result->vStride);

        memcpy(result->yPlane.data(),frame->planes[0],result->yPlane.size());
        memcpy(result->uPlane.data(),frame->planes[1],result->uPlane.size());
        memcpy(result->vPlane.data(),frame->planes[2],result->vPlane.size());
        return result;
    }

    VpxDecoder::~VpxDecoder() {
        vpx_codec_destroy(&_codec);
    }
}
#endif