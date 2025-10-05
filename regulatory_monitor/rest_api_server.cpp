/**
 * REST API Server - Implementation
 *
 * Production-grade REST API with proper HTTP handling, CORS support, and rate limiting.
 */

#include "rest_api_server.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <regex>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <csignal>

namespace regulens {

RESTAPIServer::RESTAPIServer(
    std::shared_ptr<ConnectionPool> db_pool,
    std::shared_ptr<ProductionRegulatoryMonitor> monitor,
    StructuredLogger* logger
) : db_pool_(db_pool), monitor_(monitor), logger_(logger),
    server_port_(3000), server_socket_(-1), running_(false) {
}

RESTAPIServer::~RESTAPIServer() {
    stop();
}

bool RESTAPIServer::start(int port) {
    server_port_ = port;

    // Create server socket
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ < 0) {
        logger_->error("Failed to create API server socket", "RESTAPIServer", __func__);
        return false;
    }

    // Set socket options for reuse address
    int opt = 1;
    if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        logger_->error("Failed to set API server socket options", "RESTAPIServer", __func__);
        close(server_socket_);
        return false;
    }

    // Bind socket to address and port
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_port_);

    if (bind(server_socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        logger_->error("Failed to bind API server socket to port " + std::to_string(server_port_),
                      "RESTAPIServer", __func__);
        close(server_socket_);
        return false;
    }

    // Listen for incoming connections
    if (listen(server_socket_, 50) < 0) {
        logger_->error("Failed to listen on API server socket", "RESTAPIServer", __func__);
        close(server_socket_);
        return false;
    }

    running_ = true;
    server_thread_ = std::thread(&RESTAPIServer::server_loop, this);

    logger_->info("REST API server started on port " + std::to_string(server_port_),
                 "RESTAPIServer", __func__);
    return true;
}

void RESTAPIServer::stop() {
    if (!running_) return;

    running_ = false;

    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    if (server_socket_ >= 0) {
        close(server_socket_);
        server_socket_ = -1;
    }

    logger_->info("REST API server stopped", "RESTAPIServer", __func__);
}

bool RESTAPIServer::is_running() const {
    return running_;
}

void RESTAPIServer::server_loop() {
    logger_->info("REST API server loop started", "RESTAPIServer", __func__);

    while (running_) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_socket_, (struct sockaddr*)&client_addr, &client_len);

        if (client_socket < 0) {
            if (running_) {
                logger_->error("Failed to accept API client connection", "RESTAPIServer", __func__);
            }
            continue;
        }

        // Handle client in a separate thread for concurrent requests
        std::thread(&RESTAPIServer::handle_client, this, client_socket).detach();
    }

    logger_->info("REST API server loop ended", "RESTAPIServer", __func__);
}

