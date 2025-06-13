#include "BlockIterable.h"

#include <stdexcept>

namespace wd {
    BlockIterable::Iterator &BlockIterable::Iterator::operator++() {
        const mkvparser::BlockEntry *current = _current;
        do {
            auto cluster = current->GetCluster();
            const auto segment = cluster->m_pSegment;
            const mkvparser::BlockEntry *next;
            cluster->GetNext(current, next);

            if (next == nullptr || next->EOS()) {
                cluster = segment->GetNext(cluster);
                if (cluster == nullptr || cluster->EOS()) {
                    throw std::runtime_error("No more blocks to iterate");
                }
                cluster->GetFirst(next);
            }

            current = next;
        } while (current->EOS() || current->GetBlock()->GetTrackNumber() != _source->_trackNumber);

        _current = current;

        return *this;
    }

    BlockIterable::Iterator::Iterator(const mkvparser::BlockEntry *entry, const BlockIterable *source) {
        _current = entry;
        _source = source;
    }

    BlockIterable::BlockIterable(const TrackPosition &begin, const TrackPosition &end) {
        _begin = begin;
        _end = end;
        _trackNumber = _begin.entry->GetBlock()->GetTrackNumber();
    }
}
