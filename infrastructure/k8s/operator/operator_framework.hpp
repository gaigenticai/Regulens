/**
 * Kubernetes Operator Framework - Enterprise Production-Grade
 *
 * Core framework for building Kubernetes operators that manage Regulens
 * custom resources with advanced lifecycle management, scaling, and monitoring.
 *
 * Features:
 * - Custom resource lifecycle management
 * - Advanced scaling and auto-healing
 * - Comprehensive monitoring and metrics
 * - Fault tolerance and resilience
 * - Multi-cluster support
 * - RBAC integration
 * - Webhook validation and mutation
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <nlohmann/json.hpp>

#include "../../../shared/config/configuration_manager.hpp"
#include "../../../shared/logging/structured_logger.hpp"
#include "../../../shared/error_handler.hpp"
#include "../../../shared/metrics/prometheus_metrics.hpp"

namespace regulens {
namespace k8s {

/**
 * @brief Kubernetes API client interface
 */
class KubernetesAPIClient {
public:
    virtual ~KubernetesAPIClient() = default;

    /**
     * @brief Get custom resource by name
     * @param group API group
     * @param version API version
     * @param plural Resource plural name
     * @param namespace_ Namespace
     * @param name Resource name
     * @return JSON representation of resource
     */
    virtual nlohmann::json getCustomResource(const std::string& group,
                                           const std::string& version,
                                           const std::string& plural,
                                           const std::string& namespace_,
                                           const std::string& name) = 0;

    /**
     * @brief List custom resources
     * @param group API group
     * @param version API version
     * @param plural Resource plural name
     * @param namespace_ Namespace
     * @param label_selector Label selector
     * @return JSON array of resources
     */
    virtual nlohmann::json listCustomResources(const std::string& group,
                                             const std::string& version,
                                             const std::string& plural,
                                             const std::string& namespace_,
                                             const std::string& label_selector = "") = 0;

    /**
     * @brief Create custom resource
     * @param group API group
     * @param version API version
     * @param plural Resource plural name
     * @param namespace_ Namespace
     * @param resource JSON resource definition
     * @return Created resource JSON
     */
    virtual nlohmann::json createCustomResource(const std::string& group,
                                              const std::string& version,
                                              const std::string& plural,
                                              const std::string& namespace_,
                                              const nlohmann::json& resource) = 0;

    /**
     * @brief Update custom resource
     * @param group API group
     * @param version API version
     * @param plural Resource plural name
     * @param namespace_ Namespace
     * @param name Resource name
     * @param resource Updated JSON resource definition
     * @return Updated resource JSON
     */
    virtual nlohmann::json updateCustomResource(const std::string& group,
                                              const std::string& version,
                                              const std::string& plural,
                                              const std::string& namespace_,
                                              const std::string& name,
                                              const nlohmann::json& resource) = 0;

    /**
     * @brief Delete custom resource
     * @param group API group
     * @param version API version
     * @param plural Resource plural name
     * @param namespace_ Namespace
     * @param name Resource name
     * @return Success status
     */
    virtual bool deleteCustomResource(const std::string& group,
                                    const std::string& version,
                                    const std::string& plural,
                                    const std::string& namespace_,
                                    const std::string& name) = 0;

    /**
     * @brief Patch custom resource status
     * @param group API group
     * @param version API version
     * @param plural Resource plural name
     * @param namespace_ Namespace
     * @param name Resource name
     * @param status Status JSON to patch
     * @return Updated resource JSON
     */
    virtual nlohmann::json patchCustomResourceStatus(const std::string& group,
                                                    const std::string& version,
                                                    const std::string& plural,
                                                    const std::string& namespace_,
                                                    const std::string& name,
                                                    const nlohmann::json& status) = 0;

    /**
     * @brief Watch custom resources for changes
     * @param group API group
     * @param version API version
     * @param plural Resource plural name
     * @param namespace_ Namespace
     * @param callback Function called on resource changes
     * @return Watch handle (can be used to stop watching)
     */
    virtual std::string watchCustomResources(const std::string& group,
                                           const std::string& version,
                                           const std::string& plural,
                                           const std::string& namespace_,
                                           std::function<void(const std::string&, const nlohmann::json&)> callback) = 0;

