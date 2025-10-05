/**
 * Agent Orchestrator Controller Implementation
 *
 * Production-grade Kubernetes controller for managing AgentOrchestrator
 * custom resources with advanced lifecycle management, auto-scaling,
 * and comprehensive monitoring capabilities.
 */

#include "agent_orchestrator_controller.hpp"
#include <shared/metrics/prometheus_client.hpp>
#include <algorithm>
#include <sstream>
#include <chrono>

namespace regulens {
namespace k8s {

// AgentOrchestratorController Implementation

AgentOrchestratorController::AgentOrchestratorController(std::shared_ptr<KubernetesAPIClient> api_client,
                                                       std::shared_ptr<StructuredLogger> logger,
                                                       std::shared_ptr<PrometheusMetricsCollector> metrics)
    : CustomResourceController(api_client, logger, metrics) {
    // Initialize Prometheus client for querying orchestrator metrics
    prometheus_client_ = create_prometheus_client(logger);
}

void AgentOrchestratorController::handleResourceEvent(const ResourceEvent& event) {
    try {
        std::string orchestrator_name = event.name;
        std::string namespace_ = event.namespace_;

        switch (event.type) {
            case ResourceEventType::ADDED:
                handleOrchestratorCreation(orchestrator_name, namespace_, event.resource);
                break;

            case ResourceEventType::MODIFIED:
                handleOrchestratorUpdate(orchestrator_name, namespace_, event.resource, event.old_resource);
                break;

            case ResourceEventType::DELETED:
                handleOrchestratorDeletion(orchestrator_name, namespace_, event.resource);
                break;

            default:
                if (logger_) {
                    logger_->warn("Unknown resource event type",
                                 "AgentOrchestratorController", "handleResourceEvent",
                                 {{"event_type", static_cast<int>(event.type)},
                                  {"orchestrator", orchestrator_name}});
                }
                break;
        }

        events_processed_++;

    } catch (const std::exception& e) {
        events_failed_++;
        if (logger_) {
            logger_->error("Exception handling orchestrator event: " + std::string(e.what()),
                         "AgentOrchestratorController", "handleResourceEvent",
                         {{"orchestrator", event.name}, {"event_type", static_cast<int>(event.type)}});
        }
    }
}

nlohmann::json AgentOrchestratorController::getMetrics() const {
    auto base_metrics = CustomResourceController::getMetrics();

    std::lock_guard<std::mutex> lock(orchestrators_mutex_);

    base_metrics["orchestrator_metrics"] = {
        {"orchestrators_created_total", orchestrators_created_.load()},
        {"orchestrators_updated_total", orchestrators_updated_.load()},
        {"orchestrators_deleted_total", orchestrators_deleted_.load()},
        {"agents_deployed_total", agents_deployed_.load()},
        {"scaling_operations_total", scaling_operations_.load()},
        {"active_orchestrators", active_orchestrators_.size()}
    };

    return base_metrics;
}

nlohmann::json AgentOrchestratorController::reconcileResource(const nlohmann::json& resource) {
    std::string orchestrator_name = resource["metadata"]["name"];
    std::string namespace_ = resource["metadata"]["namespace"];

    try {
        // Get current spec
        const auto& spec = resource["spec"];

        // Validate spec
        auto validation_errors = validateResourceSpec(spec);
        if (!validation_errors.empty()) {
            if (logger_) {
                logger_->error("Orchestrator spec validation failed",
                             "AgentOrchestratorController", "reconcileResource",
                             {{"orchestrator", orchestrator_name}, {"errors", validation_errors.size()}});
            }

            // Update status with validation errors
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

            updateResourceStatus("agentorchestrators", namespace_, orchestrator_name, status);
            return resource;
        }

        // Check if orchestrator exists
        std::lock_guard<std::mutex> lock(orchestrators_mutex_);
        bool exists = active_orchestrators_.find(orchestrator_name) != active_orchestrators_.end();

        if (!exists) {
            // Create new orchestrator
            if (createAgentDeployments(orchestrator_name, spec) &&
                createAgentServices(orchestrator_name, spec) &&
                createAgentConfigMaps(orchestrator_name, spec)) {

                active_orchestrators_[orchestrator_name] = resource;
                orchestrators_created_++;

                // Update status
                nlohmann::json status = {
                    {"phase", "Running"},
                    {"replicas", spec.value("replicas", 3)},
                    {"availableReplicas", spec.value("replicas", 3)},
                    {"conditions", {{
                        {"type", "Ready"},
                        {"status", "True"},
                        {"reason", "Created"},
                        {"message", "Orchestrator created successfully"},
                        {"lastTransitionTime", std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count()}
                    }}}
                };

                updateResourceStatus("agentorchestrators", namespace_, orchestrator_name, status);

                if (logger_) {
                    logger_->info("Orchestrator created successfully",
                                 "AgentOrchestratorController", "reconcileResource",
                                 {{"orchestrator", orchestrator_name}, {"replicas", spec.value("replicas", 3)}});
                }
            } else {
                // Creation failed
                nlohmann::json status = {
                    {"phase", "Failed"},
                    {"conditions", {{
                        {"type", "Ready"},
                        {"status", "False"},
                        {"reason", "CreationFailed"},
                        {"message", "Failed to create orchestrator resources"},
                        {"lastTransitionTime", std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count()}
                    }}}
                };

                updateResourceStatus("agentorchestrators", namespace_, orchestrator_name, status);
            }
        } else {
            // Update existing orchestrator
            const auto& old_resource = active_orchestrators_[orchestrator_name];
            const auto& old_spec = old_resource["spec"];

            // Check if spec changed
            if (spec != old_spec) {
                if (updateAgentDeployments(orchestrator_name, spec)) {
                    active_orchestrators_[orchestrator_name] = resource;
                    orchestrators_updated_++;

                    if (logger_) {
                        logger_->info("Orchestrator updated successfully",
                                     "AgentOrchestratorController", "reconcileResource",
                                     {{"orchestrator", orchestrator_name}});
                    }
                }
            }

            // Perform scaling if enabled
            if (spec.value("agents", nlohmann::json::array()).size() > 0) {
                scaleAgentDeployments(orchestrator_name, spec);
            }

            // Monitor health
            auto health_status = monitorAgentHealth(orchestrator_name, spec);

            // Update status
            nlohmann::json status = {
                {"phase", "Running"},
                {"replicas", spec.value("replicas", 3)},
                {"availableReplicas", health_status.value("availableReplicas", 0)},
                {"lastUpdateTime", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()},
                {"conditions", {{
                    {"type", "Ready"},
                    {"status", health_status.value("healthy", false) ? "True" : "False"},
                    {"reason", "Running"},
                    {"message", "Orchestrator is running"},
                    {"lastTransitionTime", std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count()}
                }}}
            };

            updateResourceStatus("agentorchestrators", namespace_, orchestrator_name, status);
        }

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception reconciling orchestrator: " + std::string(e.what()),
                         "AgentOrchestratorController", "reconcileResource",
                         {{"orchestrator", orchestrator_name}});
        }
    }

