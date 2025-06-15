#include "webmdx/IVideoDecoder.h"

#include "video/Av1Decoder.h"
#include "video/VpxDecoder.h"
#include "webmdx/errors.h"

namespace wdx {
    std::shared_ptr<IVideoDecoder> IVideoDecoder::Create(const VideoTrack &track) {
        switch (track.codec) {
#ifdef WEBM_DX_CODEC_VIDEO_VPX
            case VideoCodec::Vpx9:
            case VideoCodec::Vpx8:
                return std::make_shared<VpxDecoder>(track);
#endif
#ifdef WEBM_DX_CODEC_VIDEO_DAV1D
            case VideoCodec::Av1:
                return std::make_shared<Av1Decoder>(track);
#endif
            case VideoCodec::Unknown:
                throw UnknownVideoCodecError();
            default:
                break;
        }
        throw UnSupportedVideoCodecError(track.codec);
    }
}
