#ifdef WEBM_DX_CODEC_AUDIO_VORBIS
#include "VorbisDecoder.h"
#include <array>
#include "webmdx/errors.h"

namespace wdx {
    namespace {
        constexpr auto HEADER_COUNT = 3;

        // Matroska keeps the three Vorbis headers in CodecPrivate with Xiph lacing:
        // a count byte, the sizes of all but the last header, then the headers.
        std::array<std::span<const std::uint8_t>, HEADER_COUNT> splitHeaders(const AudioTrack &track) {
            const std::span data{track.codecPrivate, static_cast<std::size_t>(track.codecPrivateSize)};
            if (data.empty() || data[0] != HEADER_COUNT - 1) {
                throw AudioDecoderError("Vorbis track has no valid codec private data");
            }

            std::size_t offset = 1;
            std::array<std::size_t, HEADER_COUNT> sizes{};
            for (auto i = 0; i < HEADER_COUNT - 1; i++) {
                std::uint8_t sizeByte;
                do {
                    if (offset >= data.size()) {
                        throw AudioDecoderError("Vorbis codec private data is truncated");
                    }
                    sizeByte = data[offset++];
                    sizes[i] += sizeByte;
                } while (sizeByte == 255);
            }

            if (offset + sizes[0] + sizes[1] >= data.size()) {
                throw AudioDecoderError("Vorbis codec private data is truncated");
            }
            sizes[2] = data.size() - offset - sizes[0] - sizes[1];

            std::array<std::span<const std::uint8_t>, HEADER_COUNT> headers{};
            for (auto i = 0; i < HEADER_COUNT; i++) {
                headers[i] = data.subspan(offset, sizes[i]);
                offset += sizes[i];
            }
            return headers;
        }

        ogg_packet toOggPacket(const std::span<const std::uint8_t> &data) {
            ogg_packet packet{};
            packet.packet = const_cast<unsigned char *>(data.data());
            packet.bytes = static_cast<long>(data.size());
            return packet;
        }
    }

    VorbisDecoder::VorbisDecoder(const AudioTrack &track) {
        _track = track;
        const auto headers = splitHeaders(track);

        vorbis_info_init(&_info);
        vorbis_comment_init(&_comment);
        for (auto i = 0; i < HEADER_COUNT; i++) {
            auto packet = toOggPacket(headers[i]);
            packet.b_o_s = i == 0;
            if (vorbis_synthesis_headerin(&_info, &_comment, &packet) != 0) {
                vorbis_comment_clear(&_comment);
                vorbis_info_clear(&_info);
                throw AudioDecoderError("Failed to read Vorbis headers");
            }
        }

        if (vorbis_synthesis_init(&_state, &_info) != 0) {
            vorbis_comment_clear(&_comment);
            vorbis_info_clear(&_info);
            throw AudioDecoderError("Failed to create VorbisDecoder");
        }
        vorbis_block_init(&_state, &_block);
    }

    int VorbisDecoder::Decode(const std::shared_ptr<Packet>& input, std::vector<float> &pcm) {
        auto packet = toOggPacket(input->GetData());
        if (vorbis_synthesis(&_block, &packet) != 0 || vorbis_synthesis_blockin(&_state, &_block) != 0) {
            throw AudioDecoderError("Failed to decode vorbis packet");
        }

        const auto channels = _info.channels;
        std::size_t samplesDecoded = 0;
        float **planes = nullptr;
        // A packet can release no samples (the first one never does) or more than one block of them
        for (auto frames = vorbis_synthesis_pcmout(&_state, &planes); frames > 0; frames = vorbis_synthesis_pcmout(&_state, &planes)) {
            const auto needed = samplesDecoded + static_cast<std::size_t>(frames) * channels;
            if (pcm.size() < needed) {
                pcm.resize(needed);
            }
            for (auto frame = 0; frame < frames; frame++) {
                for (auto channel = 0; channel < channels; channel++) {
                    pcm[samplesDecoded++] = planes[channel][frame];
                }
            }
            vorbis_synthesis_read(&_state, frames);
        }
        return static_cast<int>(samplesDecoded);
    }

    VorbisDecoder::~VorbisDecoder() {
        vorbis_block_clear(&_block);
        vorbis_dsp_clear(&_state);
        vorbis_comment_clear(&_comment);
        vorbis_info_clear(&_info);
    }

    void VorbisDecoder::Reset()
    {
        vorbis_synthesis_restart(&_state);
    }
}
#endif