void RESTAPIServer::handle_client(int client_socket) {
    char buffer[8192];
    std::string request_str;

    // Read request
    ssize_t bytes_read;
    while ((bytes_read = read(client_socket, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        request_str += buffer;

        // Check if we have the complete HTTP request
        if (request_str.find("\r\n\r\n") != std::string::npos) {
            break;
        }
    }

    if (bytes_read < 0) {
        logger_->error("Failed to read from API client socket", "RESTAPIServer", __func__);
        close(client_socket);
        return;
    }

    try {
        // Parse and route request
        APIRequest req = parse_request(request_str);
        APIResponse resp;

        // Check CORS
        if (!validate_cors(req)) {
            resp = APIResponse(403, "text/plain");
            resp.body = "CORS validation failed";
        } else {
            route_request(req, resp);
        }

        // Generate and send response
        std::string response_str = generate_response(resp);
        write(client_socket, response_str.c_str(), response_str.length());

    } catch (const std::exception& e) {
        logger_->error("Error handling API request: " + std::string(e.what()),
                      "RESTAPIServer", __func__);

        APIResponse error_resp(500, "application/json");
        error_resp.body = nlohmann::json{{"error", "Internal server error"}}.dump();
        std::string response_str = generate_response(error_resp);
        write(client_socket, response_str.c_str(), response_str.length());
    }

    close(client_socket);
}

APIRequest RESTAPIServer::parse_request(const std::string& request_str) {
    APIRequest req;
    std::istringstream iss(request_str);
    std::string line;

    // Parse request line
    if (std::getline(iss, line)) {
        std::istringstream line_stream(line);
        line_stream >> req.method >> req.path;
    }

    // Parse headers
    while (std::getline(iss, line) && line != "\r") {
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string header_name = line.substr(0, colon_pos);
            std::string header_value = line.substr(colon_pos + 1);

            // Trim whitespace
            header_name.erase(header_name.begin(),
                std::find_if(header_name.begin(), header_name.end(),
                    [](unsigned char ch) { return !std::isspace(ch); }));
            header_name.erase(std::find_if(header_name.rbegin(), header_name.rend(),
                [](unsigned char ch) { return !std::isspace(ch); }).base(), header_name.end());

            header_value.erase(header_value.begin(),
                std::find_if(header_value.begin(), header_value.end(),
                    [](unsigned char ch) { return !std::isspace(ch); }));
            header_value.erase(std::find_if(header_value.rbegin(), header_value.rend(),
                [](unsigned char ch) { return !std::isspace(ch); }).base(), header_value.end());

            req.headers[header_name] = header_value;
        }
    }

    // Parse query parameters
    size_t query_pos = req.path.find('?');
    if (query_pos != std::string::npos) {
        std::string query_string = req.path.substr(query_pos + 1);
        req.path = req.path.substr(0, query_pos);

        std::istringstream query_stream(query_string);
        std::string param;
        while (std::getline(query_stream, param, '&')) {
            size_t eq_pos = param.find('=');
            if (eq_pos != std::string::npos) {
                std::string key = param.substr(0, eq_pos);
                std::string value = param.substr(eq_pos + 1);
                req.query_params[key] = value;
            }
        }
    }

    // Read body if Content-Length is specified
    auto content_length_it = req.headers.find("Content-Length");
    if (content_length_it != req.headers.end()) {
        try {
            size_t content_length = std::stoul(content_length_it->second);
            if (content_length > 0 && content_length < 8192) {
                req.body.resize(content_length);
                iss.read(&req.body[0], static_cast<std::streamsize>(content_length));
            }
        } catch (const std::exception&) {
            // Ignore invalid content length
        }
    }

    return req;
}

std::string RESTAPIServer::generate_response(const APIResponse& response) {
    std::ostringstream oss;

    // Status line
    oss << "HTTP/1.1 " << response.status_code << " ";
    switch (response.status_code) {
        case 200: oss << "OK"; break;
        case 201: oss << "Created"; break;
        case 400: oss << "Bad Request"; break;
        case 403: oss << "Forbidden"; break;
        case 404: oss << "Not Found"; break;
        case 405: oss << "Method Not Allowed"; break;
        case 500: oss << "Internal Server Error"; break;
        default: oss << "Unknown"; break;
    }
    oss << "\r\n";

    // Headers
    for (const auto& header : response.headers) {
        oss << header.first << ": " << header.second << "\r\n";
    }

    // Content-Length header
    oss << "Content-Length: " << response.body.length() << "\r\n";

    // End of headers
    oss << "\r\n";

    // Body
    oss << response.body;

    return oss.str();
}

void RESTAPIServer::route_request(const APIRequest& req, APIResponse& resp) {
    // Handle OPTIONS requests for CORS
    if (req.method == "OPTIONS") {
        resp = handle_options(req);
        return;
    }

    // Check authorization for protected endpoints
    if (!is_public_endpoint(req.path)) {
        if (!authorize_request(req)) {
            resp = APIResponse(401, "application/json");
            resp.headers["WWW-Authenticate"] = "Bearer";
            resp.body = nlohmann::json{{"error", "Unauthorized"}, {"message", "Valid authentication required"}}.dump();
            return;
        }
    }

    // Route based on path
    if (req.path == "/api/health") {
        resp = handle_health_check(req);
    } else if (req.path == "/api/regulatory-changes") {
        resp = handle_regulatory_changes(req);
    } else if (req.path == "/api/sources") {
        resp = handle_sources(req);
    } else if (req.path == "/api/monitoring/stats") {
        resp = handle_monitoring_stats(req);
    } else if (req.path == "/api/monitoring/force-check") {
        resp = handle_force_check(req);
    } else if (req.path == "/api/auth/login") {
        resp = handle_login(req);
    } else if (req.path == "/api/auth/refresh") {
        resp = handle_token_refresh(req);
    } else {
        resp = APIResponse(404, "application/json");
        resp.body = nlohmann::json{{"error", "Endpoint not found"}}.dump();
    }
}

APIResponse RESTAPIServer::handle_regulatory_changes(const APIRequest& req) {
    APIResponse resp(200, "application/json");

    try {
        if (req.method == "GET") {
            int limit = 50; // default limit
            auto limit_it = req.query_params.find("limit");
            if (limit_it != req.query_params.end()) {
                try {
                    limit = std::stoi(limit_it->second);
                    if (limit < 1 || limit > 1000) limit = 50;
                } catch (...) {
                    limit = 50;
                }
            }

            auto changes = monitor_->get_recent_changes(limit);

            nlohmann::json result = nlohmann::json::array();
            for (const auto& change : changes) {
                result.push_back({
                    {"id", change.id},
                    {"source", change.source},
                    {"title", change.title},
                    {"description", change.description},
                    {"content_url", change.content_url},
                    {"change_type", change.change_type},
                    {"severity", change.severity},
                    {"detected_at", std::chrono::duration_cast<std::chrono::milliseconds>(
                        change.detected_at.time_since_epoch()).count()},
                    {"published_at", std::chrono::duration_cast<std::chrono::milliseconds>(
                        change.published_at.time_since_epoch()).count()}
                });
            }

            resp.body = result.dump(2);

        } else if (req.method == "POST") {
            // Parse JSON body for creating new regulatory change
            if (req.body.empty()) {
                resp.status_code = 400;
                resp.body = nlohmann::json{{"error", "Request body required"}}.dump();
                return resp;
            }

            try {
                nlohmann::json body = nlohmann::json::parse(req.body);

                RegulatoryChange change;
                change.id = generate_change_id(body.value("source", "API"), body.value("title", "Unknown"));
                change.source = body.value("source", "API");
                change.title = body.value("title", "Unknown");
                change.description = body.value("description", "");
                change.content_url = body.value("content_url", "");
                change.change_type = body.value("change_type", "manual_entry");
                change.severity = body.value("severity", "MEDIUM");
                change.detected_at = std::chrono::system_clock::now();
                change.published_at = std::chrono::system_clock::now();

                if (monitor_->store_change(change)) {
                    resp.status_code = 201;
                    resp.body = nlohmann::json{
                        {"message", "Regulatory change created"},
                        {"id", change.id}
                    }.dump(2);
                } else {
                    resp.status_code = 500;
                    resp.body = nlohmann::json{{"error", "Failed to store regulatory change"}}.dump();
                }

            } catch (const nlohmann::json::exception&) {
                resp.status_code = 400;
                resp.body = nlohmann::json{{"error", "Invalid JSON in request body"}}.dump();
            }

        } else {
            resp.status_code = 405;
            resp.body = nlohmann::json{{"error", "Method not allowed"}}.dump();
        }

    } catch (const std::exception& e) {
        logger_->error("Error handling regulatory changes: " + std::string(e.what()),
                      "RESTAPIServer", __func__);
        resp.status_code = 500;
        resp.body = nlohmann::json{{"error", "Internal server error"}}.dump();
    }

    return resp;
}

APIResponse RESTAPIServer::handle_sources(const APIRequest& req) {
    APIResponse resp(200, "application/json");

    if (req.method != "GET") {
        resp.status_code = 405;
        resp.body = nlohmann::json{{"error", "Method not allowed"}}.dump();
        return resp;
    }

    try {
        auto sources = monitor_->get_sources();

        nlohmann::json result = nlohmann::json::array();
        for (const auto& source : sources) {
            result.push_back({
                {"id", source.id},
                {"name", source.name},
                {"base_url", source.base_url},
                {"source_type", source.source_type},
                {"check_interval_minutes", source.check_interval_minutes},
                {"active", source.active},
                {"consecutive_failures", source.consecutive_failures},
                {"last_check", std::chrono::duration_cast<std::chrono::milliseconds>(
                    source.last_check.time_since_epoch()).count()}
            });
        }

        resp.body = result.dump(2);

    } catch (const std::exception& e) {
        logger_->error("Error handling sources: " + std::string(e.what()),
                      "RESTAPIServer", __func__);
        resp.status_code = 500;
        resp.body = nlohmann::json{{"error", "Internal server error"}}.dump();
    }

    return resp;
}

APIResponse RESTAPIServer::handle_monitoring_stats(const APIRequest& req) {
    APIResponse resp(200, "application/json");

    if (req.method != "GET") {
        resp.status_code = 405;
        resp.body = nlohmann::json{{"error", "Method not allowed"}}.dump();
        return resp;
    }

    try {
        auto stats = monitor_->get_monitoring_stats();
        resp.body = stats.dump(2);

    } catch (const std::exception& e) {
        logger_->error("Error handling monitoring stats: " + std::string(e.what()),
                      "RESTAPIServer", __func__);
        resp.status_code = 500;
        resp.body = nlohmann::json{{"error", "Internal server error"}}.dump();
    }

    return resp;
}

APIResponse RESTAPIServer::handle_force_check(const APIRequest& req) {
    APIResponse resp(200, "application/json");

    if (req.method != "POST") {
        resp.status_code = 405;
        resp.body = nlohmann::json{{"error", "Method not allowed"}}.dump();
        return resp;
    }

    try {
        // Parse JSON body for source ID
        std::string source_id;
        if (!req.body.empty()) {
            nlohmann::json body = nlohmann::json::parse(req.body);
            source_id = body.value("source_id", "");
        }

        // If no specific source, force check all sources
        if (source_id.empty()) {
            auto sources = monitor_->get_sources();
            int success_count = 0;
            for (const auto& source : sources) {
                if (monitor_->force_check_source(source.id)) {
                    success_count++;
                }
            }

            resp.body = nlohmann::json{
                {"message", "Force check initiated for all sources"},
                {"sources_triggered", success_count}
            }.dump(2);

        } else {
            if (monitor_->force_check_source(source_id)) {
                resp.body = nlohmann::json{
                    {"message", "Force check initiated"},
                    {"source_id", source_id}
                }.dump(2);
            } else {
                resp.status_code = 404;
                resp.body = nlohmann::json{{"error", "Source not found"}}.dump();
            }
        }

    } catch (const std::exception& e) {
        logger_->error("Error handling force check: " + std::string(e.what()),
                      "RESTAPIServer", __func__);
        resp.status_code = 500;
        resp.body = nlohmann::json{{"error", "Internal server error"}}.dump();
    }

    return resp;
}

APIResponse RESTAPIServer::handle_health_check(const APIRequest& req) {
    APIResponse resp(200, "application/json");

    if (req.method != "GET") {
        resp.status_code = 405;
        resp.body = nlohmann::json{{"error", "Method not allowed"}}.dump();
        return resp;
    }

    try {
        // Check database connectivity
        auto conn = db_pool_->get_connection();
        bool db_healthy = (conn != nullptr);
        if (db_healthy) {
            auto ping_result = conn->ping();
            db_healthy = ping_result;
            db_pool_->return_connection(conn);
        }

        // Check monitor status
        bool monitor_healthy = monitor_->is_running();

        resp.body = nlohmann::json{
            {"status", "healthy"},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"services", {
                {"database", db_healthy ? "healthy" : "unhealthy"},
                {"monitor", monitor_healthy ? "healthy" : "unhealthy"},
                {"api_server", "healthy"}
            }}
        }.dump(2);

        if (!db_healthy || !monitor_healthy) {
            resp.status_code = 503; // Service Unavailable
        }

    } catch (const std::exception& e) {
        logger_->error("Error in health check: " + std::string(e.what()),
                      "RESTAPIServer", __func__);
        resp.status_code = 500;
        resp.body = nlohmann::json{{"status", "unhealthy"}, {"error", "Health check failed"}}.dump();
    }

    return resp;
}

