#pragma once
#include <webm/mkvparser/mkvparser.h>

namespace wdx {
    struct TrackPosition {
        const mkvparser::Cluster *cluster{};
        const mkvparser::BlockEntry *entry{};
        double time{};
        void UseBlockTime();
        void SetCluster(const mkvparser::Cluster *newCluster);
    };
}
