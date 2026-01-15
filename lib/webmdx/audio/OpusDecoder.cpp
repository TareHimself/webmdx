#ifdef WEBM_DX_CODEC_AUDIO_OPUS
#include "OpusDecoder.h"
#include "webmdx/errors.h"

namespace wdx {
    OpusDecoder::OpusDecoder(const AudioTrack &track) {
        _track = track;
        int error;
        _decoder = opus_decoder_create(_track.sampleRate, _track.channels, &error);
        if (error != OPUS_OK) {
            throw AudioDecoderError(std::string("Failed to create OpusDecoder: ") + opus_strerror(error));
        }
    }

    int OpusDecoder::Decode(const std::shared_ptr<Packet>& input, std::vector<float> &pcm) {
        auto data = input->GetData();
        const auto frameSize = opus_packet_get_samples_per_frame(data.data(),_track.sampleRate);
        if (pcm.size() < frameSize) {
            pcm.resize(frameSize * _track.channels);
        }

        const auto samplesDecoded = opus_decode_float(_decoder,data.data(), static_cast<opus_int32>(data.size()),pcm.data(), frameSize,0);
        if (samplesDecoded != frameSize) {
            throw AudioDecoderError(std::string("Failed to decode opus packet: ")  + opus_strerror(samplesDecoded));
        }
        return samplesDecoded * _track.channels;
    }

    OpusDecoder::~OpusDecoder() {
        opus_decoder_destroy(_decoder);
    }

    void OpusDecoder::Reset()
    {
        opus_decoder_destroy(_decoder);
        int error;
        _decoder = opus_decoder_create(_track.sampleRate, _track.channels, &error);
        if (error != OPUS_OK) {
            throw AudioDecoderError(std::string("Failed to create OpusDecoder: ") + opus_strerror(error));
        }
    }
}
#endif