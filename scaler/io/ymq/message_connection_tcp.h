#pragma once

#include <memory>
#include <optional>
#include <queue>
#include <vector>

#include "scaler/io/ymq/configuration.h"
#include "scaler/io/ymq/io_socket.h"
#include "scaler/io/ymq/message_connection.h"

class EventLoopThread;
class EventManager;

struct TcpReadOperation {
    using RecvMessageCallback = Configuration::RecvMessageCallback;
    std::shared_ptr<std::vector<char>> _buf;
    size_t _cursor = 0;
    RecvMessageCallback _callbackAfterCompleteRead;
};

struct TcpWriteOperation {
    using SendMessageCallback = Configuration::SendMessageCallback;
    uint64_t _header;
    Bytes _payload;
    SendMessageCallback _callbackAfterCompleteWrite;

    TcpWriteOperation(Message msg, SendMessageCallback callbackAfterCompleteWrite) {
        _header                     = msg.payload.len();
        _payload                    = std::move(msg.payload);
        _callbackAfterCompleteWrite = std::move(callbackAfterCompleteWrite);
    }

    TcpWriteOperation(Bytes payload, SendMessageCallback callbackAfterCompleteWrite) {
        _header                     = payload.len();
        _payload                    = std::move(payload);
        _callbackAfterCompleteWrite = std::move(callbackAfterCompleteWrite);
    }
};

// So for read operation, you want your message to be just payload, with the size filled
// for write operation, you are giving me a message, I want to parse it to payload and send
// just payload (no address, but with length)

class MessageConnectionTCP: public MessageConnection {
    int _connFd;
    sockaddr _localAddr;
    std::string _localIOSocketIdentity;
    bool _sendLocalIdentity;
    std::unique_ptr<EventManager> _eventManager;

    std::vector<TcpWriteOperation> _writeOperations;
    size_t _inclassCursor;

    std::shared_ptr<std::queue<TcpReadOperation>> _pendingReadOperations;
    std::queue<std::vector<char>> _receivedMessages;

    void onRead();
    void onWrite();
    void onClose();
    void onError() {
        printf("onError (for debug don't remove)\n");
        exit(1);
    };

    std::shared_ptr<EventLoopThread> _eventLoopThread;

    std::expected<size_t, int> trySendQueuedMessages();
    void updateWriteOperations(size_t n);

public:
    using SendMessageCallback = Configuration::SendMessageCallback;
    using RecvMessageCallback = Configuration::RecvMessageCallback;

    const sockaddr _remoteAddr;
    std::optional<std::string> _remoteIOSocketIdentity;
    const bool _responsibleForRetry;

    MessageConnectionTCP(
        std::shared_ptr<EventLoopThread> eventLoopThread,
        int connFd,
        sockaddr localAddr,
        sockaddr remoteAddr,
        std::string localIOSocketIdentity,
        bool responsibleForRetry,
        std::shared_ptr<std::queue<TcpReadOperation>> _pendingReadOperations,
        std::optional<std::string> remoteIOSocketIdentity = std::nullopt);

    void sendMessage(Message msg, SendMessageCallback callback);
    bool recvMessage();

    void onCreated();

    ~MessageConnectionTCP();

    friend void IOSocket::onConnectionIdentityReceived(MessageConnectionTCP* conn);
};
