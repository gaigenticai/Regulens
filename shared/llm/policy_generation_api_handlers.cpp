/**
 * Policy Generation API Handlers Implementation
 * REST API endpoints for natural language policy generation
 */

#include "policy_generation_api_handlers.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <sstream>

namespace regulens {

namespace {

std::string rule_type_to_string_local(RuleType type) {
    switch (type) {
        case RuleType::VALIDATION_RULE: return "VALIDATION_RULE";
        case RuleType::BUSINESS_RULE: return "BUSINESS_RULE";
        case RuleType::COMPLIANCE_RULE: return "COMPLIANCE_RULE";
        case RuleType::RISK_RULE: return "RISK_RULE";
        case RuleType::AUDIT_RULE: return "AUDIT_RULE";
        case RuleType::WORKFLOW_RULE: return "WORKFLOW_RULE";
        default: return "UNKNOWN";
    }
}

std::string domain_to_string_local(PolicyDomain domain) {
    switch (domain) {
        case PolicyDomain::FINANCIAL_COMPLIANCE: return "FINANCIAL_COMPLIANCE";
        case PolicyDomain::DATA_PRIVACY: return "DATA_PRIVACY";
        case PolicyDomain::REGULATORY_REPORTING: return "REGULATORY_REPORTING";
        case PolicyDomain::RISK_MANAGEMENT: return "RISK_MANAGEMENT";
        case PolicyDomain::OPERATIONAL_CONTROLS: return "OPERATIONAL_CONTROLS";
        case PolicyDomain::SECURITY_POLICY: return "SECURITY_POLICY";
        case PolicyDomain::AUDIT_PROCEDURES: return "AUDIT_PROCEDURES";
        default: return "UNKNOWN";
    }
}

std::string format_to_string_local(RuleFormat format) {
    switch (format) {
        case RuleFormat::JSON: return "JSON";
        case RuleFormat::YAML: return "YAML";
        case RuleFormat::DSL: return "DSL";
        case RuleFormat::PYTHON: return "PYTHON";
        case RuleFormat::JAVASCRIPT: return "JAVASCRIPT";
        default: return "UNKNOWN";
    }
}

}

PolicyGenerationAPIHandlers::PolicyGenerationAPIHandlers(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<PolicyGenerationService> policy_service
) : db_conn_(db_conn), policy_service_(policy_service), access_control_(db_conn) {

    if (!db_conn_) {
        throw std::runtime_error("Database connection is required for PolicyGenerationAPIHandlers");
    }
    if (!policy_service_) {
        throw std::runtime_error("PolicyGenerationService is required for PolicyGenerationAPIHandlers");
    }

    spdlog::info("PolicyGenerationAPIHandlers initialized");
}

PolicyGenerationAPIHandlers::~PolicyGenerationAPIHandlers() {
    spdlog::info("PolicyGenerationAPIHandlers shutting down");
}

std::string PolicyGenerationAPIHandlers::handle_generate_policy(const std::string& request_body, const std::string& user_id) {
    try {
        // Parse and validate request
        nlohmann::json request = nlohmann::json::parse(request_body);
        std::string validation_error;
        if (!validate_generation_request(request, validation_error)) {
            return create_error_response(validation_error).dump();
        }

        if (!validate_user_access(user_id, "generate_policy")) {
            return create_error_response("Access denied", 403).dump();
        }

        // Parse the generation request
        PolicyGenerationRequest generation_request = parse_generation_request(request);

        // Generate policy
        auto start_time = std::chrono::high_resolution_clock::now();
        PolicyGenerationResult result = policy_service_->generate_policy(generation_request);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        // Update processing time
        result.processing_time = processing_time;

        // Format response
        nlohmann::json response_data = format_generation_result(result);

        spdlog::info("Policy generation API request completed for user {}: {} in {}ms",
                    user_id, result.policy_id, processing_time.count());

        return create_success_response(response_data, "Policy generated successfully").dump();

    } catch (const nlohmann::json::exception& e) {
        spdlog::error("JSON parsing error in handle_generate_policy: {}", e.what());
        return create_error_response("Invalid JSON format", 400).dump();
    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_generate_policy: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string PolicyGenerationAPIHandlers::handle_validate_rule(const std::string& request_body, const std::string& user_id) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);

        if (!request.contains("rule_code") || !request["rule_code"].is_string()) {
            return create_error_response("Missing or invalid 'rule_code' field").dump();
        }

        if (!request.contains("format") || !request["format"].is_string()) {
            return create_error_response("Missing or invalid 'format' field").dump();
        }

        if (!validate_user_access(user_id, "validate_rule")) {
            return create_error_response("Access denied", 403).dump();
        }

        // Parse format
        std::string format_str = request["format"];
        RuleFormat format = RuleFormat::JSON; // default
        if (format_str == "yaml") format = RuleFormat::YAML;
        else if (format_str == "dsl") format = RuleFormat::DSL;
        else if (format_str == "python") format = RuleFormat::PYTHON;
        else if (format_str == "javascript") format = RuleFormat::JAVASCRIPT;

        // Create a minimal rule for validation
        GeneratedRule rule;
        rule.generated_code = request["rule_code"];
        rule.format = format;
        rule.name = request.value("name", "Validation Test Rule");

        // Validate the rule
        RuleValidationResult validation_result = policy_service_->validate_rule(rule);

        nlohmann::json response_data = format_validation_result(validation_result);
        response_data["rule_name"] = rule.name;
        response_data["format"] = format_str;

        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_validate_rule: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string PolicyGenerationAPIHandlers::handle_get_rule(const std::string& rule_id, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "get_rule")) {
            return create_error_response("Access denied", 403).dump();
        }

