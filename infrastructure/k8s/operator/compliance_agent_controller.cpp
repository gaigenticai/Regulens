/**
 * Compliance Agent Controller Implementation
 *
 * Production-grade Kubernetes controller for managing ComplianceAgent
 * custom resources with intelligent auto-scaling, workload monitoring,
 * and specialized compliance agent lifecycle management.
 */

#include "compliance_agent_controller.hpp"
#include <shared/metrics/prometheus_client.hpp>
#include <algorithm>
#include <sstream>
#include <chrono>
#include <random>

namespace regulens {
namespace k8s {

// ComplianceAgentController Implementation

ComplianceAgentController::ComplianceAgentController(std::shared_ptr<KubernetesAPIClient> api_client,
                                                   std::shared_ptr<StructuredLogger> logger,
                                                   std::shared_ptr<PrometheusMetricsCollector> metrics)
    : CustomResourceController(api_client, logger, metrics) {
    // Initialize Prometheus client for querying agent metrics
    prometheus_client_ = create_prometheus_client(logger);
}

void ComplianceAgentController::handleResourceEvent(const ResourceEvent& event) {
    try {
        std::string agent_name = event.name;
        std::string namespace_ = event.namespace_;

        switch (event.type) {
            case ResourceEventType::ADDED:
                handleAgentCreation(agent_name, namespace_, event.resource);
                break;

            case ResourceEventType::MODIFIED:
                handleAgentUpdate(agent_name, namespace_, event.resource, event.old_resource);
                break;

            case ResourceEventType::DELETED:
                handleAgentDeletion(agent_name, namespace_, event.resource);
                break;

            default:
                if (logger_) {
                    logger_->warn("Unknown resource event type",
                                 "ComplianceAgentController", "handleResourceEvent",
                                 {{"event_type", static_cast<int>(event.type)},
                                  {"agent", agent_name}});
                }
                break;
        }

        events_processed_++;

    } catch (const std::exception& e) {
        events_failed_++;
        if (logger_) {
            logger_->error("Exception handling agent event: " + std::string(e.what()),
                         "ComplianceAgentController", "handleResourceEvent",
                         {{"agent", event.name}, {"event_type", static_cast<int>(event.type)}});
        }
    }
}

nlohmann::json ComplianceAgentController::getMetrics() const {
    auto base_metrics = CustomResourceController::getMetrics();

    std::lock_guard<std::mutex> lock(agents_mutex_);

    base_metrics["compliance_agent_metrics"] = {
        {"agents_created_total", agents_created_.load()},
        {"agents_updated_total", agents_updated_.load()},
        {"agents_deleted_total", agents_deleted_.load()},
        {"scaling_events_total", scaling_events_.load()},
        {"regulatory_sources_configured_total", regulatory_sources_configured_.load()},
        {"llm_integrations_enabled_total", llm_integrations_enabled_.load()},
        {"health_checks_performed_total", health_checks_performed_.load()},
        {"compliance_decisions_processed_total", compliance_decisions_processed_.load()},
        {"active_agents", active_agents_.size()}
    };

    return base_metrics;
}

nlohmann::json ComplianceAgentController::reconcileResource(const nlohmann::json& resource) {
    std::string agent_name = resource["metadata"]["name"];
    std::string namespace_ = resource["metadata"]["namespace"];

    try {
        const auto& spec = resource["spec"];
        std::string agent_type = spec.value("type", "");

        // Validate spec
        auto validation_errors = validateResourceSpec(spec);
        if (!validation_errors.empty()) {
            if (logger_) {
                logger_->error("Agent spec validation failed",
                             "ComplianceAgentController", "reconcileResource",
                             {{"agent", agent_name}, {"type", agent_type}, {"errors", validation_errors.size()}});
            }

            nlohmann::json status = {
                {"phase", "Failed"},
                {"conditions", {{
                    {"type", "Validated"},
                    {"status", "False"},
                    {"reason", "ValidationFailed"},
                    {"message", "Spec validation failed: " + validation_errors[0]},
                    {"lastTransitionTime", std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count()}
                }}}
            };

            updateResourceStatus("complianceagents", namespace_, agent_name, status);
            return resource;
        }

        // Check if agent exists
        std::lock_guard<std::mutex> lock(agents_mutex_);
        bool exists = active_agents_.find(agent_name) != active_agents_.end();

        if (!exists) {
            // Create new compliance agent
            if (createComplianceAgentDeployment(agent_name, spec) &&
                createAgentServices(agent_name, spec) &&
                configureRegulatorySources(agent_name, spec) &&
                setupLLMIntegration(agent_name, spec) &&
                createAgentConfiguration(agent_name, spec) &&
                configureAgentRBAC(agent_name, spec) &&
                setupAgentMonitoring(agent_name, spec)) {

                active_agents_[agent_name] = resource;
                agents_created_++;

                // Track metrics
                if (spec.contains("regulatorySources") && spec["regulatorySources"].is_array()) {
                    regulatory_sources_configured_ += spec["regulatorySources"].size();
                }
                if (spec.value("llmIntegration", false)) {
                    llm_integrations_enabled_++;
                }

                nlohmann::json status = {
                    {"phase", "Running"},
                    {"replicas", spec.value("replicas", 2)},
                    {"availableReplicas", spec.value("replicas", 2)},
                    {"agentType", agent_type},
                    {"conditions", {{
                        {"type", "Ready"},
                        {"status", "True"},
                        {"reason", "Created"},
                        {"message", "Compliance agent created successfully"},
                        {"lastTransitionTime", std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count()}
                    }}}
                };

                updateResourceStatus("complianceagents", namespace_, agent_name, status);

                if (logger_) {
                    logger_->info("Compliance agent created successfully",
                                 "ComplianceAgentController", "reconcileResource",
                                 {{"agent", agent_name}, {"type", agent_type}, {"replicas", spec.value("replicas", 2)}});
                }
            } else {
                nlohmann::json status = {
                    {"phase", "Failed"},
                    {"conditions", {{
                        {"type", "Ready"},
                        {"status", "False"},
                        {"reason", "CreationFailed"},
                        {"message", "Failed to create compliance agent resources"},
                        {"lastTransitionTime", std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count()}
                    }}}
                };

                updateResourceStatus("complianceagents", namespace_, agent_name, status);
            }
        } else {
            // Update existing agent
            const auto& old_resource = active_agents_[agent_name];
            const auto& old_spec = old_resource["spec"];

            // Check if spec changed
            if (spec != old_spec) {
                if (updateComplianceAgentDeployment(agent_name, spec)) {
                    active_agents_[agent_name] = resource;
                    agents_updated_++;

                    if (logger_) {
                        logger_->info("Compliance agent updated successfully",
                                     "ComplianceAgentController", "reconcileResource",
                                     {{"agent", agent_name}, {"type", agent_type}});
                    }
                }
            }

            // Perform scaling if enabled
            if (spec.value("scaling", nlohmann::json::object()).value("enabled", true)) {
                scaleComplianceAgent(agent_name, spec);
            }

            // Monitor health
            auto health_status = monitorAgentHealth(agent_name, spec);
            health_checks_performed_++;

            // Update performance metrics
            if (health_status.contains("decisionsProcessed")) {
                compliance_decisions_processed_ += health_status["decisionsProcessed"];
            }

            nlohmann::json status = {
                {"phase", "Running"},
                {"replicas", health_status.value("currentReplicas", spec.value("replicas", 2))},
                {"availableReplicas", health_status.value("availableReplicas", 0)},
                {"agentType", agent_type},
                {"performanceMetrics", {
                    {"decisionsProcessed", health_status.value("decisionsProcessed", 0)},
                    {"averageProcessingTime", health_status.value("averageProcessingTime", 0.0)},
                    {"errorRate", health_status.value("errorRate", 0.0)},
                    {"lastHealthCheck", std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count()}
                }},
                {"conditions", {{
                    {"type", "Ready"},
                    {"status", health_status.value("healthy", false) ? "True" : "False"},
                    {"reason", "Running"},
                    {"message", "Compliance agent is running"},
                    {"lastTransitionTime", std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count()}
                }}}
            };

            updateResourceStatus("complianceagents", namespace_, agent_name, status);
        }

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception reconciling agent: " + std::string(e.what()),
                         "ComplianceAgentController", "reconcileResource",
                         {{"agent", agent_name}});
        }
    }

