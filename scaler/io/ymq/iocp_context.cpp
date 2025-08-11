#ifdef _WIN32

#include "scaler/io/ymq/iocp_context.h"

#include <cerrno>
#include <functional>

// #include "scaler/io/ymq/error.h"
// #include "scaler/io/ymq/event_manager.h"

namespace scaler {
namespace ymq {

void IocpContext::execPendingFunctions()
{
}

void IocpContext::loop()
{
}

void IocpContext::addFdToLoop(int fd, uint64_t events, EventManager* manager)
{
}

void IocpContext::removeFdFromLoop(int fd)
{

}  // namespace ymq
}  // namespace scaler

#endif  // __linux__
