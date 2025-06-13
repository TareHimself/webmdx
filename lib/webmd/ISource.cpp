#include "webmd/ISource.h"

namespace wd {
    bool ISource::IsEmpty() const {
        return !IsWriting() && GetAvailable() == 0;
    }
}
