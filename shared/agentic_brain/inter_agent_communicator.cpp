/**
 * Inter-Agent Communication System Implementation
 * Production-grade message-passing system for agent collaboration
 */

#include "inter_agent_communicator.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <ctime>
#include <uuid/uuid.h>
#include <spdlog/spdlog.h>

namespace regulens {

namespace {

using Clock = std::chrono::system_clock;

std::optional<Clock::time_point> parse_timestamp(const std::string& value) {
    if (value.empty()) {
        return std::nullopt;
    }

    std::tm tm{};
    std::istringstream ss(value);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) {
        ss.clear();
        ss.str(value);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    }

    if (ss.fail()) {
        return std::nullopt;
    }

#if defined(_WIN32)
    time_t time_utc = _mkgmtime(&tm);
#else
    time_t time_utc = timegm(&tm);
#endif
    if (time_utc == static_cast<time_t>(-1)) {
        return std::nullopt;
    }

    Clock::time_point tp = Clock::from_time_t(time_utc);

    auto dot_pos = value.find('.');
    if (dot_pos != std::string::npos) {
        auto end = value.find_first_not_of("0123456789", dot_pos + 1);
        std::string fraction = value.substr(dot_pos + 1, end == std::string::npos ? std::string::npos : end - (dot_pos + 1));
        if (!fraction.empty()) {
            if (fraction.size() > 9) {
                fraction = fraction.substr(0, 9);
            }
            while (fraction.size() < 9) {
                fraction.push_back('0');
            }
            auto nanos = static_cast<long long>(std::stoll(fraction));
            tp += std::chrono::nanoseconds(nanos);
        }
    }

    return tp;
}

std::string format_timestamp(const Clock::time_point& tp) {
    auto time_val = Clock::to_time_t(tp);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &time_val);
#else
    gmtime_r(&time_val, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch()).count() % 1000000000LL;
    if (nanos > 0) {
        oss << '.' << std::setw(9) << std::setfill('0') << nanos;
    }
    oss << "+00";
    return oss.str();
}

AgentMessage build_agent_message(const std::unordered_map<std::string, std::string>& row) {
    AgentMessage msg;
    auto get_value = [&](const std::string& key) -> std::string {
        auto it = row.find(key);
        return it != row.end() ? it->second : std::string();
    };

    msg.message_id = get_value("message_id");
    msg.from_agent_id = get_value("from_agent_id");

    std::string to_agent = get_value("to_agent_id");
    if (!to_agent.empty()) {
        msg.to_agent_id = to_agent;
    }

    msg.message_type = get_value("message_type");

    std::string content_str = get_value("content");
    if (!content_str.empty()) {
        try {
            msg.content = nlohmann::json::parse(content_str);
        } catch (const std::exception&) {
            msg.content = nlohmann::json::object({{"raw", content_str}});
        }
    }

    std::string priority_str = get_value("priority");
    msg.priority = priority_str.empty() ? 3 : std::stoi(priority_str);

    std::string status_str = get_value("status");
    msg.status = status_str.empty() ? "pending" : status_str;

    if (auto created = parse_timestamp(get_value("created_at")); created) {
        msg.created_at = *created;
    } else {
        msg.created_at = Clock::now();
    }

    if (auto delivered = parse_timestamp(get_value("delivered_at")); delivered) {
        msg.delivered_at = delivered;
    }

    if (auto acknowledged = parse_timestamp(get_value("acknowledged_at")); acknowledged) {
        msg.acknowledged_at = acknowledged;
    }

    if (auto expires = parse_timestamp(get_value("expires_at")); expires) {
        msg.expires_at = expires;
    }

    std::string retry_str = get_value("retry_count");
    if (!retry_str.empty()) {
        msg.retry_count = std::stoi(retry_str);
    }

    std::string max_retry_str = get_value("max_retries");
    if (!max_retry_str.empty()) {
        msg.max_retries = std::stoi(max_retry_str);
    }

    std::string error_str = get_value("error_message");
    if (!error_str.empty()) {
        msg.error_message = error_str;
    }

    std::string correlation_str = get_value("correlation_id");
    if (!correlation_str.empty()) {
        msg.correlation_id = correlation_str;
    }

    std::string parent_str = get_value("parent_message_id");
    if (!parent_str.empty()) {
        msg.parent_message_id = parent_str;
    }

    std::string conversation_str = get_value("conversation_id");
    if (!conversation_str.empty()) {
        msg.conversation_id = conversation_str;
    }

    return msg;
}

} // namespace

InterAgentCommunicator::InterAgentCommunicator(std::shared_ptr<PostgreSQLConnection> db_conn)
    : db_conn_(db_conn), processor_running_(false) {
    if (!db_conn_) {
        throw std::runtime_error("Database connection is required for InterAgentCommunicator");
    }
    if (!db_conn_->is_connected()) {
        throw std::runtime_error("Database connection must be established before creating InterAgentCommunicator");
    }
}

