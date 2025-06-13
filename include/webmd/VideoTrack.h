#pragma once
#include "VideoCodec.h"

namespace wd {
    struct VideoTrack {
        int width;
        int height;
        VideoCodec codec;
    };
}