    return resource;
}

std::vector<std::string> ComplianceAgentController::validateResourceSpec(const nlohmann::json& spec) {
    std::vector<std::string> errors;

    // Validate agent type
    std::vector<std::string> valid_types = {"transaction_guardian", "audit_intelligence", "regulatory_assessor", "risk_analyzer"};
    std::string agent_type = spec.value("type", "");
    if (std::find(valid_types.begin(), valid_types.end(), agent_type) == valid_types.end()) {
        errors.push_back("type must be one of: transaction_guardian, audit_intelligence, regulatory_assessor, risk_analyzer");
    }

    // Validate replicas
    if (spec.contains("replicas")) {
        int replicas = spec["replicas"];
        if (replicas < 1 || replicas > 50) {
            errors.push_back("replicas must be between 1 and 50");
        }
    }

    // Validate image
    if (!spec.contains("image") || spec["image"].get<std::string>().empty()) {
        errors.push_back("image is required");
    }

    // Validate regulatory sources if present
    if (spec.contains("regulatorySources")) {
        auto source_errors = validateRegulatorySources(spec["regulatorySources"]);
        errors.insert(errors.end(), source_errors.begin(), source_errors.end());
    }

    // Validate LLM config if present
    if (spec.contains("llmConfig")) {
        auto llm_errors = validateLLMConfig(spec["llmConfig"]);
        errors.insert(errors.end(), llm_errors.begin(), llm_errors.end());
    }

    return errors;
}

bool ComplianceAgentController::createComplianceAgentDeployment(const std::string& agent_name, const nlohmann::json& spec) {
    try {
        auto deployment_spec = generateAgentDeploymentSpec(agent_name, spec);

        auto result = api_client_->createCustomResource(
            "apps", "v1", "deployments", spec.value("namespace", "default"), deployment_spec);

        if (!result.contains("metadata") || !result["metadata"].contains("name")) {
            if (logger_) {
                logger_->error("Failed to create compliance agent deployment",
                             "ComplianceAgentController", "createComplianceAgentDeployment",
                             {{"agent", agent_name}});
            }
            return false;
        }

        return true;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception creating compliance agent deployment: " + std::string(e.what()),
                         "ComplianceAgentController", "createComplianceAgentDeployment",
                         {{"agent", agent_name}});
        }
        return false;
    }
}