InterAgentCommunicator::~InterAgentCommunicator() {
    stop_message_processor();
}

std::optional<std::string> InterAgentCommunicator::send_message(
    const std::string& from_agent,
    const std::string& to_agent,
    const std::string& message_type,
    const nlohmann::json& content,
    int priority,
    const std::optional<std::string>& correlation_id,
    const std::optional<std::string>& conversation_id,
    const std::optional<std::chrono::hours>& expiry_hours
) {
    try {
        // Validate inputs
        if (from_agent.empty() || to_agent.empty() || message_type.empty()) {
            spdlog::error("Invalid message parameters: from_agent, to_agent, and message_type are required");
            return std::nullopt;
        }

        if (!is_valid_priority(priority)) {
            spdlog::error("Invalid priority: {}. Must be between 1-5", priority);
            return std::nullopt;
        }

        if (!validate_message_type(message_type)) {
            spdlog::error("Unsupported message type: {}", message_type);
            return std::nullopt;
        }

        if (!validate_message_content(message_type, content)) {
            spdlog::error("Invalid message content for type: {}", message_type);
            return std::nullopt;
        }

        // Generate UUID for message_id
        uuid_t uuid;
        uuid_generate(uuid);
        char uuid_str[37];
        uuid_unparse(uuid, uuid_str);
        std::string message_id = uuid_str;

        // Prepare parameters for database insertion
        std::string content_json = content.dump();
        std::string corr_id = correlation_id.value_or("");
        std::string conv_id = conversation_id.value_or("");

        // Build INSERT query
        std::string query;
        std::vector<std::string> params;

        if (expiry_hours) {
            query = "INSERT INTO agent_messages (message_id, from_agent_id, to_agent_id, message_type, content, priority, status, correlation_id, conversation_id, expires_at, created_at) VALUES ($1, $2, $3, $4, $5::jsonb, $6, 'pending', $7, $8, NOW() + INTERVAL '" + std::to_string(expiry_hours->count()) + " hours', NOW())";
            params = {
                message_id, from_agent, to_agent, message_type, content_json,
                std::to_string(priority), corr_id, conv_id
            };
        } else {
            query = "INSERT INTO agent_messages (message_id, from_agent_id, to_agent_id, message_type, content, priority, status, correlation_id, conversation_id, created_at) VALUES ($1, $2, $3, $4, $5::jsonb, $6, 'pending', $7, $8, NOW())";
            params = {
                message_id, from_agent, to_agent, message_type, content_json,
                std::to_string(priority), corr_id, conv_id
            };
        }

        // Execute the query
        bool success = db_conn_->execute_command(query, params);
        if (success) {
            spdlog::info("Message sent successfully: {} from {} to {} (type: {})",
                        message_id, from_agent, to_agent, message_type);
            return message_id;
        }

        spdlog::error("Failed to insert message into database");
        return std::nullopt;

    } catch (const std::exception& e) {
        spdlog::error("Exception in send_message: {}", e.what());
        return std::nullopt;
    }
}

std::optional<std::string> InterAgentCommunicator::send_message_async(
    const std::string& from_agent,
    const std::string& to_agent,
    const std::string& message_type,
    const nlohmann::json& content,
    int priority,
    const std::optional<std::string>& correlation_id,
    const std::optional<std::string>& conversation_id,
    const std::optional<std::chrono::hours>& expiry_hours
) {
    try {
        auto message_id = send_message(from_agent, to_agent, message_type, content,
                                     priority, correlation_id, conversation_id, expiry_hours);
        if (message_id) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                message_queue_.push(*message_id);
            }
            cv_.notify_one();
        }
        return message_id;
    } catch (const std::exception& e) {
        spdlog::error("Exception in send_message_async: {}", e.what());
        return std::nullopt;
    }
}

