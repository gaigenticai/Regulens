/**
 * REST API Data Source Implementation - Production-Grade API Integration
 */

#include "rest_api_source.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <regex>
#include <thread>

namespace regulens {

RESTAPISource::RESTAPISource(const DataIngestionConfig& config,
                           std::shared_ptr<HttpClient> http_client,
                           StructuredLogger* logger)
    : DataSource(config, http_client, logger), connected_(false), request_count_in_window_(0) {
}

RESTAPISource::~RESTAPISource() {
    disconnect();
}

bool RESTAPISource::connect() {
    if (connected_) return true;

    // Test basic connectivity
    connected_ = test_connection();
    if (connected_) {
        logger_->log(LogLevel::INFO, "REST API source connected: " + config_.source_id);
    }
    return connected_;
}

void RESTAPISource::disconnect() {
    if (!connected_) return;

    connected_ = false;
    logger_->log(LogLevel::INFO, "REST API source disconnected: " + config_.source_id);
}

bool RESTAPISource::is_connected() const {
    return connected_;
}

std::vector<nlohmann::json> RESTAPISource::fetch_data() {
    if (!connected_) return {};

    try {
        return fetch_paginated_data();
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR,
                    "Error fetching data from REST API: " + std::string(e.what()));
        return {};
    }
}

bool RESTAPISource::validate_connection() {
    return test_connection();
}

void RESTAPISource::set_api_config(const RESTAPIConfig& api_config) {
    api_config_ = api_config;
}

bool RESTAPISource::authenticate() {
    if (api_config_.auth_type == AuthenticationType::NONE) return true;

    switch (api_config_.auth_type) {
        case AuthenticationType::API_KEY_HEADER:
        case AuthenticationType::API_KEY_QUERY:
            return authenticate_api_key();
        case AuthenticationType::BASIC_AUTH:
            return authenticate_basic_auth();
        case AuthenticationType::OAUTH2:
            return authenticate_oauth2();
        case AuthenticationType::JWT_BEARER:
            return authenticate_jwt();
        default:
            return false;
    }
}

std::vector<nlohmann::json> RESTAPISource::fetch_paginated_data() {
    std::vector<nlohmann::json> all_data;

    if (api_config_.pagination_type == PaginationType::NONE) {
        // Single request
        std::string body;
        auto headers = build_headers();
        auto url = build_url(api_config_.endpoint_path);
        auto response = execute_request("GET", url, body, headers);

        if (response.success) {
            auto data = parse_response(response);
            all_data.insert(all_data.end(), data.begin(), data.end());
        }
    } else {
        // Handle pagination based on type
        switch (api_config_.pagination_type) {
            case PaginationType::OFFSET_LIMIT:
                all_data = handle_offset_pagination();
                break;
            case PaginationType::PAGE_BASED:
                all_data = handle_page_pagination();
                break;
            case PaginationType::CURSOR_BASED:
                all_data = handle_cursor_pagination();
                break;
            case PaginationType::LINK_HEADER:
                all_data = handle_link_pagination();
                break;
            default:
                break;
        }
    }

    return all_data;
}

HttpResponse RESTAPISource::make_authenticated_request(const std::string& method,
                                                      const std::string& path,
                                                      const std::string& body) {
    if (!authenticate()) {
        logger_->log(LogLevel::ERROR, "Authentication failed for REST API request");
        HttpResponse error_response;
        error_response.success = false;
        error_response.status_code = 401;
        error_response.error_message = "Authentication failed";
        return error_response;
    }

    auto headers = build_headers();
    auto url = build_url(path);

    return execute_request(method, url, body, headers);
}

// Private implementation methods (simplified for demo)
bool RESTAPISource::authenticate_api_key() {
    auth_token_ = api_config_.auth_params.at("api_key");
    return true;
}

bool RESTAPISource::authenticate_basic_auth() {
    if (!api_config_.auth_params.contains("username") || !api_config_.auth_params.contains("password")) {
        logger_->log(LogLevel::ERROR, "Basic auth missing username or password");
        return false;
    }

    std::string username = api_config_.auth_params["username"];
    std::string password = api_config_.auth_params["password"];

    // Base64 encode credentials
    std::string credentials = username + ":" + password;
    std::string encoded = base64_encode(credentials);

    auth_token_ = "Basic " + encoded;

    logger_->log(LogLevel::INFO, "Basic authentication configured for user: " + username);
    return true;
}

