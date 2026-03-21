#include "mimic/ipc_server.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <array>
#include <cstring>

namespace mimic {

IpcServer::IpcServer(std::string socket_path) : socket_path_(std::move(socket_path)) {}

bool IpcServer::start(Handler handler) {
    handler_ = std::move(handler);
    ::unlink(socket_path_.c_str());

    server_fd_ = ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (server_fd_ < 0) {
        return false;
    }

    sockaddr_un address {};
    address.sun_family = AF_UNIX;
    std::strncpy(address.sun_path, socket_path_.c_str(), sizeof(address.sun_path) - 1);

    if (::bind(server_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        stop();
        return false;
    }
    if (::listen(server_fd_, 8) < 0) {
        stop();
        return false;
    }
    return true;
}

void IpcServer::poll_once() {
    if (server_fd_ < 0 || !handler_) {
        return;
    }

    const int client = ::accept4(server_fd_, nullptr, nullptr, SOCK_NONBLOCK);
    if (client < 0) {
        return;
    }

    std::array<char, 1024> buffer {};
    const ssize_t read_bytes = ::read(client, buffer.data(), buffer.size() - 1);
    if (read_bytes > 0) {
        std::string command(buffer.data(), static_cast<size_t>(read_bytes));
        std::string response = handler_(command);
        ::write(client, response.data(), response.size());
    }
    ::close(client);
}

void IpcServer::stop() {
    if (server_fd_ >= 0) {
        ::close(server_fd_);
        server_fd_ = -1;
    }
    ::unlink(socket_path_.c_str());
}

std::optional<std::string> send_ipc_command(const std::string& socket_path, const std::string& command) {
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return std::nullopt;
    }

    sockaddr_un address {};
    address.sun_family = AF_UNIX;
    std::strncpy(address.sun_path, socket_path.c_str(), sizeof(address.sun_path) - 1);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        ::close(fd);
        return std::nullopt;
    }

    ::write(fd, command.data(), command.size());
    std::array<char, 2048> buffer {};
    const auto received = ::read(fd, buffer.data(), buffer.size() - 1);
    ::close(fd);
    if (received < 0) {
        return std::nullopt;
    }

    return std::string(buffer.data(), static_cast<size_t>(received));
}

} // namespace mimic
