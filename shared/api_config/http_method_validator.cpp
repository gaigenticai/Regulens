/**
 * HTTP Method Validator Implementation
 * Production-grade validation of HTTP method usage
 */

#include "http_method_validator.hpp"
#include <algorithm>
#include <regex>
#include <sstream>
#include <fstream>

namespace regulens {

HTTPMethodValidator& HTTPMethodValidator::get_instance() {
    static HTTPMethodValidator instance;
    return instance;
}

bool HTTPMethodValidator::initialize(const std::string& config_path,
                                   std::shared_ptr<StructuredLogger> logger) {
    logger_ = logger;
    config_path_ = config_path;

    if (!load_config(config_path)) {
        if (logger_) {
            logger_->error("Failed to load HTTP method mapping configuration from: " + config_path);
        }
        return false;
    }

    build_method_rules();
    build_operation_mapping();

    if (logger_) {
        logger_->info("HTTP method validator initialized successfully. Methods configured: " +
                     std::to_string(method_rules_.size()));
    }

    return true;
}

bool HTTPMethodValidator::load_config(const std::string& config_path) {
    try {
        std::ifstream file(config_path);
        if (!file.is_open()) {
            if (logger_) {
                logger_->error("Cannot open HTTP method config file: " + config_path);
            }
            return false;
        }

        config_ = nlohmann::json::parse(file);
        return true;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Error loading HTTP method config: " + std::string(e.what()));
        }
        return false;
    }
}

void HTTPMethodValidator::build_method_rules() {
    if (!config_.contains("method_guidelines")) {
        return;
    }

    for (const auto& [method_name, method_config] : config_["method_guidelines"].items()) {
        HTTPMethodRules rules;

        rules.safe = method_config.value("safe", false);
        rules.idempotent = method_config.value("idempotent", false);
        rules.cacheable = method_config.value("cacheable", false);

        // Method-specific rules
        if (config_.contains("method_validation_rules") &&
            config_["method_validation_rules"].contains(method_name)) {

            const auto& validation = config_["method_validation_rules"][method_name];
            rules.body_allowed = validation.value("body_allowed", false);
            rules.query_parameters_allowed = validation.value("query_parameters_allowed", false);
            rules.path_parameters_allowed = validation.value("path_parameters_allowed", false);

            if (validation.contains("headers_required")) {
                for (const auto& header : validation["headers_required"]) {
                    rules.headers_required.push_back(header);
                }
            }

            if (validation.contains("content_types")) {
                for (const auto& content_type : validation["content_types"]) {
                    rules.content_types_allowed.push_back(content_type);
                }
            }
        }

        method_rules_[method_name] = rules;
    }
}

void HTTPMethodValidator::build_operation_mapping() {
    if (!config_.contains("resource_operation_mapping")) {
        return;
    }

    const auto& mapping = config_["resource_operation_mapping"];

    // Collection operations
    if (mapping.contains("collection_operations")) {
        for (const auto& [operation, method] : mapping["collection_operations"].items()) {
            operation_to_method_mapping_[operation] = method;
        }
    }

    // Item operations
    if (mapping.contains("item_operations")) {
        for (const auto& [operation, method] : mapping["item_operations"].items()) {
            operation_to_method_mapping_[operation] = method;
        }
    }

    // Action operations
    if (mapping.contains("action_operations")) {
        for (const auto& [operation, method] : mapping["action_operations"].items()) {
            operation_to_method_mapping_[operation] = method;
        }
    }

    // Relationship operations
    if (mapping.contains("relationship_operations")) {
        for (const auto& [operation, method] : mapping["relationship_operations"].items()) {
            operation_to_method_mapping_[operation] = method;
        }
    }
}

HTTPValidationResult HTTPMethodValidator::validate_method_usage(
    const std::string& method,
    const std::string& path,
    bool has_request_body,
    const std::vector<std::string>& headers,
    const std::string& content_type) const {

    HTTPValidationResult result = {true, "", "info", {}};

    // Check if method is valid
    if (!is_valid_http_method(method)) {
        result.valid = false;
        result.message = "Invalid HTTP method: " + method;
        result.severity = "error";
        result.suggestions = {"Use standard HTTP methods: GET, POST, PUT, PATCH, DELETE"};
        return result;
    }

    // Validate request body
    auto body_result = validate_request_body(method, has_request_body);
    if (!body_result.valid) {
        result = body_result;
        return result;
    }

    // Validate headers
    auto header_result = validate_headers(method, headers);
    if (!header_result.valid && header_result.severity == "error") {
        result = header_result;
        return result;
    }

    // Validate content type
    auto content_result = validate_content_type(method, content_type);
    if (!content_result.valid && content_result.severity == "error") {
        result = content_result;
        return result;
    }

    // Validate parameters
    auto param_result = validate_parameters(method, path);
    if (!param_result.valid) {
        result = param_result;
        return result;
    }

    // Validate operation semantics
    auto semantic_result = validate_operation_semantics(method, path);
    if (!semantic_result.valid && semantic_result.severity == "warning") {
        // Only return semantic warnings, not failures
        return semantic_result;
    }

    return result;
}

