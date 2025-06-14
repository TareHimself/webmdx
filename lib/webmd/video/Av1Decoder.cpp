#ifdef WEBMD_CODEC_VIDEO_DAV1D
#include "Av1Decoder.h"

#include <algorithm>
#include <cstring>

#include "webmd/errors.h"

namespace wd {
    void freeCallback(const std::uint8_t * data,void* cookie) {
        delete[] data;
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

        if (dav1d_send_data(_context, &_data) < 0) {
            throw AudioDecoderError("Failed to send data to dav1d");
        }
    }

    std::shared_ptr<IDecodedVideoFrame> Av1Decoder::GetFrame() {
        Dav1dPicture previousPicture = _latestPicture;

        while (dav1d_get_picture(_context, &_latestPicture) == 0) {
            dav1d_picture_unref(&previousPicture);
            previousPicture = _latestPicture;
        }

        auto result = std::make_shared<Av1Frame>();

    }

    Av1Decoder::~Av1Decoder() {

    }
}
#endif