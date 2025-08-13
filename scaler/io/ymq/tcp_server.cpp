#include "scaler/io/ymq/tcp_server.h"

#ifdef __linux__
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif  // __linux__
#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#endif  // _WIN32


#include <expected>
#include <memory>

#include "scaler/io/ymq/error.h"
#include "scaler/io/ymq/event_loop_thread.h"
#include "scaler/io/ymq/event_manager.h"
#include "scaler/io/ymq/io_socket.h"
#include "scaler/io/ymq/logging.h"
#include "scaler/io/ymq/message_connection_tcp.h"
#include "scaler/io/ymq/network_utils.h"


namespace scaler {
namespace ymq {

static auto GetErrorCode()
{
#ifdef __linux__
    return errno;
#endif  // __linux__
#ifdef _WIN32
    return WSAGetLastError();
#endif  // _WIN32
}

#ifdef _WIN32
// TODO: Error handling for this function
void TcpServer::prepareAcceptSocket()
{
    _newConn = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (_newConn == INVALID_SOCKET) {
        exit(1);
    }

    DWORD bytesReturned = 0;
    if (!_acceptExFunc(
            _serverFd,
            _newConn,
            _buffer,
            0,
            sizeof(sockaddr_in) + 16,
            sizeof(sockaddr_in) + 16,
            &bytesReturned,
            _eventManager.get())) {
        int err = WSAGetLastError();
        if (err != ERROR_IO_PENDING) {
            std::cerr << "AcceptEx failed. Error: " << err << std::endl;
            closesocket(_newConn);
            return;
        }
    }
    std::cout << "Posted an asynchronous AcceptEx operation." << std::endl;
}
#endif  // _WIN32


int TcpServer::createAndBindSocket()
{
#ifdef __linux__
    int server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
    if (server_fd == -1) {
        unrecoverableError({
            Error::ErrorCode::ConfigurationError,
            "Originated from",
            "socket(2)",
            "Errno is",
            strerror(errno),
            "_serverFd",
            _serverFd,
        });

        // _onBindReturn(std::unexpected(Error {Error::ErrorCode::ConfigurationError}));
        return -1;
    }
#endif  // __linux__
#ifdef _WIN32
    auto server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd == -1) {
        // TODO: Error handling should behave like Linux
        _onBindReturn(std::unexpected(Error {Error::ErrorCode::ConfigurationError}));
        return -1;
    }
    unsigned long turnOnNonBlocking = 1;
    ioctlsocket(server_fd, FIONBIO, &turnOnNonBlocking);
#endif  // _WIN32

    int optval = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)& optval, sizeof(optval)) == -1) {
        log(LoggingLevel::error,
            "Originated from",
            "setsockopt(2)", "Errno is",
            strerror(GetErrorCode()) // ,
        );
        CloseAndZeroSocket(server_fd);
        _onBindReturn(std::unexpected(Error {Error::ErrorCode::SetSockOptNonFatalFailure}));
        return -1;
    }

    if (bind(server_fd, &_addr, sizeof(_addr)) == -1) {
        CloseAndZeroSocket(server_fd);
        unrecoverableError({
            Error::ErrorCode::ConfigurationError,
            "Originated from",
            "bind(2)",
            "Errno is",
            strerror(GetErrorCode()),
            "server_fd",
            server_fd,
        });

        return -1;
    }

    if (listen(server_fd, SOMAXCONN) == -1) {
        CloseAndZeroSocket(server_fd);
        unrecoverableError({
            Error::ErrorCode::ConfigurationError,
            "Originated from",
            "listen(2)",
            "Errno is",
            strerror(GetErrorCode()),
            "server_fd",
            server_fd,
        });

        return -1;
    }

    return server_fd;
}

TcpServer::TcpServer(
    std::shared_ptr<EventLoopThread> eventLoopThread,
    std::string localIOSocketIdentity,
    sockaddr addr,
    BindReturnCallback onBindReturn) noexcept
    : _eventLoopThread(eventLoopThread)
    , _localIOSocketIdentity(std::move(localIOSocketIdentity))
    , _eventManager(std::make_unique<EventManager>())
    , _addr(std::move(addr))
    , _onBindReturn(std::move(onBindReturn))
    , _serverFd {}
{
    _eventManager->onRead  = [this] { this->onRead(); };
    _eventManager->onWrite = [this] { this->onWrite(); };
    _eventManager->onClose = [this] { this->onClose(); };
    _eventManager->onError = [this] { this->onError(); };
}

