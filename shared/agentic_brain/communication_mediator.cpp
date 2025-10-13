/**
 * Communication Mediator Implementation
 * Complex conversation orchestration and conflict resolution
 */

#include "communication_mediator.hpp"
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <spdlog/spdlog.h>

namespace regulens {

CommunicationMediator::CommunicationMediator(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<StructuredLogger> logger,
    std::shared_ptr<ConsensusEngine> consensus_engine,
    std::shared_ptr<MessageTranslator> message_translator
) : db_conn_(db_conn), logger_(logger),
    consensus_engine_(consensus_engine), message_translator_(message_translator) {

    if (!db_conn_) {
        throw std::runtime_error("Database connection is required for CommunicationMediator");
    }

    if (logger_) {
        logger_->info("CommunicationMediator initialized with conversation orchestration capabilities");
    }
}

CommunicationMediator::~CommunicationMediator() {
    // Cleanup active conversations
    for (const auto& [conversation_id, context] : active_conversations_) {
        try {
            end_conversation(conversation_id, "Mediator shutdown");
        } catch (const std::exception& e) {
            if (logger_) {
                logger_->error("Error ending conversation {} during shutdown: {}", conversation_id, e.what());
            }
        }
    }

    if (logger_) {
        logger_->info("CommunicationMediator shutting down");
    }
}

std::string CommunicationMediator::initiate_conversation(const std::string& topic, const std::string& objective,
                                                       const std::vector<std::string>& participant_ids) {
    try {
        // Validate input
        if (topic.empty() || objective.empty() || participant_ids.empty()) {
            if (logger_) {
                logger_->error("Invalid conversation parameters: empty topic, objective, or participants");
            }
            return "";
        }

        if (participant_ids.size() > max_participants_) {
            if (logger_) {
                logger_->error("Too many participants: {} (max: {})", participant_ids.size(), max_participants_);
            }
            return "";
        }

        // Generate conversation ID
        std::string conversation_id = generate_conversation_id();

        // Create conversation context
        if (!create_conversation_context(conversation_id, topic, objective, participant_ids)) {
            if (logger_) {
                logger_->error("Failed to create conversation context for {}", conversation_id);
            }
            return "";
        }

        // Add initial broadcast message
        nlohmann::json welcome_content = {
            {"type", "conversation_started"},
            {"topic", topic},
            {"objective", objective},
            {"participants", participant_ids}
        };

        broadcast_message(conversation_id, "mediator", welcome_content, "notification");

        if (logger_) {
            logger_->info("Conversation initiated: {} with {} participants",
                         conversation_id, participant_ids.size());
        }

        return conversation_id;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in initiate_conversation: {}", e.what());
        }
        return "";
    }
}

ConversationState CommunicationMediator::get_conversation_state(const std::string& conversation_id) {
    std::lock_guard<std::mutex> lock(mediator_mutex_);

    auto it = active_conversations_.find(conversation_id);
    if (it != active_conversations_.end()) {
        return it->second.state;
    }

    // Try to load from database
    auto context_opt = load_conversation_context(conversation_id);
    if (context_opt) {
        return context_opt->state;
    }

    return ConversationState::CANCELLED;
}

bool CommunicationMediator::send_message(const ConversationMessage& message) {
    try {
        // Validate message
        if (!validate_message(message)) {
            if (logger_) {
                logger_->error("Invalid message: {}", message.message_id);
            }
            return false;
        }

        std::lock_guard<std::mutex> lock(mediator_mutex_);

        // Check if conversation exists and is active
        auto context_it = active_conversations_.find(message.conversation_id);
        if (context_it == active_conversations_.end()) {
            if (logger_) {
                logger_->error("Conversation {} not found for message {}", message.conversation_id, message.message_id);
            }
            return false;
        }

        // Check if sender is a participant
        if (!is_agent_participant(message.conversation_id, message.sender_agent_id)) {
            if (logger_) {
                logger_->error("Agent {} is not a participant in conversation {}",
                             message.sender_agent_id, message.conversation_id);
            }
            return false;
        }

        // Add message to conversation history
        context_it->second.message_history.push_back(message);
        context_it->second.last_activity = std::chrono::system_clock::now();

        // Update participant activity
        update_participant_activity(message.conversation_id, message.sender_agent_id);

        // Store message
        if (!store_conversation_message(message)) {
            if (logger_) {
                logger_->error("Failed to store message {}", message.message_id);
            }
            return false;
        }

        // Deliver message
        if (!deliver_message(message)) {
            if (logger_) {
                logger_->error("Failed to deliver message {}", message.message_id);
            }
            return false;
        }

        // Check for conflicts if enabled
        if (conflict_detection_enabled_) {
            auto conflicts = detect_conflicts(message.conversation_id);
            if (!conflicts.empty() && automatic_mediation_enabled_) {
                // Trigger automatic mediation
                mediate_conversation(message.conversation_id);
            }
        }

        // Update conversation flow
        manage_conversation_flow(message.conversation_id);

        if (logger_) {
            logger_->info("Message sent in conversation {}: {} -> {}",
                         message.conversation_id, message.sender_agent_id,
                         message.recipient_agent_id.empty() ? "all" : message.recipient_agent_id);
        }

        return true;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in send_message: {}", e.what());
        }
        return false;
    }
}