bool RESTAPISource::authenticate_oauth2() {
    // OAuth 2.0 Client Credentials Flow
    if (!api_config_.auth_params.contains("client_id") || !api_config_.auth_params.contains("client_secret")) {
        logger_->log(LogLevel::ERROR, "OAuth2 missing client_id or client_secret");
        return false;
    }

    std::string client_id = api_config_.auth_params["client_id"];
    std::string client_secret = api_config_.auth_params["client_secret"];

    std::string token_url = api_config_.base_url + "/oauth/token";
    auto token_url_it = api_config_.auth_params.find("token_url");
    if (token_url_it != api_config_.auth_params.end()) {
        token_url = token_url_it->second;
    }

    std::string scope;
    auto scope_it = api_config_.auth_params.find("scope");
    if (scope_it != api_config_.auth_params.end()) {
        scope = scope_it->second;
    }

    // Build token request body
    std::string body = "grant_type=client_credentials";
    body += "&client_id=" + url_encode(client_id);
    body += "&client_secret=" + url_encode(client_secret);
    if (!scope.empty()) {
        body += "&scope=" + url_encode(scope);
    }

    // Request access token
    std::unordered_map<std::string, std::string> headers = {
        {"Content-Type", "application/x-www-form-urlencoded"},
        {"Accept", "application/json"}
    };

    try {
        HttpResponse response = http_client_->post(token_url, body, headers);

        if (response.success && response.status_code == 200) {
            auto token_data = nlohmann::json::parse(response.body);

            if (token_data.contains("access_token")) {
                auth_token_ = "Bearer " + token_data["access_token"].get<std::string>();

                // Store token expiration if provided
                if (token_data.contains("expires_in")) {
                    int expires_in = token_data["expires_in"];
                    token_expiry_ = std::chrono::system_clock::now() + std::chrono::seconds(expires_in);
                    logger_->log(LogLevel::INFO, "OAuth2 token obtained, expires in " + std::to_string(expires_in) + " seconds");
                }

                // Store refresh token if provided
                if (token_data.contains("refresh_token")) {
                    oauth_refresh_token_ = token_data["refresh_token"].get<std::string>();
                }

                logger_->log(LogLevel::INFO, "OAuth2 authentication successful");
                return true;
            } else {
                logger_->log(LogLevel::ERROR, "OAuth2 response missing access_token");
                return false;
            }
        } else {
            logger_->log(LogLevel::ERROR, "OAuth2 token request failed: HTTP " + std::to_string(response.status_code));
            return false;
        }
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "OAuth2 authentication exception: " + std::string(e.what()));
        return false;
    }
}

bool RESTAPISource::authenticate_jwt() {
    auth_token_ = api_config_.auth_params.at("jwt_token");
    return true;
}

std::vector<nlohmann::json> RESTAPISource::handle_offset_pagination() {
    std::vector<nlohmann::json> all_data;
    int offset = 0;
    const int limit = api_config_.page_size;

    for (int page = 0; page < api_config_.max_pages; ++page) {
        std::unordered_map<std::string, std::string> params = {
            {"offset", std::to_string(offset)},
            {"limit", std::to_string(limit)}
        };

        auto url = build_url(api_config_.endpoint_path, params);
        auto response = execute_request("GET", url);

        if (!response.success) break;

        auto page_data = parse_response(response);
        if (page_data.empty()) break;

        all_data.insert(all_data.end(), page_data.begin(), page_data.end());
        offset += limit;

        if (static_cast<int>(page_data.size()) < limit) break;
    }

    return all_data;
}

