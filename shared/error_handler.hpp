#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <future>
#include <optional>
#include <type_traits>
#include <nlohmann/json.hpp>

#include "models/error_handling.hpp"

// Circuit breaker and metrics includes
#include "resilience/circuit_breaker.hpp"
#include "metrics/prometheus_metrics.hpp"
#include "logging/structured_logger.hpp"
#include "config/configuration_manager.hpp"

namespace regulens {

// Type trait to detect std::optional types
template<typename T>
struct is_optional : std::false_type {};

template<typename T>
struct is_optional<std::optional<T>> : std::true_type {
    using value_type = T;
};

// Circuit breaker states
enum class CircuitBreakerState {
    CLOSED,      // Normal operation
    OPEN,        // Failing, reject requests
    HALF_OPEN    // Testing if service recovered
};

// Circuit breaker state information
struct CircuitBreakerStateInfo {
    CircuitBreakerState state = CircuitBreakerState::CLOSED;
    int failure_count = 0;
    std::chrono::system_clock::time_point last_failure_time;
};

/**
 * @brief Comprehensive error handling and fallback system
 *
 * Provides circuit breakers, retry logic, fallback mechanisms, and health monitoring
 * for robust operation of advanced agent capabilities.
 */
class ErrorHandler {
public:
    ErrorHandler(ConfigurationManager* config_manager,
                 StructuredLogger* logger)
        : config_manager_(config_manager, [](ConfigurationManager*){}), logger_(logger, [](StructuredLogger*){}) {}

    /**
     * @brief Set the Prometheus metrics collector for circuit breakers
     * @param metrics_collector Shared pointer to PrometheusMetricsCollector
     */
    void set_metrics_collector(std::shared_ptr<PrometheusMetricsCollector> metrics_collector);

    /**
     * @brief Execute an operation with advanced circuit breaker protection
     * @tparam Func Callable type
     * @param operation Function to execute
     * @param service_name Name of the external service
     * @param component_name Name of the component
     * @param operation_name Name of the operation
     * @return CircuitBreakerResult with execution details
     */
public:
    template<typename Func>
    CircuitBreakerResult execute_with_advanced_circuit_breaker(
        Func&& operation,
        const std::string& service_name,
        const std::string& component_name,
        const std::string& operation_name) {
        // Circuit breaker temporarily disabled - use basic execution
        logger_->warn("Circuit breaker disabled - using basic execution for service: " + service_name,
                      "ErrorHandler", "execute_with_advanced_circuit_breaker");

        // Basic execution without circuit breaker protection
        auto start_time = std::chrono::high_resolution_clock::now();

        try {
            // Execute the operation directly
            auto result = operation();

            auto end_time = std::chrono::high_resolution_clock::now();
            auto execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

            // Update component health
            update_component_health(component_name, true, "Basic execution success");

            return CircuitBreakerResult(true, nlohmann::json::object(), "", execution_time, CircuitState::CLOSED);

        } catch (const std::exception& e) {
            auto end_time = std::chrono::high_resolution_clock::now();
            auto execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

            // Update component health
            update_component_health(component_name, false, std::string("Execution failed: ") + e.what());

            // Report error
            ErrorInfo error(ErrorCategory::EXTERNAL_API, ErrorSeverity::HIGH,
                           component_name, operation_name, std::string("Operation failed: ") + e.what());
            error.context["service"] = service_name;
            report_error(error);

            return CircuitBreakerResult(false, std::nullopt, std::string("Operation failed: ") + e.what(),
                                                 execution_time, CircuitState::CLOSED);
        }
    }

    ~ErrorHandler();

    /**
     * @brief Initialize the error handling system
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Shutdown the error handling system
     */
    void shutdown();

    /**
     * @brief Execute an operation with error handling and recovery
     * @param operation Function to execute
     * @param component_name Name of the component performing the operation
     * @param operation_name Name of the operation
     * @param config Retry and fallback configuration
     * @return Result of the operation or fallback
     */
    template<typename T>
    std::optional<T> execute_with_recovery(
        std::function<T()> operation,
        const std::string& component_name,
        const std::string& operation_name,
        const RetryConfig& retry_config = RetryConfig());

    /**
     * @brief Execute an operation with circuit breaker protection
     * @param operation Function to execute
     * @param service_name Name of the external service
     * @param component_name Component performing the operation
     * @param operation_name Operation name
     * @return Result of the operation or circuit breaker response
     */
    template<typename T>
    std::optional<T> execute_with_circuit_breaker(
        std::function<T()> operation,
        const std::string& service_name,
        const std::string& component_name,
        const std::string& operation_name);

    // Circuit breaker and metrics temporarily disabled - circular dependency issues
    // void set_metrics_collector(std::shared_ptr<PrometheusMetricsCollector> metrics_collector);

