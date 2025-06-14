#include "TrackPosition.h"

#include "webmd/utils.h"

namespace wd {
    void TrackPosition::UseBlockTime() {
        time = nanoSecsToSecs(entry->GetBlock()->GetTime(cluster));
    }
}
