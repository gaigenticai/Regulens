/**
 * Health Check Manager - Header
 * 
 * Production-grade health check system for all services and dependencies.
 * Supports Kubernetes readiness, liveness, and startup probes.
 */

#ifndef REGULENS_HEALTH_CHECK_MANAGER_HPP
#define REGULENS_HEALTH_CHECK_MANAGER_HPP

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <memory>
#include <functional>

namespace regulens {

// Health status
enum class HealthStatus {
    HEALTHY,
    DEGRADED,
    UNHEALTHY,
    UNKNOWN
};

// Health check type
enum class HealthCheckType {
    READINESS,      // Service is ready to accept traffic
    LIVENESS,       // Service is alive and responsive
    STARTUP         // Service has started successfully
};

// Dependency type
enum class DependencyType {
    DATABASE,
    CACHE,
    MESSAGE_QUEUE,
    EXTERNAL_API,
    FILE_SYSTEM,
    MEMORY,
    CPU
};

// Health check result
struct HealthCheckResult {
    std::string component;
    DependencyType type;
    HealthStatus status;
    std::string message;
    int response_time_ms;
    std::chrono::system_clock::time_point timestamp;
    std::map<std::string, std::string> details;
};

// Health check function type
using HealthCheckFunction = std::function<HealthCheckResult()>;

/**
 * Health Check Manager
 * 
 * Comprehensive health check system for production deployments.
 * Features:
 * - Kubernetes-compatible health endpoints
 * - Readiness, liveness, and startup probes
 * - Dependency health monitoring
 * - Automatic service degradation detection
 * - Health check caching
 * - Circuit breaker integration
 * - Metrics export (Prometheus)
 * - Custom health checks
 */
class HealthCheckManager {
public:
    /**
     * Constructor
     */
    HealthCheckManager();

    ~HealthCheckManager();

    /**
     * Initialize health check manager
     * 
     * @return true if successful
     */
    bool initialize();

    /**
     * Register health check
     * 
     * @param name Check name
     * @param type Check type
     * @param check_function Health check function
     * @param timeout_ms Timeout in milliseconds
     * @return true if registered
     */
    bool register_check(
        const std::string& name,
        HealthCheckType type,
        HealthCheckFunction check_function,
        int timeout_ms = 5000
    );

    /**
     * Unregister health check
     * 
     * @param name Check name
     * @return true if unregistered
     */
    bool unregister_check(const std::string& name);

    /**
     * Perform all health checks
     * 
     * @param type Optional filter by check type
     * @return Vector of health check results
     */
    std::vector<HealthCheckResult> check_all(
        HealthCheckType type = HealthCheckType::READINESS
    );

    /**
     * Perform single health check
     * 
     * @param name Check name
     * @return Health check result
     */
    HealthCheckResult check_one(const std::string& name);

    /**
     * Get readiness status
     * Used for Kubernetes readiness probe
     * 
     * @return true if ready
     */
    bool is_ready();

    /**
     * Get liveness status
     * Used for Kubernetes liveness probe
     * 
     * @return true if alive
     */
    bool is_alive();

    /**
     * Get startup status
     * Used for Kubernetes startup probe
     * 
     * @return true if started
     */
    bool is_started();

    /**
     * Get overall health status
     * 
     * @return Health status
     */
    HealthStatus get_overall_status();

    /**
     * Get health status JSON
     * Detailed health information for /health endpoint
     * 
     * @return JSON health status
     */
    std::string get_health_json();

    /**
     * Check database health
     * 
     * @param connection_string Database connection
     * @return Health check result
     */
    static HealthCheckResult check_database(const std::string& connection_string);

    /**
     * Check Redis health
     * 
     * @param redis_host Redis host
     * @param redis_port Redis port
     * @return Health check result
     */
    static HealthCheckResult check_redis(const std::string& redis_host, int redis_port);

    /**
     * Check disk space
     * 
     * @param path Path to check
     * @param min_free_gb Minimum free space in GB
     * @return Health check result
     */
    static HealthCheckResult check_disk_space(const std::string& path, double min_free_gb);

    /**
     * Check memory usage
     * 
     * @param max_usage_percent Maximum memory usage percentage
     * @return Health check result
     */
    static HealthCheckResult check_memory(double max_usage_percent);

    /**
     * Check CPU usage
     * 
     * @param max_usage_percent Maximum CPU usage percentage
     * @return Health check result
     */
    static HealthCheckResult check_cpu(double max_usage_percent);

    /**
     * Check external API
     * 
     * @param name API name
     * @param url API URL
     * @param timeout_ms Timeout
     * @return Health check result
     */
    static HealthCheckResult check_external_api(
        const std::string& name,
        const std::string& url,
        int timeout_ms = 5000
    );

    /**
     * Enable health check caching
     * 
     * @param enabled Whether to enable
     * @param cache_ttl_seconds Cache TTL
     */
    void set_caching_enabled(bool enabled, int cache_ttl_seconds = 30);

    /**
     * Set degraded threshold
     * Number of failed checks before marking as degraded
     * 
     * @param threshold Threshold count
     */
    void set_degraded_threshold(int threshold);

    /**
     * Set unhealthy threshold
     * Number of failed checks before marking as unhealthy
     * 
     * @param threshold Threshold count
     */
    void set_unhealthy_threshold(int threshold);

    /**
     * Get health metrics (Prometheus format)
     * 
     * @return Prometheus metrics
     */
    std::string get_prometheus_metrics();

    /**
     * Get health history
     * 
     * @param component Component name
     * @param minutes Minutes of history
     * @return Vector of historical health results
     */
    std::vector<HealthCheckResult> get_health_history(
        const std::string& component,
        int minutes = 60
    );

private:
    bool initialized_;
    bool caching_enabled_;
    int cache_ttl_seconds_;
    int degraded_threshold_;
    int unhealthy_threshold_;

    struct RegisteredCheck {
        std::string name;
        HealthCheckType type;
        HealthCheckFunction function;
        int timeout_ms;
        int consecutive_failures;
    };

    std::map<std::string, RegisteredCheck> registered_checks_;
    std::map<std::string, HealthCheckResult> cached_results_;
    std::map<std::string, std::chrono::system_clock::time_point> cache_timestamps_;
    std::vector<HealthCheckResult> health_history_;

    /**
     * Execute health check with timeout
     */
    HealthCheckResult execute_check_with_timeout(
        const RegisteredCheck& check
    );

    /**
     * Check if cached result is valid
     */
    bool is_cache_valid(const std::string& name);

    /**
     * Update health history
     */
    void update_history(const HealthCheckResult& result);

    /**
     * Calculate overall status from results
     */
    HealthStatus calculate_overall_status(
        const std::vector<HealthCheckResult>& results
    );

    /**
     * Clean old history
     */
    void clean_old_history(int retention_minutes = 60);
};

} // namespace regulens

#endif // REGULENS_HEALTH_CHECK_MANAGER_HPP

