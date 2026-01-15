#pragma once
#include "VideoCodec.h"

namespace wdx {
    enum class DemuxResult {
        IncompleteHeader,
        IncompleteCluster
        Success,
        Finished
    };
}