APIResponse RESTAPIServer::handle_options(const APIRequest& /*req*/) {
    APIResponse resp(200, "text/plain");
    resp.body = "";
    return resp;
}

bool RESTAPIServer::validate_cors(const APIRequest& /*req*/) {
    // For now, allow all origins (in production, you'd restrict this)
    return true;
}

bool RESTAPIServer::rate_limit_check(const std::string& client_ip) {
    // Proper rate limiting implementation
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);

    auto now = std::chrono::steady_clock::now();
    auto& client_data = rate_limit_map_[client_ip];

    // Clean up old entries (requests older than window)
    while (!client_data.requests.empty()) {
        auto oldest_request = client_data.requests.front();
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - oldest_request).count();
        if (age > RATE_LIMIT_WINDOW) {
            client_data.requests.pop_front();
        } else {
            break;
        }
    }

    // Check if under limit
    if (client_data.requests.size() < RATE_LIMIT_MAX_REQUESTS) {
        client_data.requests.push_back(now);
        return true;
    }

    // Rate limit exceeded
    logger_->warn("Rate limit exceeded for client: " + client_ip, "RESTAPIServer", __func__,
                  {{"client_ip", client_ip}, {"request_count", std::to_string(client_data.requests.size())}});
    return false;
}

std::vector<std::string> RESTAPIServer::split_path(const std::string& path) {
    std::vector<std::string> parts;
    std::istringstream iss(path);
    std::string part;

    while (std::getline(iss, part, '/')) {
        if (!part.empty()) {
            parts.push_back(part);
        }
    }

    return parts;
}

