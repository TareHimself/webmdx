#pragma once
#ifdef WEBM_DX_CODEC_VIDEO_DAV1D
#include "webmdx/IVideoDecoder.h"
#include <dav1d/dav1d.h>
namespace wdx {
    struct Av1Frame final : public IDecodedVideoFrame {
        std::vector<uint8_t> yPlane{};
        std::vector<uint8_t> uPlane{};
        std::vector<uint8_t> vPlane{};
        std::vector<uint8_t> alphaPlane{};
        ptrdiff_t yStride{};
        ptrdiff_t alphaStride{};
        ptrdiff_t uvStride{};
        int height;
        int width;
        Dav1dPixelLayout layout{};
        void ToRgba(const std::span<std::uint8_t> &frame) override;

    };

    class Av1Decoder final : public IVideoDecoder {
    public:
        explicit Av1Decoder(const VideoTrack& track);
        void Decode(const std::span<std::uint8_t> &input, double timestamp) override;

        std::shared_ptr<IDecodedVideoFrame> GetFrame() override;

        ~Av1Decoder() override;

        void AdvanceToLatestPicture();
    private:
        VideoTrack _track{};
        Dav1dContext* _context{nullptr};
        Dav1dSettings _initSettings{};
        Dav1dSettings _playSettings{};
        Dav1dPicture _latestPicture = {};
        Dav1dData _data = {};
    };
}
#endif