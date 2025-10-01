#include "structured_logger.hpp"

namespace regulens {

StructuredLogger& StructuredLogger::get_instance() {
    static StructuredLogger instance;
    return instance;
}

StructuredLogger::StructuredLogger()
    : initialized_(false), current_level_(LogLevel::INFO),
      log_file_path_("logs/regulens.log"), max_file_size_(10485760), max_files_(5) {}

StructuredLogger::~StructuredLogger() {
    shutdown();
}

bool StructuredLogger::initialize(const std::string& config_path, LogLevel log_level) {
    if (initialized_) {
        return true;
    }

    current_level_ = log_level;

    // Create console logger
    console_logger_ = spdlog::stdout_color_mt("console");

    // Create file logger with rotation
    try {
        file_logger_ = spdlog::rotating_logger_mt("file", log_file_path_,
                                                 max_file_size_, max_files_);
    } catch (const spdlog::spdlog_ex& ex) {
        // If file logging fails, continue with console only
        console_logger_->warn("Failed to initialize file logger: {}", ex.what());
    }

    initialized_ = true;
    return true;
}

void StructuredLogger::shutdown() {
    if (!initialized_) {
        return;
    }

    spdlog::shutdown();
    initialized_ = false;
}

void StructuredLogger::log(LogLevel level, const std::string& message,
                          const std::string& component, const std::string& function,
                          const std::unordered_map<std::string, std::string>& context) {
    if (!initialized_ || static_cast<int>(level) < static_cast<int>(current_level_)) {
        return;
    }

    LogEntry entry(level, message, component, function, context);
    log_to_spdlog(entry);
}

void StructuredLogger::set_level(LogLevel level) {
    current_level_ = level;
}

void StructuredLogger::flush() {
    if (console_logger_) console_logger_->flush();
    if (file_logger_) file_logger_->flush();
}

void StructuredLogger::add_global_context(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(context_mutex_);
    global_context_[key] = value;
}

void StructuredLogger::remove_global_context(const std::string& key) {
    std::lock_guard<std::mutex> lock(context_mutex_);
    global_context_.erase(key);
}

std::unordered_map<std::string, std::string> StructuredLogger::get_global_context() const {
    std::lock_guard<std::mutex> lock(context_mutex_);
    return global_context_;
}

void StructuredLogger::log_to_spdlog(const LogEntry& entry) {
    std::string formatted_message = format_log_entry(entry);

    auto log_func = [this, &formatted_message](auto&& logger) {
        if (!logger) return;
        switch (entry.level) {
            case LogLevel::TRACE: logger->trace(formatted_message); break;
            case LogLevel::DEBUG: logger->debug(formatted_message); break;
            case LogLevel::INFO: logger->info(formatted_message); break;
            case LogLevel::WARN: logger->warn(formatted_message); break;
            case LogLevel::ERROR: logger->error(formatted_message); break;
            case LogLevel::CRITICAL: logger->critical(formatted_message); break;
        }
    };

    log_func(console_logger_);
    log_func(file_logger_);
}

std::string StructuredLogger::format_log_entry(const LogEntry& entry) const {
    std::stringstream ss;

    // Add timestamp
    auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");

    // Add level
    ss << " [" << log_level_to_string(entry.level) << "]";

    // Add component and function
    if (!entry.component.empty()) {
        ss << " [" << entry.component << "]";
    }
    if (!entry.function.empty()) {
        ss << " " << entry.function;
    }

    // Add message
    ss << " " << entry.message;

    // Add context
    std::unordered_map<std::string, std::string> full_context = get_global_context();
    full_context.insert(entry.context.begin(), entry.context.end());

    if (!full_context.empty()) {
        ss << " {";
        bool first = true;
        for (const auto& [key, value] : full_context) {
            if (!first) ss << ", ";
            ss << key << "=" << value;
            first = false;
        }
        ss << "}";
    }

    return ss.str();
}

std::string StructuredLogger::log_level_to_string(LogLevel level) const {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARN: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

} // namespace regulens


