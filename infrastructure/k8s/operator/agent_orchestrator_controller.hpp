/**
 * Agent Orchestrator Controller - Kubernetes Operator
 *
 * Enterprise-grade Kubernetes controller for managing AgentOrchestrator
 * custom resources with advanced lifecycle management, scaling, and monitoring.
 */

#pragma once

#include "operator_framework.hpp"
#include <memory>
#include <unordered_map>
#include <atomic>

namespace regulens {

// Forward declarations
class PrometheusClient;

namespace k8s {

/**
 * @brief Agent Orchestrator Controller
 *
 * Manages the lifecycle of AgentOrchestrator custom resources,
 * including agent deployment, scaling, health monitoring, and coordination.
 */
class AgentOrchestratorController : public CustomResourceController {
public:
    AgentOrchestratorController(std::shared_ptr<KubernetesAPIClient> api_client,
                              std::shared_ptr<StructuredLogger> logger,
                              std::shared_ptr<PrometheusMetricsCollector> metrics);

    ~AgentOrchestratorController() override = default;

    /**
     * @brief Handle resource lifecycle events
     * @param event Resource event
     */
    void handleResourceEvent(const ResourceEvent& event) override;

    /**
     * @brief Get controller-specific metrics
     * @return JSON metrics
     */
    nlohmann::json getMetrics() const override;

    /**
     * @brief Get resource type managed by this controller
     * @return "AgentOrchestrator"
     */
    std::string getResourceType() const override { return "AgentOrchestrator"; }

private:
    // Orchestrator state tracking
    std::unordered_map<std::string, nlohmann::json> active_orchestrators_;
    mutable std::mutex orchestrators_mutex_;

    // Prometheus client for metrics queries
    std::shared_ptr<PrometheusClient> prometheus_client_;

    // Metrics
    std::atomic<size_t> orchestrators_created_{0};
    std::atomic<size_t> orchestrators_updated_{0};
    std::atomic<size_t> orchestrators_deleted_{0};
    std::atomic<size_t> agents_deployed_{0};
    std::atomic<size_t> scaling_operations_{0};

    /**
     * @brief Reconcile AgentOrchestrator resource
     * @param resource Current resource state
     * @return Desired resource state
     */
    nlohmann::json reconcileResource(const nlohmann::json& resource) override;

    /**
     * @brief Validate AgentOrchestrator specification
     * @param spec Resource spec
     * @return Validation errors (empty if valid)
     */
    std::vector<std::string> validateResourceSpec(const nlohmann::json& spec) override;

    /**
     * @brief Create Kubernetes deployments for agents
     * @param orchestrator_name Orchestrator name
     * @param spec Orchestrator spec
     * @return true if deployments created successfully
     */
    bool createAgentDeployments(const std::string& orchestrator_name, const nlohmann::json& spec);

    /**
     * @brief Update existing agent deployments
     * @param orchestrator_name Orchestrator name
     * @param spec Updated orchestrator spec
     * @return true if deployments updated successfully
     */
    bool updateAgentDeployments(const std::string& orchestrator_name, const nlohmann::json& spec);

    /**
     * @brief Scale agent deployments based on load
     * @param orchestrator_name Orchestrator name
     * @param spec Orchestrator spec
     * @return true if scaling successful
     */
    bool scaleAgentDeployments(const std::string& orchestrator_name, const nlohmann::json& spec);

    /**
     * @brief Create Kubernetes services for agent communication
     * @param orchestrator_name Orchestrator name
     * @param spec Orchestrator spec
     * @return true if services created successfully
     */
    bool createAgentServices(const std::string& orchestrator_name, const nlohmann::json& spec);

    /**
     * @brief Create ConfigMaps for agent configuration
     * @param orchestrator_name Orchestrator name
     * @param spec Orchestrator spec
     * @return true if ConfigMaps created successfully
     */
    bool createAgentConfigMaps(const std::string& orchestrator_name, const nlohmann::json& spec);

    /**
     * @brief Monitor agent health and update status
     * @param orchestrator_name Orchestrator name
     * @param spec Orchestrator spec
     * @return Health status
     */
    nlohmann::json monitorAgentHealth(const std::string& orchestrator_name, const nlohmann::json& spec);

    /**
     * @brief Clean up resources when orchestrator is deleted
     * @param orchestrator_name Orchestrator name
     * @return true if cleanup successful
     */
    bool cleanupOrchestratorResources(const std::string& orchestrator_name);

    /**
     * @brief Generate agent deployment specification
     * @param orchestrator_name Orchestrator name
     * @param agent_spec Agent specification
     * @param orchestrator_spec Orchestrator specification
     * @return Deployment JSON specification
     */
    nlohmann::json generateAgentDeploymentSpec(const std::string& orchestrator_name,
                                             const nlohmann::json& agent_spec,
                                             const nlohmann::json& orchestrator_spec);

    /**
     * @brief Generate agent service specification
     * @param orchestrator_name Orchestrator name
     * @param agent_spec Agent specification
     * @return Service JSON specification
     */
    nlohmann::json generateAgentServiceSpec(const std::string& orchestrator_name,
                                          const nlohmann::json& agent_spec);

    /**
     * @brief Calculate optimal agent replica count based on load
     * @param agent_type Agent type
     * @param current_replicas Current replica count
     * @param load_metrics Current load metrics
     * @return Recommended replica count
     */
    int calculateOptimalReplicas(const std::string& agent_type, int current_replicas,
                                const nlohmann::json& load_metrics);

    /**
     * @brief Get agent load metrics from Prometheus
     * @param orchestrator_name Orchestrator name
     * @param agent_spec Agent specification
     * @return Load metrics JSON (cpu_usage, memory_usage, queue_depth)
     */
    nlohmann::json getAgentLoadMetrics(const std::string& orchestrator_name, const nlohmann::json& agent_spec);

    /**
     * @brief Update orchestrator status
     * @param orchestrator_name Orchestrator name
     * @param status Status information
     */
    void updateOrchestratorStatus(const std::string& orchestrator_name, const nlohmann::json& status);
};

/**
 * @brief Create Agent Orchestrator Controller
 * @param api_client Kubernetes API client
 * @param logger Structured logger
 * @param metrics Prometheus metrics collector
 * @return Shared pointer to controller
 */
std::shared_ptr<AgentOrchestratorController> createAgentOrchestratorController(
    std::shared_ptr<KubernetesAPIClient> api_client,
    std::shared_ptr<StructuredLogger> logger,
    std::shared_ptr<PrometheusMetricsCollector> metrics);

} // namespace k8s
} // namespace regulens
