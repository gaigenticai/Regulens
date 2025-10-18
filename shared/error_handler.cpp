#include "error_handler.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <random>
#include "../network/http_client.hpp"

namespace regulens {


ErrorHandler::~ErrorHandler() {
    // Cleanup circuit breakers
    circuit_breakers_.clear();
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

std::shared_ptr<CircuitBreaker> ErrorHandler::get_circuit_breaker(const std::string& service_name) {
    std::lock_guard<std::mutex> lock(circuit_breaker_mutex_);

    auto it = circuit_breakers_.find(service_name);
    if (it != circuit_breakers_.end()) {
        return it->second;
    }

    return nullptr;
}


bool ErrorHandler::reset_circuit_breaker(const std::string& service_name) {
    std::lock_guard<std::mutex> lock(circuit_breaker_mutex_);
    auto it = circuit_breakers_.find(service_name);
    if (it != circuit_breakers_.end()) {
        it->second->force_close();
        logger_->info("Manually reset circuit breaker for service: " + service_name);
        return true;
    }
    return false;
}

std::string ErrorHandler::report_error(const ErrorInfo& error) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    error_history_.push_back(error);
    
    // Keep log bounded to prevent memory issues
    if (error_history_.size() > 10000) {
        error_history_.pop_front();
    }
    
    if (logger_) {
        logger_->error("Error reported: {} - {}", error.error_id, error.message);
    }
    
    return error.error_id;
}

void ErrorHandler::update_component_health(const std::string& component_name, bool success,
                                          const std::string& message) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    
    auto& health = component_health_[component_name];
    health.status = success ? HealthStatus::HEALTHY : HealthStatus::UNHEALTHY;
    health.last_check = std::chrono::system_clock::now();
    
    if (logger_) {
        logger_->info("Component " + component_name + " health update: " +
                     (success ? "HEALTHY" : "UNHEALTHY") + " - " + message);
    }
}

std::optional<FallbackConfig> ErrorHandler::get_fallback_config(const std::string& component_name) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    
    // Return default fallback config for now
    FallbackConfig config(component_name);
    config.enable_fallback = true;
    config.fallback_strategy = "default";
    
    return config;
}

bool ErrorHandler::is_circuit_open(const std::string& component_name) {
    auto breaker = get_circuit_breaker(component_name);
    if (!breaker) {
        return false;
    }
    
    auto state = breaker->get_current_state();
    return state == CircuitState::OPEN;
}

std::shared_ptr<CircuitBreaker> ErrorHandler::get_or_create_circuit_breaker(const std::string& service_name) {
    std::lock_guard<std::mutex> lock(circuit_breaker_mutex_);
    
    auto it = circuit_breakers_.find(service_name);
    if (it != circuit_breakers_.end()) {
        return it->second;
    }
    
    // Create a new circuit breaker with default configuration
    auto breaker = std::make_shared<CircuitBreaker>(
        config_manager_.get(), service_name, logger_.get(), this);
    circuit_breakers_[service_name] = breaker;
    
    if (logger_) {
        logger_->info("Created circuit breaker for service: {}", service_name);
    }
    
    return breaker;
}

bool ErrorHandler::clear_error_history(const std::string& component_filter, int hours_back) {
    try {
        std::lock_guard<std::mutex> lock(error_mutex_);

        auto now = std::chrono::system_clock::now();
        auto cutoff_time = (hours_back > 0) ?
            now - std::chrono::hours(hours_back) :
            std::chrono::system_clock::time_point::min(); // Clear all if hours_back is 0

        size_t cleared_count = 0;

        // Clear from error history deque
        auto it = error_history_.begin();
        while (it != error_history_.end()) {
            bool should_clear = true;

            // Check time filter
            if (it->timestamp >= cutoff_time) {
                should_clear = false;
            }

            // Check component filter
            if (!component_filter.empty() && it->component != component_filter) {
                should_clear = false;
            }

            if (should_clear) {
                it = error_history_.erase(it);
                cleared_count++;
            } else {
                ++it;
            }
        }

        // Reset component health metrics for filtered components
        if (!component_filter.empty()) {
            auto health_it = component_health_.find(component_filter);
            if (health_it != component_health_.end()) {
                // Reset health metrics for the specific component
                health_it->second.consecutive_failures = 0;
                health_it->second.status = HealthStatus::HEALTHY;
                health_it->second.status_message = "Error history cleared";
            }
        } else {
            // Clear all component health if no filter
            for (auto& health_pair : component_health_) {
                health_pair.second.consecutive_failures = 0;
                health_pair.second.status = HealthStatus::HEALTHY;
                health_pair.second.status_message = "Error history cleared";
            }
        }

        if (logger_) {
            logger_->info("Cleared " + std::to_string(cleared_count) + " error records: component_filter='" +
                         component_filter + "', hours_back=" + std::to_string(hours_back));
        }

        return true;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception clearing error history: {}", e.what());
        }
        return false;
    }
}

} // namespace regulens