std::optional<std::string> InterAgentCommunicator::broadcast_message(
    const std::string& from_agent,
    const std::string& message_type,
    const nlohmann::json& content,
    int priority,
    const std::vector<std::string>& excluded_agents,
    const std::optional<std::string>& correlation_id,
    const std::optional<std::chrono::hours>& expiry_hours
) {
    try {
        // Validate inputs
        if (from_agent.empty() || message_type.empty()) {
            spdlog::error("Invalid broadcast parameters: from_agent and message_type are required");
            return std::nullopt;
        }

        if (!is_valid_priority(priority)) {
            spdlog::error("Invalid priority: {}. Must be between 1-5", priority);
            return std::nullopt;
        }

        if (!validate_message_type(message_type)) {
            spdlog::error("Unsupported message type: {}", message_type);
            return std::nullopt;
        }

        if (!validate_message_content(message_type, content)) {
            spdlog::error("Invalid message content for type: {}", message_type);
            return std::nullopt;
        }

        // Generate UUID for message_id
        uuid_t uuid;
        uuid_generate(uuid);
        char uuid_str[37];
        uuid_unparse(uuid, uuid_str);
        std::string message_id = uuid_str;

        // Prepare parameters for database insertion
        std::string content_json = content.dump();
        std::string corr_id = correlation_id.value_or("");

        // Build INSERT query for broadcast message (to_agent_id = NULL)
        std::string query;
        std::vector<std::string> params;

        if (expiry_hours) {
            query = "INSERT INTO agent_messages (message_id, from_agent_id, to_agent_id, message_type, content, priority, status, correlation_id, expires_at, created_at) VALUES ($1, $2, NULL, $3, $4::jsonb, $5, 'pending', $6, NOW() + INTERVAL '" + std::to_string(expiry_hours->count()) + " hours', NOW())";
            params = {
                message_id, from_agent, message_type, content_json,
                std::to_string(priority), corr_id
            };
        } else {
            query = "INSERT INTO agent_messages (message_id, from_agent_id, to_agent_id, message_type, content, priority, status, correlation_id, created_at) VALUES ($1, $2, NULL, $3, $4::jsonb, $5, 'pending', $6, NOW())";
            params = {
                message_id, from_agent, message_type, content_json,
                std::to_string(priority), corr_id
            };
        }

        // Execute the query
        bool success = db_conn_->execute_command(query, params);
        if (success) {
            spdlog::info("Broadcast message sent successfully: {} from {} (type: {})",
                        message_id, from_agent, message_type);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                message_queue_.push(message_id);
            }
            cv_.notify_one();
            return message_id;
        }

        spdlog::error("Failed to insert broadcast message into database");
        return std::nullopt;

    } catch (const std::exception& e) {
        spdlog::error("Exception in broadcast_message: {}", e.what());
        return std::nullopt;
    }
}

std::vector<AgentMessage> InterAgentCommunicator::receive_messages(
    const std::string& agent_id,
    int limit,
    const std::optional<std::string>& message_type
) {
    try {
        std::string where_clause = "(to_agent_id = '" + agent_id + "' OR to_agent_id IS NULL) AND status = 'pending'";
        if (message_type) {
            where_clause += " AND message_type = '" + *message_type + "'";
        }
        where_clause += " AND (expires_at IS NULL OR expires_at > NOW())";

        auto messages = query_messages(where_clause, limit);

        // Mark messages as delivered
        for (const auto& msg : messages) {
            std::string update_query = "UPDATE agent_messages SET status = 'delivered', delivered_at = NOW() WHERE message_id = $1";
            std::vector<std::string> update_params = {msg.message_id};
            db_conn_->execute_command(update_query, update_params);
        }

        spdlog::info("Retrieved {} messages for agent {}", messages.size(), agent_id);
        return messages;

    } catch (const std::exception& e) {
        spdlog::error("Exception in receive_messages: {}", e.what());
        return {};
    }
}

std::vector<AgentMessage> InterAgentCommunicator::get_pending_messages(
    const std::string& agent_id,
    int limit
) {
    try {
        std::string where_clause = "to_agent_id = '" + agent_id + "' AND status = 'pending'";
        where_clause += " AND (expires_at IS NULL OR expires_at > NOW())";

        return query_messages(where_clause, limit);

    } catch (const std::exception& e) {
        spdlog::error("Exception in get_pending_messages: {}", e.what());
        return {};
    }
}

bool InterAgentCommunicator::acknowledge_message(
    const std::string& message_id,
    const std::string& agent_id
) {
    try {
        // First verify the message belongs to the agent
        std::string verify_query = "SELECT to_agent_id FROM agent_messages WHERE message_id = $1 AND status = 'delivered'";
        std::vector<std::string> verify_params = {message_id};

        auto verify_result = db_conn_->execute_query(verify_query, verify_params);

        if (verify_result.rows.empty()) {
            spdlog::warn("Message {} not found or not in delivered state", message_id);
            return false;
        }

        std::string target_agent = verify_result.rows[0].at("to_agent_id");
        if (!target_agent.empty() && target_agent != agent_id) {
            spdlog::error("Agent {} attempted to acknowledge message {} belonging to {}",
                         agent_id, message_id, target_agent);
            return false;
        }

        // Update the message status
        std::string update_query = "UPDATE agent_messages SET status = 'acknowledged', acknowledged_at = NOW(), updated_at = NOW() WHERE message_id = $1";
        std::vector<std::string> update_params = {message_id};

        bool update_success = db_conn_->execute_command(update_query, update_params);

        if (update_success) {
            spdlog::info("Message {} acknowledged by agent {}", message_id, agent_id);
            return true;
        } else {
            spdlog::error("Failed to update message status for {}", message_id);
            return false;
        }

    } catch (const std::exception& e) {
        spdlog::error("Exception in acknowledge_message: {}", e.what());
        return false;
    }
}

