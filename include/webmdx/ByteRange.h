#pragma once
#include <cstdint>

namespace wdx {
    // Bytes [start, end) of a source.
    struct ByteRange {
        std::int64_t start{};
        std::int64_t end{};
    };
}