        if (!validate_rule_ownership(rule_id, user_id)) {
            return create_error_response("Rule not found or access denied", 404).dump();
        }

        auto rule_opt = policy_service_->get_rule(rule_id);
        if (!rule_opt) {
            return create_error_response("Rule not found", 404).dump();
        }

        nlohmann::json response_data = format_rule(*rule_opt);
        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_rule: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string PolicyGenerationAPIHandlers::handle_list_rules(const std::string& query_params, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "list_rules")) {
            return create_error_response("Access denied", 403).dump();
        }

        // Parse query parameters
        auto params = parse_query_params(query_params);
        PolicyDomain domain_filter = PolicyDomain::FINANCIAL_COMPLIANCE; // default
        int limit = 50;
        int offset = 0;

        if (params.count("domain")) {
            domain_filter = parse_domain_param(params["domain"]);
        }

        if (params.count("limit")) {
            limit = std::min(100, std::max(1, std::stoi(params["limit"])));
        }

        if (params.count("offset")) {
            offset = std::max(0, std::stoi(params["offset"]));
        }

        // Get rules by domain
        auto rules = policy_service_->get_rules_by_domain(domain_filter, limit);

        // Format response
        std::vector<nlohmann::json> formatted_rules;
        for (const auto& rule : rules) {
            formatted_rules.push_back(format_rule(rule));
        }

        nlohmann::json response_data = create_paginated_response(
            formatted_rules,
            formatted_rules.size(), // Simplified - should get actual count
            offset / limit + 1,
            limit
        );

        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_list_rules: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string PolicyGenerationAPIHandlers::handle_search_rules(const std::string& request_body, const std::string& user_id) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);

        if (!request.contains("query") || !request["query"].is_string()) {
            return create_error_response("Missing or invalid 'query' field").dump();
        }

        if (!validate_user_access(user_id, "search_rules")) {
            return create_error_response("Access denied", 403).dump();
        }

        std::string query = request["query"];
        int limit = request.value("limit", 20);

        auto rules = policy_service_->search_rules(query, limit);

        // Format response
        std::vector<nlohmann::json> formatted_rules;
        for (const auto& rule : rules) {
            formatted_rules.push_back(format_rule(rule));
        }

        nlohmann::json response_data = {
            {"query", query},
            {"results", formatted_rules},
            {"total_found", formatted_rules.size()}
        };

        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_search_rules: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string PolicyGenerationAPIHandlers::handle_deploy_rule(const std::string& rule_id, const std::string& request_body, const std::string& user_id) {
    try {
        if (!validate_rule_ownership(rule_id, user_id)) {
            return create_error_response("Rule not found or access denied", 404).dump();
        }

        nlohmann::json request = nlohmann::json::parse(request_body);

        if (!request.contains("target_environment")) {
            return create_error_response("Missing 'target_environment' field").dump();
        }

        // Parse deployment request
        RuleDeploymentRequest deployment_request;
        deployment_request.rule_id = rule_id;
        deployment_request.target_environment = request["target_environment"];
        deployment_request.deployed_by = user_id;
        if (request.contains("review_comments")) {
            deployment_request.review_comments = request["review_comments"];
        }

        // Deploy the rule
        RuleDeploymentResult deployment_result = policy_service_->deploy_rule(deployment_request);

        if (!deployment_result.success) {
            return create_error_response(deployment_result.error_message.value_or("Deployment failed"), 500).dump();
        }

        nlohmann::json response_data = format_deployment_result(deployment_result);
        return create_success_response(response_data, "Rule deployed successfully").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_deploy_rule: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string PolicyGenerationAPIHandlers::handle_get_templates(const std::string& domain_str, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "get_templates")) {
            return create_error_response("Access denied", 403).dump();
        }

        PolicyDomain domain = parse_domain_param(domain_str);
        auto templates = policy_service_->get_rule_templates(domain);

        nlohmann::json response_data = {
            {"domain", domain_str},
            {"templates", templates}
        };

        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_templates: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string PolicyGenerationAPIHandlers::handle_get_examples(const std::string& domain_str, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "get_examples")) {
            return create_error_response("Access denied", 403).dump();
        }

        PolicyDomain domain = parse_domain_param(domain_str);
        auto examples = policy_service_->get_example_descriptions(domain);

        nlohmann::json response_data = {
            {"domain", domain_str},
            {"examples", examples}
        };

        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_examples: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string PolicyGenerationAPIHandlers::handle_get_generation_stats(const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "get_stats")) {
            return create_error_response("Access denied", 403).dump();
        }

        auto stats = policy_service_->get_generation_stats();

        nlohmann::json response_data = {
            {"generation_stats", stats},
            {"supported_domains", get_supported_domains_list()},
            {"supported_rule_types", get_supported_rule_types_list()},
            {"supported_formats", get_supported_formats_list()}
        };

        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_generation_stats: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

