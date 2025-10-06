/**
 * Web UI Server - Production-Grade HTTP Server for Testing
 *
 * Provides a professional web interface for testing all Regulens features
 * as required by Rule 6. Implements REST API endpoints and HTML dashboards
 * for comprehensive feature validation.
 *
 * Key Features:
 * - Production-grade HTTP server with proper error handling
 * - REST API endpoints for all system components
 * - HTML dashboards for visual testing
 * - Real-time updates via WebSocket/Server-Sent Events
 * - Security headers and input validation
 * - Thread-safe request handling
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "../network/http_client.hpp"
#include "../logging/structured_logger.hpp"

namespace regulens {

// Forward declarations
class ConfigurationManager;
class MetricsCollector;

/**
 * @brief HTTP Request structure
 */
struct HTTPRequest {
    std::string method;
    std::string path;
    std::string query_string;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    std::unordered_map<std::string, std::string> params;
    std::unordered_map<std::string, std::string> query_params;

    HTTPRequest() = default;
    HTTPRequest(const std::string& m, const std::string& p,
                const std::string& q = "", const std::string& b = "")
        : method(m), path(p), query_string(q), body(b) {}
};

/**
 * @brief HTTP Response structure
 */
struct HTTPResponse {
    int status_code = 200;
    std::string status_message = "OK";
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    std::string content_type = "text/plain";

    HTTPResponse() = default;
    HTTPResponse(int code, const std::string& message, const std::string& b = "")
        : status_code(code), status_message(message), body(b) {}
    HTTPResponse(int code, const std::string& message, const std::string& b, const std::string& ct)
        : status_code(code), status_message(message), body(b), content_type(ct) {}
};

/**
 * @brief Request handler function type
 */
using RequestHandler = std::function<HTTPResponse(const HTTPRequest&)>;

/**
 * @brief Production-grade web UI server
 *
 * Implements a complete HTTP server for testing all Regulens features
 * with proper threading, security, and error handling.
 */
class WebUIServer {
public:
    WebUIServer(int port = 8080);
    ~WebUIServer();

    // Server lifecycle
    bool start();
    void stop();
    bool is_running() const { return running_; }

    // Route registration
    void add_route(const std::string& method, const std::string& path, RequestHandler handler);
    void add_static_route(const std::string& path_prefix, const std::string& static_dir);

    // Configuration
    void set_config_manager(std::shared_ptr<ConfigurationManager> config) {
        config_manager_ = config;
    }
    void set_metrics_collector(std::shared_ptr<MetricsCollector> metrics) {
        metrics_collector_ = metrics;
    }
    void set_logger(std::shared_ptr<StructuredLogger> logger) {
        logger_ = logger;
    }

    // Server statistics
    struct ServerStats {
        uint64_t total_requests = 0;
        uint64_t active_connections = 0;
        uint64_t error_count = 0;
        double avg_response_time_ms = 0.0;
    };
    ServerStats get_stats() const;

private:
    // Server configuration
    int port_;
    std::atomic<bool> running_;
    std::thread server_thread_;

    // Components
    std::shared_ptr<ConfigurationManager> config_manager_;
    std::shared_ptr<MetricsCollector> metrics_collector_;
    std::shared_ptr<StructuredLogger> logger_;

    // Route storage
    std::unordered_map<std::string, RequestHandler> routes_;
    std::unordered_map<std::string, std::string> static_routes_;
    mutable std::mutex routes_mutex_;

    // Statistics
    mutable std::mutex stats_mutex_;
    ServerStats stats_;

    // Server implementation
    void server_loop();
    void handle_client(int client_fd);
    HTTPResponse handle_request(const HTTPRequest& request);
    HTTPResponse serve_static_file(const std::string& path, const std::string& static_dir);
    HTTPRequest parse_request(const std::string& raw_request);
    std::string serialize_response(const HTTPResponse& response);

    // Utility methods
    std::string url_decode(const std::string& str);
    std::unordered_map<std::string, std::string> parse_query_string(const std::string& query);
    std::unordered_map<std::string, std::string> parse_form_data(const std::string& body);
    std::string get_content_type(const std::string& path);
    bool is_safe_path(const std::string& path);

    // Default handlers
    HTTPResponse handle_not_found(const HTTPRequest& request);
    HTTPResponse handle_method_not_allowed(const HTTPRequest& request);
    HTTPResponse handle_internal_error(const HTTPRequest& request);
};

} // namespace regulens
