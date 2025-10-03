#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <exception>
#include <optional>
#include <nlohmann/json.hpp>

namespace regulens {

/**
 * @brief Error severity levels
 */
enum class ErrorSeverity {
    LOW,        // Minor issues, operation can continue
    MEDIUM,     // Moderate issues, may affect performance
    HIGH,       // Serious issues, functionality impaired
    CRITICAL    // System-threatening issues, immediate action required
};

/**
 * @brief Error categories for classification
 */
enum class ErrorCategory {
    NETWORK,        // Network connectivity issues
    DATABASE,       // Database connection/query errors
    EXTERNAL_API,   // External service API failures
    CONFIGURATION,  // Configuration loading/parsing errors
    VALIDATION,     // Data validation failures
    PROCESSING,     // Business logic processing errors
    RESOURCE,       // Resource exhaustion (memory, CPU, disk)
    SECURITY,       // Security-related errors
    TIMEOUT,        // Operation timeouts
    UNKNOWN         // Unclassified errors
};

/**
 * @brief Recovery strategies for error handling
 */
enum class RecoveryStrategy {
    RETRY,              // Simple retry with backoff
    CIRCUIT_BREAKER,    // Circuit breaker pattern
    FALLBACK,           // Use fallback implementation
    DEGRADATION,        // Graceful degradation
    FAILOVER,           // Switch to backup system
    MANUAL,             // Requires manual intervention
    IGNORE              // Safe to ignore
};

/**
 * @brief Circuit breaker states
 */
enum class CircuitState {
    CLOSED,     // Normal operation
    OPEN,       // Failing, requests blocked
    HALF_OPEN   // Testing if service recovered
};

/**
 * @brief Structured error information
 */
struct ErrorInfo {
    std::string error_id;
    ErrorCategory category;
    ErrorSeverity severity;
    std::string component;        // Component where error occurred
    std::string operation;        // Operation being performed
    std::string message;          // Human-readable error message
    std::string details;          // Technical details
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> context;  // Additional context
    std::optional<std::string> correlation_id;  // For tracking related errors

    ErrorInfo(ErrorCategory cat, ErrorSeverity sev, std::string comp, std::string op,
             std::string msg, std::string det = "")
        : error_id(generate_error_id(cat, comp, op)),
          category(cat), severity(sev), component(std::move(comp)), operation(std::move(op)),
          message(std::move(msg)), details(std::move(det)),
          timestamp(std::chrono::system_clock::now()) {}

    nlohmann::json to_json() const {
        nlohmann::json context_json;
        for (const auto& [key, value] : context) {
            context_json[key] = value;
        }

        nlohmann::json result = {
            {"error_id", error_id},
            {"category", static_cast<int>(category)},
            {"severity", static_cast<int>(severity)},
            {"component", component},
            {"operation", operation},
            {"message", message},
            {"details", details},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp.time_since_epoch()).count()},
            {"context", context_json}
        };

        if (correlation_id) {
            result["correlation_id"] = *correlation_id;
        }

        return result;
    }

private:
    static std::string generate_error_id(ErrorCategory category, const std::string& component,
                                       const std::string& operation) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return "err_" + std::to_string(static_cast<int>(category)) + "_" +
               component + "_" + operation + "_" + std::to_string(ms);
    }
};

/**
 * @brief Circuit breaker configuration and state
 */
struct CircuitBreaker {
    std::string breaker_id;
    std::string service_name;
    CircuitState state;
    int failure_count;
    int success_count;
    std::chrono::system_clock::time_point last_failure_time;
    std::chrono::system_clock::time_point next_attempt_time;

    // Configuration
    int failure_threshold;        // Failures before opening circuit
    int success_threshold;        // Successes needed to close circuit
    std::chrono::seconds timeout; // How long to wait before trying again

    CircuitBreaker() = default;

    CircuitBreaker(std::string id, std::string service, int fail_thresh = 5,
                  int success_thresh = 3, std::chrono::seconds timeo = std::chrono::seconds(60))
        : breaker_id(std::move(id)), service_name(std::move(service)),
          state(CircuitState::CLOSED), failure_count(0), success_count(0),
          failure_threshold(fail_thresh), success_threshold(success_thresh), timeout(timeo) {}

    bool can_attempt() const {
        if (state == CircuitState::CLOSED) return true;
        if (state == CircuitState::OPEN) {
            return std::chrono::system_clock::now() >= next_attempt_time;
        }
        return state == CircuitState::HALF_OPEN;
    }

