#include "scaler/ymq/internal/raw_client_tcp_fd.h"

namespace scaler {
namespace ymq {

void RawClientTCPFD::zeroNativeHandle() noexcept
{
    _clientFD = 0;
}

RawClientTCPFD::~RawClientTCPFD()
{
    destroy();
}

}  // namespace ymq
}  // namespace scaler