std::vector<nlohmann::json> RESTAPISource::handle_page_pagination() {
    std::vector<nlohmann::json> all_data;

    int current_page = 1;
    auto start_page_it = api_config_.pagination_params.find("start_page");
    if (start_page_it != api_config_.pagination_params.end()) {
        current_page = std::stoi(start_page_it->second);
    }

    const int max_pages = api_config_.max_pages;
    const int page_size = api_config_.page_size;

    std::string page_param = "page";
    auto page_param_it = api_config_.pagination_params.find("page_param");
    if (page_param_it != api_config_.pagination_params.end()) {
        page_param = page_param_it->second;
    }

    std::string size_param = "page_size";
    auto size_param_it = api_config_.pagination_params.find("size_param");
    if (size_param_it != api_config_.pagination_params.end()) {
        size_param = size_param_it->second;
    }

    for (int page = 0; page < max_pages; ++page) {
        std::unordered_map<std::string, std::string> params = {
            {page_param, std::to_string(current_page)},
            {size_param, std::to_string(page_size)}
        };

        // Add any additional query parameters
        for (const auto& [key, value] : api_config_.query_params) {
            params[key] = value;
        }

        auto url = build_url(api_config_.endpoint_path, params);
        auto response = execute_request("GET", url);

        if (!response.success) {
            logger_->log(LogLevel::WARN, "Page " + std::to_string(current_page) + " request failed");
            break;
        }

        auto page_data = parse_response(response);
        if (page_data.empty()) {
            logger_->log(LogLevel::DEBUG, "No more data at page " + std::to_string(current_page));
            break;
        }

        all_data.insert(all_data.end(), page_data.begin(), page_data.end());
        logger_->log(LogLevel::DEBUG, "Fetched page " + std::to_string(current_page) +
                    " with " + std::to_string(page_data.size()) + " items");

        // Check if we got a partial page (indicates last page)
        if (static_cast<int>(page_data.size()) < page_size) {
            logger_->log(LogLevel::INFO, "Reached last page at page " + std::to_string(current_page));
            break;
        }

        current_page++;
    }

    logger_->log(LogLevel::INFO, "Page pagination complete: fetched " +
                std::to_string(all_data.size()) + " total records");
    return all_data;
}

std::vector<nlohmann::json> RESTAPISource::handle_cursor_pagination() {
    std::vector<nlohmann::json> all_data;
    std::string cursor;
    int page_count = 0;
    const int max_pages = api_config_.max_pages;

    std::string cursor_param = "cursor";
    auto cursor_param_it = api_config_.pagination_params.find("cursor_param");
    if (cursor_param_it != api_config_.pagination_params.end()) {
        cursor_param = cursor_param_it->second;
    }

    std::string cursor_path = "next_cursor";
    auto cursor_path_it = api_config_.pagination_params.find("cursor_response_path");
    if (cursor_path_it != api_config_.pagination_params.end()) {
        cursor_path = cursor_path_it->second;
    }

    while (page_count < max_pages) {
        std::unordered_map<std::string, std::string> params;

        // Add cursor if we have one
        if (!cursor.empty()) {
            params[cursor_param] = cursor;
        }

        // Add page size if specified
        if (api_config_.page_size > 0) {
            std::string size_param = "limit";
            auto size_param_it = api_config_.pagination_params.find("size_param");
            if (size_param_it != api_config_.pagination_params.end()) {
                size_param = size_param_it->second;
            }
            params[size_param] = std::to_string(api_config_.page_size);
        }

        // Add any additional query parameters
        for (const auto& [key, value] : api_config_.query_params) {
            params[key] = value;
        }

        auto url = build_url(api_config_.endpoint_path, params);
        auto response = execute_request("GET", url);

        if (!response.success) {
            logger_->log(LogLevel::WARN, "Cursor pagination request failed at cursor: " + cursor);
            break;
        }

        // Parse response to get data and next cursor
        try {
            auto json_response = nlohmann::json::parse(response.body);

            // Extract data items
            auto page_data = parse_response(response);
            if (page_data.empty()) {
                logger_->log(LogLevel::DEBUG, "No more data in cursor pagination");
                break;
            }

            all_data.insert(all_data.end(), page_data.begin(), page_data.end());
            logger_->log(LogLevel::DEBUG, "Fetched cursor page " + std::to_string(page_count + 1) +
                        " with " + std::to_string(page_data.size()) + " items");

            // Extract next cursor using JSON path
            nlohmann::json next_cursor_value = extract_json_path(json_response, cursor_path);

            if (next_cursor_value.is_null() || (next_cursor_value.is_string() && next_cursor_value.get<std::string>().empty())) {
                logger_->log(LogLevel::INFO, "No more pages (next cursor is null/empty)");
                break;
            }

            cursor = next_cursor_value.is_string() ? next_cursor_value.get<std::string>() : next_cursor_value.dump();

        } catch (const nlohmann::json::exception& e) {
            logger_->log(LogLevel::ERROR, "Failed to parse cursor pagination response: " + std::string(e.what()));
            break;
        }

        page_count++;
    }

    logger_->log(LogLevel::INFO, "Cursor pagination complete: fetched " +
                std::to_string(all_data.size()) + " total records across " +
                std::to_string(page_count) + " pages");
    return all_data;
}

