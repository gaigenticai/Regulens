/**
 * API Version Router Implementation
 * Production-grade version-aware request routing
 */

#include "api_version_router.hpp"
#include <algorithm>
#include <regex>
#include <sstream>
#include <nlohmann/json.hpp>

namespace regulens {

APIVersionRouter& APIVersionRouter::get_instance() {
    static APIVersionRouter instance;
    return instance;
}

bool APIVersionRouter::initialize(std::shared_ptr<StructuredLogger> logger) {
    logger_ = logger;

    if (logger_) {
        logger_->info("API version router initialized successfully");
    }

    return true;
}

bool APIVersionRouter::register_route(const VersionedRoute& route) {
    std::lock_guard<std::mutex> lock(routes_mutex_);

    // Validate route
    if (route.version.empty() || route.method.empty() || route.path_pattern.empty() || !route.handler) {
        if (logger_) {
            logger_->error("Invalid route registration attempt - missing required fields");
        }
        return false;
    }

    // Add to version-based storage
    routes_by_version_[route.version].push_back(route);

    // Add to method+version based storage for faster lookup
    std::string method_version_key = route.method + ":" + route.version;
    routes_by_method_version_[method_version_key].push_back(route);

    if (logger_) {
        logger_->info("Registered route: " + route.method + " " + route.path_pattern +
                     " for version " + route.version);
    }

    return true;
}

bool APIVersionRouter::register_route(const std::string& version, const std::string& method,
                                    const std::string& path, RouteHandler handler,
                                    const std::vector<std::string>& permissions,
                                    bool requires_auth) {
    VersionedRoute route = {
        version,
        method,
        path,
        handler,
        permissions,
        requires_auth,
        std::nullopt
    };

    return register_route(route);
}

std::optional<HTTPResponse> APIVersionRouter::route_request(const HTTPRequest& request) {
    // Negotiate version for this request
    auto negotiation = negotiate_version_for_request(request);

    if (!negotiation.success) {
        return create_version_error_response("Unable to negotiate API version: " + negotiation.negotiated_version);
    }

    // Check for deprecation warnings
    if (negotiation.deprecation_notice) {
        // In production, you might want to add deprecation headers to the response
        if (logger_) {
            logger_->warn("Deprecated API version used: " + negotiation.negotiated_version);
        }
    }

    // Normalize the request path for routing
    std::string normalized_path = normalize_request_path(request);
    std::string resolved_path = resolve_handler_path(normalized_path, negotiation.negotiated_version);

    // Find the appropriate route
    auto route = find_route(request.method, resolved_path, negotiation.negotiated_version);

    if (!route) {
        // Try fallback to default version
        auto default_version = APIVersioningService::get_instance().get_default_version();
        if (default_version != negotiation.negotiated_version) {
            route = find_route(request.method, resolved_path, default_version);
            if (route) {
                // Create redirect response to correct version
                return create_version_redirect_response(negotiation.negotiated_version, default_version);
            }
        }

        return create_json_response(404, "Route not found: " + request.method + " " + request.path);
    }

    // Track route usage
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        std::string route_key = route->version + ":" + route->method + ":" + route->path_pattern;
        route_usage_stats_[route_key]++;
    }

    // Execute the handler
    try {
        HTTPResponse response = route->handler(request);

        // Add version headers to response
        auto version_headers = APIVersioningService::get_instance().generate_version_headers(
            negotiation.negotiated_version, negotiation);

        for (const auto& [key, value] : version_headers) {
            response.headers[key] = value;
        }

        // Add deprecation headers if needed
        if (negotiation.deprecation_notice) {
            response.headers["Warning"] = *negotiation.deprecation_notice;
        }

        return response;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Route handler exception: " + std::string(e.what()));
        }
        return create_json_response(500, "Internal server error during request processing");
    }
}

std::optional<VersionedRoute> APIVersionRouter::find_route(const std::string& method,
                                                        const std::string& path,
                                                        const std::string& version) {
    std::lock_guard<std::mutex> lock(routes_mutex_);

    std::string method_version_key = method + ":" + version;
    auto method_routes_it = routes_by_method_version_.find(method_version_key);

    if (method_routes_it != routes_by_method_version_.end()) {
        for (const auto& route : method_routes_it->second) {
            if (path_matches_pattern(path, route.path_pattern)) {
                return route;
            }
        }
    }

    return std::nullopt;
}

std::string APIVersionRouter::normalize_request_path(const HTTPRequest& request) {
    // Remove version prefix if present and normalize the path
    std::string path = request.path;

    // Remove query parameters
    size_t query_pos = path.find('?');
    if (query_pos != std::string::npos) {
        path = path.substr(0, query_pos);
    }

    // Normalize path (remove double slashes, trailing slashes, etc.)
    return normalize_path(path);
}

VersionNegotiationResult APIVersionRouter::negotiate_version_for_request(const HTTPRequest& request) {
    return APIVersioningService::get_instance().negotiate_version(
        request.path,
        request.headers,
        request.query_params
    );
}

std::string APIVersionRouter::resolve_handler_path(const std::string& request_path, const std::string& version) {
    // If the request path already includes version, remove it
    // If not, the path is already normalized for the resolved version
    if (is_versioned_route(request_path)) {
        return remove_version_from_path(request_path);
    }

    return request_path;
}

std::vector<VersionedRoute> APIVersionRouter::get_routes_for_version(const std::string& version) {
    std::lock_guard<std::mutex> lock(routes_mutex_);
    auto it = routes_by_version_.find(version);
    if (it != routes_by_version_.end()) {
        return it->second;
    }
    return {};
}

