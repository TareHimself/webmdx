#pragma once
#include <webmdx/Packet.h>

namespace wdx {
    Packet::Packet(const double time, const std::span<std::uint8_t>& data, const bool isKey)
    {
        _time = time;
        _data = std::vector<std::uint8_t>(data.size());
        std::ranges::copy(data, _data.begin());
        _isKey = isKey;
    }

    double Packet::GetTime() const
    {
        return _time;
    }

    bool Packet::IsKey() const
    {
        return _isKey;
    }

    std::span<std::uint8_t> Packet::GetData()
    {
        return {_data.data(), _data.size()};
    }
}