bool CommunicationMediator::broadcast_message(const std::string& conversation_id, const std::string& sender_id,
                                            const nlohmann::json& content, const std::string& message_type) {
    ConversationMessage message;
    message.message_id = generate_message_id();
    message.conversation_id = conversation_id;
    message.sender_agent_id = sender_id;
    message.recipient_agent_id = "all"; // Broadcast
    message.message_type = message_type;
    message.content = content;
    message.sent_at = std::chrono::system_clock::now();

    return send_message(message);
}

std::vector<ConversationMessage> CommunicationMediator::get_pending_messages(const std::string& agent_id) {
    std::vector<ConversationMessage> pending_messages;

    try {
        // Query database for undelivered messages for this agent
        if (!db_conn_) return pending_messages;

        std::string query = R"(
            SELECT m.message_id, m.conversation_id, m.sender_agent_id, m.recipient_agent_id,
                   m.message_type, m.content, m.sent_at, m.metadata
            FROM conversation_messages m
            LEFT JOIN message_deliveries d ON m.message_id = d.message_id AND d.agent_id = $1
            WHERE (m.recipient_agent_id = $1 OR m.recipient_agent_id = 'all')
            AND d.message_id IS NULL
            AND m.sent_at > NOW() - INTERVAL '1 hour'
            ORDER BY m.sent_at ASC
            LIMIT 50
        )";

        auto results = db_conn_->execute_query_multi(query, {agent_id});

        for (const auto& row : results) {
            ConversationMessage msg;
            msg.message_id = row.at("message_id");
            msg.conversation_id = row.at("conversation_id");
            msg.sender_agent_id = row.at("sender_agent_id");
            msg.recipient_agent_id = row.at("recipient_agent_id");
            msg.message_type = row.at("message_type");
            msg.content = nlohmann::json::parse(row.at("content"));
            // Parse timestamp and metadata...

            pending_messages.push_back(msg);
        }

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in get_pending_messages: {}", e.what());
        }
    }

    return pending_messages;
}

std::vector<ConflictResolution> CommunicationMediator::detect_conflicts(const std::string& conversation_id) {
    std::vector<ConflictResolution> conflicts;

    try {
        std::lock_guard<std::mutex> lock(mediator_mutex_);

        auto context_it = active_conversations_.find(conversation_id);
        if (context_it == active_conversations_.end()) {
            return conflicts;
        }

        const auto& context = context_it->second;
        const auto& messages = context.message_history;

        // Check for contradictory responses
        for (size_t i = 0; i < messages.size(); ++i) {
            for (size_t j = i + 1; j < messages.size(); ++j) {
                if (messages_are_contradictory(messages[i], messages[j])) {
                    ConflictResolution conflict;
                    conflict.conflict_id = generate_message_id();
                    conflict.conversation_id = conversation_id;
                    conflict.conflict_type = ConflictType::CONTRADICTORY_RESPONSES;
                    conflict.description = "Contradictory responses detected between agents";
                    conflict.involved_agents = {messages[i].sender_agent_id, messages[j].sender_agent_id};
                    conflict.detected_at = std::chrono::system_clock::now();
                    conflict.conflict_details = {
                        {"message1", messages[i].content},
                        {"message2", messages[j].content}
                    };

                    conflicts.push_back(conflict);
                }
            }
        }

        // Check for resource conflicts
        if (detect_resource_conflicts(conversation_id)) {
            ConflictResolution conflict;
            conflict.conflict_id = generate_message_id();
            conflict.conversation_id = conversation_id;
            conflict.conflict_type = ConflictType::RESOURCE_CONFLICT;
            conflict.description = "Resource competition detected";
            conflict.detected_at = std::chrono::system_clock::now();

            conflicts.push_back(conflict);
        }

        // Store detected conflicts
        for (const auto& conflict : conflicts) {
            if (store_conflict_resolution(conflict)) {
                context_it->second.conflicts.push_back(conflict);
            }
        }

        if (!conflicts.empty() && logger_) {
            logger_->warn("Detected {} conflicts in conversation {}", conflicts.size(), conversation_id);
        }

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in detect_conflicts: {}", e.what());
        }
    }

    return conflicts;
}

