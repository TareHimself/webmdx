#ifdef WEBM_DX_CODEC_VIDEO_VPX
#include "VpxDecoder.h"
#include <cmath>
#include <cstring>
#include <thread>
#include "webmdx/errors.h"

namespace wdx {
    VpxDecoder::VpxDecoder(const VideoTrack &track) {
        _track = track;
        Init();
    }

    void VpxDecoder::Decode(const std::shared_ptr<Packet>& input) {
        const auto data = input->GetData();
        if (const auto decodeResult = vpx_codec_decode(&_codec,data.data(),data.size(), nullptr, 0); decodeResult != VPX_CODEC_OK) {
            throw VideoDecoderError(std::string("Failed to decode vpx packet ") + vpx_codec_error(&_codec));
        }
        // A fresh iterator starts a new pull over the frames this Decode() made available;
        // vpx_codec_decode can surface more than one frame per call (e.g. VP9 superframes).
        _iter = nullptr;
    }

    std::shared_ptr<IDecodedVideoFrame> VpxDecoder::GetFrame() {
        const vpx_image_t *frame = vpx_codec_get_frame(&_codec, &_iter);
        if (frame == nullptr) {
            return nullptr;
        }

        auto result = std::make_shared<I420Frame>();
        result->width = static_cast<int>(frame->d_w);
        result->height = static_cast<int>(frame->d_h);
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

    void VpxDecoder::Reset()
    {
        vpx_codec_destroy(&_codec);
        Init();
    }

    void VpxDecoder::Flush()
    {
        if (const auto flushResult = vpx_codec_decode(&_codec, nullptr, 0, nullptr, 0); flushResult != VPX_CODEC_OK) {
            throw VideoDecoderError(std::string("Failed to flush vpx decoder ") + vpx_codec_error(&_codec));
        }
        _iter = nullptr;
    }

    void VpxDecoder::Init()
    {
        _iter = nullptr;
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
}
#endif