#pragma once
#include <webm/mkvparser/mkvparser.h>

namespace wdx {
    struct TrackPosition {
        const mkvparser::Cluster *cluster{};
        const mkvparser::BlockEntry *entry{};
        bool isFirstDecode{true};
        double time{};
        void UseBlockTime();
    };
}
