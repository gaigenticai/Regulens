/**
 * Advanced Rule Engine API Handlers Implementation
 * REST API endpoints for fraud detection and rule management
 */

#include "advanced_rule_engine_api_handlers.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <random>
#include <sstream>
#include <thread>

#include <spdlog/spdlog.h>

namespace regulens {

namespace {

int safe_string_to_int(const std::string& value, int default_value = 0) {
    if (value.empty()) {
        return default_value;
    }
    char* end = nullptr;
    long parsed = std::strtol(value.c_str(), &end, 10);
    if (end == value.c_str()) {
        return default_value;
    }
    return static_cast<int>(parsed);
}

double safe_string_to_double(const std::string& value, double default_value = 0.0) {
    if (value.empty()) {
        return default_value;
    }
    char* end = nullptr;
    double parsed = std::strtod(value.c_str(), &end);
    if (end == value.c_str()) {
        return default_value;
    }
    return parsed;
}

std::string json_string_value(const nlohmann::json& row,
                              const std::string& key,
                              const std::string& fallback = "") {
    if (!row.contains(key) || row[key].is_null()) {
        return fallback;
    }
    if (row[key].is_string()) {
        return row[key].get<std::string>();
    }
    if (row[key].is_number_integer()) {
        return std::to_string(row[key].get<long long>());
    }
    if (row[key].is_number_float()) {
        return std::to_string(row[key].get<double>());
    }
    return row[key].dump();
}

} // namespace

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
    spdlog::info("AdvancedRuleEngineAPIHandlers shutting down");
}

