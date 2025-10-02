#include "error_handler.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <random>

namespace regulens {

ErrorHandler::ErrorHandler(std::shared_ptr<ConfigurationManager> config,
                         std::shared_ptr<StructuredLogger> logger)
    : config_manager_(config), logger_(logger), running_(false),
      total_errors_processed_(0), total_recovery_attempts_(0), total_successful_recoveries_(0) {
    // Load configuration from environment
    config_.enable_error_logging = config_manager_->get_bool("ERROR_ENABLE_LOGGING").value_or(true);
    config_.enable_error_alerts = config_manager_->get_bool("ERROR_ENABLE_ALERTS").value_or(true);
    config_.max_errors_per_minute = static_cast<int>(config_manager_->get_int("ERROR_MAX_PER_MINUTE").value_or(10));
    config_.error_retention_period = std::chrono::hours(
        config_manager_->get_int("ERROR_RETENTION_HOURS").value_or(24));

    // Circuit breaker configuration
    config_.circuit_breaker_failure_threshold = static_cast<int>(
        config_manager_->get_int("ERROR_CIRCUIT_BREAKER_FAILURE_THRESHOLD").value_or(5));
    config_.circuit_breaker_timeout_seconds = static_cast<int>(
        config_manager_->get_int("ERROR_CIRCUIT_BREAKER_TIMEOUT_SECONDS").value_or(60));
    config_.circuit_breaker_success_threshold = static_cast<int>(
        config_manager_->get_int("ERROR_CIRCUIT_BREAKER_SUCCESS_THRESHOLD").value_or(3));

    logger_->info("ErrorHandler initialized with retention: " +
                 std::to_string(config_.error_retention_period.count()) + " hours");
}

ErrorHandler::~ErrorHandler() {
    shutdown();
}

bool ErrorHandler::initialize() {
    logger_->info("Initializing ErrorHandler");

    running_ = true;

    // Initialize default circuit breakers for common services
    initialize_default_circuit_breakers();

    // Initialize default fallback configurations
    initialize_default_fallback_configs();

    // Start cleanup worker thread
    cleanup_thread_ = std::thread(&ErrorHandler::cleanup_worker, this);

    logger_->info("ErrorHandler initialization complete");
    return true;
}

void ErrorHandler::shutdown() {
    if (!running_) return;

    logger_->info("Shutting down ErrorHandler");

    running_ = false;

    // Wake up cleanup thread
    {
        std::unique_lock<std::mutex> lock(cleanup_cv_mutex_);
        cleanup_cv_.notify_one();
    }

    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }

    logger_->info("ErrorHandler shutdown complete");
}

std::string ErrorHandler::report_error(const ErrorInfo& error) {
    std::lock_guard<std::mutex> lock(error_mutex_);

    // Add to error history
    error_history_.push_back(error);

    // Enforce history size limit
    while (error_history_.size() > 10000) { // Keep last 10k errors
        error_history_.pop_front();
    }

    total_errors_processed_++;

    // Log the error
    if (config_.enable_error_logging) {
        std::string severity_str;
        switch (error.severity) {
            case ErrorSeverity::LOW: severity_str = "LOW"; break;
            case ErrorSeverity::MEDIUM: severity_str = "MEDIUM"; break;
            case ErrorSeverity::HIGH: severity_str = "HIGH"; break;
            case ErrorSeverity::CRITICAL: severity_str = "CRITICAL"; break;
        }

        logger_->error("Error reported - Component: {}, Operation: {}, Severity: {}, Message: {}",
                      error.component, "", "", {{"operation", error.operation}, {"severity", severity_str}, {"message", error.message}});
    }

    // Check for alerts
    if (config_.enable_error_alerts && should_alert_on_error(error)) {
        check_error_rate_limits();
        send_error_alerts(error);
    }

    // Update component health
    update_component_health(error.component, false, error.message);

    // Analyze error patterns periodically
    if (total_errors_processed_ % 100 == 0) { // Every 100 errors
        analyze_error_patterns();
    }

    return error.error_id;
}

