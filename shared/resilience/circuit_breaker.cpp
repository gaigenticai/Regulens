/**
 * Circuit Breaker Pattern Implementation
 *
 * Production-grade circuit breaker with comprehensive failure handling,
 * recovery mechanisms, and enterprise monitoring capabilities.
 */

#include "circuit_breaker.hpp"
#include <algorithm>
#include <random>
#include <cmath>
#include <sstream>

namespace regulens {

// CircuitBreakerMetrics Implementation

nlohmann::json CircuitBreakerMetrics::to_json() const {
    return {
        {"total_requests", total_requests.load()},
        {"successful_requests", successful_requests.load()},
        {"failed_requests", failed_requests.load()},
        {"rejected_requests", rejected_requests.load()},
        {"state_transitions", state_transitions.load()},
        {"recovery_attempts", recovery_attempts.load()},
        {"successful_recoveries", successful_recoveries.load()},
        {"last_failure_time", std::chrono::duration_cast<std::chrono::milliseconds>(
            last_failure_time.time_since_epoch()).count()},
        {"last_state_change_time", std::chrono::duration_cast<std::chrono::milliseconds>(
            last_state_change_time.time_since_epoch()).count()},
        {"created_time", std::chrono::duration_cast<std::chrono::milliseconds>(
            created_time.time_since_epoch()).count()},
        {"uptime_seconds", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - created_time).count()}
    };
}

// CircuitBreaker Implementation

CircuitBreaker::CircuitBreaker(std::shared_ptr<ConfigurationManager> config,
                             std::string name,
                             StructuredLogger* logger,
                             ErrorHandler* error_handler)
    : config_(std::move(name)),
      config_manager_(config),
      logger_(logger),
      error_handler_(error_handler),
      current_state_(CircuitState::CLOSED),
      consecutive_failures_(0),
      consecutive_successes_(0),
      active_requests_(0),
      current_backoff_time_(config_.recovery_timeout),
      backoff_attempt_count_(0) {

    metrics_.last_state_change_time = std::chrono::system_clock::now();
    metrics_.last_failure_time = std::chrono::system_clock::now();
}

CircuitBreaker::~CircuitBreaker() {
    // Ensure clean shutdown
    reset();
}

bool CircuitBreaker::initialize() {
    try {
        load_config();

        if (logger_ && config_.enable_logging) {
            logger_->info("Circuit breaker initialized",
                         "CircuitBreaker", "initialize",
                         {{"circuit_name", config_.name},
                          {"failure_threshold", std::to_string(config_.failure_threshold)},
                          {"recovery_timeout_ms", std::to_string(config_.recovery_timeout.count())}});
        }

        return true;
    } catch (const std::exception& e) {
        if (error_handler_) {
            error_handler_->report_error(ErrorInfo{
                ErrorCategory::INITIALIZATION,
                ErrorSeverity::ERROR,
                "CircuitBreaker",
                "initialize",
                "Failed to initialize circuit breaker: " + std::string(e.what()),
                config_.name
            });
        }
        return false;
    }
}