MediationResult CommunicationMediator::resolve_conflict(const std::string& conversation_id,
                                                      const std::string& conflict_id,
                                                      ResolutionStrategy strategy) {
    MediationResult result;
    result.success = false;
    result.new_state = ConversationState::CONFLICT_DETECTED;
    auto start_time = std::chrono::high_resolution_clock::now();

    try {
        std::lock_guard<std::mutex> lock(mediator_mutex_);

        auto context_it = active_conversations_.find(conversation_id);
        if (context_it == active_conversations_.end()) {
            result.mediation_details = {"error", "Conversation not found"};
            return result;
        }

        // Find the conflict
        auto conflict_it = std::find_if(context_it->second.conflicts.begin(),
                                       context_it->second.conflicts.end(),
                                       [&](const ConflictResolution& c) { return c.conflict_id == conflict_id; });

        if (conflict_it == context_it->second.conflicts.end()) {
            result.mediation_details = {"error", "Conflict not found"};
            return result;
        }

        // Apply resolution strategy
        switch (strategy) {
            case ResolutionStrategy::MAJORITY_VOTE:
                result = apply_majority_voting(conversation_id, context_it->second.message_history);
                break;
            case ResolutionStrategy::WEIGHTED_VOTE:
                result = apply_weighted_consensus(conversation_id, context_it->second.message_history);
                break;
            case ResolutionStrategy::EXPERT_ARBITRATION: {
                // Find the most expert agent
                std::string expert_id = find_most_expert_agent(context_it->second.participants);
                result = apply_expert_arbitration(conversation_id, expert_id, context_it->second.message_history);
                break;
            }
            case ResolutionStrategy::COMPROMISE_NEGOTIATION:
                result = negotiate_compromise(conversation_id, context_it->second.message_history);
                break;
            default:
                result.mediation_details = {"error", "Unsupported resolution strategy"};
                return result;
        }

        // Update conflict resolution record
        conflict_it->strategy_used = strategy;
        conflict_it->resolved_at = std::chrono::system_clock::now();
        conflict_it->resolved_successfully = result.success;
        conflict_it->resolution_result = result.mediation_details;
        conflict_it->resolution_summary = result.resolution;

        if (!store_conflict_resolution(*conflict_it)) {
            if (logger_) {
                logger_->error("Failed to store conflict resolution for {}", conflict_id);
            }
        }

        // Update conversation state
        if (result.success) {
            update_conversation_state(conversation_id, result.new_state);
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        result.processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        if (logger_) {
            logger_->info("Conflict resolution {} for conversation {}: success={}",
                         conflict_id, conversation_id, result.success);
        }

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in resolve_conflict: {}", e.what());
        }
        result.mediation_details = {"error", e.what()};
    }

    return result;
}