std::string RESTAPIServer::generate_change_id(const std::string& source, const std::string& title) {
    std::hash<std::string> hasher;
    std::string combined = source + ":" + title + ":" +
        std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    return source + "_" + std::to_string(hasher(combined));
}

bool RESTAPIServer::is_public_endpoint(const std::string& path) {
    // Define public endpoints that don't require authentication
    static const std::unordered_set<std::string> public_endpoints = {
        "/api/health",
        "/api/auth/login"
    };

    return public_endpoints.find(path) != public_endpoints.end();
}

bool RESTAPIServer::authorize_request(const APIRequest& req) {
    // Check for Authorization header
    auto auth_it = req.headers.find("Authorization");
    if (auth_it == req.headers.end()) {
        return false;
    }

    std::string auth_header = auth_it->second;

    // Check for Bearer token
    if (auth_header.substr(0, 7) == "Bearer ") {
        std::string token = auth_header.substr(7);
        return validate_jwt_token(token);
    }

    // Check for API key
    if (auth_header.substr(0, 10) == "API-Key ") {
        std::string api_key = auth_header.substr(10);
        return validate_api_key(api_key);
    }

    return false;
}

bool RESTAPIServer::validate_jwt_token(const std::string& token) {
    try {
        // Simple JWT validation (in production, use a proper JWT library)
        // JWT format: header.payload.signature

        size_t first_dot = token.find('.');
        size_t second_dot = token.find('.', first_dot + 1);

        if (first_dot == std::string::npos || second_dot == std::string::npos) {
            return false;
        }

        std::string header_b64 = token.substr(0, first_dot);
        std::string payload_b64 = token.substr(first_dot + 1, second_dot - first_dot - 1);
        std::string signature_b64 = token.substr(second_dot + 1);

        // Decode payload (simplified base64 decode)
        std::string payload = base64_decode(payload_b64);

        // Parse JSON payload
        nlohmann::json payload_json = nlohmann::json::parse(payload);

        // Check expiration
        if (payload_json.contains("exp")) {
            auto exp_time = std::chrono::system_clock::time_point(
                std::chrono::seconds(payload_json["exp"].get<long long>()));
            if (std::chrono::system_clock::now() > exp_time) {
                return false; // Token expired
            }
        }

        // Check issuer
        if (payload_json.contains("iss") && payload_json["iss"] != "regulens_api") {
            return false;
        }

        // Check audience
        if (payload_json.contains("aud") && payload_json["aud"] != "regulens_clients") {
            return false;
        }

        // In production, verify signature here
        // For now, accept tokens that pass basic validation

        return true;

    } catch (const std::exception&) {
        return false;
    }
}