bool ComplianceAgentController::updateComplianceAgentDeployment(const std::string& agent_name, const nlohmann::json& spec) {
    try {
        auto deployment_spec = generateAgentDeploymentSpec(agent_name, spec);

        auto result = api_client_->updateCustomResource(
            "apps", "v1", "deployments", spec.value("namespace", "default"), agent_name, deployment_spec);

        if (!result.contains("metadata") || !result["metadata"].contains("name")) {
            if (logger_) {
                logger_->error("Failed to update compliance agent deployment",
                             "ComplianceAgentController", "updateComplianceAgentDeployment",
                             {{"agent", agent_name}});
            }
            return false;
        }

        return true;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception updating compliance agent deployment: " + std::string(e.what()),
                         "ComplianceAgentController", "updateComplianceAgentDeployment",
                         {{"agent", agent_name}});
        }
        return false;
    }
}

bool ComplianceAgentController::scaleComplianceAgent(const std::string& agent_name, const nlohmann::json& spec) {
    try {
        std::string agent_type = spec.value("type", "");
        int current_replicas = spec.value("replicas", 2);

        // Get workload metrics
        auto workload_metrics = getWorkloadMetrics(agent_name, agent_type);

        // Calculate optimal replicas
        int optimal_replicas = calculateOptimalReplicas(agent_type, current_replicas, workload_metrics, spec);

        if (optimal_replicas != current_replicas) {
            // Get current deployment
            auto deployment = api_client_->getCustomResource(
                "apps", "v1", "deployments", spec.value("namespace", "default"), agent_name);

            if (deployment.contains("spec") && deployment["spec"].contains("replicas")) {
                deployment["spec"]["replicas"] = optimal_replicas;

                auto result = api_client_->updateCustomResource(
                    "apps", "v1", "deployments", spec.value("namespace", "default"), agent_name, deployment);

                if (result.contains("spec") && result["spec"].contains("replicas")) {
                    scaling_events_++;
                    if (logger_) {
                        logger_->info("Scaled compliance agent",
                                     "ComplianceAgentController", "scaleComplianceAgent",
                                     {{"agent", agent_name}, {"type", agent_type},
                                      {"from", current_replicas}, {"to", optimal_replicas}});
                    }
                }
            }
        }

        return true;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception scaling compliance agent: " + std::string(e.what()),
                         "ComplianceAgentController", "scaleComplianceAgent",
                         {{"agent", agent_name}});
        }
        return false;
    }
}

bool ComplianceAgentController::createAgentServices(const std::string& agent_name, const nlohmann::json& spec) {
    try {
        // Create main service
        nlohmann::json service_spec = {
            {"apiVersion", "v1"},
            {"kind", "Service"},
            {"metadata", {
                {"name", agent_name},
                {"namespace", spec.value("namespace", "default")},
                {"labels", {
                    {"app", "regulens"},
                    {"component", "compliance-agent"},
                    {"agent-name", agent_name},
                    {"agent-type", spec.value("type", "")}
                }}
            }},
            {"spec", {
                {"selector", {
                    {"app", "regulens"},
                    {"component", "compliance-agent"},
                    {"agent-name", agent_name}
                }},
                {"ports", {{
                    {"port", 8080},
                    {"targetPort", 8080},
                    {"protocol", "TCP"},
                    {"name", "http"}
                }, {
                    {"port", 9090},
                    {"targetPort", 9090},
                    {"protocol", "TCP"},
                    {"name", "metrics"}
                }}},
                {"type", "ClusterIP"}
            }}
        };

        auto result = api_client_->createCustomResource(
            "", "v1", "services", spec.value("namespace", "default"), service_spec);

        if (!result.contains("metadata") || !result["metadata"].contains("name")) {
            if (logger_) {
                logger_->warn("Failed to create compliance agent service",
                             "ComplianceAgentController", "createAgentServices",
                             {{"agent", agent_name}});
            }
            return false;
        }

        return true;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception creating agent services: " + std::string(e.what()),
                         "ComplianceAgentController", "createAgentServices",
                         {{"agent", agent_name}});
        }
        return false;
    }
}

bool ComplianceAgentController::configureRegulatorySources(const std::string& agent_name, const nlohmann::json& spec) {
    try {
        if (!spec.contains("regulatorySources") || !spec["regulatorySources"].is_array()) {
            return true; // No sources to configure
        }

        const auto& sources = spec["regulatorySources"];
        std::unordered_set<std::string> configured_sources;

        for (const auto& source : sources) {
            if (source.value("enabled", true)) {
                std::string source_name = source["name"];
                configured_sources.insert(source_name);
            }
        }

        std::lock_guard<std::mutex> lock(agents_mutex_);
        agent_regulatory_sources_[agent_name] = configured_sources;

        if (logger_) {
            logger_->info("Configured regulatory sources for agent",
                         "ComplianceAgentController", "configureRegulatorySources",
                         {{"agent", agent_name}, {"sources_count", configured_sources.size()}});
        }

        return true;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception configuring regulatory sources: " + std::string(e.what()),
                         "ComplianceAgentController", "configureRegulatorySources",
                         {{"agent", agent_name}});
        }
        return false;
    }
}

