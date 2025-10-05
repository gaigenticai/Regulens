/**
 * Regulatory Data Controller - Kubernetes Operator
 *
 * Enterprise-grade Kubernetes controller for managing RegulatoryDataSource
 * custom resources with advanced data ingestion, scaling, and monitoring
 * for regulatory compliance data pipelines.
 */

#pragma once

#include "operator_framework.hpp"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <mutex>

namespace regulens {

// Forward declarations
class PrometheusClient;

namespace k8s {

/**
 * @brief Regulatory Data Controller
 *
 * Manages the lifecycle of RegulatoryDataSource custom resources,
 * including data ingestion pipelines, intelligent scaling based on data volume,
 * health monitoring, and regulatory data processing orchestration.
 */
class RegulatoryDataController : public CustomResourceController {
public:
    RegulatoryDataController(std::shared_ptr<KubernetesAPIClient> api_client,
                           std::shared_ptr<StructuredLogger> logger,
                           std::shared_ptr<PrometheusMetricsCollector> metrics);

    ~RegulatoryDataController() override = default;

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
     * @return "RegulatoryDataSource"
     */
    std::string getResourceType() const override { return "RegulatoryDataSource"; }

private:
    // Data source state tracking
    std::unordered_map<std::string, nlohmann::json> active_data_sources_;
    std::unordered_map<std::string, std::unordered_set<std::string>> source_endpoints_;
    mutable std::mutex sources_mutex_;

    // Prometheus client for metrics queries
    std::shared_ptr<PrometheusClient> prometheus_client_;

    // Metrics
    std::atomic<size_t> sources_created_{0};
    std::atomic<size_t> sources_updated_{0};
    std::atomic<size_t> sources_deleted_{0};
    std::atomic<size_t> ingestion_jobs_started_{0};
    std::atomic<size_t> data_ingestion_errors_{0};
    std::atomic<size_t> documents_processed_total_{0};
    std::atomic<size_t> data_volume_processed_bytes_{0};
    std::atomic<size_t> scaling_events_{0};
    std::atomic<size_t> health_checks_performed_{0};

    /**
     * @brief Reconcile RegulatoryDataSource resource
     * @param resource Current resource state
     * @return Desired resource state
     */
    nlohmann::json reconcileResource(const nlohmann::json& resource) override;

    /**
     * @brief Validate RegulatoryDataSource specification
     * @param spec Resource spec
     * @return Validation errors (empty if valid)
     */
    std::vector<std::string> validateResourceSpec(const nlohmann::json& spec) override;

    /**
     * @brief Create data ingestion deployment
     * @param source_name Data source name
     * @param spec Data source spec
     * @return true if deployment created successfully
     */
    bool createDataIngestionDeployment(const std::string& source_name, const nlohmann::json& spec);

    /**
     * @brief Update existing data ingestion deployment
     * @param source_name Data source name
     * @param spec Updated data source spec
     * @return true if deployment updated successfully
     */
    bool updateDataIngestionDeployment(const std::string& source_name, const nlohmann::json& spec);

    /**
     * @brief Perform intelligent scaling based on data volume
     * @param source_name Data source name
     * @param spec Data source spec
     * @return true if scaling successful
     */
    bool scaleDataIngestion(const std::string& source_name, const nlohmann::json& spec);

    /**
     * @brief Create data ingestion services and networking
     * @param source_name Data source name
     * @param spec Data source spec
     * @return true if services created successfully
     */
    bool createDataIngestionServices(const std::string& source_name, const nlohmann::json& spec);

    /**
     * @brief Configure data source endpoints and connections
     * @param source_name Data source name
     * @param spec Data source spec
     * @return true if endpoints configured successfully
     */
    bool configureDataSourceEndpoints(const std::string& source_name, const nlohmann::json& spec);

    /**
     * @brief Set up data transformation pipelines
     * @param source_name Data source name
     * @param spec Data source spec
     * @return true if transformation configured successfully
     */
    bool setupDataTransformation(const std::string& source_name, const nlohmann::json& spec);

    /**
     * @brief Create data ingestion configuration and secrets
     * @param source_name Data source name
     * @param spec Data source spec
     * @return true if configuration created successfully
     */
    bool createDataIngestionConfiguration(const std::string& source_name, const nlohmann::json& spec);

    /**
     * @brief Monitor data ingestion health and performance
     * @param source_name Data source name
     * @param spec Data source spec
     * @return Health status with metrics
     */
    nlohmann::json monitorDataIngestionHealth(const std::string& source_name, const nlohmann::json& spec);

    /**
     * @brief Clean up data source resources on deletion
     * @param source_name Data source name
     * @return true if cleanup successful
     */
    bool cleanupDataSourceResources(const std::string& source_name);