std::string AdvancedRuleEngineAPIHandlers::handle_evaluate_transaction(const std::string& request_body, const std::string& user_id) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);
        std::string validation_error;
        if (!validate_transaction_request(request, validation_error)) {
            return create_error_response(validation_error).dump();
        }

        if (!validate_user_access(user_id, "evaluate_transaction")) {
            return create_error_response("Access denied", 403).dump();
        }

        // Parse transaction context
        RuleExecutionContext context = parse_transaction_context(request);

        // Extract rule IDs if specified
        std::vector<std::string> rule_ids;
        if (request.contains("rule_ids") && request["rule_ids"].is_array()) {
            for (const auto& rule_id : request["rule_ids"]) {
                if (rule_id.is_string()) {
                    rule_ids.push_back(rule_id.get<std::string>());
                }
            }
        }

        // Evaluate transaction
        auto start_time = std::chrono::high_resolution_clock::now();
        FraudDetectionResult result = rule_engine_->evaluate_transaction(context, rule_ids);
        auto end_time = std::chrono::high_resolution_clock::now();

        // Add processing time
        auto processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        result.processing_duration = std::to_string(processing_time.count()) + "ms";

        nlohmann::json response_data = format_fraud_detection_result(result);
        response_data["processing_time_ms"] = processing_time.count();

        return create_success_response(response_data, "Transaction evaluated successfully").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_evaluate_transaction: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string AdvancedRuleEngineAPIHandlers::handle_batch_evaluate_transactions(const std::string& request_body, const std::string& user_id) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);

        if (!request.contains("transactions") || !request["transactions"].is_array()) {
            return create_error_response("Missing or invalid 'transactions' array").dump();
        }

        if (!validate_user_access(user_id, "batch_evaluate_transactions")) {
            return create_error_response("Access denied", 403).dump();
        }

        // Parse transactions
        std::vector<RuleExecutionContext> contexts;
        for (const auto& txn_request : request["transactions"]) {
            if (txn_request.contains("transaction_data")) {
                RuleExecutionContext context = parse_transaction_context(txn_request);
                contexts.push_back(context);
            }
        }

        if (contexts.empty()) {
            return create_error_response("No valid transactions found").dump();
        }

        // Extract rule IDs if specified
        std::vector<std::string> rule_ids;
        if (request.contains("rule_ids") && request["rule_ids"].is_array()) {
            for (const auto& rule_id : request["rule_ids"]) {
                if (rule_id.is_string()) {
                    rule_ids.push_back(rule_id.get<std::string>());
                }
            }
        }

        // Submit batch evaluation
        std::string batch_id = rule_engine_->submit_batch_evaluation(contexts, rule_ids);

        // Start background processing
        std::thread([this, batch_id, contexts, rule_ids]() {
            update_batch_progress(batch_id, 0.0);

            std::unordered_map<std::string, FraudDetectionResult> results;
            int processed = 0;

            for (const auto& context : contexts) {
                FraudDetectionResult result = rule_engine_->evaluate_transaction(context, rule_ids);
                results[context.transaction_id] = result;

                processed++;
                double progress = static_cast<double>(processed) / contexts.size();
                update_batch_progress(batch_id, progress);
            }

            // Store results
            std::lock_guard<std::mutex> lock(batch_mutex_);
            batch_results_[batch_id] = std::move(results);
            batch_progress_[batch_id] = 1.0;
        }).detach();

        nlohmann::json response_data = {
            {"batch_id", batch_id},
            {"total_transactions", contexts.size()},
            {"status", "processing"}
        };

        return create_success_response(response_data, "Batch evaluation started").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_batch_evaluate_transactions: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string AdvancedRuleEngineAPIHandlers::handle_get_batch_results(const std::string& batch_id, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "get_batch_results", batch_id)) {
            return create_error_response("Access denied", 403).dump();
        }

        auto results = get_batch_results_safe(batch_id);
        double progress = 0.0;

        {
            std::lock_guard<std::mutex> lock(batch_mutex_);
            auto progress_it = batch_progress_.find(batch_id);
            if (progress_it != batch_progress_.end()) {
                progress = progress_it->second;
            }
        }

        nlohmann::json response_data = {
            {"batch_id", batch_id},
            {"progress", progress},
            {"completed", progress >= 1.0}
        };

        if (!results.empty()) {
            nlohmann::json results_json;
            for (const auto& [txn_id, result] : results) {
                results_json[txn_id] = format_fraud_detection_result(result);
            }
            response_data["results"] = results_json;
            response_data["total_results"] = results.size();
        }

        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_batch_results: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string AdvancedRuleEngineAPIHandlers::handle_register_rule(const std::string& request_body, const std::string& user_id) {
    try {
        if (!is_admin_user(user_id)) {
            return create_error_response("Admin access required", 403).dump();
        }

        nlohmann::json request = nlohmann::json::parse(request_body);
        std::string validation_error;
        if (!validate_rule_request(request, validation_error)) {
            return create_error_response(validation_error).dump();
        }

        // Parse rule definition
        RuleDefinition rule = parse_rule_definition(request, user_id);

        // Register the rule
        if (!rule_engine_->register_rule(rule)) {
            return create_error_response("Failed to register rule").dump();
        }

        nlohmann::json response_data = format_rule_definition(rule);
        return create_success_response(response_data, "Rule registered successfully").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_register_rule: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string AdvancedRuleEngineAPIHandlers::handle_get_rule(const std::string& rule_id, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "get_rule", rule_id)) {
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

        auto params = parse_query_params(query_params);
        std::string rule_type = params["type"];
        bool active_only = parse_bool_param(params["active_only"], true);
        int limit = parse_int_param(params["limit"], 50);
        if (limit < 0) {
            limit = 0;
        }
        const std::size_t limit_size = static_cast<std::size_t>(limit);

        std::vector<RuleDefinition> rules;
        if (rule_type.empty()) {
            rules = rule_engine_->get_active_rules();
        } else {
            rules = rule_engine_->get_rules_by_type(rule_type);
        }

        // Filter by active status if requested
        if (active_only) {
            rules.erase(std::remove_if(rules.begin(), rules.end(),
                [](const RuleDefinition& rule) { return !rule.is_active; }), rules.end());
        }

        // Apply limit
        if (rules.size() > limit_size) {
            rules.resize(limit_size);
        }

        // Format response
        std::vector<nlohmann::json> formatted_rules;
        for (const auto& rule : rules) {
            formatted_rules.push_back(format_rule_definition(rule));
        }

        nlohmann::json response_data = create_paginated_response(
            formatted_rules,
            static_cast<int>(formatted_rules.size()),
            1,
            limit
        );

        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_list_rules: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string AdvancedRuleEngineAPIHandlers::handle_execute_rule(const std::string& rule_id, const std::string& request_body, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "execute_rule", rule_id)) {
            return create_error_response("Access denied", 403).dump();
        }

        // Get the rule
        auto rule_opt = rule_engine_->get_rule(rule_id);
        if (!rule_opt) {
            return create_error_response("Rule not found", 404).dump();
        }

        // Parse execution context
        nlohmann::json request = nlohmann::json::parse(request_body);
        RuleExecutionContext context = parse_transaction_context(request);

        // Execute the rule
        RuleExecutionResultDetail result = rule_engine_->execute_rule(*rule_opt, context);

        nlohmann::json response_data = format_execution_result(result);
        return create_success_response(response_data, "Rule executed successfully").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_execute_rule: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string AdvancedRuleEngineAPIHandlers::handle_get_rule_metrics(const std::string& rule_id, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "get_rule_metrics", rule_id)) {
            return create_error_response("Access denied", 403).dump();
        }

        RulePerformanceMetrics metrics = rule_engine_->get_rule_metrics(rule_id);

        if (metrics.total_executions == 0) {
            return create_error_response("No metrics found for rule", 404).dump();
        }

        nlohmann::json response_data = format_rule_metrics(metrics);
        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_rule_metrics: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string AdvancedRuleEngineAPIHandlers::handle_reload_rules(const std::string& user_id) {
    try {
        if (!is_admin_user(user_id)) {
            return create_error_response("Admin access required", 403).dump();
        }

        rule_engine_->reload_rules();

        return create_success_response(nullptr, "Rules reloaded successfully").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_reload_rules: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string AdvancedRuleEngineAPIHandlers::handle_get_fraud_detection_stats(const std::string& query_params, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "get_fraud_stats")) {
            return create_error_response("Access denied", 403).dump();
        }

        auto params = parse_query_params(query_params);
        std::string start_date = params["start_date"];
        std::string end_date = params["end_date"];

        // Get various statistics
        nlohmann::json summary = get_fraud_detection_summary(start_date, end_date);
        auto top_rules = get_top_fraud_rules(10);
        auto risk_distribution = get_fraud_detection_by_risk_level();

        nlohmann::json response_data = {
            {"summary", summary},
            {"top_fraud_rules", top_rules},
            {"risk_level_distribution", risk_distribution}
        };

        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_fraud_detection_stats: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

// Helper method implementations

RuleExecutionContext AdvancedRuleEngineAPIHandlers::parse_transaction_context(const nlohmann::json& request) {
    RuleExecutionContext context;

    context.transaction_id = request.value("transaction_id", generate_batch_id());
    context.user_id = request.value("user_id", "unknown");
    context.session_id = request.value("session_id", "");
    context.transaction_data = request.value("transaction_data", nlohmann::json::object());
    context.user_profile = request.value("user_profile", nlohmann::json::object());
    context.historical_data = request.value("historical_data", nlohmann::json::object());
    context.execution_time = std::chrono::system_clock::now();
    context.source_system = request.value("source_system", "api");

    if (request.contains("metadata") && request["metadata"].is_object()) {
        for (const auto& [key, value] : request["metadata"].items()) {
            if (value.is_string()) {
                context.metadata[key] = value.get<std::string>();
            }
        }
    }

    return context;
}

RuleDefinition AdvancedRuleEngineAPIHandlers::parse_rule_definition(const nlohmann::json& request, const std::string& user_id) {
    RuleDefinition rule;

    std::string provided_id = request.value("rule_id", "");
    if (provided_id.empty()) {
        rule.rule_id = generate_rule_identifier();
    } else {
        rule.rule_id = provided_id;
    }
    rule.name = request.value("name", "");
    rule.description = request.value("description", "");
    rule.priority = string_to_rule_priority(request.value("priority", "MEDIUM"));
    rule.rule_type = request.value("rule_type", "VALIDATION");
    rule.rule_logic = request.value("rule_logic", nlohmann::json::object());
    rule.parameters = request.value("parameters", nlohmann::json::object());
    rule.input_fields = request.value("input_fields", std::vector<std::string>{});
    rule.output_fields = request.value("output_fields", std::vector<std::string>{});
    rule.is_active = request.value("is_active", true);
    rule.created_by = user_id;
    rule.created_at = std::chrono::system_clock::now();
    rule.updated_at = std::chrono::system_clock::now();

    return rule;
}

nlohmann::json AdvancedRuleEngineAPIHandlers::format_rule_definition(const RuleDefinition& rule) {
    return {
        {"rule_id", rule.rule_id},
        {"name", rule.name},
        {"description", rule.description},
        {"priority", rule_priority_to_string(rule.priority)},
        {"rule_type", rule.rule_type},
        {"rule_logic", rule.rule_logic},
        {"parameters", rule.parameters},
        {"input_fields", rule.input_fields},
        {"output_fields", rule.output_fields},
        {"is_active", rule.is_active},
        {"created_by", rule.created_by},
        {"created_at", std::chrono::duration_cast<std::chrono::seconds>(
            rule.created_at.time_since_epoch()).count()},
        {"updated_at", std::chrono::duration_cast<std::chrono::seconds>(
            rule.updated_at.time_since_epoch()).count()}
    };
}

nlohmann::json AdvancedRuleEngineAPIHandlers::format_execution_result(const RuleExecutionResultDetail& result) {
    return {
        {"rule_id", result.rule_id},
        {"rule_name", result.rule_name},
        {"result", execution_result_to_string(result.result)},
        {"confidence_score", result.confidence_score},
        {"risk_level", risk_level_to_string(result.risk_level)},
        {"rule_output", result.rule_output},
        {"execution_time_ms", result.execution_time.count()},
        {"triggered_conditions", result.triggered_conditions},
        {"error_message", result.error_message}
    };
}

nlohmann::json AdvancedRuleEngineAPIHandlers::format_fraud_detection_result(const FraudDetectionResult& result) {
    nlohmann::json rule_results_json = nlohmann::json::array();
    for (const auto& rule_result : result.rule_results) {
        rule_results_json.push_back(format_execution_result(rule_result));
    }

    return {
        {"transaction_id", result.transaction_id},
        {"is_fraudulent", result.is_fraudulent},
        {"overall_risk", risk_level_to_string(result.overall_risk)},
        {"fraud_score", result.fraud_score},
        {"rule_results", rule_results_json},
        {"aggregated_findings", result.aggregated_findings},
        {"recommendation", result.recommendation},
        {"processing_duration", result.processing_duration},
        {"detection_time", std::chrono::duration_cast<std::chrono::seconds>(
            result.detection_time.time_since_epoch()).count()}
    };
}

bool AdvancedRuleEngineAPIHandlers::validate_transaction_request(const nlohmann::json& request, std::string& error_message) {
    if (!request.contains("transaction_data")) {
        error_message = "Missing 'transaction_data' field";
        return false;
    }

    if (!request["transaction_data"].is_object()) {
        error_message = "'transaction_data' must be an object";
        return false;
    }

    return true;
}

bool AdvancedRuleEngineAPIHandlers::validate_rule_request(const nlohmann::json& request, std::string& error_message) {
    if (!request.contains("name") || !request["name"].is_string()) {
        error_message = "Missing or invalid 'name' field";
        return false;
    }

    if (!request.contains("rule_type") || !request["rule_type"].is_string()) {
        error_message = "Missing or invalid 'rule_type' field";
        return false;
    }

    if (!request.contains("rule_logic")) {
        error_message = "Missing 'rule_logic' field";
        return false;
    }

    return true;
}

bool AdvancedRuleEngineAPIHandlers::validate_user_access(const std::string& user_id, const std::string& operation, const std::string& resource_id) {
    if (user_id.empty()) {
        spdlog::warn("Access denied: empty user_id");
        return false;
    }

    try {
        if (!db_conn_) {
            spdlog::error("Database connection not available for access validation");
            return false;
        }

        // Query user permissions from database
        std::string query = R"(
            SELECT p.operation, p.resource_type, p.resource_id, p.permission_level
            FROM user_permissions p
            INNER JOIN users u ON u.id = p.user_id
            WHERE u.user_id = $1 AND u.is_active = true AND p.is_active = true
        )";

        auto results = db_conn_->execute_query_multi(query, {user_id});

        // Check if user has permission for this operation
        for (const auto& row : results) {
            std::string perm_operation = row.at("operation");
            std::string perm_resource_id = row.at("resource_id");

            // Check for wildcard permission or exact match
            if (perm_operation == operation || perm_operation == "*") {
                // Check resource-specific permissions
                if (resource_id.empty() || perm_resource_id == resource_id || perm_resource_id == "*") {
                    spdlog::debug("Access granted for user: {} operation: {} resource: {}",
                                 user_id, operation, resource_id);
                    return true;
                }
            }
        }

        spdlog::warn("Access denied for user: {} operation: {} resource: {}",
                    user_id, operation, resource_id);
        return false;

    } catch (const std::exception& e) {
        spdlog::error("Access validation error: {}", e.what());
        return false; // Fail closed on errors
    }
}

