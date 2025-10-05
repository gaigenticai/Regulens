/**
 * Circuit Breaker Pattern Implementation - Production-Grade Resilience
 *
 * Enterprise-grade circuit breaker implementation providing resilience against
 * external API failures with configurable failure thresholds, recovery mechanisms,
 * and comprehensive monitoring capabilities.
 *
 * Features:
 * - Three-state operation: Closed, Open, Half-Open
 * - Configurable failure thresholds and recovery timeouts
 * - Exponential backoff and jitter for recovery attempts
 * - Comprehensive metrics and health monitoring
 * - Thread-safe operations with atomic state management
 * - Integration with Prometheus metrics collection
 */

#pragma once

#include <memory>
#include <string>
#include <atomic>
#include <chrono>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <optional>
#include <nlohmann/json.hpp>

#include "../config/configuration_manager.hpp"
#include "../logging/structured_logger.hpp"
#include "../error_handler.hpp"

namespace regulens {

/**
 * @brief Circuit breaker states
 */
enum class CircuitState {
    CLOSED,      // Normal operation - requests pass through
    OPEN,        // Circuit is open - requests fail fast
    HALF_OPEN    // Testing recovery - limited requests allowed
};

/**
 * @brief Circuit breaker metrics
 */
struct CircuitBreakerMetrics {
    std::atomic<size_t> total_requests{0};
    std::atomic<size_t> successful_requests{0};
    std::atomic<size_t> failed_requests{0};
    std::atomic<size_t> rejected_requests{0};      // Requests rejected when OPEN
    std::atomic<size_t> state_transitions{0};
    std::atomic<size_t> recovery_attempts{0};
    std::atomic<size_t> successful_recoveries{0};
    std::chrono::system_clock::time_point last_failure_time;
    std::chrono::system_clock::time_point last_state_change_time;
    std::chrono::system_clock::time_point created_time;

    CircuitBreakerMetrics() : created_time(std::chrono::system_clock::now()) {}

    CircuitBreakerMetrics(const CircuitBreakerMetrics& other) {
        total_requests.store(other.total_requests.load());
        successful_requests.store(other.successful_requests.load());
        failed_requests.store(other.failed_requests.load());
        rejected_requests.store(other.rejected_requests.load());
        state_transitions.store(other.state_transitions.load());
        recovery_attempts.store(other.recovery_attempts.load());
        successful_recoveries.store(other.successful_recoveries.load());
        last_failure_time = other.last_failure_time;
        last_state_change_time = other.last_state_change_time;
        created_time = other.created_time;
    }

    CircuitBreakerMetrics& operator=(const CircuitBreakerMetrics& other) {
        if (this != &other) {
            total_requests.store(other.total_requests.load());
            successful_requests.store(other.successful_requests.load());
            failed_requests.store(other.failed_requests.load());
            rejected_requests.store(other.rejected_requests.load());
            state_transitions.store(other.state_transitions.load());
            recovery_attempts.store(other.recovery_attempts.load());
            successful_recoveries.store(other.successful_recoveries.load());
            last_failure_time = other.last_failure_time;
            last_state_change_time = other.last_state_change_time;
            created_time = other.created_time;
        }
        return *this;
    }

    nlohmann::json to_json() const;
};

/**
 * @brief Circuit breaker configuration
 */
struct CircuitBreakerConfig {
    std::string name;
    size_t failure_threshold = 5;                    // Failures before opening circuit
    std::chrono::milliseconds recovery_timeout = std::chrono::milliseconds(60000);  // 1 minute
    size_t success_threshold = 3;                    // Successes needed to close circuit
    std::chrono::milliseconds timeout_duration = std::chrono::milliseconds(30000);   // 30 second timeout
    double slow_call_rate_threshold = 0.5;          // 50% of calls are slow
    std::chrono::milliseconds slow_call_duration = std::chrono::milliseconds(5000);   // 5 seconds
    size_t max_concurrent_requests = 10;            // Max concurrent requests in half-open
    std::chrono::milliseconds metrics_window = std::chrono::milliseconds(600000);     // 10 minutes
    bool enable_metrics = true;
    bool enable_logging = true;

    // Exponential backoff configuration
    double backoff_multiplier = 2.0;
    std::chrono::milliseconds max_backoff_time = std::chrono::milliseconds(300000); // 5 minutes
    double jitter_factor = 0.1; // 10% jitter

