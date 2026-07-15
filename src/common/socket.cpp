#include "socket.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <stdio.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
    #define MM_INVALID_SOCKET INVALID_SOCKET
    static bool ws_initialized = false;
    static void ws_init() {
        if (!ws_initialized) {
            WSADATA wsa;
            WSAStartup(MAKEWORD(2, 2), &wsa);
            ws_initialized = true;
        }
    }
#else
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <sys/select.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <unistd.h>
    #include <errno.h>
    #include <fcntl.h>
    #define MM_INVALID_SOCKET (-1)
    // On POSIX, close() is used; TcpSocket::close handles the mapping.
    static int closesocket_posix(int fd) { return ::close(fd); }
    #define platform_close closesocket_posix
#endif

#include <cstring>

#include "log.h"
#include "util.h"

namespace mm::common {

TcpSocket::TcpSocket() : fd_(MM_INVALID_SOCKET), last_err_(0) {
#ifdef _WIN32
    ws_init();
#endif
}

TcpSocket::~TcpSocket() { close(); }

int TcpSocket::connect(std::string_view host, std::string_view port) {
    last_err_ = 0;
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    std::string h(host), p(port);
    addrinfo* res = nullptr;
    if (::getaddrinfo(h.c_str(), p.c_str(), &hints, &res) != 0) {
#ifdef _WIN32
        last_err_ = WSAGetLastError();
#else
        last_err_ = errno;
#endif
        return -1;
    }

    fd_ = MM_INVALID_SOCKET;
    for (addrinfo* ai = res; ai != nullptr; ai = ai->ai_next) {
        int s = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (s < 0) continue;
        if (::connect(s, ai->ai_addr, static_cast<socklen_t>(ai->ai_addrlen)) == 0) {
            fd_ = s;
            break;
        }
#ifdef _WIN32
        ::closesocket(s);
#else
        ::close(s);
#endif
    }
    ::freeaddrinfo(res);

    if (fd_ < 0) {
#ifdef _WIN32
        last_err_ = WSAGetLastError();
#else
        last_err_ = errno;
#endif
        return -1;
    }
    return 0;
}

int TcpSocket::send(std::string_view data) {
    last_err_ = 0;
    if (fd_ < 0) return -1;
    const char* p = data.data();
    size_t remaining = data.size();
    while (remaining > 0) {
#ifdef _WIN32
        int n = ::send(fd_, p, static_cast<int>(remaining), 0);
#else
        ssize_t n = ::send(fd_, p, remaining, 0);
#endif
        if (n <= 0) {
#ifdef _WIN32
            last_err_ = WSAGetLastError();
#else
            last_err_ = errno;
#endif
            return -1;
        }
        p += n;
        remaining -= static_cast<size_t>(n);
    }
    return static_cast<int>(data.size() - remaining);
}

int TcpSocket::recv(void* buf, size_t max) {
    last_err_ = 0;
    if (fd_ < 0) return -1;
#ifdef _WIN32
    int n = ::recv(fd_, static_cast<char*>(buf), static_cast<int>(max), 0);
#else
    ssize_t n = ::recv(fd_, buf, max, 0);
#endif
    if (n < 0) {
#ifdef _WIN32
        last_err_ = WSAGetLastError();
#else
        last_err_ = errno;
#endif
    }
    return static_cast<int>(n);
}

bool TcpSocket::recv_until(std::string& out, char delim, uint32_t timeout_ms) {
    last_err_ = 0;
    if (fd_ < 0) return false;

#ifndef _WIN32
    int flags = ::fcntl(fd_, F_GETFL, 0);
    ::fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
#endif

    char ch;
    uint64_t start = steady_ms();
    while (true) {
        int n = recv(&ch, 1);
        if (n > 0) {
            out.push_back(ch);
            if (ch == delim) return true;
            continue;
        }
        if (n == 0) { // orderly close
            return false;
        }
        // would-block / error: wait a bit
#ifdef _WIN32
        if (last_err_ != WSAEWOULDBLOCK) return false;
#else
        if (last_err_ != EAGAIN && last_err_ != EWOULDBLOCK) return false;
#endif
        if (steady_ms() - start > timeout_ms) break;
#ifdef _WIN32
        ::Sleep(5);
#else
        struct timeval tv {0, 5000};
        ::select(fd_ + 1, nullptr, nullptr, nullptr, &tv);
#endif
    }
#ifndef _WIN32
    ::fcntl(fd_, F_SETFL, flags);
#endif
    return false;
}

void TcpSocket::close() {
    if (fd_ >= 0) {
#ifdef _WIN32
        ::closesocket(fd_);
#else
        ::close(fd_);
#endif
        fd_ = MM_INVALID_SOCKET;
    }
}

} // namespace mm::common
