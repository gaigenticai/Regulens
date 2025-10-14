/**
 * Advanced Rule Engine API Handlers
 * REST API endpoints for rule engine management and evaluation
 */

#ifndef ADVANCED_RULE_ENGINE_API_HANDLERS_HPP
#define ADVANCED_RULE_ENGINE_API_HANDLERS_HPP

#include <memory>
#include <string>
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

    // Rule Management Endpoints
    std::string handle_create_rule(const std::string& request_body, const std::string& user_id);
    std::string handle_update_rule(const std::string& rule_id, const std::string& request_body, const std::string& user_id);
    std::string handle_delete_rule(const std::string& rule_id, const std::string& user_id);
    std::string handle_get_rule(const std::string& rule_id, const std::string& user_id);
    std::string handle_list_rules(const std::string& query_params, const std::string& user_id);
    std::string handle_enable_rule(const std::string& rule_id, const std::string& user_id);
    std::string handle_disable_rule(const std::string& rule_id, const std::string& user_id);

    // Rule Evaluation Endpoints
    std::string handle_evaluate_entity(const std::string& request_body, const std::string& user_id);
    std::string handle_evaluate_batch(const std::string& request_body, const std::string& user_id);

    // Analytics and Monitoring Endpoints
    std::string handle_get_performance_stats(const std::string& user_id);
    std::string handle_get_rule_stats(const std::string& rule_id, const std::string& user_id);
    std::string handle_get_evaluation_history(const std::string& query_params, const std::string& user_id);

    // Configuration Endpoints
    std::string handle_get_configuration(const std::string& user_id);
    std::string handle_update_configuration(const std::string& request_body, const std::string& user_id);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<AdvancedRuleEngine> rule_engine_;

    // Helper methods
    nlohmann::json parse_rule_definition(const nlohmann::json& request);
    nlohmann::json format_rule_definition(const RuleDefinition& rule);
    nlohmann::json format_rule_result(const RuleResult& result);
    nlohmann::json format_evaluation_batch(const EvaluationBatch& batch);

    nlohmann::json parse_evaluation_context(const nlohmann::json& request);
    std::vector<EvaluationContext> parse_evaluation_contexts(const nlohmann::json& request);

    bool validate_rule_request(const nlohmann::json& request, std::string& error_message);
    bool validate_evaluation_request(const nlohmann::json& request, std::string& error_message);
    bool validate_user_access(const std::string& user_id, const std::string& action);

    // Response formatting
    nlohmann::json create_success_response(const nlohmann::json& data, const std::string& message = "");
    nlohmann::json create_error_response(const std::string& message, int status_code = 400);

    // Utility methods
    std::string generate_request_id();
    std::string url_decode(const std::string& str);
    std::unordered_map<std::string, std::string> parse_query_params(const std::string& query_string);

    // Database operations for additional analytics
    std::vector<nlohmann::json> get_recent_evaluations(int limit = 100);
    nlohmann::json get_evaluation_summary(const std::string& time_range = "24h");
};

} // namespace regulens

#endif // ADVANCED_RULE_ENGINE_API_HANDLERS_HPP