    /**
     * @brief Stop watching resources
     * @param watch_handle Handle returned by watchCustomResources
     */
    virtual void stopWatching(const std::string& watch_handle) = 0;

    /**
     * @brief Get cluster information
     * @return JSON with cluster info
     */
    virtual nlohmann::json getClusterInfo() = 0;

    /**
     * @brief Check API server health
     * @return true if healthy
     */
    virtual bool isHealthy() = 0;
};

/**
 * @brief Custom resource lifecycle event types
 */
enum class ResourceEventType {
    ADDED,
    MODIFIED,
    DELETED,
    BOOKMARK
};

/**
 * @brief Custom resource lifecycle event
 */
struct ResourceEvent {
    ResourceEventType type;
    std::string resource_type;
    std::string namespace_;
    std::string name;
    nlohmann::json resource;
    nlohmann::json old_resource; // Only for MODIFIED events
    std::chrono::system_clock::time_point timestamp;

    ResourceEvent(ResourceEventType t, std::string rt, std::string ns, std::string n,
                 nlohmann::json res, nlohmann::json old_res = nullptr)
        : type(t), resource_type(std::move(rt)), namespace_(std::move(ns)),
          name(std::move(n)), resource(std::move(res)), old_resource(std::move(old_res)),
          timestamp(std::chrono::system_clock::now()) {}
};

/**
 * @brief Base class for custom resource controllers
 */
class CustomResourceController {
public:
    CustomResourceController(std::shared_ptr<KubernetesAPIClient> api_client,
                           std::shared_ptr<StructuredLogger> logger,
                           std::shared_ptr<PrometheusMetricsCollector> metrics);

    virtual ~CustomResourceController() = default;

    /**
     * @brief Initialize the controller
     * @return true if initialization successful
     */
    virtual bool initialize();

    /**
     * @brief Shutdown the controller
     */
    virtual void shutdown();

    /**
     * @brief Handle resource lifecycle event
     * @param event Resource event
     */
    virtual void handleResourceEvent(const ResourceEvent& event) = 0;

    /**
     * @brief Get controller health status
     * @return JSON health status
     */
    virtual nlohmann::json getHealthStatus() const;

    /**
     * @brief Get controller metrics
     * @return JSON metrics
     */
    virtual nlohmann::json getMetrics() const;

    /**
     * @brief Get the resource type managed by this controller
     * @return Resource type identifier (e.g., "ComplianceAgent", "RegulatoryDataSource")
     */
    virtual std::string getResourceType() const = 0;

protected:
    std::shared_ptr<KubernetesAPIClient> api_client_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<PrometheusMetricsCollector> metrics_;

    std::atomic<size_t> events_processed_;
    std::atomic<size_t> events_failed_;
    std::chrono::system_clock::time_point last_event_time_;

    /**
     * @brief Update resource status
     * @param resource_type Resource type
     * @param namespace_ Namespace
     * @param name Resource name
     * @param status Status JSON
     * @return true if update successful
     */
    bool updateResourceStatus(const std::string& resource_type,
                            const std::string& namespace_,
                            const std::string& name,
                            const nlohmann::json& status);

    /**
     * @brief Reconcile resource state
     * @param resource Current resource state
     * @return Desired resource state
     */
    virtual nlohmann::json reconcileResource(const nlohmann::json& resource) = 0;

    /**
     * @brief Validate resource specification
     * @param spec Resource spec
     * @return Validation errors (empty if valid)
     */
    virtual std::vector<std::string> validateResourceSpec(const nlohmann::json& spec) = 0;
};

/**
 * @brief Operator configuration
 */
struct OperatorConfig {
    std::string namespace_;
    std::string service_account;
    bool enable_webhooks = true;
    bool enable_metrics = true;
    int reconcile_interval_seconds = 30;
    int max_concurrent_reconciles = 10;
    std::chrono::seconds health_check_interval = std::chrono::seconds(30);
    std::chrono::seconds metrics_interval = std::chrono::seconds(15);
    bool enable_leader_election = true;
    std::string leader_election_namespace = "kube-system";
    std::string leader_election_id = "regulens-operator";
};

