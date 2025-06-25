
#include "scaler/io/ymq/message_connection_tcp.h"

#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>

#include "scaler/io/ymq/event_loop_thread.h"
#include "scaler/io/ymq/event_manager.h"
#include "scaler/io/ymq/io_socket.h"

static constexpr const size_t headerSize = sizeof(uint64_t);

static bool isCompleteMessage(const std::vector<char>& vec) {
    if (vec.size() < headerSize)
        return false;
    uint64_t size = *(uint64_t*)vec.data();
    return vec.size() == size + headerSize;
}

MessageConnectionTCP::MessageConnectionTCP(
    std::shared_ptr<EventLoopThread> eventLoopThread,
    int connFd,
    sockaddr localAddr,
    sockaddr remoteAddr,
    std::string localIOSocketIdentity,
    bool responsibleForRetry,
    std::shared_ptr<std::queue<TcpReadOperation>> pendingReadOperations,
    std::optional<std::string> remoteIOSocketIdentity)
    : _eventLoopThread(eventLoopThread)
    , _eventManager(std::make_unique<EventManager>())
    , _connFd(std::move(connFd))
    , _localAddr(std::move(localAddr))
    , _remoteAddr(std::move(remoteAddr))
    , _localIOSocketIdentity(std::move(localIOSocketIdentity))
    , _remoteIOSocketIdentity(std::move(remoteIOSocketIdentity))
    , _sendLocalIdentity(false)
    , _responsibleForRetry(responsibleForRetry)
    , _pendingReadOperations(pendingReadOperations)
    , _inclassCursor {} {
    _eventManager->onRead  = [this] { this->onRead(); };
    _eventManager->onWrite = [this] { this->onWrite(); };
    _eventManager->onClose = [this] { this->onClose(); };
    _eventManager->onError = [this] { this->onError(); };
}

void MessageConnectionTCP::onCreated() {
    if (_connFd != 0) {
        this->_eventLoopThread->_eventLoop.addFdToLoop(
            _connFd, EPOLLIN | EPOLLOUT | EPOLLET, this->_eventManager.get());
        _writeOperations.push_back({Bytes {_localIOSocketIdentity.data(), _localIOSocketIdentity.size()}, [](int) {}});
    }
}

void MessageConnectionTCP::onRead() {
    // TODO:
    // - do not assume the identity to be less than 128bytes
    // - do not make remoteIOSocketIdentity a failing point
    if (!_remoteIOSocketIdentity) {
        // Other sizes are possible, but the size needs to be >= 8, in order for idBuf
        // to be aligned with 8 bytes boundary because of strict aliasing
        uint64_t header {};
        int n = read(_connFd, &header, headerSize);
        char idBuf[128] {};
        n           = read(_connFd, idBuf, header);
        char* first = idBuf;
        std::string remoteID(first, first + header);
        _remoteIOSocketIdentity.emplace(std::move(remoteID));
        auto& sock = this->_eventLoopThread->_identityToIOSocket[_localIOSocketIdentity];
        sock->onConnectionIdentityReceived(this);
    }

    while (true) {
        const size_t headerSize = 8;
        size_t leftOver         = 0;
        size_t first            = 0;

        // Good case
        if (_receivedMessages.empty() || isCompleteMessage(_receivedMessages.back())) {
            _receivedMessages.push({});
            _receivedMessages.back().resize(1024);
            leftOver = headerSize;
            first    = 0;
        } else {
            // Bad case, we currently have an incomplete message
            auto& message = _receivedMessages.back();
            if (message.size() < headerSize) {
                leftOver = headerSize - message.size();
                first    = message.size();
                message.resize(1024);
            } else {
                size_t payloadSize = *(uint64_t*)message.data();

                if (message.size() == 1024) {
                    message.resize(std::min((payloadSize), headerSize));
                }

                leftOver = payloadSize - (message.size() - headerSize);

                first = message.size();
                message.resize(payloadSize + headerSize);
            }
        }

        auto& message = _receivedMessages.back();

        while (leftOver) {
            assert(first + leftOver <= message.size());
            int res = read(_connFd, message.data() + first, leftOver);

            if (res == 0) {
                onClose();
                return;
            }

            if (res == -1 && errno == EAGAIN) {
                perror("read");
                message.resize(first);
                // Reviewer: To jump out of double loop, reconsider when you say no. - gxu
                goto ReadExhuasted;
            }

            leftOver -= res;
            first += res;
            if (first == headerSize) {
                // reading the payload
                leftOver = *(uint64_t*)message.data();
                message.resize(leftOver + headerSize);
            }
        }
        assert(isCompleteMessage(_receivedMessages.back()));
    }

ReadExhuasted:
    printf("READ EXHAUSTED\n");
    while (_pendingReadOperations->size() && _receivedMessages.size()) {
        if (isCompleteMessage(_receivedMessages.front())) {
            *_pendingReadOperations->front()._buf = std::move(_receivedMessages.front());
            _receivedMessages.pop();

            Bytes address(_remoteIOSocketIdentity->data(), _remoteIOSocketIdentity->size());
            Bytes payload(
                _pendingReadOperations->front()._buf->data() + headerSize,
                _pendingReadOperations->front()._buf->size() - headerSize);

            _pendingReadOperations->front()._callbackAfterCompleteRead(Message(std::move(address), std::move(payload)));

            _pendingReadOperations->pop();
        } else {
            assert(_pendingReadOperations->size());
            break;
        }
    }
}