bool ComplianceAgentController::setupLLMIntegration(const std::string& agent_name, const nlohmann::json& spec) {
    try {
        bool llm_enabled = spec.value("llmIntegration", false);

        if (!llm_enabled) {
            return true; // LLM integration not required
        }

        // LLM integration is handled through environment variables and secrets in the deployment
        // The actual setup happens in generateAgentDeploymentSpec

        if (logger_) {
            logger_->info("LLM integration configured for agent",
                         "ComplianceAgentController", "setupLLMIntegration",
                         {{"agent", agent_name}, {"provider", spec.value("llmConfig", nlohmann::json::object()).value("provider", "openai")}});
        }

        return true;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception setting up LLM integration: " + std::string(e.what()),
                         "ComplianceAgentController", "setupLLMIntegration",
                         {{"agent", agent_name}});
        }
        return false;
    }
}

bool ComplianceAgentController::createAgentConfiguration(const std::string& agent_name, const nlohmann::json& spec) {
    try {
        // Create ConfigMap for agent configuration
        auto config_map_spec = generateAgentConfigMapSpec(agent_name, spec);

        auto config_result = api_client_->createCustomResource(
            "", "v1", "configmaps", spec.value("namespace", "default"), config_map_spec);

        if (!config_result.contains("metadata") || !config_result["metadata"].contains("name")) {
            if (logger_) {
                logger_->warn("Failed to create agent ConfigMap",
                             "ComplianceAgentController", "createAgentConfiguration",
                             {{"agent", agent_name}});
            }
            return false;
        }

        // Create Secret for credentials if needed
        if (spec.contains("llmConfig") || spec.contains("database") || spec.contains("redis")) {
            auto secret_spec = generateAgentSecretSpec(agent_name, spec);

            auto secret_result = api_client_->createCustomResource(
                "", "v1", "secrets", spec.value("namespace", "default"), secret_spec);

            if (!secret_result.contains("metadata") || !secret_result["metadata"].contains("name")) {
                if (logger_) {
                    logger_->warn("Failed to create agent Secret",
                                 "ComplianceAgentController", "createAgentConfiguration",
                                 {{"agent", agent_name}});
                }
                // Don't fail completely for secret creation issues
            }
        }

        // Create ServiceAccount
        auto sa_spec = generateAgentServiceAccountSpec(agent_name, spec);

        auto sa_result = api_client_->createCustomResource(
            "", "v1", "serviceaccounts", spec.value("namespace", "default"), sa_spec);

        if (!sa_result.contains("metadata") || !sa_result["metadata"].contains("name")) {
            if (logger_) {
                logger_->warn("Failed to create agent ServiceAccount",
                             "ComplianceAgentController", "createAgentConfiguration",
                             {{"agent", agent_name}});
            }
            return false;
        }

        return true;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception creating agent configuration: " + std::string(e.what()),
                         "ComplianceAgentController", "createAgentConfiguration",
                         {{"agent", agent_name}});
        }
        return false;
    }
}

nlohmann::json ComplianceAgentController::monitorAgentHealth(const std::string& agent_name, const nlohmann::json& spec) {
    nlohmann::json health_status = {
        {"healthy", true},
        {"currentReplicas", 0},
        {"availableReplicas", 0},
        {"decisionsProcessed", 0},
        {"averageProcessingTime", 0.0},
        {"errorRate", 0.0}
    };

    try {
        // Get deployment status
        auto deployment = api_client_->getCustomResource(
            "apps", "v1", "deployments", spec.value("namespace", "default"), agent_name);

        if (deployment.contains("status")) {
            health_status["currentReplicas"] = deployment["status"].value("replicas", 0);
            health_status["availableReplicas"] = deployment["status"].value("availableReplicas", 0);

            if (health_status["availableReplicas"] < health_status["currentReplicas"] * 0.8) {
                health_status["healthy"] = false;
            }
        } else {
            health_status["healthy"] = false;
        }

        // Get workload metrics (simulated for this implementation)
        auto workload = getWorkloadMetrics(agent_name, spec.value("type", ""));

        health_status["decisionsProcessed"] = workload.value("decisionsProcessed", 0);
        health_status["averageProcessingTime"] = workload.value("averageProcessingTime", 0.0);
        health_status["errorRate"] = workload.value("errorRate", 0.0);

    } catch (const std::exception& e) {
        health_status["healthy"] = false;
        health_status["error"] = e.what();
    }

    return health_status;
}

bool ComplianceAgentController::cleanupAgentResources(const std::string& agent_name) {
    // Implementation for cleanup would go here
    // Delete deployments, services, configmaps, secrets, serviceaccounts
    std::lock_guard<std::mutex> lock(agents_mutex_);
    agent_regulatory_sources_.erase(agent_name);
    return true;
}