bool AdvancedRuleEngineAPIHandlers::is_admin_user(const std::string& user_id) {
    if (user_id.empty()) {
        return false;
    }

    try {
        if (!db_conn_) {
            spdlog::error("Database connection not available for admin check");
            return false;
        }

        // Query user role from database
        std::string query = R"(
            SELECT r.role_name, r.role_level
            FROM user_roles ur
            INNER JOIN roles r ON r.id = ur.role_id
            INNER JOIN users u ON u.id = ur.user_id
            WHERE u.user_id = $1 AND ur.is_active = true AND u.is_active = true
            ORDER BY r.role_level DESC
            LIMIT 1
        )";

        auto results = db_conn_->execute_query_multi(query, {user_id});

        if (!results.empty()) {
            const auto& row = results[0];
            std::string role_name = json_string_value(row, "role_name");
            int role_level = safe_string_to_int(json_string_value(row, "role_level"));

            // Admin role has highest privilege level (level >= 90)
            // Or role name is explicitly "administrator" or "super_admin"
            if (role_name == "administrator" || role_name == "super_admin" || role_level >= 90) {
                spdlog::debug("Admin access confirmed for user: {} role: {}", user_id, role_name);
                return true;
            }
        }

        return false;

    } catch (const std::exception& e) {
        spdlog::error("Admin check error: {}", e.what());
        return false; // Fail closed on errors
    }
}