void CircuitBreaker::load_config() {
    if (!config_manager_) return;

    // Load circuit breaker specific configuration
    std::string config_prefix = "CIRCUIT_BREAKER_" + config_.name + "_";

    config_.failure_threshold = config_manager_->get_int((config_prefix + "FAILURE_THRESHOLD").c_str())
                              .value_or(config_.failure_threshold);

    config_.recovery_timeout = std::chrono::milliseconds(
        config_manager_->get_int((config_prefix + "RECOVERY_TIMEOUT_MS").c_str())
        .value_or(config_.recovery_timeout.count()));

    config_.success_threshold = config_manager_->get_int((config_prefix + "SUCCESS_THRESHOLD").c_str())
                              .value_or(config_.success_threshold);

    config_.timeout_duration = std::chrono::milliseconds(
        config_manager_->get_int((config_prefix + "TIMEOUT_DURATION_MS").c_str())
        .value_or(config_.timeout_duration.count()));

    config_.slow_call_rate_threshold = config_manager_->get_double((config_prefix + "SLOW_CALL_RATE_THRESHOLD").c_str())
                                    .value_or(config_.slow_call_rate_threshold);

    config_.slow_call_duration = std::chrono::milliseconds(
        config_manager_->get_int((config_prefix + "SLOW_CALL_DURATION_MS").c_str())
        .value_or(config_.slow_call_duration.count()));

    config_.max_concurrent_requests = static_cast<size_t>(
        config_manager_->get_int((config_prefix + "MAX_CONCURRENT_REQUESTS").c_str())
        .value_or(static_cast<int>(config_.max_concurrent_requests)));

    config_.backoff_multiplier = config_manager_->get_double((config_prefix + "BACKOFF_MULTIPLIER").c_str())
                               .value_or(config_.backoff_multiplier);

    config_.max_backoff_time = std::chrono::milliseconds(
        config_manager_->get_int((config_prefix + "MAX_BACKOFF_TIME_MS").c_str())
        .value_or(config_.max_backoff_time.count()));

    config_.jitter_factor = config_manager_->get_double((config_prefix + "JITTER_FACTOR").c_str())
                          .value_or(config_.jitter_factor);

    config_.enable_metrics = config_manager_->get_bool((config_prefix + "ENABLE_METRICS").c_str())
                           .value_or(config_.enable_metrics);

    config_.enable_logging = config_manager_->get_bool((config_prefix + "ENABLE_LOGGING").c_str())
                           .value_or(config_.enable_logging);
}

CircuitState CircuitBreaker::get_current_state() const {
    CircuitState state = current_state_.load();

    // Check if we should attempt recovery from OPEN state
    if (state == CircuitState::OPEN && is_recovery_timeout_elapsed()) {
        // Atomically attempt to transition to HALF_OPEN
        CircuitState expected = CircuitState::OPEN;
        if (current_state_.compare_exchange_strong(expected, CircuitState::HALF_OPEN)) {
            transition_to_state(CircuitState::HALF_OPEN);
            metrics_.recovery_attempts++;
            return CircuitState::HALF_OPEN;
        }
    }

    return state;
}

bool CircuitBreaker::update_config(const CircuitBreakerConfig& new_config) {
    try {
        std::lock_guard<std::mutex> lock(state_mutex_);
        config_ = new_config;

        // Reset backoff tracking on config change
        current_backoff_time_ = config_.recovery_timeout;
        backoff_attempt_count_ = 0;

        if (logger_ && config_.enable_logging) {
            logger_->info("Circuit breaker configuration updated",
                         "CircuitBreaker", "update_config",
                         {{"circuit_name", config_.name}});
        }

        return true;
    } catch (const std::exception& e) {
        if (error_handler_) {
            error_handler_->report_error(ErrorInfo{
                ErrorCategory::CONFIGURATION,
                ErrorSeverity::ERROR,
                "CircuitBreaker",
                "update_config",
                "Failed to update circuit breaker config: " + std::string(e.what()),
                config_.name
            });
        }
        return false;
    }
}

CircuitBreakerMetrics CircuitBreaker::get_metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return metrics_;
}

bool CircuitBreaker::force_open() {
    transition_to_state(CircuitState::OPEN);
    return true;
}

bool CircuitBreaker::force_close() {
    transition_to_state(CircuitState::CLOSED);
    consecutive_failures_ = 0;
    consecutive_successes_ = 0;
    current_backoff_time_ = config_.recovery_timeout;
    backoff_attempt_count_ = 0;
    return true;
}

bool CircuitBreaker::reset() {
    force_close();
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_ = CircuitBreakerMetrics();
    return true;
}