std::vector<std::string> APIVersionRouter::get_supported_versions() {
    return APIVersioningService::get_instance().get_supported_versions();
}

std::unordered_map<std::string, std::vector<VersionedRoute>> APIVersionRouter::get_all_routes() {
    std::lock_guard<std::mutex> lock(routes_mutex_);
    return routes_by_version_;
}

HTTPResponse APIVersionRouter::create_version_redirect_response(const std::string& requested_version,
                                                             const std::string& resolved_version) {
    std::string new_path = APIVersioningService::get_instance().build_versioned_path(
        remove_version_from_path("/api" + extract_version_from_path(requested_version) + "/"),
        resolved_version
    );

    HTTPResponse response = create_redirect_response(new_path, 302);
    response.headers["X-API-Version-Redirect"] = "Redirected from " + requested_version + " to " + resolved_version;

    return response;
}

HTTPResponse APIVersionRouter::create_deprecation_warning_response(const VersionNegotiationResult& negotiation) {
    nlohmann::json response_data = {
        {"message", "API version deprecated"},
        {"deprecated_version", negotiation.negotiated_version},
        {"recommended_version", APIVersioningService::get_instance().get_current_version()},
        {"deprecation_notice", negotiation.deprecation_notice.value_or("")},
        {"sunset_date", negotiation.sunset_date.value_or("")}
    };

    return create_json_response(200, "success", response_data);
}

HTTPResponse APIVersionRouter::create_version_error_response(const std::string& message) {
    nlohmann::json response_data = {
        {"error", "Version negotiation failed"},
        {"message", message},
        {"supported_versions", get_supported_versions()},
        {"current_version", APIVersioningService::get_instance().get_current_version()}
    };

    return create_json_response(400, "Version negotiation failed", response_data);
}

std::unordered_map<std::string, int> APIVersionRouter::get_route_usage_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return route_usage_stats_;
}

void APIVersionRouter::reset_usage_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    route_usage_stats_.clear();
}

// Private helper methods
bool APIVersionRouter::path_matches_pattern(const std::string& path, const std::string& pattern) {
    // Convert pattern to regex
    // Replace {param} with ([^/]+) for parameter matching
    std::string regex_pattern = pattern;
    regex_pattern = std::regex_replace(regex_pattern, std::regex(R"(\{[^}]+\})"), R"([^/]+)");

    // Add ^ and $ to match full path
    regex_pattern = "^" + regex_pattern + "$";

    std::regex route_regex(regex_pattern);
    return std::regex_match(path, route_regex);
}

std::string APIVersionRouter::normalize_path(const std::string& path) {
    std::string normalized = path;

    // Remove double slashes
    std::regex double_slash("//+");
    normalized = std::regex_replace(normalized, double_slash, "/");

    // Remove trailing slash (except for root path)
    if (normalized.length() > 1 && normalized.back() == '/') {
        normalized.pop_back();
    }

    return normalized;
}

std::unordered_map<std::string, std::string> APIVersionRouter::extract_path_parameters(
    const std::string& path, const std::string& pattern) {

    std::unordered_map<std::string, std::string> params;

    // Extract parameter names from pattern
    std::regex param_regex(R"(\{([^}]+)\})");
    std::sregex_iterator param_it(pattern.begin(), pattern.end(), param_regex);
    std::sregex_iterator param_end;

    std::vector<std::string> param_names;
    for (; param_it != param_end; ++param_it) {
        param_names.push_back(param_it->str(1));
    }

    // Convert pattern to regex and extract values
    std::string regex_pattern = pattern;
    regex_pattern = std::regex_replace(regex_pattern, std::regex(R"(\{[^}]+\})"), R"(([^/]+))");
    regex_pattern = "^" + regex_pattern + "$";

    std::regex route_regex(regex_pattern);
    std::smatch matches;

    if (std::regex_match(path, matches, route_regex)) {
        // matches[0] is the full match, parameters start from matches[1]
        for (size_t i = 1; i < matches.size() && i - 1 < param_names.size(); ++i) {
            params[param_names[i - 1]] = matches[i].str();
        }
    }

    return params;
}

std::string APIVersionRouter::extract_version_from_path(const std::string& path) {
    return APIVersioningService::get_instance().extract_version_from_path(path);
}

std::string APIVersionRouter::remove_version_from_path(const std::string& path) {
    return APIVersioningService::get_instance().normalize_path_for_version(path, "");
}

bool APIVersionRouter::is_versioned_route(const std::string& path) {
    return APIVersioningService::get_instance().is_versioned_path(path);
}

HTTPResponse APIVersionRouter::create_json_response(int status_code, const std::string& message,
                                                  const nlohmann::json& data) {
    HTTPResponse response;
    response.status_code = status_code;
    response.status_message = message;
    response.content_type = "application/json";

    nlohmann::json response_body = {
        {"status", message},
        {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
    };

    if (!data.is_null()) {
        response_body["data"] = data;
    }

    response.body = response_body.dump(2);
    return response;
}

HTTPResponse APIVersionRouter::create_redirect_response(const std::string& location, int status_code) {
    HTTPResponse response;
    response.status_code = status_code;
    response.status_message = "Found";
    response.headers["Location"] = location;
    response.content_type = "text/plain";
    response.body = "Redirecting to: " + location;

    return response;
}

} // namespace regulens
