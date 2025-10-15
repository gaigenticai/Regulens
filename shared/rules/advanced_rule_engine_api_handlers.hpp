/**
 * Advanced Rule Engine API Handlers
 * REST API endpoints for fraud detection and rule management
 */

#ifndef ADVANCED_RULE_ENGINE_API_HANDLERS_HPP
#define ADVANCED_RULE_ENGINE_API_HANDLERS_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <nlohmann/json.hpp>
#include "advanced_rule_engine.hpp"
#include "../database/postgresql_connection.hpp"

namespace regulens {

class AdvancedRuleEngineAPIHandlers {
public:
    AdvancedRuleEngineAPIHandlers(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<AdvancedRuleEngine> rule_engine
    );

    ~AdvancedRuleEngineAPIHandlers();

    // Transaction evaluation endpoints
    std::string handle_evaluate_transaction(const std::string& request_body, const std::string& user_id);
    std::string handle_batch_evaluate_transactions(const std::string& request_body, const std::string& user_id);
    std::string handle_get_batch_results(const std::string& batch_id, const std::string& user_id);
    std::string handle_get_batch_progress(const std::string& batch_id, const std::string& user_id);

    // Rule management endpoints
    std::string handle_register_rule(const std::string& request_body, const std::string& user_id);
    std::string handle_get_rule(const std::string& rule_id, const std::string& user_id);
    std::string handle_update_rule(const std::string& rule_id, const std::string& request_body, const std::string& user_id);
    std::string handle_delete_rule(const std::string& rule_id, const std::string& user_id);
    std::string handle_list_rules(const std::string& query_params, const std::string& user_id);
    std::string handle_deactivate_rule(const std::string& rule_id, const std::string& user_id);

    // Rule execution and testing endpoints
    std::string handle_execute_rule(const std::string& rule_id, const std::string& request_body, const std::string& user_id);
    std::string handle_test_rule(const std::string& rule_id, const std::string& request_body, const std::string& user_id);
    std::string handle_validate_rule_logic(const std::string& request_body, const std::string& user_id);

    // Performance and analytics endpoints
    std::string handle_get_rule_metrics(const std::string& rule_id, const std::string& user_id);
    std::string handle_get_all_rule_metrics(const std::string& user_id);
    std::string handle_reset_rule_metrics(const std::string& rule_id, const std::string& user_id);
    std::string handle_get_fraud_detection_stats(const std::string& query_params, const std::string& user_id);

    // Configuration and management endpoints
    std::string handle_reload_rules(const std::string& user_id);
    std::string handle_get_engine_status(const std::string& user_id);
    std::string handle_update_engine_config(const std::string& request_body, const std::string& user_id);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<AdvancedRuleEngine> rule_engine_;

    // Batch processing state
    std::unordered_map<std::string, std::unordered_map<std::string, FraudDetectionResult>> batch_results_;
    std::unordered_map<std::string, double> batch_progress_;
    std::mutex batch_mutex_;

    // Helper methods
    RuleExecutionContext parse_transaction_context(const nlohmann::json& request);
    RuleDefinition parse_rule_definition(const nlohmann::json& request, const std::string& user_id);
    nlohmann::json format_rule_definition(const RuleDefinition& rule);
    nlohmann::json format_execution_result(const RuleExecutionResultDetail& result);
    nlohmann::json format_fraud_detection_result(const FraudDetectionResult& result);
    nlohmann::json format_rule_metrics(const RulePerformanceMetrics& metrics);

    // Query parameter parsing
    std::unordered_map<std::string, std::string> parse_query_params(const std::string& query_string);
    int parse_int_param(const std::string& value, int default_value = 0);
    bool parse_bool_param(const std::string& value, bool default_value = false);
    std::vector<std::string> parse_string_array_param(const std::string& value);

    // Request validation
    bool validate_transaction_request(const nlohmann::json& request, std::string& error_message);
    bool validate_rule_request(const nlohmann::json& request, std::string& error_message);
    bool validate_user_access(const std::string& user_id, const std::string& operation, const std::string& resource_id = "");
    bool is_admin_user(const std::string& user_id);

    // Response formatting
    nlohmann::json create_success_response(const nlohmann::json& data = nullptr,
                                         const std::string& message = "");
    nlohmann::json create_error_response(const std::string& message,
                                       int status_code = 400);
    nlohmann::json create_paginated_response(const std::vector<nlohmann::json>& items,
                                           int total_count,
                                           int page,
                                           int page_size);

    // Batch processing helpers
    std::string generate_batch_id();
    std::string generate_rule_identifier();
    void update_batch_progress(const std::string& batch_id, double progress);
    std::unordered_map<std::string, FraudDetectionResult> get_batch_results_safe(const std::string& batch_id);

    // Fraud detection stats helpers
    nlohmann::json get_fraud_detection_summary(const std::string& start_date = "", const std::string& end_date = "");
    std::vector<nlohmann::json> get_top_fraud_rules(int limit = 10);
    std::unordered_map<std::string, int> get_fraud_detection_by_risk_level();

    // Utility methods
    std::string rule_priority_to_string(RulePriority priority);
    RulePriority string_to_rule_priority(const std::string& priority_str);
    std::string execution_result_to_string(RuleExecutionResult result);
    std::string risk_level_to_string(FraudRiskLevel level);
    FraudRiskLevel string_to_risk_level(const std::string& level_str);
};

} // namespace regulens

#endif // ADVANCED_RULE_ENGINE_API_HANDLERS_HPP
