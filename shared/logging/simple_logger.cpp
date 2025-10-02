#include "structured_logger.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>

namespace regulens {

// Simple logger implementation using std::cout
class SimpleLogger : public Logger {
public:
    void info(const std::string& message) override {
        log("INFO", message);
    }

    void warning(const std::string& message) override {
        log("WARN", message);
    }

    void error(const std::string& message) override {
        log("ERROR", message);
    }

private:
    void log(const std::string& level, const std::string& message) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);

        std::cout << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "] "
                  << "[" << level << "] " << message << std::endl;
    }
};

StructuredLogger& StructuredLogger::get_instance() {
    static StructuredLogger instance;
    return instance;
}

StructuredLogger::StructuredLogger()
    : initialized_(false), current_level_(LogLevel::INFO), logger_(std::make_shared<SimpleLogger>()) {}

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
                          const std::unordered_map<std::string, std::string>& metadata) {
    if (!initialized_ || level < current_level_) {
        return;
    }

    std::string full_message = message;
    if (!metadata.empty()) {
        full_message += " |";
        for (const auto& [key, value] : metadata) {
            full_message += " " + key + "=" + value;
        }
    }

    switch (level) {
        case LogLevel::INFO:
            logger_->info(full_message);
            break;
        case LogLevel::WARNING:
            logger_->warning(full_message);
            break;
        case LogLevel::ERROR:
            logger_->error(full_message);
            break;
        default:
            logger_->info(full_message);
            break;
    }
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

