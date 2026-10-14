#ifdef WEBM_DX_CODEC_VIDEO_DAV1D
#include "Av1Decoder.h"

#include <bit>
#include <cstring>
#include "webmdx/errors.h"

namespace wdx {
    void freeCallback(const std::uint8_t * data,void* cookie) {
        delete[] data;
    }

    Av1Decoder::Av1Decoder(const VideoTrack &track) {
        _track = track;
        dav1d_default_settings(&_initSettings);
        if (const auto result = dav1d_open(&_context,&_initSettings); result < 0) {
            throw AudioDecoderError(std::string("Failed to create Av1 decoder: "));
        }
    }

    void Av1Decoder::Decode(const std::shared_ptr<Packet>& input) {
        auto inputData = input->GetData();
        dav1d_data_unref(&_data);
        const auto data = new std::uint8_t[inputData.size()];
        std::memcpy(data, inputData.data(), inputData.size());
        if (dav1d_data_wrap(&_data,data,inputData.size(),freeCallback, nullptr) < 0) {
            delete[] data;
            throw AudioDecoderError("Failed to wrap AV1 data");
        }
        // dav1d hands this back on the picture the packet decodes to, however long it holds it
        _data.m.timestamp = std::bit_cast<std::int64_t>(input->GetTime());

        auto hasCheckedBuffer = false;
        auto result = 0;
        do {
            result = dav1d_send_data(_context, &_data);

            if (result < 0 && hasCheckedBuffer) {
                throw AudioDecoderError("Failed to send data to dav1d");
            }

            if (result < 0) {
                hasCheckedBuffer = true;
                DrainPictures();
            }
        }while (result < 0);

    }

    std::shared_ptr<IDecodedVideoFrame> Av1Decoder::GetFrame() {
        DrainPictures();

        if (_pendingPictures.empty()) {
            return nullptr;
        }

        Dav1dPicture picture = _pendingPictures.front();
        _pendingPictures.pop_front();

        auto result = std::make_shared<I420Frame>();
        result->width = picture.p.w;
        result->height = picture.p.h;
        result->time = std::bit_cast<double>(picture.m.timestamp);
        const auto halfHeight = (result->height + 1) / 2;
        const auto plane = [&](const int index, const int height) {
            return DecodedPlane{static_cast<const std::uint8_t*>(picture.data[index]), static_cast<int>(picture.stride[index > 0]), height, picture.p.bpc > 8 ? 2 : 1, picture.p.bpc};
        };
        I420Frame::CopyPlane(result->yPlane, result->yStride, plane(0, result->height));
        I420Frame::CopyPlane(result->uPlane, result->uStride, plane(1, halfHeight));
        I420Frame::CopyPlane(result->vPlane, result->vStride, plane(2, halfHeight));

        dav1d_picture_unref(&picture);
        return result;
    }

    Av1Decoder::~Av1Decoder() {
        DrainPictures();
        for (auto& picture : _pendingPictures) {
            dav1d_picture_unref(&picture);
        }
        _pendingPictures.clear();
        dav1d_data_unref(&_data);
        dav1d_close(&_context);
    }

    void Av1Decoder::DrainPictures() {
        Dav1dPicture picture{};
        while (dav1d_get_picture(_context, &picture) == 0) {
            _pendingPictures.push_back(picture);
            picture = Dav1dPicture{};
        }
    }

    void Av1Decoder::Flush()
    {
        // dav1d only gives up its delayed pictures on a dav1d_get_picture that follows one with no new data
        DrainPictures();
        DrainPictures();
    }

    void Av1Decoder::Reset()
    {
        DrainPictures();
        for (auto& picture : _pendingPictures) {
            dav1d_picture_unref(&picture);
        }
        _pendingPictures.clear();
        dav1d_data_unref(&_data);
        dav1d_close(&_context);

        dav1d_default_settings(&_initSettings);
        if (const auto result = dav1d_open(&_context,&_initSettings); result < 0) {
            throw AudioDecoderError(std::string("Failed to create Av1 decoder: "));
        }
    }
}
#endif