    void record_success() {
        failure_count = 0;
        success_count++;

        if (state == CircuitState::HALF_OPEN && success_count >= success_threshold) {
            state = CircuitState::CLOSED;
            success_count = 0;
        }
    }

    void record_failure() {
        failure_count++;
        success_count = 0;
        last_failure_time = std::chrono::system_clock::now();

        if (state == CircuitState::CLOSED && failure_count >= failure_threshold) {
            state = CircuitState::OPEN;
            next_attempt_time = last_failure_time + timeout;
        } else if (state == CircuitState::HALF_OPEN) {
            state = CircuitState::OPEN;
            next_attempt_time = last_failure_time + timeout;
        }
    }

    nlohmann::json to_json() const {
        return {
            {"breaker_id", breaker_id},
            {"service_name", service_name},
            {"state", static_cast<int>(state)},
            {"failure_count", failure_count},
            {"success_count", success_count},
            {"failure_threshold", failure_threshold},
            {"success_threshold", success_threshold},
            {"last_failure_time", std::chrono::duration_cast<std::chrono::milliseconds>(
                last_failure_time.time_since_epoch()).count()},
            {"next_attempt_time", std::chrono::duration_cast<std::chrono::milliseconds>(
                next_attempt_time.time_since_epoch()).count()}
        };
    }
};

/**
 * @brief Retry configuration
 */
struct RetryConfig {
    int max_attempts;                     // Maximum number of retry attempts
    std::chrono::milliseconds initial_delay;  // Initial delay between retries
    double backoff_multiplier;           // Multiplier for exponential backoff
    std::chrono::milliseconds max_delay;     // Maximum delay between retries
    std::vector<ErrorCategory> retryable_errors; // Which error types to retry

    RetryConfig(int max_att = 3, std::chrono::milliseconds init_delay = std::chrono::milliseconds(100),
               double multiplier = 2.0, std::chrono::milliseconds max_del = std::chrono::seconds(30))
        : max_attempts(max_att), initial_delay(init_delay), backoff_multiplier(multiplier),
          max_delay(max_del),
          retryable_errors({ErrorCategory::NETWORK, ErrorCategory::TIMEOUT, ErrorCategory::EXTERNAL_API}) {}

    nlohmann::json to_json() const {
        nlohmann::json retryable_json;
        for (auto err : retryable_errors) {
            retryable_json.push_back(static_cast<int>(err));
        }

        return {
            {"max_attempts", max_attempts},
            {"initial_delay_ms", initial_delay.count()},
            {"backoff_multiplier", backoff_multiplier},
            {"max_delay_ms", max_delay.count()},
            {"retryable_errors", retryable_json}
        };
    }
};

/**
 * @brief Fallback mechanism configuration
 */
struct FallbackConfig {
    std::string component_name;
    bool enable_fallback;
    std::string fallback_strategy;       // "basic", "cached", "simplified", "external"
    std::chrono::seconds cache_ttl;      // How long to cache fallback results
    std::unordered_map<std::string, std::string> fallback_parameters;

    FallbackConfig() = default;

    FallbackConfig(std::string name)
        : component_name(std::move(name)), enable_fallback(true),
          fallback_strategy("basic"), cache_ttl(std::chrono::seconds(300)) {}

    nlohmann::json to_json() const {
        nlohmann::json params_json;
        for (const auto& [key, value] : fallback_parameters) {
            params_json[key] = value;
        }

        return {
            {"component_name", component_name},
            {"enable_fallback", enable_fallback},
            {"fallback_strategy", fallback_strategy},
            {"cache_ttl_seconds", cache_ttl.count()},
            {"fallback_parameters", params_json}
        };
    }
};

/**
 * @brief Health status for components
 */
enum class HealthStatus {
    HEALTHY,        // Component is functioning normally
    DEGRADED,       // Component has issues but still functional
    UNHEALTHY,      // Component is not functioning
    UNKNOWN         // Health status cannot be determined
};

/**
 * @brief Component health information
 */
struct ComponentHealth {
    std::string component_name;
    HealthStatus status;
    std::chrono::system_clock::time_point last_check;
    std::chrono::system_clock::time_point last_success;
    std::chrono::system_clock::time_point last_failure;
    int consecutive_failures;
    std::string status_message;
    std::unordered_map<std::string, double> metrics;  // Health metrics (response time, error rate, etc.)