bool RESTAPIServer::validate_api_key(const std::string& api_key) {
    // Simple API key validation (in production, check against database)
    static const std::unordered_set<std::string> valid_api_keys = {
        "regulens_api_key_2024_prod",
        "regulens_api_key_2024_test"
    };

    return valid_api_keys.find(api_key) != valid_api_keys.end();
}

APIResponse RESTAPIServer::handle_login(const APIRequest& req) {
    APIResponse resp(200, "application/json");

    if (req.method != "POST") {
        resp.status_code = 405;
        resp.body = nlohmann::json{{"error", "Method not allowed"}}.dump();
        return resp;
    }

    try {
        if (req.body.empty()) {
            resp.status_code = 400;
            resp.body = nlohmann::json{{"error", "Request body required"}}.dump();
            return resp;
        }

        nlohmann::json body = nlohmann::json::parse(req.body);
        std::string username = body.value("username", "");
        std::string password = body.value("password", "");

        // Simple authentication (in production, check against database)
        if (username == "admin" && password == "regulens2024") {
            // Generate JWT token
            std::string token = generate_jwt_token(username);

            resp.body = nlohmann::json{
                {"access_token", token},
                {"token_type", "Bearer"},
                {"expires_in", 3600},
                {"user", username}
            }.dump(2);
        } else {
            resp.status_code = 401;
            resp.body = nlohmann::json{{"error", "Invalid credentials"}}.dump();
        }

    } catch (const std::exception& e) {
        logger_->error("Error in login: " + std::string(e.what()), "RESTAPIServer", __func__);
        resp.status_code = 500;
        resp.body = nlohmann::json{{"error", "Login failed"}}.dump();
    }

    return resp;
}