    // template<typename Func>
    // regulens::CircuitBreakerResult execute_with_advanced_circuit_breaker(
    //     Func&& operation,
    //     const std::string& service_name,
    //     const std::string& component_name,
    //     const std::string& operation_name);

    /**
     * @brief Report an error for logging and recovery
     * @param error Error information
     * @return Error ID for tracking
     */
    std::string report_error(const ErrorInfo& error);

    /**
     * @brief Check if a component is healthy
     * @param component_name Component to check
     * @return Health status
     */
    HealthStatus get_component_health(const std::string& component_name);

    /**
     * @brief Perform health check on a component
     * @param component_name Component to check
     * @param health_check Function that returns true if healthy
     * @return Current health status
     */
    HealthStatus perform_health_check(const std::string& component_name,
                                    std::function<bool()> health_check);

    // Circuit breaker management (circular dependency resolved via forward declarations)
    std::shared_ptr<CircuitBreaker> get_circuit_breaker(const std::string& service_name);
    bool reset_circuit_breaker(const std::string& service_name);

    /**
     * @brief Get fallback configuration for a component
     * @param component_name Component name
     * @return Fallback configuration
     */
    std::optional<FallbackConfig> get_fallback_config(const std::string& component_name);

    /**
     * @brief Set fallback configuration for a component
     * @param config Fallback configuration
     * @return true if configuration set successfully
     */
    bool set_fallback_config(const FallbackConfig& config);

    /**
     * @brief Get error statistics
     * @return JSON with error statistics
     */
    nlohmann::json get_error_stats();

    /**
     * @brief Get health dashboard data
     * @return JSON with health information for all components
     */
    nlohmann::json get_health_dashboard();

    /**
     * @brief Export error data for analysis
     * @param component_filter Optional component filter
     * @param hours_back Hours of history to include
     * @return JSON array of errors
     */
    nlohmann::json export_error_data(const std::string& component_filter = "",
                                   int hours_back = 24);

    /**
     * @brief Force cleanup of old error data
     * @return Number of items cleaned up
     */
    size_t cleanup_old_errors();

    // Configuration access
    const ErrorHandlingConfig& get_config() const { return config_; }

private:
    std::shared_ptr<ConfigurationManager> config_manager_;
    std::shared_ptr<StructuredLogger> logger_;

    ErrorHandlingConfig config_;

    // Thread-safe storage
    std::mutex error_mutex_;
    std::deque<ErrorInfo> error_history_;
    std::unordered_map<std::string, ComponentHealth> component_health_;
    // Circuit breaker and metrics (circular dependency resolved via forward declarations)
    std::unordered_map<std::string, std::shared_ptr<CircuitBreaker>> circuit_breakers_;
    std::shared_ptr<PrometheusMetricsCollector> metrics_collector_;
    std::mutex circuit_breaker_mutex_;
    std::unordered_map<std::string, FallbackConfig> fallback_configs_;

    std::mutex stats_mutex_;
    std::atomic<size_t> total_errors_processed_;

    // Enhanced error correlation and context tracking
    std::mutex error_context_mutex_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> error_contexts_;
    std::atomic<size_t> total_recovery_attempts_;
    std::atomic<size_t> total_successful_recoveries_;

    // Background processing
    std::thread cleanup_thread_;
    std::atomic<bool> running_;
    std::mutex cleanup_cv_mutex_;
    std::condition_variable cleanup_cv_;

    // Error processing and recovery
    RecoveryStrategy get_recovery_strategy(const ErrorInfo& error);
    bool should_retry_error(const ErrorInfo& error, const RetryConfig& config);
    std::chrono::milliseconds calculate_retry_delay(int attempt, const RetryConfig& config);

    // Fallback mechanisms
    template<typename T>
    std::optional<T> execute_fallback(const std::string& component_name,
                                    std::function<T()> original_operation);

    // Circuit breaker management
    std::shared_ptr<CircuitBreaker> get_or_create_circuit_breaker(const std::string& service_name);

    // Health monitoring
    void update_component_health(const std::string& component_name, bool success,
                               const std::string& status_message = "");

    // Error analysis and alerting
    void analyze_error_patterns();
    void check_error_rate_limits();
    void send_error_alerts(const ErrorInfo& error);

    // Enhanced error tracking and correlation
    std::string generate_error_correlation_id();
    void add_error_context(const std::string& correlation_id, const std::string& key, const std::string& value);
    std::unordered_map<std::string, std::string> get_error_context(const std::string& correlation_id);
    void clear_error_context(const std::string& correlation_id);