    CircuitBreakerConfig(const std::string& circuit_name = "default")
        : name(circuit_name) {}
};

/**
 * @brief Circuit breaker call result
 */
struct CircuitBreakerResult {
    bool success;
    std::optional<nlohmann::json> data;
    std::string error_message;
    std::chrono::milliseconds execution_time;
    CircuitState circuit_state_at_call;

    CircuitBreakerResult(bool s = false,
                        std::optional<nlohmann::json> d = std::nullopt,
                        std::string err = "",
                        std::chrono::milliseconds time = std::chrono::milliseconds(0),
                        CircuitState state = CircuitState::CLOSED)
        : success(s), data(std::move(d)), error_message(std::move(err)),
          execution_time(time), circuit_state_at_call(state) {}
};

/**
 * @brief Circuit Breaker Implementation
 *
 * Production-grade circuit breaker with comprehensive failure handling,
 * recovery mechanisms, and monitoring capabilities.
 */
class CircuitBreaker {
public:
    CircuitBreaker(std::shared_ptr<ConfigurationManager> config,
                  std::string name,
                  StructuredLogger* logger,
                  ErrorHandler* error_handler);

    ~CircuitBreaker();

    /**
     * @brief Initialize the circuit breaker
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Execute a function call through the circuit breaker
     * @param operation Function to execute
     * @return CircuitBreakerResult with execution details
     */
    template<typename Func>
    CircuitBreakerResult execute(Func&& operation) {
        auto start_time = std::chrono::high_resolution_clock::now();

        CircuitState current_state = get_current_state();

        // Check if circuit is open - fail fast
        if (current_state == CircuitState::OPEN) {
            metrics_.rejected_requests++;
            return CircuitBreakerResult(false, std::nullopt,
                                      "Circuit breaker is OPEN - request rejected",
                                      std::chrono::milliseconds(0), current_state);
        }

        // Check concurrent request limits in half-open state
        if (current_state == CircuitState::HALF_OPEN) {
            std::unique_lock<std::mutex> lock(concurrent_mutex_);
            if (active_requests_ >= config_.max_concurrent_requests) {
                metrics_.rejected_requests++;
                return CircuitBreakerResult(false, std::nullopt,
                                          "Circuit breaker is HALF_OPEN - too many concurrent requests",
                                          std::chrono::milliseconds(0), current_state);
            }
            active_requests_++;
            lock.unlock();
        }

        try {
            // Execute the operation
            auto result = operation();

            auto end_time = std::chrono::high_resolution_clock::now();
            auto execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

            // Decrement concurrent requests counter for half-open state
            if (current_state == CircuitState::HALF_OPEN) {
                std::lock_guard<std::mutex> lock(concurrent_mutex_);
                active_requests_--;
            }

            // Check if operation was successful
            if (result.success) {
                on_success(execution_time);
                metrics_.successful_requests++;
                return CircuitBreakerResult(true, result.data, "",
                                          execution_time, current_state);
            } else {
                on_failure(execution_time);
                metrics_.failed_requests++;
                return CircuitBreakerResult(false, std::nullopt, result.error_message,
                                          execution_time, current_state);
            }

        } catch (const std::exception& e) {
            auto end_time = std::chrono::high_resolution_clock::now();
            auto execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

            // Decrement concurrent requests counter for half-open state
            if (current_state == CircuitState::HALF_OPEN) {
                std::lock_guard<std::mutex> lock(concurrent_mutex_);
                active_requests_--;
            }

            on_failure(execution_time);
            metrics_.failed_requests++;

            return CircuitBreakerResult(false, std::nullopt,
                                      std::string("Exception in circuit breaker operation: ") + e.what(),
                                      execution_time, current_state);
        }
    }

    /**
     * @brief Get current circuit breaker state
     * @return Current state
     */
    CircuitState get_current_state() const;

    /**
     * @brief Get circuit breaker configuration
     * @return Current configuration
     */
    const CircuitBreakerConfig& get_config() const { return config_; }

    /**
     * @brief Update circuit breaker configuration
     * @param new_config New configuration
     * @return true if update successful
     */
    bool update_config(const CircuitBreakerConfig& new_config);

    /**
     * @brief Get circuit breaker metrics
     * @return Current metrics snapshot
     */
    CircuitBreakerMetrics get_metrics() const;

    /**
     * @brief Manually open the circuit breaker
     * @return true if operation successful
     */
    bool force_open();

    /**
     * @brief Manually close the circuit breaker
     * @return true if operation successful
     */
    bool force_close();

