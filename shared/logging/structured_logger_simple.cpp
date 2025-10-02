#include "structured_logger.hpp"

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
    if (initialized_) {
        return true;
    }

    current_level_ = log_level;

    try {
        // Create simple console logger
        console_logger_ = spdlog::stdout_color_mt("console");
        console_logger_->set_level(spdlog_level(log_level));
        console_logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");

        initialized_ = true;
        return true;
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Logger initialization failed: " << ex.what() << std::endl;
        return false;
    }
}

void StructuredLogger::shutdown() {
    if (console_logger_) {
        spdlog::drop(console_logger_->name());
    }
    initialized_ = false;
}

spdlog::level::level_enum StructuredLogger::spdlog_level(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return spdlog::level::trace;
        case LogLevel::DEBUG: return spdlog::level::debug;
        case LogLevel::INFO: return spdlog::level::info;
        case LogLevel::WARNING: return spdlog::level::warn;
        case LogLevel::ERROR: return spdlog::level::err;
        case LogLevel::CRITICAL: return spdlog::level::critical;
        default: return spdlog::level::info;
    }
}

void StructuredLogger::log(LogLevel level, const std::string& message,
                          const std::unordered_map<std::string, std::string>& metadata) {
    if (!initialized_ || level < current_level_) {
        return;
    }

    LogEntry entry{level, message, metadata, std::chrono::system_clock::now()};
    log_to_spdlog(entry);
}

void StructuredLogger::log_to_spdlog(const LogEntry& entry) {
    if (!console_logger_) return;

    // Format message with metadata
    std::string formatted_message = entry.message;
    if (!entry.metadata.empty()) {
        formatted_message += " |";
        for (const auto& [key, value] : entry.metadata) {
            formatted_message += " " + key + "=" + value;
        }
    }

    // Log based on level
    switch (entry.level) {
        case LogLevel::TRACE:
            console_logger_->trace(formatted_message);
            break;
        case LogLevel::DEBUG:
            console_logger_->debug(formatted_message);
            break;
        case LogLevel::INFO:
            console_logger_->info(formatted_message);
            break;
        case LogLevel::WARNING:
            console_logger_->warn(formatted_message);
            break;
        case LogLevel::ERROR:
            console_logger_->error(formatted_message);
            break;
        case LogLevel::CRITICAL:
            console_logger_->critical(formatted_message);
            break;
    }
}

// Convenience logging methods
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