MediationResult CommunicationMediator::apply_majority_voting(const std::string& conversation_id,
                                                           const std::vector<ConversationMessage>& conflicting_messages) {
    MediationResult result;

    try {
        if (!consensus_engine_) {
            result.success = false;
            result.resolution = "Consensus engine not available";
            return result;
        }

        // Create a temporary consensus for conflict resolution
        ConsensusConfiguration config;
        config.topic = "Conflict Resolution: " + conversation_id;
        config.algorithm = VotingAlgorithm::MAJORITY;

        // Extract participant IDs from messages
        std::unordered_set<std::string> participant_ids;
        for (const auto& msg : conflicting_messages) {
            participant_ids.insert(msg.sender_agent_id);
        }

        for (const auto& agent_id : participant_ids) {
            Agent agent;
            agent.agent_id = agent_id;
            agent.name = agent_id;
            agent.role = AgentRole::EXPERT;
            config.participants.push_back(agent);
        }

        std::string consensus_id = consensus_engine_->initiate_consensus(config);
        if (consensus_id.empty()) {
            result.success = false;
            result.resolution = "Failed to initiate consensus";
            return result;
        }

        // Submit opinions based on recent messages
        for (const auto& msg : conflicting_messages) {
            AgentOpinion opinion;
            opinion.agent_id = msg.sender_agent_id;
            opinion.decision = msg.content.dump(); // Use message content as decision
            opinion.confidence_score = 0.8; // Default confidence
            opinion.reasoning = "Based on recent message in conversation";

            consensus_engine_->submit_opinion(consensus_id, opinion);
        }

        // Calculate consensus
        ConsensusResult consensus_result = consensus_engine_->calculate_consensus(consensus_id);

        result.success = (consensus_result.final_state == ConsensusState::REACHED_CONSENSUS);
        result.resolution = consensus_result.final_decision;
        result.new_state = result.success ? ConversationState::CONSENSUS_REACHED : ConversationState::DEADLOCK;
        result.mediation_details = {
            {"strategy", "majority_voting"},
            {"consensus_id", consensus_id},
            {"agreement_percentage", consensus_result.agreement_percentage},
            {"final_decision", consensus_result.final_decision}
        };

    } catch (const std::exception& e) {
        result.success = false;
        result.resolution = "Exception during majority voting: " + std::string(e.what());
    }

    return result;
}

MediationResult CommunicationMediator::apply_weighted_consensus(const std::string& conversation_id,
                                                              const std::vector<ConversationMessage>& conflicting_messages) {
    MediationResult result;

    try {
        if (!consensus_engine_) {
            result.success = false;
            result.resolution = "Consensus engine not available";
            return result;
        }

        // Similar to majority voting but with weighted algorithm
        ConsensusConfiguration config;
        config.topic = "Weighted Conflict Resolution: " + conversation_id;
        config.algorithm = VotingAlgorithm::WEIGHTED_MAJORITY;

        // Get participants with their expertise weights
        std::lock_guard<std::mutex> lock(mediator_mutex_);
        auto context_it = active_conversations_.find(conversation_id);
        if (context_it != active_conversations_.end()) {
            for (const auto& participant : context_it->second.participants) {
                Agent agent;
                agent.agent_id = participant.agent_id;
                agent.name = participant.agent_id;
                agent.role = AgentRole::EXPERT;
                agent.voting_weight = participant.expertise_weight;
                config.participants.push_back(agent);
            }
        }

        std::string consensus_id = consensus_engine_->initiate_consensus(config);
        if (consensus_id.empty()) {
            result.success = false;
            result.resolution = "Failed to initiate weighted consensus";
            return result;
        }

        // Submit opinions with weights
        for (const auto& msg : conflicting_messages) {
            AgentOpinion opinion;
            opinion.agent_id = msg.sender_agent_id;
            opinion.decision = msg.content.dump();
            opinion.confidence_score = 0.8;
            opinion.reasoning = "Weighted opinion based on expertise";

            consensus_engine_->submit_opinion(consensus_id, opinion);
        }

        ConsensusResult consensus_result = consensus_engine_->calculate_consensus(consensus_id);

        result.success = (consensus_result.final_state == ConsensusState::REACHED_CONSENSUS);
        result.resolution = consensus_result.final_decision;
        result.new_state = result.success ? ConversationState::CONSENSUS_REACHED : ConversationState::DEADLOCK;
        result.mediation_details = {
            {"strategy", "weighted_consensus"},
            {"consensus_id", consensus_id},
            {"agreement_percentage", consensus_result.agreement_percentage},
            {"final_decision", consensus_result.final_decision}
        };

    } catch (const std::exception& e) {
        result.success = false;
        result.resolution = "Exception during weighted consensus: " + std::string(e.what());
    }

    return result;
}