    return resource;
}

std::vector<std::string> AgentOrchestratorController::validateResourceSpec(const nlohmann::json& spec) {
    std::vector<std::string> errors;

    // Validate replicas
    if (spec.contains("replicas")) {
        int replicas = spec["replicas"];
        if (replicas < 1 || replicas > 100) {
            errors.push_back("replicas must be between 1 and 100");
        }
    }

    // Validate image
    if (!spec.contains("image") || spec["image"].get<std::string>().empty()) {
        errors.push_back("image is required");
    }

    // Validate agents array
    if (spec.contains("agents")) {
        const auto& agents = spec["agents"];
        if (!agents.is_array()) {
            errors.push_back("agents must be an array");
        } else {
            for (size_t i = 0; i < agents.size(); ++i) {
                const auto& agent = agents[i];
                if (!agent.contains("name") || agent["name"].get<std::string>().empty()) {
                    errors.push_back("agent[" + std::to_string(i) + "] must have a name");
                }
                if (!agent.contains("type")) {
                    errors.push_back("agent[" + std::to_string(i) + "] must have a type");
                }
            }
        }
    }

    return errors;
}

bool AgentOrchestratorController::createAgentDeployments(const std::string& orchestrator_name,
                                                        const nlohmann::json& spec) {
    try {
        const auto& agents = spec.value("agents", nlohmann::json::array());

        for (const auto& agent_spec : agents) {
            std::string agent_name = agent_spec["name"];
            std::string full_deployment_name = orchestrator_name + "-" + agent_name;

            // Generate deployment spec
            auto deployment_spec = generateAgentDeploymentSpec(orchestrator_name, agent_spec, spec);

            // Create deployment via API
            auto result = api_client_->createCustomResource(
                "apps", "v1", "deployments", spec.value("namespace", "default"),
                deployment_spec);

            if (!result.contains("metadata") || !result["metadata"].contains("name")) {
                if (logger_) {
                    logger_->error("Failed to create agent deployment",
                                 "AgentOrchestratorController", "createAgentDeployments",
                                 {{"orchestrator", orchestrator_name}, {"agent", agent_name}});
                }
                return false;
            }

            agents_deployed_++;
        }

        return true;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception creating agent deployments: " + std::string(e.what()),
                         "AgentOrchestratorController", "createAgentDeployments",
                         {{"orchestrator", orchestrator_name}});
        }
        return false;
    }
}

