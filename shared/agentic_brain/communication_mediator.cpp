/**
 * Communication Mediator Implementation
 * Complex conversation orchestration and conflict resolution
 */

#include "communication_mediator.hpp"
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <unordered_set>
#include <set>
#include <chrono>
#include <cctype>
#include <cmath>
#include <iterator>
#include <spdlog/spdlog.h>
#include <fmt/format.h>

namespace regulens {

namespace {

using Clock = std::chrono::system_clock;

std::chrono::system_clock::time_point parse_timestamp(const std::string& value) {
    if (value.empty()) {
        return Clock::time_point{};
    }

    try {
        if (std::all_of(value.begin(), value.end(), ::isdigit)) {
            auto seconds = std::stoll(value);
            return Clock::time_point{std::chrono::seconds(seconds)};
        }

        std::tm tm{};
        std::istringstream ss(value);
        if (value.find('T') != std::string::npos) {
            ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        } else {
            ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        }

        if (!ss.fail()) {
            auto time = std::mktime(&tm);
            if (time != -1) {
                return Clock::from_time_t(time);
            }
        }
    } catch (...) {
        // Fall through to epoch return below
    }

    return Clock::time_point{};
}

std::string format_timestamp_iso(const std::chrono::system_clock::time_point& tp) {
    if (tp.time_since_epoch().count() == 0) {
        return "";
    }

    std::time_t tt = Clock::to_time_t(tp);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &tt);
#else
    gmtime_r(&tt, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

nlohmann::json safe_parse_json(const std::string& value, const nlohmann::json& fallback = nlohmann::json::object()) {
    if (value.empty()) {
        return fallback;
    }
    try {
        return nlohmann::json::parse(value);
    } catch (...) {
        return fallback;
    }
}

std::unordered_map<std::string, std::string> json_to_map(const nlohmann::json& json_obj) {
    std::unordered_map<std::string, std::string> metadata;
    if (!json_obj.is_object()) {
        return metadata;
    }
    for (const auto& [key, value] : json_obj.items()) {
        if (value.is_string()) {
            metadata[key] = value.get<std::string>();
        } else {
            metadata[key] = value.dump();
        }
    }
    return metadata;
}

bool parse_bool(const std::string& value) {
    if (value.empty()) {
        return false;
    }

    std::string lower;
    lower.reserve(value.size());
    for (unsigned char ch : value) {
        lower.push_back(static_cast<char>(std::tolower(ch)));
    }

    return lower == "true" || lower == "t" || lower == "1";
}

int safe_to_int(const std::string& value, int fallback = 0) {
    if (value.empty()) {
        return fallback;
    }
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

ConsensusDecisionConfidence confidence_from_percentage(double agreement_percentage, int rounds_used) {
    if (agreement_percentage >= 0.9 && rounds_used <= 2) {
        return ConsensusDecisionConfidence::VERY_HIGH;
    }
    if (agreement_percentage >= 0.7 && rounds_used <= 3) {
        return ConsensusDecisionConfidence::HIGH;
    }
    if (agreement_percentage >= 0.5) {
        return ConsensusDecisionConfidence::MEDIUM;
    }
    return ConsensusDecisionConfidence::LOW;
}

} // namespace

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
        logger_->info(
            "CommunicationMediator initialized with conversation orchestration capabilities",
            "CommunicationMediator",
            __func__
        );
    }
}

CommunicationMediator::~CommunicationMediator() {
    // Cleanup active conversations
    for (const auto& [conversation_id, context] : active_conversations_) {
        try {
            end_conversation(conversation_id, "Mediator shutdown");
        } catch (const std::exception& e) {
            if (logger_) {
                logger_->error(
                    "Error ending conversation during shutdown",
                    "CommunicationMediator",
                    __func__,
                    {
                        {"conversation_id", conversation_id},
                        {"error", e.what()}
                    }
                );
            }
        }
    }

    if (logger_) {
        logger_->info(
            "CommunicationMediator shutting down",
            "CommunicationMediator",
            __func__
        );
    }
}

std::string CommunicationMediator::initiate_conversation(const std::string& topic, const std::string& objective,
                                                       const std::vector<std::string>& participant_ids) {
    try {
        // Validate input
        if (topic.empty() || objective.empty() || participant_ids.empty()) {
            if (logger_) {
                logger_->error(
                    "Invalid conversation parameters",
                    "CommunicationMediator",
                    __func__,
                    {
                        {"topic", topic},
                        {"objective", objective},
                        {"participant_count", std::to_string(participant_ids.size())}
                    }
                );
            }
            return "";
        }

        if (participant_ids.size() > max_participants_) {
            if (logger_) {
                logger_->error(
                    "Too many participants for conversation",
                    "CommunicationMediator",
                    __func__,
                    {
                        {"participant_count", std::to_string(participant_ids.size())},
                        {"max_participants", std::to_string(max_participants_)}
                    }
                );
            }
            return "";
        }

        // Generate conversation ID
        std::string conversation_id = generate_conversation_id();

        // Create conversation context
        if (!create_conversation_context(conversation_id, topic, objective, participant_ids)) {
            if (logger_) {
                logger_->error(
                    "Failed to create conversation context",
                    "CommunicationMediator",
                    __func__,
                    {{"conversation_id", conversation_id}}
                );
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
            logger_->info(
                "Conversation initiated",
                "CommunicationMediator",
                __func__,
                {
                    {"conversation_id", conversation_id},
                    {"participant_count", std::to_string(participant_ids.size())}
                }
            );
        }

        return conversation_id;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error(
                "Exception in initiate_conversation",
                "CommunicationMediator",
                __func__,
                {{"error", e.what()}}
            );
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
                logger_->error(
                    "Invalid conversation message",
                    "CommunicationMediator",
                    __func__,
                    {
                        {"message_id", message.message_id},
                        {"conversation_id", message.conversation_id}
                    }
                );
            }
            return false;
        }

        std::lock_guard<std::mutex> lock(mediator_mutex_);

        // Check if conversation exists and is active
        auto context_it = active_conversations_.find(message.conversation_id);
        if (context_it == active_conversations_.end()) {
            if (logger_) {
                logger_->error(
                    "Conversation not found for message",
                    "CommunicationMediator",
                    __func__,
                    {
                        {"conversation_id", message.conversation_id},
                        {"message_id", message.message_id}
                    }
                );
            }
            return false;
        }

        // Check if sender is a participant
        if (!is_agent_participant(message.conversation_id, message.sender_agent_id)) {
            if (logger_) {
                logger_->error(
                    "Sender not authorized for conversation",
                    "CommunicationMediator",
                    __func__,
                    {
                        {"conversation_id", message.conversation_id},
                        {"agent_id", message.sender_agent_id}
                    }
                );
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
                logger_->error(
                    "Failed to store conversation message",
                    "CommunicationMediator",
                    __func__,
                    {{"message_id", message.message_id}}
                );
            }
            return false;
        }

        // Deliver message
        if (!deliver_message(message)) {
            if (logger_) {
                logger_->error(
                    "Failed to deliver conversation message",
                    "CommunicationMediator",
                    __func__,
                    {{"message_id", message.message_id}}
                );
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
            logger_->info(
                "Conversation message sent",
                "CommunicationMediator",
                __func__,
                {
                    {"conversation_id", message.conversation_id},
                    {"sender_id", message.sender_agent_id},
                    {"recipient", message.recipient_agent_id.empty() ? "all" : message.recipient_agent_id}
                }
            );
        }

        return true;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error(
                "Exception in send_message",
                "CommunicationMediator",
                __func__,
                {{"error", e.what()}}
            );
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
            msg.message_id = row.value("message_id", std::string{});
            msg.conversation_id = row.value("conversation_id", std::string{});
            msg.sender_agent_id = row.value("sender_agent_id", std::string{});
            msg.recipient_agent_id = row.value("recipient_agent_id", std::string{});
            msg.message_type = row.value("message_type", std::string{});

            const auto content_json = safe_parse_json(row.value("content", std::string{}), nlohmann::json::object());
            msg.content = content_json;
            msg.sent_at = parse_timestamp(row.value("sent_at", std::string{}));

            const auto metadata_json = safe_parse_json(row.value("metadata", std::string{}), nlohmann::json::object());
            if (metadata_json.is_object()) {
                for (const auto& [key, value] : metadata_json.items()) {
                    if (value.is_string()) {
                        msg.metadata[key] = value.get<std::string>();
                    }
                }
            }

            pending_messages.push_back(std::move(msg));
        }

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error(
                "Exception in get_pending_messages",
                "CommunicationMediator",
                __func__,
                {{"error", e.what()} }
            );
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
                    nlohmann::json message_array = nlohmann::json::array();
                    message_array.push_back({
                        {"message_id", messages[i].message_id},
                        {"sender", messages[i].sender_agent_id},
                        {"recipient", messages[i].recipient_agent_id},
                        {"content", messages[i].content}
                    });
                    message_array.push_back({
                        {"message_id", messages[j].message_id},
                        {"sender", messages[j].sender_agent_id},
                        {"recipient", messages[j].recipient_agent_id},
                        {"content", messages[j].content}
                    });

                    conflict.conflict_details = {
                        {"type", "contradictory_responses"},
                        {"messages", message_array}
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
            conflict.conflict_details = {
                {"type", "resource_conflict"}
            };

            conflicts.push_back(conflict);
        }

        // Store detected conflicts
        for (const auto& conflict : conflicts) {
            if (store_conflict_resolution(conflict)) {
                context_it->second.conflicts.push_back(conflict);
            }
        }

        if (!conflicts.empty() && logger_) {
            logger_->warn(
                "Conflicts detected in conversation",
                "CommunicationMediator",
                __func__,
                {
                    {"conversation_id", conversation_id},
                    {"conflict_count", std::to_string(conflicts.size())}
                }
            );
        }

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error(
                "Exception in detect_conflicts",
                "CommunicationMediator",
                __func__,
                {{"error", e.what()}}
            );
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
                logger_->error(
                    "Failed to store conflict resolution",
                    "CommunicationMediator",
                    __func__,
                    {
                        {"conversation_id", conversation_id},
                        {"conflict_id", conflict_id}
                    }
                );
            }
        }

        // Update conversation state
        if (result.success) {
            update_conversation_state(conversation_id, result.new_state);
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        result.processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        if (logger_) {
            logger_->info(
                "Conflict resolved",
                "CommunicationMediator",
                __func__,
                {
                    {"conversation_id", conversation_id},
                    {"conflict_id", conflict_id},
                    {"success", result.success ? "true" : "false"}
                }
            );
        }

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error(
                "Exception in resolve_conflict",
                "CommunicationMediator",
                __func__,
                {{"error", e.what()}}
            );
        }
        result.mediation_details = {"error", e.what()};
    }

    return result;
}