    ComponentHealth(std::string name)
        : component_name(std::move(name)), status(HealthStatus::UNKNOWN),
          consecutive_failures(0) {}

    void record_success() {
        status = HealthStatus::HEALTHY;
        last_success = std::chrono::system_clock::now();
        last_check = last_success;
        consecutive_failures = 0;
        status_message = "Component is healthy";
    }

    void record_failure(const std::string& error_msg) {
        consecutive_failures++;
        last_failure = std::chrono::system_clock::now();
        last_check = last_failure;

        if (consecutive_failures >= 5) {
            status = HealthStatus::UNHEALTHY;
        } else if (consecutive_failures >= 2) {
            status = HealthStatus::DEGRADED;
        }

        status_message = error_msg;
    }

    nlohmann::json to_json() const {
        nlohmann::json metrics_json;
        for (const auto& [key, value] : metrics) {
            metrics_json[key] = value;
        }

        return {
            {"component_name", component_name},
            {"status", static_cast<int>(status)},
            {"last_check", std::chrono::duration_cast<std::chrono::milliseconds>(
                last_check.time_since_epoch()).count()},
            {"last_success", std::chrono::duration_cast<std::chrono::milliseconds>(
                last_success.time_since_epoch()).count()},
            {"last_failure", std::chrono::duration_cast<std::chrono::milliseconds>(
                last_failure.time_since_epoch()).count()},
            {"consecutive_failures", consecutive_failures},
            {"status_message", status_message},
            {"metrics", metrics_json}
        };
    }
};

/**
 * @brief Error handling configuration
 */
struct ErrorHandlingConfig {
    bool enable_error_logging;
    bool enable_error_alerts;
    int max_errors_per_minute;           // Rate limiting for error alerts
    std::chrono::hours error_retention_period;
    std::unordered_map<ErrorCategory, RecoveryStrategy> default_strategies;
    std::unordered_map<std::string, FallbackConfig> component_fallbacks;

    // Circuit breaker configuration
    int circuit_breaker_failure_threshold;     // Failures before opening circuit
    int circuit_breaker_timeout_seconds;       // Seconds before trying half-open
    int circuit_breaker_success_threshold;     // Successes needed to close circuit

    ErrorHandlingConfig()
        : enable_error_logging(true), enable_error_alerts(true),
          max_errors_per_minute(10), error_retention_period(std::chrono::hours(24)),
          circuit_breaker_failure_threshold(5), circuit_breaker_timeout_seconds(60),
          circuit_breaker_success_threshold(3) {

        // Default recovery strategies by error category
        default_strategies = {
            {ErrorCategory::NETWORK, RecoveryStrategy::RETRY},
            {ErrorCategory::DATABASE, RecoveryStrategy::CIRCUIT_BREAKER},
            {ErrorCategory::EXTERNAL_API, RecoveryStrategy::CIRCUIT_BREAKER},
            {ErrorCategory::CONFIGURATION, RecoveryStrategy::MANUAL},
            {ErrorCategory::VALIDATION, RecoveryStrategy::FALLBACK},
            {ErrorCategory::PROCESSING, RecoveryStrategy::DEGRADATION},
            {ErrorCategory::RESOURCE, RecoveryStrategy::CIRCUIT_BREAKER},
            {ErrorCategory::SECURITY, RecoveryStrategy::MANUAL},
            {ErrorCategory::TIMEOUT, RecoveryStrategy::RETRY},
            {ErrorCategory::UNKNOWN, RecoveryStrategy::IGNORE}
        };
    }

    nlohmann::json to_json() const {
        nlohmann::json strategies_json;
        for (const auto& [cat, strategy] : default_strategies) {
            strategies_json[std::to_string(static_cast<int>(cat))] = static_cast<int>(strategy);
        }

        nlohmann::json fallbacks_json;
        for (const auto& [name, config] : component_fallbacks) {
            fallbacks_json[name] = config.to_json();
        }

        return {
            {"enable_error_logging", enable_error_logging},
            {"enable_error_alerts", enable_error_alerts},
            {"max_errors_per_minute", max_errors_per_minute},
            {"error_retention_period_hours", error_retention_period.count()},
            {"default_strategies", strategies_json},
            {"component_fallbacks", fallbacks_json}
        };
    }
};

} // namespace regulens
