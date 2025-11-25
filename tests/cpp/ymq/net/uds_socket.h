#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "address.h"
#include "i_socket.h"

class UdsSocket: public ISocket {
public:
    UdsSocket();
    UdsSocket(long long fd);
    ~UdsSocket();

    // move-only
    UdsSocket(UdsSocket&&) noexcept;
    UdsSocket& operator=(UdsSocket&&) noexcept;
    UdsSocket(const UdsSocket&)            = delete;
    UdsSocket& operator=(const UdsSocket&) = delete;

    void try_connect(const std::string& address, int tries = 10) const override;
    void bind(const std::string& address) const override;
    void listen(int backlog = 5) const override;
    std::unique_ptr<ISocket> accept() const override;

    void write_all(const void* data, size_t size) const override;
    void write_all(std::string msg) const override;

    void read_exact(void* buffer, size_t size) const override;

    void write_message(std::string msg) const override;

    std::string read_message() const override;

private:
    long long _fd;

    // write up to `size` bytes
    int write(const void* buffer, size_t size) const;

    // read up to `size` bytes
    int read(void* buffer, size_t size) const;
};