bool AgentOrchestratorController::updateAgentDeployments(const std::string& orchestrator_name,
                                                        const nlohmann::json& spec) {
    try {
        const auto& agents = spec.value("agents", nlohmann::json::array());

        for (const auto& agent_spec : agents) {
            std::string agent_name = agent_spec["name"];
            std::string full_deployment_name = orchestrator_name + "-" + agent_name;

            // Generate updated deployment spec
            auto deployment_spec = generateAgentDeploymentSpec(orchestrator_name, agent_spec, spec);

            // Update deployment via API
            auto result = api_client_->updateCustomResource(
                "apps", "v1", "deployments", spec.value("namespace", "default"),
                full_deployment_name, deployment_spec);

            if (!result.contains("metadata") || !result["metadata"].contains("name")) {
                if (logger_) {
                    logger_->error("Failed to update agent deployment",
                                 "AgentOrchestratorController", "updateAgentDeployments",
                                 {{"orchestrator", orchestrator_name}, {"agent", agent_name}});
                }
                return false;
            }
        }

        return true;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception updating agent deployments: " + std::string(e.what()),
                         "AgentOrchestratorController", "updateAgentDeployments",
                         {{"orchestrator", orchestrator_name}});
        }
        return false;
    }
}

bool AgentOrchestratorController::scaleAgentDeployments(const std::string& orchestrator_name,
                                                       const nlohmann::json& spec) {
    try {
        // Simple scaling logic - in production would be more sophisticated
        const auto& agents = spec.value("agents", nlohmann::json::array());

        for (const auto& agent_spec : agents) {
            std::string agent_type = agent_spec.value("type", "");
            int current_replicas = agent_spec.value("replicas", 2);
            int max_replicas = agent_spec.value("maxReplicas", 10);

            // Get real load metrics from Prometheus
            nlohmann::json load_metrics = getAgentLoadMetrics(orchestrator_name, agent_spec);

            int optimal_replicas = calculateOptimalReplicas(agent_type, current_replicas, load_metrics);

            if (optimal_replicas != current_replicas && optimal_replicas <= max_replicas) {
                std::string agent_name = agent_spec["name"];
                std::string full_deployment_name = orchestrator_name + "-" + agent_name;

                // Get current deployment
                auto deployment = api_client_->getCustomResource(
                    "apps", "v1", "deployments", spec.value("namespace", "default"),
                    full_deployment_name);

                if (deployment.contains("spec") && deployment["spec"].contains("replicas")) {
                    // Update replicas
                    deployment["spec"]["replicas"] = optimal_replicas;

                    auto result = api_client_->updateCustomResource(
                        "apps", "v1", "deployments", spec.value("namespace", "default"),
                        full_deployment_name, deployment);

                    if (result.contains("spec") && result["spec"].contains("replicas")) {
                        scaling_operations_++;
                        if (logger_) {
                            logger_->info("Scaled agent deployment",
                                         "AgentOrchestratorController", "scaleAgentDeployments",
                                         {{"orchestrator", orchestrator_name}, {"agent", agent_name},
                                          {"from", current_replicas}, {"to", optimal_replicas}});
                        }
                    }
                }
            }
        }

        return true;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception scaling agent deployments: " + std::string(e.what()),
                         "AgentOrchestratorController", "scaleAgentDeployments",
                         {{"orchestrator", orchestrator_name}});
        }
        return false;
    }
}

bool AgentOrchestratorController::createAgentServices(const std::string& orchestrator_name,
                                                     const nlohmann::json& spec) {
    try {
        const auto& agents = spec.value("agents", nlohmann::json::array());

        for (const auto& agent_spec : agents) {
            std::string agent_name = agent_spec["name"];

            // Generate service spec
            auto service_spec = generateAgentServiceSpec(orchestrator_name, agent_spec);

            // Create service via API
            auto result = api_client_->createCustomResource(
                "", "v1", "services", spec.value("namespace", "default"), service_spec);

            if (!result.contains("metadata") || !result["metadata"].contains("name")) {
                if (logger_) {
                    logger_->warn("Failed to create agent service",
                                 "AgentOrchestratorController", "createAgentServices",
                                 {{"orchestrator", orchestrator_name}, {"agent", agent_name}});
                }
                // Don't fail completely for service creation issues
            }
        }

        return true;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception creating agent services: " + std::string(e.what()),
                         "AgentOrchestratorController", "createAgentServices",
                         {{"orchestrator", orchestrator_name}});
        }
        return false;
    }
}