nlohmann::json AdvancedRuleEngineAPIHandlers::create_success_response(const nlohmann::json& data, const std::string& message) {
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

nlohmann::json AdvancedRuleEngineAPIHandlers::create_error_response(const std::string& message, int status_code) {
    return {
        {"success", false},
        {"status_code", status_code},
        {"error", message}
    };
}

std::string AdvancedRuleEngineAPIHandlers::generate_batch_id() {
    return "batch_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
}

std::string AdvancedRuleEngineAPIHandlers::generate_rule_identifier() {
    static thread_local std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<uint64_t> dist;
    auto timestamp = static_cast<unsigned long long>(std::chrono::system_clock::now().time_since_epoch().count());
    auto random_suffix = dist(rng);
    std::ostringstream oss;
    oss << "rule_" << timestamp << '_' << std::hex << random_suffix;
    return oss.str();
}

void AdvancedRuleEngineAPIHandlers::update_batch_progress(const std::string& batch_id, double progress) {
    std::lock_guard<std::mutex> lock(batch_mutex_);
    batch_progress_[batch_id] = progress;
}

std::unordered_map<std::string, FraudDetectionResult> AdvancedRuleEngineAPIHandlers::get_batch_results_safe(const std::string& batch_id) {
    std::lock_guard<std::mutex> lock(batch_mutex_);
    auto it = batch_results_.find(batch_id);
    return (it != batch_results_.end()) ? it->second : std::unordered_map<std::string, FraudDetectionResult>{};
}

nlohmann::json AdvancedRuleEngineAPIHandlers::get_fraud_detection_summary(const std::string& start_date, const std::string& end_date) {
    try {
        if (!db_conn_) {
            spdlog::error("Database connection not available");
            return {};
        }

        std::string query = R"(
            SELECT
                COUNT(*) as total_transactions,
                SUM(CASE WHEN is_fraudulent = true THEN 1 ELSE 0 END) as fraudulent_transactions,
                AVG(EXTRACT(EPOCH FROM (detection_time - created_at)) * 1000) as avg_processing_time_ms,
                MODE() WITHIN GROUP (ORDER BY fraud_type) FILTER (WHERE is_fraudulent = true) as most_common_fraud_type
            FROM fraud_detection_results
            WHERE detection_time >= $1::timestamp AND detection_time <= $2::timestamp
        )";

        auto results = db_conn_->execute_query_multi(query, {start_date, end_date});

        if (results.empty()) {
            return {
                {"total_transactions", 0},
                {"fraudulent_transactions", 0},
                {"fraud_rate", 0.0},
                {"average_processing_time_ms", 0.0},
                {"most_common_fraud_type", ""}
            };
        }

        const auto& row = results[0];
        int total = safe_string_to_int(json_string_value(row, "total_transactions"));
        int fraudulent = safe_string_to_int(json_string_value(row, "fraudulent_transactions"));
        double fraud_rate = total > 0 ? static_cast<double>(fraudulent) / total : 0.0;

        return {
            {"total_transactions", total},
            {"fraudulent_transactions", fraudulent},
            {"fraud_rate", fraud_rate},
            {"average_processing_time_ms", safe_string_to_double(json_string_value(row, "avg_processing_time_ms"))},
            {"most_common_fraud_type", json_string_value(row, "most_common_fraud_type")}
        };

    } catch (const std::exception& e) {
        spdlog::error("Failed to get fraud detection summary: {}", e.what());
        return {};
    }
}

