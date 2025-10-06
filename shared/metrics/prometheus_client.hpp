/**
 * Prometheus Client - Query API
 *
 * Production HTTP client for querying Prometheus metrics.
 * Supports instant queries and range queries via Prometheus HTTP API.
 */

#pragma once

#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include "../network/http_client.hpp"
#include "../logging/structured_logger.hpp"

namespace regulens {

/**
 * @brief Prometheus query result
 */
struct PrometheusQueryResult {
    bool success;
    std::string error_message;
    nlohmann::json data;
    std::string result_type; // "vector", "matrix", "scalar", "string"
};

/**
 * @brief Prometheus HTTP API client
 *
 * Queries Prometheus for metrics using the HTTP API.
 * Supports instant queries (/api/v1/query) and range queries (/api/v1/query_range).
 */
class PrometheusClient {
public:
    /**
     * @brief Construct Prometheus client
     * @param prometheus_url Prometheus server URL (e.g., "http://prometheus:9090")
     * @param logger Structured logger
     */
    PrometheusClient(const std::string& prometheus_url, 
                    std::shared_ptr<StructuredLogger> logger = nullptr);

    ~PrometheusClient() = default;

    // Delete copy operations
    PrometheusClient(const PrometheusClient&) = delete;
    PrometheusClient& operator=(const PrometheusClient&) = delete;

    /**
     * @brief Execute instant query
     * @param query PromQL query string
     * @param time Optional evaluation timestamp (RFC3339 or Unix timestamp)
     * @param timeout Optional query timeout
     * @return Query result
     */
    PrometheusQueryResult query(const std::string& query,
                               const std::string& time = "",
                               const std::string& timeout = "");

    /**
     * @brief Execute range query
     * @param query PromQL query string
     * @param start Start timestamp (RFC3339 or Unix timestamp)
     * @param end End timestamp (RFC3339 or Unix timestamp)
     * @param step Query resolution step (e.g., "15s", "1m")
     * @param timeout Optional query timeout
     * @return Query result
     */
    PrometheusQueryResult query_range(const std::string& query,
                                     const std::string& start,
                                     const std::string& end,
                                     const std::string& step,
                                     const std::string& timeout = "");

    /**
     * @brief Get single scalar value from query result
     * @param result Query result
     * @return Scalar value, or 0.0 if not available
     */
    static double get_scalar_value(const PrometheusQueryResult& result);

    /**
     * @brief Get single vector value from query result
     * @param result Query result
     * @param label_filter Optional label to filter results
     * @return Vector value, or 0.0 if not available
     */
    static double get_vector_value(const PrometheusQueryResult& result,
                                   const std::string& label_filter = "");

    /**
     * @brief Set query timeout
     * @param timeout_seconds Timeout in seconds
     */
    void set_timeout(long timeout_seconds);

private:
    std::string prometheus_url_;
    std::shared_ptr<StructuredLogger> logger_;
    HttpClient http_client_;
    long timeout_seconds_;

    /**
     * @brief Build query URL with parameters
     * @param endpoint API endpoint (/api/v1/query or /api/v1/query_range)
     * @param params Query parameters
     * @return Full URL
     */
    std::string build_url(const std::string& endpoint,
                         const std::unordered_map<std::string, std::string>& params);

    /**
     * @brief URL-encode parameter
     * @param value Value to encode
     * @return URL-encoded value
     */
    std::string url_encode(const std::string& value);
};

/**
 * @brief Create Prometheus client from environment configuration
 * @param logger Structured logger
 * @return Shared pointer to Prometheus client
 */
std::shared_ptr<PrometheusClient> create_prometheus_client(
    std::shared_ptr<StructuredLogger> logger = nullptr);

} // namespace regulens
