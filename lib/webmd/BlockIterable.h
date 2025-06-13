#pragma once
#include <webm/mkvparser/mkvparser.h>

#include "TrackPosition.h"

namespace wd {
    struct BlockIterable {
        struct Iterator {
            const mkvparser::Block *operator*() const { return _current->GetBlock(); }
            const mkvparser::Block *operator->() const { return _current->GetBlock(); }

            // Prefix increment
            Iterator &operator++();

            // Postfix increment
            Iterator operator++(int) {
                Iterator tmp = *this;
                ++(*this);
                return tmp;
            }

            friend bool operator==(const Iterator &a, const Iterator &b) { return a._current == b._current; };
            friend bool operator!=(const Iterator &a, const Iterator &b) { return a._current != b._current; };
            bool completed{false};

            Iterator(const mkvparser::BlockEntry *entry, const BlockIterable *source);

        private:
            const BlockIterable *_source;
            const mkvparser::BlockEntry *_current{nullptr};
        };

        BlockIterable(const TrackPosition &begin, const TrackPosition &end);

        [[nodiscard]] Iterator begin() const { return Iterator{_begin.entry, this}; }
        [[nodiscard]] Iterator end() const { return Iterator{_end.entry, this}; }

    private:
        friend Iterator;
        TrackPosition _begin{};
        TrackPosition _end{};
        long long _trackNumber;
    };
}