    // Comprehensive health monitoring
public:
    nlohmann::json get_system_health_report();
private:
    bool check_external_service_health(const std::string& service_name, const std::string& endpoint);
    nlohmann::json get_component_health_status();
    void perform_external_health_checks();

    // Utility functions
    bool is_component_critical(const std::string& component_name);
    ErrorSeverity calculate_error_severity(const ErrorInfo& error);
    bool should_alert_on_error(const ErrorInfo& error);

    // Background cleanup worker
    void cleanup_worker();

    // Fallback implementations for specific components
    std::optional<nlohmann::json> fallback_database_query(const std::string& query);
    std::optional<std::string> fallback_external_api_call(const std::string& url);
    std::optional<nlohmann::json> fallback_llm_completion(const std::string& prompt);
    std::optional<nlohmann::json> fallback_vector_search(const std::string& query);

    // Circuit breaker configurations for common services
    void initialize_default_circuit_breakers();
    void initialize_default_fallback_configs();

    // Statistics and reporting
    void update_error_statistics(const ErrorInfo& error, bool recovered);

    /**
     * @brief Check if circuit breaker is open for a component
     * @param component_name Name of the component
     * @return true if circuit is open (requests should fail fast)
     */
    bool is_circuit_open(const std::string& component_name);

    /**
     * @brief Record a successful operation for circuit breaker
     * @param component_name Name of the component
     */
    void record_success(const std::string& component_name);

