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

} // namespace regulens
