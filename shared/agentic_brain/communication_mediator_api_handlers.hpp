/**
 * Communication Mediator API Handlers
 * REST API endpoints for conversation orchestration and conflict resolution
 */

#ifndef COMMUNICATION_MEDIATOR_API_HANDLERS_HPP
#define COMMUNICATION_MEDIATOR_API_HANDLERS_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "communication_mediator.hpp"
#include "../../database/postgresql_connection.hpp"

namespace regulens {

class CommunicationMediatorAPIHandlers {
public:
    CommunicationMediatorAPIHandlers(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<CommunicationMediator> mediator
    );

    ~CommunicationMediatorAPIHandlers();

    // Conversation management endpoints
    std::string handle_initiate_conversation(const std::string& request_body, const std::string& user_id);
    std::string handle_get_conversation(const std::string& conversation_id, const std::string& user_id);
    std::string handle_get_conversation_state(const std::string& conversation_id, const std::string& user_id);
    std::string handle_end_conversation(const std::string& conversation_id, const std::string& request_body, const std::string& user_id);
    std::string handle_list_conversations(const std::string& query_params, const std::string& user_id);

    // Message handling endpoints
    std::string handle_send_message(const std::string& conversation_id, const std::string& request_body, const std::string& user_id);
    std::string handle_broadcast_message(const std::string& conversation_id, const std::string& request_body, const std::string& user_id);
    std::string handle_get_pending_messages(const std::string& user_id);
    std::string handle_acknowledge_message(const std::string& message_id, const std::string& user_id);

    // Participant management endpoints
    std::string handle_add_participant(const std::string& conversation_id, const std::string& request_body, const std::string& user_id);
    std::string handle_remove_participant(const std::string& conversation_id, const std::string& agent_id, const std::string& user_id);
    std::string handle_get_participants(const std::string& conversation_id, const std::string& user_id);

    // Conflict resolution endpoints
    std::string handle_detect_conflicts(const std::string& conversation_id, const std::string& user_id);
    std::string handle_resolve_conflict(const std::string& conversation_id, const std::string& request_body, const std::string& user_id);
    std::string handle_mediate_conversation(const std::string& conversation_id, const std::string& user_id);

    // Advanced orchestration endpoints
    std::string handle_orchestrate_turn_taking(const std::string& conversation_id, const std::string& user_id);
    std::string handle_facilitate_discussion(const std::string& conversation_id, const std::string& request_body, const std::string& user_id);
    std::string handle_coordinate_task(const std::string& conversation_id, const std::string& request_body, const std::string& user_id);

    // Analytics and monitoring endpoints
    std::string handle_get_conversation_stats(const std::string& user_id);
    std::string handle_get_agent_participation_metrics(const std::string& user_id);
    std::string handle_get_conflict_resolution_stats(const std::string& user_id);
    std::string handle_get_conversation_effectiveness(const std::string& conversation_id, const std::string& user_id);

    // Configuration endpoints
    std::string handle_get_mediator_config(const std::string& user_id);
    std::string handle_update_mediator_config(const std::string& request_body, const std::string& user_id);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<CommunicationMediator> mediator_;

    // Helper methods
    ConversationContext parse_conversation_context(const nlohmann::json& request);
    ConversationMessage parse_conversation_message(const nlohmann::json& request, const std::string& conversation_id, const std::string& sender_id);
    ConversationParticipant parse_conversation_participant(const nlohmann::json& request);

    nlohmann::json format_conversation_context(const ConversationContext& context);
    nlohmann::json format_conversation_message(const ConversationMessage& message);
    nlohmann::json format_conversation_participant(const ConversationParticipant& participant);
    nlohmann::json format_conflict_resolution(const ConflictResolution& resolution);
    nlohmann::json format_mediation_result(const MediationResult& result);

