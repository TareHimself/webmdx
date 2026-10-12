#pragma once
#ifdef WEBM_DX_CODEC_VIDEO_VPX
#include "webmdx/IVideoDecoder.h"
#include "I420Frame.h"
#include <vpx/vp8dx.h>
#include <vpx/vpx_decoder.h>
#include <span>
namespace wdx {

    class VpxDecoder final : public IVideoDecoder {
    public:
        explicit VpxDecoder(const VideoTrack& track);
        std::shared_ptr<IDecodedVideoFrame> GetFrame() override;

        ~VpxDecoder() override;
        void Decode(const std::shared_ptr<Packet>& input) override;
        void Reset() override;
        void Flush() override;
    private:
        void Init();
        VideoTrack _track{};
        vpx_codec_ctx_t _codec{};
        vpx_codec_iter_t _iter{nullptr};
        double _packetTime{0};
    };
}
#endif