std::vector<nlohmann::json> RESTAPISource::handle_link_pagination() {
    std::vector<nlohmann::json> all_data;
    std::string next_url = build_url(api_config_.endpoint_path, api_config_.query_params);
    int page_count = 0;
    const int max_pages = api_config_.max_pages;

    while (!next_url.empty() && page_count < max_pages) {
        auto response = execute_request("GET", next_url);

        if (!response.success) {
            logger_->log(LogLevel::WARN, "Link pagination request failed");
            break;
        }

        auto page_data = parse_response(response);
        if (page_data.empty()) {
            logger_->log(LogLevel::DEBUG, "No more data in link pagination");
            break;
        }

        all_data.insert(all_data.end(), page_data.begin(), page_data.end());
        logger_->log(LogLevel::DEBUG, "Fetched link page " + std::to_string(page_count + 1) +
                    " with " + std::to_string(page_data.size()) + " items");

        // Extract next link from response headers (RFC 5988 Link header)
        next_url = "";
        auto link_header_it = response.headers.find("Link");
        if (link_header_it != response.headers.end()) {
            std::string link_header = link_header_it->second;

            // Parse Link header: <https://api.example.com/items?page=2>; rel="next"
            std::regex next_link_pattern(R"(<([^>]+)>;\s*rel\s*=\s*[\"']?next[\"']?)");
            std::smatch match;

            if (std::regex_search(link_header, match, next_link_pattern)) {
                next_url = match[1].str();
                logger_->log(LogLevel::DEBUG, "Found next link: " + next_url);
            } else {
                logger_->log(LogLevel::INFO, "No next link found in Link header");
            }
        } else {
            // Try to find next link in response body
            try {
                auto json_response = nlohmann::json::parse(response.body);

                std::string next_path = "links.next";
                auto next_path_it = api_config_.pagination_params.find("next_link_path");
                if (next_path_it != api_config_.pagination_params.end()) {
                    next_path = next_path_it->second;
                }

                nlohmann::json next_link = extract_json_path(json_response, next_path);
                if (!next_link.is_null() && next_link.is_string()) {
                    next_url = next_link.get<std::string>();
                    logger_->log(LogLevel::DEBUG, "Found next link in body: " + next_url);
                }
            } catch (const nlohmann::json::exception&) {
                logger_->log(LogLevel::DEBUG, "Could not find next link in response");
            }
        }

        page_count++;
    }

    logger_->log(LogLevel::INFO, "Link pagination complete: fetched " +
                std::to_string(all_data.size()) + " total records across " +
                std::to_string(page_count) + " pages");
    return all_data;
}

