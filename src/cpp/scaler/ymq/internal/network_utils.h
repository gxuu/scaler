#pragma once
#include "scaler/ymq/internal/defs.h"

namespace scaler {
namespace ymq {

bool setReuseAddress(auto fd)
{
    int optval = 1;
    return !(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval)) == -1);
}

}  // namespace ymq
}  // namespace scaler
