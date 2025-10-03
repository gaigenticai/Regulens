/**
 * Prometheus Metrics Collection and Exposition
 *
 * Enterprise-grade metrics collection system with Prometheus integration
 * for comprehensive monitoring of system performance, business KPIs, and
 * operational health across all Regulens components.
 *
 * Features:
 * - Circuit breaker metrics (states, failure rates, recovery times)
 * - LLM performance metrics (response times, token usage, error rates)
 * - Compliance metrics (decision accuracy, processing times)
 * - System metrics (database, cache, API performance)
 * - Business metrics (regulatory coverage, agent performance)
 * - Prometheus exposition via HTTP endpoint
 * - Alert-ready metric definitions
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <chrono>
#include <nlohmann/json.hpp>

#include "../config/configuration_manager.hpp"
#include "../logging/structured_logger.hpp"
#include "../error_handler.hpp"
#include "../resilience/circuit_breaker.hpp"

namespace regulens {

/**
 * @brief Metric types supported by the system
 */
enum class MetricType {
    COUNTER,      // Monotonically increasing counter
    GAUGE,        // Value that can go up or down
    HISTOGRAM,    // Distribution of values with buckets
    SUMMARY       // Similar to histogram but with quantiles
};

/**
 * @brief Metric labels for dimensional metrics
 */
struct MetricLabels {
    std::unordered_map<std::string, std::string> labels;

    MetricLabels() = default;

    MetricLabels(std::unordered_map<std::string, std::string> lbls)
        : labels(std::move(lbls)) {}

    std::string to_string() const;
};

/**
 * @brief Individual metric definition
 */
struct MetricDefinition {
    std::string name;
    std::string help;
    MetricType type;
    MetricLabels labels;
    std::string value;

    MetricDefinition(std::string n, std::string h, MetricType t,
                    MetricLabels lbls = {}, std::string val = "")
        : name(std::move(n)), help(std::move(h)), type(t),
          labels(std::move(lbls)), value(std::move(val)) {}

    std::string to_prometheus_format() const;
};

/**
 * @brief Circuit breaker metrics collector
 */
class CircuitBreakerMetricsCollector {
public:
    CircuitBreakerMetricsCollector(std::shared_ptr<StructuredLogger> logger);

    /**
     * @brief Collect metrics from all registered circuit breakers
     * @return Vector of metric definitions
     */
    std::vector<MetricDefinition> collect_metrics();

    /**
     * @brief Register a circuit breaker for metrics collection
     * @param breaker Circuit breaker to monitor
     */
    void register_circuit_breaker(std::shared_ptr<CircuitBreaker> breaker);

    /**
     * @brief Unregister a circuit breaker
     * @param breaker_name Name of circuit breaker to remove
     */
    void unregister_circuit_breaker(const std::string& breaker_name);

private:
    std::shared_ptr<StructuredLogger> logger_;
    std::unordered_map<std::string, std::shared_ptr<CircuitBreaker>> breakers_;
    mutable std::mutex breakers_mutex_;

    /**
     * @brief Convert circuit breaker state to string
     * @param state Circuit state
     * @return String representation
     */
    std::string circuit_state_to_string(CircuitState state) const;
};

/**
 * @brief LLM performance metrics collector
 */
class LLMMetricsCollector {
public:
    LLMMetricsCollector(std::shared_ptr<StructuredLogger> logger);

    /**
     * @brief Record API call metrics
     * @param provider LLM provider (openai, anthropic)
     * @param model Model name
     * @param success Whether call was successful
     * @param response_time_ms Response time in milliseconds
     * @param input_tokens Number of input tokens
     * @param output_tokens Number of output tokens
     * @param cost_usd Estimated cost in USD
     */
    void record_api_call(const std::string& provider, const std::string& model,
                        bool success, long response_time_ms,
                        int input_tokens = 0, int output_tokens = 0,
                        double cost_usd = 0.0);

    /**
     * @brief Record rate limit hit
     * @param provider LLM provider
     */
    void record_rate_limit_hit(const std::string& provider);

    /**
     * @brief Record circuit breaker event
     * @param provider LLM provider
     * @param event_type Event type (opened, closed, half_open)
     */
    void record_circuit_breaker_event(const std::string& provider, const std::string& event_type);

    /**
     * @brief Collect LLM metrics
     * @return Vector of metric definitions
     */
    std::vector<MetricDefinition> collect_metrics();

private:
    std::shared_ptr<StructuredLogger> logger_;

