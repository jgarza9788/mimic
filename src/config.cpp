#include "mimic/config.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace mimic {

namespace {

/**
 * @brief Trims ASCII whitespace from both sides of a string.
 */
std::string trim(std::string value) {
    auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](char ch) { return !is_space(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](char ch) { return !is_space(ch); }).base(), value.end());
    return value;
}

/**
 * @brief Removes surrounding quotes when present.
 */
std::string unquote(const std::string& value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

} // namespace

Config ConfigLoader::load_layered(const std::filesystem::path& system_path,
                                  const std::filesystem::path& user_path) const {
    Config config;
    apply_file(system_path, config);
    apply_file(user_path, config);
    return config;
}

void ConfigLoader::apply_file(const std::filesystem::path& path, Config& target) const {
    if (!std::filesystem::exists(path)) {
        return;
    }

    std::ifstream stream(path);
    std::string section;
    std::string line;
    while (std::getline(stream, line)) {
        parse_line(line, section, target);
    }
}

void ConfigLoader::parse_line(const std::string& line, std::string& section, Config& target) const {
    auto cleaned = trim(line);
    if (cleaned.empty() || cleaned[0] == '#') {
        return;
    }
    if (cleaned.front() == '[' && cleaned.back() == ']') {
        section = cleaned.substr(1, cleaned.size() - 2);
        return;
    }

    const auto equals = cleaned.find('=');
    if (equals == std::string::npos) {
        return;
    }
    auto key = trim(cleaned.substr(0, equals));
    auto value = trim(cleaned.substr(equals + 1));

    if (section.empty()) {
        if (key == "gaps") {
            target.gaps = std::stoi(value);
        } else if (key == "border_width") {
            target.border_width = std::stoi(value);
        } else if (key == "dim_inactive") {
            target.dim_inactive = value == "true";
        } else if (key == "focus_follows_mouse") {
            target.focus_follows_mouse = value == "true";
        } else if (key == "bar_wheel_scroll") {
            target.bar_wheel_scroll = value == "true";
        } else if (key == "ipc_socket_path") {
            target.ipc_socket_path = unquote(value);
        }
        return;
    }

    if (section.rfind("keybindings.", 0) == 0) {
        const auto action = section.substr(std::string("keybindings.").size());
        auto& binding = target.keybindings[action];
        if (key == "modifiers") {
            binding.modifiers = unquote(value);
        } else if (key == "key") {
            binding.key = unquote(value);
        } else if (key == "command") {
            binding.command = unquote(value);
        }
    }
}

} // namespace mimic