nlohmann::json CircuitBreaker::get_health_status() const {
    auto metrics = get_metrics();
    CircuitState state = get_current_state();

    double failure_rate = metrics.total_requests > 0 ?
        static_cast<double>(metrics.failed_requests) / metrics.total_requests : 0.0;

    double success_rate = metrics.total_requests > 0 ?
        static_cast<double>(metrics.successful_requests) / metrics.total_requests : 0.0;

    return {
        {"circuit_name", config_.name},
        {"state", state == CircuitState::CLOSED ? "CLOSED" :
                 state == CircuitState::OPEN ? "OPEN" : "HALF_OPEN"},
        {"failure_rate", failure_rate},
        {"success_rate", success_rate},
        {"total_requests", metrics.total_requests},
        {"consecutive_failures", consecutive_failures_.load()},
        {"consecutive_successes", consecutive_successes_.load()},
        {"active_requests", active_requests_.load()},
        {"is_healthy", state == CircuitState::CLOSED && failure_rate < 0.5},
        {"last_failure_time", std::chrono::duration_cast<std::chrono::milliseconds>(
            metrics.last_failure_time.time_since_epoch()).count()},
        {"next_recovery_attempt", state == CircuitState::OPEN ?
            std::chrono::duration_cast<std::chrono::milliseconds>(
                next_attempt_time_.time_since_epoch()).count() : 0}
    };
}

void CircuitBreaker::on_success(std::chrono::milliseconds execution_time) {
    metrics_.total_requests++;

    // Check if operation was slow
    if (is_slow_operation(execution_time)) {
        // Could implement slow call rate tracking here for future enhancements
    }

    CircuitState current_state = current_state_.load();

    if (current_state == CircuitState::HALF_OPEN) {
        size_t successes = ++consecutive_successes_;
        if (successes >= config_.success_threshold) {
            transition_to_state(CircuitState::CLOSED);
            consecutive_failures_ = 0;
            consecutive_successes_ = 0;
            current_backoff_time_ = config_.recovery_timeout;
            backoff_attempt_count_ = 0;
            metrics_.successful_recoveries++;
        }
    } else if (current_state == CircuitState::CLOSED) {
        // Reset failure counter on success
        consecutive_failures_ = 0;
    }
}

void CircuitBreaker::on_failure(std::chrono::milliseconds execution_time) {
    metrics_.total_requests++;
    metrics_.last_failure_time = std::chrono::system_clock::now();

    size_t failures = ++consecutive_failures_;

    CircuitState current_state = current_state_.load();

    if (current_state == CircuitState::HALF_OPEN) {
        // Any failure in half-open state immediately opens the circuit
        transition_to_state(CircuitState::OPEN);
        consecutive_successes_ = 0;
        next_attempt_time_ = std::chrono::system_clock::now() + calculate_backoff_duration();
    } else if (current_state == CircuitState::CLOSED && failures >= config_.failure_threshold) {
        transition_to_state(CircuitState::OPEN);
        next_attempt_time_ = std::chrono::system_clock::now() + calculate_backoff_duration();
    }
}

void CircuitBreaker::transition_to_state(CircuitState new_state) {
    CircuitState old_state = current_state_.exchange(new_state);

    if (old_state != new_state) {
        metrics_.state_transitions++;
        metrics_.last_state_change_time = std::chrono::system_clock::now();

        std::string reason;
        switch (new_state) {
            case CircuitState::OPEN:
                reason = "Failure threshold exceeded";
                break;
            case CircuitState::HALF_OPEN:
                reason = "Recovery timeout elapsed";
                break;
            case CircuitState::CLOSED:
                reason = "Success threshold reached in half-open state";
                break;
        }

        log_state_transition(old_state, new_state, reason);
    }
}

void CircuitBreaker::log_state_transition(CircuitState old_state, CircuitState new_state, const std::string& reason) {
    if (logger_ && config_.enable_logging) {
        std::string old_state_str = old_state == CircuitState::CLOSED ? "CLOSED" :
                                   old_state == CircuitState::OPEN ? "OPEN" : "HALF_OPEN";
        std::string new_state_str = new_state == CircuitState::CLOSED ? "CLOSED" :
                                   new_state == CircuitState::OPEN ? "OPEN" : "HALF_OPEN";

        logger_->info("Circuit breaker state transition",
                     "CircuitBreaker", "transition_to_state",
                     {{"circuit_name", config_.name},
                      {"old_state", old_state_str},
                      {"new_state", new_state_str},
                      {"reason", reason},
                      {"consecutive_failures", std::to_string(consecutive_failures_.load())},
                      {"consecutive_successes", std::to_string(consecutive_successes_.load())}});
    }
}

