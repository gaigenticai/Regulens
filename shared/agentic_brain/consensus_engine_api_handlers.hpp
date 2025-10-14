#pragma once
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "consensus_engine.hpp"
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"

namespace regulens {

class ConsensusEngineAPIHandlers {
public:
    ConsensusEngineAPIHandlers(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<ConsensusEngine> consensus_engine,
        std::shared_ptr<StructuredLogger> logger
    );
    ~ConsensusEngineAPIHandlers();

    // Consensus session management endpoints
    std::string handle_initiate_consensus(const std::string& request_body, const std::string& user_id);
    std::string handle_get_consensus_result(const std::string& consensus_id, const std::string& user_id);
    std::string handle_get_consensus_state(const std::string& consensus_id, const std::string& user_id);

    // Agent opinion management endpoints
    std::string handle_submit_opinion(const std::string& consensus_id, const std::string& request_body, const std::string& user_id);
    std::string handle_get_agent_opinions(const std::string& consensus_id, const std::string& query_params, const std::string& user_id);
    std::string handle_update_opinion(const std::string& consensus_id, const std::string& agent_id, const std::string& request_body, const std::string& user_id);

    // Voting round management endpoints
    std::string handle_start_voting_round(const std::string& consensus_id, const std::string& user_id);
    std::string handle_end_voting_round(const std::string& consensus_id, const std::string& user_id);
    std::string handle_calculate_consensus(const std::string& consensus_id, const std::string& user_id);

    // Agent management endpoints
    std::string handle_register_agent(const std::string& request_body, const std::string& user_id);
    std::string handle_update_agent(const std::string& agent_id, const std::string& request_body, const std::string& user_id);
    std::string handle_get_agent(const std::string& agent_id, const std::string& user_id);
    std::string handle_get_active_agents(const std::string& user_id);
    std::string handle_deactivate_agent(const std::string& agent_id, const std::string& user_id);

    // Analytics and monitoring endpoints
    std::string handle_get_consensus_statistics(const std::string& user_id);
    std::string handle_get_agent_performance_metrics(const std::string& user_id);
    std::string handle_calculate_decision_accuracy(const std::string& consensus_id, const std::string& request_body, const std::string& user_id);

    // Configuration endpoints
    std::string handle_set_default_algorithm(const std::string& request_body, const std::string& user_id);
    std::string handle_set_max_rounds(const std::string& request_body, const std::string& user_id);
    std::string handle_set_timeout_per_round(const std::string& request_body, const std::string& user_id);
    std::string handle_optimize_for_scenario(const std::string& request_body, const std::string& user_id);

    // Conflict resolution endpoints
    std::string handle_identify_conflicts(const std::string& consensus_id, const std::string& user_id);
    std::string handle_suggest_resolution_strategies(const std::string& consensus_id, const std::string& user_id);
    std::string handle_resolve_conflict(const std::string& consensus_id, const std::string& request_body, const std::string& user_id);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<ConsensusEngine> consensus_engine_;
    std::shared_ptr<StructuredLogger> logger_;

    // Helper methods for request/response parsing and validation
    bool validate_consensus_config(const nlohmann::json& config_json, std::string& error_message);
    ConsensusConfiguration parse_consensus_config_from_json(const nlohmann::json& config_json);
    bool validate_agent_opinion(const nlohmann::json& opinion_json, std::string& error_message);
    AgentOpinion parse_agent_opinion_from_json(const nlohmann::json& opinion_json);
    bool validate_agent_data(const nlohmann::json& agent_json, std::string& error_message);
    Agent parse_agent_from_json(const nlohmann::json& agent_json);

    // Response formatting
    nlohmann::json format_consensus_result_to_json(const ConsensusResult& result);
    nlohmann::json format_agent_opinion_to_json(const AgentOpinion& opinion);
    nlohmann::json format_agent_to_json(const Agent& agent);
    nlohmann::json format_consensus_config_to_json(const ConsensusConfiguration& config);

    // Error handling
    nlohmann::json create_error_response(const std::string& message, int status_code = 500);
    nlohmann::json create_success_response(const nlohmann::json& data, const std::string& message = "Success");

    // Access control
    bool validate_user_access(const std::string& user_id, const std::string& permission_scope);
    bool can_participate_in_consensus(const std::string& user_id, const std::string& consensus_id);
};

} // namespace regulens