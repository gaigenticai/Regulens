/**
 * HTTP Method Validator
 * Production-grade validation of HTTP method usage against RESTful conventions
 */

#ifndef HTTP_METHOD_VALIDATOR_HPP
#define HTTP_METHOD_VALIDATOR_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>
#include "../../shared/error_handler.hpp"
#include "../logging/structured_logger.hpp"

namespace regulens {

struct HTTPMethodRules {
    bool body_allowed;
    bool query_parameters_allowed;
    bool path_parameters_allowed;
    std::vector<std::string> headers_required;
    std::vector<std::string> content_types_allowed;
    bool safe;           // Doesn't modify server state
    bool idempotent;     // Multiple identical requests have same effect
    bool cacheable;      // Response can be cached
};

struct HTTPValidationResult {
    bool valid;
    std::string message;
    std::string severity; // "error", "warning", "info"
    std::vector<std::string> suggestions;
};

class HTTPMethodValidator {
public:
    static HTTPMethodValidator& get_instance();

    // Initialize with method mapping configuration
    bool initialize(const std::string& config_path,
                   std::shared_ptr<StructuredLogger> logger);

    // Validate HTTP method usage
    HTTPValidationResult validate_method_usage(const std::string& method,
                                         const std::string& path,
                                         bool has_request_body = false,
                                         const std::vector<std::string>& headers = {},
                                         const std::string& content_type = "") const;

    // Get method rules
    std::optional<HTTPMethodRules> get_method_rules(const std::string& method) const;

    // Check if method is safe (read-only)
    bool is_method_safe(const std::string& method) const;

    // Check if method is idempotent
    bool is_method_idempotent(const std::string& method) const;

    // Check if method is cacheable
    bool is_method_cacheable(const std::string& method) const;

    // Get recommended method for operation type
    std::string get_recommended_method(const std::string& operation_type) const;

    // Validate entire API endpoint configuration
    std::vector<ValidationResult> validate_api_endpoints(const nlohmann::json& endpoints_config) const;

private:
    HTTPMethodValidator() = default;
    ~HTTPMethodValidator() = default;
    HTTPMethodValidator(const HTTPMethodValidator&) = delete;
    HTTPMethodValidator& operator=(const HTTPMethodValidator&) = delete;

    bool load_config(const std::string& config_path);
    void build_method_rules();
    void build_operation_mapping();

    std::shared_ptr<StructuredLogger> logger_;
    nlohmann::json config_;
    std::string config_path_;

    // Cached data structures
    std::unordered_map<std::string, HTTPMethodRules> method_rules_;
    std::unordered_map<std::string, std::string> operation_to_method_mapping_;

    // Validation helper methods
    ValidationResult validate_request_body(const std::string& method, bool has_request_body) const;
    ValidationResult validate_headers(const std::string& method, const std::vector<std::string>& headers) const;
    ValidationResult validate_content_type(const std::string& method, const std::string& content_type) const;
    ValidationResult validate_parameters(const std::string& method, const std::string& path) const;
    ValidationResult validate_operation_semantics(const std::string& method, const std::string& path) const;

    // Utility methods
    bool is_valid_http_method(const std::string& method) const;
    bool has_path_parameters(const std::string& path) const;
    bool has_query_parameters(const std::string& path) const;
    std::string extract_operation_type(const std::string& path) const;
    std::vector<std::string> get_required_headers(const std::string& method) const;
};

} // namespace regulens

#endif // HTTP_METHOD_VALIDATOR_HPP
