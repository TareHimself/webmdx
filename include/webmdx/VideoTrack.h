#pragma once
#include "VideoCodec.h"

namespace wdx {
    struct WEBMDX_API VideoTrack {
        int width;
        int height;
        VideoCodec codec;
    };
}
