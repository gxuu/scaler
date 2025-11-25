#pragma once

#include <cstdint>  // uint64_t

#include "scaler/ymq/internal/defs.h"
#include "scaler/ymq/internal/socket_address.h"

namespace scaler {
namespace ymq {

class RawStreamClientHandle {
public:
    RawStreamClientHandle(SocketAddress remoteAddr);
    ~RawStreamClientHandle();

    RawStreamClientHandle(RawStreamClientHandle&&)                  = delete;
    RawStreamClientHandle& operator=(RawStreamClientHandle&& other) = delete;
    RawStreamClientHandle(const RawStreamClientHandle&)             = delete;
    RawStreamClientHandle& operator=(const RawStreamClientHandle&)  = delete;

    void create();
    void destroy();
    bool prepConnect(void* notifyHandle);
    bool needRetry();

    void zeroNativeHandle() noexcept;

    auto nativeHandle() const noexcept { return (RawSocketType)_clientFD; }

    bool isNetworkFD() const noexcept { return _remoteAddr._type == SocketAddress::Type::TCP; }

    socklen_t addrSize() const noexcept { return _remoteAddr._addrLen; }

private:
    uint64_t _clientFD;
    SocketAddress _remoteAddr;
#ifdef _WIN32
    LPFN_CONNECTEX _connectExFunc;
#endif  // _WIN32
};

}  // namespace ymq
}  // namespace scaler
