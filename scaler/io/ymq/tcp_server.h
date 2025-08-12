#pragma once

// C++
#ifdef __linux__
#include <sys/socket.h>
#endif  // __linux__
#ifdef _WIN32
#include <winsock2.h>
#include <mswsock.h>
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif  // _WIN32


#include <memory>

// First-party
#include "scaler/io/ymq/configuration.h"

namespace scaler {
namespace ymq {

class EventLoopThread;
class EventManager;

class TcpServer {
public:
    using BindReturnCallback = Configuration::BindReturnCallback;

    TcpServer(
        std::shared_ptr<EventLoopThread> eventLoop,
        std::string localIOSocketIdentity,
        sockaddr addr,
        BindReturnCallback onBindReturn) noexcept;
    TcpServer(const TcpServer&)            = delete;
    TcpServer& operator=(const TcpServer&) = delete;
    ~TcpServer() noexcept;

    void onCreated();
    std::shared_ptr<EventLoopThread> _eventLoopThread;

private:
    // Implementation defined method. accept(3) should happen here.
    // This function will call user defined onAcceptReturn()
    // It will handle error it can handle. If it is unreasonable to
    // handle the error here, pass it to onAcceptReturn()
    void onRead();
    void onWrite() { printf("%s\n", __PRETTY_FUNCTION__); }
    void onClose() { printf("%s\n", __PRETTY_FUNCTION__); }
    void onError() { printf("%s\n", __PRETTY_FUNCTION__); }

    int createAndBindSocket();

    BindReturnCallback _onBindReturn;
    int _serverFd;
    sockaddr _addr;
    std::string _localIOSocketIdentity;

    std::unique_ptr<EventManager> _eventManager;  // will copy the `onRead()` to itself

#ifdef _WIN32
    // TODO: Put those in the constructor
    LPFN_ACCEPTEX _acceptExFunc;
    SOCKET _newConn {0}; 
    char _buffer[128] {};
    void prepareAcceptSocket();
#endif  // _WIN32
};

}  // namespace ymq
}  // namespace scaler
