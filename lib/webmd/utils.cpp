#include "webmd/utils.h"

namespace wd {
    double nanoSecsToSecs(const long long nanoseconds) {
        return static_cast<double>(nanoseconds) / 1e9;
    }
}