bool InterAgentCommunicator::mark_message_read(
    const std::string& message_id,
    const std::string& agent_id
) {
    try {
        pqxx::work txn(*db_conn_);

        // Verify ownership and update
        pqxx::result result = txn.exec_params(
            "UPDATE agent_messages SET read_at = NOW(), updated_at = NOW() "
            "WHERE message_id = $1 AND to_agent_id = $2 AND read_at IS NULL",
            message_id, agent_id
        );

        txn.commit();

        bool updated = result.affected_rows() > 0;
        if (updated) {
            spdlog::debug("Message {} marked as read by agent {}", message_id, agent_id);
        }

        return updated;

    } catch (const std::exception& e) {
        spdlog::error("Exception in mark_message_read: {}", e.what());
        return false;
    }
}

std::optional<std::string> InterAgentCommunicator::start_conversation(
    const std::string& topic,
    const std::vector<std::string>& participant_agents,
    const std::string& priority,
    const std::optional<nlohmann::json>& metadata,
    const std::optional<std::chrono::hours>& expiry_hours
) {
    try {
        if (topic.empty() || participant_agents.empty()) {
            spdlog::error("Topic and participants are required for conversation");
            return std::nullopt;
        }

        // Generate conversation ID
        uuid_t uuid;
        uuid_generate(uuid);
        char uuid_str[37];
        uuid_unparse(uuid, uuid_str);
        std::string conversation_id = uuid_str;

        pqxx::work txn(*db_conn_);

        // Convert participant array to PostgreSQL array format
        std::stringstream participants_stream;
        participants_stream << "{";
        for (size_t i = 0; i < participant_agents.size(); ++i) {
            if (i > 0) participants_stream << ",";
            participants_stream << "\"" << participant_agents[i] << "\"";
        }
        participants_stream << "}";

        // Insert conversation
        std::string metadata_str = metadata ? metadata->dump() : "NULL";
        std::string expires_at_str = expiry_hours ?
            "(NOW() + INTERVAL '" + std::to_string(expiry_hours->count()) + " hours')" : "NULL";

        if (metadata) {
            std::string query = "INSERT INTO agent_conversations "
                               "(conversation_id, topic, participant_agents, priority, metadata, expires_at) "
                               "VALUES ($1, $2, $3, $4, $5, " + expires_at_str + ")";
            txn.exec_params(query, conversation_id, topic, participants_stream.str(), priority, metadata_str);
        } else {
            std::string query = "INSERT INTO agent_conversations "
                               "(conversation_id, topic, participant_agents, priority, expires_at) "
                               "VALUES ($1, $2, $3, $4, " + expires_at_str + ")";
            txn.exec_params(query, conversation_id, topic, participants_stream.str(), priority);
        }

        txn.commit();

        spdlog::info("Conversation started: {} with {} participants", conversation_id, participant_agents.size());
        return conversation_id;

    } catch (const std::exception& e) {
        spdlog::error("Exception in start_conversation: {}", e.what());
        return std::nullopt;
    }
}

bool InterAgentCommunicator::add_message_to_conversation(
    const std::string& message_id,
    const std::string& conversation_id
) {
    try {
        pqxx::work txn(*db_conn_);

        // Update message with conversation ID
        pqxx::result result = txn.exec_params(
            "UPDATE agent_messages SET conversation_id = $1, updated_at = NOW() WHERE message_id = $2",
            conversation_id, message_id
        );

        // Update conversation activity
        txn.exec_params(
            "UPDATE agent_conversations SET last_activity = NOW(), message_count = message_count + 1 WHERE conversation_id = $1",
            conversation_id
        );

        txn.commit();

        bool updated = result.affected_rows() > 0;
        if (updated) {
            spdlog::debug("Message {} added to conversation {}", message_id, conversation_id);
        }

        return updated;

    } catch (const std::exception& e) {
        spdlog::error("Exception in add_message_to_conversation: {}", e.what());
        return false;
    }
}

std::vector<AgentMessage> InterAgentCommunicator::get_conversation_messages(
    const std::string& conversation_id,
    int limit
) {
    try {
        std::string where_clause = "conversation_id = '" + conversation_id + "'";
        return query_messages(where_clause, limit);

    } catch (const std::exception& e) {
        spdlog::error("Exception in get_conversation_messages: {}", e.what());
        return {};
    }
}

bool InterAgentCommunicator::update_conversation_activity(const std::string& conversation_id) {
    try {
        pqxx::work txn(*db_conn_);

        txn.exec_params(
            "UPDATE agent_conversations SET last_activity = NOW() WHERE conversation_id = $1",
            conversation_id
        );

        txn.commit();
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Exception in update_conversation_activity: {}", e.what());
        return false;
    }
}

bool InterAgentCommunicator::update_message_status(
    const std::string& message_id,
    const std::string& new_status,
    const std::optional<std::string>& error_message
) {
    try {
        if (!is_valid_status(new_status)) {
            spdlog::error("Invalid status: {}", new_status);
            return false;
        }

        pqxx::work txn(*db_conn_);

        std::string query = "UPDATE agent_messages SET status = $1, updated_at = NOW()";
        if (error_message) {
            query += ", error_message = $2";
        }
        query += " WHERE message_id = $3";

        if (error_message) {
            txn.exec_params(query, new_status, *error_message, message_id);
        } else {
            txn.exec_params(query, new_status, message_id);
        }

        txn.commit();

        spdlog::debug("Message {} status updated to {}", message_id, new_status);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Exception in update_message_status: {}", e.what());
        return false;
    }
}

