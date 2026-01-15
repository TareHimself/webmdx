#define NOMINMAX
#include "NetworkSource.h"
#include <cpr/cpr.h>
#include <algorithm>
void NetworkSource::FetchHeaders()
{
    auto response = cpr::Head(cpr::Url{_url});
    if (response.status_code != 200)
    {
        throw std::runtime_error("Failed to fetch headers");
    }

    auto header = response.header.at("Content-Length");

    std::istringstream iss(header);

    iss >> _length;
}


NetworkSource::NetworkSource(const std::string& url, const std::uint64_t& preloadSize)
{
    _url = url;
    _preloadSize = preloadSize;
    FetchHeaders();
}

void NetworkSource::Read(const std::int64_t& pos, std::span<std::uint8_t> data)
{
    //MakeAvailable(_length);
    MakeAvailable(std::min<std::uint64_t>(pos + data.size(),GetLength()));
    memcpy(data.data(),_data.data() + pos,data.size());
}

std::int64_t NetworkSource::GetLength() const
{
    return _length;
}

std::int64_t NetworkSource::GetAvailable() const
{
    return _length;
}

void NetworkSource::MakeAvailable(const std::uint64_t& size)
{
    auto available = _data.size();
    auto end = size;
    auto networkStart = available;
    if (end > networkStart)
    {
        if (end - available < _preloadSize && _length > 0)
        {
            end = std::min(end + _preloadSize,static_cast<std::uint64_t>(_length));
        }

        auto result = cpr::Get(cpr::Url{_url},cpr::Range{available,end - 1});

        if (result.status_code == 206)
        {
            _data.resize(_data.size() + result.downloaded_bytes);
            memcpy(_data.data() + available,result.text.data(),result.downloaded_bytes);
        }
        else
        {
            _data.resize(result.downloaded_bytes);
            memcpy(_data.data(),result.text.data(),result.downloaded_bytes);
        }
    }
}
