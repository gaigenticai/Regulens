/**
 * Advanced Rule Engine API Handlers Implementation
 * REST API endpoints for rule engine management and evaluation
 */

#include "advanced_rule_engine_api_handlers.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <uuid/uuid.h>

namespace regulens {

AdvancedRuleEngineAPIHandlers::AdvancedRuleEngineAPIHandlers(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<AdvancedRuleEngine> rule_engine
) : db_conn_(db_conn), rule_engine_(rule_engine) {

    if (!db_conn_) {
        throw std::runtime_error("Database connection is required for AdvancedRuleEngineAPIHandlers");
    }
    if (!rule_engine_) {
        throw std::runtime_error("AdvancedRuleEngine is required for AdvancedRuleEngineAPIHandlers");
    }

    spdlog::info("AdvancedRuleEngineAPIHandlers initialized");
}

AdvancedRuleEngineAPIHandlers::~AdvancedRuleEngineAPIHandlers() {
    spdlog::info("AdvancedRuleEngineAPIHandlers shut down");
}

std::string AdvancedRuleEngineAPIHandlers::handle_create_rule(const std::string& request_body, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "create_rule")) {
            return create_error_response("Access denied", 403).dump();
        }

        nlohmann::json request = nlohmann::json::parse(request_body);
        std::string validation_error;
        if (!validate_rule_request(request, validation_error)) {
            return create_error_response(validation_error).dump();
        }

        RuleDefinition rule = parse_rule_definition(request);

        if (!rule_engine_->create_rule(rule)) {
            return create_error_response("Failed to create rule", 500).dump();
        }

        nlohmann::json response_data = format_rule_definition(rule);
        spdlog::info("Rule created successfully: {} by user {}", rule.rule_name, user_id);

        return create_success_response(response_data, "Rule created successfully").dump();

    } catch (const nlohmann::json::exception& e) {
        spdlog::error("JSON parsing error in handle_create_rule: {}", e.what());
        return create_error_response("Invalid JSON format", 400).dump();
    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_create_rule: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string AdvancedRuleEngineAPIHandlers::handle_update_rule(const std::string& rule_id, const std::string& request_body, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "update_rule")) {
            return create_error_response("Access denied", 403).dump();
        }

        nlohmann::json request = nlohmann::json::parse(request_body);
        std::string validation_error;
        if (!validate_rule_request(request, validation_error)) {
            return create_error_response(validation_error).dump();
        }

        RuleDefinition rule = parse_rule_definition(request);
        rule.rule_id = rule_id; // Ensure ID matches

        if (!rule_engine_->update_rule(rule_id, rule)) {
            return create_error_response("Failed to update rule", 500).dump();
        }

        nlohmann::json response_data = format_rule_definition(rule);
        spdlog::info("Rule updated successfully: {} by user {}", rule.rule_name, user_id);

        return create_success_response(response_data, "Rule updated successfully").dump();

    } catch (const nlohmann::json::exception& e) {
        spdlog::error("JSON parsing error in handle_update_rule: {}", e.what());
        return create_error_response("Invalid JSON format", 400).dump();
    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_update_rule: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string AdvancedRuleEngineAPIHandlers::handle_delete_rule(const std::string& rule_id, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "delete_rule")) {
            return create_error_response("Access denied", 403).dump();
        }

        if (!rule_engine_->delete_rule(rule_id)) {
            return create_error_response("Failed to delete rule or rule not found", 404).dump();
        }

        spdlog::info("Rule deleted successfully: {} by user {}", rule_id, user_id);
        return create_success_response(nlohmann::json{{"rule_id", rule_id}}, "Rule deleted successfully").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_delete_rule: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string AdvancedRuleEngineAPIHandlers::handle_get_rule(const std::string& rule_id, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "read_rule")) {
            return create_error_response("Access denied", 403).dump();
        }

        auto rule_opt = rule_engine_->get_rule(rule_id);
        if (!rule_opt) {
            return create_error_response("Rule not found", 404).dump();
        }

        nlohmann::json response_data = format_rule_definition(*rule_opt);
        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_rule: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string AdvancedRuleEngineAPIHandlers::handle_list_rules(const std::string& query_params, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "list_rules")) {
            return create_error_response("Access denied", 403).dump();
        }

        // Parse query parameters
        auto params = parse_query_params(query_params);
        std::string category_filter = params["category"];

        std::vector<RuleDefinition> rules;
        if (!category_filter.empty()) {
            RuleCategory category;
            if (category_filter == "FRAUD_DETECTION") category = RuleCategory::FRAUD_DETECTION;
            else if (category_filter == "COMPLIANCE_CHECK") category = RuleCategory::COMPLIANCE_CHECK;
            else if (category_filter == "RISK_ASSESSMENT") category = RuleCategory::RISK_ASSESSMENT;
            else if (category_filter == "BUSINESS_LOGIC") category = RuleCategory::BUSINESS_LOGIC;
            else if (category_filter == "SECURITY_POLICY") category = RuleCategory::SECURITY_POLICY;
            else if (category_filter == "AUDIT_PROCEDURE") category = RuleCategory::AUDIT_PROCEDURE;
            else return create_error_response("Invalid category filter").dump();

            rules = rule_engine_->get_rules_by_category(category);
        } else {
            rules = rule_engine_->get_active_rules();
        }

        nlohmann::json rules_array = nlohmann::json::array();
        for (const auto& rule : rules) {
            rules_array.push_back(format_rule_definition(rule));
        }

        nlohmann::json response_data = {
            {"rules", rules_array},
            {"total", rules.size()}
        };

        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_list_rules: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string AdvancedRuleEngineAPIHandlers::handle_enable_rule(const std::string& rule_id, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "manage_rule")) {
            return create_error_response("Access denied", 403).dump();
        }

        if (!rule_engine_->enable_rule(rule_id)) {
            return create_error_response("Failed to enable rule or rule not found", 404).dump();
        }

        spdlog::info("Rule enabled successfully: {} by user {}", rule_id, user_id);
        return create_success_response(nlohmann::json{{"rule_id", rule_id}, {"enabled", true}}, "Rule enabled successfully").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_enable_rule: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string AdvancedRuleEngineAPIHandlers::handle_disable_rule(const std::string& rule_id, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "manage_rule")) {
            return create_error_response("Access denied", 403).dump();
        }

        if (!rule_engine_->disable_rule(rule_id)) {
            return create_error_response("Failed to disable rule or rule not found", 404).dump();
        }

        spdlog::info("Rule disabled successfully: {} by user {}", rule_id, user_id);
        return create_success_response(nlohmann::json{{"rule_id", rule_id}, {"enabled", false}}, "Rule disabled successfully").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_disable_rule: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string AdvancedRuleEngineAPIHandlers::handle_evaluate_entity(const std::string& request_body, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "evaluate_entity")) {
            return create_error_response("Access denied", 403).dump();
        }

        nlohmann::json request = nlohmann::json::parse(request_body);
        std::string validation_error;
        if (!validate_evaluation_request(request, validation_error)) {
            return create_error_response(validation_error).dump();
        }

        EvaluationContext context = parse_evaluation_context(request);

        auto start_time = std::chrono::high_resolution_clock::now();
        RuleResult result = rule_engine_->evaluate_entity(context);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        nlohmann::json response_data = format_rule_result(result);
        response_data["total_processing_time_ms"] = processing_time.count();

        spdlog::info("Entity evaluation completed for {}: score={}, triggered={} in {}ms",
                    context.entity_id, result.score, result.triggered, processing_time.count());

        return create_success_response(response_data, "Entity evaluation completed").dump();

    } catch (const nlohmann::json::exception& e) {
        spdlog::error("JSON parsing error in handle_evaluate_entity: {}", e.what());
        return create_error_response("Invalid JSON format", 400).dump();
    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_evaluate_entity: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string AdvancedRuleEngineAPIHandlers::handle_evaluate_batch(const std::string& request_body, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "evaluate_batch")) {
            return create_error_response("Access denied", 403).dump();
        }

        nlohmann::json request = nlohmann::json::parse(request_body);
        if (!request.contains("contexts") || !request["contexts"].is_array()) {
            return create_error_response("Missing or invalid 'contexts' array").dump();
        }

        if (request["contexts"].size() > 1000) {
            return create_error_response("Batch size too large (maximum 1000 contexts)").dump();
        }

        std::vector<EvaluationContext> contexts = parse_evaluation_contexts(request);

        auto start_time = std::chrono::high_resolution_clock::now();
        EvaluationBatch batch = rule_engine_->evaluate_batch(contexts);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        nlohmann::json response_data = format_evaluation_batch(batch);
        response_data["total_processing_time_ms"] = processing_time.count();

        spdlog::info("Batch evaluation completed: {} contexts, {} triggered rules in {}ms",
                    contexts.size(), batch.rules_triggered, processing_time.count());

        return create_success_response(response_data, "Batch evaluation completed").dump();

    } catch (const nlohmann::json::exception& e) {
        spdlog::error("JSON parsing error in handle_evaluate_batch: {}", e.what());
        return create_error_response("Invalid JSON format", 400).dump();
    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_evaluate_batch: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string AdvancedRuleEngineAPIHandlers::handle_get_performance_stats(const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "read_stats")) {
            return create_error_response("Access denied", 403).dump();
        }

        nlohmann::json stats = rule_engine_->get_performance_stats();
        return create_success_response(stats).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_performance_stats: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string AdvancedRuleEngineAPIHandlers::handle_get_rule_stats(const std::string& rule_id, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "read_stats")) {
            return create_error_response("Access denied", 403).dump();
        }

        nlohmann::json stats = rule_engine_->get_rule_execution_stats(rule_id);
        return create_success_response(stats).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_rule_stats: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string AdvancedRuleEngineAPIHandlers::handle_get_evaluation_history(const std::string& query_params, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "read_history")) {
            return create_error_response("Access denied", 403).dump();
        }

        auto params = parse_query_params(query_params);
        int limit = 100;
        if (params.count("limit")) {
            limit = std::stoi(params["limit"]);
            limit = std::max(1, std::min(limit, 1000)); // Clamp between 1 and 1000
        }

        std::vector<nlohmann::json> evaluations = get_recent_evaluations(limit);

        nlohmann::json response_data = {
            {"evaluations", evaluations},
            {"total", evaluations.size()}
        };

        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_evaluation_history: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string AdvancedRuleEngineAPIHandlers::handle_get_configuration(const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "read_config")) {
            return create_error_response("Access denied", 403).dump();
        }

        nlohmann::json config = {
            {"execution_timeout_ms", 5000},
            {"max_parallel_executions", 10},
            {"cache_enabled", true},
            {"cache_ttl_seconds", 300},
            {"batch_processing_enabled", true},
            {"max_batch_size", 100}
        };

        return create_success_response(config).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_configuration: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string AdvancedRuleEngineAPIHandlers::handle_update_configuration(const std::string& request_body, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "update_config")) {
            return create_error_response("Access denied", 403).dump();
        }

        nlohmann::json request = nlohmann::json::parse(request_body);

        // Update configuration (simplified - in production, validate and persist)
        if (request.contains("execution_timeout_ms")) {
            std::chrono::milliseconds timeout(request["execution_timeout_ms"]);
            rule_engine_->set_execution_timeout(timeout);
        }

        if (request.contains("max_parallel_executions")) {
            int max_parallel = request["max_parallel_executions"];
            rule_engine_->set_max_parallel_executions(max_parallel);
        }

        if (request.contains("cache_enabled")) {
            bool cache_enabled = request["cache_enabled"];
            rule_engine_->set_cache_enabled(cache_enabled);
        }

        if (request.contains("cache_ttl_seconds")) {
            int ttl = request["cache_ttl_seconds"];
            rule_engine_->set_cache_ttl_seconds(ttl);
        }

        if (request.contains("batch_processing_enabled")) {
            bool batch_enabled = request["batch_processing_enabled"];
            rule_engine_->set_batch_processing_enabled(batch_enabled);
        }

        if (request.contains("max_batch_size")) {
            int batch_size = request["max_batch_size"];
            rule_engine_->set_max_batch_size(batch_size);
        }

        spdlog::info("Configuration updated by user {}", user_id);
        return create_success_response(nlohmann::json{{"message", "Configuration updated successfully"}}, "Configuration updated successfully").dump();

    } catch (const nlohmann::json::exception& e) {
        spdlog::error("JSON parsing error in handle_update_configuration: {}", e.what());
        return create_error_response("Invalid JSON format", 400).dump();
    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_update_configuration: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

// Helper method implementations

RuleDefinition AdvancedRuleEngineAPIHandlers::parse_rule_definition(const nlohmann::json& request) {
    RuleDefinition rule;

    rule.rule_id = request.value("rule_id", generate_request_id());
    rule.rule_name = request["rule_name"];
    rule.description = request.value("description", "");
    rule.threshold_score = request.value("threshold_score", 0.5);
    rule.enabled = request.value("enabled", true);

    // Parse category
    std::string category_str = request.value("category", "COMPLIANCE_CHECK");
    if (category_str == "FRAUD_DETECTION") rule.category = RuleCategory::FRAUD_DETECTION;
    else if (category_str == "COMPLIANCE_CHECK") rule.category = RuleCategory::COMPLIANCE_CHECK;
    else if (category_str == "RISK_ASSESSMENT") rule.category = RuleCategory::RISK_ASSESSMENT;
    else if (category_str == "BUSINESS_LOGIC") rule.category = RuleCategory::BUSINESS_LOGIC;
    else if (category_str == "SECURITY_POLICY") rule.category = RuleCategory::SECURITY_POLICY;
    else if (category_str == "AUDIT_PROCEDURE") rule.category = RuleCategory::AUDIT_PROCEDURE;

    // Parse severity
    std::string severity_str = request.value("severity", "MEDIUM");
    if (severity_str == "CRITICAL") rule.severity = RuleSeverity::CRITICAL;
    else if (severity_str == "HIGH") rule.severity = RuleSeverity::HIGH;
    else if (severity_str == "MEDIUM") rule.severity = RuleSeverity::MEDIUM;
    else rule.severity = RuleSeverity::LOW;

    // Parse action
    std::string action_str = request.value("action", "ALLOW");
    if (action_str == "DENY") rule.action = RuleAction::DENY;
    else if (action_str == "ESCALATE") rule.action = RuleAction::ESCALATE;
    else if (action_str == "MONITOR") rule.action = RuleAction::MONITOR;
    else if (action_str == "ALERT") rule.action = RuleAction::ALERT;
    else if (action_str == "QUARANTINE") rule.action = RuleAction::QUARANTINE;
    else rule.action = RuleAction::ALLOW;

    // Parse conditions
    if (request.contains("conditions") && request["conditions"].is_array()) {
        for (const auto& cond_json : request["conditions"]) {
            RuleCondition condition;
            condition.field_name = cond_json["field_name"];
            condition.operator_type = cond_json["operator"];
            condition.value = cond_json["value"];
            condition.weight = cond_json.value("weight", 1.0);
            rule.conditions.push_back(condition);
        }
    }

    // Parse tags
    if (request.contains("tags") && request["tags"].is_array()) {
        for (const auto& tag : request["tags"]) {
            rule.tags.push_back(tag);
        }
    }

    rule.created_at = std::chrono::system_clock::now();
    rule.updated_at = rule.created_at;

    return rule;
}

nlohmann::json AdvancedRuleEngineAPIHandlers::format_rule_definition(const RuleDefinition& rule) {
    nlohmann::json json;

    json["rule_id"] = rule.rule_id;
    json["rule_name"] = rule.rule_name;
    json["description"] = rule.description;
    json["threshold_score"] = rule.threshold_score;
    json["enabled"] = rule.enabled;

    // Format category
    switch (rule.category) {
        case RuleCategory::FRAUD_DETECTION: json["category"] = "FRAUD_DETECTION"; break;
        case RuleCategory::COMPLIANCE_CHECK: json["category"] = "COMPLIANCE_CHECK"; break;
        case RuleCategory::RISK_ASSESSMENT: json["category"] = "RISK_ASSESSMENT"; break;
        case RuleCategory::BUSINESS_LOGIC: json["category"] = "BUSINESS_LOGIC"; break;
        case RuleCategory::SECURITY_POLICY: json["category"] = "SECURITY_POLICY"; break;
        case RuleCategory::AUDIT_PROCEDURE: json["category"] = "AUDIT_PROCEDURE"; break;
    }

    // Format severity
    switch (rule.severity) {
        case RuleSeverity::CRITICAL: json["severity"] = "CRITICAL"; break;
        case RuleSeverity::HIGH: json["severity"] = "HIGH"; break;
        case RuleSeverity::MEDIUM: json["severity"] = "MEDIUM"; break;
        case RuleSeverity::LOW: json["severity"] = "LOW"; break;
    }

    // Format action
    switch (rule.action) {
        case RuleAction::ALLOW: json["action"] = "ALLOW"; break;
        case RuleAction::DENY: json["action"] = "DENY"; break;
        case RuleAction::ESCALATE: json["action"] = "ESCALATE"; break;
        case RuleAction::MONITOR: json["action"] = "MONITOR"; break;
        case RuleAction::ALERT: json["action"] = "ALERT"; break;
        case RuleAction::QUARANTINE: json["action"] = "QUARANTINE"; break;
    }

    // Format conditions
    nlohmann::json conditions_array = nlohmann::json::array();
    for (const auto& condition : rule.conditions) {
        nlohmann::json cond_json;
        cond_json["field_name"] = condition.field_name;
        cond_json["operator"] = condition.operator_type;
        cond_json["value"] = condition.value;
        cond_json["weight"] = condition.weight;
        conditions_array.push_back(cond_json);
    }
    json["conditions"] = conditions_array;

    json["tags"] = rule.tags;

    return json;
}

nlohmann::json AdvancedRuleEngineAPIHandlers::format_rule_result(const RuleResult& result) {
    nlohmann::json json;

    json["evaluation_id"] = result.evaluation_id;
    json["rule_id"] = result.rule_id;
    json["entity_id"] = result.entity_id;
    json["score"] = result.score;
    json["triggered"] = result.triggered;
    json["processing_time_ms"] = result.processing_time.count();

    // Format action
    switch (result.action) {
        case RuleAction::ALLOW: json["action"] = "ALLOW"; break;
        case RuleAction::DENY: json["action"] = "DENY"; break;
        case RuleAction::ESCALATE: json["action"] = "ESCALATE"; break;
        case RuleAction::MONITOR: json["action"] = "MONITOR"; break;
        case RuleAction::ALERT: json["action"] = "ALERT"; break;
        case RuleAction::QUARANTINE: json["action"] = "QUARANTINE"; break;
    }

    json["matched_conditions"] = result.matched_conditions;
    json["condition_scores"] = result.condition_scores;

    return json;
}

nlohmann::json AdvancedRuleEngineAPIHandlers::format_evaluation_batch(const EvaluationBatch& batch) {
    nlohmann::json json;

    json["batch_id"] = batch.batch_id;
    json["total_processing_time_ms"] = batch.total_processing_time.count();
    json["rules_evaluated"] = batch.rules_evaluated;
    json["rules_triggered"] = batch.rules_triggered;

    nlohmann::json results_array = nlohmann::json::array();
    for (const auto& result : batch.results) {
        results_array.push_back(format_rule_result(result));
    }
    json["results"] = results_array;

    return json;
}

EvaluationContext AdvancedRuleEngineAPIHandlers::parse_evaluation_context(const nlohmann::json& request) {
    EvaluationContext context;

    context.entity_id = request["entity_id"];
    context.entity_type = request.value("entity_type", "unknown");
    context.source_system = request.value("source_system", "api");
    context.timestamp = std::chrono::system_clock::now();

    if (request.contains("data")) {
        context.data = request["data"];
    }

    if (request.contains("metadata") && request["metadata"].is_object()) {
        for (auto& [key, value] : request["metadata"].items()) {
            context.metadata[key] = value.dump();
        }
    }

    return context;
}

std::vector<EvaluationContext> AdvancedRuleEngineAPIHandlers::parse_evaluation_contexts(const nlohmann::json& request) {
    std::vector<EvaluationContext> contexts;

    for (const auto& context_json : request["contexts"]) {
        contexts.push_back(parse_evaluation_context(context_json));
    }

    return contexts;
}

bool AdvancedRuleEngineAPIHandlers::validate_rule_request(const nlohmann::json& request, std::string& error_message) {
    if (!request.contains("rule_name") || !request["rule_name"].is_string()) {
        error_message = "Missing or invalid 'rule_name' field";
        return false;
    }

    if (!request.contains("conditions") || !request["conditions"].is_array()) {
        error_message = "Missing or invalid 'conditions' array";
        return false;
    }

    if (request["conditions"].empty()) {
        error_message = "At least one condition is required";
        return false;
    }

    return true;
}

bool AdvancedRuleEngineAPIHandlers::validate_evaluation_request(const nlohmann::json& request, std::string& error_message) {
    if (!request.contains("entity_id") || !request["entity_id"].is_string()) {
        error_message = "Missing or invalid 'entity_id' field";
        return false;
    }

    if (!request.contains("data") || !request["data"].is_object()) {
        error_message = "Missing or invalid 'data' object";
        return false;
    }

    return true;
}

bool AdvancedRuleEngineAPIHandlers::validate_user_access(const std::string& user_id, const std::string& action) {
    // Simplified access control - in production, implement proper RBAC
    // For now, allow all authenticated users
    return !user_id.empty();
}

nlohmann::json AdvancedRuleEngineAPIHandlers::create_success_response(const nlohmann::json& data, const std::string& message) {
    nlohmann::json response;
    response["success"] = true;
    response["data"] = data;
    if (!message.empty()) {
        response["message"] = message;
    }
    response["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return response;
}

nlohmann::json AdvancedRuleEngineAPIHandlers::create_error_response(const std::string& message, int status_code) {
    nlohmann::json response;
    response["success"] = false;
    response["error"] = message;
    response["status_code"] = status_code;
    response["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return response;
}

std::string AdvancedRuleEngineAPIHandlers::generate_request_id() {
    uuid_t uuid;
    uuid_generate(uuid);

    char uuid_str[37];
    uuid_unparse(uuid, uuid_str);

    return std::string(uuid_str);
}

std::string AdvancedRuleEngineAPIHandlers::url_decode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%') {
            if (i + 2 < str.length()) {
                int value;
                std::stringstream ss(str.substr(i + 1, 2));
                ss >> std::hex >> value;
                result += static_cast<char>(value);
                i += 2;
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

std::unordered_map<std::string, std::string> AdvancedRuleEngineAPIHandlers::parse_query_params(const std::string& query_string) {
    std::unordered_map<std::string, std::string> params;

    if (query_string.empty()) {
        return params;
    }

    std::stringstream ss(query_string);
    std::string pair;

    while (std::getline(ss, pair, '&')) {
        size_t equals_pos = pair.find('=');
        if (equals_pos != std::string::npos) {
            std::string key = url_decode(pair.substr(0, equals_pos));
            std::string value = url_decode(pair.substr(equals_pos + 1));
            params[key] = value;
        }
    }

    return params;
}

std::vector<nlohmann::json> AdvancedRuleEngineAPIHandlers::get_recent_evaluations(int limit) {
    std::vector<nlohmann::json> evaluations;

    try {
        const char* query = R"(
            SELECT evaluation_id, rule_id, entity_id, score, triggered, action,
                   matched_conditions, condition_scores, processing_time_ms, evaluated_at
            FROM rule_evaluation_results
            ORDER BY evaluated_at DESC
            LIMIT $1
        )";

        const char* param_values[1] = {std::to_string(limit).c_str()};
        PGresult* pg_result = PQexecParams(db_conn_->get_connection(), query, 1, NULL, param_values, NULL, NULL, 0);

        if (PQresultStatus(pg_result) != PGRES_TUPLES_OK) {
            spdlog::error("Failed to fetch recent evaluations: {}", PQerrorMessage(db_conn_->get_connection()));
            PQclear(pg_result);
            return evaluations;
        }

        int num_rows = PQntuples(pg_result);
        for (int i = 0; i < num_rows; ++i) {
            nlohmann::json eval_json;
            eval_json["evaluation_id"] = PQgetvalue(pg_result, i, 0);
            eval_json["rule_id"] = PQgetvalue(pg_result, i, 1);
            eval_json["entity_id"] = PQgetvalue(pg_result, i, 2);
            eval_json["score"] = std::stod(PQgetvalue(pg_result, i, 3) ? PQgetvalue(pg_result, i, 3) : "0");
            eval_json["triggered"] = std::string(PQgetvalue(pg_result, i, 4)) == "t";

            std::string action_str = PQgetvalue(pg_result, i, 5) ? PQgetvalue(pg_result, i, 5) : "ALLOW";
            eval_json["action"] = action_str;

            if (PQgetvalue(pg_result, i, 6)) {
                eval_json["matched_conditions"] = nlohmann::json::parse(PQgetvalue(pg_result, i, 6));
            }

            if (PQgetvalue(pg_result, i, 7)) {
                eval_json["condition_scores"] = nlohmann::json::parse(PQgetvalue(pg_result, i, 7));
            }

            if (PQgetvalue(pg_result, i, 8)) {
                eval_json["processing_time_ms"] = std::stoi(PQgetvalue(pg_result, i, 8));
            }

            evaluations.push_back(eval_json);
        }

        PQclear(pg_result);

    } catch (const std::exception& e) {
        spdlog::error("Failed to get recent evaluations: {}", e.what());
    }

    return evaluations;
}

nlohmann::json AdvancedRuleEngineAPIHandlers::get_evaluation_summary(const std::string& time_range) {
    nlohmann::json summary;

    try {
        // Simplified summary - in production, implement proper time-based queries
        const char* query = R"(
            SELECT
                COUNT(*) as total_evaluations,
                COUNT(CASE WHEN triggered = true THEN 1 END) as triggered_evaluations,
                AVG(score) as avg_score,
                AVG(processing_time_ms) as avg_processing_time
            FROM rule_evaluation_results
            WHERE evaluated_at >= NOW() - INTERVAL '24 hours'
        )";

        PGresult* pg_result = PQexec(db_conn_->get_connection(), query);

        if (PQresultStatus(pg_result) == PGRES_TUPLES_OK && PQntuples(pg_result) > 0) {
            summary["total_evaluations"] = std::stoi(PQgetvalue(pg_result, 0, 0));
            summary["triggered_evaluations"] = std::stoi(PQgetvalue(pg_result, 0, 1));
            summary["avg_score"] = std::stod(PQgetvalue(pg_result, 0, 2) ? PQgetvalue(pg_result, 0, 2) : "0");
            summary["avg_processing_time_ms"] = std::stod(PQgetvalue(pg_result, 0, 3) ? PQgetvalue(pg_result, 0, 3) : "0");
        }

        PQclear(pg_result);

    } catch (const std::exception& e) {
        spdlog::error("Failed to get evaluation summary: {}", e.what());
    }

    return summary;
}

} // namespace regulens