    // Request validation
    bool validate_conversation_request(const nlohmann::json& request, std::string& error_message);
    bool validate_message_request(const nlohmann::json& request, std::string& error_message);
    bool validate_participant_request(const nlohmann::json& request, std::string& error_message);
    bool validate_user_access(const std::string& user_id, const std::string& operation, const std::string& resource_id = "");
    bool is_admin_user(const std::string& user_id);
    bool is_conversation_participant(const std::string& user_id, const std::string& conversation_id);
    bool is_conversation_facilitator(const std::string& user_id, const std::string& conversation_id);

    // Response formatting
    nlohmann::json create_success_response(const nlohmann::json& data = nullptr,
                                         const std::string& message = "");
    nlohmann::json create_error_response(const std::string& message,
                                       int status_code = 400);
    nlohmann::json create_paginated_response(const std::vector<nlohmann::json>& items,
                                           int total_count,
                                           int page,
                                           int page_size);

    // Query parameter parsing
    std::unordered_map<std::string, std::string> parse_query_params(const std::string& query_string);
    ConversationState parse_state_param(const std::string& state_str);
    ResolutionStrategy parse_resolution_strategy(const std::string& strategy_str);
    int parse_int_param(const std::string& value, int default_value = 0);
    bool parse_bool_param(const std::string& value, bool default_value = false);

    // Conversation utilities
    std::vector<nlohmann::json> get_recent_conversations(int limit = 20);
    nlohmann::json get_conversation_summary(const std::string& conversation_id);
    std::vector<nlohmann::json> get_conversation_participants(const std::string& conversation_id);
    std::vector<nlohmann::json> get_conversation_messages(const std::string& conversation_id, int limit = 50);

    // Conflict resolution helpers
    std::vector<nlohmann::json> format_conflict_list(const std::vector<ConflictResolution>& conflicts);
    nlohmann::json create_resolution_request(const std::string& conflict_id, ResolutionStrategy strategy);
    bool validate_resolution_strategy(ResolutionStrategy strategy, const std::string& conversation_id);

    // Performance helpers
    nlohmann::json format_conversation_stats(const std::unordered_map<std::string, int>& stats);
    nlohmann::json format_agent_metrics(const std::vector<std::pair<std::string, double>>& metrics);
    nlohmann::json format_conflict_stats(const std::vector<std::pair<std::string, int>>& stats);
    nlohmann::json calculate_effectiveness_metrics(const std::string& conversation_id);

    // Configuration helpers
    nlohmann::json get_mediator_configuration();
    bool update_mediator_configuration(const nlohmann::json& config);

    // Message queue helpers
    std::vector<nlohmann::json> get_agent_message_queue(const std::string& agent_id, int limit = 20);
    bool process_message_acknowledgment(const std::string& message_id, const std::string& agent_id);

    // Utility methods
    std::string conversation_state_to_string(ConversationState state);
    std::string conflict_type_to_string(ConflictType type);
    std::string resolution_strategy_to_string(ResolutionStrategy strategy);
    ConversationState string_to_conversation_state(const std::string& state_str);
    ConflictType string_to_conflict_type(const std::string& type_str);
    ResolutionStrategy string_to_resolution_strategy(const std::string& strategy_str);

    // Permission checking
    bool can_initiate_conversation(const std::string& user_id);
    bool can_manage_conversation(const std::string& user_id, const std::string& conversation_id);
    bool can_send_message(const std::string& user_id, const std::string& conversation_id);
    bool can_resolve_conflicts(const std::string& user_id, const std::string& conversation_id);

    // Error handling
    std::string create_validation_error(const std::string& field, const std::string& reason);
    std::string create_permission_error(const std::string& operation, const std::string& resource);
    std::string create_conversation_error(const std::string& conversation_id, const std::string& reason);
    std::string create_participant_error(const std::string& agent_id, const std::string& reason);

    // Logging helpers
    void log_conversation_action(const std::string& user_id, const std::string& action,
                               const std::string& conversation_id);
    void log_message_action(const std::string& user_id, const std::string& action,
                          const std::string& message_id);
    void log_conflict_action(const std::string& user_id, const std::string& action,
                           const std::string& conflict_id);
};

} // namespace regulens

#endif // COMMUNICATION_MEDIATOR_API_HANDLERS_HPP