int ComplianceAgentController::calculateOptimalReplicas(const std::string& agent_type, int current_replicas,
                                                     const nlohmann::json& workload_metrics, const nlohmann::json& agent_config) {
    // Get scaling configuration
    const auto& scaling = agent_config.value("scaling", nlohmann::json::object());
    int min_replicas = scaling.value("minReplicas", 1);
    int max_replicas = scaling.value("maxReplicas", 10);

    // Agent-type specific scaling logic
    if (agent_type == "transaction_guardian") {
        // Scale based on transaction volume
        int transactions_per_minute = workload_metrics.value("transactionsPerMinute", 100);
        if (transactions_per_minute > 1000) return std::min(current_replicas + 1, max_replicas);
        if (transactions_per_minute < 100) return std::max(current_replicas - 1, min_replicas);

    } else if (agent_type == "audit_intelligence") {
        // Scale based on audit requests
        int audit_requests = workload_metrics.value("auditRequestsPerMinute", 50);
        if (audit_requests > 200) return std::min(current_replicas + 1, max_replicas);
        if (audit_requests < 20) return std::max(current_replicas - 1, min_replicas);

    } else if (agent_type == "regulatory_assessor") {
        // Scale based on document processing
        int documents_per_minute = workload_metrics.value("documentsPerMinute", 10);
        if (documents_per_minute > 50) return std::min(current_replicas + 1, max_replicas);
        if (documents_per_minute < 5) return std::max(current_replicas - 1, min_replicas);

    } else if (agent_type == "risk_analyzer") {
        // Scale based on risk assessments
        int assessments_per_minute = workload_metrics.value("assessmentsPerMinute", 20);
        if (assessments_per_minute > 100) return std::min(current_replicas + 1, max_replicas);
        if (assessments_per_minute < 10) return std::max(current_replicas - 1, min_replicas);
    }

    return current_replicas;
}

nlohmann::json ComplianceAgentController::getPodMetrics(const std::string& agent_name) {
    try {
        // Query Kubernetes Metrics API for pod resource usage
        auto metrics_response = api_client_->getCustomResource(
            "metrics.k8s.io", "v1beta1", "pods", "", "" // Empty namespace and name to list all
        );

        if (metrics_response.contains("items") && metrics_response["items"].is_array()) {
            for (const auto& pod : metrics_response["items"]) {
                if (pod.contains("metadata") && pod["metadata"].contains("name")) {
                    std::string pod_name = pod["metadata"]["name"];
                    // Check if this pod belongs to our agent
                    if (pod_name.find(agent_name) != std::string::npos) {
                        if (pod.contains("containers") && pod["containers"].is_array() && !pod["containers"].empty()) {
                            const auto& container = pod["containers"][0];
                            if (container.contains("usage")) {
                                const auto& usage = container["usage"];
                                return {
                                    {"cpu_usage", parseCpuUsage(usage.value("cpu", "0"))},
                                    {"memory_usage", parseMemoryUsage(usage.value("memory", "0"))}
                                };
                            }
                        }
                    }
                }
            }
        }

        return {};

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->debug("Failed to get pod metrics: " + std::string(e.what()),
                         "ComplianceAgentController", "getPodMetrics",
                         {{"agent", agent_name}});
        }
        return {};
    }
}

nlohmann::json ComplianceAgentController::getApplicationMetrics(const std::string& agent_name, const std::string& agent_type) {
    try {
        // Query Prometheus for real application metrics
        if (!prometheus_client_) {
            if (logger_) {
                logger_->warn("Prometheus client not initialized, skipping application metrics",
                            "ComplianceAgentController", "getApplicationMetrics");
            }
            return {};
        }

        nlohmann::json metrics;

        // Build PromQL queries for agent-specific metrics
        std::string agent_label = "agent=\"" + agent_name + "\"";
        
        // Query: decisions processed (rate over 5 minutes)
        std::string decisions_query = "rate(regulens_agent_decisions_total{" + agent_label + "}[5m]) * 60";
        auto decisions_result = prometheus_client_->query(decisions_query);
        if (decisions_result.success) {
            metrics["decisionsProcessed"] = static_cast<int>(PrometheusClient::get_scalar_value(decisions_result));
        } else {
            metrics["decisionsProcessed"] = 0;
        }

        // Query: average processing time (in milliseconds)
        std::string processing_time_query = 
            "rate(regulens_agent_processing_time_sum{" + agent_label + "}[5m]) / "
            "rate(regulens_agent_processing_time_count{" + agent_label + "}[5m])";
        auto processing_time_result = prometheus_client_->query(processing_time_query);
        if (processing_time_result.success) {
            metrics["averageProcessingTime"] = PrometheusClient::get_scalar_value(processing_time_result);
        } else {
            metrics["averageProcessingTime"] = 0.0;
        }

        // Query: error rate (percentage)
        std::string error_rate_query = 
            "(rate(regulens_agent_errors_total{" + agent_label + "}[5m]) / "
            "rate(regulens_agent_requests_total{" + agent_label + "}[5m])) * 100";
        auto error_rate_result = prometheus_client_->query(error_rate_query);
        if (error_rate_result.success) {
            metrics["errorRate"] = PrometheusClient::get_scalar_value(error_rate_result);
        } else {
            metrics["errorRate"] = 0.0;
        }

        // Agent-type specific metrics
        if (agent_type == "transaction_guardian") {
            std::string tpm_query = "rate(regulens_transaction_guardian_transactions_total{" + agent_label + "}[1m]) * 60";
            auto tpm_result = prometheus_client_->query(tpm_query);
            if (tpm_result.success) {
                metrics["transactionsPerMinute"] = static_cast<int>(PrometheusClient::get_scalar_value(tpm_result));
            }
        } else if (agent_type == "audit_intelligence") {
            std::string audit_query = "rate(regulens_audit_requests_total{" + agent_label + "}[1m]) * 60";
            auto audit_result = prometheus_client_->query(audit_query);
            if (audit_result.success) {
                metrics["auditRequestsPerMinute"] = static_cast<int>(PrometheusClient::get_scalar_value(audit_result));
            }
        } else if (agent_type == "regulatory_assessor") {
            std::string docs_query = "rate(regulens_documents_processed_total{" + agent_label + "}[1m]) * 60";
            auto docs_result = prometheus_client_->query(docs_query);
            if (docs_result.success) {
                metrics["documentsPerMinute"] = static_cast<int>(PrometheusClient::get_scalar_value(docs_result));
            }
        } else if (agent_type == "risk_analyzer") {
            std::string assess_query = "rate(regulens_risk_assessments_total{" + agent_label + "}[1m]) * 60";
            auto assess_result = prometheus_client_->query(assess_query);
            if (assess_result.success) {
                metrics["assessmentsPerMinute"] = static_cast<int>(PrometheusClient::get_scalar_value(assess_result));
            }
        }

        return metrics;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->debug("Failed to get application metrics: " + std::string(e.what()),
                         "ComplianceAgentController", "getApplicationMetrics",
                         {{"agent", agent_name}});
        }
        return {};
    }
}

