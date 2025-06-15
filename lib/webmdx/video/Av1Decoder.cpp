#ifdef WEBM_DX_CODEC_VIDEO_DAV1D
#include "Av1Decoder.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <libyuv.h>
#include "webmdx/errors.h"

namespace wdx {
    void freeCallback(const std::uint8_t * data,void* cookie) {
        delete[] data;
    }


    void Av1Frame::ToRgba(const std::span<std::uint8_t> &frame) {
        if (alphaPlane.empty()) {
            alphaStride = yStride;
            alphaPlane = std::vector<uint8_t>(yPlane.size(), 255);
        }

        libyuv::I420AlphaToABGR(
        yPlane.data(),
       yStride,
       uPlane.data(),
       uvStride,
       vPlane.data(),
       uvStride,
       alphaPlane.data(),
       alphaStride,
       frame.data(),
       static_cast<int>(width) * 4,
       static_cast<int>(width),
       static_cast<int>(height),
       0
       );
        return;
    }

    Av1Decoder::Av1Decoder(const VideoTrack &track) {
        _track = track;
        dav1d_picture_unref(&_latestPicture);
        dav1d_default_settings(&_initSettings);
        if (const auto result = dav1d_open(&_context,&_initSettings); result < 0) {
            throw AudioDecoderError(std::string("Failed to create Av1 decoder: "));
        }
    }

    void Av1Decoder::Decode(const std::span<std::uint8_t> &input, double timestamp) {
        dav1d_data_unref(&_data);
        const auto data = new std::uint8_t[input.size()];
        std::memcpy(data, input.data(), input.size());
        if (dav1d_data_wrap(&_data,data,input.size(),freeCallback, nullptr) < 0) {
            delete[] data;
            throw AudioDecoderError("Failed to wrap AV1 data");
        }

        auto hasCheckedBuffer = false;
        auto result = 0;
        do {
            result = dav1d_send_data(_context, &_data);

            if (result < 0 && hasCheckedBuffer) {
                throw AudioDecoderError("Failed to send data to dav1d");
            }

            if (result < 0) {
                hasCheckedBuffer = true;
                AdvanceToLatestPicture();
            }
        }while (result < 0);

    }

    std::shared_ptr<IDecodedVideoFrame> Av1Decoder::GetFrame() {
        AdvanceToLatestPicture();

        auto result = std::make_shared<Av1Frame>();
        result->width = _latestPicture.p.w;
        result->height = _latestPicture.p.h;
        result->yStride = _latestPicture.stride[0];
        result->uvStride = _latestPicture.stride[1];
        auto halfHeight = static_cast<int>(std::floor(static_cast<float>(result->height) / 2));
        result->yPlane.resize(result->height * result->yStride);
        result->uPlane.resize(halfHeight * result->uvStride);
        result->vPlane.resize(halfHeight * result->uvStride);

        memcpy(result->yPlane.data(),_latestPicture.data[0],result->yPlane.size());
        memcpy(result->uPlane.data(),_latestPicture.data[1],result->uPlane.size());
        memcpy(result->vPlane.data(),_latestPicture.data[2],result->vPlane.size());
        return result;
    }

    Av1Decoder::~Av1Decoder() {
        AdvanceToLatestPicture();
        dav1d_picture_unref(&_latestPicture);
        dav1d_data_unref(&_data);
        dav1d_close(&_context);
    }

    void Av1Decoder::AdvanceToLatestPicture() {
        Dav1dPicture previousPicture = _latestPicture;

        while (dav1d_get_picture(_context, &_latestPicture) == 0) {
            dav1d_picture_unref(&previousPicture);
            previousPicture = _latestPicture;
        }
    }
}
#endif