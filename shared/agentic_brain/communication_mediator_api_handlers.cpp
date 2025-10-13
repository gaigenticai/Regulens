/**
 * Communication Mediator API Handlers Implementation
 * REST API endpoints for conversation orchestration and conflict resolution
 */

#include "communication_mediator_api_handlers.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace regulens {

CommunicationMediatorAPIHandlers::CommunicationMediatorAPIHandlers(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<CommunicationMediator> mediator
) : db_conn_(db_conn), mediator_(mediator) {

    if (!db_conn_) {
        throw std::runtime_error("Database connection is required for CommunicationMediatorAPIHandlers");
    }
    if (!mediator_) {
        throw std::runtime_error("CommunicationMediator is required for CommunicationMediatorAPIHandlers");
    }

    spdlog::info("CommunicationMediatorAPIHandlers initialized");
}

CommunicationMediatorAPIHandlers::~CommunicationMediatorAPIHandlers() {
    spdlog::info("CommunicationMediatorAPIHandlers shutting down");
}

std::string CommunicationMediatorAPIHandlers::handle_initiate_conversation(const std::string& request_body, const std::string& user_id) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);
        std::string validation_error;
        if (!validate_conversation_request(request, validation_error)) {
            return create_error_response(validation_error).dump();
        }

        if (!can_initiate_conversation(user_id)) {
            return create_error_response("Access denied - cannot initiate conversations", 403).dump();
        }

        // Parse conversation parameters
        std::string topic = request.value("topic", "");
        std::string objective = request.value("objective", "");
        std::vector<std::string> participant_ids;

        if (request.contains("participants") && request["participants"].is_array()) {
            for (const auto& participant : request["participants"]) {
                if (participant.is_string()) {
                    participant_ids.push_back(participant.get<std::string>());
                } else if (participant.is_object() && participant.contains("agent_id")) {
                    participant_ids.push_back(participant["agent_id"].get<std::string>());
                }
            }
        }

        // Add initiator as participant if not already included
        if (std::find(participant_ids.begin(), participant_ids.end(), user_id) == participant_ids.end()) {
            participant_ids.push_back(user_id);
        }

        // Initiate conversation
        std::string conversation_id = mediator_->initiate_conversation(topic, objective, participant_ids);

        if (conversation_id.empty()) {
            return create_error_response("Failed to initiate conversation").dump();
        }

        log_conversation_action(user_id, "initiate", conversation_id);

        nlohmann::json response_data = {
            {"conversation_id", conversation_id},
            {"topic", topic},
            {"objective", objective},
            {"participants_count", participant_ids.size()},
            {"participants", participant_ids},
            {"status", "initialized"}
        };

        return create_success_response(response_data, "Conversation initiated successfully").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_initiate_conversation: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string CommunicationMediatorAPIHandlers::handle_get_conversation(const std::string& conversation_id, const std::string& user_id) {
    try {
        if (!is_conversation_participant(user_id, conversation_id)) {
            return create_error_response("Access denied - not a conversation participant", 403).dump();
        }

        ConversationContext context = mediator_->get_conversation_context(conversation_id);

        if (context.conversation_id.empty()) {
            return create_error_response("Conversation not found", 404).dump();
        }

        nlohmann::json response_data = format_conversation_context(context);
        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_conversation: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string CommunicationMediatorAPIHandlers::handle_send_message(const std::string& conversation_id, const std::string& request_body, const std::string& user_id) {
    try {
        if (!can_send_message(user_id, conversation_id)) {
            return create_error_response("Access denied - cannot send messages to this conversation", 403).dump();
        }

        nlohmann::json request = nlohmann::json::parse(request_body);
        std::string validation_error;
        if (!validate_message_request(request, validation_error)) {
            return create_error_response(validation_error).dump();
        }

        // Parse message
        ConversationMessage message = parse_conversation_message(request, conversation_id, user_id);

        // Send message
        if (!mediator_->send_message(message)) {
            return create_error_response("Failed to send message").dump();
        }

        log_message_action(user_id, "send", message.message_id);

        nlohmann::json response_data = format_conversation_message(message);
        response_data["status"] = "sent";

        return create_success_response(response_data, "Message sent successfully").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_send_message: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string CommunicationMediatorAPIHandlers::handle_broadcast_message(const std::string& conversation_id, const std::string& request_body, const std::string& user_id) {
    try {
        if (!is_conversation_participant(user_id, conversation_id)) {
            return create_error_response("Access denied - not a conversation participant", 403).dump();
        }

        nlohmann::json request = nlohmann::json::parse(request_body);
        nlohmann::json content = request.value("content", nlohmann::json::object());
        std::string message_type = request.value("message_type", "notification");

        // Broadcast message
        if (!mediator_->broadcast_message(conversation_id, user_id, content, message_type)) {
            return create_error_response("Failed to broadcast message").dump();
        }

        nlohmann::json response_data = {
            {"conversation_id", conversation_id},
            {"sender_id", user_id},
            {"message_type", message_type},
            {"content", content},
            {"broadcast", true},
            {"status", "sent"}
        };

        return create_success_response(response_data, "Message broadcast successfully").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_broadcast_message: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string CommunicationMediatorAPIHandlers::handle_get_pending_messages(const std::string& user_id) {
    try {
        auto pending_messages = mediator_->get_pending_messages(user_id);

        // Format messages
        std::vector<nlohmann::json> formatted_messages;
        for (const auto& message : pending_messages) {
            formatted_messages.push_back(format_conversation_message(message));
        }

        nlohmann::json response_data = create_paginated_response(
            formatted_messages,
            formatted_messages.size(), // Simplified
            1,
            formatted_messages.size()
        );

        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_pending_messages: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string CommunicationMediatorAPIHandlers::handle_detect_conflicts(const std::string& conversation_id, const std::string& user_id) {
    try {
        if (!is_conversation_participant(user_id, conversation_id)) {
            return create_error_response("Access denied - not a conversation participant", 403).dump();
        }

        auto conflicts = mediator_->detect_conflicts(conversation_id);

        nlohmann::json response_data = {
            {"conversation_id", conversation_id},
            {"conflicts_detected", conflicts.size()},
            {"conflicts", format_conflict_list(conflicts)}
        };

        return create_success_response(response_data, "Conflict detection completed").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_detect_conflicts: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string CommunicationMediatorAPIHandlers::handle_resolve_conflict(const std::string& conversation_id, const std::string& request_body, const std::string& user_id) {
    try {
        if (!can_resolve_conflicts(user_id, conversation_id)) {
            return create_error_response("Access denied - cannot resolve conflicts", 403).dump();
        }

        nlohmann::json request = nlohmann::json::parse(request_body);
        std::string conflict_id = request.value("conflict_id", "");
        std::string strategy_str = request.value("strategy", "MAJORITY_VOTE");

        if (conflict_id.empty()) {
            return create_error_response("Conflict ID is required").dump();
        }

        ResolutionStrategy strategy = parse_resolution_strategy(strategy_str);

        if (!validate_resolution_strategy(strategy, conversation_id)) {
            return create_error_response("Invalid resolution strategy for this conversation").dump();
        }

        // Resolve conflict
        MediationResult result = mediator_->resolve_conflict(conversation_id, conflict_id, strategy);

        log_conflict_action(user_id, "resolve", conflict_id);

        nlohmann::json response_data = format_mediation_result(result);
        response_data["conflict_id"] = conflict_id;
        response_data["strategy_used"] = strategy_str;

        return create_success_response(response_data,
            result.success ? "Conflict resolved successfully" : "Conflict resolution failed").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_resolve_conflict: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string CommunicationMediatorAPIHandlers::handle_mediate_conversation(const std::string& conversation_id, const std::string& user_id) {
    try {
        if (!is_conversation_facilitator(user_id, conversation_id)) {
            return create_error_response("Access denied - not a conversation facilitator", 403).dump();
        }

        MediationResult result = mediator_->mediate_conversation(conversation_id);

        nlohmann::json response_data = format_mediation_result(result);
        response_data["conversation_id"] = conversation_id;
        response_data["action"] = "mediate_conversation";

        return create_success_response(response_data,
            result.success ? "Conversation mediation completed" : "Conversation mediation failed").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_mediate_conversation: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string CommunicationMediatorAPIHandlers::handle_get_conversation_stats(const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "get_conversation_stats")) {
            return create_error_response("Access denied", 403).dump();
        }

        auto stats = mediator_->get_conversation_stats();
        auto agent_metrics = mediator_->get_agent_participation_metrics();
        auto conflict_stats = mediator_->get_conflict_resolution_stats();

        nlohmann::json response_data = {
            {"conversation_stats", format_conversation_stats(stats)},
            {"agent_participation", format_agent_metrics(agent_metrics)},
            {"conflict_resolution", format_conflict_stats(conflict_stats)},
            {"generated_at", "2024-01-15T10:30:00Z"}
        };

        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_conversation_stats: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string CommunicationMediatorAPIHandlers::handle_add_participant(const std::string& conversation_id, const std::string& request_body, const std::string& user_id) {
    try {
        if (!can_manage_conversation(user_id, conversation_id)) {
            return create_error_response("Access denied - cannot manage conversation participants", 403).dump();
        }

        nlohmann::json request = nlohmann::json::parse(request_body);
        std::string agent_id = request.value("agent_id", "");
        std::string role = request.value("role", "participant");

        if (agent_id.empty()) {
            return create_error_response("Agent ID is required").dump();
        }

        if (!mediator_->add_participant(conversation_id, agent_id, role)) {
            return create_error_response("Failed to add participant").dump();
        }

        nlohmann::json response_data = {
            {"conversation_id", conversation_id},
            {"agent_id", agent_id},
            {"role", role},
            {"status", "added"}
        };

        return create_success_response(response_data, "Participant added successfully").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_add_participant: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string CommunicationMediatorAPIHandlers::handle_facilitate_discussion(const std::string& conversation_id, const std::string& request_body, const std::string& user_id) {
    try {
        if (!is_conversation_facilitator(user_id, conversation_id)) {
            return create_error_response("Access denied - not a conversation facilitator", 403).dump();
        }

        nlohmann::json request = nlohmann::json::parse(request_body);
        std::string discussion_topic = request.value("discussion_topic", "");

        if (!mediator_->facilitate_discussion(conversation_id, discussion_topic)) {
            return create_error_response("Failed to facilitate discussion").dump();
        }

        nlohmann::json response_data = {
            {"conversation_id", conversation_id},
            {"discussion_topic", discussion_topic},
            {"facilitator", user_id},
            {"status", "discussion_facilitated"}
        };

        return create_success_response(response_data, "Discussion facilitation started").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_facilitate_discussion: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

// Helper method implementations

ConversationMessage CommunicationMediatorAPIHandlers::parse_conversation_message(const nlohmann::json& request,
                                                                               const std::string& conversation_id,
                                                                               const std::string& sender_id) {
    ConversationMessage message;

    message.message_id = mediator_->generate_message_id();
    message.conversation_id = conversation_id;
    message.sender_agent_id = sender_id;
    message.recipient_agent_id = request.value("recipient_agent_id", "all");
    message.message_type = request.value("message_type", "message");
    message.content = request.value("content", nlohmann::json::object());
    message.sent_at = std::chrono::system_clock::now();

    if (request.contains("metadata") && request["metadata"].is_object()) {
        for (const auto& [key, value] : request["metadata"].items()) {
            if (value.is_string()) {
                message.metadata[key] = value.get<std::string>();
            }
        }
    }

    return message;
}

nlohmann::json CommunicationMediatorAPIHandlers::format_conversation_context(const ConversationContext& context) {
    nlohmann::json participants_json;
    for (const auto& participant : context.participants) {
        participants_json.push_back(format_conversation_participant(participant));
    }

    nlohmann::json messages_json;
    for (const auto& message : context.message_history) {
        messages_json.push_back(format_conversation_message(message));
    }

    nlohmann::json conflicts_json;
    for (const auto& conflict : context.conflicts) {
        conflicts_json.push_back(format_conflict_resolution(conflict));
    }

    return {
        {"conversation_id", context.conversation_id},
        {"topic", context.topic},
        {"objective", context.objective},
        {"state", conversation_state_to_string(context.state)},
        {"participants", participants_json},
        {"message_history", messages_json},
        {"conflicts", conflicts_json},
        {"started_at", std::chrono::duration_cast<std::chrono::seconds>(
            context.started_at.time_since_epoch()).count()},
        {"last_activity", std::chrono::duration_cast<std::chrono::seconds>(
            context.last_activity.time_since_epoch()).count()},
        {"timeout_duration_minutes", context.timeout_duration.count()}
    };
}

nlohmann::json CommunicationMediatorAPIHandlers::format_conversation_message(const ConversationMessage& message) {
    return {
        {"message_id", message.message_id},
        {"conversation_id", message.conversation_id},
        {"sender_agent_id", message.sender_agent_id},
        {"recipient_agent_id", message.recipient_agent_id},
        {"message_type", message.message_type},
        {"content", message.content},
        {"sent_at", std::chrono::duration_cast<std::chrono::seconds>(
            message.sent_at.time_since_epoch()).count()},
        {"acknowledged", message.acknowledged},
        {"metadata", message.metadata}
    };
}

nlohmann::json CommunicationMediatorAPIHandlers::format_conversation_participant(const ConversationParticipant& participant) {
    return {
        {"agent_id", participant.agent_id},
        {"role", participant.role},
        {"expertise_weight", participant.expertise_weight},
        {"domain_specialty", participant.domain_specialty},
        {"is_active", participant.is_active},
        {"joined_at", std::chrono::duration_cast<std::chrono::seconds>(
            participant.joined_at.time_since_epoch()).count()},
        {"last_active", std::chrono::duration_cast<std::chrono::seconds>(
            participant.last_active.time_since_epoch()).count()},
        {"messages_sent", participant.messages_sent},
        {"responses_received", participant.responses_received}
    };
}

nlohmann::json CommunicationMediatorAPIHandlers::format_conflict_resolution(const ConflictResolution& resolution) {
    return {
        {"conflict_id", resolution.conflict_id},
        {"conversation_id", resolution.conversation_id},
        {"conflict_type", conflict_type_to_string(resolution.conflict_type)},
        {"description", resolution.description},
        {"involved_agents", resolution.involved_agents},
        {"strategy_used", resolution_strategy_to_string(resolution.strategy_used)},
        {"conflict_details", resolution.conflict_details},
        {"resolution_result", resolution.resolution_result},
        {"detected_at", std::chrono::duration_cast<std::chrono::seconds>(
            resolution.detected_at.time_since_epoch()).count()},
        {"resolved_at", resolution.resolved_at.time_since_epoch().count() > 0 ?
            std::chrono::duration_cast<std::chrono::seconds>(
                resolution.resolved_at.time_since_epoch()).count() : 0},
        {"resolved_successfully", resolution.resolved_successfully},
        {"resolution_summary", resolution.resolution_summary}
    };
}

nlohmann::json CommunicationMediatorAPIHandlers::format_mediation_result(const MediationResult& result) {
    nlohmann::json messages_json;
    for (const auto& message : result.mediation_messages) {
        messages_json.push_back(format_conversation_message(message));
    }

    return {
        {"success", result.success},
        {"resolution", result.resolution},
        {"new_state", conversation_state_to_string(result.new_state)},
        {"mediation_messages", messages_json},
        {"processing_time_ms", result.processing_time.count()},
        {"mediation_details", result.mediation_details}
    };
}

bool CommunicationMediatorAPIHandlers::validate_conversation_request(const nlohmann::json& request, std::string& error_message) {
    if (!request.contains("topic") || !request["topic"].is_string() || request["topic"].get<std::string>().empty()) {
        error_message = "Missing or invalid 'topic' field";
        return false;
    }

    if (!request.contains("objective") || !request["objective"].is_string() || request["objective"].get<std::string>().empty()) {
        error_message = "Missing or invalid 'objective' field";
        return false;
    }

    if (!request.contains("participants") || !request["participants"].is_array() || request["participants"].empty()) {
        error_message = "Missing or invalid 'participants' array - must contain at least one participant";
        return false;
    }

    return true;
}

bool CommunicationMediatorAPIHandlers::validate_message_request(const nlohmann::json& request, std::string& error_message) {
    if (!request.contains("content")) {
        error_message = "Missing 'content' field";
        return false;
    }

    return true;
}

bool CommunicationMediatorAPIHandlers::validate_user_access(const std::string& user_id, const std::string& operation, const std::string& resource_id) {
    // TODO: Implement proper access control based on user roles and permissions
    return !user_id.empty();
}

bool CommunicationMediatorAPIHandlers::is_admin_user(const std::string& user_id) {
    // TODO: Implement proper admin user checking
    return user_id == "admin" || user_id == "system";
}

bool CommunicationMediatorAPIHandlers::is_conversation_participant(const std::string& user_id, const std::string& conversation_id) {
    // TODO: Check if user is a registered participant in the conversation
    return !user_id.empty();
}

bool CommunicationMediatorAPIHandlers::is_conversation_facilitator(const std::string& user_id, const std::string& conversation_id) {
    // TODO: Check if user has facilitator role in the conversation
    return is_admin_user(user_id);
}

bool CommunicationMediatorAPIHandlers::can_initiate_conversation(const std::string& user_id) {
    return validate_user_access(user_id, "initiate_conversation");
}

bool CommunicationMediatorAPIHandlers::can_manage_conversation(const std::string& user_id, const std::string& conversation_id) {
    return is_conversation_facilitator(user_id, conversation_id) || is_admin_user(user_id);
}

bool CommunicationMediatorAPIHandlers::can_send_message(const std::string& user_id, const std::string& conversation_id) {
    return is_conversation_participant(user_id, conversation_id);
}

bool CommunicationMediatorAPIHandlers::can_resolve_conflicts(const std::string& user_id, const std::string& conversation_id) {
    return can_manage_conversation(user_id, conversation_id);
}

nlohmann::json CommunicationMediatorAPIHandlers::create_success_response(const nlohmann::json& data, const std::string& message) {
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

nlohmann::json CommunicationMediatorAPIHandlers::create_error_response(const std::string& message, int status_code) {
    return {
        {"success", false},
        {"status_code", status_code},
        {"error", message}
    };
}

nlohmann::json CommunicationMediatorAPIHandlers::create_paginated_response(const std::vector<nlohmann::json>& items, int total_count, int page, int page_size) {
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

std::unordered_map<std::string, std::string> CommunicationMediatorAPIHandlers::parse_query_params(const std::string& query_string) {
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

ResolutionStrategy CommunicationMediatorAPIHandlers::parse_resolution_strategy(const std::string& strategy_str) {
    if (strategy_str == "MAJORITY_VOTE") return ResolutionStrategy::MAJORITY_VOTE;
    if (strategy_str == "WEIGHTED_VOTE") return ResolutionStrategy::WEIGHTED_VOTE;
    if (strategy_str == "EXPERT_ARBITRATION") return ResolutionStrategy::EXPERT_ARBITRATION;
    if (strategy_str == "COMPROMISE_NEGOTIATION") return ResolutionStrategy::COMPROMISE_NEGOTIATION;
    if (strategy_str == "ESCALATION") return ResolutionStrategy::ESCALATION;
    if (strategy_str == "EXTERNAL_MEDIATION") return ResolutionStrategy::EXTERNAL_MEDiation;
    if (strategy_str == "TIMEOUT_ABORT") return ResolutionStrategy::TIMEOUT_ABORT;
    if (strategy_str == "MANUAL_OVERRIDE") return ResolutionStrategy::MANUAL_OVERRIDE;
    return ResolutionStrategy::MAJORITY_VOTE;
}

std::vector<nlohmann::json> CommunicationMediatorAPIHandlers::format_conflict_list(const std::vector<ConflictResolution>& conflicts) {
    std::vector<nlohmann::json> formatted_conflicts;
    for (const auto& conflict : conflicts) {
        formatted_conflicts.push_back(format_conflict_resolution(conflict));
    }
    return formatted_conflicts;
}

bool CommunicationMediatorAPIHandlers::validate_resolution_strategy(ResolutionStrategy strategy, const std::string& conversation_id) {
    // Basic validation - can be extended based on conversation context
    return true;
}

nlohmann::json CommunicationMediatorAPIHandlers::format_conversation_stats(const std::unordered_map<std::string, int>& stats) {
    return stats;
}

nlohmann::json CommunicationMediatorAPIHandlers::format_agent_metrics(const std::vector<std::pair<std::string, double>>& metrics) {
    nlohmann::json metrics_json;
    for (const auto& [agent, score] : metrics) {
        metrics_json[agent] = score;
    }
    return metrics_json;
}

nlohmann::json CommunicationMediatorAPIHandlers::format_conflict_stats(const std::vector<std::pair<std::string, int>>& stats) {
    nlohmann::json stats_json;
    for (const auto& [strategy, count] : stats) {
        stats_json[strategy] = count;
    }
    return stats_json;
}

// String conversion utilities
std::string CommunicationMediatorAPIHandlers::conversation_state_to_string(ConversationState state) {
    switch (state) {
        case ConversationState::INITIALIZING: return "INITIALIZING";
        case ConversationState::ACTIVE: return "ACTIVE";
        case ConversationState::WAITING_FOR_RESPONSE: return "WAITING_FOR_RESPONSE";
        case ConversationState::CONFLICT_DETECTED: return "CONFLICT_DETECTED";
        case ConversationState::RESOLVING_CONFLICT: return "RESOLVING_CONFLICT";
        case ConversationState::CONSENSUS_REACHED: return "CONSENSUS_REACHED";
        case ConversationState::DEADLOCK: return "DEADLOCK";
        case ConversationState::COMPLETED: return "COMPLETED";
        case ConversationState::TIMEOUT: return "TIMEOUT";
        case ConversationState::CANCELLED: return "CANCELLED";
        default: return "UNKNOWN";
    }
}

std::string CommunicationMediatorAPIHandlers::conflict_type_to_string(ConflictType type) {
    switch (type) {
        case ConflictType::CONTRADICTORY_RESPONSES: return "CONTRADICTORY_RESPONSES";
        case ConflictType::RESOURCE_CONFLICT: return "RESOURCE_CONFLICT";
        case ConflictType::PRIORITY_CONFLICT: return "PRIORITY_CONFLICT";
        case ConflictType::TIMING_CONFLICT: return "TIMING_CONFLICT";
        case ConflictType::PROTOCOL_MISMATCH: return "PROTOCOL_MISMATCH";
        case ConflictType::CONSENSUS_FAILURE: return "CONSENSUS_FAILURE";
        case ConflictType::EXTERNAL_CONSTRAINT: return "EXTERNAL_CONSTRAINT";
        default: return "UNKNOWN";
    }
}

std::string CommunicationMediatorAPIHandlers::resolution_strategy_to_string(ResolutionStrategy strategy) {
    switch (strategy) {
        case ResolutionStrategy::MAJORITY_VOTE: return "MAJORITY_VOTE";
        case ResolutionStrategy::WEIGHTED_VOTE: return "WEIGHTED_VOTE";
        case ResolutionStrategy::EXPERT_ARBITRATION: return "EXPERT_ARBITRATION";
        case ResolutionStrategy::COMPROMISE_NEGOTIATION: return "COMPROMISE_NEGOTIATION";
        case ResolutionStrategy::ESCALATION: return "ESCALATION";
        case ResolutionStrategy::EXTERNAL_MEDIATION: return "EXTERNAL_MEDIATION";
        case ResolutionStrategy::TIMEOUT_ABORT: return "TIMEOUT_ABORT";
        case ResolutionStrategy::MANUAL_OVERRIDE: return "MANUAL_OVERRIDE";
        default: return "UNKNOWN";
    }
}

void CommunicationMediatorAPIHandlers::log_conversation_action(const std::string& user_id, const std::string& action, const std::string& conversation_id) {
    spdlog::info("Conversation {} action: {} by user {}", action, conversation_id, user_id);
}

void CommunicationMediatorAPIHandlers::log_message_action(const std::string& user_id, const std::string& action, const std::string& message_id) {
    spdlog::info("Message {} action: {} by user {}", action, message_id, user_id);
}

void CommunicationMediatorAPIHandlers::log_conflict_action(const std::string& user_id, const std::string& action, const std::string& conflict_id) {
    spdlog::info("Conflict {} action: {} by user {}", action, conflict_id, user_id);
}

} // namespace regulens
