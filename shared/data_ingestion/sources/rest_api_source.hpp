/**
 * REST API Data Source - Production-Grade API Integration
 *
 * Enhanced REST API client that builds upon existing HTTP client with:
 * - Connection pooling and reuse
 * - Automatic retry logic with exponential backoff
 * - Rate limiting and throttling
 * - Authentication support (API keys, OAuth, JWT)
 * - Pagination handling
 * - Response caching
 *
 * Retrospective Enhancement: Improves existing regulatory monitoring APIs
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <chrono>
#include "../data_ingestion_framework.hpp"
#include "../../network/http_client.hpp"
#include "../../logging/structured_logger.hpp"
#include <nlohmann/json.hpp>

namespace regulens {

enum class AuthenticationType {
    NONE,
    API_KEY_HEADER,
    API_KEY_QUERY,
    BASIC_AUTH,
    OAUTH2,
    JWT_BEARER
};

enum class PaginationType {
    NONE,
    OFFSET_LIMIT,
    PAGE_BASED,
    CURSOR_BASED,
    LINK_HEADER
};

struct RESTAPIConfig {
    std::string base_url;
    std::string endpoint_path;
    AuthenticationType auth_type = AuthenticationType::NONE;
    std::unordered_map<std::string, std::string> auth_params;
    PaginationType pagination_type = PaginationType::NONE;
    std::unordered_map<std::string, std::string> pagination_params;
    std::unordered_map<std::string, std::string> query_params;
    int page_size = 100;
    int max_pages = 100;
    std::chrono::seconds rate_limit_window = std::chrono::seconds(60);
    int rate_limit_requests = 60;
    std::chrono::seconds cache_ttl = std::chrono::seconds(300);
    bool follow_redirects = true;
    int max_redirects = 5;
    std::unordered_map<std::string, std::string> default_headers;
    std::unordered_map<std::string, std::string> custom_headers;
    nlohmann::json request_template;
};

class RESTAPISource : public DataSource {
public:
    RESTAPISource(const DataIngestionConfig& config,
                  std::shared_ptr<HttpClient> http_client,
                  StructuredLogger* logger);

    ~RESTAPISource() override;

    // DataSource interface implementation
    bool connect() override;
    void disconnect() override;
    bool is_connected() const override;
    std::vector<nlohmann::json> fetch_data() override;
    bool validate_connection() override;

    // REST API specific methods
    void set_api_config(const RESTAPIConfig& api_config);
    bool authenticate();
    std::vector<nlohmann::json> fetch_paginated_data();
    HttpResponse make_authenticated_request(const std::string& method,
                                            const std::string& path,
                                            const std::string& body = "");

private:
    // Authentication methods
    bool authenticate_api_key();
    bool authenticate_basic_auth();
    bool authenticate_oauth2();
    bool authenticate_jwt();

    // Pagination handling
    std::vector<nlohmann::json> handle_offset_pagination();
    std::vector<nlohmann::json> handle_page_pagination();
    std::vector<nlohmann::json> handle_cursor_pagination();
    std::vector<nlohmann::json> handle_link_pagination();

    // Request building and execution
    HttpResponse execute_request(const std::string& method,
                               const std::string& url,
                               const std::string& body = "",
                               const std::unordered_map<std::string, std::string>& headers = {});
    std::string build_url(const std::string& path, const std::unordered_map<std::string, std::string>& params = {});
    std::unordered_map<std::string, std::string> build_headers();

    // Rate limiting and caching
    bool check_rate_limit();
    void update_rate_limit();
    nlohmann::json get_cached_response(const std::string& cache_key);
    void set_cached_response(const std::string& cache_key, const nlohmann::json& response);

    // Response processing
    std::vector<nlohmann::json> parse_response(const HttpResponse& response);
    bool validate_response(const HttpResponse& response);
    std::string extract_next_page_url(const HttpResponse& response);

    // Connection management
    bool test_connection();
    void refresh_auth_token();
    bool handle_rate_limit_exceeded(const HttpResponse& response);

    // Helper functions
    std::string base64_encode(const std::string& input);
    std::string url_encode(const std::string& value);
    nlohmann::json extract_json_path(const nlohmann::json& json_data, const std::string& path);

    // Internal state
    RESTAPIConfig api_config_;
    bool connected_;
    std::string auth_token_;
    std::string auth_header_name_;  // Custom authentication header name (e.g., "X-API-Key")
    std::string oauth_refresh_token_;
    std::chrono::system_clock::time_point token_expiry_;
    int request_count_in_window_;
    std::chrono::steady_clock::time_point window_start_;
    std::unordered_map<std::string, std::pair<nlohmann::json, std::chrono::system_clock::time_point>> response_cache_;

    // Constants
    const int MAX_RETRIES = 3;
    const std::chrono::seconds RETRY_DELAY = std::chrono::seconds(1);
    const std::chrono::seconds TOKEN_REFRESH_BUFFER = std::chrono::seconds(300); // Refresh 5 mins before expiry
};

} // namespace regulens