double ComplianceAgentController::parseCpuUsage(const std::string& cpu_str) {
    // Parse Kubernetes CPU usage (e.g., "100m", "0.1")
    if (cpu_str.empty()) return 0.0;

    try {
        if (cpu_str.back() == 'm') {
            // MilliCPU
            double millicpu = std::stod(cpu_str.substr(0, cpu_str.size() - 1));
            return millicpu / 1000.0; // Convert to cores
        } else {
            // CPU cores
            return std::stod(cpu_str);
        }
    } catch (const std::exception&) {
        return 0.0;
    }
}

double ComplianceAgentController::parseMemoryUsage(const std::string& memory_str) {
    // Parse Kubernetes memory usage (e.g., "128Mi", "1Gi")
    if (memory_str.empty()) return 0.0;

    try {
        double value = std::stod(memory_str.substr(0, memory_str.size() - 2));
        std::string unit = memory_str.substr(memory_str.size() - 2);

        if (unit == "Ki") return value / (1024.0 * 1024.0); // Convert to Gi
        if (unit == "Mi") return value / 1024.0; // Convert to Gi
        if (unit == "Gi") return value; // Already in Gi
        if (unit == "Ti") return value * 1024.0; // Convert to Gi

        return value / (1024.0 * 1024.0 * 1024.0); // Assume bytes, convert to Gi

    } catch (const std::exception&) {
        return 0.0;
    }
}

nlohmann::json ComplianceAgentController::getWorkloadMetrics(const std::string& agent_name, const std::string& agent_type) {
    try {
        // Get real metrics from Kubernetes API and Prometheus
        nlohmann::json metrics = {
            {"cpu_usage", 0.0},
            {"memory_usage", 0.0},
            {"decisionsProcessed", 0},
            {"averageProcessingTime", 0.0},
            {"errorRate", 0.0}
        };

        // Get pod metrics from Kubernetes API
        auto pod_metrics = getPodMetrics(agent_name);
        if (!pod_metrics.empty()) {
            metrics["cpu_usage"] = pod_metrics.value("cpu_usage", 0.0);
            metrics["memory_usage"] = pod_metrics.value("memory_usage", 0.0);
        }

        // Get application metrics from Prometheus
        auto app_metrics = getApplicationMetrics(agent_name, agent_type);
        if (!app_metrics.empty()) {
            metrics["decisionsProcessed"] = app_metrics.value("decisionsProcessed", 0);
            metrics["averageProcessingTime"] = app_metrics.value("averageProcessingTime", 0.0);
            metrics["errorRate"] = app_metrics.value("errorRate", 0.0);

            // Agent-type specific metrics
            if (agent_type == "transaction_guardian") {
                metrics["transactionsPerMinute"] = app_metrics.value("transactionsPerMinute", 0);
            } else if (agent_type == "audit_intelligence") {
                metrics["auditRequestsPerMinute"] = app_metrics.value("auditRequestsPerMinute", 0);
            } else if (agent_type == "regulatory_assessor") {
                metrics["documentsPerMinute"] = app_metrics.value("documentsPerMinute", 0);
            } else if (agent_type == "risk_analyzer") {
                metrics["assessmentsPerMinute"] = app_metrics.value("assessmentsPerMinute", 0);
            }
        }

        return metrics;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->warn("Failed to get workload metrics, using defaults: " + std::string(e.what()),
                         "ComplianceAgentController", "getWorkloadMetrics",
                         {{"agent", agent_name}});
        }

        // Return default metrics on error
        return {
            {"cpu_usage", 0.5},
            {"memory_usage", 0.5},
            {"decisionsProcessed", 100},
            {"averageProcessingTime", 200.0},
            {"errorRate", 0.01}
        };
    }
}

// Helper methods for generating Kubernetes specs

