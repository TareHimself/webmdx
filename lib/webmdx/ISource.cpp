#include "webmdx/ISource.h"

namespace wdx {
    bool ISource::IsEmpty() const {
        return !IsWriting() && GetAvailable() == 0;
    }
}
