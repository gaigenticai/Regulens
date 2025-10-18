/**
 * API Endpoint Configuration Manager
 * Production-grade centralized configuration for all API endpoints
 * Loads from JSON config and provides programmatic access
 */

#ifndef API_ENDPOINT_CONFIG_HPP
#define API_ENDPOINT_CONFIG_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>
#include "../error_handler.hpp"
#include "../logging/structured_logger.hpp"
#include "http_method_validator.hpp"

namespace regulens {

struct APIEndpointInfo {
    std::string method;
    std::string path;
    std::string description;
    std::string category;
    bool requires_auth;
    std::vector<std::string> permissions;
    std::string full_path; // Includes base_url prefix
};

struct APICategoryInfo {
    std::string description;
    std::string priority;
};

class APIEndpointConfig {
public:
    static APIEndpointConfig& get_instance();

    // Initialize with config file path
    bool initialize(const std::string& config_path,
                   std::shared_ptr<StructuredLogger> logger);

    // Get endpoint information
    std::optional<APIEndpointInfo> get_endpoint(const std::string& category,
                                               const std::string& endpoint) const;

    // Get all endpoints in a category
    std::vector<APIEndpointInfo> get_category_endpoints(const std::string& category) const;

    // Get all endpoints
    std::unordered_map<std::string, std::vector<APIEndpointInfo>> get_all_endpoints() const;

    // Get category information
    std::optional<APICategoryInfo> get_category_info(const std::string& category) const;

    // Get all categories
    std::unordered_map<std::string, APICategoryInfo> get_all_categories() const;

    // Validate endpoint exists
    bool endpoint_exists(const std::string& category, const std::string& endpoint) const;

    // Get base URL
    std::string get_base_url() const { return base_url_; }

    // Get version
    std::string get_version() const { return version_; }

    // Validate configuration
    bool validate_config() const;

    // Validate HTTP method usage
    std::vector<HTTPValidationResult> validate_http_methods() const;

    // Get permission description
    std::string get_permission_description(const std::string& permission) const;

private:
    APIEndpointConfig() = default;
    ~APIEndpointConfig() = default;
    APIEndpointConfig(const APIEndpointConfig&) = delete;
    APIEndpointConfig& operator=(const APIEndpointConfig&) = delete;

    bool load_config(const std::string& config_path);
    void build_endpoint_map();
    void build_category_map();
    void build_permission_map();

    std::shared_ptr<StructuredLogger> logger_;
    nlohmann::json config_;
    std::string base_url_;
    std::string version_;
    std::string config_path_;

    // Cached data structures
    std::unordered_map<std::string, std::unordered_map<std::string, APIEndpointInfo>> endpoints_;
    std::unordered_map<std::string, APICategoryInfo> categories_;
    std::unordered_map<std::string, std::string> permissions_;

    // Validation methods
    bool validate_endpoint_structure(const nlohmann::json& endpoint) const;
    bool validate_category_structure(const nlohmann::json& category) const;
    bool validate_permission_structure(const nlohmann::json& permission) const;
    bool validate_paths_unique() const;
    bool validate_permissions_exist() const;
    bool validate_naming_conventions() const;
    bool validate_path_format(const std::string& path) const;
    bool has_action_verb_in_path(const std::string& path) const;
    bool validate_parameter_naming(const std::string& path) const;
};

// Helper functions for common operations
std::string build_full_path(const std::string& base_url, const std::string& path);
bool is_valid_http_method(const std::string& method);
bool is_valid_path_pattern(const std::string& path);
std::vector<std::string> extract_path_parameters(const std::string& path);

} // namespace regulens

#endif // API_ENDPOINT_CONFIG_HPP
