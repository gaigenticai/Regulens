/**
 * Health Check Handlers - Kubernetes Probes Implementation
 *
 * Enterprise-grade health check handlers for Kubernetes readiness, liveness,
 * and startup probes with comprehensive service health monitoring.
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <atomic>
#include <nlohmann/json.hpp>

#include "../logging/structured_logger.hpp"
#include "../config/configuration_manager.hpp"
#include "../error_handler.hpp"
#include "../metrics/prometheus_metrics.hpp"

namespace regulens {

/**
 * @brief Health check result
 */
struct HealthCheckResult {
    bool healthy{true};
    std::string status{"healthy"};
    std::string message{"Service is operating normally"};
    nlohmann::json details{};
    std::chrono::system_clock::time_point timestamp{std::chrono::system_clock::now()};

    nlohmann::json to_json() const {
        return {
            {"healthy", healthy},
            {"status", status},
            {"message", message},
            {"details", details},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp.time_since_epoch()).count()}
        };
    }
};

/**
 * @brief Health check function type
 */
using HealthCheckFunction = std::function<HealthCheckResult()>;

/**
 * @brief Health probe type
 */
enum class HealthProbeType {
    READINESS,    // Service ready to serve requests
    LIVENESS,     // Service alive and should not be restarted
    STARTUP       // Service has finished starting
};

/**
 * @brief Health check handler for Kubernetes probes
 *
 * Provides comprehensive health checking for different service types
 * with configurable checks and probe endpoints.
 */
class HealthCheckHandler {
public:
    HealthCheckHandler(std::shared_ptr<ConfigurationManager> config,
                      std::shared_ptr<StructuredLogger> logger,
                      std::shared_ptr<ErrorHandler> error_handler,
                      std::shared_ptr<PrometheusMetricsCollector> metrics = nullptr);

    ~HealthCheckHandler() = default;

    /**
     * @brief Initialize health check handler
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Register a health check
     * @param name Unique name for the health check
     * @param check_function Function that performs the health check
     * @param critical Whether this check affects overall service health
     * @param probe_types Which probe types this check applies to
     */
    void register_health_check(const std::string& name,
                              HealthCheckFunction check_function,
                              bool critical = true,
                              const std::vector<HealthProbeType>& probe_types = {HealthProbeType::READINESS, HealthProbeType::LIVENESS});

    /**
     * @brief Unregister a health check
     * @param name Health check name
     */
    void unregister_health_check(const std::string& name);

    /**
     * @brief Perform readiness probe
     * @return HTTP status code and response
     */
    std::pair<int, std::string> readiness_probe();

    /**
     * @brief Perform liveness probe
     * @return HTTP status code and response
     */
    std::pair<int, std::string> liveness_probe();

    /**
     * @brief Perform startup probe
     * @return HTTP status code and response
     */
    std::pair<int, std::string> startup_probe();

    /**
     * @brief Get detailed health status
     * @return JSON health status
     */
    nlohmann::json get_detailed_health();

    /**
     * @brief Get health metrics
     * @return JSON metrics
     */
    nlohmann::json get_health_metrics();

private:
    std::shared_ptr<ConfigurationManager> config_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<ErrorHandler> error_handler_;
    std::shared_ptr<PrometheusMetricsCollector> metrics_;

    // Health check storage
    struct HealthCheckInfo {
        std::string name;
        HealthCheckFunction function;
        bool critical;
        std::vector<HealthProbeType> probe_types;
        std::atomic<size_t> executions{0};
        std::atomic<size_t> failures{0};
        std::chrono::system_clock::time_point last_execution{std::chrono::system_clock::now()};
        std::chrono::system_clock::time_point last_failure{std::chrono::system_clock::now()};
    };

    std::unordered_map<std::string, HealthCheckInfo> health_checks_;
    mutable std::mutex health_checks_mutex_;

    // Service state
    std::atomic<bool> service_started_{false};
    std::chrono::system_clock::time_point service_start_time_{std::chrono::system_clock::now()};

    // Metrics
    std::atomic<size_t> total_probes_{0};
    std::atomic<size_t> failed_probes_{0};
    
