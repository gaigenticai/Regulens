/**
 * Communication Mediator
 * Complex conversation orchestration and conflict resolution
 */

#ifndef COMMUNICATION_MEDIATOR_HPP
#define COMMUNICATION_MEDIATOR_HPP

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>
#include <queue>
#include <chrono>
#include <nlohmann/json.hpp>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"
#include "consensus_engine.hpp"
#include "message_translator.hpp"

namespace regulens {

enum class ConversationState {
    INITIALIZING,      // Setting up the conversation
    ACTIVE,           // Conversation is ongoing
    WAITING_FOR_RESPONSE, // Waiting for agent response
    CONFLICT_DETECTED, // Conflict detected between agents
    RESOLVING_CONFLICT, // Conflict resolution in progress
    CONSENSUS_REACHED, // Agreement reached
    DEADLOCK,         // Unable to resolve conflicts
    COMPLETED,        // Conversation successfully completed
    TIMEOUT,          // Conversation timed out
    CANCELLED         // Conversation was cancelled
};

enum class ConflictType {
    CONTRADICTORY_RESPONSES, // Agents give contradictory answers
    RESOURCE_CONFLICT,       // Agents compete for same resources
    PRIORITY_CONFLICT,       // Agents have conflicting priorities
    TIMING_CONFLICT,         // Timing issues between agents
    PROTOCOL_MISMATCH,       // Communication protocol issues
    CONSENSUS_FAILURE,       // Unable to reach agreement
    EXTERNAL_CONSTRAINT      // External factors causing conflict
};

enum class ResolutionStrategy {
    MAJORITY_VOTE,          // Use majority voting
    WEIGHTED_VOTE,          // Use weighted voting based on expertise
    EXPERT_ARBITRATION,     // Let domain expert decide
    COMPROMISE_NEGOTIATION, // Negotiate compromise
    ESCALATION,            // Escalate to higher authority
    EXTERNAL_MEDiation,     // Involve external mediator
    TIMEOUT_ABORT,          // Abort on timeout
    MANUAL_OVERRIDE         // Manual human intervention
};

struct ConversationParticipant {
    std::string agent_id;
    std::string role;              // facilitator, participant, observer
    double expertise_weight = 1.0; // Weight in decision making
    std::string domain_specialty;  // Area of expertise
    bool is_active = true;
    std::chrono::system_clock::time_point joined_at;
    std::chrono::system_clock::time_point last_active;
    int messages_sent = 0;
    int responses_received = 0;
};

struct ConversationMessage {
    std::string message_id;
    std::string conversation_id;
    std::string sender_agent_id;
    std::string recipient_agent_id; // "all" for broadcast
    std::string message_type;       // request, response, notification, conflict
    nlohmann::json content;
    std::chrono::system_clock::time_point sent_at;
    std::chrono::system_clock::time_point delivered_at;
    bool acknowledged = false;
    std::unordered_map<std::string, std::string> metadata;
};

struct ConflictResolution {
    std::string conflict_id;
    std::string conversation_id;
    ConflictType conflict_type;
    std::string description;
    std::vector<std::string> involved_agents;
    ResolutionStrategy strategy_used;
    nlohmann::json conflict_details;
    nlohmann::json resolution_result;
    std::chrono::system_clock::time_point detected_at;
    std::chrono::system_clock::time_point resolved_at;
    bool resolved_successfully = false;
    std::string resolution_summary;
};

struct ConversationContext {
    std::string conversation_id;
    std::string topic;
    std::string objective;
    ConversationState state;
    std::vector<ConversationParticipant> participants;
    std::vector<ConversationMessage> message_history;
    std::vector<ConflictResolution> conflicts;
    std::chrono::system_clock::time_point started_at;
    std::chrono::system_clock::time_point last_activity;
    std::chrono::minutes timeout_duration = std::chrono::minutes(30);
    nlohmann::json conversation_metadata;
    std::unordered_map<std::string, nlohmann::json> agent_contexts;
};

struct MediationResult {
    bool success;
    std::string resolution;
    ConversationState new_state;
    std::vector<ConversationMessage> mediation_messages;
    std::chrono::milliseconds processing_time;
    nlohmann::json mediation_details;
};

class CommunicationMediator {
public:
    CommunicationMediator(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<StructuredLogger> logger,
        std::shared_ptr<ConsensusEngine> consensus_engine = nullptr,
        std::shared_ptr<MessageTranslator> message_translator = nullptr
    );

    ~CommunicationMediator();

    // Conversation management
    std::string initiate_conversation(const std::string& topic, const std::string& objective,
                                    const std::vector<std::string>& participant_ids);
    ConversationState get_conversation_state(const std::string& conversation_id);
    ConversationContext get_conversation_context(const std::string& conversation_id);
    bool end_conversation(const std::string& conversation_id, const std::string& reason);

    // Message handling
    bool send_message(const ConversationMessage& message);
    bool broadcast_message(const std::string& conversation_id, const std::string& sender_id,
                          const nlohmann::json& content, const std::string& message_type = "notification");
    std::vector<ConversationMessage> get_pending_messages(const std::string& agent_id);
    bool acknowledge_message(const std::string& message_id, const std::string& agent_id);

    // Participant management
    bool add_participant(const std::string& conversation_id, const std::string& agent_id,
                        const std::string& role = "participant");
    bool remove_participant(const std::string& conversation_id, const std::string& agent_id);
    std::vector<ConversationParticipant> get_participants(const std::string& conversation_id);

