/**
 * API Endpoint Configuration Manager Implementation
 * Production-grade centralized configuration management
 */

#include "api_endpoint_config.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <regex>

namespace regulens {

APIEndpointConfig& APIEndpointConfig::get_instance() {
    static APIEndpointConfig instance;
    return instance;
}

bool APIEndpointConfig::initialize(const std::string& config_path,
                                  std::shared_ptr<StructuredLogger> logger) {
    logger_ = logger;
    config_path_ = config_path;

    if (!load_config(config_path)) {
        if (logger_) {
            logger_->error("Failed to load API endpoint configuration from: " + config_path);
        }
        return false;
    }

    build_endpoint_map();
    build_category_map();
    build_permission_map();

    if (!validate_config()) {
        if (logger_) {
            logger_->error("API endpoint configuration validation failed");
        }
        return false;
    }

    if (logger_) {
        logger_->info("API endpoint configuration loaded successfully. Version: " + version_ +
                     ", Base URL: " + base_url_ +
                     ", Categories: " + std::to_string(categories_.size()));
    }

    return true;
}

bool APIEndpointConfig::load_config(const std::string& config_path) {
    try {
        std::ifstream file(config_path);
        if (!file.is_open()) {
            if (logger_) {
                logger_->error("Cannot open config file: " + config_path);
            }
            return false;
        }

        config_ = nlohmann::json::parse(file);

        // Extract basic configuration
        version_ = config_.value("version", "1.0.0");
        base_url_ = config_.value("base_url", "/api");

        return true;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Error loading config: " + std::string(e.what()));
        }
        return false;
    }
}

void APIEndpointConfig::build_endpoint_map() {
    if (!config_.contains("endpoints")) {
        return;
    }

    for (const auto& [category_name, category_data] : config_["endpoints"].items()) {
        std::unordered_map<std::string, APIEndpointInfo> category_endpoints;

        for (const auto& [endpoint_name, endpoint_data] : category_data.items()) {
            APIEndpointInfo info;
            info.method = endpoint_data.value("method", "");
            info.path = endpoint_data.value("path", "");
            info.description = endpoint_data.value("description", "");
            info.category = endpoint_data.value("category", category_name);
            info.requires_auth = endpoint_data.value("requires_auth", true);

            if (endpoint_data.contains("permissions")) {
                for (const auto& perm : endpoint_data["permissions"]) {
                    info.permissions.push_back(perm);
                }
            }

            info.full_path = build_full_path(base_url_, info.path);
            category_endpoints[endpoint_name] = info;
        }

        endpoints_[category_name] = category_endpoints;
    }
}

void APIEndpointConfig::build_category_map() {
    if (!config_.contains("categories")) {
        return;
    }

    for (const auto& [category_name, category_data] : config_["categories"].items()) {
        APICategoryInfo info;
        info.description = category_data.value("description", "");
        info.priority = category_data.value("priority", "medium");
        categories_[category_name] = info;
    }
}

void APIEndpointConfig::build_permission_map() {
    if (!config_.contains("permissions")) {
        return;
    }

    for (const auto& [permission_name, permission_data] : config_["permissions"].items()) {
        permissions_[permission_name] = permission_data;
    }
}

std::optional<APIEndpointInfo> APIEndpointConfig::get_endpoint(
    const std::string& category, const std::string& endpoint) const {

    auto category_it = endpoints_.find(category);
    if (category_it == endpoints_.end()) {
        return std::nullopt;
    }

    auto endpoint_it = category_it->second.find(endpoint);
    if (endpoint_it == category_it->second.end()) {
        return std::nullopt;
    }

    return endpoint_it->second;
}

std::vector<APIEndpointInfo> APIEndpointConfig::get_category_endpoints(
    const std::string& category) const {

    std::vector<APIEndpointInfo> result;

    auto category_it = endpoints_.find(category);
    if (category_it != endpoints_.end()) {
        for (const auto& [name, info] : category_it->second) {
            result.push_back(info);
        }
    }

    return result;
}

std::unordered_map<std::string, std::vector<APIEndpointInfo>>
APIEndpointConfig::get_all_endpoints() const {

    std::unordered_map<std::string, std::vector<APIEndpointInfo>> result;

    for (const auto& [category_name, category_endpoints] : endpoints_) {
        std::vector<APIEndpointInfo> category_list;
        for (const auto& [endpoint_name, info] : category_endpoints) {
            category_list.push_back(info);
        }
        result[category_name] = category_list;
    }

    return result;
}

