#pragma once
#include <stdexcept>

#include "AudioCodec.h"
#include "VideoCodec.h"

namespace wdx {


    class SourceDecodeException : public std::exception{

    };

    class HeaderDecodeFailedException : public std::exception{

    };

    class NoTracksAvailableException : public std::exception {
    };

    class ExpectedDataException : public std::exception {

    };

    class UnknownCodecException : public std::exception{

    };

    class UnknownAudioCodecException : public UnknownCodecException{

    };

    class UnknownVideoCodecException : public UnknownCodecException{

    };

    class UnSupportedCodecException : public std::exception{

    };

    class UnSupportedAudioCodecException : public UnSupportedCodecException{
    public:
        explicit  UnSupportedAudioCodecException(const AudioCodec& _codec) : codec(_codec) {};
        AudioCodec codec;
    };

    class UnSupportedVideoCodecException : public UnSupportedCodecException{
    public:
        explicit  UnSupportedVideoCodecException(const VideoCodec& _codec) : codec(_codec) {};
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
