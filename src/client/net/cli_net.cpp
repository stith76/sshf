#include "cli_net.hpp"

#include <chrono>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string_view>

namespace sshf::client::net {

auto get_host_ping(const std::string& host, int port) -> std::optional<double> {
    addrinfo hints{.ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM}, *res = nullptr;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0) {
        return std::nullopt;
    }

    int sock = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) return (freeaddrinfo(res), std::nullopt);

    timeval tv{.tv_sec = 2, .tv_usec = 0};
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    auto start = std::chrono::high_resolution_clock::now();
    int status = ::connect(sock, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    if (status != 0) {
        ::close(sock);
        return std::nullopt;
    }

    char buffer[4] = {0};
    ssize_t bytes_read = ::recv(sock, buffer, sizeof(buffer), 0);
    ::close(sock);

    if (bytes_read >= 4 && std::string_view(buffer, 4) == "SSH-") {
        std::chrono::duration<double, std::milli> elapsed = std::chrono::high_resolution_clock::now() - start;
        return elapsed.count();
    }

    return std::nullopt;
}

}