HealthStatus ErrorHandler::get_component_health(const std::string& component_name) {
    std::lock_guard<std::mutex> lock(error_mutex_);

    auto it = component_health_.find(component_name);
    if (it != component_health_.end()) {
        return it->second.status;
    }

    return HealthStatus::UNKNOWN;
}

HealthStatus ErrorHandler::perform_health_check(const std::string& component_name,
                                              std::function<bool()> health_check) {
    try {
        bool is_healthy = health_check();
        update_component_health(component_name, is_healthy,
                              is_healthy ? "Health check passed" : "Health check failed");
        return is_healthy ? HealthStatus::HEALTHY : HealthStatus::UNHEALTHY;
    } catch (const std::exception& e) {
        update_component_health(component_name, false, std::string("Health check exception: ") + e.what());
        return HealthStatus::UNHEALTHY;
    }
}

std::optional<CircuitBreaker> ErrorHandler::get_circuit_breaker(const std::string& service_name) {
    std::lock_guard<std::mutex> lock(error_mutex_);

    auto it = circuit_breakers_.find(service_name);
    if (it != circuit_breakers_.end()) {
        return it->second;
    }

    return std::nullopt;
}

bool ErrorHandler::reset_circuit_breaker(const std::string& service_name) {
    std::lock_guard<std::mutex> lock(error_mutex_);

    auto it = circuit_breakers_.find(service_name);
    if (it != circuit_breakers_.end()) {
        it->second.state = CircuitState::CLOSED;
        it->second.failure_count = 0;
        it->second.success_count = 0;
        logger_->info("Manually reset circuit breaker for service: {}", service_name);
        return true;
    }

    return false;
}

std::optional<FallbackConfig> ErrorHandler::get_fallback_config(const std::string& component_name) {
    std::lock_guard<std::mutex> lock(error_mutex_);

    auto it = fallback_configs_.find(component_name);
    if (it != fallback_configs_.end()) {
        return it->second;
    }

    return std::nullopt;
}

bool ErrorHandler::set_fallback_config(const FallbackConfig& config) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    fallback_configs_[config.component_name] = config;
    logger_->info("Updated fallback config for component: {}", config.component_name);
    return true;
}

nlohmann::json ErrorHandler::get_error_stats() {
    std::lock_guard<std::mutex> lock(error_mutex_);

    std::unordered_map<int, size_t> error_severity_counts;
    std::unordered_map<int, size_t> error_category_counts;
    std::unordered_map<std::string, size_t> component_error_counts;

    for (const auto& error : error_history_) {
        error_severity_counts[static_cast<int>(error.severity)]++;
        error_category_counts[static_cast<int>(error.category)]++;
        component_error_counts[error.component]++;
    }

    return {
        {"total_errors", static_cast<size_t>(total_errors_processed_)},
        {"total_recovery_attempts", static_cast<size_t>(total_recovery_attempts_)},
        {"total_successful_recoveries", static_cast<size_t>(total_successful_recoveries_)},
        {"current_error_history_size", error_history_.size()},
        {"error_severity_distribution", error_severity_counts},
        {"error_category_distribution", error_category_counts},
        {"component_error_counts", component_error_counts},
        {"config", config_.to_json()}
    };
}

nlohmann::json ErrorHandler::get_health_dashboard() {
    std::lock_guard<std::mutex> lock(error_mutex_);

    nlohmann::json health_data = nlohmann::json::array();

    for (const auto& [name, health] : component_health_) {
        health_data.push_back(health.to_json());
    }

    nlohmann::json circuit_breaker_data = nlohmann::json::array();
    for (const auto& [name, breaker] : circuit_breakers_) {
        circuit_breaker_data.push_back(breaker.to_json());
    }

    return {
        {"components", health_data},
        {"circuit_breakers", circuit_breaker_data},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };
}

