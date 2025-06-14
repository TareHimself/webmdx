#pragma once
#ifdef WEBMD_CODEC_VIDEO_VPX
#include "webmd/IVideoDecoder.h"
#include <vpx/vp8dx.h>
#include <vpx/vpx_decoder.h>
namespace wd {

    struct VpxFrame : IDecodedVideoFrame {
        std::uint32_t width{0};
        std::uint32_t height{0};
        int yStride{0};
        int vStride{0};
        int uStride{0};
        std::vector<uint8_t> yPlane{};
        std::vector<uint8_t> uPlane{};
        std::vector<uint8_t> vPlane{};
        void ToRgba(std::vector<std::uint8_t> &frame) override;
    };

    class VpxDecoder final : public IVideoDecoder {
    public:
        explicit VpxDecoder(const VideoTrack& track);
        void Decode(const std::span<std::uint8_t> &input, double timestamp) override;
        std::shared_ptr<IDecodedVideoFrame> GetFrame() override;

        ~VpxDecoder() override;
    private:
        VideoTrack _track{};
        vpx_codec_ctx_t _codec{};
    };
}
#endif
