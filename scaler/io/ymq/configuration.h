#pragma once

// C++
#include <expected>
#include <functional>
#include <memory>
#include <string>

// Because the devil cast spells in plain English.
#ifdef _WIN32
#undef SendMessageCallback
#endif  // _WIN32


namespace scaler {
namespace ymq {

class EpollContext;
class IocpContext;
class Message;
class IOSocket;
class Error;

#if defined(_WIN32)
#ifdef BUILDING_CC_YMQ
#define CC_YMQ_API __declspec(dllexport)
#else
#define CC_YMQ_API __declspec(dllimport)
#endif
#else
#define CC_YMQ_API
#endif

struct CC_YMQ_API Configuration {
#ifdef __linux__
    using PollingContext                  = EpollContext;
#endif  // __linux__

#ifdef _WIN32
    using PollingContext                  = IocpContext;
#endif  // _WIN32
    using IOSocketIdentity                = std::string;
    using SendMessageCallback             = std::move_only_function<void(std::expected<void, Error>)>;
    using RecvMessageCallback             = std::move_only_function<void(std::pair<Message, Error>)>;
    using ConnectReturnCallback           = std::move_only_function<void(std::expected<void, Error>)>;
    using BindReturnCallback              = std::move_only_function<void(std::expected<void, Error>)>;
    using CreateIOSocketCallback          = std::move_only_function<void(std::shared_ptr<IOSocket>)>;
    using TimedQueueCallback              = std::move_only_function<void()>;
    using ExecutionFunction               = std::move_only_function<void()>;
    using ExecutionCancellationIdentifier = size_t;
};

}  // namespace ymq
}  // namespace scaler
