#pragma once

#include <iostream>
#include <mutex>
#include <string>

namespace mimic {

/**
 * @brief Severity levels for structured logs.
 */
enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error,
};

/**
 * @brief Thread-safe console logger used across mimic subsystems.
 */
class Logger {
public:
    /**
     * @brief Constructs a logger with an initial level.
     */
    explicit Logger(LogLevel level = LogLevel::Info);

    /**
     * @brief Sets the minimum log level for output.
     */
    void set_level(LogLevel level);

    /**
     * @brief Gets the current minimum log level.
     */
    [[nodiscard]] LogLevel level() const;

    /**
     * @brief Writes one log line if the message level passes filtering.
     */
    void log(LogLevel level, const std::string& message);

private:
    /**
     * @brief Converts a level enum to a fixed label string.
     */
    [[nodiscard]] static const char* level_name(LogLevel level);

    LogLevel level_;
    mutable std::mutex mutex_;
};

} // namespace mimic
