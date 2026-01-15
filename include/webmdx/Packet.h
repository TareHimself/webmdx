#pragma once
#include <cstdint>
#include <span>
#include <vector>
#include "export.h"
namespace wdx {
    class WEBMDX_API Packet {

        double _time;
        bool _isKey;
        std::vector<std::uint8_t> _data;
    public:
        Packet(double time,const std::span<std::uint8_t>& data,bool isKey);
        [[nodiscard]] double GetTime() const;
        [[nodiscard]] bool IsKey() const;
        std::span<std::uint8_t> GetData();
    };
}
