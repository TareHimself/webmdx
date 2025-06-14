#include "TrackPosition.h"

#include "webmdx/utils.h"

namespace wdx {
    void TrackPosition::UseBlockTime() {
        time = nanoSecsToSecs(entry->GetBlock()->GetTime(cluster));
    }

    void TrackPosition::SetCluster(const mkvparser::Cluster *newCluster) {
        cluster = newCluster;
        cluster->GetFirst(entry);
        UseBlockTime();
    }
}
