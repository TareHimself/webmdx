#include <algorithm>
#include <bit>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <typeinfo>
#include <utility>
#include <vector>

#include "webmdx/FileSource.h"
#include "webmdx/errors.h"
#include "webmdx/SourceDecoder.h"
#include "webmdx/StreamSource.h"

namespace
{
    struct Expected
    {
        int width;
        int height;
        std::size_t videoPackets;
    };

    int failures = 0;

    void check(const bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << std::endl;
            failures++;
        }
    }

    std::vector<std::uint8_t> readFile(const char* path)
    {
        std::ifstream file(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }

    // Holds a whole file but only lets go of what it has been asked for, the way a source reading from a network would.
    class PullSource final : public wdx::ISource
    {
    public:
        explicit PullSource(std::vector<std::uint8_t> data) : _data(std::move(data)) {}

        void Read(const std::int64_t& pos, std::span<std::uint8_t> data) override
        {
            std::memcpy(data.data(), _data.data() + pos, data.size());
        }

        [[nodiscard]] std::int64_t GetLength() const override { return static_cast<std::int64_t>(_data.size()); }

        [[nodiscard]] std::int64_t GetAvailable() const override { return _available; }

        void MakeAvailable(const std::uint64_t& size) override
        {
            _available = std::min(std::max(_available, static_cast<std::int64_t>(size)), GetLength());
        }

    private:
        std::vector<std::uint8_t> _data;
        std::int64_t _available{};
    };

    // Holds a whole file but lets go of only the pieces it has been asked for, wherever in the file they are.
    class RangeSource final : public wdx::ISource
    {
    public:
        explicit RangeSource(std::vector<std::uint8_t> data) : _data(std::move(data)) {}

        void Read(const std::int64_t& pos, std::span<std::uint8_t> data) override
        {
            std::memcpy(data.data(), _data.data() + pos, data.size());
        }

        [[nodiscard]] std::int64_t GetLength() const override { return static_cast<std::int64_t>(_data.size()); }

        [[nodiscard]] std::int64_t GetAvailable() const override { return GetAvailableFrom(0); }

        [[nodiscard]] std::int64_t GetAvailableFrom(const std::int64_t& pos) const override
        {
            const auto after = _held.upper_bound(pos);
            if (after == _held.begin())
            {
                return pos;
            }
            return std::max(pos, std::prev(after)->second);
        }

        void MakeAvailable(const std::uint64_t& size) override { MakeRangeAvailable(0, size); }

        void MakeRangeAvailable(const std::int64_t& pos, const std::uint64_t& size) override
        {
            auto start = pos;
            auto end = std::min(pos + static_cast<std::int64_t>(size), GetLength());
            // Pieces that touch are kept as one.
            for (auto piece = _held.begin(); piece != _held.end();)
            {
                if (piece->second >= start && piece->first <= end)
                {
                    start = std::min(start, piece->first);
                    end = std::max(end, piece->second);
                    piece = _held.erase(piece);
                }
                else
                {
                    ++piece;
                }
            }
            if (end > start)
            {
                _held[start] = end;
            }
        }

        [[nodiscard]] std::size_t CountHeld() const
        {
            std::int64_t held = 0;
            for (const auto& [start, end] : _held)
            {
                held += end - start;
            }
            return static_cast<std::size_t>(held);
        }

    private:
        std::vector<std::uint8_t> _data;
        // Start to end of each piece held.
        std::map<std::int64_t, std::int64_t> _held;
    };

    struct StreamedCounts
    {
        std::size_t videoPackets;
        std::size_t audioPackets;
        wdx::DemuxResult result;
        bool hintsPointAhead{true};
        std::size_t mostBuffered{};
    };

    // Hands the decoder a file one chunk at a time, giving it more only when it says it has run out.
    // A chunk size of 0 gives it exactly as much as it says it needs.
    StreamedCounts demuxInChunks(const std::span<const std::uint8_t> data, const std::size_t chunkSize, const bool dropReleased = false)
    {
        const auto source = std::make_shared<wdx::StreamSource>(dropReleased);
        const auto decoder = std::make_shared<wdx::SourceDecoder>();
        decoder->SetSource(source);

        StreamedCounts counts{};
        decoder->SetVideoPacketCallback([&](const std::shared_ptr<wdx::Packet>&, wdx::IVideoDecoder*)
        {
            counts.videoPackets++;
        });
        decoder->SetAudioPacketCallback([&](const std::shared_ptr<wdx::Packet>&, wdx::IAudioDecoder*)
        {
            counts.audioPackets++;
        });

        std::size_t offset = 0;
        for (;;)
        {
            counts.result = decoder->Demux(0.1);
            if (counts.result == wdx::DemuxResult::Success)
            {
                continue;
            }
            if (counts.result == wdx::DemuxResult::Finished || offset == data.size())
            {
                break;
            }
            const auto needed = static_cast<std::size_t>(decoder->GetBytesNeeded());
            counts.hintsPointAhead = counts.hintsPointAhead && needed > offset;
            const auto wanted = chunkSize > 0 ? chunkSize : std::max(needed, offset + 1) - offset;
            const auto size = std::min(wanted, data.size() - offset);
            source->Append(data.subspan(offset, size));
            offset += size;
            counts.mostBuffered = std::max(counts.mostBuffered, source->GetBufferedSize());
            if (offset == data.size())
            {
                source->SetEndOfStream();
            }
        }
        return counts;
    }

    void testStreaming(const char* path, const Expected& expected, const std::size_t audioPackets)
    {
        const auto data = readFile(path);
        const auto chunkSize = std::max<std::size_t>(1000, data.size() / 4000);

        const auto whole = demuxInChunks(data, chunkSize);
        check(whole.result == wdx::DemuxResult::Finished, "demux reaches the end of a file that arrives in chunks");

        const auto dropping = demuxInChunks(data, chunkSize, true);
        check(dropping.result == wdx::DemuxResult::Finished && dropping.videoPackets == whole.videoPackets && dropping.audioPackets == whole.audioPackets,
            "a stream that drops what the decoder is done with gives the same packets");
        std::cout << path << " dropping released data: at most " << dropping.mostBuffered << " of " << data.size() << " bytes held" << std::endl;
        check(whole.videoPackets == expected.videoPackets, "a file that arrives in chunks gives " + std::to_string(expected.videoPackets) + " video packets, got " + std::to_string(whole.videoPackets));
        check(whole.audioPackets == audioPackets, "a file that arrives in chunks gives " + std::to_string(audioPackets) + " audio packets, got " + std::to_string(whole.audioPackets));

        const auto hinted = demuxInChunks(data, 0);
        check(hinted.result == wdx::DemuxResult::Finished, "demux reaches the end of a file fed by what the decoder asks for");
        check(hinted.hintsPointAhead, "the decoder always asks for more than it already has");
        check(hinted.videoPackets == expected.videoPackets && hinted.audioPackets == audioPackets, "a file fed by what the decoder asks for gives every packet, got "
            + std::to_string(hinted.videoPackets) + " video and " + std::to_string(hinted.audioPackets) + " audio");

        const auto cut = demuxInChunks(std::span(data).first(data.size() * 6 / 10), chunkSize);
        check(cut.result == wdx::DemuxResult::Finished, "demux reaches the end of a file that is cut short");
        check(cut.videoPackets > 0 && cut.videoPackets < expected.videoPackets, "a file that is cut short gives some of its video packets, got " + std::to_string(cut.videoPackets));
        check(cut.audioPackets <= audioPackets, "a file that is cut short gives no more audio packets than the whole file, got " + std::to_string(cut.audioPackets));

        bool headerRejected = false;
        try
        {
            demuxInChunks(std::span(data).first(16), chunkSize);
        }
        catch (const wdx::HeaderDecodeFailedException&)
        {
            headerRejected = true;
        }
        check(headerRejected, "a file that ends inside its header is rejected");

        std::cout << path << " in chunks of " << chunkSize << ": " << whole.videoPackets << " video packets, " << whole.audioPackets
            << " audio packets, cut short: " << cut.videoPackets << " video packets, " << cut.audioPackets << " audio packets" << std::endl;
    }

    void testPulling(const char* path, const Expected& expected, const std::size_t audioPackets)
    {
        const auto source = std::make_shared<PullSource>(readFile(path));
        const auto decoder = std::make_shared<wdx::SourceDecoder>();
        decoder->SetSource(source);

        std::size_t videoPackets = 0;
        std::size_t pulledAudioPackets = 0;
        decoder->SetVideoPacketCallback([&](const std::shared_ptr<wdx::Packet>&, wdx::IVideoDecoder*)
        {
            videoPackets++;
        });
        decoder->SetAudioPacketCallback([&](const std::shared_ptr<wdx::Packet>&, wdx::IAudioDecoder*)
        {
            pulledAudioPackets++;
        });

        auto result = decoder->Demux(0.1);
        const auto pulledAtFirst = source->GetAvailable();
        check(result == wdx::DemuxResult::Success, "a source that can be asked for data never leaves the decoder waiting");
        check(pulledAtFirst < source->GetLength(), "the first tenth of a second does not pull the whole file, pulled " + std::to_string(pulledAtFirst));
        while (result == wdx::DemuxResult::Success)
        {
            result = decoder->Demux(0.1);
        }
        check(result == wdx::DemuxResult::Finished, "demux reaches the end of a file it pulls in itself");
        check(videoPackets == expected.videoPackets && pulledAudioPackets == audioPackets, "a file the decoder pulls in itself gives every packet, got "
            + std::to_string(videoPackets) + " video and " + std::to_string(pulledAudioPackets) + " audio");

        std::cout << path << " pulled: " << pulledAtFirst << " of " << source->GetLength() << " bytes for the first tenth of a second" << std::endl;
    }

    void testSeeking(const char* path)
    {
        const auto data = readFile(path);
        const auto source = std::make_shared<wdx::StreamSource>();
        const auto decoder = std::make_shared<wdx::SourceDecoder>();
        decoder->SetSource(source);

        std::size_t videoPackets = 0;
        bool firstIsKey = false;
        double firstTime = 0;
        decoder->SetVideoPacketCallback([&](const std::shared_ptr<wdx::Packet>& packet, wdx::IVideoDecoder*)
        {
            if (videoPackets == 0)
            {
                firstIsKey = packet->IsKey();
                firstTime = packet->GetTime();
            }
            videoPackets++;
        });
        const auto demuxToEnd = [&]
        {
            auto result = decoder->Demux(0.1);
            while (result == wdx::DemuxResult::Success)
            {
                result = decoder->Demux(0.1);
            }
            return result;
        };

        check(decoder->Seek(0) == wdx::DemuxResult::IncompleteHeader, "a seek before any data has arrived asks for the header");

        const auto arrived = data.size() * 3 / 10;
        source->Append(std::span(data).first(arrived));
        check(decoder->Seek(0) == wdx::DemuxResult::Success, "a seek to the start works with the start of the file");
        const auto late = decoder->GetDuration() * 0.9;
        check(decoder->Seek(late) == wdx::DemuxResult::IncompleteCluster, "a seek past the data that has arrived asks for more");
        check(decoder->GetPosition() == 0 && videoPackets == 0, "a seek that needs more data leaves the decoder where it was");

        source->Append(std::span(data).subspan(arrived));
        source->SetEndOfStream();
        check(decoder->Seek(late) == wdx::DemuxResult::Success, "the same seek works once the data has arrived");
        check(decoder->GetPosition() == late, "a seek moves the position to the time asked for");
        check(demuxToEnd() == wdx::DemuxResult::Finished, "demux reaches the end after a seek");
        check(videoPackets > 0 && firstIsKey && firstTime <= late, "video restarts from a key frame at or before the seek time");
        const auto afterLate = videoPackets;

        videoPackets = 0;
        const auto early = decoder->GetDuration() * 0.25;
        check(decoder->Seek(early) == wdx::DemuxResult::Success, "a seek back towards the start works");
        check(demuxToEnd() == wdx::DemuxResult::Finished, "demux reaches the end after a seek back");
        check(videoPackets >= afterLate, "an earlier seek gives no fewer video packets than a later one, got " + std::to_string(videoPackets) + " and " + std::to_string(afterLate));
    }

    struct VideoPacket
    {
        double time;
        bool isKey;

        bool operator==(const VideoPacket&) const = default;
    };

    wdx::DemuxResult demuxToEnd(const wdx::SourceDecoder& decoder)
    {
        auto result = decoder.Demux(0.1);
        while (result == wdx::DemuxResult::Success)
        {
            result = decoder.Demux(0.1);
        }
        return result;
    }

    // Hands everything on to the built-in decoder, counting what it is asked to do.
    struct CountingVideoDecoder final : wdx::IVideoDecoder
    {
        explicit CountingVideoDecoder(const wdx::VideoTrack& track) : inner(Create(track)) {}

        void Decode(const std::shared_ptr<wdx::Packet>& input) override { inner->Decode(input); }
        void Reset() override { resets++; inner->Reset(); }
        void Flush() override { flushes++; inner->Flush(); }
        std::shared_ptr<wdx::IDecodedVideoFrame> GetFrame() override { return inner->GetFrame(); }

        std::shared_ptr<IVideoDecoder> inner;
        int resets{};
        int flushes{};
    };

    void testDecoderFactories(const char* path)
    {
        const wdx::SourceDecoder decoder{};
        std::shared_ptr<CountingVideoDecoder> made{};
        decoder.SetVideoDecoderFactory([&](const wdx::VideoTrack& track)
        {
            made = std::make_shared<CountingVideoDecoder>(track);
            return made;
        });
        bool handedMade = true;
        decoder.SetVideoPacketCallback([&](const std::shared_ptr<wdx::Packet>&, wdx::IVideoDecoder* videoDecoder)
        {
            handedMade = handedMade && videoDecoder == made.get();
        });
        decoder.SetSource(std::make_shared<wdx::FileSource>(path));
        check(decoder.EnsureHeader() == wdx::DemuxResult::Success && made != nullptr && decoder.GetVideoDecoder() == made.get(), "the video decoder comes from the factory");
        check(decoder.Seek(0) == wdx::DemuxResult::Success && made->resets == 1, "a seek resets the decoder from the factory");
        check(demuxToEnd(decoder) == wdx::DemuxResult::Finished && made->flushes == 1, "the end flushes the decoder from the factory");
        check(handedMade, "packets come with the decoder from the factory");

        const wdx::SourceDecoder audioOnly{};
        audioOnly.SetVideoDecoderFactory([](const wdx::VideoTrack&) { return std::shared_ptr<wdx::IVideoDecoder>{}; });
        std::size_t videoPackets = 0;
        std::size_t audioPackets = 0;
        audioOnly.SetVideoPacketCallback([&](const std::shared_ptr<wdx::Packet>&, wdx::IVideoDecoder*) { videoPackets++; });
        audioOnly.SetAudioPacketCallback([&](const std::shared_ptr<wdx::Packet>&, wdx::IAudioDecoder*) { audioPackets++; });
        audioOnly.SetSource(std::make_shared<wdx::FileSource>(path));
        check(demuxToEnd(audioOnly) == wdx::DemuxResult::Finished && videoPackets == 0 && audioPackets > 0, "a factory that makes no video decoder leaves video out, got " + std::to_string(videoPackets) + " video packets");
    }

    void testSeekingIntoDropped(const char* path)
    {
        const auto data = readFile(path);
        const auto source = std::make_shared<wdx::StreamSource>(true);
        const wdx::SourceDecoder decoder{};
        decoder.SetSource(source);
        bool emptyPacket = false;
        decoder.SetVideoPacketCallback([&](const std::shared_ptr<wdx::Packet>& packet, wdx::IVideoDecoder*)
        {
            emptyPacket = emptyPacket || packet->GetData().empty();
        });
        source->Append(data);
        source->SetEndOfStream();
        check(demuxToEnd(decoder) == wdx::DemuxResult::Finished, "a dropping stream plays to the end");
        if (source->GetBufferedSize() < data.size())
        {
            const auto position = decoder.GetPosition();
            check(decoder.Seek(0) == wdx::DemuxResult::IncompleteCluster, "a seek back into dropped data asks for it");
            check(decoder.GetPosition() == position, "a seek back into dropped data leaves the decoder where it was");
        }
    }

    void testIndexSeeking(const char* path)
    {
        const auto data = readFile(path);
        std::vector<VideoPacket> packets{};
        const auto collect = [&](const std::shared_ptr<wdx::Packet>& packet, wdx::IVideoDecoder*)
        {
            packets.push_back({packet->GetTime(), packet->IsKey()});
        };

        const wdx::SourceDecoder whole{};
        whole.SetSource(std::make_shared<wdx::FileSource>(path));
        whole.SetVideoPacketCallback(collect);
        demuxToEnd(whole);
        const auto all = packets;
        const auto duration = whole.GetDuration();
        // The index points at clusters, so a file with few of them has nothing to skip.
        const std::string_view bytes{reinterpret_cast<const char*>(data.data()), data.size()};
        std::size_t clusters = 0;
        for (auto at = bytes.find("\x1F\x43\xB6\x75"); at != std::string_view::npos; at = bytes.find("\x1F\x43\xB6\x75", at + 1))
        {
            clusters++;
        }

        // What a seek should give: everything from the last key frame at or before the time.
        const auto expectedFrom = [&](const double time)
        {
            std::size_t start = 0;
            for (std::size_t i = 0; i < all.size() && all[i].time <= time; i++)
            {
                if (all[i].isKey)
                {
                    start = i;
                }
            }
            return std::vector(all.begin() + static_cast<std::ptrdiff_t>(start), all.end());
        };

        const auto source = std::make_shared<RangeSource>(data);
        const wdx::SourceDecoder decoder{};
        decoder.SetSource(source);
        decoder.SetVideoPacketCallback(collect);
        check(decoder.EnsureHeader() == wdx::DemuxResult::Success, "the headers of a source that holds pieces can be read");

        // Late first so that the seek back crosses from what was played into what was jumped to.
        for (const auto fraction : {0.9, 0.25, 0.6})
        {
            const auto time = duration * fraction;
            const auto name = "a seek to " + std::to_string(fraction) + " of a source that holds pieces";
            packets.clear();
            check(decoder.Seek(time) == wdx::DemuxResult::Success, name + " works");
            const auto expected = expectedFrom(time);
            const auto held = source->CountHeld();
            check(demuxToEnd(decoder) == wdx::DemuxResult::Finished, name + " plays to the end");
            check(packets == expected, name + " gives the " + std::to_string(expected.size()) + " video packets from its key frame on, got " + std::to_string(packets.size()));
            if (fraction == 0.9)
            {
                std::cout << path << " seek to 0.9: fetched " << held << " of " << data.size() << " bytes, key frame at "
                    << expected.front().time << "s of " << duration << "s, " << clusters << " clusters" << std::endl;
                if (clusters >= 4 && expected.front().time > duration * 0.75)
                {
                    check(held < data.size() / 2, "a seek near the end fetches less than half the file, got " + std::to_string(held) + " of " + std::to_string(data.size()));
                }
            }
        }

        const auto stream = std::make_shared<wdx::StreamSource>();
        const wdx::SourceDecoder streaming{};
        streaming.SetSource(stream);
        const auto arrived = data.size() * 3 / 10;
        stream->Append(std::span(data).first(arrived));
        if (streaming.Seek(duration * 0.9) != wdx::DemuxResult::Success)
        {
            const auto needed = streaming.GetNeededRange();
            check(needed.end > static_cast<std::int64_t>(arrived) && needed.start < needed.end && needed.end == streaming.GetBytesNeeded(),
                "a seek that needs more data says which bytes, got " + std::to_string(needed.start) + " to " + std::to_string(needed.end));
            stream->Append(std::span(data).subspan(arrived, std::min(static_cast<std::size_t>(needed.end), data.size()) - arrived));
            std::cout << path << " seek to 0.9 of a stream holding " << arrived << " bytes asks for " << needed.start << " to " << needed.end << std::endl;
        }
    }

    // True when the decoder gives up on the source as not being WebM.
    bool rejectsHeader(const std::function<wdx::DemuxResult()>& step, wdx::DemuxResult& result)
    {
        try
        {
            result = step();
        }
        catch (const wdx::HeaderDecodeFailedException&)
        {
            return true;
        }
        return false;
    }

    void testRejecting(const char* path)
    {
        const auto data = readFile(path);
        wdx::DemuxResult result{};

        // Bytes with no pattern to them, some of which are the 0x1A a WebM file starts with.
        std::vector<std::uint8_t> noise(1 << 20);
        std::uint32_t state = 12345;
        for (auto& value : noise)
        {
            state = state * 1664525u + 1013904223u;
            value = static_cast<std::uint8_t>(state >> 24);
        }
        // The same without any 0x1A, which is the most a decoder has to look through before it can tell.
        auto blank = noise;
        std::ranges::replace(blank, std::uint8_t{0x1A}, std::uint8_t{0});

        for (const auto& garbage : {noise, blank})
        {
            const auto pulled = std::make_shared<PullSource>(garbage);
            const wdx::SourceDecoder pulling{};
            pulling.SetSource(pulled);
            check(pulled->GetAvailable() == 0, "setting a source reads nothing from it");
            check(rejectsHeader([&] { return pulling.EnsureHeader(); }, result), "a source that is not WebM is rejected");
            check(!pulling.IsReady(), "a decoder that rejected its source is not ready");
            check(pulled->GetAvailable() <= 2048, "a source that is not WebM is rejected early, after " + std::to_string(pulled->GetAvailable()) + " bytes");

            const auto streamed = std::make_shared<wdx::StreamSource>();
            const wdx::SourceDecoder streaming{};
            const auto demux = [&] { return streaming.Demux(0.1); };
            streaming.SetSource(streamed);
            check(streaming.EnsureHeader() == wdx::DemuxResult::IncompleteHeader && !streaming.IsReady(), "a decoder given an empty stream waits for its headers");
            streamed->Append(std::span(garbage).first(16));
            const auto early = rejectsHeader(demux, result);
            streamed->Append(std::span(garbage).subspan(16, 4096));
            check(early || rejectsHeader(demux, result), "a stream that is not WebM is rejected before it ends");
        }

        // The EBML header of a real file, then a segment whose first element claims to be 100 MiB long.
        const std::size_t sizeLength = std::countl_zero(data[4]) + 1;
        std::size_t ebmlSize = data[4] & (0xFF >> sizeLength);
        for (std::size_t i = 1; i < sizeLength; i++)
        {
            ebmlSize = ebmlSize << 8 | data[4 + i];
        }
        std::vector<std::uint8_t> endless(data.begin(), data.begin() + static_cast<std::ptrdiff_t>(4 + sizeLength + ebmlSize));
        for (const auto value : {0x18, 0x53, 0x80, 0x67, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEC, 0x16, 0x40, 0x00, 0x00})
        {
            endless.push_back(static_cast<std::uint8_t>(value));
        }

        const auto unlimited = std::make_shared<wdx::StreamSource>();
        const wdx::SourceDecoder patient{};
        patient.SetMaxHeaderSize(0);
        patient.SetSource(unlimited);
        unlimited->Append(endless);
        check(!rejectsHeader([&] { return patient.Demux(0.1); }, result) && result == wdx::DemuxResult::IncompleteHeader, "headers of any length are waited for when there is no limit");
        check(patient.GetBytesNeeded() > 100 * 1024 * 1024, "the decoder asks for all of a long header, got " + std::to_string(patient.GetBytesNeeded()));

        const auto limited = std::make_shared<wdx::StreamSource>();
        const wdx::SourceDecoder impatient{};
        impatient.SetSource(limited);
        limited->Append(endless);
        check(rejectsHeader([&] { return impatient.Demux(0.1); }, result), "headers longer than the limit are rejected");

        const auto whole = std::make_shared<PullSource>(data);
        const wdx::SourceDecoder strict{};
        strict.SetMaxHeaderSize(32);
        strict.SetSource(whole);
        check(rejectsHeader([&] { return strict.EnsureHeader(); }, result), "a real file is rejected when its headers are longer than the limit");
        check(whole->GetAvailable() <= 32, "nothing past the limit is asked for, got " + std::to_string(whole->GetAvailable()) + " bytes");

        const auto lazy = std::make_shared<PullSource>(data);
        const wdx::SourceDecoder ready{};
        ready.SetSource(lazy);
        check(!ready.IsReady(), "a decoder is not ready before its headers have been read");
        check(ready.EnsureHeader() == wdx::DemuxResult::Success && ready.IsReady(), "a decoder is ready once its headers have been read");
        check(ready.HasVideo() && ready.GetBytesNeeded() == 0 && ready.GetPosition() == 0, "a ready decoder knows its tracks, needs nothing and has not moved");
        std::size_t packets = 0;
        ready.SetVideoPacketCallback([&](const std::shared_ptr<wdx::Packet>&, wdx::IVideoDecoder*) { packets++; });
        ready.SetAudioPacketCallback([&](const std::shared_ptr<wdx::Packet>&, wdx::IAudioDecoder*) { packets++; });
        check(ready.Demux(0) == wdx::DemuxResult::Success && packets == 0, "a demux of no time gives no packets");
    }

    std::size_t testDecoder(const char* path, const Expected& expected)
    {
        const auto source = std::make_shared<wdx::FileSource>(path);
        const auto decoder = std::make_shared<wdx::SourceDecoder>();
        decoder->SetSource(source);

        check(!decoder->IsReady(), "a decoder is not ready until its headers have been read");
        check(decoder->EnsureHeader() == wdx::DemuxResult::Success && decoder->IsReady(), "the headers of a complete file can be read");
        check(decoder->HasVideo(), "source has a video track");
        const auto track = decoder->GetVideoTrack();
        check(track.width == expected.width, "video width is " + std::to_string(expected.width) + ", got " + std::to_string(track.width));
        check(track.height == expected.height, "video height is " + std::to_string(expected.height) + ", got " + std::to_string(track.height));

        std::size_t videoPackets = 0;
        std::size_t videoFrames = 0;
        std::size_t audioPackets = 0;
        std::size_t audioSamples = 0;
        double lastVideoTime = -1;
        double lastAudioTime = -1;
        bool videoInOrder = true;
        bool audioInOrder = true;
        bool firstFrameHasContent = false;
        bool framesCarryPacketTimes = true;
        std::vector<double> videoPacketTimes{};
        std::vector<std::uint8_t> rgba(static_cast<std::size_t>(track.width) * track.height * 4);

        const auto takeFrames = [&](wdx::IVideoDecoder* videoDecoder)
        {
            while (const auto frame = videoDecoder->GetFrame())
            {
                if (videoFrames == 0)
                {
                    frame->ToRgba(rgba);
                    for (const auto value : rgba)
                    {
                        firstFrameHasContent = firstFrameHasContent || value != rgba[0];
                    }
                }
                framesCarryPacketTimes = framesCarryPacketTimes && videoFrames < videoPacketTimes.size()
                    && frame->GetTime() == videoPacketTimes[videoFrames];
                videoFrames++;
            }
        };

        decoder->SetVideoPacketCallback([&](const std::shared_ptr<wdx::Packet>& packet, wdx::IVideoDecoder* videoDecoder)
        {
            videoInOrder = videoInOrder && packet->GetTime() >= lastVideoTime;
            lastVideoTime = packet->GetTime();
            videoPackets++;
            videoPacketTimes.push_back(packet->GetTime());
            videoDecoder->Decode(packet);
            takeFrames(videoDecoder);
        });
        decoder->SetAudioPacketCallback([&](const std::shared_ptr<wdx::Packet>& packet, wdx::IAudioDecoder* audioDecoder)
        {
            audioInOrder = audioInOrder && packet->GetTime() >= lastAudioTime;
            lastAudioTime = packet->GetTime();
            audioPackets++;
            std::vector<float> pcm{};
            audioSamples += audioDecoder->Decode(packet, pcm);
        });

        auto result = decoder->Demux(0.1);
        while (result == wdx::DemuxResult::Success)
        {
            result = decoder->Demux(0.1);
        }

        check(result == wdx::DemuxResult::Finished, "demux reaches the end of a complete file");
        takeFrames(decoder->GetVideoDecoder());
        check(videoPackets == expected.videoPackets, "video packet count is " + std::to_string(expected.videoPackets) + ", got " + std::to_string(videoPackets));
        check(videoFrames == videoPackets, "every video packet decodes to a frame, got " + std::to_string(videoFrames));
        check(framesCarryPacketTimes, "every frame carries the time of the packet it was decoded from");
        check(firstFrameHasContent, "first frame converts to a non-blank RGBA image");
        check(videoInOrder, "video packet times never go backwards");
        check(audioInOrder, "audio packet times never go backwards");
        if (decoder->HasAudio())
        {
            check(audioPackets > 0 && audioSamples > 0, "audio packets decode to samples");
        }

        std::cout << path << ": " << track.width << "x" << track.height << ", " << decoder->GetDuration() << "s, "
            << videoPackets << " video packets, " << videoFrames << " frames, "
            << audioPackets << " audio packets, " << audioSamples << " samples" << std::endl;
        return audioPackets;
    }
}

int main(int argc, char* argv[])
{
    if (argc < 5)
    {
        std::cerr << "Usage: " << argv[0] << " <path_to_webm_file> <width> <height> <video_packets>" << std::endl;
        return 2;
    }
    try
    {
        const Expected expected{std::atoi(argv[2]), std::atoi(argv[3]), std::strtoull(argv[4], nullptr, 10)};
        const auto audioPackets = testDecoder(argv[1], expected);
        testStreaming(argv[1], expected, audioPackets);
        testPulling(argv[1], expected, audioPackets);
        testSeeking(argv[1]);
        testRejecting(argv[1]);
        testIndexSeeking(argv[1]);
        testSeekingIntoDropped(argv[1]);
        testDecoderFactories(argv[1]);
    }
    catch (const std::exception& e)
    {
        check(false, std::string("threw ") + typeid(e).name() + ": " + e.what());
    }
    return failures == 0 ? 0 : 1;
}
