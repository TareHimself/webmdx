#pragma once
#include "AudioCodec.h"
#include <cstdint>
namespace wd {
    struct AudioTrack {
        int channels{0};
        int sampleRate{0};
        int bitDepth{0};
        double codecDelay{0};
        double seekPreRoll{0};
        AudioCodec codec{0};
        const std::uint8_t* codecPrivate{nullptr};
        std::uint64_t codecPrivateSize{0};
    };
}
