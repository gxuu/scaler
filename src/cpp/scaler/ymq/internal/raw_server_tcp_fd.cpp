
#include "scaler/ymq/internal/raw_server_tcp_fd.h"

#include "scaler/error/error.h"
#include "scaler/ymq/internal/defs.h"

namespace scaler {
namespace ymq {

bool RawServerTCPFD::setReuseAddress()
{
    int optval = 1;
    if (setsockopt(_serverFD, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval)) == -1) {
        CloseAndZeroSocket(_serverFD);
        return false;
    }
    return true;
}

void RawServerTCPFD::bindAndListen()
{
    if (bind(_serverFD, &_addr, sizeof(_addr)) == -1) {
        CloseAndZeroSocket(_serverFD);
        unrecoverableError({
            Error::ErrorCode::ConfigurationError,
            "Originated from",
            "bind(2)",
            "Errno is",
            strerror(GetErrorCode()),
            "_serverFD",
            _serverFD,
        });

        return;
    }

    if (listen(_serverFD, SOMAXCONN) == -1) {
        CloseAndZeroSocket(_serverFD);
        unrecoverableError({
            Error::ErrorCode::ConfigurationError,
            "Originated from",
            "listen(2)",
            "Errno is",
            strerror(GetErrorCode()),
            "_serverFD",
            _serverFD,
        });

        return;
    }
}

}  // namespace ymq
}  // namespace scaler
