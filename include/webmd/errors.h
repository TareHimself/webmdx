#pragma once
#include <stdexcept>

#include "AudioCodec.h"
#include "VideoCodec.h"

namespace wd {

    class NoTracksAvailableException : public std::exception {
    };

    class ExpectedDataException : public std::exception {

    };

    class UnknownCodecError : public std::exception{

    };

    class UnknownAudioCodecError : public UnknownCodecError{

    };

    class UnknownVideoCodecError : public UnknownCodecError{

    };

    class UnSupportedCodecError : public std::exception{

    };

    class UnSupportedAudioCodecError : public UnSupportedCodecError{
    public:
        explicit  UnSupportedAudioCodecError(const AudioCodec& _codec) : codec(_codec) {};
        AudioCodec codec;
    };

    class UnSupportedVideoCodecError : public UnSupportedCodecError{
    public:
        explicit  UnSupportedVideoCodecError(const VideoCodec& _codec) : codec(_codec) {};
        VideoCodec codec;
    };

    class DecoderError : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    class AudioDecoderError : public DecoderError {
        using DecoderError::DecoderError;

    };

    class VideoDecoderError : public DecoderError {
        using DecoderError::DecoderError;
    };
}
