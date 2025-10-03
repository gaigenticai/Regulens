/**
 * Compliance Agent Controller - Kubernetes Operator
 *
 * Enterprise-grade Kubernetes controller for managing ComplianceAgent
 * custom resources with advanced auto-scaling, workload monitoring,
 * and specialized compliance agent lifecycle management.
 */

#pragma once

#include "operator_framework.hpp"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <mutex>

namespace regulens {
namespace k8s {

/**
 * @brief Compliance Agent Controller
 *
 * Manages the lifecycle of ComplianceAgent custom resources,
 * including specialized agent deployment, intelligent auto-scaling,
 * workload monitoring, and compliance-specific operations.
 */
class ComplianceAgentController : public CustomResourceController {
public:
    ComplianceAgentController(std::shared_ptr<KubernetesAPIClient> api_client,
                            std::shared_ptr<StructuredLogger> logger,
                            std::shared_ptr<PrometheusMetricsCollector> metrics);

    ~ComplianceAgentController() override = default;

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

private:
    // Agent state tracking
    std::unordered_map<std::string, nlohmann::json> active_agents_;
    std::unordered_map<std::string, std::unordered_set<std::string>> agent_regulatory_sources_;
    mutable std::mutex agents_mutex_;

    // Metrics
    std::atomic<size_t> agents_created_{0};
    std::atomic<size_t> agents_updated_{0};
    std::atomic<size_t> agents_deleted_{0};
    std::atomic<size_t> scaling_events_{0};
    std::atomic<size_t> regulatory_sources_configured_{0};
    std::atomic<size_t> llm_integrations_enabled_{0};
    std::atomic<size_t> health_checks_performed_{0};
    std::atomic<size_t> compliance_decisions_processed_{0};

    /**
     * @brief Reconcile ComplianceAgent resource
     * @param resource Current resource state
     * @return Desired resource state
     */
    nlohmann::json reconcileResource(const nlohmann::json& resource) override;

    /**
     * @brief Validate ComplianceAgent specification
     * @param spec Resource spec
     * @return Validation errors (empty if valid)
     */
    std::vector<std::string> validateResourceSpec(const nlohmann::json& spec) override;

    /**
     * @brief Create specialized compliance agent deployment
     * @param agent_name Agent name
     * @param spec Agent spec
     * @return true if deployment created successfully
     */
    bool createComplianceAgentDeployment(const std::string& agent_name, const nlohmann::json& spec);

    /**
     * @brief Update existing compliance agent deployment
     * @param agent_name Agent name
     * @param spec Updated agent spec
     * @return true if deployment updated successfully
     */
    bool updateComplianceAgentDeployment(const std::string& agent_name, const nlohmann::json& spec);

    /**
     * @brief Perform intelligent auto-scaling for compliance agents
     * @param agent_name Agent name
     * @param spec Agent spec
     * @return true if scaling successful
     */
    bool scaleComplianceAgent(const std::string& agent_name, const nlohmann::json& spec);

    /**
     * @brief Create agent services and networking
     * @param agent_name Agent name
     * @param spec Agent spec
     * @return true if services created successfully
     */
    bool createAgentServices(const std::string& agent_name, const nlohmann::json& spec);

    /**
     * @brief Configure regulatory data sources for agent
     * @param agent_name Agent name
     * @param spec Agent spec
     * @return true if sources configured successfully
     */
    bool configureRegulatorySources(const std::string& agent_name, const nlohmann::json& spec);

    /**
     * @brief Set up LLM integration for agent
     * @param agent_name Agent name
     * @param spec Agent spec
     * @return true if LLM integration configured successfully
     */
    bool setupLLMIntegration(const std::string& agent_name, const nlohmann::json& spec);

    /**
     * @brief Create agent configuration and secrets
     * @param agent_name Agent name
     * @param spec Agent spec
     * @return true if configuration created successfully
     */
    bool createAgentConfiguration(const std::string& agent_name, const nlohmann::json& spec);