// Helper method implementations

PolicyGenerationRequest PolicyGenerationAPIHandlers::parse_generation_request(const nlohmann::json& request) {
    PolicyGenerationRequest gen_request;

    gen_request.natural_language_description = request.value("description", "");
    gen_request.rule_type = RuleType::COMPLIANCE_RULE; // default

    if (request.contains("rule_type")) {
        std::string type_str = request["rule_type"];
        gen_request.rule_type = parse_rule_type_param(type_str);
    }

    if (request.contains("domain")) {
        std::string domain_str = request["domain"];
        gen_request.domain = parse_domain_param(domain_str);
    }

    if (request.contains("output_format")) {
        std::string format_str = request["output_format"];
        if (format_str == "yaml") gen_request.output_format = RuleFormat::YAML;
        else if (format_str == "dsl") gen_request.output_format = RuleFormat::DSL;
        else if (format_str == "python") gen_request.output_format = RuleFormat::PYTHON;
        else if (format_str == "javascript") gen_request.output_format = RuleFormat::JAVASCRIPT;
        else gen_request.output_format = RuleFormat::JSON;
    }

    gen_request.include_validation_tests = request.value("include_tests", true);
    gen_request.include_documentation = request.value("include_docs", true);
    gen_request.max_complexity_level = request.value("complexity_level", 3);

    if (request.contains("regulatory_framework")) {
        gen_request.regulatory_framework = request["regulatory_framework"];
    }

    if (request.contains("compliance_standard")) {
        gen_request.compliance_standard = request["compliance_standard"];
    }

    if (request.contains("existing_rules_context")) {
        gen_request.existing_rules_context = request["existing_rules_context"];
    }

    return gen_request;
}

nlohmann::json PolicyGenerationAPIHandlers::format_generation_result(const PolicyGenerationResult& result) {
    nlohmann::json response = {
        {"policy_id", result.policy_id},
        {"request_id", result.request_id},
        {"success", result.success},
        {"processing_time_ms", result.processing_time.count()},
        {"tokens_used", result.tokens_used},
        {"cost", result.cost},
        {"version", result.version},
        {"primary_rule", format_rule(result.primary_rule)}
    };

    if (!result.alternative_rules.empty()) {
        nlohmann::json alternatives = nlohmann::json::array();
        for (const auto& alt : result.alternative_rules) {
            alternatives.push_back(format_rule(alt));
        }
        response["alternative_rules"] = alternatives;
    }

    if (result.validation.syntax_valid || result.validation.logic_valid) {
        response["validation"] = format_validation_result(result.validation);
    }

    if (!result.success && result.error_message) {
        response["error"] = *result.error_message;
    }

    return response;
}