std::optional<APICategoryInfo> APIEndpointConfig::get_category_info(
    const std::string& category) const {

    auto it = categories_.find(category);
    if (it != categories_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::unordered_map<std::string, APICategoryInfo>
APIEndpointConfig::get_all_categories() const {
    return categories_;
}

bool APIEndpointConfig::endpoint_exists(const std::string& category,
                                       const std::string& endpoint) const {
    return get_endpoint(category, endpoint).has_value();
}

bool APIEndpointConfig::validate_config() const {
    bool is_valid = true;

    // Validate basic structure
    if (!config_.contains("endpoints")) {
        if (logger_) logger_->error("Config missing 'endpoints' section");
        is_valid = false;
    }

    if (!config_.contains("categories")) {
        if (logger_) logger_->warn("Config missing 'categories' section");
    }

    if (!config_.contains("permissions")) {
        if (logger_) logger_->warn("Config missing 'permissions' section");
    }

    // Validate endpoints
    for (const auto& [category_name, category_data] : config_["endpoints"].items()) {
        for (const auto& [endpoint_name, endpoint_data] : category_data.items()) {
            if (!validate_endpoint_structure(endpoint_data)) {
                if (logger_) {
                    logger_->error("Invalid endpoint structure: " + category_name + "." + endpoint_name);
                }
                is_valid = false;
            }
        }
    }

    // Validate categories
    if (config_.contains("categories")) {
        for (const auto& [category_name, category_data] : config_["categories"].items()) {
            if (!validate_category_structure(category_data)) {
                if (logger_) {
                    logger_->error("Invalid category structure: " + category_name);
                }
                is_valid = false;
            }
        }
    }

    // Validate permissions
    if (config_.contains("permissions")) {
        for (const auto& [permission_name, permission_data] : config_["permissions"].items()) {
            if (!validate_permission_structure(permission_data)) {
                if (logger_) {
                    logger_->error("Invalid permission structure: " + permission_name);
                }
                is_valid = false;
            }
        }
    }

    // Cross-validation
    if (!validate_paths_unique()) {
        is_valid = false;
    }

    if (!validate_permissions_exist()) {
        is_valid = false;
    }

    if (!validate_naming_conventions()) {
        is_valid = false;
    }

    return is_valid;
}

std::vector<ValidationResult> APIEndpointConfig::validate_http_methods() const {
    // Initialize HTTP method validator if not already done
    static bool validator_initialized = false;
    static HTTPMethodValidator* validator_instance = nullptr;

    if (!validator_initialized) {
        std::string method_config_path = config_path_;
        // Replace api_endpoints_config.json with http_method_mapping.json
        size_t pos = method_config_path.find("api_endpoints_config.json");
        if (pos != std::string::npos) {
            method_config_path.replace(pos, std::strlen("api_endpoints_config.json"), "http_method_mapping.json");
        }

        validator_instance = &HTTPMethodValidator::get_instance();
        if (!validator_instance->initialize(method_config_path, logger_)) {
            if (logger_) {
                logger_->warn("HTTP method validator initialization failed, skipping method validation");
            }
            return {};
        }
        validator_initialized = true;
    }

    return validator_instance->validate_api_endpoints(config_["endpoints"]);
}

bool APIEndpointConfig::validate_endpoint_structure(const nlohmann::json& endpoint) const {
    if (!endpoint.contains("method") || !endpoint["method"].is_string()) {
        return false;
    }

    if (!endpoint.contains("path") || !endpoint["path"].is_string()) {
        return false;
    }

    if (!endpoint.contains("description") || !endpoint["description"].is_string()) {
        return false;
    }

    std::string method = endpoint["method"];
    if (!is_valid_http_method(method)) {
        return false;
    }

    std::string path = endpoint["path"];
    if (!is_valid_path_pattern(path)) {
        return false;
    }

    return true;
}

bool APIEndpointConfig::validate_category_structure(const nlohmann::json& category) const {
    if (!category.contains("description") || !category["description"].is_string()) {
        return false;
    }

    if (category.contains("priority")) {
        std::string priority = category["priority"];
        if (priority != "low" && priority != "medium" && priority != "high" && priority != "critical") {
            return false;
        }
    }

    return true;
}

bool APIEndpointConfig::validate_permission_structure(const nlohmann::json& permission) const {
    return permission.is_string();
}

bool APIEndpointConfig::validate_paths_unique() const {
    std::unordered_set<std::string> paths;

    for (const auto& [category_name, category_endpoints] : endpoints_) {
        for (const auto& [endpoint_name, info] : category_endpoints) {
            if (paths.find(info.full_path) != paths.end()) {
                if (logger_) {
                    logger_->error("Duplicate path found: " + info.full_path);
                }
                return false;
            }
            paths.insert(info.full_path);
        }
    }

    return true;
}

bool APIEndpointConfig::validate_naming_conventions() const {
    bool is_valid = true;

    for (const auto& [category_name, category_endpoints] : endpoints_) {
        for (const auto& [endpoint_name, info] : category_endpoints) {
            // Check path format
            if (!validate_path_format(info.path)) {
                if (logger_) {
                    logger_->error("Invalid path format: " + info.path + " (should start with / and use kebab-case)");
                }
                is_valid = false;
            }

            // Check for action verbs in paths (should rely on HTTP methods instead)
            if (has_action_verb_in_path(info.path)) {
                if (logger_) {
                    logger_->warn("Action verb detected in path: " + info.path + " (consider using HTTP method instead)");
                }
                // Warning only, not a failure
            }

            // Check parameter naming
            if (!validate_parameter_naming(info.path)) {
                if (logger_) {
                    logger_->error("Invalid parameter naming in path: " + info.path + " (use {param} format)");
                }
                is_valid = false;
            }
        }
    }

    return is_valid;
}

bool APIEndpointConfig::validate_path_format(const std::string& path) const {
    // Must start with /
    if (path.empty() || path[0] != '/') {
        return false;
    }

    // Should use kebab-case (no camelCase or snake_case for multi-word)
    // Allow alphanumeric, hyphens, slashes, and curly braces for parameters
    std::regex valid_path_regex(R"(^\/[a-z0-9\/\-\{\}]*$)");

    return std::regex_match(path, valid_path_regex);
}

bool APIEndpointConfig::has_action_verb_in_path(const std::string& path) const {
    // Common action verbs that should be avoided in REST paths
    static const std::vector<std::string> action_verbs = {
        "create", "update", "delete", "add", "remove", "get", "set", "do", "make", "run", "execute", "perform"
    };

    std::string lower_path = path;
    std::transform(lower_path.begin(), lower_path.end(), lower_path.begin(), ::tolower);

    for (const auto& verb : action_verbs) {
        if (lower_path.find("/" + verb) != std::string::npos) {
            return true;
        }
    }

    return false;
}

bool APIEndpointConfig::validate_parameter_naming(const std::string& path) const {
    // Check that parameters use {param} format
    std::regex param_regex(R"(\{[^}]+\})");
    std::smatch matches;

    std::string::const_iterator search_start(path.cbegin());
    while (std::regex_search(search_start, path.cend(), matches, param_regex)) {
        std::string param = matches[0];
        // Ensure parameter name follows conventions (lowercase, underscores allowed)
        std::regex valid_param_regex(R"(\{[a-z_][a-z0-9_]*\})");

        if (!std::regex_match(param, valid_param_regex)) {
            return false;
        }

        search_start = matches.suffix().first;
    }

    return true;
}

bool APIEndpointConfig::validate_permissions_exist() const {
    for (const auto& [category_name, category_endpoints] : endpoints_) {
        for (const auto& [endpoint_name, info] : category_endpoints) {
            for (const auto& permission : info.permissions) {
                if (permissions_.find(permission) == permissions_.end()) {
                    if (logger_) {
                        logger_->warn("Undefined permission used: " + permission +
                                    " in " + category_name + "." + endpoint_name);
                    }
                    // Don't fail validation for undefined permissions, just warn
                }
            }
        }
    }

    return true;
}

std::string APIEndpointConfig::get_permission_description(const std::string& permission) const {
    auto it = permissions_.find(permission);
    if (it != permissions_.end()) {
        return it->second;
    }
    return "Unknown permission";
}

// Helper function implementations
std::string build_full_path(const std::string& base_url, const std::string& path) {
    if (base_url.empty()) return path;
    if (path.empty()) return base_url;

    // Ensure proper joining
    if (base_url.back() == '/' && path.front() == '/') {
        return base_url + path.substr(1);
    } else if (base_url.back() != '/' && path.front() != '/') {
        return base_url + "/" + path;
    } else {
        return base_url + path;
    }
}

bool is_valid_http_method(const std::string& method) {
    static const std::vector<std::string> valid_methods = {
        "GET", "POST", "PUT", "DELETE", "PATCH", "HEAD", "OPTIONS"
    };

    return std::find(valid_methods.begin(), valid_methods.end(),
                    method) != valid_methods.end();
}

bool is_valid_path_pattern(const std::string& path) {
    // Basic path validation - should start with /
    if (path.empty() || path[0] != '/') {
        return false;
    }

    // Check for valid path characters and parameter patterns
    std::regex path_regex(R"(^\/[a-zA-Z0-9\/_\-\{\}]*$)");

    return std::regex_match(path, path_regex);
}

std::vector<std::string> extract_path_parameters(const std::string& path) {
    std::vector<std::string> params;
    std::regex param_regex(R"(\{([^}]+)\})");
    std::smatch matches;

    std::string::const_iterator search_start(path.cbegin());
    while (std::regex_search(search_start, path.cend(), matches, param_regex)) {
        params.push_back(matches[1]);
        search_start = matches.suffix().first;
    }

    return params;
}

} // namespace regulens
