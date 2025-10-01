#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <nlohmann/json.hpp>

namespace regulens {

/**
 * @brief Log levels for structured logging
 */
enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    CRITICAL
};

/**
 * @brief Structured log entry with context
 */
struct LogEntry {
    LogLevel level;
    std::string message;
    std::string component;
    std::string function;
    std::unordered_map<std::string, std::string> context;
    std::chrono::system_clock::time_point timestamp;

    LogEntry(LogLevel lvl, std::string msg, std::string comp = "",
            std::string func = "", std::unordered_map<std::string, std::string> ctx = {})
        : level(lvl), message(std::move(msg)), component(std::move(comp)),
          function(std::move(func)), context(std::move(ctx)),
          timestamp(std::chrono::system_clock::now()) {}

    nlohmann::json to_json() const {
        nlohmann::json context_json;
        for (const auto& [key, value] : context) {
            context_json[key] = value;
        }

        return {
            {"level", static_cast<int>(level)},
            {"message", message},
            {"component", component},
            {"function", function},
            {"context", context_json},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp.time_since_epoch()).count()}
        };
    }
};

/**
 * @brief Structured logging system for enterprise applications
 *
 * Provides consistent, structured logging across all components
 * with support for JSON output, log rotation, and contextual information.
 */
class StructuredLogger {
public:
    /**
     * @brief Get singleton logger instance
     */
    static StructuredLogger& get_instance();

    /**
     * @brief Initialize the logger with configuration
     * @param config_path Path to configuration file (optional)
     * @param log_level Default log level
     * @return true if initialization successful
     */
    bool initialize(const std::string& config_path = "",
                   LogLevel log_level = LogLevel::INFO);

    /**
     * @brief Shutdown the logger and flush all logs
     */
    void shutdown();

    /**
     * @brief Log a message with context
     * @param level Log level
     * @param message Log message
     * @param component Component name (optional)
     * @param function Function name (optional)
     * @param context Additional context key-value pairs
     */
    void log(LogLevel level, const std::string& message,
            const std::string& component = "",
            const std::string& function = "",
            const std::unordered_map<std::string, std::string>& context = {});

    /**
     * @brief Log trace level message
     */
    void trace(const std::string& message,
              const std::string& component = "",
              const std::string& function = "",
              const std::unordered_map<std::string, std::string>& context = {}) {
        log(LogLevel::TRACE, message, component, function, context);
    }

    /**
     * @brief Log debug level message
     */
    void debug(const std::string& message,
              const std::string& component = "",
              const std::string& function = "",
              const std::unordered_map<std::string, std::string>& context = {}) {
        log(LogLevel::DEBUG, message, component, function, context);
    }

    /**
     * @brief Log info level message
     */
    void info(const std::string& message,
             const std::string& component = "",
             const std::string& function = "",
             const std::unordered_map<std::string, std::string>& context = {}) {
        log(LogLevel::INFO, message, component, function, context);
    }

    /**
     * @brief Log warning level message
     */
    void warn(const std::string& message,
             const std::string& component = "",
             const std::string& function = "",
             const std::unordered_map<std::string, std::string>& context = {}) {
        log(LogLevel::WARN, message, component, function, context);
    }

    /**
     * @brief Log error level message
     */
    void error(const std::string& message,
              const std::string& component = "",
              const std::string& function = "",
              const std::unordered_map<std::string, std::string>& context = {}) {
        log(LogLevel::ERROR, message, component, function, context);
    }

    /**
     * @brief Log critical level message
     */
    void critical(const std::string& message,
                 const std::string& component = "",
                 const std::string& function = "",
                 const std::unordered_map<std::string, std::string>& context = {}) {
        log(LogLevel::CRITICAL, message, component, function, context);
    }

    /**
     * @brief Set log level
     * @param level New log level
     */
    void set_level(LogLevel level);

    /**
     * @brief Flush all pending log messages
     */
    void flush();

    /**
     * @brief Add context that will be included in all subsequent logs
     * @param key Context key
     * @param value Context value
     */
    void add_global_context(const std::string& key, const std::string& value);

    /**
     * @brief Remove global context
     * @param key Context key to remove
     */
    void remove_global_context(const std::string& key);

    /**
     * @brief Get current global context
     * @return Current global context
     */
    std::unordered_map<std::string, std::string> get_global_context() const;

private:
    StructuredLogger();
    ~StructuredLogger();

    // Prevent copying
    StructuredLogger(const StructuredLogger&) = delete;
    StructuredLogger& operator=(const StructuredLogger&) = delete;

    // Internal logging methods
    void log_to_spdlog(const LogEntry& entry);
    std::string format_log_entry(const LogEntry& entry) const;
    std::string log_level_to_string(LogLevel level) const;

    // Configuration
    bool initialized_;
    LogLevel current_level_;
    std::string log_file_path_;
    size_t max_file_size_;
    size_t max_files_;

    // Logger instances
    std::shared_ptr<spdlog::logger> console_logger_;
    std::shared_ptr<spdlog::logger> file_logger_;

    // Global context
    mutable std::mutex context_mutex_;
    std::unordered_map<std::string, std::string> global_context_;
};

/**
 * @brief RAII helper for adding temporary context to logs
 */
class LogContext {
public:
    LogContext(const std::string& key, const std::string& value)
        : key_(key), value_(value) {
        StructuredLogger::get_instance().add_global_context(key_, value_);
    }

    ~LogContext() {
        StructuredLogger::get_instance().remove_global_context(key_);
    }

private:
    std::string key_;
    std::string value_;
};

/**
 * @brief Performance logging helper
 */
class PerformanceLogger {
public:
    PerformanceLogger(const std::string& operation_name,
                     const std::string& component = "")
        : operation_name_(operation_name), component_(component),
          start_time_(std::chrono::high_resolution_clock::now()) {}

    ~PerformanceLogger() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time_);

        StructuredLogger::get_instance().info(
            "Operation completed",
            component_,
            operation_name_,
            {{"duration_ms", std::to_string(duration.count())}}
        );
    }

private:
    std::string operation_name_;
    std::string component_;
    std::chrono::high_resolution_clock::time_point start_time_;
};

} // namespace regulens