HttpResponse RESTAPISource::execute_request(const std::string& method,
                                         const std::string& url,
                                         const std::string& body,
                                         const std::unordered_map<std::string, std::string>& additional_headers) {
    if (!check_rate_limit()) {
        logger_->log(LogLevel::WARN, "Rate limit exceeded, request blocked");
        HttpResponse rate_limit_response;
        rate_limit_response.success = false;
        rate_limit_response.status_code = 429;
        rate_limit_response.error_message = "Rate limit exceeded";
        return rate_limit_response;
    }

    update_rate_limit();

    // Build headers with authentication
    std::unordered_map<std::string, std::string> request_headers = additional_headers;

    // Add authentication header
    if (!auth_token_.empty()) {
        request_headers["Authorization"] = auth_token_;
    }

    // Add default headers if not present
    if (request_headers.find("Accept") == request_headers.end()) {
        request_headers["Accept"] = "application/json";
    }
    if (method == "POST" || method == "PUT" || method == "PATCH") {
        if (request_headers.find("Content-Type") == request_headers.end()) {
            request_headers["Content-Type"] = "application/json";
        }
    }

    // Add custom headers from config
    for (const auto& [key, value] : api_config_.custom_headers) {
        if (request_headers.find(key) == request_headers.end()) {
            request_headers[key] = value;
        }
    }

    // Execute request with retry logic
    int max_retries = 3;
    int retry_delay_ms = 1000;

    for (int attempt = 0; attempt <= max_retries; ++attempt) {
        try {
            HttpResponse response;

            if (method == "GET") {
                response = http_client_->get(url, request_headers);
            } else if (method == "POST") {
                response = http_client_->post(url, body, request_headers);
            } else if (method == "PUT") {
                response = http_client_->put(url, body, request_headers);
            } else if (method == "DELETE") {
                response = http_client_->del(url, request_headers);
            } else if (method == "PATCH") {
                response = http_client_->patch(url, body, request_headers);
            } else {
                logger_->log(LogLevel::ERROR, "Unsupported HTTP method: " + method);
                HttpResponse unsupported_response;
                unsupported_response.success = false;
                unsupported_response.status_code = 400;
                unsupported_response.error_message = "Unsupported method";
                return unsupported_response;
            }

            // Check if response is successful
            if (response.success && response.status_code >= 200 && response.status_code < 300) {
                logger_->log(LogLevel::DEBUG, method + " " + url + " -> " + std::to_string(response.status_code));
                return response;
            }

            // Check if we should retry
            if (attempt < max_retries) {
                // Retry on specific status codes
                if (response.status_code == 429 ||  // Too Many Requests
                    response.status_code == 500 ||  // Internal Server Error
                    response.status_code == 502 ||  // Bad Gateway
                    response.status_code == 503 ||  // Service Unavailable
                    response.status_code == 504) {  // Gateway Timeout

                    logger_->log(LogLevel::WARN, "Request failed with " + std::to_string(response.status_code) +
                                ", retrying in " + std::to_string(retry_delay_ms) + "ms (attempt " +
                                std::to_string(attempt + 1) + "/" + std::to_string(max_retries) + ")");

                    std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));
                    retry_delay_ms *= 2;  // Exponential backoff
                    continue;
                }
            }

            // Non-retriable error or max retries exceeded
            logger_->log(LogLevel::ERROR, method + " " + url + " failed with status " +
                        std::to_string(response.status_code) + ": " + response.body);
            return response;

        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR, "Request exception on attempt " + std::to_string(attempt + 1) +
                        ": " + std::string(e.what()));

            if (attempt < max_retries) {
                std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));
                retry_delay_ms *= 2;
                continue;
            }

            HttpResponse exception_response;
            exception_response.success = false;
            exception_response.status_code = 0;
            exception_response.error_message = std::string("Exception: ") + e.what();
            return exception_response;
        }
    }

    HttpResponse max_retries_response;
    max_retries_response.success = false;
    max_retries_response.status_code = 0;
    max_retries_response.error_message = "Max retries exceeded";
    return max_retries_response;
}

std::string RESTAPISource::build_url(const std::string& path, const std::unordered_map<std::string, std::string>& params) {
    std::string url = api_config_.base_url + path;

    if (!params.empty()) {
        url += "?";
        for (auto it = params.begin(); it != params.end(); ++it) {
            if (it != params.begin()) url += "&";
            url += it->first + "=" + it->second;
        }
    }

    return url;
}

std::unordered_map<std::string, std::string> RESTAPISource::build_headers() {
    std::unordered_map<std::string, std::string> headers = api_config_.default_headers;

    if (!auth_token_.empty()) {
        if (api_config_.auth_type == AuthenticationType::API_KEY_HEADER) {
            headers["X-API-Key"] = auth_token_;
        } else if (api_config_.auth_type == AuthenticationType::JWT_BEARER) {
            headers["Authorization"] = "Bearer " + auth_token_;
        }
    }

    headers["Content-Type"] = "application/json";
    headers["User-Agent"] = "Regulens-Data-Ingestion/1.0";

    return headers;
}

