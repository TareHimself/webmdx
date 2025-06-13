#pragma once
#include <webm/mkvparser/mkvparser.h>

namespace wd {
    struct TrackPosition {
        const mkvparser::Cluster *cluster{};
        const mkvparser::BlockEntry *entry{};
        bool isFirstDecode{true};
        double seconds{};

        void UpdateSeconds();
    };
}
