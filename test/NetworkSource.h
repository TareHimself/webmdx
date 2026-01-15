#pragma once
#include "webmdx/ISource.h"
#include <string>
#include <vector>

class NetworkSource : public wdx::ISource
{

    bool _headersFetched{ false };
    std::int64_t _pos{ 0 };
    std::int64_t _length{ 0 };
    std::vector<std::uint8_t> _data;
    void FetchHeaders();

    std::string _url;
    std::uint64_t _preloadSize{ 0 };
public:

    explicit NetworkSource(const std::string& url,const std::uint64_t& preloadSize = 1024 * 1024);

    void Read(const std::int64_t& pos, std::span<std::uint8_t> data) override;
    [[nodiscard]] std::int64_t GetLength() const override;
    [[nodiscard]] std::int64_t GetAvailable() const override;
    void MakeAvailable(const std::uint64_t& size) override;
};