std::vector<nlohmann::json> AdvancedRuleEngineAPIHandlers::get_top_fraud_rules(int limit) {
    try {
        if (!db_conn_) {
            spdlog::error("Database connection not available");
            return {};
        }

        std::string query = R"(
            SELECT
                r.rule_id,
                r.name as rule_name,
                COUNT(fdr.id) as fraud_detections,
                AVG(fdr.fraud_score) as avg_fraud_score
            FROM fraud_detection_rules r
            INNER JOIN fraud_detection_results fdr
                ON fdr.rule_results::jsonb @> jsonb_build_array(
                    jsonb_build_object('rule_id', r.rule_id, 'result', 'FAIL')
                )
            WHERE fdr.is_fraudulent = true
            GROUP BY r.rule_id, r.name
            ORDER BY fraud_detections DESC
            LIMIT $1
        )";

        auto results = db_conn_->execute_query_multi(query, {std::to_string(limit)});

        std::vector<nlohmann::json> top_rules;
        for (const auto& row : results) {
            top_rules.push_back({
                {"rule_id", json_string_value(row, "rule_id")},
                {"rule_name", json_string_value(row, "rule_name")},
                {"fraud_detections", safe_string_to_int(json_string_value(row, "fraud_detections"))},
                {"avg_fraud_score", safe_string_to_double(json_string_value(row, "avg_fraud_score"))}
            });
        }

        return top_rules;

    } catch (const std::exception& e) {
        spdlog::error("Failed to get top fraud rules: {}", e.what());
        return {};
    }
}

