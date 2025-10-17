/**
 * API Version Router
 * Production-grade version-aware request routing with compatibility handling
 */

#ifndef API_VERSION_ROUTER_HPP
#define API_VERSION_ROUTER_HPP

#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <mutex>
#include "api_versioning_service.hpp"
#include "../api_registry/api_registry.hpp"
#include "../../shared/error_handler.hpp"
#include "../logging/structured_logger.hpp"
#include "../web_ui/web_ui_server.hpp"

namespace regulens {

// HTTPRequest and HTTPResponse are defined in api_registry.hpp

using RouteHandler = std::function<HTTPResponse(const HTTPRequest&)>;

struct VersionedRoute {
    std::string version;
    std::string method;
    std::string path_pattern;
    RouteHandler handler;
    std::vector<std::string> required_permissions;
    bool requires_authentication;
    std::optional<std::string> deprecated_message;
};

class APIVersionRouter {
public:
    static APIVersionRouter& get_instance();

    // Initialize with versioning service
    bool initialize(std::shared_ptr<StructuredLogger> logger);

    // Route registration methods
    bool register_route(const VersionedRoute& route);
    bool register_route(const std::string& version, const std::string& method,
                       const std::string& path, RouteHandler handler,
                       const std::vector<std::string>& permissions = {},
                       bool requires_auth = false);

    // Request routing methods
    std::optional<HTTPResponse> route_request(const HTTPRequest& request);
    std::optional<VersionedRoute> find_route(const std::string& method,
                                           const std::string& path,
                                           const std::string& version);

    // Version-aware routing helpers
    std::string normalize_request_path(const HTTPRequest& request);
    VersionNegotiationResult negotiate_version_for_request(const HTTPRequest& request);
    std::string resolve_handler_path(const std::string& request_path, const std::string& version);

    // Route management methods
    std::vector<VersionedRoute> get_routes_for_version(const std::string& version);
    std::vector<std::string> get_supported_versions();
    std::unordered_map<std::string, std::vector<VersionedRoute>> get_all_routes();

    // Compatibility and migration helpers
    HTTPResponse create_version_redirect_response(const std::string& requested_version,
                                                const std::string& resolved_version);
    HTTPResponse create_deprecation_warning_response(const VersionNegotiationResult& negotiation);
    HTTPResponse create_version_error_response(const std::string& message);

    // Statistics and monitoring
    std::unordered_map<std::string, int> get_route_usage_stats();
    void reset_usage_stats();

private:
    APIVersionRouter() = default;
    ~APIVersionRouter() = default;
    APIVersionRouter(const APIVersionRouter&) = delete;
    APIVersionRouter& operator=(const APIVersionRouter&) = delete;

    // Route storage and indexing
    std::unordered_map<std::string, std::vector<VersionedRoute>> routes_by_version_;
    std::unordered_map<std::string, std::vector<VersionedRoute>> routes_by_method_version_;
    std::mutex routes_mutex_;

    // Path matching and normalization
    bool path_matches_pattern(const std::string& path, const std::string& pattern);
    std::string normalize_path(const std::string& path);
    std::unordered_map<std::string, std::string> extract_path_parameters(
        const std::string& path, const std::string& pattern);

    // Version handling
    std::string extract_version_from_path(const std::string& path);
    std::string remove_version_from_path(const std::string& path);
    bool is_versioned_route(const std::string& path);

    // Response creation helpers
    HTTPResponse create_json_response(int status_code, const std::string& message,
                                    const nlohmann::json& data = nullptr);
    HTTPResponse create_redirect_response(const std::string& location, int status_code = 302);

    // Statistics tracking
    std::unordered_map<std::string, int> route_usage_stats_;
    std::mutex stats_mutex_;

    std::shared_ptr<StructuredLogger> logger_;
};

} // namespace regulens

#endif // API_VERSION_ROUTER_HPP