    // Conflict detection and resolution
    std::vector<ConflictResolution> detect_conflicts(const std::string& conversation_id);
    MediationResult resolve_conflict(const std::string& conversation_id, const std::string& conflict_id,
                                   ResolutionStrategy strategy = ResolutionStrategy::MAJORITY_VOTE);
    MediationResult mediate_conversation(const std::string& conversation_id);

    // Conversation orchestration
    bool orchestrate_turn_taking(const std::string& conversation_id);
    bool facilitate_discussion(const std::string& conversation_id, const std::string& discussion_topic);
    bool coordinate_task_execution(const std::string& conversation_id, const nlohmann::json& task_spec);

    // Advanced mediation strategies
    MediationResult apply_majority_voting(const std::string& conversation_id,
                                        const std::vector<ConversationMessage>& conflicting_messages);
    MediationResult apply_weighted_consensus(const std::string& conversation_id,
                                           const std::vector<ConversationMessage>& conflicting_messages);
    MediationResult apply_expert_arbitration(const std::string& conversation_id,
                                           const std::string& expert_agent_id,
                                           const std::vector<ConversationMessage>& conflicting_messages);
    MediationResult negotiate_compromise(const std::string& conversation_id,
                                       const std::vector<ConversationMessage>& positions);

    // Analytics and monitoring
    std::unordered_map<std::string, int> get_conversation_stats();
    std::vector<std::pair<std::string, double>> get_agent_participation_metrics();
    std::vector<std::pair<std::string, int>> get_conflict_resolution_stats();
    double calculate_conversation_effectiveness(const std::string& conversation_id);

    // Configuration
    void set_default_timeout(std::chrono::minutes timeout);
    void set_max_participants(int max_participants);
    void set_conflict_detection_enabled(bool enabled);
    void set_automatic_mediation_enabled(bool enabled);
    void set_consensus_required_for_resolution(bool required);

    // Utility methods
    std::string generate_conversation_id();
    std::string generate_message_id();
    bool is_agent_participant(const std::string& conversation_id, const std::string& agent_id);
    bool validate_conversation_state(const std::string& conversation_id, ConversationState required_state);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<ConsensusEngine> consensus_engine_;
    std::shared_ptr<MessageTranslator> message_translator_;

    // Configuration
    std::chrono::minutes default_timeout_ = std::chrono::minutes(30);
    int max_participants_ = 10;
    bool conflict_detection_enabled_ = true;
    bool automatic_mediation_enabled_ = true;
    bool consensus_required_for_resolution_ = true;

    // In-memory state
    std::unordered_map<std::string, ConversationContext> active_conversations_;
    std::queue<ConversationMessage> message_queue_;
    std::mutex mediator_mutex_;

    // Helper methods
    bool create_conversation_context(const std::string& conversation_id, const std::string& topic,
                                   const std::string& objective, const std::vector<std::string>& participant_ids);
    bool update_conversation_state(const std::string& conversation_id, ConversationState new_state);
    void update_participant_activity(const std::string& conversation_id, const std::string& agent_id);

    // Conflict detection
    ConflictType analyze_conflict_type(const std::vector<ConversationMessage>& messages);
    bool messages_are_contradictory(const ConversationMessage& msg1, const ConversationMessage& msg2);
    bool detect_resource_conflicts(const std::string& conversation_id);
    bool detect_timing_conflicts(const std::string& conversation_id);

    // Message processing
    bool process_message_queue();
    bool validate_message(const ConversationMessage& message);
    bool deliver_message(const ConversationMessage& message);
    void update_message_delivery_status(const std::string& message_id);

    // Resolution strategy implementations
    MediationResult execute_majority_vote_resolution(const std::string& conversation_id,
                                                   const std::vector<ConversationMessage>& messages);
    MediationResult execute_weighted_vote_resolution(const std::string& conversation_id,
                                                   const std::vector<ConversationMessage>& messages);
    MediationResult execute_expert_arbitration_resolution(const std::string& conversation_id,
                                                        const std::string& expert_id,
                                                        const std::vector<ConversationMessage>& messages);
    MediationResult execute_compromise_negotiation(const std::string& conversation_id,
                                                 const std::vector<ConversationMessage>& positions);

    // Conversation flow management
    bool manage_conversation_flow(const std::string& conversation_id);
    std::string determine_next_speaker(const std::string& conversation_id);
    bool enforce_turn_taking_rules(const std::string& conversation_id);
    bool check_conversation_completion(const std::string& conversation_id);

    // Database operations
    bool store_conversation_context(const std::string& conversation_id, const ConversationContext& context);
    bool store_conversation_message(const ConversationMessage& message);
    bool store_conflict_resolution(const ConflictResolution& resolution);
    bool update_conversation_participants(const std::string& conversation_id,
                                        const std::vector<ConversationParticipant>& participants);

    std::optional<ConversationContext> load_conversation_context(const std::string& conversation_id);
    std::vector<ConversationMessage> load_conversation_messages(const std::string& conversation_id,
                                                              int limit = 100);
    std::vector<ConflictResolution> load_conflict_resolutions(const std::string& conversation_id);

    // Performance tracking
    void update_conversation_metrics(const std::string& conversation_id, const std::string& metric_type,
                                   double value);
    void update_agent_metrics(const std::string& agent_id, const std::string& metric_type, double value);
    void log_mediation_event(const std::string& conversation_id, const std::string& event_type,
                           const nlohmann::json& details);

    // Cleanup and maintenance
    void cleanup_expired_conversations();
    void archive_completed_conversations();
    bool validate_conversation_integrity(const std::string& conversation_id);
};

} // namespace regulens

#endif // COMMUNICATION_MEDIATOR_HPP
