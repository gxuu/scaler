#ifdef _WIN32
#include "scaler/error/error.h"
#include "scaler/ymq/internal/defs.h"
#include "scaler/ymq/internal/raw_stream_client_handle.h"

namespace scaler {
namespace ymq {

RawStreamClientHandle::RawStreamClientHandle(sockaddr remoteAddr)
    : _clientFD {}, _remoteAddr {}, _addrSize(sizeof(sockaddr))
{
    *(sockaddr*)&_remoteAddr = std::move(remoteAddr);
    _connectExFunc           = {};

    auto tmp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    DWORD res;
    GUID guid = WSAID_CONNECTEX;
    WSAIoctl(
        tmp,
        SIO_GET_EXTENSION_FUNCTION_POINTER,
        (void*)&guid,
        sizeof(GUID),
        &_connectExFunc,
        sizeof(_connectExFunc),
        &res,
        0,
        0);
    closesocket(tmp);
    if (!_connectExFunc) {
        unrecoverableError({
            Error::ErrorCode::CoreBug,
            "Originated from",
            "WSAIoctl",
            "Errno is",
            GetErrorCode(),
            "_connectExFunc",
            (void*)_connectExFunc,
        });
    }
}

RawStreamClientHandle::RawStreamClientHandle(sockaddr_un remoteAddr): _clientFD {}, _addrSize(sizeof(sockaddr_un))
{
    printf("Hitting IPC Socket constructor, not supported on this system!\n");
    assert(false);
}

void RawStreamClientHandle::create()
{
    _clientFD = {};
    if (_addrSize == sizeof(sockaddr)) {
        _clientFD = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    }
    if (_addrSize == sizeof(sockaddr_un)) {
        _clientFD = socket(AF_UNIX, SOCK_STREAM, 0);
    }
    if (_clientFD == -1) {
        unrecoverableError({
            Error::ErrorCode::CoreBug,
            "Originated from",
            "socket(2)",
            "Errno is",
            strerror(GetErrorCode()),
        });
    }
    u_long nonblock = 1;
    ioctlsocket(_clientFD, FIONBIO, &nonblock);
}

bool RawStreamClientHandle::prepConnect(void* notifyHandle)
{
    sockaddr_in localAddr      = {};
    localAddr.sin_family       = AF_INET;
    const char ip4[]           = {127, 0, 0, 1};
    *(int*)&localAddr.sin_addr = *(int*)ip4;

    const int bindRes = bind(_clientFD, (struct sockaddr*)&localAddr, _addrSize);
    if (bindRes == -1) {
        unrecoverableError({
            Error::ErrorCode::ConfigurationError,
            "Originated from",
            "bind",
            "Errno is",
            GetErrorCode(),
            "_clientFD",
            _clientFD,
        });
    }

    const bool ok =
        _connectExFunc(_clientFD, (sockaddr*)&_remoteAddr, _addrSize, NULL, 0, NULL, (LPOVERLAPPED)notifyHandle);
    if (ok) {
        unrecoverableError({
            Error::ErrorCode::CoreBug,
            "Originated from",
            "connectEx",
            "_clientFD",
            _clientFD,
        });
    }

    const int myErrno = GetErrorCode();
    if (myErrno == ERROR_IO_PENDING) {
        return false;
    }

    unrecoverableError({
        Error::ErrorCode::CoreBug,
        "Originated from",
        "connectEx",
        "Errno is",
        myErrno,
        "_clientFD",
        _clientFD,
    });
}

bool RawStreamClientHandle::needRetry()
{
    const int iResult = setsockopt(_clientFD, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
    return iResult == -1;
}

void RawStreamClientHandle::destroy()
{
    if (_clientFD) {
        CancelIoEx((HANDLE)_clientFD, nullptr);
    }
    if (_clientFD) {
        CloseAndZeroSocket(_clientFD);
    }
}

void RawStreamClientHandle::zeroNativeHandle() noexcept
{
    _clientFD = 0;
}

RawStreamClientHandle::~RawStreamClientHandle()
{
    destroy();
}

}  // namespace ymq
}  // namespace scaler
#endif
