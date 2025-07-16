

// C++
#include <stdio.h>
#include <unistd.h>

#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "./common.h"
#include "scaler/io/ymq/io_context.h"
#include "scaler/io/ymq/io_socket.h"
#include "scaler/io/ymq/typedefs.h"

using namespace scaler::ymq;

int main() {
    IOContext context;

    auto clientSocket = syncCreateSocket(context, IOSocketType::Connector, "ClientSocket");
    printf("Successfully created socket.\n");

    syncConnectSocket(clientSocket, "tcp://127.0.0.1:8080");
    printf("Connected to server.\n");

    for (int cnt = 0; cnt < 10; ++cnt) {
        std::string line = std::to_string(cnt) + "message send to remote";

        Message message;
        std::string destAddress = "ServerSocket";

        message.address = Bytes {const_cast<char*>(destAddress.c_str()), destAddress.size()};

        message.payload = Bytes {const_cast<char*>(line.c_str()), line.size()};

        auto send_promise = std::promise<void>();
        auto send_future  = send_promise.get_future();

        clientSocket->sendMessage(std::move(message), [&send_promise](int) { send_promise.set_value(); });

        send_future.wait();

        auto recv_promise = std::promise<Message>();
        auto recv_future  = recv_promise.get_future();

        clientSocket->recvMessage([&recv_promise](Message msg) { recv_promise.set_value(std::move(msg)); });

        Message reply = recv_future.get();
        std::string reply_str(reply.payload.data(), reply.payload.data() + reply.payload.len());
        if (reply_str != line) {
            printf("Soemthing goes wrong\n");
            exit(1);
        }
    }
    printf("Send and recv 10 messages, checksum fits, exiting.\n");

    // TODO: remove IOSocket also needs a future
    context.removeIOSocket(clientSocket);

    return 0;
}