nlohmann::json PolicyGenerationAPIHandlers::format_rule(const GeneratedRule& rule) {
    nlohmann::json rule_json = {
        {"rule_id", rule.rule_id},
        {"name", rule.name},
        {"description", rule.description},
        {"natural_language_input", rule.natural_language_input},
        {"rule_type", rule_type_to_string_local(rule.rule_type)},
        {"domain", domain_to_string_local(rule.domain)},
        {"format", format_to_string_local(rule.format)},
        {"generated_code", rule.generated_code},
        {"confidence_score", rule.confidence_score},
        {"generated_at", std::chrono::duration_cast<std::chrono::seconds>(
            rule.generated_at.time_since_epoch()).count()},
        {"metadata", rule.rule_metadata}
    };

    if (!rule.validation_tests.empty()) {
        rule_json["validation_tests"] = rule.validation_tests;
    }

    if (!rule.documentation.empty()) {
        rule_json["documentation"] = rule.documentation;
    }

    if (!rule.suggested_improvements.empty()) {
        rule_json["suggested_improvements"] = rule.suggested_improvements;
    }

    return rule_json;
}

nlohmann::json PolicyGenerationAPIHandlers::format_validation_result(const RuleValidationResult& result) {
    return {
        {"syntax_valid", result.syntax_valid},
        {"logic_valid", result.logic_valid},
        {"security_safe", result.security_safe},
        {"overall_score", result.overall_score},
        {"validation_errors", result.validation_errors},
        {"warnings", result.warnings},
        {"test_results", result.test_results}
    };
}

nlohmann::json PolicyGenerationAPIHandlers::format_deployment_result(const RuleDeploymentResult& result) {
    return {
        {"success", result.success},
        {"deployment_id", result.deployment_id},
        {"status", result.status},
        {"deployed_at", std::chrono::duration_cast<std::chrono::seconds>(
            result.deployed_at.time_since_epoch()).count()},
        {"error_message", result.error_message.value_or("")}
    };
}

bool PolicyGenerationAPIHandlers::validate_generation_request(const nlohmann::json& request, std::string& error_message) {
    if (!request.contains("description") || !request["description"].is_string()) {
        error_message = "Missing or invalid 'description' field";
        return false;
    }

    std::string description = request["description"];
    if (description.empty()) {
        error_message = "Description cannot be empty";
        return false;
    }

    if (description.length() > 2000) {
        error_message = "Description too long (maximum 2000 characters)";
        return false;
    }

    return true;
}

bool PolicyGenerationAPIHandlers::validate_user_access(const std::string& user_id, const std::string& operation) {
    if (user_id.empty() || operation.empty()) {
        return false;
    }

    if (access_control_.is_admin(user_id)) {
        return true;
    }

    std::vector<AccessControlService::PermissionQuery> queries = {
        {operation, "policy_generation", "", 0},
        {operation, "policy_rule", "", 0},
        {operation, "policy_template", "", 0},
        {"manage_policy_generation", "", "", 0},
        {operation, "", "", 0}
    };

    if (operation.find("rule") != std::string::npos) {
        queries.push_back({"manage_policy_rules", "policy_rule", "", 0});
    }
    if (operation.find("template") != std::string::npos) {
        queries.push_back({"manage_policy_templates", "policy_template", "", 0});
    }
    if (operation.find("stats") != std::string::npos || operation.find("analytics") != std::string::npos) {
        queries.push_back({"view_policy_generation_metrics", "", "", 0});
    }

    return access_control_.has_any_permission(user_id, queries);
}

