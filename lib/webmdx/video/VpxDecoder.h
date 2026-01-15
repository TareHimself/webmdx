#pragma once
#ifdef WEBM_DX_CODEC_VIDEO_VPX
#include "webmdx/IVideoDecoder.h"
#include <vpx/vp8dx.h>
#include <vpx/vpx_decoder.h>
#include <span>
namespace wdx {

    struct VpxFrame : IDecodedVideoFrame {
        std::uint32_t width{0};
        std::uint32_t height{0};
        int yStride{0};
        int vStride{0};
        int uStride{0};
        int alphaStride{0};
        std::vector<std::uint8_t> yPlane{};
        std::vector<std::uint8_t> uPlane{};
        std::vector<std::uint8_t> vPlane{};
        std::vector<std::uint8_t> alphaPlane{};
        void ToRgba(const std::span<std::uint8_t> &frame) override;
    };

    class VpxDecoder final : public IVideoDecoder {
    public:
        explicit VpxDecoder(const VideoTrack& track);
        std::shared_ptr<IDecodedVideoFrame> GetFrame() override;

        ~VpxDecoder() override;
        void Decode(const std::shared_ptr<Packet>& input) override;
        void Reset() override;
    private:
        void Init();
        VideoTrack _track{};
        vpx_codec_ctx_t _codec{};
    };
}
#endif
