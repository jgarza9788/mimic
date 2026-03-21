#pragma once

#include <filesystem>
#include <map>
#include <string>

namespace mimic {

/**
 * @brief User-adjustable keybinding representation.
 */
struct Keybinding {
    std::string modifiers;
    std::string key;
    std::string command;
};

/**
 * @brief Runtime configuration loaded from layered TOML files.
 */
struct Config {
    int gaps {8};
    int border_width {2};
    bool dim_inactive {true};
    bool focus_follows_mouse {false};
    bool bar_wheel_scroll {true};
    std::string ipc_socket_path {"/tmp/mimic.sock"};
    std::map<std::string, Keybinding> keybindings;
};

/**
 * @brief Configuration loader for system defaults and user overrides.
 */
class ConfigLoader {
public:
    /**
     * @brief Loads config by layering default path first, then user path.
     */
    Config load_layered(const std::filesystem::path& system_path,
                        const std::filesystem::path& user_path) const;

    /**
     * @brief Loads one file if it exists and applies values into target config.
     */
    void apply_file(const std::filesystem::path& path, Config& target) const;

private:
    /**
     * @brief Parses one logical line from a narrow subset of TOML.
     */
    void parse_line(const std::string& line, std::string& section, Config& target) const;
};

} // namespace mimic