MediationResult CommunicationMediator::mediate_conversation(const std::string& conversation_id) {
    auto start_time = std::chrono::high_resolution_clock::now();

    MediationResult result;
    result.success = false;
    result.new_state = ConversationState::CONFLICT_DETECTED;
    result.mediation_details = nlohmann::json::object();

    if (conversation_id.empty()) {
        result.mediation_details["error"] = "Conversation ID is required";
        result.processing_time = std::chrono::milliseconds(0);
        return result;
    }

    // Load conversation context
    std::optional<ConversationContext> context_opt;
    {
        std::lock_guard<std::mutex> lock(mediator_mutex_);
        auto it = active_conversations_.find(conversation_id);
        if (it != active_conversations_.end()) {
            context_opt = it->second;
        }
    }

    if (!context_opt) {
        context_opt = load_conversation_context(conversation_id);
        if (context_opt) {
            std::lock_guard<std::mutex> lock(mediator_mutex_);
            active_conversations_[conversation_id] = *context_opt;
        }
    }

    if (!context_opt) {
        result.mediation_details["error"] = "Conversation context not found";
        result.processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start_time);
        return result;
    }

    ConversationContext context_snapshot = *context_opt;

    // Consolidate unresolved conflicts
    std::vector<ConflictResolution> conflicts;
    std::set<std::string> seen_conflicts;
    for (const auto& conflict : context_snapshot.conflicts) {
        if (!conflict.resolved_successfully) {
            conflicts.push_back(conflict);
            seen_conflicts.insert(conflict.conflict_id);
        }
    }

    auto newly_detected = detect_conflicts(conversation_id);
    for (auto& conflict : newly_detected) {
        if (seen_conflicts.insert(conflict.conflict_id).second) {
            conflicts.push_back(conflict);
        }
    }

    if (conflicts.empty()) {
        result.success = true;
        result.resolution = "No active conflicts detected";
        result.new_state = context_snapshot.state;
        result.mediation_details = {
            {"status", "no_conflicts"},
            {"conflict_count", 0}
        };
        result.processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start_time);
        return result;
    }

    update_conversation_state(conversation_id, ConversationState::RESOLVING_CONFLICT);
    context_snapshot.state = ConversationState::RESOLVING_CONFLICT;

    // Retrieve message history for analysis
    std::vector<ConversationMessage> message_history = context_snapshot.message_history;
    if (message_history.empty()) {
        message_history = load_conversation_messages(conversation_id, 200);
    }

    if (message_history.empty()) {
        result.mediation_details = {
            {"error", "Unable to retrieve conversation messages"},
            {"conflict_count", conflicts.size()}
        };
        result.processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start_time);
        return result;
    }

    std::unordered_map<std::string, ConversationMessage> message_lookup;
    message_lookup.reserve(message_history.size());
    for (const auto& message : message_history) {
        message_lookup.emplace(message.message_id, message);
    }

    std::vector<ConversationMessage> relevant_messages;
    std::set<std::string> referenced_message_ids;
    for (const auto& conflict : conflicts) {
        if (conflict.conflict_details.contains("messages") && conflict.conflict_details["messages"].is_array()) {
            for (const auto& item : conflict.conflict_details["messages"]) {
                std::string message_id = item.value("message_id", "");
                if (!message_id.empty()) {
                    referenced_message_ids.insert(message_id);
                }
            }
        }
    }

    for (const auto& message_id : referenced_message_ids) {
        auto it = message_lookup.find(message_id);
        if (it != message_lookup.end()) {
            relevant_messages.push_back(it->second);
        }
    }

    if (relevant_messages.empty()) {
        size_t take = std::min<size_t>(message_history.size(), 10);
        relevant_messages.assign(message_history.end() - take, message_history.end());
    }

    struct NormalizedMessage {
        ConversationMessage original;
        nlohmann::json payload;
    };

    std::vector<NormalizedMessage> normalized_messages;
    normalized_messages.reserve(relevant_messages.size());

    for (const auto& message : relevant_messages) {
        nlohmann::json payload = message.content;
        if (!payload.is_object()) {
            if (payload.is_string()) {
                payload = safe_parse_json(payload.get<std::string>(), nlohmann::json::object());
            }
            if (!payload.is_object()) {
                payload = nlohmann::json{{"raw", message.content}};
            }
        }

        if (message_translator_) {
            try {
                std::string serialized = message.content.dump();
                MessageHeader header;
                header.message_id = message.message_id;
                header.correlation_id = message.metadata.count("correlation_id") ?
                    message.metadata.at("correlation_id") : "";
                header.message_type = MessageType::NOTIFICATION;
                header.source_protocol = MessageProtocol::CUSTOM;
                header.target_protocol = MessageProtocol::JSON_RPC;
                header.timestamp = message.sent_at;
                header.sender_id = message.sender_agent_id;
                header.recipient_id = message.recipient_agent_id;
                header.priority = 3;
                header.custom_headers = message.metadata;

                if (auto detected = message_translator_->detect_protocol(serialized)) {
                    header.source_protocol = *detected;
                }

                auto translation = message_translator_->translate_message(
                    serialized, header, MessageProtocol::JSON_RPC);
                if (translation.result == TranslationResult::SUCCESS) {
                    auto parsed = message_translator_->parse_message(
                        translation.translated_message, MessageProtocol::JSON_RPC);
                    if (parsed && parsed->is_object()) {
                        payload = *parsed;
                    }
                }
            } catch (const std::exception& translation_error) {
                if (logger_) {
                    logger_->warn(
                        "Mediation translation warning",
                        "CommunicationMediator",
                        __func__,
                        {
                            {"error", translation_error.what()},
                            {"conversation_id", conversation_id},
                            {"message_id", message.message_id}
                        }
                    );
                }
            }
        }

        normalized_messages.push_back({message, payload});
    }

    bool use_weighted_consensus = false;
    if (!context_snapshot.participants.empty()) {
        double baseline_weight = context_snapshot.participants.front().expertise_weight;
        for (const auto& participant : context_snapshot.participants) {
            if (std::abs(participant.expertise_weight - baseline_weight) > 0.001) {
                use_weighted_consensus = true;
                break;
            }
        }
    }

    ResolutionStrategy applied_strategy = use_weighted_consensus ?
        ResolutionStrategy::WEIGHTED_VOTE : ResolutionStrategy::MAJORITY_VOTE;

    double agreement_percentage = 0.0;
    ConsensusDecisionConfidence decision_confidence = ConsensusDecisionConfidence::LOW;
    int rounds_used = 1;
    std::string consensus_id;

    if (consensus_engine_) {
        try {
            ConsensusConfiguration config;
            config.topic = "Conversation mediation: " + conversation_id;
            config.algorithm = use_weighted_consensus ?
                VotingAlgorithm::WEIGHTED_MAJORITY : VotingAlgorithm::MAJORITY;
            config.max_rounds = std::max(1, static_cast<int>(normalized_messages.size()));
            config.timeout_per_round = std::chrono::minutes(5);
            config.consensus_threshold = consensus_required_for_resolution_ ? 0.75 : 0.6;
            config.allow_discussion = true;
            config.require_justification = true;

            for (const auto& participant : context_snapshot.participants) {
                Agent agent;
                agent.agent_id = participant.agent_id;
                agent.name = participant.agent_id;
                agent.role = AgentRole::EXPERT;
                agent.voting_weight = participant.expertise_weight;
                agent.domain_expertise = participant.domain_specialty;
                agent.confidence_threshold = 0.6;
                agent.is_active = participant.is_active;
                agent.last_active = participant.last_active;
                config.participants.push_back(agent);
            }

            if (config.participants.size() < static_cast<size_t>(config.min_participants)) {
                config.min_participants = static_cast<int>(config.participants.size());
            }

            consensus_id = consensus_engine_->initiate_consensus(config);
            consensus_engine_->start_voting_round(consensus_id);

            for (const auto& entry : normalized_messages) {
                const auto& message = entry.original;
                const auto& payload = entry.payload;

                AgentOpinion opinion;
                opinion.agent_id = message.sender_agent_id;
                opinion.decision = payload.value("decision", message.content.dump());
                opinion.confidence_score = payload.value("confidence_score",
                    payload.value("confidence", 0.7));
                opinion.reasoning = payload.value("reasoning", "Derived from conversation message");
                opinion.supporting_data = payload;
                if (payload.contains("concerns") && payload["concerns"].is_array()) {
                    for (const auto& concern : payload["concerns"]) {
                        if (concern.is_string()) {
                            opinion.concerns.push_back(concern.get<std::string>());
                        }
                    }
                }
                opinion.submitted_at = message.sent_at;
                opinion.round_number = 1;

                consensus_engine_->submit_opinion(consensus_id, opinion);
            }

            consensus_engine_->end_voting_round(consensus_id);
            auto consensus_result = consensus_engine_->calculate_consensus(consensus_id);

            agreement_percentage = consensus_result.agreement_percentage;
            rounds_used = static_cast<int>(consensus_result.rounds.size());
            decision_confidence = consensus_result.confidence_level;

            result.success = (consensus_result.final_state == ConsensusState::REACHED_CONSENSUS);
            result.resolution = consensus_result.final_decision;
            if (result.resolution.empty()) {
                result.resolution = "Consensus reached without explicit decision payload";
            }
            result.new_state = result.success ? ConversationState::CONSENSUS_REACHED
                                              : ConversationState::DEADLOCK;

            result.mediation_details = {
                {"strategy", use_weighted_consensus ? "weighted_consensus" : "majority_consensus"},
                {"consensus_id", consensus_id},
                {"agreement_percentage", agreement_percentage},
                {"rounds_used", rounds_used},
                {"final_state", static_cast<int>(consensus_result.final_state)}
            };

        } catch (const std::exception& e) {
            if (logger_) {
                logger_->error(
                    "Consensus mediation failed",
                    "CommunicationMediator",
                    __func__,
                    {{"error", e.what()}}
                );
            }
            result.success = false;
            result.resolution = "Consensus mediation failed";
            result.new_state = ConversationState::DEADLOCK;
            result.mediation_details = {
                {"strategy", "consensus_engine"},
                {"error", e.what()}
            };
        }
    }

    if (!consensus_engine_ || !result.success) {
        std::unordered_map<std::string, int> decision_counts;
        std::unordered_map<std::string, double> confidence_totals;

        for (const auto& entry : normalized_messages) {
            const auto& payload = entry.payload;
            std::string decision = payload.value("decision", entry.original.content.dump());
            double confidence = payload.value("confidence_score",
                payload.value("confidence", 0.7));

            decision_counts[decision] += 1;
            confidence_totals[decision] += confidence;
        }

        if (!decision_counts.empty()) {
            auto best = std::max_element(decision_counts.begin(), decision_counts.end(),
                [](const auto& a, const auto& b) {
                    return a.second < b.second;
                });

            int total_votes = 0;
            for (const auto& entry : decision_counts) {
                total_votes += entry.second;
            }

            agreement_percentage = total_votes == 0 ? 0.0 :
                static_cast<double>(best->second) / static_cast<double>(total_votes);
            decision_confidence = confidence_from_percentage(agreement_percentage, rounds_used);

            double average_confidence = confidence_totals[best->first] /
                static_cast<double>(best->second);

            bool meets_threshold = agreement_percentage >= (consensus_required_for_resolution_ ? 0.7 : 0.5);

            result.success = meets_threshold;
            result.resolution = best->first;
            result.new_state = meets_threshold ? ConversationState::CONSENSUS_REACHED
                                               : ConversationState::CONFLICT_DETECTED;
            result.mediation_details = {
                {"strategy", "heuristic_majority"},
                {"agreement_percentage", agreement_percentage},
                {"average_confidence", average_confidence},
                {"meets_threshold", meets_threshold}
            };
            applied_strategy = ResolutionStrategy::COMPROMISE_NEGOTIATION;
        } else if (!result.success) {
            result.resolution = "Unable to determine dominant decision";
            result.new_state = ConversationState::DEADLOCK;
            result.mediation_details = {
                {"strategy", "heuristic_majority"},
                {"error", "No decisions available"}
            };
            applied_strategy = ResolutionStrategy::COMPROMISE_NEGOTIATION;
        }
    }

    auto resolution_time = Clock::now();
    for (auto& conflict : conflicts) {
        conflict.resolution_summary = result.resolution;
        conflict.resolution_result = result.mediation_details;
        conflict.resolved_at = resolution_time;
        conflict.resolved_successfully = result.success;
        conflict.strategy_used = applied_strategy;
        store_conflict_resolution(conflict);
    }

    ConversationMessage mediation_message;
    mediation_message.message_id = generate_message_id();
    mediation_message.conversation_id = conversation_id;
    mediation_message.sender_agent_id = "mediator";
    mediation_message.recipient_agent_id = "all";
    mediation_message.message_type = "mediation_result";
    mediation_message.content = {
        {"resolution", result.resolution},
        {"success", result.success},
        {"agreement_percentage", agreement_percentage},
        {"confidence", static_cast<int>(decision_confidence)},
        {"details", result.mediation_details},
        {"conflict_count", conflicts.size()}
    };
    mediation_message.sent_at = resolution_time;
    mediation_message.delivered_at = Clock::time_point{};
    mediation_message.acknowledged = false;
    mediation_message.metadata = {
        {"source", "communication_mediator"},
        {"mediated_conflicts", std::to_string(conflicts.size())}
    };

    if (result.success || !automatic_mediation_enabled_) {
        send_message(mediation_message);
    } else {
        store_conversation_message(mediation_message);
        update_message_delivery_status(mediation_message.message_id);
    }

    result.mediation_messages.push_back(mediation_message);

    ConversationContext updated_context;
    {
        std::lock_guard<std::mutex> lock(mediator_mutex_);
        auto& context_ref = active_conversations_[conversation_id];
        context_ref.state = result.new_state;
        context_ref.last_activity = resolution_time;
        context_ref.conflicts = conflicts;
        context_ref.message_history.push_back(mediation_message);
        updated_context = context_ref;
    }

    store_conversation_context(conversation_id, updated_context);

    log_mediation_event(conversation_id, "mediation_completed", {
        {"success", result.success},
        {"agreement_percentage", agreement_percentage},
        {"strategy", applied_strategy == ResolutionStrategy::COMPROMISE_NEGOTIATION ?
            "heuristic_majority" : (use_weighted_consensus ? "weighted_consensus" : "majority_consensus")},
        {"conflict_count", conflicts.size()}
    });

    if (logger_) {
        logger_->info(
            "Mediated conversation outcome",
            "CommunicationMediator",
            __func__,
            {
                {"conversation_id", conversation_id},
                {"success", result.success ? "true" : "false"},
                {"resolution", result.resolution}
            }
        );
    }

    result.processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start_time);

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
            logger_->error(
                "Exception in get_conversation_stats",
                "CommunicationMediator",
                __func__,
                {{"error", e.what()}}
            );
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
            logger_->error(
                "Exception in store_conversation_context",
                "CommunicationMediator",
                __func__,
                {{"error", e.what()}}
            );
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
            logger_->error(
                "Exception in store_conversation_message",
                "CommunicationMediator",
                __func__,
                {{"error", e.what()}}
            );
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
            ON CONFLICT (conflict_id) DO UPDATE SET
                conflict_type = EXCLUDED.conflict_type,
                description = EXCLUDED.description,
                involved_agents = EXCLUDED.involved_agents,
                strategy_used = EXCLUDED.strategy_used,
                conflict_details = EXCLUDED.conflict_details,
                resolution_result = EXCLUDED.resolution_result,
                detected_at = EXCLUDED.detected_at,
                resolved_at = EXCLUDED.resolved_at,
                resolved_successfully = EXCLUDED.resolved_successfully,
                resolution_summary = EXCLUDED.resolution_summary
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
            logger_->error(
                "Exception in store_conflict_resolution",
                "CommunicationMediator",
                __func__,
                {{"error", e.what()}}
            );
        }
        return false;
    }
}