std::unordered_map<std::string, int> AdvancedRuleEngineAPIHandlers::get_fraud_detection_by_risk_level() {
    try {
        if (!db_conn_) {
            spdlog::error("Database connection not available");
            return {};
        }

        std::string query = R"(
            SELECT
                overall_risk as risk_level,
                COUNT(*) as count
            FROM fraud_detection_results
            WHERE is_fraudulent = true
            GROUP BY overall_risk
            ORDER BY
                CASE overall_risk
                    WHEN 'CRITICAL' THEN 1
                    WHEN 'HIGH' THEN 2
                    WHEN 'MEDIUM' THEN 3
                    WHEN 'LOW' THEN 4
                    ELSE 5
                END
        )";

        auto results = db_conn_->execute_query_multi(query, {});

        std::unordered_map<std::string, int> risk_distribution;
        for (const auto& row : results) {
            std::string risk_level = json_string_value(row, "risk_level");
            int count = safe_string_to_int(json_string_value(row, "count"));
            risk_distribution[risk_level] = count;
        }

        return risk_distribution;

    } catch (const std::exception& e) {
        spdlog::error("Failed to get fraud detection by risk level: {}", e.what());
        return {};
    }
}

// Utility string conversion methods
std::string AdvancedRuleEngineAPIHandlers::rule_priority_to_string(RulePriority priority) {
    switch (priority) {
        case RulePriority::LOW: return "LOW";
        case RulePriority::MEDIUM: return "MEDIUM";
        case RulePriority::HIGH: return "HIGH";
        case RulePriority::CRITICAL: return "CRITICAL";
        default: return "MEDIUM";
    }
}