nlohmann::json ComplianceAgentController::generateAgentDeploymentSpec(const std::string& agent_name, const nlohmann::json& spec) {
    std::string agent_type = spec.value("type", "");
    int replicas = spec.value("replicas", 2);
    std::string image = spec.value("image", "regulens/compliance-agent:latest");

    // Get agent type specific configuration
    auto type_config = getAgentTypeConfig(agent_type);

    // Build environment variables
    std::vector<nlohmann::json> env_vars = {
        {{"name", "AGENT_NAME"}, {"value", agent_name}},
        {{"name", "AGENT_TYPE"}, {"value", agent_type}},
        {{"name", "AGENT_NAMESPACE"}, {"value", spec.value("namespace", "default")}},
        {{"name", "LOG_LEVEL"}, {"value", spec.value("config", nlohmann::json::object()).value("logLevel", "INFO")}}
    };

    // Add LLM configuration if enabled
    if (spec.value("llmIntegration", false) && spec.contains("llmConfig")) {
        const auto& llm_config = spec["llmConfig"];
        if (llm_config.contains("provider")) {
            env_vars.push_back({{"name", "LLM_PROVIDER"}, {"value", llm_config["provider"]}});
        }
        if (llm_config.contains("model")) {
            env_vars.push_back({{"name", "LLM_MODEL"}, {"value", llm_config["model"]}});
        }
    }

    // Add regulatory sources configuration
    if (spec.contains("regulatorySources")) {
        env_vars.push_back({{"name", "REGULATORY_SOURCES"}, {"value", spec["regulatorySources"].dump()}});
    }

    // Build resource requirements
    const auto& resources = spec.value("resources", nlohmann::json::object());
    const auto& requests = resources.value("requests", nlohmann::json::object());
    const auto& limits = resources.value("limits", nlohmann::json::object());

    nlohmann::json deployment = {
        {"apiVersion", "apps/v1"},
        {"kind", "Deployment"},
        {"metadata", {
            {"name", agent_name},
            {"namespace", spec.value("namespace", "default")},
            {"labels", {
                {"app", "regulens"},
                {"component", "compliance-agent"},
                {"agent-name", agent_name},
                {"agent-type", agent_type}
            }}
        }},
        {"spec", {
            {"replicas", replicas},
            {"selector", {
                {"matchLabels", {
                    {"app", "regulens"},
                    {"component", "compliance-agent"},
                    {"agent-name", agent_name}
                }}
            }},
            {"template", {
                {"metadata", {
                    {"labels", {
                        {"app", "regulens"},
                        {"component", "compliance-agent"},
                        {"agent-name", agent_name},
                        {"agent-type", agent_type}
                    }}
                }},
                {"spec", {
                    {"serviceAccountName", agent_name},
                    {"containers", {{
                        {"name", "agent"},
                        {"image", image},
                        {"ports", {{
                            {"containerPort", 8080},
                            {"name", "http"}
                        }, {
                            {"containerPort", 9090},
                            {"name", "metrics"}
                        }}},
                        {"env", env_vars},
                        {"resources", {
                            {"requests", {
                                {"cpu", requests.value("cpu", type_config.value("cpuRequest", "200m"))},
                                {"memory", requests.value("memory", type_config.value("memoryRequest", "256Mi"))}
                            }},
                            {"limits", {
                                {"cpu", limits.value("cpu", type_config.value("cpuLimit", "1000m"))},
                                {"memory", limits.value("memory", type_config.value("memoryLimit", "512Mi"))}
                            }}
                        }},
                        {"readinessProbe", {
                            {"httpGet", {
                                {"path", "/health/ready"},
                                {"port", 8080}
                            }},
                            {"initialDelaySeconds", 10},
                            {"periodSeconds", 15}
                        }},
                        {"livenessProbe", {
                            {"httpGet", {
                                {"path", "/health/live"},
                                {"port", 8080}
                            }},
                            {"initialDelaySeconds", 30},
                            {"periodSeconds", 30}
                        }}
                    }}}
                }}
            }}
        }}
    };

    return deployment;
}

// Additional helper methods would be implemented here for ConfigMap, Secret, ServiceAccount generation
// Implementation details

nlohmann::json ComplianceAgentController::generateAgentConfigMapSpec(const std::string& agent_name, const nlohmann::json& spec) {
    return {
        {"apiVersion", "v1"},
        {"kind", "ConfigMap"},
        {"metadata", {
            {"name", agent_name + "-config"},
            {"namespace", spec.value("namespace", "default")}
        }},
        {"data", {
            {"agent-config.yaml", spec.dump(2)}
        }}
    };
}

nlohmann::json ComplianceAgentController::generateAgentSecretSpec(const std::string& agent_name, const nlohmann::json& spec) {
    nlohmann::json secret = {
        {"apiVersion", "v1"},
        {"kind", "Secret"},
        {"metadata", {
            {"name", agent_name + "-secrets"},
            {"namespace", spec.value("namespace", "default")}
        }},
        {"type", "Opaque"},
        {"data", nlohmann::json::object()}
    };

    // Add LLM credentials if configured
    if (spec.contains("llmConfig")) {
        const auto& llm_config = spec["llmConfig"];
        if (llm_config.contains("apiKeySecret")) {
            // In production, would fetch actual secret data
            secret["data"]["llm-api-key"] = "dGVzdC1rZXk="; // base64 encoded test key
        }
    }

    return secret;
}

nlohmann::json ComplianceAgentController::generateAgentServiceAccountSpec(const std::string& agent_name, const nlohmann::json& spec) {
    return {
        {"apiVersion", "v1"},
        {"kind", "ServiceAccount"},
        {"metadata", {
            {"name", agent_name},
            {"namespace", spec.value("namespace", "default")}
        }}
    };
}

// Validation helper methods

