/**
 * Policy Generation API Handlers
 * REST API endpoints for natural language policy generation
 */

#ifndef POLICY_GENERATION_API_HANDLERS_HPP
#define POLICY_GENERATION_API_HANDLERS_HPP

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "policy_generation_service.hpp"
#include "../database/postgresql_connection.hpp"
#include "../security/access_control_service.hpp"

namespace regulens {

class PolicyGenerationAPIHandlers {
public:
    PolicyGenerationAPIHandlers(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<PolicyGenerationService> policy_service
    );

    ~PolicyGenerationAPIHandlers();

    // Policy Generation Endpoints
    std::string handle_generate_policy(const std::string& request_body, const std::string& user_id);
    std::string handle_validate_rule(const std::string& request_body, const std::string& user_id);

    // Rule Management Endpoints
    std::string handle_get_rule(const std::string& rule_id, const std::string& user_id);
    std::string handle_list_rules(const std::string& query_params, const std::string& user_id);
    std::string handle_search_rules(const std::string& request_body, const std::string& user_id);
    std::string handle_update_rule(const std::string& rule_id, const std::string& request_body, const std::string& user_id);
    std::string handle_delete_rule(const std::string& rule_id, const std::string& user_id);

    // Version Control & Deployment Endpoints
    std::string handle_deploy_rule(const std::string& rule_id, const std::string& request_body, const std::string& user_id);
    std::string handle_get_rule_history(const std::string& rule_id, const std::string& user_id);
    std::string handle_get_rule_version(const std::string& rule_id, const std::string& version, const std::string& user_id);

    // Template & Example Endpoints
    std::string handle_get_templates(const std::string& domain, const std::string& user_id);
    std::string handle_get_examples(const std::string& domain, const std::string& user_id);

    // Analytics Endpoints
    std::string handle_get_generation_stats(const std::string& user_id);
    std::string handle_get_popular_templates(const std::string& user_id);

    // Configuration Endpoints
    std::string handle_get_config();
    std::string handle_update_config(const std::string& request_body);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<PolicyGenerationService> policy_service_;
    AccessControlService access_control_;

    // Helper methods
    PolicyGenerationRequest parse_generation_request(const nlohmann::json& request);
    RuleDeploymentRequest parse_deployment_request(const nlohmann::json& request);
    nlohmann::json format_generation_result(const PolicyGenerationResult& result);
    nlohmann::json format_rule(const GeneratedRule& rule);
    nlohmann::json format_validation_result(const RuleValidationResult& result);
    nlohmann::json format_deployment_result(const RuleDeploymentResult& result);

    // Validation methods
    bool validate_generation_request(const nlohmann::json& request, std::string& error_message);
    bool validate_user_access(const std::string& user_id, const std::string& operation);
    bool validate_rule_ownership(const std::string& rule_id, const std::string& user_id);

    // Query parsing
    std::unordered_map<std::string, std::string> parse_query_params(const std::string& query_string);
    PolicyDomain parse_domain_param(const std::string& domain_str);
    RuleType parse_rule_type_param(const std::string& type_str);

    // Response formatting
    nlohmann::json create_success_response(const nlohmann::json& data = nullptr,
                                         const std::string& message = "");
    nlohmann::json create_error_response(const std::string& message,
                                       int status_code = 400);
    nlohmann::json create_paginated_response(const std::vector<nlohmann::json>& items,
                                           int total_count,
                                           int page,
                                           int page_size);

    // Utility methods
    std::string get_supported_domains_list();
    std::string get_supported_rule_types_list();
    std::string get_supported_formats_list();
    bool is_domain_supported(PolicyDomain domain);
    bool is_rule_type_supported(RuleType type);
    bool is_format_supported(RuleFormat format);
};

} // namespace regulens

#endif // POLICY_GENERATION_API_HANDLERS_HPP
