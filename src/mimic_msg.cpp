#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "mimic/config.hpp"
#include "mimic/ipc_server.hpp"

/**
 * @brief Entry point for command client that talks to running mimic instance.
 */
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: mimic-msg <command>\n";
        return 1;
    }

    mimic::ConfigLoader loader;
    auto config = loader.load_layered("/usr/share/mimic/config/default.toml",
                                      std::filesystem::path(getenv("HOME")) / ".config/mimic/config.toml");

    std::string command;
    for (int i = 1; i < argc; ++i) {
        if (!command.empty()) {
            command += ' ';
        }
        command += argv[i];
    }

    auto response = mimic::send_ipc_command(config.ipc_socket_path, command);
    if (!response.has_value()) {
        std::cerr << "failed to send command\n";
        return 1;
    }

    std::cout << response.value();
    return 0;
}
