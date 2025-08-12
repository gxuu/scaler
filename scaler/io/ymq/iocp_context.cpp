#ifdef _WIN32

#include "scaler/io/ymq/iocp_context.h"

#include <cerrno>
#include <functional>

#include "scaler/io/ymq/error.h"
#include "scaler/io/ymq/event_manager.h"

namespace scaler {
namespace ymq {

void IocpContext::execPendingFunctions()
{
    while (_delayedFunctions.size()) {
        auto top = std::move(_delayedFunctions.front());
        top();
        _delayedFunctions.pop();
    }
}

void IocpContext::loop()
{
    std::array<OVERLAPPED_ENTRY, _reventSize> events {};
    ULONG n = 0;
    GetQueuedCompletionStatusEx(_completionPort, events.data(), _reventSize, &n, INFINITE, true);

    for (auto it = events.begin(); it != events.begin() + n; ++it) {
        auto current_event = *it;
        if (current_event.lpCompletionKey == _isInterruptiveFd) {
            auto vec = _interruptiveFunctions.dequeue();
            std::ranges::for_each(vec, [](auto&& x) { x(); });
        } else if (current_event.lpCompletionKey == _isTimingFd) {
            auto vec = _timingFunctions.dequeue();
            std::ranges::for_each(vec, [](auto& x) { x(); });
        } else {
            auto event = (EventManager*)(current_event.lpOverlapped);
            // TODO: Figure out the best stuff to put in
            event->onEvents(0);
        }
    }

    execPendingFunctions();
}

void IocpContext::addFdToLoop(int fd, uint64_t events, EventManager* manager)
{
    if (!CreateIoCompletionPort((HANDLE)fd, _completionPort, static_cast<ULONG_PTR>(fd), 0)) {
        // TODO: Handle error
        exit(1);
    }
}

void IocpContext::removeFdFromLoop(int fd)
{
}

}  // namespace ymq
}  // namespace scaler

#endif  // __linux__