std::optional<ConversationContext> CommunicationMediator::load_conversation_context(const std::string& conversation_id) {
    if (!db_conn_) {
        return std::nullopt;
    }

    try {
        auto result = db_conn_->execute_query(
            "SELECT conversation_id, topic, objective, state, participants, "
            "started_at, last_activity, timeout_duration_min, metadata "
            "FROM conversation_contexts WHERE conversation_id = $1 LIMIT 1",
            {conversation_id}
        );

        if (result.rows.empty()) {
            return std::nullopt;
        }

        const auto& row = result.rows.front();
        ConversationContext context;
        context.conversation_id = row.at("conversation_id");
        context.topic = row.at("topic");
        context.objective = row.at("objective");
        context.state = static_cast<ConversationState>(safe_to_int(row.at("state")));
        context.started_at = parse_timestamp(row.at("started_at"));
        context.last_activity = parse_timestamp(row.at("last_activity"));
        context.timeout_duration = std::chrono::minutes(safe_to_int(row.at("timeout_duration_min"), 30));
        context.conversation_metadata = safe_parse_json(row.at("metadata"), nlohmann::json::object());

        auto participants_json = safe_parse_json(row.at("participants"), nlohmann::json::array());
        if (participants_json.is_array()) {
            for (const auto& entry : participants_json) {
                if (!entry.is_object()) continue;
                ConversationParticipant participant;
                participant.agent_id = entry.value("agent_id", "");
                participant.role = entry.value("role", "participant");
                participant.expertise_weight = entry.value("expertise_weight", 1.0);
                participant.domain_specialty = entry.value("domain_specialty", "");
                participant.is_active = entry.contains("is_active") ?
                    entry.value("is_active", true) : true;
                participant.joined_at = context.started_at;
                participant.last_active = context.last_activity;
                participant.messages_sent = entry.value("messages_sent", 0);
                participant.responses_received = entry.value("responses_received", 0);
                context.participants.push_back(participant);
            }
        }

        context.message_history = load_conversation_messages(conversation_id, 100);
        context.conflicts = load_conflict_resolutions(conversation_id);

        return context;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error(
                "Failed to load conversation context",
                "CommunicationMediator",
                __func__,
                {
                    {"conversation_id", conversation_id},
                    {"error", e.what()}
                }
            );
        }
        return std::nullopt;
    }
}