RulePriority AdvancedRuleEngineAPIHandlers::string_to_rule_priority(const std::string& priority_str) {
    if (priority_str == "LOW") return RulePriority::LOW;
    if (priority_str == "HIGH") return RulePriority::HIGH;
    if (priority_str == "CRITICAL") return RulePriority::CRITICAL;
    return RulePriority::MEDIUM;
}

std::string AdvancedRuleEngineAPIHandlers::execution_result_to_string(RuleExecutionResult result) {
    switch (result) {
        case RuleExecutionResult::PASS: return "PASS";
        case RuleExecutionResult::FAIL: return "FAIL";
        case RuleExecutionResult::ERROR: return "ERROR";
        case RuleExecutionResult::TIMEOUT: return "TIMEOUT";
        case RuleExecutionResult::SKIPPED: return "SKIPPED";
        default: return "UNKNOWN";
    }
}

std::string AdvancedRuleEngineAPIHandlers::risk_level_to_string(FraudRiskLevel level) {
    switch (level) {
        case FraudRiskLevel::LOW: return "LOW";
        case FraudRiskLevel::MEDIUM: return "MEDIUM";
        case FraudRiskLevel::HIGH: return "HIGH";
        case FraudRiskLevel::CRITICAL: return "CRITICAL";
        default: return "LOW";
    }
}

FraudRiskLevel AdvancedRuleEngineAPIHandlers::string_to_risk_level(const std::string& level_str) {
    if (level_str == "MEDIUM") return FraudRiskLevel::MEDIUM;
    if (level_str == "HIGH") return FraudRiskLevel::HIGH;
    if (level_str == "CRITICAL") return FraudRiskLevel::CRITICAL;
    return FraudRiskLevel::LOW;
}

} // namespace regulens
