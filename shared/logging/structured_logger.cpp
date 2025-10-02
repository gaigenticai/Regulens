#include "structured_logger.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>

namespace regulens {

StructuredLogger& StructuredLogger::get_instance() {
    static StructuredLogger instance;
    return instance;
}

StructuredLogger::StructuredLogger()
    : initialized_(false), current_level_(LogLevel::INFO) {}

StructuredLogger::~StructuredLogger() {
    shutdown();
}

bool StructuredLogger::initialize(const std::string& /*config_path*/, LogLevel log_level) {
    current_level_ = log_level;
    initialized_ = true;
    return true;
}

void StructuredLogger::shutdown() {
    initialized_ = false;
}

void StructuredLogger::log(LogLevel level, const std::string& message,
                          const std::string& component,
                          const std::string& function,
                          const std::unordered_map<std::string, std::string>& /*context*/) {
    if (!initialized_ || level < current_level_) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::string level_str;
    switch (level) {
        case LogLevel::TRACE: level_str = "TRACE"; break;
        case LogLevel::DEBUG: level_str = "DEBUG"; break;
        case LogLevel::INFO: level_str = "INFO"; break;
        case LogLevel::WARN: level_str = "WARN"; break;
        case LogLevel::ERROR: level_str = "ERROR"; break;
        case LogLevel::CRITICAL: level_str = "CRIT"; break;
        default: level_str = "INFO"; break;
    }

    std::string full_message = message;
    if (!component.empty()) {
        full_message = "[" + component + "] " + full_message;
    }
    if (!function.empty()) {
        full_message = "[" + function + "] " + full_message;
    }

    std::cout << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "] "
              << "[" << level_str << "] " << full_message << std::endl;
}

// Method implementations are provided by the header's default parameters

} // namespace regulens