std::unordered_map<std::string, int> CommunicationMediator::get_conversation_stats() {
    std::unordered_map<std::string, int> stats;

    try {
        if (!db_conn_) return stats;

        // Get basic conversation statistics
        std::string query = R"(
            SELECT
                COUNT(*) as total_conversations,
                COUNT(CASE WHEN state = 6 THEN 1 END) as completed_conversations,
                COUNT(CASE WHEN state = 7 THEN 1 END) as deadlocked_conversations,
                COUNT(CASE WHEN state = 8 THEN 1 END) as timed_out_conversations,
                AVG(EXTRACT(EPOCH FROM (last_activity - started_at))/60) as avg_duration_minutes
            FROM conversation_contexts
            WHERE started_at > NOW() - INTERVAL '30 days'
        )";

        auto results = db_conn_->execute_query_multi(query, {});
        if (!results.empty()) {
            // Parse and populate stats
            stats["total_conversations"] = 0; // Would parse from results
            stats["active_conversations"] = active_conversations_.size();
            stats["completed_conversations"] = 0;
            stats["deadlocked_conversations"] = 0;
        }

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in get_conversation_stats: {}", e.what());
        }
    }

    return stats;
}

// Helper method implementations
bool CommunicationMediator::create_conversation_context(const std::string& conversation_id,
                                                      const std::string& topic,
                                                      const std::string& objective,
                                                      const std::vector<std::string>& participant_ids) {
    ConversationContext context;
    context.conversation_id = conversation_id;
    context.topic = topic;
    context.objective = objective;
    context.state = ConversationState::INITIALIZING;
    context.started_at = std::chrono::system_clock::now();
    context.last_activity = context.started_at;

    // Add participants
    for (const auto& agent_id : participant_ids) {
        ConversationParticipant participant;
        participant.agent_id = agent_id;
        participant.role = "participant";
        participant.expertise_weight = 1.0;
        participant.is_active = true;
        participant.joined_at = context.started_at;
        participant.last_active = context.started_at;

        context.participants.push_back(participant);
    }

    // Store in memory and database
    std::lock_guard<std::mutex> lock(mediator_mutex_);
    active_conversations_[conversation_id] = context;

    if (!store_conversation_context(conversation_id, context)) {
        active_conversations_.erase(conversation_id);
        return false;
    }

    update_conversation_state(conversation_id, ConversationState::ACTIVE);
    return true;
}

bool CommunicationMediator::messages_are_contradictory(const ConversationMessage& msg1, const ConversationMessage& msg2) {
    // Simple contradiction detection - in real implementation would be more sophisticated
    if (msg1.message_type == "decision" && msg2.message_type == "decision") {
        // Check if decisions are mutually exclusive
        std::string decision1 = msg1.content.value("decision", "");
        std::string decision2 = msg2.content.value("decision", "");

        // Example: if one says "approve" and another says "deny"
        if ((decision1 == "approve" && decision2 == "deny") ||
            (decision1 == "deny" && decision2 == "approve")) {
            return true;
        }
    }

    return false;
}

bool CommunicationMediator::detect_resource_conflicts(const std::string& conversation_id) {
    // Check for resource allocation conflicts
    // This would analyze message content for resource requests
    return false; // Placeholder
}

std::string CommunicationMediator::generate_conversation_id() {
    return "conv_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
}

std::string CommunicationMediator::generate_message_id() {
    return "msg_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
}

bool CommunicationMediator::validate_message(const ConversationMessage& message) {
    return !message.message_id.empty() &&
           !message.conversation_id.empty() &&
           !message.sender_agent_id.empty() &&
           !message.message_type.empty();
}

bool CommunicationMediator::deliver_message(const ConversationMessage& message) {
    // Mark message as delivered in database
    return update_message_delivery_status(message.message_id);
}

void CommunicationMediator::update_participant_activity(const std::string& conversation_id, const std::string& agent_id) {
    auto& context = active_conversations_[conversation_id];
    for (auto& participant : context.participants) {
        if (participant.agent_id == agent_id) {
            participant.last_active = std::chrono::system_clock::now();
            participant.messages_sent++;
            break;
        }
    }
}

bool CommunicationMediator::manage_conversation_flow(const std::string& conversation_id) {
    // Implement conversation flow management logic
    // This would handle turn-taking, timeouts, completion detection, etc.
    return true;
}