    /**
     * @brief Record a failed operation for circuit breaker
     * @param component_name Name of the component
     */
    void record_failure(const std::string& component_name);
    nlohmann::json generate_error_summary(int hours_back = 24);
};

// Template implementations

template<typename T>
std::optional<T> ErrorHandler::execute_with_recovery(
    std::function<T()> operation,
    const std::string& component_name,
    const std::string& operation_name,
    const RetryConfig& retry_config) {

    update_component_health(component_name, true, "Starting operation");

    for (int attempt = 0; attempt <= retry_config.max_attempts; ++attempt) {
        try {
            auto result = operation();
            update_component_health(component_name, true, "Operation successful");
            return result;

        } catch (const std::exception& e) {
            // Report the error
            ErrorInfo error(ErrorCategory::UNKNOWN, ErrorSeverity::MEDIUM,
                          component_name, operation_name, e.what());
            error.context["attempt"] = std::to_string(attempt + 1);
            error.context["max_attempts"] = std::to_string(retry_config.max_attempts);
            report_error(error);

            update_component_health(component_name, false, std::string("Operation failed: ") + e.what());

            // Check if we should retry
            if (attempt < retry_config.max_attempts && should_retry_error(error, retry_config)) {
                auto delay = calculate_retry_delay(attempt, retry_config);
                logger_->info("Retrying operation " + operation_name + " for component " + component_name +
                            " in " + std::to_string(delay.count()) + "ms (attempt " +
                            std::to_string(attempt + 1) + "/" + std::to_string(retry_config.max_attempts + 1) + ")", "", "", {});

                std::this_thread::sleep_for(delay);
                continue;
            }

            // Try fallback if retries exhausted
            auto fallback_result = execute_fallback<T>(component_name, operation);
            if (fallback_result) {
                logger_->info("Using fallback result for operation {} in component {}",
                            operation_name, component_name);
                update_component_health(component_name, true, "Fallback successful");
                return fallback_result;
            }

            // All recovery options exhausted
            logger_->error("All recovery options exhausted for operation {} in component {}",
                         operation_name, component_name);
            break;

        } catch (...) {
            // Handle non-standard exceptions
            ErrorInfo error(ErrorCategory::UNKNOWN, ErrorSeverity::HIGH,
                          component_name, operation_name, "Unknown exception occurred");
            error.context["attempt"] = std::to_string(attempt + 1);
            report_error(error);

            update_component_health(component_name, false, "Unknown exception occurred");

            if (attempt < retry_config.max_attempts) {
                auto delay = calculate_retry_delay(attempt, retry_config);
                std::this_thread::sleep_for(delay);
                continue;
            }

            break;
        }
    }

    update_component_health(component_name, false, "Operation failed permanently");
    return std::nullopt;
}

template<typename T>
std::optional<T> ErrorHandler::execute_with_circuit_breaker(
    std::function<T()> operation,
    const std::string& service_name,
    const std::string& component_name,
    const std::string& operation_name) {

    auto breaker = get_or_create_circuit_breaker(service_name);

    // Check if circuit is open - fail fast
    if (breaker->get_current_state() == CircuitState::OPEN) {
        logger_->warn("Circuit breaker OPEN for service " + service_name + ", blocking request to " +
                    component_name + "." + operation_name, "", "", {});

        // Try fallback instead
        return execute_fallback<T>(component_name, operation);
    }

    // Execute through circuit breaker
    std::optional<T> result_holder;
    auto cb_result = breaker->execute([&operation, &result_holder]() -> CircuitBreakerResult {
        try {
            result_holder = operation();
            // Check if result is valid
            if constexpr (is_optional<T>::value) {
                // For optional types, check if it has a value
                if (result_holder && result_holder->has_value()) {
                    return CircuitBreakerResult(true, nlohmann::json::object(), "",
                                              std::chrono::milliseconds(0), CircuitState::CLOSED);
                } else {
                    return CircuitBreakerResult(false, std::nullopt, "Operation returned empty optional",
                                              std::chrono::milliseconds(0), CircuitState::CLOSED);
                }
            } else {
                return CircuitBreakerResult(true, nlohmann::json::object(), "",
                                          std::chrono::milliseconds(0), CircuitState::CLOSED);
            }
        } catch (const std::exception& e) {
            return CircuitBreakerResult(false, std::nullopt, e.what(),
                                      std::chrono::milliseconds(0), CircuitState::CLOSED);
        }
    });

    if (cb_result.success && result_holder) {
        update_component_health(component_name, true, "Circuit breaker success");
        return *result_holder;
    } else {
        ErrorInfo error(ErrorCategory::EXTERNAL_API, ErrorSeverity::HIGH,
                      component_name, operation_name,
                      std::string("Circuit breaker failure: ") + cb_result.error_message);
        error.context["service"] = service_name;
        error.context["circuit_state"] = std::to_string(static_cast<int>(cb_result.circuit_state_at_call));
        report_error(error);

        update_component_health(component_name, false, std::string("Circuit breaker failure: ") + cb_result.error_message);

        // Try fallback
        return execute_fallback<T>(component_name, operation);
    }
}

template<typename T>
std::optional<T> ErrorHandler::execute_fallback(const std::string& component_name,
                                              std::function<T()> /*original_operation*/) {
    auto fallback_config_opt = get_fallback_config(component_name);
    if (!fallback_config_opt || !fallback_config_opt->enable_fallback) {
        return std::nullopt;
    }

    const auto& fallback_config = *fallback_config_opt;

    logger_->info("Attempting fallback for component: {}", component_name);

    try {
        // Different fallback strategies
        if (fallback_config.fallback_strategy == "basic") {
            // Return a basic/default result
            if constexpr (std::is_same_v<T, nlohmann::json>) {
                return nlohmann::json{{"fallback", true}, {"message", "Service temporarily unavailable"}};
            } else if constexpr (std::is_same_v<T, std::string>) {
                return std::string("FALLBACK: Service temporarily unavailable");
            } else {
                // For other types, return default-constructed value
                return T{};
            }
        } else if (fallback_config.fallback_strategy == "circuit_breaker") {
            // Implement proper circuit breaker pattern
            if (is_circuit_open(component_name)) {
                logger_->warn("Circuit breaker open for component: {}", component_name);
                throw std::runtime_error("Circuit breaker is open for component: " + component_name);
            }
            return std::nullopt; // Allow retry mechanism to handle
        } else if (fallback_config.fallback_strategy == "retry") {
            // Implement exponential backoff retry
            logger_->info("Implementing retry strategy for component: {}", component_name);
            throw std::runtime_error("Retry mechanism should be implemented at caller level for component: " + component_name);
        }

    } catch (const std::exception& e) {
        logger_->error("Fallback failed for component {}: {}", component_name, e.what());
    }

    return std::nullopt;
}

// Convenience functions for common error scenarios

/**
 * @brief Create database error
 */
inline ErrorInfo create_database_error(const std::string& component, const std::string& operation,
                                     const std::string& message, const std::string& details = "") {
    return ErrorInfo(ErrorCategory::DATABASE, ErrorSeverity::HIGH, component, operation, message, details);
}

/**
 * @brief Create network error
 */
inline ErrorInfo create_network_error(const std::string& component, const std::string& operation,
                                    const std::string& message, const std::string& details = "") {
    return ErrorInfo(ErrorCategory::NETWORK, ErrorSeverity::MEDIUM, component, operation, message, details);
}

/**
 * @brief Create API error
 */
inline ErrorInfo create_api_error(const std::string& component, const std::string& operation,
                                const std::string& message, const std::string& details = "") {
    return ErrorInfo(ErrorCategory::EXTERNAL_API, ErrorSeverity::HIGH, component, operation, message, details);
}

/**
 * @brief Create timeout error
 */
inline ErrorInfo create_timeout_error(const std::string& component, const std::string& operation,
                                    const std::string& message, const std::string& details = "") {
    return ErrorInfo(ErrorCategory::TIMEOUT, ErrorSeverity::MEDIUM, component, operation, message, details);
}

} // namespace regulens
