/**
 * Prometheus Client Implementation
 */

#include "prometheus_client.hpp"
#include <sstream>
#include <iomanip>
#include <cstdlib>

namespace regulens {

PrometheusClient::PrometheusClient(const std::string& prometheus_url,
                                 std::shared_ptr<StructuredLogger> logger)
    : prometheus_url_(prometheus_url)
    , logger_(logger)
    , timeout_seconds_(30) {
    
    http_client_.set_timeout(timeout_seconds_);
    http_client_.set_user_agent("Regulens-Prometheus-Client/1.0");
}

PrometheusQueryResult PrometheusClient::query(const std::string& query,
                                             const std::string& time,
                                             const std::string& timeout) {
    PrometheusQueryResult result;
    result.success = false;

    try {
        // Build query parameters
        std::unordered_map<std::string, std::string> params;
        params["query"] = query;
        if (!time.empty()) {
            params["time"] = time;
        }
        if (!timeout.empty()) {
            params["timeout"] = timeout;
        }

        // Build URL
        std::string url = build_url("/api/v1/query", params);

        if (logger_) {
            logger_->debug("Executing Prometheus query: " + query,
                          "PrometheusClient", "query",
                          {{"url", url}});
        }

        // Execute HTTP GET
        auto response = http_client_.get(url);

        if (!response.success || response.status_code != 200) {
            result.error_message = "HTTP request failed: " + response.error_message;
            if (logger_) {
                logger_->warn("Prometheus query failed: " + result.error_message,
                            "PrometheusClient", "query",
                            {{"status_code", std::to_string(response.status_code)}});
            }
            return result;
        }

        // Parse JSON response
        auto json_response = nlohmann::json::parse(response.body);

        if (json_response.value("status", "") != "success") {
            result.error_message = json_response.value("error", "Unknown error");
            if (logger_) {
                logger_->warn("Prometheus returned error: " + result.error_message,
                            "PrometheusClient", "query");
            }
            return result;
        }

        // Extract data
        if (json_response.contains("data")) {
            result.data = json_response["data"];
            result.result_type = json_response["data"].value("resultType", "");
            result.success = true;
        }

    } catch (const std::exception& e) {
        result.error_message = std::string("Exception: ") + e.what();
        if (logger_) {
            logger_->error("Prometheus query exception: " + result.error_message,
                          "PrometheusClient", "query",
                          {{"query", query}});
        }
    }

    return result;
}

PrometheusQueryResult PrometheusClient::query_range(const std::string& query,
                                                   const std::string& start,
                                                   const std::string& end,
                                                   const std::string& step,
                                                   const std::string& timeout) {
    PrometheusQueryResult result;
    result.success = false;

    try {
        // Build query parameters
        std::unordered_map<std::string, std::string> params;
        params["query"] = query;
        params["start"] = start;
        params["end"] = end;
        params["step"] = step;
        if (!timeout.empty()) {
            params["timeout"] = timeout;
        }

        // Build URL
        std::string url = build_url("/api/v1/query_range", params);

        if (logger_) {
            logger_->debug("Executing Prometheus range query",
                          "PrometheusClient", "query_range",
                          {{"query", query}, {"start", start}, {"end", end}, {"step", step}});
        }

        // Execute HTTP GET
        auto response = http_client_.get(url);

        if (!response.success || response.status_code != 200) {
            result.error_message = "HTTP request failed: " + response.error_message;
            if (logger_) {
                logger_->warn("Prometheus range query failed: " + result.error_message,
                            "PrometheusClient", "query_range",
                            {{"status_code", std::to_string(response.status_code)}});
            }
            return result;
        }

        // Parse JSON response
        auto json_response = nlohmann::json::parse(response.body);

        if (json_response.value("status", "") != "success") {
            result.error_message = json_response.value("error", "Unknown error");
            if (logger_) {
                logger_->warn("Prometheus returned error: " + result.error_message,
                            "PrometheusClient", "query_range");
            }
            return result;
        }

        // Extract data
        if (json_response.contains("data")) {
            result.data = json_response["data"];
            result.result_type = json_response["data"].value("resultType", "");
            result.success = true;
        }

    } catch (const std::exception& e) {
        result.error_message = std::string("Exception: ") + e.what();
        if (logger_) {
            logger_->error("Prometheus range query exception: " + result.error_message,
                          "PrometheusClient", "query_range",
                          {{"query", query}});
        }
    }

    return result;
}

double PrometheusClient::get_scalar_value(const PrometheusQueryResult& result) {
    if (!result.success || !result.data.contains("result")) {
        return 0.0;
    }

    try {
        if (result.result_type == "scalar") {
            // Scalar format: [timestamp, "value"]
            auto scalar_result = result.data["result"];
            if (scalar_result.is_array() && scalar_result.size() >= 2) {
                return std::stod(scalar_result[1].get<std::string>());
            }
        } else if (result.result_type == "vector") {
            // Vector format: [{"metric": {...}, "value": [timestamp, "value"]}]
            auto vector_results = result.data["result"];
            if (vector_results.is_array() && !vector_results.empty()) {
                auto first_result = vector_results[0];
                if (first_result.contains("value") && first_result["value"].is_array() && 
                    first_result["value"].size() >= 2) {
                    return std::stod(first_result["value"][1].get<std::string>());
                }
            }
        }
    } catch (const std::exception&) {
        return 0.0;
    }

    return 0.0;
}

double PrometheusClient::get_vector_value(const PrometheusQueryResult& result,
                                         const std::string& label_filter) {
    if (!result.success || !result.data.contains("result") || result.result_type != "vector") {
        return 0.0;
    }

    try {
        auto vector_results = result.data["result"];
        if (!vector_results.is_array()) {
            return 0.0;
        }

        for (const auto& item : vector_results) {
            // Check label filter if provided
            if (!label_filter.empty() && item.contains("metric")) {
                bool match = false;
                for (const auto& [key, value] : item["metric"].items()) {
                    if (value.is_string() && value.get<std::string>().find(label_filter) != std::string::npos) {
                        match = true;
                        break;
                    }
                }
                if (!match) {
                    continue;
                }
            }

            // Extract value
            if (item.contains("value") && item["value"].is_array() && item["value"].size() >= 2) {
                return std::stod(item["value"][1].get<std::string>());
            }
        }
    } catch (const std::exception&) {
        return 0.0;
    }

    return 0.0;
}

void PrometheusClient::set_timeout(long timeout_seconds) {
    timeout_seconds_ = timeout_seconds;
    http_client_.set_timeout(timeout_seconds);
}

std::string PrometheusClient::build_url(const std::string& endpoint,
                                       const std::unordered_map<std::string, std::string>& params) {
    std::ostringstream url;
    url << prometheus_url_ << endpoint;

    bool first = true;
    for (const auto& [key, value] : params) {
        url << (first ? "?" : "&");
        url << url_encode(key) << "=" << url_encode(value);
        first = false;
    }

    return url.str();
}

std::string PrometheusClient::url_encode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        // Keep alphanumeric and other safe characters intact
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }

        // Any other characters are percent-encoded
        escaped << std::uppercase;
        escaped << '%' << std::setw(2) << int(static_cast<unsigned char>(c));
        escaped << std::nouppercase;
    }

    return escaped.str();
}

std::shared_ptr<PrometheusClient> create_prometheus_client(
    std::shared_ptr<StructuredLogger> logger) {
    
    // Get Prometheus URL from environment or use default
    const char* prometheus_url_env = std::getenv("PROMETHEUS_URL");
    std::string prometheus_url = prometheus_url_env ? 
        prometheus_url_env : "http://prometheus:9090";

    return std::make_shared<PrometheusClient>(prometheus_url, logger);
}

} // namespace regulens