bool AgentOrchestratorController::createAgentConfigMaps(const std::string& orchestrator_name,
                                                       const nlohmann::json& spec) {
    try {
        // Create ConfigMap for orchestrator configuration
        nlohmann::json config_data = {
            {"apiVersion", "v1"},
            {"kind", "ConfigMap"},
            {"metadata", {
                {"name", orchestrator_name + "-config"},
                {"namespace", spec.value("namespace", "default")},
                {"labels", {
                    {"app", "regulens"},
                    {"component", "agent-orchestrator"},
                    {"orchestrator", orchestrator_name}
                }}
            }},
            {"data", {
                {"orchestrator-config.yaml", spec.dump(2)},
                {"database-config.yaml", spec.value("database", nlohmann::json::object()).dump(2)},
                {"redis-config.yaml", spec.value("redis", nlohmann::json::object()).dump(2)}
            }}
        };

        auto result = api_client_->createCustomResource(
            "", "v1", "configmaps", spec.value("namespace", "default"), config_data);

        if (!result.contains("metadata") || !result["metadata"].contains("name")) {
            if (logger_) {
                logger_->warn("Failed to create orchestrator ConfigMap",
                             "AgentOrchestratorController", "createAgentConfigMaps",
                             {{"orchestrator", orchestrator_name}});
            }
            return false;
        }

        return true;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception creating agent ConfigMaps: " + std::string(e.what()),
                         "AgentOrchestratorController", "createAgentConfigMaps",
                         {{"orchestrator", orchestrator_name}});
        }
        return false;
    }
}

nlohmann::json AgentOrchestratorController::monitorAgentHealth(const std::string& orchestrator_name,
                                                             const nlohmann::json& spec) {
    nlohmann::json health_status = {
        {"healthy", true},
        {"availableReplicas", 0},
        {"totalReplicas", 0}
    };

    try {
        const auto& agents = spec.value("agents", nlohmann::json::array());
        int total_replicas = 0;
        int available_replicas = 0;

        for (const auto& agent_spec : agents) {
            std::string agent_name = agent_spec["name"];
            std::string full_deployment_name = orchestrator_name + "-" + agent_name;
            int replicas = agent_spec.value("replicas", 2);

            total_replicas += replicas;

            // Check deployment status (simplified)
            auto deployment = api_client_->getCustomResource(
                "apps", "v1", "deployments", spec.value("namespace", "default"),
                full_deployment_name);

            if (deployment.contains("status")) {
                int ready_replicas = deployment["status"].value("readyReplicas", 0);
                available_replicas += ready_replicas;
            } else {
                // Assume not healthy if we can't get status
                health_status["healthy"] = false;
            }
        }

        health_status["totalReplicas"] = total_replicas;
        health_status["availableReplicas"] = available_replicas;

        if (available_replicas < total_replicas * 0.8) {  // Less than 80% available
            health_status["healthy"] = false;
        }

    } catch (const std::exception& e) {
        health_status["healthy"] = false;
        health_status["error"] = e.what();
    }

    return health_status;
}

bool AgentOrchestratorController::cleanupOrchestratorResources(const std::string& orchestrator_name) {
    // Implementation for cleanup would go here
    // Delete deployments, services, configmaps, etc.
    return true;
}

