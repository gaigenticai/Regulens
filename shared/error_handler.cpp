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
        logger_->info("Component {} health update: {} - {}", component_name,
                     success ? "HEALTHY" : "UNHEALTHY", message);
    }
}

std::optional<FallbackConfig> ErrorHandler::get_fallback_config(const std::string& component_name) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    
    // Return default fallback config for now
    FallbackConfig config;
    config.use_fallback = true;
    config.fallback_timeout_ms = 5000;
    config.fallback_priority = FallbackPriority::HIGH;
    
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
    auto breaker = std::make_shared<CircuitBreaker>(service_name);
    circuit_breakers_[service_name] = breaker;
    
    if (logger_) {
        logger_->info("Created circuit breaker for service: {}", service_name);
    }
    
    return breaker;
}

} // namespace regulens