bool InterAgentCommunicator::retry_failed_message(const std::string& message_id) {
    try {
        pqxx::work txn(*db_conn_);

        // Get current retry count
        pqxx::result result = txn.exec_params(
            "SELECT retry_count, max_retries FROM agent_messages WHERE message_id = $1",
            message_id
        );

        if (result.empty()) {
            spdlog::error("Message {} not found for retry", message_id);
            return false;
        }

        int retry_count = result[0][0].as<int>();
        int max_retries = result[0][1].as<int>();

        if (retry_count >= max_retries) {
            spdlog::warn("Message {} has exceeded max retries ({})", message_id, max_retries);
            return false;
        }

        // Reset status to pending and increment retry count
        txn.exec_params(
            "UPDATE agent_messages SET status = 'pending', retry_count = retry_count + 1, updated_at = NOW() WHERE message_id = $1",
            message_id
        );

        txn.commit();

        spdlog::info("Message {} scheduled for retry (attempt {})", message_id, retry_count + 1);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Exception in retry_failed_message: {}", e.what());
        return false;
    }
}

std::vector<AgentMessage> InterAgentCommunicator::get_expired_messages(int limit) {
    try {
        std::string where_clause = "expires_at IS NOT NULL AND expires_at <= NOW() AND status NOT IN ('expired', 'acknowledged')";
        return query_messages(where_clause, limit);

    } catch (const std::exception& e) {
        spdlog::error("Exception in get_expired_messages: {}", e.what());
        return {};
    }
}

bool InterAgentCommunicator::cleanup_expired_messages() {
    try {
        pqxx::work txn(*db_conn_);

        // Mark expired messages
        pqxx::result result = txn.exec_params(
            "UPDATE agent_messages SET status = 'expired', updated_at = NOW() "
            "WHERE expires_at IS NOT NULL AND expires_at <= NOW() AND status NOT IN ('expired', 'acknowledged')"
        );

        txn.commit();

        int updated_count = result.affected_rows();
        if (updated_count > 0) {
            spdlog::info("Cleaned up {} expired messages", updated_count);
        }

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Exception in cleanup_expired_messages: {}", e.what());
        return false;
    }
}

bool InterAgentCommunicator::save_message_template(
    const std::string& template_name,
    const std::string& message_type,
    const nlohmann::json& template_content,
    const std::string& description,
    const std::optional<std::string>& created_by
) {
    try {
        pqxx::work txn(*db_conn_);

        txn.exec_params(
            "INSERT INTO message_templates (template_name, message_type, template_content, description, created_by) "
            "VALUES ($1, $2, $3, $4, $5) "
            "ON CONFLICT (template_name) DO UPDATE SET "
            "message_type = EXCLUDED.message_type, "
            "template_content = EXCLUDED.template_content, "
            "description = EXCLUDED.description, "
            "updated_at = NOW()",
            template_name, message_type, template_content.dump(), description, created_by.value_or("")
        );

        txn.commit();

        spdlog::info("Message template '{}' saved", template_name);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Exception in save_message_template: {}", e.what());
        return false;
    }
}

std::optional<nlohmann::json> InterAgentCommunicator::get_message_template(const std::string& template_name) {
    try {
        pqxx::work txn(*db_conn_);

        pqxx::result result = txn.exec_params(
            "SELECT template_content FROM message_templates WHERE template_name = $1 AND is_active = true",
            template_name
        );

        if (!result.empty()) {
            std::string content_str = result[0][0].as<std::string>();
            return nlohmann::json::parse(content_str);
        }

        return std::nullopt;

    } catch (const std::exception& e) {
        spdlog::error("Exception in get_message_template: {}", e.what());
        return std::nullopt;
    }
}

std::vector<std::string> InterAgentCommunicator::list_message_templates() {
    try {
        pqxx::work txn(*db_conn_);

        pqxx::result result = txn.exec_params(
            "SELECT template_name FROM message_templates WHERE is_active = true ORDER BY template_name"
        );

        std::vector<std::string> templates;
        for (const auto& row : result) {
            templates.push_back(row[0].as<std::string>());
        }

        return templates;

    } catch (const std::exception& e) {
        spdlog::error("Exception in list_message_templates: {}", e.what());
        return {};
    }
}

bool InterAgentCommunicator::validate_message_type(const std::string& message_type) {
    try {
        pqxx::work txn(*db_conn_);

        pqxx::result result = txn.exec_params(
            "SELECT COUNT(*) FROM message_types WHERE message_type = $1",
            message_type
        );

        return result[0][0].as<int>() > 0;

    } catch (const std::exception& e) {
        spdlog::error("Exception in validate_message_type: {}", e.what());
        return false;
    }
}

