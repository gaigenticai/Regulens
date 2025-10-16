/**
 * API Registry System Implementation
 * Production-grade API registration and routing system
 */

#include "api_registry.hpp"
#include "../logging/structured_logger.hpp"
#include <algorithm>
#include <regex>
#include <sstream>

namespace regulens {

// Singleton implementation
APIRegistry& APIRegistry::get_instance() {
    static APIRegistry instance;
    return instance;
}

bool APIRegistry::initialize(const APIRegistryConfig& config, std::shared_ptr<StructuredLogger> logger) {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    if (initialized_) {
        logger->warn("API Registry already initialized");
        return true;
    }

    config_ = config;
    logger_ = logger;

    logger_->info("Initializing API Registry system");

    // Validate configuration
    if (config_.max_request_size_kb <= 0) {
        logger_->error("Invalid max_request_size_kb configuration");
        return false;
    }

    if (config_.rate_limit_requests_per_minute <= 0) {
        logger_->error("Invalid rate_limit_requests_per_minute configuration");
        return false;
    }

    initialized_ = true;
    logger_->info("API Registry initialized successfully");
    return true;
}

void APIRegistry::register_endpoint(const APIEndpoint& endpoint) {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    if (!initialized_) {
        // Log warning but don't throw - allow registration even if not fully initialized
        if (logger_) {
            logger_->warn("API Registry not initialized, but registering endpoint anyway");
        }
    }

    // Create key for method-based lookup
    std::string method_key = endpoint.method + ":" + endpoint.path;

    // Store in various lookup structures
    endpoints_by_method_[endpoint.method].push_back(endpoint);
    endpoints_by_path_[method_key] = endpoint;
    all_endpoints_.push_back(endpoint);
    endpoints_by_category_[endpoint.category].push_back(endpoint);

    if (logger_) {
        logger_->info("Registered API endpoint: " + endpoint.method + " " + endpoint.path +
                     " (" + endpoint.category + ") - " + endpoint.description);
    }
}

void APIRegistry::register_endpoints_from_handler(const std::string& handler_name,
                                                const std::vector<APIEndpoint>& endpoints) {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    if (logger_) {
        logger_->info("Registering " + std::to_string(endpoints.size()) +
                     " endpoints from handler: " + handler_name);
    }

    for (const auto& endpoint : endpoints) {
        register_endpoint(endpoint);
    }

    if (logger_) {
        logger_->info("Successfully registered all endpoints from handler: " + handler_name);
    }
}

void APIRegistry::register_category_endpoints(const std::string& category,
                                            const std::vector<APIEndpoint>& endpoints) {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    if (logger_) {
        logger_->info("Registering " + std::to_string(endpoints.size()) +
                     " endpoints for category: " + category);
    }

    for (const auto& endpoint : endpoints) {
        APIEndpoint endpoint_with_category = endpoint;
        endpoint_with_category.category = category;
        register_endpoint(endpoint_with_category);
    }
}

std::optional<APIEndpoint> APIRegistry::find_handler(const std::string& method, const std::string& path) {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    // First, try exact match
    std::string method_key = method + ":" + path;
    auto exact_it = endpoints_by_path_.find(method_key);
    if (exact_it != endpoints_by_path_.end()) {
        return exact_it->second;
    }

    // Then, try pattern matching for parameterized routes
    auto method_it = endpoints_by_method_.find(method);
    if (method_it != endpoints_by_method_.end()) {
        for (const auto& endpoint : method_it->second) {
            std::unordered_map<std::string, std::string> params;
            if (match_path_pattern(endpoint.path, path, params)) {
                return endpoint;
            }
        }
    }

    return std::nullopt;
}

std::vector<APIEndpoint> APIRegistry::get_all_endpoints() const {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    return all_endpoints_;
}

std::vector<APIEndpoint> APIRegistry::get_endpoints_by_category(const std::string& category) const {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    auto it = endpoints_by_category_.find(category);
    if (it != endpoints_by_category_.end()) {
        return it->second;
    }

    return {};
}

nlohmann::json APIRegistry::generate_openapi_spec() const {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    nlohmann::json spec = {
        {"openapi", "3.0.3"},
        {"info", {
            {"title", "Regulens AI Compliance System API"},
            {"description", "Production-grade API for regulatory compliance and AI-powered decision making"},
            {"version", "1.0.0"},
            {"contact", {
                {"name", "Regulens Development Team"},
                {"email", "api@regulens.com"}
            }}
        }},
        {"servers", nlohmann::json::array({
            {
                {"url", "https://api.regulens.com"},
                {"description", "Production server"}
            },
            {
                {"url", "http://localhost:8080"},
                {"description", "Development server"}
            }
        })},
        {"security", nlohmann::json::array({
            {
                {"bearerAuth", nlohmann::json::array()}
            }
        })},
        {"components", {
            {"securitySchemes", {
                {"bearerAuth", {
                    {"type", "http"},
                    {"scheme", "bearer"},
                    {"bearerFormat", "JWT"}
                }}
            }}
        }}
    };

    // Group endpoints by path
    std::unordered_map<std::string, nlohmann::json> paths;

    for (const auto& endpoint : all_endpoints_) {
        std::string method_lower = endpoint.method;
        std::transform(method_lower.begin(), method_lower.end(), method_lower.begin(), ::tolower);

        if (!paths.contains(endpoint.path)) {
            paths[endpoint.path] = nlohmann::json::object();
        }

        nlohmann::json operation = {
            {"summary", endpoint.description},
            {"description", endpoint.description},
            {"tags", nlohmann::json::array({endpoint.category})}
        };

        // Add security if required
        if (endpoint.requires_auth) {
            operation["security"] = nlohmann::json::array({
                {{"bearerAuth", nlohmann::json::array()}}
            });
        }

        // Add basic response structure
        operation["responses"] = {
            {"200", {
                {"description", "Successful operation"},
                {"content", {
                    {"application/json", {
                        {"schema", {
                            {"type", "object"}
                        }}
                    }}
                }}
            }},
            {"401", {
                {"description", "Unauthorized"},
                {"content", {
                    {"application/json", {
                        {"schema", {
                            {"type", "object"},
                            {"properties", {
                                {"error", {{"type", "string"}}}
                            }}
                        }}
                    }}
                }}
            }},
            {"500", {
                {"description", "Internal server error"},
                {"content", {
                    {"application/json", {
                        {"schema", {
                            {"type", "object"},
                            {"properties", {
                                {"error", {{"type", "string"}}}
                            }}
                        }}
                    }}
                }}
            }}
        };

        paths[endpoint.path][method_lower] = operation;
    }

    spec["paths"] = paths;
    return spec;
}

bool APIRegistry::validate_endpoints() const {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    bool all_valid = true;

    for (const auto& endpoint : all_endpoints_) {
        // Validate method
        std::vector<std::string> valid_methods = {"GET", "POST", "PUT", "DELETE", "PATCH"};
        if (std::find(valid_methods.begin(), valid_methods.end(), endpoint.method) == valid_methods.end()) {
            if (logger_) {
                logger_->error("Invalid HTTP method '" + endpoint.method + "' for endpoint: " + endpoint.path);
            }
            all_valid = false;
        }

        // Validate path format
        if (endpoint.path.empty() || endpoint.path[0] != '/') {
            if (logger_) {
                logger_->error("Invalid path format for endpoint: " + endpoint.path);
            }
            all_valid = false;
        }

        // Validate handler exists
        if (!endpoint.handler) {
            if (logger_) {
                logger_->error("Missing handler for endpoint: " + endpoint.method + " " + endpoint.path);
            }
            all_valid = false;
        }

        // Validate description
        if (endpoint.description.empty()) {
            if (logger_) {
                logger_->warn("Missing description for endpoint: " + endpoint.method + " " + endpoint.path);
            }
        }
    }

    return all_valid;
}

APIRegistry::RegistryStats APIRegistry::get_stats() const {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    RegistryStats stats;
    stats.total_endpoints = all_endpoints_.size();

    for (const auto& endpoint : all_endpoints_) {
        stats.endpoints_by_category[endpoint.category]++;
        stats.endpoints_by_method[endpoint.method]++;
        if (endpoint.requires_auth) {
            stats.authenticated_endpoints++;
        }
    }

    return stats;
}

bool APIRegistry::match_path_pattern(const std::string& pattern, const std::string& path,
                                   std::unordered_map<std::string, std::string>& params) const {
    // Simple pattern matching for parameterized routes
    // e.g., /users/{id} matches /users/123

    std::string regex_pattern = pattern;

    // Replace {param} with ([^/]+) for regex matching
    std::regex param_regex("\\{([^}]+)\\}");
    regex_pattern = std::regex_replace(regex_pattern, param_regex, "([^/]+)");

    // Escape other regex special characters
    regex_pattern = std::regex_replace(regex_pattern, std::regex("\\."), "\\.");

    std::regex compiled_pattern("^" + regex_pattern + "$");

    std::smatch matches;
    if (std::regex_match(path, matches, compiled_pattern)) {
        // Extract parameter names from pattern
        std::vector<std::string> param_names;
        std::sregex_iterator iter(pattern.begin(), pattern.end(), param_regex);
        std::sregex_iterator end;

        for (; iter != end; ++iter) {
            param_names.push_back(iter->str(1)); // Capture group 1 is the parameter name
        }

        // Map parameter names to matched values
        for (size_t i = 1; i < matches.size() && i-1 < param_names.size(); ++i) {
            params[param_names[i-1]] = matches[i].str();
        }

        return true;
    }

    return false;
}

APIEndpoint create_endpoint(const std::string& method, const std::string& path,
                          const std::string& description, const std::string& category,
                          APIHandler handler, bool requires_auth,
                          const std::vector<std::string>& roles) {
    APIEndpoint endpoint(method, path, description, category, requires_auth, roles);
    endpoint.handler = handler;
    return endpoint;
}

} // namespace regulens