void MessageConnectionTCP::onWrite() {
    // This is because after disconnected, onRead will be called first, and that will set
    // _connFd to 0. There's no way to not call onWrite in this case. So we return early.
    if (_connFd == 0) {
        return;
    }

    auto res = trySendQueuedMessages();
    if (res) {
        updateWriteOperations(res.value());
        return;
    }

    // EPIPE: Shutdown for writing or disconnected, since we don't provide the former,
    // it means the later.
    if (res.error() == ECONNRESET || res.error() == EPIPE) {
        onClose();
        return;
    } else {
        printf("SOMETHING REALLY BAD\n");
        exit(1);
    }
}

void MessageConnectionTCP::onClose() {
    _eventLoopThread->_eventLoop.removeFdFromLoop(_connFd);
    close(_connFd);
    _connFd    = 0;
    auto& sock = _eventLoopThread->_identityToIOSocket.at(_localIOSocketIdentity);
    sock->onConnectionDisconnected(this);
};

std::expected<size_t, int> MessageConnectionTCP::trySendQueuedMessages() {
    std::vector<struct iovec> iovecs;
    iovecs.reserve(IOV_MAX);

    for (auto it = _writeOperations.begin(); it != _writeOperations.end(); ++it) {
        if (iovecs.size() >= IOV_MAX + 2) {
            break;
        }

        iovec iovHeader {};
        iovec iovPayload {};
        if (it == _writeOperations.begin()) {
            if (_inclassCursor < headerSize) {
                iovHeader.iov_base  = (char*)(&it->_header) + _inclassCursor;
                iovHeader.iov_len   = headerSize - _inclassCursor;
                iovPayload.iov_base = (void*)(it->_payload.data());
                iovPayload.iov_len  = it->_payload.len();
            } else {
                iovHeader.iov_base  = nullptr;
                iovHeader.iov_len   = 0;
                iovPayload.iov_base = (char*)(it->_payload.data()) + (_inclassCursor - headerSize);
                iovPayload.iov_len  = it->_payload.len() + (_inclassCursor - headerSize);
            }
        } else {
            iovHeader.iov_base  = (void*)(&it->_header);
            iovHeader.iov_len   = headerSize;
            iovPayload.iov_base = (void*)(it->_payload.data());
            iovPayload.iov_len  = it->_payload.len();
        }

        iovecs.push_back(iovHeader);
        iovecs.push_back(iovPayload);
    }

    if (iovecs.empty()) {
        return 0;
    }

    struct msghdr msg {};
    msg.msg_iov    = iovecs.data();
    msg.msg_iovlen = iovecs.size();

    ssize_t bytesSent = ::sendmsg(_connFd, &msg, MSG_NOSIGNAL);
    if (bytesSent == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        } else
            return std::unexpected {errno};
    }

    return bytesSent;
}

// TODO: There is a classic optimization that can (and should) be done. That is, we store
// prefix sum in each write operation, and perform binary search instead of linear search
// to find the first write operation we haven't complete. - gxu
void MessageConnectionTCP::updateWriteOperations(size_t n) {
    auto target = _writeOperations.begin();
    _inclassCursor += n;
    // Post condition of the loop: target contains the first write op we haven't complete.
    for (auto it = _writeOperations.begin(); it != _writeOperations.end(); ++it) {
        size_t msgSize = it->_payload.len() + headerSize;
        if (_inclassCursor < msgSize) {
            target = it;
            break;
        }

        if (_inclassCursor == msgSize) {
            target         = it + 1;
            _inclassCursor = 0;
            break;
        }

        _inclassCursor -= msgSize;
    }

    // NOTE: [complete, _nwriteOperations.end()) is a range of completed write operations.
    auto complete = std::rotate(_writeOperations.begin(), target, _writeOperations.end());

    for (auto tmp = complete; tmp != _writeOperations.end(); ++tmp) {
        tmp->_callbackAfterCompleteWrite(0);
    }

    _writeOperations.erase(complete, _writeOperations.end());
}

void MessageConnectionTCP::sendMessage(Message msg, std::function<void(int)> callback) {
    TcpWriteOperation writeOp(std::move(msg), std::move(callback));
    _writeOperations.push_back(std::move(writeOp));

    if (_connFd == 0) {
        return;
    }
    onWrite();
}

bool MessageConnectionTCP::recvMessage() {
    if (_receivedMessages.empty() || _pendingReadOperations->empty() || !isCompleteMessage(_receivedMessages.front())) {
        return false;
    }

    while (_pendingReadOperations->size() && _receivedMessages.size()) {
        if (isCompleteMessage(_receivedMessages.front())) {
            *_pendingReadOperations->front()._buf = std::move(_receivedMessages.front());
            _receivedMessages.pop();

            Bytes address(_remoteIOSocketIdentity->data(), _remoteIOSocketIdentity->size());
            Bytes payload(
                _pendingReadOperations->front()._buf->data() + headerSize,
                _pendingReadOperations->front()._buf->size() - headerSize);

            _pendingReadOperations->front()._callbackAfterCompleteRead(Message(std::move(address), std::move(payload)));

            _pendingReadOperations->pop();
        } else {
            assert(_pendingReadOperations->size());
            break;
        }
    }
    return true;
}

MessageConnectionTCP::~MessageConnectionTCP() {
    if (_connFd != 0) {
        _eventLoopThread->_eventLoop.removeFdFromLoop(_connFd);
        shutdown(_connFd, SHUT_RDWR);
        close(_connFd);
        _connFd = 0;
    }

    std::ranges::for_each(_writeOperations, [](const auto& x) { x._callbackAfterCompleteWrite(-1); });

    // TODO: What to do with this?
    // std::queue<std::vector<char>> _receivedMessages;
}