std::optional<HTTPMethodRules> HTTPMethodValidator::get_method_rules(
    const std::string& method) const {

    auto it = method_rules_.find(method);
    if (it != method_rules_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool HTTPMethodValidator::is_method_safe(const std::string& method) const {
    auto rules = get_method_rules(method);
    return rules ? rules->safe : false;
}

bool HTTPMethodValidator::is_method_idempotent(const std::string& method) const {
    auto rules = get_method_rules(method);
    return rules ? rules->idempotent : false;
}

bool HTTPMethodValidator::is_method_cacheable(const std::string& method) const {
    auto rules = get_method_rules(method);
    return rules ? rules->cacheable : false;
}

std::string HTTPMethodValidator::get_recommended_method(const std::string& operation_type) const {
    auto it = operation_to_method_mapping_.find(operation_type);
    if (it != operation_to_method_mapping_.end()) {
        return it->second;
    }

    // Default recommendations based on operation type
    if (operation_type == "list" || operation_type == "get" || operation_type == "retrieve") {
        return "GET";
    } else if (operation_type == "create" || operation_type == "add") {
        return "POST";
    } else if (operation_type == "update" || operation_type == "replace") {
        return "PUT";
    } else if (operation_type == "modify" || operation_type == "patch") {
        return "PATCH";
    } else if (operation_type == "delete" || operation_type == "remove") {
        return "DELETE";
    } else if (operation_type.find("execute") != std::string::npos ||
               operation_type.find("process") != std::string::npos ||
               operation_type.find("run") != std::string::npos) {
        return "POST";
    }

    return "POST"; // Default fallback
}

std::vector<HTTPValidationResult> HTTPMethodValidator::validate_api_endpoints(
    const nlohmann::json& endpoints_config) const {

    std::vector<HTTPValidationResult> results;

    for (const auto& [category_name, category_data] : endpoints_config.items()) {
        if (!category_data.is_object()) continue;

        for (const auto& [endpoint_name, endpoint_data] : category_data.items()) {
            if (!endpoint_data.is_object()) continue;

            std::string method = endpoint_data.value("method", "");
            std::string path = endpoint_data.value("path", "");

            if (method.empty() || path.empty()) {
                HTTPValidationResult error_result = {
                    false,
                    "Missing method or path for endpoint: " + category_name + "." + endpoint_name,
                    "error",
                    {"Add method and path fields to endpoint configuration"}
                };
                results.push_back(error_result);
                continue;
            }

            // For now, just validate basic method usage
            // In production, this would also check has_request_body, headers, etc.
            auto validation = validate_method_usage(method, path);
            if (!validation.valid || validation.severity == "warning") {
                HTTPValidationResult result = validation;
                result.message = category_name + "." + endpoint_name + ": " + result.message;
                results.push_back(result);
            }
        }
    }

    return results;
}

// Private validation methods
HTTPValidationResult HTTPMethodValidator::validate_request_body(
    const std::string& method, bool has_request_body) const {

    auto rules = get_method_rules(method);
    if (!rules) {
        return {true, "", "info", {}}; // Unknown method, skip validation
    }

    if (!rules->body_allowed && has_request_body) {
        return {
            false,
            "HTTP method " + method + " should not have a request body",
            "error",
            {
                "Use POST, PUT, or PATCH for operations requiring request bodies",
                "Use query parameters or path parameters for " + method + " requests"
            }
        };
    }

    if (rules->body_allowed && !has_request_body) {
        // This is just informational, not an error
        return {
            true,
            "HTTP method " + method + " typically expects a request body",
            "info",
            {"Consider adding a request body for " + method + " operations"}
        };
    }

    return {true, "", "info", {}};
}

HTTPValidationResult HTTPMethodValidator::validate_headers(
    const std::string& method, const std::vector<std::string>& headers) const {

    auto required_headers = get_required_headers(method);

    for (const auto& required : required_headers) {
        bool found = std::find(headers.begin(), headers.end(), required) != headers.end();
        if (!found) {
            return {
                false,
                "Missing required header '" + required + "' for " + method + " method",
                "warning",
                {"Add '" + required + "' header to " + method + " requests"}
            };
        }
    }

    return {true, "", "info", {}};
}

HTTPValidationResult HTTPMethodValidator::validate_content_type(
    const std::string& method, const std::string& content_type) const {

    auto rules = get_method_rules(method);
    if (!rules || rules->content_types_allowed.empty()) {
        return {true, "", "info", {}}; // No specific content type requirements
    }

    // Check if content type is allowed
    bool allowed = std::find(rules->content_types_allowed.begin(),
                           rules->content_types_allowed.end(),
                           content_type) != rules->content_types_allowed.end();

    if (!allowed && !content_type.empty()) {
        std::stringstream ss;
        ss << "Content-Type '" << content_type << "' not recommended for " << method << " method. ";
        ss << "Recommended types: ";
        for (size_t i = 0; i < rules->content_types_allowed.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << rules->content_types_allowed[i];
        }

        return {
            false,
            ss.str(),
            "warning",
            {"Use recommended Content-Type for " + method + " method"}
        };
    }

    return {true, "", "info", {}};
}

HTTPValidationResult HTTPMethodValidator::validate_parameters(
    const std::string& method, const std::string& path) const {

    auto rules = get_method_rules(method);
    if (!rules) {
        return {true, "", "info", {}};
    }

    bool has_path_params = has_path_parameters(path);
    bool has_query_params = has_query_parameters(path);

    if (has_path_params && !rules->path_parameters_allowed) {
        return {
            false,
            "HTTP method " + method + " should not use path parameters",
            "warning",
            {"Consider using query parameters or request body instead of path parameters"}
        };
    }

    if (has_query_params && !rules->query_parameters_allowed) {
        return {
            false,
            "HTTP method " + method + " should not use query parameters",
            "warning",
            {"Consider using path parameters or request body instead of query parameters"}
        };
    }

    return {true, "", "info", {}};
}

HTTPValidationResult HTTPMethodValidator::validate_operation_semantics(
    const std::string& method, const std::string& path) const {

    std::string operation_type = extract_operation_type(path);
    std::string recommended_method = get_recommended_method(operation_type);

    if (recommended_method != method) {
        return {
            true, // Not a failure, just a suggestion
            "Operation '" + operation_type + "' typically uses " + recommended_method +
            " but " + method + " was specified",
            "info",
            {
                "Consider using " + recommended_method + " for " + operation_type + " operations",
                "Current usage follows RESTful conventions but could be more standard"
            }
        };
    }

    return {true, "", "info", {}};
}

// Utility methods
bool HTTPMethodValidator::is_valid_http_method(const std::string& method) const {
    static const std::vector<std::string> valid_methods = {
        "GET", "POST", "PUT", "PATCH", "DELETE", "HEAD", "OPTIONS"
    };

    return std::find(valid_methods.begin(), valid_methods.end(), method) != valid_methods.end();
}

bool HTTPMethodValidator::has_path_parameters(const std::string& path) const {
    std::regex param_regex(R"(\{[^{}]+\})");
    return std::regex_search(path, param_regex);
}

bool HTTPMethodValidator::has_query_parameters(const std::string& path) const {
    // This is a simplified check - in practice, query params are added at runtime
    // For validation purposes, we assume paths ending with ? have query params
    return path.find('?') != std::string::npos;
}

std::string HTTPMethodValidator::extract_operation_type(const std::string& path) const {
    // Extract operation type from path
    // This is a heuristic approach - in production, this would be more sophisticated

    std::vector<std::string> path_segments;
    std::stringstream ss(path);
    std::string segment;

    while (std::getline(ss, segment, '/')) {
        if (!segment.empty() && segment != "api") {
            path_segments.push_back(segment);
        }
    }

    if (path_segments.empty()) return "unknown";

    // Check last segment for operation indicators
    const std::string& last_segment = path_segments.back();

    if (last_segment == "list" || last_segment == "index") return "list";
    if (last_segment == "create" || last_segment == "new") return "create";
    if (last_segment == "update" || last_segment == "edit") return "update";
    if (last_segment == "delete" || last_segment == "remove") return "delete";
    if (last_segment.find("search") != std::string::npos) return "search";
    if (last_segment.find("filter") != std::string::npos) return "filter";
    if (last_segment.find("execute") != std::string::npos) return "execute";
    if (last_segment.find("process") != std::string::npos) return "process";
    if (last_segment.find("generate") != std::string::npos) return "generate";
    if (last_segment.find("analyze") != std::string::npos) return "analyze";

    // Check if it's an item operation (has parameter)
    if (last_segment.find('{') != std::string::npos) return "item_operation";

    return "unknown";
}

std::vector<std::string> HTTPMethodValidator::get_required_headers(const std::string& method) const {
    auto rules = get_method_rules(method);
    return rules ? rules->headers_required : std::vector<std::string>{};
}

} // namespace regulens