bool PolicyGenerationAPIHandlers::validate_rule_ownership(const std::string& rule_id, const std::string& user_id) {
    if (rule_id.empty() || user_id.empty()) {
        return false;
    }

    if (access_control_.is_admin(user_id)) {
        return true;
    }

    std::vector<AccessControlService::PermissionQuery> ownership_checks = {
        {"manage_rule", "policy_rule", rule_id, 0},
        {"manage_policy_rules", "policy_rule", rule_id, 0},
        {"manage_rule", "policy_rule", "*", 0},
        {"manage_policy_rules", "policy_rule", "*", 0}
    };

    return access_control_.has_any_permission(user_id, ownership_checks);
}

std::unordered_map<std::string, std::string> PolicyGenerationAPIHandlers::parse_query_params(const std::string& query_string) {
    std::unordered_map<std::string, std::string> params;

    if (query_string.empty()) return params;

    std::stringstream ss(query_string);
    std::string pair;

    while (std::getline(ss, pair, '&')) {
        size_t equals_pos = pair.find('=');
        if (equals_pos != std::string::npos) {
            std::string key = pair.substr(0, equals_pos);
            std::string value = pair.substr(equals_pos + 1);
            params[key] = value;
        }
    }

    return params;
}

PolicyDomain PolicyGenerationAPIHandlers::parse_domain_param(const std::string& domain_str) {
    if (domain_str == "data_privacy") return PolicyDomain::DATA_PRIVACY;
    if (domain_str == "regulatory_reporting") return PolicyDomain::REGULATORY_REPORTING;
    if (domain_str == "risk_management") return PolicyDomain::RISK_MANAGEMENT;
    if (domain_str == "operational_controls") return PolicyDomain::OPERATIONAL_CONTROLS;
    if (domain_str == "security_policy") return PolicyDomain::SECURITY_POLICY;
    if (domain_str == "audit_procedures") return PolicyDomain::AUDIT_PROCEDURES;
    return PolicyDomain::FINANCIAL_COMPLIANCE; // default
}

RuleType PolicyGenerationAPIHandlers::parse_rule_type_param(const std::string& type_str) {
    if (type_str == "validation") return RuleType::VALIDATION_RULE;
    if (type_str == "business") return RuleType::BUSINESS_RULE;
    if (type_str == "risk") return RuleType::RISK_RULE;
    if (type_str == "audit") return RuleType::AUDIT_RULE;
    if (type_str == "workflow") return RuleType::WORKFLOW_RULE;
    return RuleType::COMPLIANCE_RULE; // default
}

nlohmann::json PolicyGenerationAPIHandlers::create_success_response(const nlohmann::json& data, const std::string& message) {
    nlohmann::json response = {
        {"success", true},
        {"status_code", 200}
    };

    if (!message.empty()) {
        response["message"] = message;
    }

    if (data.is_object() || data.is_array()) {
        response["data"] = data;
    }

    return response;
}

nlohmann::json PolicyGenerationAPIHandlers::create_error_response(const std::string& message, int status_code) {
    return {
        {"success", false},
        {"status_code", status_code},
        {"error", message}
    };
}

nlohmann::json PolicyGenerationAPIHandlers::create_paginated_response(
    const std::vector<nlohmann::json>& items,
    int total_count,
    int page,
    int page_size
) {
    int total_pages = (total_count + page_size - 1) / page_size;

    return {
        {"items", items},
        {"pagination", {
            {"page", page},
            {"page_size", page_size},
            {"total_count", total_count},
            {"total_pages", total_pages},
            {"has_next", page < total_pages},
            {"has_prev", page > 1}
        }}
    };
}

std::string PolicyGenerationAPIHandlers::get_supported_domains_list() {
    return "financial_compliance, data_privacy, regulatory_reporting, risk_management, operational_controls, security_policy, audit_procedures";
}

std::string PolicyGenerationAPIHandlers::get_supported_rule_types_list() {
    return "validation_rule, business_rule, compliance_rule, risk_rule, audit_rule, workflow_rule";
}

std::string PolicyGenerationAPIHandlers::get_supported_formats_list() {
    return "json, yaml, dsl, python, javascript";
}

bool PolicyGenerationAPIHandlers::is_domain_supported(PolicyDomain domain) {
    return true; // All domains are supported
}

bool PolicyGenerationAPIHandlers::is_rule_type_supported(RuleType type) {
    return true; // All rule types are supported
}

bool PolicyGenerationAPIHandlers::is_format_supported(RuleFormat format) {
    return true; // All formats are supported
}

} // namespace regulens
