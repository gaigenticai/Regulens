/**
 * NL Policy Builder API Handlers
 * REST API endpoints for natural language policy conversion and management
 */

#ifndef REGULENS_POLICY_API_HANDLERS_HPP
#define REGULENS_POLICY_API_HANDLERS_HPP

#include <string>
#include <map>
#include <memory>
#include <libpq-fe.h>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"
#include "nl_policy_converter.hpp"

namespace regulens {
namespace policy {

class PolicyAPIHandlers {
public:
    PolicyAPIHandlers(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<StructuredLogger> logger,
        std::shared_ptr<NLPolicyConverter> policy_converter
    );

    // Policy conversion endpoints
    std::string handle_convert_natural_language(const std::string& request_body, const std::string& user_id);
    std::string handle_get_conversion(const std::string& conversion_id, const std::string& user_id);
    std::string handle_get_user_conversions(const std::string& user_id, const std::map<std::string, std::string>& query_params);

    // Policy management endpoints
    std::string handle_update_conversion_status(const std::string& conversion_id, const std::string& request_body, const std::string& user_id);
    std::string handle_deploy_policy(const std::string& conversion_id, const std::string& request_body, const std::string& user_id);
    std::string handle_submit_feedback(const std::string& conversion_id, const std::string& request_body, const std::string& user_id);

    // Template management endpoints
    std::string handle_get_templates(const std::map<std::string, std::string>& query_params);
    std::string handle_get_template(const std::string& template_id);
    std::string handle_create_template(const std::string& request_body, const std::string& user_id);

    // Analytics and reporting endpoints
    std::string handle_get_conversion_analytics(const std::string& user_id, const std::map<std::string, std::string>& query_params);
    std::string handle_get_popular_templates(const std::map<std::string, std::string>& query_params);
    std::string handle_get_success_rates();

    // Validation endpoints
    std::string handle_validate_policy(const std::string& request_body);
    std::string handle_get_validation_rules(const std::map<std::string, std::string>& query_params);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<NLPolicyConverter> policy_converter_;

    // Helper methods
    PolicyConversionRequest parse_conversion_request(const nlohmann::json& request_json, const std::string& user_id);
    PolicyDeploymentRequest parse_deployment_request(const nlohmann::json& request_json, const std::string& conversion_id, const std::string& user_id);

    nlohmann::json format_conversion_result(const PolicyConversionResult& result);
    nlohmann::json format_deployment_result(const PolicyDeploymentResult& result);
    nlohmann::json format_template(const PolicyTemplate& tmpl);
    nlohmann::json format_validation_result(const PolicyValidationResult& result);

    bool validate_conversion_access(const std::string& conversion_id, const std::string& user_id);
    bool validate_admin_access(const std::string& user_id); // For template management

    std::string create_error_response(const std::string& message, int status_code = 400);
    std::string create_success_response(const nlohmann::json& data, const std::string& message = "");

    // Database query helpers
    std::vector<nlohmann::json> query_conversions_paginated(const std::string& user_id, const std::map<std::string, std::string>& filters, int limit, int offset);
    std::optional<nlohmann::json> query_conversion_details(const std::string& conversion_id);

    // Analytics helpers
    nlohmann::json calculate_conversion_metrics(const std::string& user_id, const std::map<std::string, std::string>& filters);
    nlohmann::json calculate_template_popularity();
    nlohmann::json calculate_policy_type_success_rates();

    // Validation helpers
    std::vector<nlohmann::json> get_validation_rules_for_type(const std::string& policy_type);
    bool apply_validation_rule(const nlohmann::json& policy, const nlohmann::json& validation_rule);

    // Utility methods
    std::string extract_policy_type_from_request(const nlohmann::json& request_json);
    bool is_valid_policy_status(const std::string& status);
    std::map<std::string, std::string> parse_query_parameters(const std::map<std::string, std::string>& query_params);

    // Rate limiting and security
    bool check_conversion_rate_limit(const std::string& user_id);
    void record_conversion_attempt(const std::string& user_id);

    // Caching helpers
    std::optional<nlohmann::json> get_cached_analytics(const std::string& cache_key);
    void cache_analytics_result(const std::string& cache_key, const nlohmann::json& data, int ttl_seconds = 300);
};

} // namespace policy
} // namespace regulens

#endif // REGULENS_POLICY_API_HANDLERS_HPP