std::optional<nlohmann::json> InterAgentCommunicator::get_message_type_schema(const std::string& message_type) {
    try {
        pqxx::work txn(*db_conn_);

        pqxx::result result = txn.exec_params(
            "SELECT schema_definition FROM message_types WHERE message_type = $1",
            message_type
        );

        if (!result.empty() && !result[0][0].is_null()) {
            std::string schema_str = result[0][0].as<std::string>();
            return nlohmann::json::parse(schema_str);
        }

        return std::nullopt;

    } catch (const std::exception& e) {
        spdlog::error("Exception in get_message_type_schema: {}", e.what());
        return std::nullopt;
    }
}

std::vector<std::string> InterAgentCommunicator::get_supported_message_types() {
    try {
        pqxx::work txn(*db_conn_);

        pqxx::result result = txn.exec_params(
            "SELECT message_type FROM message_types ORDER BY message_type"
        );

        std::vector<std::string> types;
        for (const auto& row : result) {
            types.push_back(row[0].as<std::string>());
        }

        return types;

    } catch (const std::exception& e) {
        spdlog::error("Exception in get_supported_message_types: {}", e.what());
        return {};
    }
}

InterAgentCommunicator::CommunicationStats InterAgentCommunicator::get_communication_stats(
    const std::optional<std::string>& agent_id,
    const std::optional<std::chrono::hours>& hours_back
) {
    CommunicationStats stats;

    try {
        pqxx::work txn(*db_conn_);

        std::string time_filter = hours_back ?
            " AND created_at >= (NOW() - INTERVAL '" + std::to_string(hours_back->count()) + " hours')" : "";

        std::string agent_filter = agent_id ? " AND from_agent_id = '" + *agent_id + "'" : "";

        // Total messages sent
        pqxx::result sent_result = txn.exec(
            "SELECT COUNT(*) FROM agent_messages WHERE 1=1" + time_filter + agent_filter
        );
        stats.total_messages_sent = sent_result[0][0].as<int>();

        // Messages delivered
        pqxx::result delivered_result = txn.exec(
            "SELECT COUNT(*) FROM agent_messages WHERE status IN ('delivered', 'acknowledged', 'read')" + time_filter + agent_filter
        );
        stats.total_messages_delivered = delivered_result[0][0].as<int>();

        // Messages failed
        pqxx::result failed_result = txn.exec(
            "SELECT COUNT(*) FROM agent_messages WHERE status = 'failed'" + time_filter + agent_filter
        );
        stats.total_messages_failed = failed_result[0][0].as<int>();

        // Pending messages (if agent specified)
        if (agent_id) {
            pqxx::result pending_result = txn.exec(
                "SELECT COUNT(*) FROM agent_messages WHERE to_agent_id = '" + *agent_id + "' AND status = 'pending'"
            );
            stats.pending_messages = pending_result[0][0].as<int>();
        }

        // Active conversations
        pqxx::result conv_result = txn.exec(
            "SELECT COUNT(*) FROM agent_conversations WHERE status = 'active'"
        );
        stats.active_conversations = conv_result[0][0].as<int>();

        // Calculate rates
        if (stats.total_messages_sent > 0) {
            stats.delivery_success_rate = static_cast<double>(stats.total_messages_delivered) / stats.total_messages_sent;
        }

    } catch (const std::exception& e) {
        spdlog::error("Exception in get_communication_stats: {}", e.what());
    }

    return stats;
}

void InterAgentCommunicator::start_message_processor() {
    if (processor_running_) {
        return;
    }

    processor_running_ = true;
    processor_thread_ = std::thread(&InterAgentCommunicator::message_processor_loop, this);
    spdlog::info("Message processor started");
}

void InterAgentCommunicator::stop_message_processor() {
    if (!processor_running_) {
        return;
    }

    processor_running_ = false;
    cv_.notify_one();

    if (processor_thread_.joinable()) {
        processor_thread_.join();
    }

    spdlog::info("Message processor stopped");
}

bool InterAgentCommunicator::is_processor_running() const {
    return processor_running_;
}

void InterAgentCommunicator::set_max_retries(int max_retries) {
    max_retries_ = std::max(0, max_retries);
}

void InterAgentCommunicator::set_retry_delay(std::chrono::seconds delay) {
    retry_delay_ = delay;
}

void InterAgentCommunicator::set_batch_size(int batch_size) {
    batch_size_ = std::max(1, batch_size);
}

