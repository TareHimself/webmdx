#pragma once
#ifdef WEBM_DX_CODEC_VIDEO_DAV1D
#include "webmdx/IVideoDecoder.h"
#include "PlanarFrame.h"
#include <dav1d/dav1d.h>
#include <deque>
namespace wdx {
    class Av1Decoder final : public IVideoDecoder {
    public:
        explicit Av1Decoder(const VideoTrack& track);
        void Decode(const ::std::shared_ptr<Packet>& input) override;

        std::shared_ptr<IDecodedVideoFrame> GetFrame() override;

        ~Av1Decoder() override;

        void Reset() override;
        void Flush() override;

    private:
        // Pulls every picture dav1d currently has ready into _pendingPictures.
        // A single Decode() can make more than one picture available (frame
        // delay/reordering), so this must not discard any of them.
        void DrainPictures();

        VideoTrack _track{};
        Dav1dContext* _context{nullptr};
        Dav1dSettings _initSettings{};
        Dav1dSettings _playSettings{};
        std::deque<Dav1dPicture> _pendingPictures{};
        Dav1dData _data = {};
    };
}
#endif