nlohmann::json ErrorHandler::export_error_data(const std::string& component_filter, int hours_back) {
    std::lock_guard<std::mutex> lock(error_mutex_);

    auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours(hours_back);

    nlohmann::json export_data = nlohmann::json::array();

    for (const auto& error : error_history_) {
        if (error.timestamp >= cutoff_time &&
            (component_filter.empty() || error.component == component_filter)) {
            export_data.push_back(error.to_json());
        }
    }

    return export_data;
}

size_t ErrorHandler::cleanup_old_errors() {
    std::lock_guard<std::mutex> lock(error_mutex_);

    auto cutoff_time = std::chrono::system_clock::now() - config_.error_retention_period;
    size_t removed_count = 0;

    // Remove old errors from history
    while (!error_history_.empty() && error_history_.front().timestamp < cutoff_time) {
        error_history_.pop_front();
        removed_count++;
    }

    // Clean up old component health data (reset metrics for components not seen recently)
    auto health_cutoff = std::chrono::system_clock::now() - std::chrono::hours(1);
    for (auto& [name, health] : component_health_) {
        if (health.last_check < health_cutoff) {
            health.status = HealthStatus::UNKNOWN;
            health.consecutive_failures = 0;
        }
    }

    logger_->info("Cleaned up " + std::to_string(removed_count) + " old error records");
    return removed_count;
}

// Private method implementations

RecoveryStrategy ErrorHandler::get_recovery_strategy(const ErrorInfo& error) {
    // Check if component has specific fallback config
    auto fallback_config = get_fallback_config(error.component);
    if (fallback_config && fallback_config->enable_fallback) {
        return RecoveryStrategy::FALLBACK;
    }

    // Use default strategy based on error category
    auto it = config_.default_strategies.find(error.category);
    if (it != config_.default_strategies.end()) {
        return it->second;
    }

    return RecoveryStrategy::IGNORE;
}

bool ErrorHandler::should_retry_error(const ErrorInfo& error, const RetryConfig& config) {
    return std::find(config.retryable_errors.begin(), config.retryable_errors.end(), error.category)
           != config.retryable_errors.end();
}

std::chrono::milliseconds ErrorHandler::calculate_retry_delay(int attempt, const RetryConfig& config) {
    // Exponential backoff with jitter
    double delay_ms = config.initial_delay.count() * std::pow(config.backoff_multiplier, attempt);

    // Add random jitter (±25%)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.75, 1.25);
    delay_ms *= dis(gen);

    // Cap at maximum delay
    delay_ms = std::min(delay_ms, static_cast<double>(config.max_delay.count()));

    return std::chrono::milliseconds(static_cast<long long>(delay_ms));
}

CircuitBreaker& ErrorHandler::get_or_create_circuit_breaker(const std::string& service_name) {
    std::lock_guard<std::mutex> lock(error_mutex_);

    auto it = circuit_breakers_.find(service_name);
    if (it == circuit_breakers_.end()) {
        // Create default circuit breaker
        auto result = circuit_breakers_.emplace(service_name, CircuitBreaker("cb_" + service_name, service_name));
        return result.first->second;
    }

    return it->second;
}

void ErrorHandler::update_component_health(const std::string& component_name, bool success,
                                         const std::string& status_message) {
    std::lock_guard<std::mutex> lock(error_mutex_);

    auto it = component_health_.find(component_name);
    if (it == component_health_.end()) {
        auto result = component_health_.emplace(component_name, ComponentHealth(component_name));
        it = result.first;
    }

    if (success) {
        it->second.record_success();
    } else {
        it->second.record_failure(status_message);
    }
}

void ErrorHandler::analyze_error_patterns() {
    // Analyze recent error patterns to identify trends
    auto recent_errors = export_error_data("", 1); // Last hour

    if (recent_errors.size() < 5) return; // Not enough data

    std::unordered_map<std::string, int> component_errors;
    std::unordered_map<int, int> category_errors;

    for (const auto& error_json : recent_errors) {
        std::string component = error_json["component"];
        int category = error_json["category"];
        component_errors[component]++;
        category_errors[category]++;
    }

    // Check for components with high error rates
    for (const auto& [component, count] : component_errors) {
        if (count > 10) { // More than 10 errors in last hour
            logger_->warn("High error rate detected for component {}: {} errors in last hour",
                         component, "", "", {{"error_count", std::to_string(count)}});
        }
    }

    // Log pattern analysis summary
    logger_->info("Error pattern analysis: {} components, {} total errors in last hour",
                 "", "", "", {{"component_count", std::to_string(component_errors.size())},
                              {"total_errors", std::to_string(recent_errors.size())}});
}

