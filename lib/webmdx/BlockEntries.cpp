#include "BlockEntries.h"
#include <stdexcept>

#include "SourceReader.h"

namespace wdx {
    const mkvparser::Cluster* NextKnownCluster(const mkvparser::Cluster* current) {
        const auto segment = current->m_pSegment;
        static_cast<SourceReader*>(segment->m_pReader)->focus = current->m_element_start;
        const mkvparser::Cluster* next = nullptr;
        long long neededAt = 0;
        long neededLength = 0;
        // Segment::GetNext cannot be used here, it expects the whole file for clusters a seek jumped to.
        return segment->ParseNext(current, next, neededAt, neededLength) == 0 ? next : nullptr;
    }

    BlockEntries::Iterator &BlockEntries::Iterator::operator++() {
        const mkvparser::BlockEntry *current = _current;
        do {
            auto cluster = current->GetCluster();
            const mkvparser::BlockEntry *next;
            cluster->GetNext(current, next);
            auto time = nanoSecsToSecs(current->GetBlock()->GetTime(cluster));
            if (next == nullptr || next->EOS()) {
                cluster = NextKnownCluster(cluster);
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

    BlockEntries::Iterator::Iterator(const mkvparser::BlockEntry *entry, const BlockEntries *source) {
        _current = entry;
        _source = source;
    }

    BlockEntries::BlockEntries(const TrackPosition &begin, const TrackPosition &end) {
        _begin = begin;
        _end = end;
        _trackNumber = _begin.entry->GetBlock()->GetTrackNumber();
    }
}