/**
 * @brief Base Kubernetes operator class
 */
class KubernetesOperator {
    /**
     * @brief Worker thread for processing resource events
     */
    virtual void workerThread();
public:
    KubernetesOperator(std::shared_ptr<ConfigurationManager> config,
                     std::shared_ptr<StructuredLogger> logger,
                     std::shared_ptr<ErrorHandler> error_handler,
                     std::shared_ptr<PrometheusMetricsCollector> metrics);

    virtual ~KubernetesOperator();

    /**
     * @brief Initialize the operator
     * @return true if initialization successful
     */
    virtual bool initialize();

    /**
     * @brief Run the operator (blocking)
     */
    virtual void run();

    /**
     * @brief Shutdown the operator
     */
    virtual void shutdown();

    /**
     * @brief Get operator health status
     * @return JSON health status
     */
    virtual nlohmann::json getHealthStatus() const;

    /**
     * @brief Get operator metrics
     * @return JSON metrics
     */
    virtual nlohmann::json getMetrics() const;

    /**
     * @brief Register a custom resource controller
     * @param controller Controller to register
     */
    void registerController(std::shared_ptr<CustomResourceController> controller);

    /**
     * @brief Unregister a custom resource controller
     * @param resource_type Resource type to unregister
     */
    void unregisterController(const std::string& resource_type);

protected:
    std::shared_ptr<ConfigurationManager> config_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<ErrorHandler> error_handler_;
    std::shared_ptr<PrometheusMetricsCollector> metrics_;

    OperatorConfig operator_config_;
    std::shared_ptr<KubernetesAPIClient> api_client_;

    std::unordered_map<std::string, std::shared_ptr<CustomResourceController>> controllers_;
    mutable std::mutex controllers_mutex_;

    std::atomic<bool> running_;
    std::atomic<bool> initialized_;

    // Watch handles
    std::vector<std::string> watch_handles_;
    mutable std::mutex watch_mutex_;

    // Work queue for reconciliation
    std::queue<ResourceEvent> work_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::vector<std::thread> worker_threads_;

    /**
     * @brief Load operator configuration
     */
    virtual void loadConfig();

    /**
     * @brief Initialize Kubernetes API client
     * @return true if initialization successful
     */
    virtual bool initializeAPIClient();

    /**
     * @brief Start resource watches
     * @return true if watches started successfully
     */
    virtual bool startResourceWatches();

    /**
     * @brief Start worker threads
     * @return true if workers started successfully
     */
    virtual bool startWorkers();

    /**
     * @brief Process resource event
     * @param event Event to process
     */
    virtual void processResourceEvent(const ResourceEvent& event);

    /**
     * @brief Handle resource watch callback
     * @param event_type Event type
     * @param resource Resource JSON
     */
    virtual void handleWatchCallback(const std::string& event_type, const nlohmann::json& resource);

    /**
     * @brief Reconcile all managed resources
     */
    virtual void reconcileAllResources();

    /**
     * @brief Perform health checks
     */
    virtual void performHealthChecks();

    /**
     * @brief Update metrics
     */
    virtual void updateMetrics();

    /**
     * @brief Create Kubernetes API client (factory method)
     * @return API client instance
     */
    virtual std::shared_ptr<KubernetesAPIClient> createAPIClient() = 0;
};

/**
 * @brief Create Kubernetes operator instance
 * @param config Configuration manager
 * @param logger Structured logger
 * @param error_handler Error handler
 * @param metrics Metrics collector
 * @return Shared pointer to operator
 */
std::shared_ptr<KubernetesOperator> createKubernetesOperator(
    std::shared_ptr<ConfigurationManager> config,
    std::shared_ptr<StructuredLogger> logger,
    std::shared_ptr<ErrorHandler> error_handler,
    std::shared_ptr<PrometheusMetricsCollector> metrics);

} // namespace k8s
} // namespace regulens