    // Counters for API calls
    std::atomic<size_t> openai_calls_{0};
    std::atomic<size_t> openai_successful_calls_{0};
    std::atomic<size_t> anthropic_calls_{0};
    std::atomic<size_t> anthropic_successful_calls_{0};

    // Rate limiting metrics
    std::atomic<size_t> openai_rate_limits_{0};
    std::atomic<size_t> anthropic_rate_limits_{0};

    // Circuit breaker events
    std::atomic<size_t> openai_breaker_opened_{0};
    std::atomic<size_t> openai_breaker_closed_{0};
    std::atomic<size_t> anthropic_breaker_opened_{0};
    std::atomic<size_t> anthropic_breaker_closed_{0};

    // Performance histograms (simplified as gauges for now)
    std::atomic<long> openai_avg_response_time_{0};
    std::atomic<long> anthropic_avg_response_time_{0};
    std::atomic<size_t> openai_total_tokens_{0};
    std::atomic<size_t> anthropic_total_tokens_{0};
    std::atomic<double> openai_total_cost_{0.0};
    std::atomic<double> anthropic_total_cost_{0.0};
};

/**
 * @brief Compliance metrics collector
 */
class ComplianceMetricsCollector {
public:
    ComplianceMetricsCollector(std::shared_ptr<StructuredLogger> logger);

    /**
     * @brief Record compliance decision
     * @param agent_type Type of agent making decision
     * @param decision_type Type of decision (approve, deny, escalate)
     * @param processing_time_ms Time to make decision
     * @param confidence_score Decision confidence (0.0-1.0)
     * @param was_correct Whether decision was later verified as correct
     */
    void record_decision(const std::string& agent_type, const std::string& decision_type,
                        long processing_time_ms, double confidence_score, bool was_correct = true);

    /**
     * @brief Record regulatory data ingestion
     * @param source Regulatory source (SEC, FCA, ECB)
     * @param documents_processed Number of documents processed
     * @param new_regulations_found Number of new regulations discovered
     * @param processing_time_ms Time spent processing
     */
    void record_regulatory_ingestion(const std::string& source, size_t documents_processed,
                                   size_t new_regulations_found, long processing_time_ms);

    /**
     * @brief Record risk assessment
     * @param entity_type Type of entity assessed
     * @param risk_score Calculated risk score (0.0-1.0)
     * @param risk_level Risk level (LOW, MEDIUM, HIGH, CRITICAL)
     * @param processing_time_ms Assessment time
     */
    void record_risk_assessment(const std::string& entity_type, double risk_score,
                               const std::string& risk_level, long processing_time_ms);

    /**
     * @brief Collect compliance metrics
     * @return Vector of metric definitions
     */
    std::vector<MetricDefinition> collect_metrics();

private:
    std::shared_ptr<StructuredLogger> logger_;

    // Decision metrics
    std::atomic<size_t> total_decisions_{0};
    std::atomic<size_t> correct_decisions_{0};
    std::atomic<size_t> approve_decisions_{0};
    std::atomic<size_t> deny_decisions_{0};
    std::atomic<size_t> escalate_decisions_{0};

    // Regulatory metrics
    std::atomic<size_t> sec_documents_processed_{0};
    std::atomic<size_t> fca_documents_processed_{0};
    std::atomic<size_t> ecb_documents_processed_{0};
    std::atomic<size_t> new_regulations_found_{0};

    // Risk assessment metrics
    std::atomic<size_t> risk_assessments_low_{0};
    std::atomic<size_t> risk_assessments_medium_{0};
    std::atomic<size_t> risk_assessments_high_{0};
    std::atomic<size_t> risk_assessments_critical_{0};

    // Performance metrics
    std::atomic<long> avg_decision_time_{0};
    std::atomic<long> avg_regulatory_processing_time_{0};
    std::atomic<long> avg_risk_assessment_time_{0};
};

/**
 * @brief System performance metrics collector
 */
class SystemMetricsCollector {
public:
    SystemMetricsCollector(std::shared_ptr<StructuredLogger> logger);

    /**
     * @brief Record database operation
     * @param operation_type Type of operation (SELECT, INSERT, UPDATE, DELETE)
     * @param table_name Table affected
     * @param success Whether operation succeeded
     * @param response_time_ms Operation time
     * @param rows_affected Number of rows affected
     */
    void record_database_operation(const std::string& operation_type, const std::string& table_name,
                                 bool success, long response_time_ms, size_t rows_affected = 0);

