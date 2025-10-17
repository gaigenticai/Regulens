/**
 * API Registry System - Systematic API Endpoint Registration
 * Production-grade API registration and management system
 * Implements modular endpoint discovery and registration
 */

#ifndef REGULENS_API_REGISTRY_HPP
#define REGULENS_API_REGISTRY_HPP

#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <vector>
#include <mutex>
#include <nlohmann/json.hpp>
#include <libpq-fe.h>

namespace regulens {

// Forward declarations
struct HTTPRequest;
struct HTTPResponse;
class StructuredLogger;

// API endpoint handler function type
using APIHandler = std::function<HTTPResponse(const HTTPRequest&, PGconn*)>;

// API parameter definition for OpenAPI
struct APIParameter {
    std::string name;
    std::string in; // "query", "header", "path", "cookie"
    std::string description;
    bool required;
    std::string type;
};

// API response definition for OpenAPI
struct APIResponse {
    std::string description;
    nlohmann::json schema;
};

// API endpoint metadata
struct APIEndpoint {
    std::string method;           // HTTP method (GET, POST, PUT, DELETE)
    std::string path;             // Endpoint path pattern
    std::string description;      // Human-readable description
    std::string category;         // API category (auth, transactions, etc.)
    bool requires_auth;          // Whether endpoint requires authentication
    std::vector<std::string> roles; // Required roles for access
    APIHandler handler;          // The actual handler function

    // OpenAPI-specific fields
    std::string summary;          // Short summary for OpenAPI
    std::string operation_id;     // Unique operation identifier
    std::vector<std::string> tags; // OpenAPI tags
    std::vector<APIParameter> parameters; // Endpoint parameters
    std::map<int, APIResponse> responses; // Response definitions
    std::vector<std::string> security_schemes; // Security schemes

    // Default constructor
    APIEndpoint() : requires_auth(false) {}

    APIEndpoint(const std::string& m, const std::string& p, const std::string& desc,
                const std::string& cat, bool auth = false, const std::vector<std::string>& r = {})
        : method(m), path(p), description(desc), category(cat), requires_auth(auth), roles(r) {}
};

// API registry configuration
struct APIRegistryConfig {
    bool enable_cors = true;
    bool enable_rate_limiting = true;
    bool enable_request_logging = true;
    bool enable_error_handling = true;
    std::string cors_allowed_origins = "*";
    int max_request_size_kb = 1024;
    int rate_limit_requests_per_minute = 60;
};

/**
 * Production-grade API Registry System
 * Manages systematic registration and routing of all API endpoints
 */
class APIRegistry {
public:
    // Singleton access
    static APIRegistry& get_instance();

    // Delete copy/move operations
    APIRegistry(const APIRegistry&) = delete;
    APIRegistry& operator=(const APIRegistry&) = delete;
    APIRegistry(APIRegistry&&) = delete;
    APIRegistry& operator=(APIRegistry&&) = delete;

    /**
     * Initialize the API registry with configuration
     */
    bool initialize(const APIRegistryConfig& config, std::shared_ptr<StructuredLogger> logger);

    /**
     * Register a single API endpoint
     */
    void register_endpoint(const APIEndpoint& endpoint);

    /**
     * Register multiple endpoints from a handler file
     */
    void register_endpoints_from_handler(const std::string& handler_name,
                                       const std::vector<APIEndpoint>& endpoints);

    /**
     * Register all endpoints from a specific category
     */
    void register_category_endpoints(const std::string& category,
                                   const std::vector<APIEndpoint>& endpoints);

    /**
     * Find and return handler for a given request
     */
    std::optional<APIEndpoint> find_handler(const std::string& method, const std::string& path);

    /**
     * Get all registered endpoints
     */
    std::vector<APIEndpoint> get_all_endpoints() const;

    /**
     * Get endpoints by category
     */
    std::vector<APIEndpoint> get_endpoints_by_category(const std::string& category) const;

    /**
     * Generate OpenAPI/Swagger specification
     */
    nlohmann::json generate_openapi_spec() const;

    /**
     * Validate all registered endpoints
     */
    bool validate_endpoints() const;

    /**
     * Get registry statistics
     */
    struct RegistryStats {
        size_t total_endpoints = 0;
        std::unordered_map<std::string, size_t> endpoints_by_category;
        std::unordered_map<std::string, size_t> endpoints_by_method;
        size_t authenticated_endpoints = 0;
    };
    RegistryStats get_stats() const;

private:
    APIRegistry() = default;
    ~APIRegistry() = default;

    // Parse path parameters (e.g., /users/{id} -> /users/123)
    bool match_path_pattern(const std::string& pattern, const std::string& path,
                          std::unordered_map<std::string, std::string>& params) const;

    // Storage
    std::unordered_map<std::string, std::vector<APIEndpoint>> endpoints_by_method_;
    std::unordered_map<std::string, APIEndpoint> endpoints_by_path_;
    std::vector<APIEndpoint> all_endpoints_;
    std::unordered_map<std::string, std::vector<APIEndpoint>> endpoints_by_category_;

    // Configuration and services
    APIRegistryConfig config_;
    std::shared_ptr<StructuredLogger> logger_;
    mutable std::mutex registry_mutex_;
    bool initialized_ = false;
};

/**
 * Helper function to create standardized API endpoint
 */
APIEndpoint create_endpoint(const std::string& method, const std::string& path,
                          const std::string& description, const std::string& category,
                          APIHandler handler, bool requires_auth = false,
                          const std::vector<std::string>& roles = {});

/**
 * Macro to register endpoints in a handler file
 */
#define REGISTER_API_ENDPOINTS(category_name, endpoints_list) \
    namespace { \
        bool register_##category_name##_endpoints() { \
            auto& registry = regulens::APIRegistry::get_instance(); \
            registry.register_endpoints_from_handler(#category_name, endpoints_list); \
            return true; \
        } \
        static bool category_name##_registered = register_##category_name##_endpoints(); \
    }

} // namespace regulens

#endif // REGULENS_API_REGISTRY_HPP