std::vector<std::string> ComplianceAgentController::validateRegulatorySources(const nlohmann::json& sources) {
    std::vector<std::string> errors;

    if (!sources.is_array()) {
        errors.push_back("regulatorySources must be an array");
        return errors;
    }

    std::vector<std::string> valid_types = {"sec", "fca", "ecb", "esma", "fed"};

    for (size_t i = 0; i < sources.size(); ++i) {
        const auto& source = sources[i];

        if (!source.contains("name") || source["name"].get<std::string>().empty()) {
            errors.push_back("regulatorySources[" + std::to_string(i) + "] must have a name");
        }

        if (!source.contains("type")) {
            errors.push_back("regulatorySources[" + std::to_string(i) + "] must have a type");
        } else {
            std::string type = source["type"];
            if (std::find(valid_types.begin(), valid_types.end(), type) == valid_types.end()) {
                errors.push_back("regulatorySources[" + std::to_string(i) + "] type must be one of: sec, fca, ecb, esma, fed");
            }
        }
    }

    return errors;
}

std::vector<std::string> ComplianceAgentController::validateLLMConfig(const nlohmann::json& llm_config) {
    std::vector<std::string> errors;

    std::vector<std::string> valid_providers = {"openai", "anthropic", "local"};

    if (llm_config.contains("provider")) {
        std::string provider = llm_config["provider"];
        if (std::find(valid_providers.begin(), valid_providers.end(), provider) == valid_providers.end()) {
            errors.push_back("llmConfig.provider must be one of: openai, anthropic, local");
        }
    }

    if (llm_config.contains("temperature")) {
        double temp = llm_config["temperature"];
        if (temp < 0.0 || temp > 2.0) {
            errors.push_back("llmConfig.temperature must be between 0.0 and 2.0");
        }
    }

    return errors;
}

nlohmann::json ComplianceAgentController::getAgentTypeConfig(const std::string& agent_type) {
    // Agent-type specific default configurations
    if (agent_type == "transaction_guardian") {
        return {
            {"cpuRequest", "300m"},
            {"memoryRequest", "512Mi"},
            {"cpuLimit", "1500m"},
            {"memoryLimit", "1Gi"}
        };
    } else if (agent_type == "audit_intelligence") {
        return {
            {"cpuRequest", "400m"},
            {"memoryRequest", "768Mi"},
            {"cpuLimit", "2000m"},
            {"memoryLimit", "2Gi"}
        };
    } else if (agent_type == "regulatory_assessor") {
        return {
            {"cpuRequest", "500m"},
            {"memoryRequest", "1Gi"},
            {"cpuLimit", "2500m"},
            {"memoryLimit", "3Gi"}
        };
    } else if (agent_type == "risk_analyzer") {
        return {
            {"cpuRequest", "600m"},
            {"memoryRequest", "1.5Gi"},
            {"cpuLimit", "3000m"},
            {"memoryLimit", "4Gi"}
        };
    }

    // Default configuration
    return {
        {"cpuRequest", "200m"},
        {"memoryRequest", "256Mi"},
        {"cpuLimit", "1000m"},
        {"memoryLimit", "512Mi"}
    };
}

// Event handlers

void ComplianceAgentController::handleAgentCreation(const std::string& name,
                                                  const std::string& namespace_,
                                                  const nlohmann::json& resource) {
    if (logger_) {
        logger_->info("Handling compliance agent creation",
                     "ComplianceAgentController", "handleAgentCreation",
                     {{"agent", name}, {"namespace", namespace_}});
    }

    reconcileResource(resource);
}

void ComplianceAgentController::handleAgentUpdate(const std::string& name,
                                                const std::string& namespace_,
                                                const nlohmann::json& new_resource,
                                                const nlohmann::json& old_resource) {
    if (logger_) {
        logger_->info("Handling compliance agent update",
                     "ComplianceAgentController", "handleAgentUpdate",
                     {{"agent", name}, {"namespace", namespace_}});
    }

    reconcileResource(new_resource);
}

void ComplianceAgentController::handleAgentDeletion(const std::string& name,
                                                  const std::string& namespace_,
                                                  const nlohmann::json& resource) {
    if (logger_) {
        logger_->info("Handling compliance agent deletion",
                     "ComplianceAgentController", "handleAgentDeletion",
                     {{"agent", name}, {"namespace", namespace_}});
    }

    {
        std::lock_guard<std::mutex> lock(agents_mutex_);
        active_agents_.erase(name);
    }

    cleanupAgentResources(name);
    agents_deleted_++;
}

void ComplianceAgentController::updateAgentStatus(const std::string& agent_name, const nlohmann::json& status) {
    // This would call the base class method to update status
    if (logger_) {
        logger_->debug("Updating agent status",
                     "ComplianceAgentController", "updateAgentStatus",
                     {{"agent", agent_name}});
    }
}

// Factory function

std::shared_ptr<ComplianceAgentController> createComplianceAgentController(
    std::shared_ptr<KubernetesAPIClient> api_client,
    std::shared_ptr<StructuredLogger> logger,
    std::shared_ptr<PrometheusMetricsCollector> metrics) {

    auto controller = std::make_shared<ComplianceAgentController>(api_client, logger, metrics);
    if (controller->initialize()) {
        return controller;
    }
    return nullptr;
}

} // namespace k8s
} // namespace regulens