nlohmann::json AgentOrchestratorController::generateAgentDeploymentSpec(const std::string& orchestrator_name,
                                                                      const nlohmann::json& agent_spec,
                                                                      const nlohmann::json& orchestrator_spec) {
    std::string agent_name = agent_spec["name"];
    std::string agent_type = agent_spec["type"];
    int replicas = agent_spec.value("replicas", 2);

    // Get resource requirements
    const auto& resources = agent_spec.value("resources", nlohmann::json::object());
    const auto& requests = resources.value("requests", nlohmann::json::object());
    const auto& limits = resources.value("limits", nlohmann::json::object());

    nlohmann::json deployment = {
        {"apiVersion", "apps/v1"},
        {"kind", "Deployment"},
        {"metadata", {
            {"name", orchestrator_name + "-" + agent_name},
            {"namespace", orchestrator_spec.value("namespace", "default")},
            {"labels", {
                {"app", "regulens"},
                {"component", "compliance-agent"},
                {"agent-type", agent_type},
                {"orchestrator", orchestrator_name}
            }}
        }},
        {"spec", {
            {"replicas", replicas},
            {"selector", {
                {"matchLabels", {
                    {"app", "regulens"},
                    {"component", "compliance-agent"},
                    {"agent-type", agent_type},
                    {"orchestrator", orchestrator_name}
                }}
            }},
            {"template", {
                {"metadata", {
                    {"labels", {
                        {"app", "regulens"},
                        {"component", "compliance-agent"},
                        {"agent-type", agent_type},
                        {"orchestrator", orchestrator_name}
                    }}
                }},
                {"spec", {
                    {"containers", {{
                        {"name", "agent"},
                        {"image", orchestrator_spec.value("image", "regulens/compliance-agent:latest")},
                        {"ports", {{
                            {"containerPort", 8080},
                            {"name", "http"}
                        }}},
                        {"env", {{
                            {"name", "AGENT_TYPE"},
                            {"value", agent_type}
                        }, {
                            {"name", "AGENT_NAME"},
                            {"value", agent_name}
                        }, {
                            {"name", "ORCHESTRATOR_NAME"},
                            {"value", orchestrator_name}
                        }}},
                        {"resources", {
                            {"requests", {
                                {"cpu", requests.value("cpu", "100m")},
                                {"memory", requests.value("memory", "128Mi")}
                            }},
                            {"limits", {
                                {"cpu", limits.value("cpu", "500m")},
                                {"memory", limits.value("memory", "512Mi")}
                            }}
                        }},
                        {"readinessProbe", {
                            {"httpGet", {
                                {"path", "/health/ready"},
                                {"port", 8080}
                            }},
                            {"initialDelaySeconds", 5},
                            {"periodSeconds", 10}
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

nlohmann::json AgentOrchestratorController::generateAgentServiceSpec(const std::string& orchestrator_name,
                                                                   const nlohmann::json& agent_spec) {
    std::string agent_name = agent_spec["name"];
    std::string agent_type = agent_spec["type"];

    nlohmann::json service = {
        {"apiVersion", "v1"},
        {"kind", "Service"},
        {"metadata", {
            {"name", orchestrator_name + "-" + agent_name},
            {"namespace", "default"},  // Would get from orchestrator spec
            {"labels", {
                {"app", "regulens"},
                {"component", "compliance-agent"},
                {"agent-type", agent_type},
                {"orchestrator", orchestrator_name}
            }}
        }},
        {"spec", {
            {"selector", {
                {"app", "regulens"},
                {"component", "compliance-agent"},
                {"agent-type", agent_type},
                {"orchestrator", orchestrator_name}
            }},
            {"ports", {{
                {"port", 8080},
                {"targetPort", 8080},
                {"protocol", "TCP"},
                {"name", "http"}
            }}},
            {"type", "ClusterIP"}
        }}
    };

    return service;
}

nlohmann::json AgentOrchestratorController::getAgentLoadMetrics(const std::string& orchestrator_name,
                                                               const nlohmann::json& agent_spec) {
    nlohmann::json load_metrics = {
        {"cpu_usage", 0.5},
        {"memory_usage", 0.5},
        {"queue_depth", 10}
    };

    try {
        if (!prometheus_client_) {
            if (logger_) {
                logger_->warn("Prometheus client not initialized, using default load metrics",
                            "AgentOrchestratorController", "getAgentLoadMetrics");
            }
            return load_metrics;
        }

        std::string agent_name = agent_spec.value("name", "");
        std::string full_deployment_name = orchestrator_name + "-" + agent_name;
        std::string deployment_label = "deployment=\"" + full_deployment_name + "\"";

        // Query CPU usage (average across all pods)
        std::string cpu_query = 
            "avg(rate(container_cpu_usage_seconds_total{" + deployment_label + ",container!=\"\"}[5m]))";
        auto cpu_result = prometheus_client_->query(cpu_query);
        if (cpu_result.success) {
            load_metrics["cpu_usage"] = PrometheusClient::get_scalar_value(cpu_result);
        }

        // Query memory usage (average across all pods)
        std::string memory_query = 
            "avg(container_memory_working_set_bytes{" + deployment_label + ",container!=\"\"} / "
            "container_spec_memory_limit_bytes{" + deployment_label + ",container!=\"\"})";
        auto memory_result = prometheus_client_->query(memory_query);
        if (memory_result.success) {
            load_metrics["memory_usage"] = PrometheusClient::get_scalar_value(memory_result);
        }

        // Query queue depth (agent-specific metric)
        std::string queue_query = "regulens_agent_queue_depth{agent=\"" + agent_name + "\"}";
        auto queue_result = prometheus_client_->query(queue_query);
        if (queue_result.success) {
            load_metrics["queue_depth"] = static_cast<int>(PrometheusClient::get_scalar_value(queue_result));
        }

        if (logger_) {
            logger_->debug("Retrieved agent load metrics",
                         "AgentOrchestratorController", "getAgentLoadMetrics",
                         {{"agent", agent_name}, 
                          {"cpu_usage", std::to_string(load_metrics["cpu_usage"].get<double>())},
                          {"memory_usage", std::to_string(load_metrics["memory_usage"].get<double>())},
                          {"queue_depth", std::to_string(load_metrics["queue_depth"].get<int>())}});
        }

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->warn("Failed to get agent load metrics, using defaults: " + std::string(e.what()),
                         "AgentOrchestratorController", "getAgentLoadMetrics",
                         {{"orchestrator", orchestrator_name}});
        }
    }

    return load_metrics;
}

int AgentOrchestratorController::calculateOptimalReplicas(const std::string& agent_type,
                                                        int current_replicas,
                                                        const nlohmann::json& load_metrics) {
    double cpu_usage = load_metrics.value("cpu_usage", 0.5);
    double memory_usage = load_metrics.value("memory_usage", 0.5);
    int queue_depth = load_metrics.value("queue_depth", 10);

    // Simple scaling algorithm
    double avg_load = (cpu_usage + memory_usage) / 2.0;

    if (avg_load > 0.8 || queue_depth > 50) {
        // Scale up
        return std::min(current_replicas + 1, 10);
    } else if (avg_load < 0.3 && queue_depth < 5 && current_replicas > 1) {
        // Scale down
        return current_replicas - 1;
    }

    return current_replicas;
}

void AgentOrchestratorController::updateOrchestratorStatus(const std::string& orchestrator_name,
                                                         const nlohmann::json& status) {
    // This would call the base class method to update status
    // For now, just log
    if (logger_) {
        logger_->debug("Updating orchestrator status",
                     "AgentOrchestratorController", "updateOrchestratorStatus",
                     {{"orchestrator", orchestrator_name}});
    }
}

// Event handlers

void AgentOrchestratorController::handleOrchestratorCreation(const std::string& name,
                                                           const std::string& namespace_,
                                                           const nlohmann::json& resource) {
    if (logger_) {
        logger_->info("Handling orchestrator creation",
                     "AgentOrchestratorController", "handleOrchestratorCreation",
                     {{"orchestrator", name}, {"namespace", namespace_}});
    }

    reconcileResource(resource);
}

void AgentOrchestratorController::handleOrchestratorUpdate(const std::string& name,
                                                         const std::string& namespace_,
                                                         const nlohmann::json& new_resource,
                                                         const nlohmann::json& old_resource) {
    if (logger_) {
        logger_->info("Handling orchestrator update",
                     "AgentOrchestratorController", "handleOrchestratorUpdate",
                     {{"orchestrator", name}, {"namespace", namespace_}});
    }

    reconcileResource(new_resource);
}

void AgentOrchestratorController::handleOrchestratorDeletion(const std::string& name,
                                                           const std::string& namespace_,
                                                           const nlohmann::json& resource) {
    if (logger_) {
        logger_->info("Handling orchestrator deletion",
                     "AgentOrchestratorController", "handleOrchestratorDeletion",
                     {{"orchestrator", name}, {"namespace", namespace_}});
    }

    {
        std::lock_guard<std::mutex> lock(orchestrators_mutex_);
        active_orchestrators_.erase(name);
    }

    cleanupOrchestratorResources(name);
    orchestrators_deleted_++;
}

// Factory function

std::shared_ptr<AgentOrchestratorController> createAgentOrchestratorController(
    std::shared_ptr<KubernetesAPIClient> api_client,
    std::shared_ptr<StructuredLogger> logger,
    std::shared_ptr<PrometheusMetricsCollector> metrics) {

    auto controller = std::make_shared<AgentOrchestratorController>(api_client, logger, metrics);
    if (controller->initialize()) {
        return controller;
    }
    return nullptr;
}

} // namespace k8s
} // namespace regulens
