#include "scaler/ymq/internal/raw_connection_tcp_fd.h"

#include <algorithm>
#include <cassert>  // assert

namespace scaler {
namespace ymq {

std::pair<uint64_t, RawConnectionTCPFD::IOStatus> RawConnectionTCPFD::tryReadUntilComplete(void* dest, size_t size)
{
    uint64_t cnt = 0;
    while (size) {
        const auto current = readBytes((char*)dest + cnt, size);
        if (current) {
            cnt += current.value();
            size -= current.value();
        } else {
            return {cnt, current.error()};
        }
    }
    return {cnt, IOStatus::MoreBytesAvailable};
}

std::pair<uint64_t, RawConnectionTCPFD::IOStatus> RawConnectionTCPFD::tryWriteUntilComplete(
    const std::vector<std::pair<void*, size_t>>& buffers)
{
    if (buffers.empty()) {
        return {0, IOStatus::MoreBytesAvailable};
    }

    std::vector<size_t> prefixSum(buffers.size() + 1);
    for (size_t i = 0; i < buffers.size(); ++i) {
        prefixSum[i + 1] = prefixSum[i] + buffers[i].second;
    }
    const size_t total = prefixSum.back();

    size_t sent = 0;
    while (sent != total) {
        auto unfinished = std::upper_bound(prefixSum.begin(), prefixSum.end(), sent);
        --unfinished;

        std::vector<std::pair<void*, size_t>> currentBuffers;

        auto begin          = buffers.begin() + std::distance(prefixSum.begin(), unfinished);
        const size_t remain = sent - *unfinished;

        currentBuffers.push_back({(char*)begin->first + remain, begin->second - remain});
        while (++begin != buffers.end()) {
            currentBuffers.push_back(*begin);
        }

        const auto res = writeBytes(currentBuffers);
        if (res) {
            sent += res.value();
        } else {
            return {sent, res.error()};
        }
    }
    return {total, IOStatus::MoreBytesAvailable};
}

void RawConnectionTCPFD::shutdownBoth() noexcept
{
    shutdownWrite();
    shutdownRead();
}

}  // namespace ymq
}  // namespace scaler
