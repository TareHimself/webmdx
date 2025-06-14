#include "webmdx/utils.h"

namespace wdx {
    double nanoSecsToSecs(const long long nanoseconds) {
        return static_cast<double>(nanoseconds) / 1e9;
    }
}