std::string CommunicationMediator::find_most_expert_agent(const std::vector<ConversationParticipant>& participants) {
    if (participants.empty()) return "";

    // Find participant with highest expertise weight
    const auto& expert = *std::max_element(participants.begin(), participants.end(),
        [](const ConversationParticipant& a, const ConversationParticipant& b) {
            return a.expertise_weight < b.expertise_weight;
        });

    return expert.agent_id;
}

// Database operations (simplified implementations)
bool CommunicationMediator::store_conversation_context(const std::string& conversation_id, const ConversationContext& context) {
    try {
        if (!db_conn_) return false;

        std::string query = R"(
            INSERT INTO conversation_contexts (
                conversation_id, topic, objective, state, participants,
                started_at, last_activity, timeout_duration_min, metadata
            ) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9)
            ON CONFLICT (conversation_id) DO UPDATE SET
                state = EXCLUDED.state,
                participants = EXCLUDED.participants,
                last_activity = EXCLUDED.last_activity,
                metadata = EXCLUDED.metadata
        )";

        nlohmann::json participants_json;
        for (const auto& participant : context.participants) {
            participants_json.push_back({
                {"agent_id", participant.agent_id},
                {"role", participant.role},
                {"expertise_weight", participant.expertise_weight},
                {"is_active", participant.is_active}
            });
        }

        std::vector<std::string> params = {
            conversation_id,
            context.topic,
            context.objective,
            std::to_string(static_cast<int>(context.state)),
            participants_json.dump(),
            std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                context.started_at.time_since_epoch()).count()),
            std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                context.last_activity.time_since_epoch()).count()),
            std::to_string(context.timeout_duration.count()),
            context.conversation_metadata.dump()
        };

        return db_conn_->execute_command(query, params);

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in store_conversation_context: {}", e.what());
        }
        return false;
    }
}

bool CommunicationMediator::store_conversation_message(const ConversationMessage& message) {
    try {
        if (!db_conn_) return false;

        std::string query = R"(
            INSERT INTO conversation_messages (
                message_id, conversation_id, sender_agent_id, recipient_agent_id,
                message_type, content, sent_at, metadata
            ) VALUES ($1, $2, $3, $4, $5, $6, $7, $8)
        )";

        std::vector<std::string> params = {
            message.message_id,
            message.conversation_id,
            message.sender_agent_id,
            message.recipient_agent_id,
            message.message_type,
            message.content.dump(),
            std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                message.sent_at.time_since_epoch()).count()),
            nlohmann::json(message.metadata).dump()
        };

        return db_conn_->execute_command(query, params);

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in store_conversation_message: {}", e.what());
        }
        return false;
    }
}

bool CommunicationMediator::store_conflict_resolution(const ConflictResolution& resolution) {
    try {
        if (!db_conn_) return false;

        std::string query = R"(
            INSERT INTO conflict_resolutions (
                conflict_id, conversation_id, conflict_type, description,
                involved_agents, strategy_used, conflict_details,
                resolution_result, detected_at, resolved_at,
                resolved_successfully, resolution_summary
            ) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12)
        )";

        std::vector<std::string> params = {
            resolution.conflict_id,
            resolution.conversation_id,
            std::to_string(static_cast<int>(resolution.conflict_type)),
            resolution.description,
            nlohmann::json(resolution.involved_agents).dump(),
            std::to_string(static_cast<int>(resolution.strategy_used)),
            resolution.conflict_details.dump(),
            resolution.resolution_result.dump(),
            std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                resolution.detected_at.time_since_epoch()).count()),
            resolution.resolved_at.time_since_epoch().count() > 0 ?
                std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                    resolution.resolved_at.time_since_epoch()).count()) : "0",
            resolution.resolved_successfully ? "true" : "false",
            resolution.resolution_summary
        };

        return db_conn_->execute_command(query, params);

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in store_conflict_resolution: {}", e.what());
        }
        return false;
    }
}

bool CommunicationMediator::update_conversation_state(const std::string& conversation_id, ConversationState new_state) {
    std::lock_guard<std::mutex> lock(mediator_mutex_);

    auto it = active_conversations_.find(conversation_id);
    if (it != active_conversations_.end()) {
        it->second.state = new_state;
        it->second.last_activity = std::chrono::system_clock::now();
        return store_conversation_context(conversation_id, it->second);
    }

    return false;
}

bool CommunicationMediator::update_message_delivery_status(const std::string& message_id) {
    // Update message delivery status in database
    return true; // Placeholder
}

} // namespace regulens