void TcpServer::onCreated()
{
    _serverFd = createAndBindSocket();
    if (_serverFd == -1) {
        _serverFd = 0;
        return;
    }
#ifdef __linux__
    _eventLoopThread->_eventLoop.addFdToLoop(_serverFd, EPOLLIN | EPOLLET, this->_eventManager.get());
#endif  // __linux__
#ifdef _WIN32
    // Events and EventManager are not used here.
    _eventLoopThread->_eventLoop.addFdToLoop(_serverFd, 0, nullptr);
    // Retrieve AcceptEx pointer once
    GUID guidAcceptEx   = WSAID_ACCEPTEX;
    DWORD bytesReturned = 0;
    if (WSAIoctl(
            _serverFd,
            SIO_GET_EXTENSION_FUNCTION_POINTER,
            &guidAcceptEx,
            sizeof(guidAcceptEx),
            &_acceptExFunc,
            sizeof(_acceptExFunc),
            &bytesReturned,
            nullptr,
            nullptr) == SOCKET_ERROR) {
        // TODO: Error handling here
        printf("?\n");
        exit(1);
    }
    prepareAcceptSocket();
#endif  // _WIN32

    _onBindReturn({});
}

void TcpServer::onRead()
{
#ifdef __linux__
    while (true) {
        sockaddr remoteAddr {};
        socklen_t remoteAddrLen = sizeof(remoteAddr);

        int fd = accept4(_serverFd, &remoteAddr, &remoteAddrLen, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (fd < 0) {
            const int myErrno = errno;
            switch (myErrno) {
                // Not an error
                // case EWOULDBLOCK: // same as EAGAIN
                case EAGAIN:
                case ECONNABORTED: return;

                case ENOTSOCK:
                case EOPNOTSUPP:
                case EINVAL:
                case EBADF:
                    unrecoverableError({
                        Error::ErrorCode::CoreBug,
                        "Originated from",
                        "accept4(2)",
                        "Errno is",
                        strerror(myErrno),
                        "_serverFd",
                        _serverFd,
                    });
                    break;

                case EINTR:
                    unrecoverableError({
                        Error::ErrorCode::SignalNotSupported,
                        "Originated from",
                        "accept4(2)",
                        "Errno is",
                        strerror(myErrno),
                    });
                    break;

                // config
                case EMFILE:
                case ENFILE:
                case ENOBUFS:
                case ENOMEM:
                case EFAULT:
                case EPERM:
                case EPROTO:
                case ENOSR:
                case ESOCKTNOSUPPORT:
                case EPROTONOSUPPORT:
                case ETIMEDOUT:
                default:
                    unrecoverableError({
                        Error::ErrorCode::ConfigurationError,
                        "Originated from",
                        "accept4(2)",
                        "Errno is",
                        strerror(myErrno),
                    });
                    break;
            }
        }

        if (remoteAddrLen > sizeof(remoteAddr)) {
            unrecoverableError({
                Error::ErrorCode::IPv6NotSupported,
                "Originated from",
                "accept4(2)",
                "remoteAddrLen",
                remoteAddrLen,
                "sizeof(remoteAddr)",
                sizeof(remoteAddr),
            });
        }

        std::string id = this->_localIOSocketIdentity;
        auto sock      = this->_eventLoopThread->_identityToIOSocket.at(id);
        sock->onConnectionCreated(setNoDelay(fd), getLocalAddr(fd), remoteAddr, false);
    }
#endif
#ifdef _WIN32
        if (setsockopt(_newConn, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
            reinterpret_cast<char*>(&_serverFd), sizeof(_serverFd)) == SOCKET_ERROR) {
            std::cerr << "setsockopt(SO_UPDATE_ACCEPT_CONTEXT) failed. Error: " << WSAGetLastError() << std::endl;
            CloseAndZeroSocket(_serverFd);
            return;
        }
        std::cout << "Accepted a new connection. Client socket: " << _newConn << std::endl;
        prepareAcceptSocket();
        std::string id = this->_localIOSocketIdentity;
        auto sock      = this->_eventLoopThread->_identityToIOSocket.at(id);
        sock->onConnectionCreated(setNoDelay(_newConn), getLocalAddr(_newConn), {}, false);
#endif  // _WIN32
}

TcpServer::~TcpServer() noexcept
{
    if (_serverFd != 0) {
        _eventLoopThread->_eventLoop.removeFdFromLoop(_serverFd);
        CloseAndZeroSocket(_serverFd);
    }
}

}  // namespace ymq
}  // namespace scaler
