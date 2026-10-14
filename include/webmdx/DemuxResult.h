#pragma once
#include "VideoCodec.h"

namespace wdx {
    enum class DemuxResult {
        // Headers not all here yet. Give the source more data and call again.
        IncompleteHeader,
        // Data for the requested time not here yet. Give the source more data and call again.
        IncompleteCluster,
        Success,
        // No more packets, including when the source ended early.
        Finished
    };
}