std::vector<ConversationMessage> CommunicationMediator::load_conversation_messages(const std::string& conversation_id,
                                                                                  int limit) {
    std::vector<ConversationMessage> messages;

    if (!db_conn_) {
        return messages;
    }

    try {
        std::string query = R"(
            SELECT message_id, conversation_id, sender_agent_id, recipient_agent_id,
                   message_type, content, sent_at, delivered_at, acknowledged, metadata
            FROM conversation_messages
            WHERE conversation_id = $1
            ORDER BY sent_at ASC
            LIMIT $2::int
        )";

        auto result = db_conn_->execute_query(query, {conversation_id, std::to_string(limit)});
        for (const auto& row : result.rows) {
            auto get_value = [&row](const std::string& key) -> std::string {
                auto it = row.find(key);
                return it != row.end() ? it->second : "";
            };

            ConversationMessage message;
            message.message_id = get_value("message_id");
            message.conversation_id = get_value("conversation_id");
            message.sender_agent_id = get_value("sender_agent_id");
            message.recipient_agent_id = get_value("recipient_agent_id");
            message.message_type = get_value("message_type");
            message.content = safe_parse_json(get_value("content"), nlohmann::json::object());
            message.sent_at = parse_timestamp(get_value("sent_at"));
            message.delivered_at = parse_timestamp(get_value("delivered_at"));
            message.acknowledged = parse_bool(get_value("acknowledged"));
            message.metadata = json_to_map(safe_parse_json(get_value("metadata"), nlohmann::json::object()));
            messages.push_back(std::move(message));
        }
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error(
                "Failed to load conversation messages",
                "CommunicationMediator",
                __func__,
                {
                    {"conversation_id", conversation_id},
                    {"error", e.what()}
                }
            );
        }
    }

    return messages;
}

