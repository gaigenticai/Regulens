/**
 * REST API Server - Enterprise Grade
 *
 * Production-grade REST API for regulatory data access with authentication,
 * rate limiting, and comprehensive endpoint coverage.
 */

#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <nlohmann/json.hpp>
#include <shared/database/postgresql_connection.hpp>
#include <shared/network/http_client.hpp>
#include <shared/logging/structured_logger.hpp>
#include "production_regulatory_monitor.hpp"

namespace regulens {

struct APIRequest {
    std::string method;
    std::string path;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    std::unordered_map<std::string, std::string> query_params;
    std::unordered_map<std::string, std::string> path_params;
};

struct APIResponse {
    int status_code;
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    APIResponse() : status_code(200) {}
    APIResponse(int code, const std::string& content_type = "application/json")
        : status_code(code) {
        headers["Content-Type"] = content_type;
        headers["Access-Control-Allow-Origin"] = "*";
        headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
        headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization";
    }
};

class RESTAPIServer {
public:
    RESTAPIServer(
        std::shared_ptr<ConnectionPool> db_pool,
        std::shared_ptr<ProductionRegulatoryMonitor> monitor,
        StructuredLogger* logger
    );

    ~RESTAPIServer();

    bool start(int port = 3000);
    void stop();
    bool is_running() const;

    // API endpoint handlers
    APIResponse handle_regulatory_changes(const APIRequest& req);
    APIResponse handle_sources(const APIRequest& req);
    APIResponse handle_monitoring_stats(const APIRequest& req);
    APIResponse handle_force_check(const APIRequest& req);
    APIResponse handle_health_check(const APIRequest& req);
    APIResponse handle_options(const APIRequest& req);

private:
    void server_loop();
    void handle_client(int client_socket);
    APIRequest parse_request(const std::string& request_str);
    std::string generate_response(const APIResponse& response);
    void route_request(const APIRequest& req, APIResponse& resp);
    std::vector<std::string> split_path(const std::string& path);
    std::string generate_change_id(const std::string& source, const std::string& title);

    // CORS and middleware
    bool validate_cors(const APIRequest& /*req*/);
    bool rate_limit_check(const std::string& client_ip);

    // Authorization
    bool is_public_endpoint(const std::string& path);
    bool authorize_request(const APIRequest& req);
    bool validate_jwt_token(const std::string& token);
    bool validate_api_key(const std::string& api_key);
    APIResponse handle_login(const APIRequest& req);
    APIResponse handle_token_refresh(const APIRequest& req);
    std::string generate_jwt_token(const std::string& username);
    bool validate_refresh_token(const std::string& token);
    std::string base64_encode(const std::string& input);
    std::string base64_decode(const std::string& input);

    // Member variables
    std::shared_ptr<ConnectionPool> db_pool_;
    std::shared_ptr<ProductionRegulatoryMonitor> monitor_;
    StructuredLogger* logger_;

    int server_port_;
    int server_socket_;
    std::thread server_thread_;
    std::atomic<bool> running_;

    // Rate limiting
    struct ClientRateLimit {
        std::deque<std::chrono::steady_clock::time_point> requests;
    };
    std::unordered_map<std::string, ClientRateLimit> rate_limit_map_;
    mutable std::mutex rate_limit_mutex_;
    static const int RATE_LIMIT_WINDOW = 60;     // seconds
    static const int RATE_LIMIT_MAX_REQUESTS = 100; // requests per window
};

} // namespace regulens
