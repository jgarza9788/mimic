#pragma once

#include <functional>
#include <optional>
#include <string>

namespace mimic {

/**
 * @brief UNIX socket IPC skeleton for `mimic msg` style commands.
 */
class IpcServer {
public:
    /**
     * @brief Callback used to process one command and return one response.
     */
    using Handler = std::function<std::string(const std::string&)>;

    /**
     * @brief Constructs IPC server bound to a filesystem socket path.
     */
    explicit IpcServer(std::string socket_path);

    /**
     * @brief Starts listening and configures command handler.
     */
    bool start(Handler handler);

    /**
     * @brief Polls server for one request without blocking.
     */
    void poll_once();

    /**
     * @brief Stops server and removes socket file.
     */
    void stop();

private:
    std::string socket_path_;
    int server_fd_ {-1};
    Handler handler_;
};

/**
 * @brief Sends one IPC command to running mimic server.
 */
std::optional<std::string> send_ipc_command(const std::string& socket_path, const std::string& command);

} // namespace mimic
