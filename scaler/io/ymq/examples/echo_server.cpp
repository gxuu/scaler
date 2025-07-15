#include <stdio.h>
#include <unistd.h>

#include <future>
#include <memory>

#include "scaler/io/ymq/io_context.h"
#include "scaler/io/ymq/io_socket.h"

// For test directory, okay to not use project path
#include "./common.h"

using namespace scaler::ymq;

int main() {
    IOContext context;

    auto socket = syncCreateSocket(context, IOSocketType::Binder, "ServerSocket");
    printf("Successfully created socket.\n");

    syncBindSocket(socket, "tcp://127.0.0.1:8080");
    printf("Successfully bound socket\n");

    while (true) {
        printf("Try to recv a message\n");

        auto recv_promise = std::promise<Message>();
        auto recv_future  = recv_promise.get_future();

        socket->recvMessage([&recv_promise](Message msg) { recv_promise.set_value(std::move(msg)); });

        Message received_msg = recv_future.get();
        printf(
            "Receiving message from '%s', message content is: '%s'\n",
            received_msg.address.as_string().c_str(),
            std::string(received_msg.payload.data(), received_msg.payload.data() + received_msg.payload.len()).c_str());

        auto send_promise = std::promise<void>();
        auto send_future  = send_promise.get_future();
        socket->sendMessage(std::move(received_msg), [&send_promise](int) { send_promise.set_value(); });
        send_future.wait();

        printf("Message echoed back. Looping...\n");
    }

    return 0;
}