    /**
     * @brief Calculate optimal replicas based on data volume and processing load
     * @param source_type Type of data source
     * @param current_replicas Current replica count
     * @param data_metrics Current data processing metrics
     * @param source_config Data source configuration
     * @return Recommended replica count
     */
    int calculateOptimalReplicas(const std::string& source_type, int current_replicas,
                               const nlohmann::json& data_metrics, const nlohmann::json& source_config);

    /**
     * @brief Get data processing metrics for scaling decisions
     * @param source_name Data source name
     * @param source_type Type of data source
     * @return Data processing metrics JSON
     */
    nlohmann::json getDataProcessingMetrics(const std::string& source_name, const std::string& source_type);

    /**
     * @brief Get pod resource metrics from Kubernetes Metrics API
     * @param source_name Data source name
     * @return Pod metrics JSON
     */
    nlohmann::json getPodMetrics(const std::string& source_name);

    /**
     * @brief Get data source specific metrics from monitoring systems
     * @param source_name Data source name
     * @param source_type Type of data source
     * @return Data source metrics JSON
     */
    nlohmann::json getDataSourceMetrics(const std::string& source_name, const std::string& source_type);

    /**
     * @brief Parse CPU usage from Kubernetes metrics format
     * @param cpu_str CPU usage string (e.g., "100m", "0.1")
     * @return CPU usage as cores
     */
    double parseCpuUsage(const std::string& cpu_str);

    /**
     * @brief Parse memory usage from Kubernetes metrics format
     * @param memory_str Memory usage string (e.g., "128Mi", "1Gi")
     * @return Memory usage in Gi
     */
    double parseMemoryUsage(const std::string& memory_str);

    /**
     * @brief Generate deployment spec for data ingestion
     * @param source_name Data source name
     * @param spec Data source spec
     * @return Deployment JSON specification
     */
    nlohmann::json generateDataIngestionDeploymentSpec(const std::string& source_name, const nlohmann::json& spec);

    /**
     * @brief Generate ConfigMap for data source configuration
     * @param source_name Data source name
     * @param spec Data source spec
     * @return ConfigMap JSON specification
     */
    nlohmann::json generateDataSourceConfigMapSpec(const std::string& source_name, const nlohmann::json& spec);

    /**
     * @brief Generate Secret for data source credentials
     * @param source_name Data source name
     * @param spec Data source spec
     * @return Secret JSON specification
     */
    nlohmann::json generateDataSourceSecretSpec(const std::string& source_name, const nlohmann::json& spec);

    /**
     * @brief Configure RBAC for data ingestion
     * @param source_name Data source name
     * @param spec Data source spec
     * @return true if RBAC configured successfully
     */
    bool configureDataIngestionRBAC(const std::string& source_name, const nlohmann::json& spec);

    /**
     * @brief Set up monitoring and metrics collection for data ingestion
     * @param source_name Data source name
     * @param spec Data source spec
     * @return true if monitoring configured successfully
     */
    bool setupDataIngestionMonitoring(const std::string& source_name, const nlohmann::json& spec);

    /**
     * @brief Validate data source endpoints configuration
     * @param endpoints Endpoints array
     * @return Validation errors
     */
    std::vector<std::string> validateDataSourceEndpoints(const nlohmann::json& endpoints);

    /**
     * @brief Validate scraping configuration
     * @param scraping_config Web scraping configuration
     * @return Validation errors
     */
    std::vector<std::string> validateScrapingConfig(const nlohmann::json& scraping_config);

    /**
     * @brief Validate database configuration
     * @param db_config Database configuration
     * @return Validation errors
     */
    std::vector<std::string> validateDatabaseConfig(const nlohmann::json& db_config);

    /**
     * @brief Get data source type specific configuration
     * @param source_type Type of data source
     * @return Type-specific configuration
     */
    nlohmann::json getDataSourceTypeConfig(const std::string& source_type);

    /**
     * @brief Update data source status with current state
     * @param source_name Data source name
     * @param status Status information
     */
    void updateDataSourceStatus(const std::string& source_name, const nlohmann::json& status);
};

/**
 * @brief Create Regulatory Data Controller
 * @param api_client Kubernetes API client
 * @param logger Structured logger
 * @param metrics Prometheus metrics collector
 * @return Shared pointer to controller
 */
std::shared_ptr<RegulatoryDataController> createRegulatoryDataController(
    std::shared_ptr<KubernetesAPIClient> api_client,
    std::shared_ptr<StructuredLogger> logger,
    std::shared_ptr<PrometheusMetricsCollector> metrics);

} // namespace k8s
} // namespace regulens
