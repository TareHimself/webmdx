#pragma once

namespace wd {
    struct AudioTrack {
        int channels;
        int sampleRate;
        int bitDepth;
        double codecDelay;
        double seekPreRoll;
    };
}