std::vector<ConflictResolution> CommunicationMediator::load_conflict_resolutions(const std::string& conversation_id) {
    std::vector<ConflictResolution> conflicts;

    if (!db_conn_) {
        return conflicts;
    }

    try {
        std::string query = R"(
            SELECT conflict_id, conflict_type, description, involved_agents,
                   strategy_used, conflict_details, resolution_result,
                   detected_at, resolved_at, resolved_successfully, resolution_summary
            FROM conflict_resolutions
            WHERE conversation_id = $1
            ORDER BY detected_at ASC
        )";

        auto result = db_conn_->execute_query(query, {conversation_id});
        for (const auto& row : result.rows) {
            auto get_value = [&row](const std::string& key) -> std::string {
                auto it = row.find(key);
                return it != row.end() ? it->second : "";
            };

            ConflictResolution conflict;
            conflict.conflict_id = get_value("conflict_id");
            conflict.conversation_id = conversation_id;
            conflict.conflict_type = static_cast<ConflictType>(safe_to_int(get_value("conflict_type")));
            conflict.description = get_value("description");
            auto agents_json = safe_parse_json(get_value("involved_agents"), nlohmann::json::array());
            if (agents_json.is_array()) {
                for (const auto& agent_entry : agents_json) {
                    if (agent_entry.is_string()) {
                        conflict.involved_agents.push_back(agent_entry.get<std::string>());
                    }
                }
            }
            conflict.strategy_used = static_cast<ResolutionStrategy>(safe_to_int(get_value("strategy_used")));
            conflict.conflict_details = safe_parse_json(get_value("conflict_details"), nlohmann::json::object());
            conflict.resolution_result = safe_parse_json(get_value("resolution_result"), nlohmann::json::object());
            conflict.detected_at = parse_timestamp(get_value("detected_at"));
            conflict.resolved_at = parse_timestamp(get_value("resolved_at"));
            conflict.resolved_successfully = parse_bool(get_value("resolved_successfully"));
            conflict.resolution_summary = get_value("resolution_summary");

            conflicts.push_back(std::move(conflict));
        }
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error(
                "Failed to load conflict resolutions",
                "CommunicationMediator",
                __func__,
                {
                    {"conversation_id", conversation_id},
                    {"error", e.what()}
                }
            );
        }
    }

    return conflicts;
}

