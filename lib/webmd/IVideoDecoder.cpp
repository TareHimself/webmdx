#include "webmd/IVideoDecoder.h"
#include "video/VpxDecoder.h"
#include "webmd/errors.h"

namespace wd {
    std::shared_ptr<IVideoDecoder> IVideoDecoder::Create(const VideoTrack &track) {
        switch (track.codec) {
#ifdef WEBMD_CODEC_VIDEO_VPX
            case VideoCodec::Vpx9:
            case VideoCodec::Vpx8:
                return std::make_shared<VpxDecoder>(track);
#endif
#ifdef WEBMD_CODEC_VIDEO_AV1

#endif
            case VideoCodec::Unknown:
                throw UnknownVideoCodecError();
            default:
                break;
        }
        throw UnSupportedVideoCodecError(track.codec);
    }
}