    // Production-grade probe tracking (per probe type)
    std::unordered_map<HealthProbeType, std::chrono::system_clock::time_point> last_probe_time_;
    
    // Database connection for health checks (void* to avoid circular dependencies)
    void* db_connection_{nullptr};
    
    // Instance identifier for distributed deployments
    std::string instance_id_;

    /**
     * @brief Execute health checks for specific probe type
     * @param probe_type Type of probe
     * @return Health check results
     */
    std::vector<HealthCheckResult> execute_health_checks(HealthProbeType probe_type);

    /**
     * @brief Aggregate health check results
     * @param results Individual check results
     * @param require_all_critical Whether all critical checks must pass
     * @return Overall health result
     */
    HealthCheckResult aggregate_results(const std::vector<HealthCheckResult>& results,
                                       bool require_all_critical = true);

    /**
     * @brief Format health check response
     * @param result Overall health result
     * @param detailed Whether to include detailed information
     * @return HTTP status code and JSON response
     */
    std::pair<int, std::string> format_response(const HealthCheckResult& result, bool detailed = false);

    /**
     * @brief Update health metrics
     * @param probe_type Type of probe executed
     * @param success Whether probe was successful
     */
    void update_metrics(HealthProbeType probe_type, bool success);

    /**
     * @brief Mark service as started
     */
    void mark_service_started();
};

/**
 * @brief Create health check handler
 * @param config Configuration manager
 * @param logger Structured logger
 * @param error_handler Error handler
 * @param metrics Prometheus metrics collector
 * @return Shared pointer to health check handler
 */
std::shared_ptr<HealthCheckHandler> create_health_check_handler(
    std::shared_ptr<ConfigurationManager> config,
    std::shared_ptr<StructuredLogger> logger,
    std::shared_ptr<ErrorHandler> error_handler,
    std::shared_ptr<PrometheusMetricsCollector> metrics = nullptr);

/**
 * @brief Standard health check functions for common services
 */
namespace health_checks {

    /**
     * @brief Database connectivity health check
     * @return Health check function
     */
    HealthCheckFunction database_health_check();

    /**
     * @brief Redis connectivity health check
     * @return Health check function
     */
    HealthCheckFunction redis_health_check();

    /**
     * @brief External API connectivity health check
     * @param service_name Name of the external service
     * @param endpoint API endpoint to check
     * @return Health check function
     */
    HealthCheckFunction api_health_check(const std::string& service_name, const std::string& endpoint);

    /**
     * @brief File system health check
     * @param paths Paths to check for accessibility
     * @return Health check function
     */
    HealthCheckFunction filesystem_health_check(const std::vector<std::string>& paths);

    /**
     * @brief Memory usage health check
     * @param max_memory_percent Maximum allowed memory usage percentage
     * @return Health check function
     */
    HealthCheckFunction memory_health_check(double max_memory_percent = 90.0);

    /**
     * @brief CPU usage health check
     * @param max_cpu_percent Maximum allowed CPU usage percentage
     * @return Health check function
     */
    HealthCheckFunction cpu_health_check(double max_cpu_percent = 95.0);

    /**
     * @brief Disk space health check
     * @param min_free_percent Minimum required free disk space percentage
     * @return Health check function
     */
    HealthCheckFunction disk_space_health_check(double min_free_percent = 10.0);

    /**
     * @brief Service dependency health check
     * @param dependencies List of service dependencies with their health endpoints
     * @return Health check function
     */
    HealthCheckFunction dependency_health_check(const std::unordered_map<std::string, std::string>& dependencies);

    /**
     * @brief Queue depth health check
     * @param max_queue_depth Maximum allowed queue depth
     * @return Health check function
     */
    HealthCheckFunction queue_depth_health_check(size_t max_queue_depth = 1000);

    /**
     * @brief Thread pool health check
     * @param min_available_threads Minimum required available threads
     * @return Health check function
     */
    HealthCheckFunction thread_pool_health_check(size_t min_available_threads = 1);

} // namespace health_checks

} // namespace regulens