    /**
     * @brief Monitor compliance agent health and performance
     * @param agent_name Agent name
     * @param spec Agent spec
     * @return Health status with metrics
     */
    nlohmann::json monitorAgentHealth(const std::string& agent_name, const nlohmann::json& spec);

    /**
     * @brief Clean up agent resources on deletion
     * @param agent_name Agent name
     * @return true if cleanup successful
     */
    bool cleanupAgentResources(const std::string& agent_name);

    /**
     * @brief Calculate optimal replicas based on agent type and workload
     * @param agent_type Type of compliance agent
     * @param current_replicas Current replica count
     * @param workload_metrics Current workload metrics
     * @param agent_config Agent configuration
     * @return Recommended replica count
     */
    int calculateOptimalReplicas(const std::string& agent_type, int current_replicas,
                               const nlohmann::json& workload_metrics, const nlohmann::json& agent_config);

    /**
     * @brief Get workload metrics for scaling decisions
     * @param agent_name Agent name
     * @param agent_type Agent type
     * @return Workload metrics JSON
     */
    nlohmann::json getWorkloadMetrics(const std::string& agent_name, const std::string& agent_type);

    /**
     * @brief Generate deployment spec for compliance agent
     * @param agent_name Agent name
     * @param spec Agent spec
     * @return Deployment JSON specification
     */
    nlohmann::json generateAgentDeploymentSpec(const std::string& agent_name, const nlohmann::json& spec);

    /**
     * @brief Generate ConfigMap for agent configuration
     * @param agent_name Agent name
     * @param spec Agent spec
     * @return ConfigMap JSON specification
     */
    nlohmann::json generateAgentConfigMapSpec(const std::string& agent_name, const nlohmann::json& spec);

    /**
     * @brief Generate Secret for agent credentials
     * @param agent_name Agent name
     * @param spec Agent spec
     * @return Secret JSON specification
     */
    nlohmann::json generateAgentSecretSpec(const std::string& agent_name, const nlohmann::json& spec);

    /**
     * @brief Generate ServiceAccount for agent
     * @param agent_name Agent name
     * @param spec Agent spec
     * @return ServiceAccount JSON specification
     */
    nlohmann::json generateAgentServiceAccountSpec(const std::string& agent_name, const nlohmann::json& spec);

    /**
     * @brief Configure RBAC for agent
     * @param agent_name Agent name
     * @param spec Agent spec
     * @return true if RBAC configured successfully
     */
    bool configureAgentRBAC(const std::string& agent_name, const nlohmann::json& spec);

    /**
     * @brief Set up monitoring and metrics collection for agent
     * @param agent_name Agent name
     * @param spec Agent spec
     * @return true if monitoring configured successfully
     */
    bool setupAgentMonitoring(const std::string& agent_name, const nlohmann::json& spec);

    /**
     * @brief Validate regulatory sources configuration
     * @param sources Regulatory sources array
     * @return Validation errors
     */
    std::vector<std::string> validateRegulatorySources(const nlohmann::json& sources);

    /**
     * @brief Validate LLM configuration
     * @param llm_config LLM configuration
     * @return Validation errors
     */
    std::vector<std::string> validateLLMConfig(const nlohmann::json& llm_config);

    /**
     * @brief Get agent type specific configuration
     * @param agent_type Agent type
     * @return Type-specific configuration
     */
    nlohmann::json getAgentTypeConfig(const std::string& agent_type);

    /**
     * @brief Update agent status with current state
     * @param agent_name Agent name
     * @param status Status information
     */
    void updateAgentStatus(const std::string& agent_name, const nlohmann::json& status);
};

/**
 * @brief Create Compliance Agent Controller
 * @param api_client Kubernetes API client
 * @param logger Structured logger
 * @param metrics Prometheus metrics collector
 * @return Shared pointer to controller
 */
std::shared_ptr<ComplianceAgentController> createComplianceAgentController(
    std::shared_ptr<KubernetesAPIClient> api_client,
    std::shared_ptr<StructuredLogger> logger,
    std::shared_ptr<PrometheusMetricsCollector> metrics);

} // namespace k8s
} // namespace regulens
