#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace mm::common {

// Minimal cross-platform blocking TCP socket. Enough for a Stratum V1 client:
// connect, send, recv-until-delimiter or with timeout.
class TcpSocket {
public:
    TcpSocket();
    ~TcpSocket();

    TcpSocket(const TcpSocket&) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;

    // Returns 0 on success, negative on error (errno on POSIX, WSAGetLastError on Win).
    int connect(std::string_view host, std::string_view port);

    // Returns bytes sent, or -1 on error.
    int send(std::string_view data);

    // Receive up to `max` bytes. Returns bytes read (0 = orderly close, -1 = error).
    int recv(void* buf, size_t max);

    // Read until `delim` is seen (appended to `out`), or until `timeout_ms` elapses.
    // Returns true if delim was found, false on timeout/error/close.
    bool recv_until(std::string& out, char delim, uint32_t timeout_ms);

    void close();
    bool is_open() const { return fd_ >= 0; }

    int last_error() const { return last_err_; }

private:
    int fd_;
    int last_err_;
};

} // namespace mm::common
