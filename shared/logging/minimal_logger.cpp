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
                          const std::unordered_map<std::string, std::string>& /*metadata*/) {
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
        case LogLevel::WARNING: level_str = "WARN"; break;
        case LogLevel::ERROR: level_str = "ERROR"; break;
        case LogLevel::CRITICAL: level_str = "CRIT"; break;
        default: level_str = "INFO"; break;
    }

    std::cout << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "] "
              << "[" << level_str << "] " << message << std::endl;
}

void StructuredLogger::trace(const std::string& message,
                            const std::unordered_map<std::string, std::string>& metadata) {
    log(LogLevel::TRACE, message, metadata);
}

void StructuredLogger::debug(const std::string& message,
                            const std::unordered_map<std::string, std::string>& metadata) {
    log(LogLevel::DEBUG, message, metadata);
}

void StructuredLogger::info(const std::string& message,
                           const std::unordered_map<std::string, std::string>& metadata) {
    log(LogLevel::INFO, message, metadata);
}

void StructuredLogger::warning(const std::string& message,
                              const std::unordered_map<std::string, std::string>& metadata) {
    log(LogLevel::WARNING, message, metadata);
}

void StructuredLogger::error(const std::string& message,
                            const std::unordered_map<std::string, std::string>& metadata) {
    log(LogLevel::ERROR, message, metadata);
}

void StructuredLogger::critical(const std::string& message,
                               const std::unordered_map<std::string, std::string>& metadata) {
    log(LogLevel::CRITICAL, message, metadata);
}

} // namespace regulens

