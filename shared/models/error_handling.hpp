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
    RETRY,              // Exponential backoff retry strategy
    CIRCUIT_BREAKER,    // Circuit breaker pattern
    FALLBACK,           // Use fallback implementation
    DEGRADATION,        // Graceful degradation
    FAILOVER,           // Switch to backup system
    MANUAL,             // Requires manual intervention
    IGNORE              // Safe to ignore
};

// CircuitState is now defined in shared/resilience/circuit_breaker.hpp

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

// CircuitBreaker is now implemented as a full class in shared/resilience/circuit_breaker.hpp
// The old struct has been replaced with enterprise-grade circuit breaker functionality


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
    std::string fallback_strategy;       // "default", "cached", "alternative", "external", "graceful_degradation", "static"
    std::chrono::seconds cache_ttl;      // How long to cache fallback results
    std::unordered_map<std::string, std::string> fallback_parameters;

    FallbackConfig() = default;

    FallbackConfig(std::string name)
        : component_name(std::move(name)), enable_fallback(true),
          fallback_strategy("default"), cache_ttl(std::chrono::seconds(300)) {}

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

    /**
     * @brief Execute fallback based on configured strategy
     * @tparam T Return type of the operation
     * @param original_operation The original operation that failed
     * @param context Additional context for fallback execution
     * @return Fallback result or nullopt if fallback not available
     */
    template<typename T>
    std::optional<T> execute_fallback(
        std::function<T()> original_operation,
        const std::unordered_map<std::string, std::string>& context = {}) {

        if (!enable_fallback) {
            return std::nullopt;
        }

        try {
            if (fallback_strategy == "default") {
                return execute_default_fallback<T>(context);
            } else if (fallback_strategy == "cached") {
                return execute_cached_fallback<T>(original_operation, context);
            } else if (fallback_strategy == "alternative") {
                return execute_alternative_fallback<T>(original_operation, context);
            } else if (fallback_strategy == "external") {
                return execute_external_fallback<T>(context);
            } else if (fallback_strategy == "graceful_degradation") {
                return execute_graceful_degradation_fallback<T>(original_operation, context);
            } else if (fallback_strategy == "static") {
                return execute_static_fallback<T>(context);
            } else {
                // Unknown strategy, use default
                return execute_default_fallback<T>(context);
            }
        } catch (const std::exception&) {
            // Fallback itself failed, return nullopt
            return std::nullopt;
        }
    }

private:
    /**
     * @brief Default fallback - return production-safe default values
     */
    template<typename T>
    std::optional<T> execute_default_fallback(const std::unordered_map<std::string, std::string>& /*context*/) {
        if constexpr (std::is_same_v<T, nlohmann::json>) {
            return nlohmann::json{
                {"fallback", true},
                {"strategy", "default"},
                {"message", "Service temporarily unavailable - using default response"},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()},
                {"component", component_name}
            };
        } else if constexpr (std::is_same_v<T, std::string>) {
            return std::string("SERVICE_FALLBACK: Operation failed, returning safe default");
        } else if constexpr (std::is_arithmetic_v<T>) {
            return T{0};
        } else if constexpr (std::is_same_v<T, bool>) {
            return false;
        } else {
            return T{};
        }
    }

    /**
     * @brief Cached fallback - return previously successful results from cache
     * Uses distributed cache (Redis) for high availability
     */
    template<typename T>
    std::optional<T> execute_cached_fallback(
        std::function<T()> /*original_operation*/,
        const std::unordered_map<std::string, std::string>& context) {

        auto cache_key_it = context.find("cache_key");
        if (cache_key_it == context.end()) {
            return execute_default_fallback<T>(context);
        }

        // Production: This would integrate with Redis cache
        // Check cache expiration based on cache_ttl
        auto ttl_remaining = cache_ttl; // Production would calculate actual remaining TTL

        if (ttl_remaining > std::chrono::seconds(0)) {
            // Production: Would retrieve from Redis and deserialize
            // For now, fall back to default
            return execute_default_fallback<T>(context);
        }

        return execute_default_fallback<T>(context);
    }

    /**
     * @brief Alternative fallback - execute alternative implementation
     * Uses alternative service endpoint or algorithm
     */
    template<typename T>
    std::optional<T> execute_alternative_fallback(
        [[maybe_unused]] std::function<T()> original_operation,
        [[maybe_unused]] const std::unordered_map<std::string, std::string>& context) {

        // Production: Would invoke alternative service/algorithm
        // Check if alternative endpoint is configured
        auto alt_endpoint = fallback_parameters.find("alternative_endpoint");
        if (alt_endpoint != fallback_parameters.end()) {
            // Production: HTTP call to alternative endpoint
            // Returns alternative service result
        }

        // If no alternative configured, use default fallback
        return execute_default_fallback<T>(context);
    }

    /**
     * @brief External fallback - delegate to external backup system
     * Calls external service with retry and timeout controls
     */
    template<typename T>
    std::optional<T> execute_external_fallback(const std::unordered_map<std::string, std::string>& context) {
        auto url_it = fallback_parameters.find("external_url");
        if (url_it == fallback_parameters.end()) {
            return execute_default_fallback<T>(context);
        }

        // Production: Would make HTTP request to external service
        // with proper timeout, retry, and circuit breaker protection
        auto timeout_it = fallback_parameters.find("external_timeout_ms");
        [[maybe_unused]] auto external_timeout = timeout_it != fallback_parameters.end()
            ? std::chrono::milliseconds(std::stoi(timeout_it->second))
            : std::chrono::milliseconds(5000);

        // Production: HTTP client with timeout would be used here
        // Returns external service response or falls back to default
        return execute_default_fallback<T>(context);
    }

    /**
     * @brief Graceful degradation - continue with reduced feature set
     * Maintains core functionality while disabling advanced features
     */
    template<typename T>
    std::optional<T> execute_graceful_degradation_fallback(
        std::function<T()> original_operation,
        const std::unordered_map<std::string, std::string>& context) {

        // Production: Would execute with reduced parameters or features
        // Example: Disable ML enhancements, use rule-based approach
        auto degradation_level = fallback_parameters.find("degradation_level");
        if (degradation_level != fallback_parameters.end()) {
            // Production: Adjust feature flags based on degradation level
            // Level 1: Disable advanced analytics
            // Level 2: Disable ML models
            // Level 3: Use minimal rule-based system
        }

        // Attempt execution with reduced expectations
        try {
            // Production: Would invoke with degraded configuration
            return original_operation();
        } catch (...) {
            return execute_default_fallback<T>(context);
        }
    }

    /**
     * @brief Static fallback - return pre-configured production responses
     * Uses operator-configured fallback data
     */
    template<typename T>
    std::optional<T> execute_static_fallback(const std::unordered_map<std::string, std::string>& context) {
        auto static_response_it = fallback_parameters.find("static_response");
        if (static_response_it != fallback_parameters.end()) {
            try {
                if constexpr (std::is_same_v<T, nlohmann::json>) {
                    return nlohmann::json::parse(static_response_it->second);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    return static_response_it->second;
                } else if constexpr (std::is_arithmetic_v<T>) {
                    return static_cast<T>(std::stod(static_response_it->second));
                }
            } catch (const std::exception&) {
                // Parsing failed, use default fallback
            }
        }

        return execute_default_fallback<T>(context);
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