void ErrorHandler::check_error_rate_limits() {
    // Simple rate limiting - count errors in last minute
    auto cutoff_time = std::chrono::system_clock::now() - std::chrono::minutes(1);
    size_t recent_errors = 0;

    {
        std::lock_guard<std::mutex> lock(error_mutex_);
        for (const auto& error : error_history_) {
            if (error.timestamp >= cutoff_time) {
                recent_errors++;
            } else {
                break; // Since errors are in chronological order
            }
        }
    }

    if (recent_errors > static_cast<size_t>(config_.max_errors_per_minute)) {
        logger_->warn("Error rate limit exceeded: {} errors in last minute (limit: {})",
                     "", "", "", {{"error_count", std::to_string(recent_errors)},
                                  {"limit", std::to_string(config_.max_errors_per_minute)}});
    }
}

void ErrorHandler::send_error_alerts(const ErrorInfo& error) {
    // In a production system, this would send alerts via email, Slack, etc.
    // For now, just log critical errors
    if (error.severity == ErrorSeverity::CRITICAL) {
        logger_->error("🚨 CRITICAL ERROR ALERT 🚨");
        logger_->error("Component: {}", error.component);
        logger_->error("Operation: {}", error.operation);
        logger_->error("Message: {}", error.message);
        logger_->error("Details: {}", error.details);
    }
}

bool ErrorHandler::is_component_critical(const std::string& component_name) {
    // Define critical components that require immediate attention
    static const std::unordered_set<std::string> critical_components = {
        "database", "authentication", "security", "monitoring"
    };

    return critical_components.find(component_name) != critical_components.end();
}

ErrorSeverity ErrorHandler::calculate_error_severity(const ErrorInfo& error) {
    // Base severity on error category and component criticality
    ErrorSeverity base_severity = error.severity;

    if (is_component_critical(error.component)) {
        // Increase severity for critical components
        switch (base_severity) {
            case ErrorSeverity::LOW: return ErrorSeverity::MEDIUM;
            case ErrorSeverity::MEDIUM: return ErrorSeverity::HIGH;
            case ErrorSeverity::HIGH: return ErrorSeverity::CRITICAL;
            default: return base_severity;
        }
    }

    return base_severity;
}

bool ErrorHandler::should_alert_on_error(const ErrorInfo& error) {
    return error.severity >= ErrorSeverity::HIGH || is_component_critical(error.component);
}

void ErrorHandler::cleanup_worker() {
    logger_->info("Error handler cleanup worker started");

    while (running_) {
        std::unique_lock<std::mutex> lock(cleanup_cv_mutex_);

        // Wait for cleanup interval or shutdown
        cleanup_cv_.wait_for(lock, std::chrono::hours(1)); // Clean up every hour

        if (!running_) break;

        try {
            cleanup_old_errors();
        } catch (const std::exception& e) {
            logger_->error("Error in cleanup worker: {}", e.what());
        }
    }

    logger_->info("Error handler cleanup worker stopped");
}

void ErrorHandler::initialize_default_circuit_breakers() {
    // Initialize circuit breakers for common external services
    std::vector<std::string> services = {
        "openai_api", "anthropic_api", "database", "vector_db",
        "email_service", "external_monitoring", "regulatory_api"
    };

    for (const auto& service : services) {
        CircuitBreaker breaker("cb_" + service, service);
        circuit_breakers_[service] = breaker;
    }

    logger_->info("Initialized {} default circuit breakers", "", "", {{"count", std::to_string(services.size())}});
}

