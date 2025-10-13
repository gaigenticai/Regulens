/**
 * Consensus Engine API Handlers
 * REST API endpoints for multi-agent decision making
 */

#ifndef CONSENSUS_ENGINE_API_HANDLERS_HPP
#define CONSENSUS_ENGINE_API_HANDLERS_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "consensus_engine.hpp"
#include "../../database/postgresql_connection.hpp"

namespace regulens {

class ConsensusEngineAPIHandlers {
public:
    ConsensusEngineAPIHandlers(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<ConsensusEngine> consensus_engine
    );

    ~ConsensusEngineAPIHandlers();

    // Consensus process management endpoints
    std::string handle_initiate_consensus(const std::string& request_body, const std::string& user_id);
    std::string handle_get_consensus(const std::string& consensus_id, const std::string& user_id);
    std::string handle_get_consensus_state(const std::string& consensus_id, const std::string& user_id);
    std::string handle_list_consensus_processes(const std::string& query_params, const std::string& user_id);

    // Agent opinion management endpoints
    std::string handle_submit_opinion(const std::string& consensus_id, const std::string& request_body, const std::string& user_id);
    std::string handle_get_opinions(const std::string& consensus_id, const std::string& query_params, const std::string& user_id);
    std::string handle_update_opinion(const std::string& consensus_id, const std::string& agent_id, const std::string& request_body, const std::string& user_id);

    // Voting management endpoints
    std::string handle_start_voting_round(const std::string& consensus_id, const std::string& user_id);
    std::string handle_end_voting_round(const std::string& consensus_id, const std::string& user_id);
    std::string handle_calculate_consensus(const std::string& consensus_id, const std::string& user_id);

    // Agent management endpoints
    std::string handle_register_agent(const std::string& request_body, const std::string& user_id);
    std::string handle_get_agent(const std::string& agent_id, const std::string& user_id);
    std::string handle_update_agent(const std::string& agent_id, const std::string& request_body, const std::string& user_id);
    std::string handle_list_agents(const std::string& query_params, const std::string& user_id);
    std::string handle_deactivate_agent(const std::string& agent_id, const std::string& user_id);

    // Conflict resolution endpoints
    std::string handle_identify_conflicts(const std::string& consensus_id, const std::string& user_id);
    std::string handle_suggest_resolution(const std::string& consensus_id, const std::string& user_id);
    std::string handle_resolve_conflict(const std::string& consensus_id, const std::string& request_body, const std::string& user_id);

    // Analytics and monitoring endpoints
    std::string handle_get_consensus_stats(const std::string& user_id);
    std::string handle_get_agent_performance(const std::string& user_id);
    std::string handle_get_decision_accuracy(const std::string& consensus_id, const std::string& query_params, const std::string& user_id);

    // Configuration endpoints
    std::string handle_get_engine_config(const std::string& user_id);
    std::string handle_update_engine_config(const std::string& request_body, const std::string& user_id);
    std::string handle_optimize_for_scenario(const std::string& scenario_type, const std::string& user_id);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<ConsensusEngine> consensus_engine_;

    // Helper methods
    ConsensusConfiguration parse_consensus_config(const nlohmann::json& request);
    Agent parse_agent_config(const nlohmann::json& request, const std::string& user_id);
    AgentOpinion parse_agent_opinion(const nlohmann::json& request, const std::string& agent_id);

    nlohmann::json format_consensus_config(const ConsensusConfiguration& config);
    nlohmann::json format_consensus_result(const ConsensusResult& result);
    nlohmann::json format_agent_opinion(const AgentOpinion& opinion);
    nlohmann::json format_voting_round(const VotingRound& round);
    nlohmann::json format_agent(const Agent& agent);

    // Query parameter parsing
    std::unordered_map<std::string, std::string> parse_query_params(const std::string& query_string);
    int parse_int_param(const std::string& value, int default_value = 0);
    bool parse_bool_param(const std::string& value, bool default_value = false);
    VotingAlgorithm parse_algorithm_param(const std::string& algorithm_str);
    AgentRole parse_role_param(const std::string& role_str);

    // Request validation
    bool validate_consensus_request(const nlohmann::json& request, std::string& error_message);
    bool validate_agent_request(const nlohmann::json& request, std::string& error_message);
    bool validate_opinion_request(const nlohmann::json& request, std::string& error_message);
    bool validate_user_access(const std::string& user_id, const std::string& operation, const std::string& resource_id = "");
    bool is_admin_user(const std::string& user_id);
    bool is_participant(const std::string& user_id, const std::string& consensus_id);

    // Response formatting
    nlohmann::json create_success_response(const nlohmann::json& data = nullptr,
                                         const std::string& message = "");
    nlohmann::json create_error_response(const std::string& message,
                                       int status_code = 400);
    nlohmann::json create_paginated_response(const std::vector<nlohmann::json>& items,
                                           int total_count,
                                           int page,
                                           int page_size);

    // Consensus state helpers
    nlohmann::json get_consensus_summary(const std::string& consensus_id);
    std::vector<nlohmann::json> get_recent_consensus_processes(int limit = 20);
    std::unordered_map<std::string, int> get_consensus_state_distribution();

    // Agent performance helpers
    std::vector<nlohmann::json> get_top_performing_agents(int limit = 10);
    nlohmann::json calculate_agent_metrics(const std::string& agent_id);
    double calculate_consensus_success_rate(const std::string& agent_id);

    // Utility methods
    std::string algorithm_to_string(VotingAlgorithm algorithm);
    std::string role_to_string(AgentRole role);
    std::string state_to_string(ConsensusState state);
    std::string confidence_to_string(DecisionConfidence confidence);
    VotingAlgorithm string_to_algorithm(const std::string& algorithm_str);
    AgentRole string_to_role(const std::string& role_str);
    ConsensusState string_to_state(const std::string& state_str);
    DecisionConfidence string_to_confidence(const std::string& confidence_str);

    // Permission checking
    bool can_modify_consensus(const std::string& user_id, const std::string& consensus_id);
    bool can_submit_opinion(const std::string& user_id, const std::string& consensus_id);
    bool owns_agent(const std::string& user_id, const std::string& agent_id);

    // Authorization helper functions
    bool check_user_permission(const std::string& user_id, const std::string& permission);
    bool check_user_role(const std::string& user_id, const std::string& role);
    std::string get_user_role(const std::string& user_id);
    bool check_consensus_participant(const std::string& user_id, const std::string& consensus_id);
};

} // namespace regulens

#endif // CONSENSUS_ENGINE_API_HANDLERS_HPP