std::chrono::milliseconds CircuitBreaker::calculate_backoff_duration() {
    // Exponential backoff with jitter
    std::chrono::milliseconds base_backoff = current_backoff_time_;

    // Apply exponential backoff
    double multiplier = std::pow(config_.backoff_multiplier, backoff_attempt_count_);
    std::chrono::milliseconds calculated_backoff = base_backoff * multiplier;

    // Cap at maximum backoff time
    calculated_backoff = std::min(calculated_backoff, config_.max_backoff_time);

    // Add jitter (±10% by default)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-config_.jitter_factor, config_.jitter_factor);
    double jitter = 1.0 + dis(gen);
    calculated_backoff = std::chrono::milliseconds(
        static_cast<long long>(calculated_backoff.count() * jitter));

    // Ensure minimum backoff
    calculated_backoff = std::max(calculated_backoff, std::chrono::milliseconds(1000));

    backoff_attempt_count_++;
    current_backoff_time_ = calculated_backoff;

    return calculated_backoff;
}

bool CircuitBreaker::is_recovery_timeout_elapsed() const {
    return std::chrono::system_clock::now() >= next_attempt_time_;
}

bool CircuitBreaker::is_slow_operation(std::chrono::milliseconds execution_time) const {
    return execution_time > config_.slow_call_duration;
}

// CircuitBreakerRegistry Implementation

CircuitBreakerRegistry& CircuitBreakerRegistry::get_instance() {
    static CircuitBreakerRegistry instance;
    return instance;
}

bool CircuitBreakerRegistry::register_breaker(std::shared_ptr<CircuitBreaker> breaker) {
    if (!breaker) return false;

    std::lock_guard<std::mutex> lock(registry_mutex_);
    const std::string& name = breaker->get_name();

    if (breakers_.find(name) != breakers_.end()) {
        return false; // Already registered
    }

    breakers_[name] = breaker;
    return true;
}

std::shared_ptr<CircuitBreaker> CircuitBreakerRegistry::get_breaker(const std::string& name) const {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    auto it = breakers_.find(name);
    return (it != breakers_.end()) ? it->second : nullptr;
}

bool CircuitBreakerRegistry::unregister_breaker(const std::string& name) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    return breakers_.erase(name) > 0;
}

std::unordered_map<std::string, std::shared_ptr<CircuitBreaker>> CircuitBreakerRegistry::get_all_breakers() const {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    return breakers_;
}

nlohmann::json CircuitBreakerRegistry::get_registry_health() const {
    auto breakers = get_all_breakers();

    nlohmann::json health = {
        {"total_circuits", breakers.size()},
        {"circuits", nlohmann::json::array()},
        {"healthy_circuits", 0},
        {"unhealthy_circuits", 0}
    };

    for (const auto& [name, breaker] : breakers) {
        auto circuit_health = breaker->get_health_status();
        health["circuits"].push_back(circuit_health);

        if (circuit_health["is_healthy"]) {
            health["healthy_circuits"] = health["healthy_circuits"].get<int>() + 1;
        } else {
            health["unhealthy_circuits"] = health["unhealthy_circuits"].get<int>() + 1;
        }
    }

    return health;
}

// Factory function

std::shared_ptr<CircuitBreaker> create_circuit_breaker(
    std::shared_ptr<ConfigurationManager> config,
    const std::string& name,
    StructuredLogger* logger,
    ErrorHandler* error_handler) {

    auto breaker = std::make_shared<CircuitBreaker>(config, name, logger, error_handler);
    if (breaker->initialize()) {
        CircuitBreakerRegistry::get_instance().register_breaker(breaker);
        return breaker;
    }
    return nullptr;
}

} // namespace regulens
