#include "webmd/IAudioDecoder.h"

#include "audio/OpusDecoder.h"
#include "webmd/errors.h"

namespace wd {
    std::shared_ptr<IAudioDecoder> IAudioDecoder::Create(const AudioTrack &track) {
        switch (track.codec) {
#ifdef WEBMD_CODEC_AUDIO_OPUS
            case AudioCodec::Opus:
                return std::make_shared<OpusDecoder>(track);
#endif
#ifdef WEBMD_CODEC_AUDIO_VORBIS
            case AudioCodec::Vorbis:
                return std::make_shared<VorbisDecoder>(track);
#endif
            case AudioCodec::Unknown:
                throw UnknownAudioCodecError();
            default:
                break;
        }
        throw UnSupportedAudioCodecError(track.codec);
    }
}
