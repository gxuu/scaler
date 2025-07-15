

// C++
#include <stdio.h>
#include <unistd.h>

#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "scaler/io/ymq/io_context.h"
#include "scaler/io/ymq/io_socket.h"
#include "scaler/io/ymq/typedefs.h"

int main() {
    IOContext context;

    auto createSocketPromise = std::promise<std::shared_ptr<IOSocket>>();
    auto createSocketFuture  = createSocketPromise.get_future();
    context.createIOSocket("ServerSocket", IOSocketType::Connector, [&createSocketPromise](auto sock) {
        createSocketPromise.set_value(sock);
    });

    auto clientSocket = createSocketFuture.get();
    printf("Successfully created socket.\n");

    auto connect_promise = std::make_shared<std::promise<void>>();
    auto connect_future  = connect_promise->get_future();

    clientSocket->connectTo("tcp://127.0.0.1:8080", [connect_promise](int result) { connect_promise->set_value(); });

    printf("Waiting for connection...\n");
    connect_future.wait();
    printf("Connected to server.\n");

    context.removeIOSocket(clientSocket);

    return 0;
}