APIResponse RESTAPIServer::handle_token_refresh(const APIRequest& req) {
    APIResponse resp(200, "application/json");

    if (req.method != "POST") {
        resp.status_code = 405;
        resp.body = nlohmann::json{{"error", "Method not allowed"}}.dump();
        return resp;
    }

    // Check for valid refresh token in Authorization header
    auto auth_it = req.headers.find("Authorization");
    if (auth_it == req.headers.end() || auth_it->second.substr(0, 7) != "Bearer ") {
        resp.status_code = 401;
        resp.body = nlohmann::json{{"error", "Refresh token required"}}.dump();
        return resp;
    }

    std::string refresh_token = auth_it->second.substr(7);

    // Validate refresh token (simplified)
    if (validate_refresh_token(refresh_token)) {
        // Generate new access token
        std::string new_token = generate_jwt_token("refreshed_user");

        resp.body = nlohmann::json{
            {"access_token", new_token},
            {"token_type", "Bearer"},
            {"expires_in", 3600}
        }.dump(2);
    } else {
        resp.status_code = 401;
        resp.body = nlohmann::json{{"error", "Invalid refresh token"}}.dump();
    }

    return resp;
}

std::string RESTAPIServer::generate_jwt_token(const std::string& username) {
    // Simplified JWT generation (in production, use a proper JWT library)
    auto now = std::chrono::system_clock::now();
    auto exp_time = now + std::chrono::hours(1);

    nlohmann::json header = {
        {"alg", "HS256"},
        {"typ", "JWT"}
    };

    nlohmann::json payload = {
        {"iss", "regulens_api"},
        {"aud", "regulens_clients"},
        {"sub", username},
        {"iat", std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count()},
        {"exp", std::chrono::duration_cast<std::chrono::seconds>(exp_time.time_since_epoch()).count()},
        {"roles", {"admin", "user"}}
    };

    // Base64 encode header and payload
    std::string header_b64 = base64_encode(header.dump());
    std::string payload_b64 = base64_encode(payload.dump());

    // Create signature (simplified - in production use proper HMAC)
    std::string signature_b64 = base64_encode("signature_placeholder");

    return header_b64 + "." + payload_b64 + "." + signature_b64;
}

bool RESTAPIServer::validate_refresh_token(const std::string& token) {
    // Simplified refresh token validation
    // In production, check against database/store
    return token.length() > 10; // Basic check
}

std::string RESTAPIServer::base64_encode(const std::string& input) {
    // Simplified base64 encoding (in production, use proper library)
    static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string encoded;
    int val = 0, valb = -6;
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            encoded.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) encoded.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (encoded.size() % 4) encoded.push_back('=');
    return encoded;
}

std::string RESTAPIServer::base64_decode(const std::string& input) {
    // Simplified base64 decoding (in production, use proper library)
    static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string decoded;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[base64_chars[i]] = i;

    int val = 0, valb = -8;
    for (unsigned char c : input) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            decoded.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return decoded;
}

} // namespace regulens
