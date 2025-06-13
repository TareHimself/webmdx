#include "TrackPosition.h"

#include "webmd/utils.h"

namespace wd {
    void TrackPosition::UpdateSeconds() {
        seconds = nanoSecsToSecs(entry->GetBlock()->GetTime(cluster));
    }
}