// Private helper methods implementation
std::optional<std::string> InterAgentCommunicator::insert_message(const AgentMessage& message) {
    try {
        // Build the INSERT query
        std::string query = "INSERT INTO agent_messages "
                           "(message_id, from_agent_id, to_agent_id, message_type, content, "
                           "priority, correlation_id, conversation_id, expires_at) "
                           "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9)";

        std::vector<std::string> params = {
            message.message_id,
            message.from_agent_id,
            message.to_agent_id.value_or(""),
            message.message_type,
            message.content.dump(),
            std::to_string(message.priority),
            message.correlation_id.value_or(""),
            message.conversation_id.value_or(""),
            message.expires_at ?
                std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                    message.expires_at->time_since_epoch()).count()) : ""
        };

        bool success = db_conn_->execute_command(query, params);
        if (success) {
            spdlog::info("Message inserted: {}", message.message_id);
            return message.message_id;
        }

        spdlog::error("Failed to insert message");
        return std::nullopt;

    } catch (const std::exception& e) {
        spdlog::error("Exception in insert_message: {}", e.what());
        return std::nullopt;
    }
}

std::vector<AgentMessage> InterAgentCommunicator::query_messages(const std::string& where_clause, int limit) {
    std::vector<AgentMessage> messages;

    try {
        std::string query =
            "SELECT message_id, from_agent_id, to_agent_id, message_type, content, "
            "priority, status, created_at, delivered_at, acknowledged_at, read_at, "
            "retry_count, max_retries, expires_at, error_message, correlation_id, "
            "parent_message_id, conversation_id "
            "FROM agent_messages WHERE " + where_clause +
            " ORDER BY priority ASC, created_at ASC LIMIT " + std::to_string(limit);

        auto result = db_conn_->execute_query(query);
        messages.reserve(result.rows.size());
        for (const auto& row : result.rows) {
            messages.push_back(build_agent_message(row));
        }

    } catch (const std::exception& e) {
        spdlog::error("Exception in query_messages: {}", e.what());
    }

    return messages;
}

std::optional<AgentMessage> InterAgentCommunicator::get_message_by_id(const std::string& message_id) {
    try {
        auto result = db_conn_->execute_query(
            "SELECT message_id, from_agent_id, to_agent_id, message_type, content, "
            "priority, status, created_at, delivered_at, acknowledged_at, read_at, "
            "retry_count, max_retries, expires_at, error_message, correlation_id, "
            "parent_message_id, conversation_id "
            "FROM agent_messages WHERE message_id = $1 LIMIT 1",
            {message_id}
        );

        if (!result.rows.empty()) {
            return build_agent_message(result.rows.front());
        }

    } catch (const std::exception& e) {
        spdlog::error("Exception in get_message_by_id: {}", e.what());
    }

    return std::nullopt;
}

bool InterAgentCommunicator::update_message_delivery_status(
    const std::string& message_id,
    const std::string& status,
    const std::optional<std::chrono::system_clock::time_point>& timestamp
) {
    if (!is_valid_status(status)) {
        spdlog::error("Attempted to set invalid message status '{}'", status);
        return false;
    }

    try {
        pqxx::work txn(*db_conn_);
        pqxx::result result;

        const auto effective_ts = timestamp.value_or(Clock::now());
        const std::string ts_value = format_timestamp(effective_ts);

        if (status == "delivered") {
            result = txn.exec_params(
                "UPDATE agent_messages SET status = $1, delivered_at = $2::timestamptz, updated_at = NOW() WHERE message_id = $3",
                status, ts_value, message_id
            );
        } else if (status == "acknowledged") {
            result = txn.exec_params(
                "UPDATE agent_messages SET status = $1, acknowledged_at = $2::timestamptz, updated_at = NOW() WHERE message_id = $3",
                status, ts_value, message_id
            );
        } else if (status == "read") {
            result = txn.exec_params(
                "UPDATE agent_messages SET status = $1, read_at = $2::timestamptz, updated_at = NOW() WHERE message_id = $3",
                status, ts_value, message_id
            );
        } else {
            result = txn.exec_params(
                "UPDATE agent_messages SET status = $1, updated_at = NOW() WHERE message_id = $2",
                status, message_id
            );
        }

        txn.commit();

        bool updated = result.affected_rows() > 0;
        if (!updated) {
            spdlog::warn("Message {} status update to '{}' affected 0 rows", message_id, status);
        }
        return updated;

    } catch (const std::exception& e) {
        spdlog::error("Exception in update_message_delivery_status: {}", e.what());
        return false;
    }
}

std::optional<AgentMessage> InterAgentCommunicator::fetch_next_pending_message() {
    try {
        auto messages = query_messages(
            "status = 'pending' AND (expires_at IS NULL OR expires_at > NOW())",
            1
        );
        if (!messages.empty()) {
            return messages.front();
        }
    } catch (const std::exception& e) {
        spdlog::error("Exception in fetch_next_pending_message: {}", e.what());
    }
    return std::nullopt;
}

