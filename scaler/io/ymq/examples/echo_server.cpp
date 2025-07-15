

#include <stdio.h>
#include <unistd.h>

#include <future>
#include <memory>

#include "scaler/io/ymq/io_context.h"
#include "scaler/io/ymq/io_socket.h"

int main() {
    IOContext context;

    auto createSocketPromise = std::promise<std::shared_ptr<IOSocket>>();
    auto createSocketFuture  = createSocketPromise.get_future();
    context.createIOSocket("ServerSocket", IOSocketType::Binder, [&createSocketPromise](auto sock) {
        createSocketPromise.set_value(sock);
    });
    auto socket = createSocketFuture.get();
    printf("Successfully created socket.\n");

    auto bind_promise = std::promise<void>();
    auto bind_future  = bind_promise.get_future();
    // Optionally handle result in the callback
    socket->bindTo("tcp://127.0.0.1:8080", [&bind_promise](int result) { bind_promise.set_value(); });
    bind_future.wait();
    printf("Successfully bound socket\n");

    while (true) {
        printf("Try to recv a message\n");

        auto recv_promise = std::promise<Message>();
        auto recv_future  = recv_promise.get_future();

        socket->recvMessage([socket, &recv_promise](Message msg) { recv_promise.set_value(std::move(msg)); });

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