    /**
     * @brief Record cache operation
     * @param cache_type Type of cache (redis, memory)
     * @param operation_type Operation (GET, SET, DELETE)
     * @param hit Whether cache hit (for GET operations)
     * @param response_time_ms Operation time
     */
    void record_cache_operation(const std::string& cache_type, const std::string& operation_type,
                              bool hit, long response_time_ms);

    /**
     * @brief Record HTTP API call
     * @param endpoint API endpoint called
     * @param method HTTP method
     * @param status_code HTTP status code
     * @param response_time_ms Response time
     */
    void record_http_call(const std::string& endpoint, const std::string& method,
                         int status_code, long response_time_ms);

    /**
     * @brief Update system resource metrics
     * @param cpu_usage CPU usage percentage
     * @param memory_usage Memory usage percentage
     * @param active_connections Number of active connections
     */
    void update_system_resources(double cpu_usage, double memory_usage, size_t active_connections);

    /**
     * @brief Collect system metrics
     * @return Vector of metric definitions
     */
    std::vector<MetricDefinition> collect_metrics();

private:
    std::shared_ptr<StructuredLogger> logger_;

    // Database metrics
    std::atomic<size_t> db_queries_total_{0};
    std::atomic<size_t> db_queries_successful_{0};
    std::atomic<long> db_avg_response_time_{0};

    // Cache metrics
    std::atomic<size_t> cache_requests_total_{0};
    std::atomic<size_t> cache_hits_{0};
    std::atomic<long> cache_avg_response_time_{0};

    // HTTP metrics
    std::atomic<size_t> http_requests_total_{0};
    std::atomic<size_t> http_requests_2xx_{0};
    std::atomic<size_t> http_requests_4xx_{0};
    std::atomic<size_t> http_requests_5xx_{0};
    std::atomic<long> http_avg_response_time_{0};

    // System resources
    std::atomic<double> current_cpu_usage_{0.0};
    std::atomic<double> current_memory_usage_{0.0};
    std::atomic<size_t> current_active_connections_{0};
};

/**
 * @brief Main Prometheus metrics collector
 */
class PrometheusMetricsCollector {
public:
    PrometheusMetricsCollector(std::shared_ptr<ConfigurationManager> config,
                             std::shared_ptr<StructuredLogger> logger,
                             std::shared_ptr<ErrorHandler> error_handler);

    ~PrometheusMetricsCollector();

    /**
     * @brief Initialize the metrics collector
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Shutdown the metrics collector
     */
    void shutdown();

    /**
     * @brief Get circuit breaker metrics collector
     * @return Reference to circuit breaker collector
     */
    CircuitBreakerMetricsCollector& get_circuit_breaker_collector() { return *circuit_breaker_collector_; }

    /**
     * @brief Get LLM metrics collector
     * @return Reference to LLM collector
     */
    LLMMetricsCollector& get_llm_collector() { return *llm_collector_; }

    /**
     * @brief Get compliance metrics collector
     * @return Reference to compliance collector
     */
    ComplianceMetricsCollector& get_compliance_collector() { return *compliance_collector_; }

    /**
     * @brief Get system metrics collector
     * @return Reference to system collector
     */
    SystemMetricsCollector& get_system_collector() { return *system_collector_; }

    /**
     * @brief Collect all metrics in Prometheus format
     * @return Complete metrics output as string
     */
    std::string collect_all_metrics();

    /**
     * @brief Get metrics HTTP endpoint response
     * @return HTTP response with metrics
     */
    std::string get_metrics_endpoint_response();

private:
    std::shared_ptr<ConfigurationManager> config_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<ErrorHandler> error_handler_;

    std::unique_ptr<CircuitBreakerMetricsCollector> circuit_breaker_collector_;
    std::unique_ptr<LLMMetricsCollector> llm_collector_;
    std::unique_ptr<ComplianceMetricsCollector> compliance_collector_;
    std::unique_ptr<SystemMetricsCollector> system_collector_;

    std::atomic<bool> initialized_{false};

    /**
     * @brief Generate Prometheus header with metadata
     * @return Header string
     */
    std::string generate_prometheus_header() const;
};

/**
 * @brief Create Prometheus metrics collector
 * @param config Configuration manager
 * @param logger Structured logger
 * @param error_handler Error handler
 * @return Shared pointer to metrics collector
 */
std::shared_ptr<PrometheusMetricsCollector> create_prometheus_metrics_collector(
    std::shared_ptr<ConfigurationManager> config,
    std::shared_ptr<StructuredLogger> logger,
    std::shared_ptr<ErrorHandler> error_handler);

} // namespace regulens