MessageDeliveryResult InterAgentCommunicator::attempt_delivery(const AgentMessage& message) {
    MessageDeliveryResult result;
    result.message_id = message.message_id;
    result.retry_count = message.retry_count;

    try {
        auto delivered_ts = Clock::now();

        if (!update_message_delivery_status(message.message_id, "delivered", delivered_ts)) {
            result.error_message = "Failed to update delivery status";
            result.will_retry = message.retry_count + 1 < message.max_retries;
            return result;
        }

        if (message.to_agent_id && !message.to_agent_id->empty()) {
            pqxx::work txn(*db_conn_);
            txn.exec_params(
                "INSERT INTO message_deliveries (message_id, agent_id, delivered_at, status) "
                "VALUES ($1, $2, $3::timestamptz, 'delivered') "
                "ON CONFLICT (message_id, agent_id) DO UPDATE SET delivered_at = EXCLUDED.delivered_at, status = 'delivered'",
                message.message_id,
                *message.to_agent_id,
                format_timestamp(delivered_ts)
            );
            txn.commit();
        }

        spdlog::debug("Message {} delivered to {}", message.message_id,
                      message.to_agent_id ? *message.to_agent_id : std::string("broadcast"));

        result.success = true;
        return result;

    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
        result.will_retry = message.retry_count + 1 < message.max_retries;
        return result;
    }
}

void InterAgentCommunicator::handle_delivery_failure(
    const AgentMessage& message,
    const std::string& error_code,
    const std::string& error_message
) {
    spdlog::warn("Delivery failed for message {}: {}", message.message_id, error_message);
    log_delivery_attempt(message.message_id, message.retry_count + 1, error_code, error_message);

    update_message_status(message.message_id, "failed", error_message);

    if (message.retry_count + 1 >= message.max_retries) {
        spdlog::error("Message {} reached max retries ({}). Marking as failed.",
                      message.message_id, message.max_retries);
        return;
    }

    if (retry_failed_message(message.message_id)) {
        std::lock_guard<std::mutex> lock(mutex_);
        message_queue_.push(message.message_id);
        cv_.notify_one();
    }
}

bool InterAgentCommunicator::validate_message_content(const std::string& message_type, const nlohmann::json& content) {
    // Basic validation - in production, this would use JSON schema validation
    if (!content.is_object()) {
        return false;
    }

    // Add specific validation rules per message type
    if (message_type == "TASK_ASSIGNMENT") {
        return content.contains("task_description") && content.contains("priority");
    } else if (message_type == "DATA_REQUEST") {
        return content.contains("data_type") && content.contains("query_parameters");
    }

    // Default: accept any valid JSON object
    return true;
}

bool InterAgentCommunicator::is_valid_priority(int priority) const {
    return priority >= 1 && priority <= 5;
}

bool InterAgentCommunicator::is_valid_status(const std::string& status) const {
    static const std::vector<std::string> valid_statuses = {
        "pending", "delivered", "acknowledged", "read", "failed", "expired"
    };
    return std::find(valid_statuses.begin(), valid_statuses.end(), status) != valid_statuses.end();
}

void InterAgentCommunicator::message_processor_loop() {
    spdlog::info("Message processor loop started");

    while (processor_running_) {
        try {
            std::optional<AgentMessage> message;

            std::string queued_message_id;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (!message_queue_.empty()) {
                    queued_message_id = message_queue_.front();
                    message_queue_.pop();
                }
            }

            if (!queued_message_id.empty()) {
                message = get_message_by_id(queued_message_id);
                if (!message) {
                    spdlog::warn("Queued message {} could not be loaded", queued_message_id);
                    continue;
                }
            } else {
                auto now = Clock::now();
                if (now - last_queue_refresh_ >= queue_refresh_interval_) {
                    message = fetch_next_pending_message();
                    last_queue_refresh_ = now;
                }

                if (!message) {
                    std::unique_lock<std::mutex> lock(mutex_);
                    cv_.wait_for(lock, queue_refresh_interval_, [this]() {
                        return !message_queue_.empty() || !processor_running_;
                    });
                    continue;
                }
            }

            auto delivery_result = attempt_delivery(*message);
            if (!delivery_result.success) {
                handle_delivery_failure(
                    *message,
                    "DELIVERY_ERROR",
                    delivery_result.error_message.empty() ? "Unknown delivery failure" : delivery_result.error_message
                );
            } else if (queued_message_id.empty()) {
                // Allow immediate follow-up pulls from backlog when we sourced directly from the database
                last_queue_refresh_ = Clock::now() - queue_refresh_interval_;
            }

        } catch (const std::exception& e) {
            spdlog::error("Exception in message processor loop: {}", e.what());
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    spdlog::info("Message processor loop ended");
}

void InterAgentCommunicator::log_delivery_attempt(
    const std::string& message_id,
    int attempt_number,
    const std::string& error_code,
    const std::string& error_message
) {
    try {
        pqxx::work txn(*db_conn_);

        txn.exec_params(
            "INSERT INTO message_delivery_attempts (message_id, attempt_number, error_code, error_message) "
            "VALUES ($1, $2, $3, $4)",
            message_id, attempt_number, error_code, error_message
        );

        txn.commit();

    } catch (const std::exception& e) {
        spdlog::error("Exception in log_delivery_attempt: {}", e.what());
    }
}

} // namespace regulens
