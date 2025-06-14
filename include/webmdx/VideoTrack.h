#pragma once
#include "VideoCodec.h"

namespace wdx {
    struct VideoTrack {
        int width;
        int height;
        VideoCodec codec;
    };
}
