#pragma once

#include <cstddef>  // size_t
#include <cstdint>  // uint64_t
#include <expected>
#include <utility>  // std::pair
#include <vector>

namespace scaler {
namespace ymq {

class RawConnectionTCPFD {
public:
    enum class IOStatus {
        MoreBytesAvailable,
        Drained,
        Disconnected,
        Aborted,
    };

    // TODO: It is awkward because the user, when received a disconnect, would still
    // calculate the pointer offset himself.
    std::pair<uint64_t, IOStatus> tryReadUntilComplete(void* dest, size_t size);
    std::pair<uint64_t, IOStatus> tryWriteUntilComplete(const std::vector<std::pair<void*, size_t>>& buffers);

    // I need the semantic that, if this return certain value,
    // i need to immediatley read bytes
    bool prepareReadBytes(void* notifyHandle);
    // TODO: This might need error handling
    std::pair<size_t, bool> prepareWriteBytes(void* dest, size_t len, void* notifyHandle);

public:
    std::expected<uint64_t, IOStatus> readBytes(void* dest, size_t size);
    std::expected<uint64_t, IOStatus> writeBytes(const std::vector<std::pair<void*, size_t>>& buffers);
    // TODO: Semantics of destruction
    // For compatibility - Windows fd is defined to be HANDLE.
    // maybe we should close it eventually
    uint64_t _fd;
};

}  // namespace ymq
}  // namespace scaler
