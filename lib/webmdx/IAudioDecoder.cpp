#include "webmdx/IAudioDecoder.h"

#include "audio/OpusDecoder.h"
#include "webmdx/errors.h"

namespace wdx {
    std::shared_ptr<IAudioDecoder> IAudioDecoder::Create(const AudioTrack &track) {
        switch (track.codec) {
#ifdef WEBM_DX_CODEC_AUDIO_OPUS
            case AudioCodec::Opus:
                return std::make_shared<OpusDecoder>(track);
#endif
#ifdef WEBM_DX_CODEC_AUDIO_VORBIS
            case AudioCodec::Vorbis:
                return std::make_shared<VorbisDecoder>(track);
#endif
            case AudioCodec::Unknown:
                throw UnknownAudioCodecException();
            default:
                break;
        }
        throw UnSupportedAudioCodecException(track.codec);
    }
}
