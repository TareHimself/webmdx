#pragma once
#include <utility>
#include <webm/mkvparser/mkvparser.h>

#include "TrackPosition.h"
#include "webmdx/utils.h"

namespace wdx {
    struct BlockEntries {
        struct Iterator {
            using PairType = std::pair<const mkvparser::Block *,double>;


            const mkvparser::BlockEntry* operator*() const { return _current; }
            const mkvparser::BlockEntry* operator->() const { return _current; }

            // Prefix increment
            Iterator &operator++();

            // Postfix increment
            Iterator operator++(int) {
                Iterator tmp = *this;
                ++(*this);
                return tmp;
            }

            friend bool operator==(const Iterator &a, const Iterator &b) {
                auto aTime = nanoSecsToSecs(a._current->GetBlock()->GetTime(a._current->GetCluster()));
                auto bTime = nanoSecsToSecs(b._current->GetBlock()->GetTime(b._current->GetCluster()));
                return a._current == b._current;
            };
            friend bool operator!=(const Iterator &a, const Iterator &b) {
                auto aTime = nanoSecsToSecs(a._current->GetBlock()->GetTime(a._current->GetCluster()));
                auto bTime = nanoSecsToSecs(b._current->GetBlock()->GetTime(b._current->GetCluster()));
                return a._current != b._current;
            };
            bool completed{false};

            Iterator(const mkvparser::BlockEntry *entry, const BlockEntries *source);

        private:
            const BlockEntries *_source;
            const mkvparser::BlockEntry * _current{};
        };

        BlockEntries(const TrackPosition &begin, const TrackPosition &end);

        [[nodiscard]] Iterator begin() const { return Iterator{_begin.entry, this}; }
        [[nodiscard]] Iterator end() const { return Iterator{_end.entry, this}; }

    private:
        friend Iterator;
        TrackPosition _begin{};
        TrackPosition _end{};
        long long _trackNumber;
    };
}
