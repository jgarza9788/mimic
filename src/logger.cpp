#include "mimic/logger.hpp"

namespace mimic {

Logger::Logger(LogLevel level) : level_(level) {}

void Logger::set_level(LogLevel level) {
    std::scoped_lock lock(mutex_);
    level_ = level;
}

LogLevel Logger::level() const {
    std::scoped_lock lock(mutex_);
    return level_;
}

void Logger::log(LogLevel level, const std::string& message) {
    std::scoped_lock lock(mutex_);
    if (static_cast<int>(level) < static_cast<int>(level_)) {
        return;
    }
    std::cerr << "[" << level_name(level) << "] " << message << '\n';
}

const char* Logger::level_name(LogLevel level) {
    switch (level) {
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warn:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    }
    return "UNKNOWN";
}

} // namespace mimic