bool RESTAPISource::check_rate_limit() {
    return request_count_in_window_ < api_config_.rate_limit_requests;
}

void RESTAPISource::update_rate_limit() {
    ++request_count_in_window_;
}

nlohmann::json RESTAPISource::get_cached_response(const std::string& cache_key) {
    auto it = response_cache_.find(cache_key);
    if (it != response_cache_.end()) {
        auto& [response, timestamp] = it->second;
        auto age = std::chrono::system_clock::now() - timestamp;
        if (age < api_config_.cache_ttl) {
            return response;
        }
    }
    return nullptr;
}

void RESTAPISource::set_cached_response(const std::string& cache_key, const nlohmann::json& response) {
    response_cache_[cache_key] = {response, std::chrono::system_clock::now()};
}

std::vector<nlohmann::json> RESTAPISource::parse_response(const HttpResponse& response) {
    try {
        auto json = nlohmann::json::parse(response.body);
        if (json.contains("data") && json["data"].is_array()) {
            return json["data"];
        } else if (json.is_array()) {
            return json;
        } else {
            return {json};
        }
    } catch (const std::exception&) {
        return {};
    }
}

bool RESTAPISource::validate_response(const HttpResponse& response) {
    return response.success && response.status_code >= 200 && response.status_code < 300;
}

std::string RESTAPISource::extract_next_page_url(const HttpResponse& response) {
    // Production-grade next page URL extraction from various pagination patterns
    
    // Method 1: Check Link header (RFC 8288 - common in REST APIs)
    auto link_header = response.headers.find("Link");
    if (link_header != response.headers.end()) {
        const std::string& link_value = link_header->second;
        
        // Parse Link header: <https://api.example.com/items?page=3>; rel="next"
        std::regex next_link_pattern(R"(<([^>]+)>;\s*rel="next")");
        std::smatch match;
        if (std::regex_search(link_value, match, next_link_pattern)) {
            return match[1].str();
        }
    }
    
    // Method 2: Parse JSON response body for next page URL
    try {
        auto json_body = nlohmann::json::parse(response.body);
        
        // Common JSON pagination patterns
        std::vector<std::vector<std::string>> next_url_paths = {
            {"next"},                       // Stripe, Twitter
            {"next_url"},
            {"next_page"},
            {"_links", "next", "href"},   // HAL format
            {"pagination", "next"},
            {"paging", "next"},
            {"links", "next"},
            {"meta", "next_page_url"}     // Laravel
        };
        
        for (const auto& path : next_url_paths) {
            nlohmann::json current = json_body;
            bool found = true;
            
            for (const auto& segment : path) {
                if (current.contains(segment)) {
                    current = current[segment];
                } else {
                    found = false;
                    break;
                }
            }
            
            if (found && current.is_string()) {
                std::string next_url = current.get<std::string>();
                if (!next_url.empty() && next_url != "null") {
                    return next_url;
                }
            }
        }
        
        // Method 3: Check for page number indicators
        if (json_body.contains("page") && json_body.contains("total_pages")) {
            int current_page = json_body["page"];
            int total_pages = json_body["total_pages"];
            
            if (current_page < total_pages) {
                // Build next page URL by incrementing page parameter
                std::string current_url = api_config_.base_url + api_config_.endpoint_path;
                
                // Check if URL already has query parameters
                size_t query_pos = current_url.find('?');
                if (query_pos != std::string::npos) {
                    // Extract and modify existing page parameter
                    std::regex page_param(R"([?&]page=\d+)");
                    if (std::regex_search(current_url, page_param)) {
                        return std::regex_replace(current_url, std::regex(R"(page=\d+)"),
                                                "page=" + std::to_string(current_page + 1));
                    } else {
                        return current_url + "&page=" + std::to_string(current_page + 1);
                    }
                } else {
                    return current_url + "?page=" + std::to_string(current_page + 1);
                }
            }
        }
        
    } catch (const nlohmann::json::exception&) {
        // If JSON parsing fails, return empty string
    }
    
    // Method 4: Check for cursor-based pagination in headers
    auto next_cursor = response.headers.find("X-Next-Cursor");
    if (next_cursor != response.headers.end() && !next_cursor->second.empty()) {
        std::string base_url = api_config_.base_url + api_config_.endpoint_path;
        return base_url + (base_url.find('?') != std::string::npos ? "&" : "?") +
               "cursor=" + next_cursor->second;
    }
    
    // No next page found
    return "";
}