void ErrorHandler::initialize_default_fallback_configs() {
    // Initialize fallback configurations for common components
    std::vector<std::pair<std::string, std::string>> component_strategies = {
        {"llm_service", "circuit_breaker"},
        {"database", "retry"},
        {"vector_search", "circuit_breaker"},
        {"email_service", "retry"},
        {"external_api", "circuit_breaker"}
    };

    for (const auto& [component, strategy] : component_strategies) {
        FallbackConfig config(component);
        config.fallback_strategy = strategy;
        fallback_configs_[component] = config;
    }

    logger_->info("Initialized {} default fallback configurations", "", "", {{"count", std::to_string(component_strategies.size())}});
}

bool ErrorHandler::is_circuit_open(const std::string& component_name) {
    std::lock_guard<std::mutex> lock(circuit_breaker_mutex_);

    auto it = circuit_breakers_.find(component_name);
    if (it == circuit_breakers_.end()) {
        // Initialize circuit breaker for new component
        CircuitBreakerState state;
        state.failure_count = 0;
        state.last_failure_time = std::chrono::system_clock::time_point::min();
        state.state = CircuitBreakerState::CLOSED;
        circuit_breakers_[component_name] = state;
        return false;
    }

    auto& state = it->second;

    // Check if circuit should transition from OPEN to HALF_OPEN
    if (state.state == CircuitBreakerState::OPEN) {
        auto now = std::chrono::system_clock::now();
        auto time_since_failure = std::chrono::duration_cast<std::chrono::seconds>(
            now - state.last_failure_time).count();

        if (time_since_failure >= config_.circuit_breaker_timeout_seconds) {
            state.state = CircuitBreakerState::HALF_OPEN;
            state.failure_count = 0;
            logger_->info("Circuit breaker for {} transitioned to HALF_OPEN", component_name);
            return false; // Allow one request through
        }
        return true; // Still open
    }

    return false; // Closed or half-open
}

void ErrorHandler::record_success(const std::string& component_name) {
    std::lock_guard<std::mutex> lock(circuit_breaker_mutex_);

    auto it = circuit_breakers_.find(component_name);
    if (it != circuit_breakers_.end()) {
        auto& state = it->second;

        if (state.state == CircuitBreakerState::HALF_OPEN) {
            // Success in half-open state - close the circuit
            state.state = CircuitBreakerState::CLOSED;
            state.failure_count = 0;
            logger_->info("Circuit breaker for {} closed after successful operation", component_name);
        }
        // In CLOSED state, success is normal - no action needed
    }
}

void ErrorHandler::record_failure(const std::string& component_name) {
    std::lock_guard<std::mutex> lock(circuit_breaker_mutex_);

    auto it = circuit_breakers_.find(component_name);
    if (it == circuit_breakers_.end()) {
        // Initialize if not exists
        CircuitBreakerState state;
        state.failure_count = 1;
        state.last_failure_time = std::chrono::system_clock::now();
        state.state = CircuitBreakerState::CLOSED;
        circuit_breakers_[component_name] = state;
        return;
    }

    auto& state = it->second;
    state.failure_count++;
    state.last_failure_time = std::chrono::system_clock::now();

    // Check if circuit should open
    if (state.state == CircuitBreakerState::CLOSED &&
        state.failure_count >= config_.circuit_breaker_failure_threshold) {
        state.state = CircuitBreakerState::OPEN;
        logger_->warn("Circuit breaker for {} opened after {} failures", component_name, state.failure_count);
    } else if (state.state == CircuitBreakerState::HALF_OPEN) {
        // Failure in half-open state - go back to open
        state.state = CircuitBreakerState::OPEN;
        logger_->warn("Circuit breaker for {} returned to OPEN after failure in HALF_OPEN state", component_name);
    }
}

void ErrorHandler::update_error_statistics(const ErrorInfo& error, bool recovered) {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    total_recovery_attempts_++;
    if (recovered) {
        total_successful_recoveries_++;
    }
}

} // namespace regulens