void CommunicationMediator::log_mediation_event(const std::string& conversation_id,
                                                const std::string& event_type,
                                                const nlohmann::json& details) {
    if (logger_) {
        logger_->info(
            "Mediation event recorded",
            "communication_mediator",
            "log_mediation_event",
            {
                {"conversation_id", conversation_id},
                {"event", event_type}
            }
        );
        logger_->debug(
            "Mediation event payload",
            "communication_mediator",
            "log_mediation_event",
            {
                {"conversation_id", conversation_id},
                {"event", event_type},
                {"details", details.dump()}
            }
        );
    }

    if (!db_conn_) {
        return;
    }

    try {
        double metric_value = 0.0;
        if (details.contains("agreement_percentage")) {
            metric_value = details.at("agreement_percentage").get<double>();
        }

        std::string agent_id = details.value("agent_id", "");
        std::vector<std::string> params = {
            conversation_id,
            agent_id,
            event_type,
            std::to_string(metric_value)
        };

        db_conn_->execute_command(
            "INSERT INTO conversation_performance_metrics (conversation_id, agent_id, metric_type, metric_value) "
            "VALUES ($1, NULLIF($2,''), $3, $4)",
            params
        );
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->warn(
                "Failed to record mediation event",
                "communication_mediator",
                "log_mediation_event",
                {
                    {"conversation_id", conversation_id},
                    {"event", event_type},
                    {"error", e.what()}
                }
            );
        }
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
    if (!db_conn_) {
        return false;
    }

    try {
        std::vector<std::string> params = {message_id};
        bool updated = db_conn_->execute_command(
            "UPDATE conversation_messages SET delivered_at = NOW() WHERE message_id = $1 AND delivered_at IS NULL",
            params
        );

        db_conn_->execute_command(
            "UPDATE message_deliveries SET status = 'delivered', delivered_at = NOW() WHERE message_id = $1",
            params
        );

        if (!updated && logger_) {
            logger_->warn(
                "Message delivery status update had no effect",
                "CommunicationMediator",
                __func__,
                {{"message_id", message_id}}
            );
        }

        return true;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error(
                "Exception updating delivery status",
                "CommunicationMediator",
                __func__,
                {
                    {"message_id", message_id},
                    {"error", e.what()}
                }
            );
        }
        return false;
    }
}

} // namespace regulens