bool RESTAPISource::test_connection() {
    try {
        auto response = execute_request("GET", api_config_.base_url + "/health");
        return response.success && response.status_code == 200;
    } catch (const std::exception&) {
        return false;
    }
}

void RESTAPISource::refresh_auth_token() {
    // Simplified - in production would refresh OAuth/JWT tokens
    if (api_config_.auth_type == AuthenticationType::OAUTH2) {
        authenticate_oauth2();
    }
}

bool RESTAPISource::handle_rate_limit_exceeded(const HttpResponse& response) {
    // Extract retry-after header if available
    int retry_after_seconds = 60; // Default

    auto retry_header = response.headers.find("Retry-After");
    if (retry_header != response.headers.end()) {
        try {
            retry_after_seconds = std::stoi(retry_header->second);
        } catch (...) {
            // If parsing fails, use default
        }
    }

    logger_->log(LogLevel::WARN, "Rate limit exceeded, waiting " +
                std::to_string(retry_after_seconds) + " seconds before retry");

    std::this_thread::sleep_for(std::chrono::seconds(retry_after_seconds));
    return true;
}

// Helper methods for authentication and URL handling
std::string RESTAPISource::base64_encode(const std::string& input) {
    static const char encoding_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    int val = 0;
    int val_b = -6;

    for (unsigned char c : input) {
        val = (val << 8) + c;
        val_b += 8;
        while (val_b >= 0) {
            encoded.push_back(encoding_table[(val >> val_b) & 0x3F]);
            val_b -= 6;
        }
    }

    if (val_b > -6) {
        encoded.push_back(encoding_table[((val << 8) >> (val_b + 8)) & 0x3F]);
    }

    while (encoded.size() % 4) {
        encoded.push_back('=');
    }

    return encoded;
}

std::string RESTAPISource::url_encode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        // Keep alphanumeric and other safe characters intact
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            // Encode other characters
            escaped << '%' << std::setw(2) << int(static_cast<unsigned char>(c));
        }
    }

    return escaped.str();
}

nlohmann::json RESTAPISource::extract_json_path(const nlohmann::json& json_data, const std::string& path) {
    // Parse JSON path like "data.items[0].name"
    if (path.empty() || json_data.is_null()) {
        return nullptr;
    }

    nlohmann::json current = json_data;
    std::string current_path = path;

    // Split path by '.' and process each segment
    size_t pos = 0;
    while (pos < current_path.length()) {
        size_t next_dot = current_path.find('.', pos);
        std::string segment = (next_dot != std::string::npos) ?
            current_path.substr(pos, next_dot - pos) :
            current_path.substr(pos);

        // Check for array index: items[0]
        size_t bracket_pos = segment.find('[');
        if (bracket_pos != std::string::npos) {
            std::string key = segment.substr(0, bracket_pos);
            size_t close_bracket = segment.find(']');
            if (close_bracket != std::string::npos) {
                int index = std::stoi(segment.substr(bracket_pos + 1, close_bracket - bracket_pos - 1));

                if (current.contains(key) && current[key].is_array() &&
                    index >= 0 && index < static_cast<int>(current[key].size())) {
                    current = current[key][index];
                } else {
                    return nullptr;
                }
            }
        } else {
            // Simple key access
            if (current.contains(segment)) {
                current = current[segment];
            } else {
                return nullptr;
            }
        }

        pos = (next_dot != std::string::npos) ? next_dot + 1 : current_path.length();
    }

    return current;
}

} // namespace regulens

