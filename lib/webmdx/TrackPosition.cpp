#include "TrackPosition.h"

#include "webmdx/utils.h"

namespace wdx {
    void TrackPosition::UseBlockTime() {
        time = nanoSecsToSecs(entry->GetBlock()->GetTime(cluster));
    }
}