    /**
     * @brief Reset circuit breaker to initial state
     * @return true if operation successful
     */
    bool reset();

    /**
     * @brief Get circuit breaker health status
     * @return JSON with health information
     */
    nlohmann::json get_health_status() const;

    /**
     * @brief Get circuit breaker name
     * @return Circuit breaker identifier
     */
    const std::string& get_name() const { return config_.name; }

private:
    // Configuration and dependencies
    CircuitBreakerConfig config_;
    std::shared_ptr<ConfigurationManager> config_manager_;
    StructuredLogger* logger_;
    ErrorHandler* error_handler_;

    // State management
    std::atomic<CircuitState> current_state_;
    mutable std::mutex state_mutex_;

    // Failure tracking
    std::atomic<size_t> consecutive_failures_;
    std::atomic<size_t> consecutive_successes_;
    std::chrono::system_clock::time_point last_failure_time_;
    std::chrono::system_clock::time_point next_attempt_time_;

    // Concurrent request management for half-open state
    mutable std::mutex concurrent_mutex_;
    std::atomic<size_t> active_requests_;

    // Metrics
    mutable std::mutex metrics_mutex_;
    CircuitBreakerMetrics metrics_;

    // Exponential backoff tracking
    std::chrono::milliseconds current_backoff_time_;
    size_t backoff_attempt_count_;

    /**
     * @brief Load configuration from configuration manager
     */
    void load_config();

    /**
     * @brief Handle successful operation
     * @param execution_time Time taken for operation
     */
    void on_success(std::chrono::milliseconds execution_time);

    /**
     * @brief Handle failed operation
     * @param execution_time Time taken for operation
     */
    void on_failure(std::chrono::milliseconds execution_time);

    /**
     * @brief Attempt to transition from half-open to closed
     */
    void attempt_recovery();

    /**
     * @brief Calculate next backoff duration with jitter
     * @return Backoff duration
     */
    std::chrono::milliseconds calculate_backoff_duration();

    /**
     * @brief Transition to new state
     * @param new_state New circuit state
     */
    void transition_to_state(CircuitState new_state);

    /**
     * @brief Log state transition
     * @param old_state Previous state
     * @param new_state New state
     * @param reason Reason for transition
     */
    void log_state_transition(CircuitState old_state, CircuitState new_state, const std::string& reason);

    /**
     * @brief Check if recovery timeout has elapsed
     * @return true if ready to attempt recovery
     */
    bool is_recovery_timeout_elapsed() const;

    /**
     * @brief Check if operation is considered slow
     * @param execution_time Operation execution time
     * @return true if operation is slow
     */
    bool is_slow_operation(std::chrono::milliseconds execution_time) const;
};

/**
 * @brief Circuit Breaker Registry for managing multiple circuit breakers
 */
class CircuitBreakerRegistry {
public:
    static CircuitBreakerRegistry& get_instance();

    /**
     * @brief Register a circuit breaker
     * @param breaker Circuit breaker to register
     * @return true if registration successful
     */
    bool register_breaker(std::shared_ptr<CircuitBreaker> breaker);

    /**
     * @brief Get circuit breaker by name
     * @param name Circuit breaker name
     * @return Circuit breaker if found
     */
    std::shared_ptr<CircuitBreaker> get_breaker(const std::string& name) const;

    /**
     * @brief Unregister a circuit breaker
     * @param name Circuit breaker name
     * @return true if unregistration successful
     */
    bool unregister_breaker(const std::string& name);

    /**
     * @brief Get all registered circuit breakers
     * @return Map of circuit breaker names to breakers
     */
    std::unordered_map<std::string, std::shared_ptr<CircuitBreaker>> get_all_breakers() const;

    /**
     * @brief Get registry health status
     * @return JSON with registry health information
     */
    nlohmann::json get_registry_health() const;

private:
    CircuitBreakerRegistry() = default;
    mutable std::mutex registry_mutex_;
    std::unordered_map<std::string, std::shared_ptr<CircuitBreaker>> breakers_;
};

/**
 * @brief Create circuit breaker instance
 * @param config Configuration manager
 * @param name Circuit breaker name
 * @param logger Structured logger
 * @param error_handler Error handler
 * @return Shared pointer to circuit breaker
 */
std::shared_ptr<CircuitBreaker> create_circuit_breaker(
    std::shared_ptr<ConfigurationManager> config,
    const std::string& name,
    StructuredLogger* logger,
    ErrorHandler* error_handler);

} // namespace regulens
