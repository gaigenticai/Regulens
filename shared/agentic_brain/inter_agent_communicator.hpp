/**
 * Inter-Agent Communication System
 * Production-grade message-passing system for agent collaboration
 */

#ifndef INTER_AGENT_COMMUNICATOR_HPP
#define INTER_AGENT_COMMUNICATOR_HPP

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>
#include <nlohmann/json.hpp>
#include <mutex>
#include <condition_variable>
#include <queue>
#include "../database/postgresql_connection.hpp"

namespace regulens {

struct AgentMessage {
    std::string message_id;
    std::string from_agent_id;
    std::optional<std::string> to_agent_id; // NULL for broadcasts
    std::string message_type;
    nlohmann::json content;
    int priority = 3; // 1=urgent, 5=low
    std::string status = "pending";
    std::chrono::system_clock::time_point created_at;
    std::optional<std::chrono::system_clock::time_point> delivered_at;
    std::optional<std::chrono::system_clock::time_point> acknowledged_at;
    int retry_count = 0;
    int max_retries = 3;
    std::optional<std::chrono::system_clock::time_point> expires_at;
    std::optional<std::string> error_message;
    std::optional<std::string> correlation_id;
    std::optional<std::string> parent_message_id;
    std::optional<std::string> conversation_id;
};

struct MessageDeliveryResult {
    bool success = false;
    std::string message_id;
    std::string error_message;
    int retry_count = 0;
    bool will_retry = false;
    std::chrono::system_clock::time_point next_retry_at;
};

class InterAgentCommunicator {
public:
    InterAgentCommunicator(std::shared_ptr<PostgreSQLConnection> db_conn);
    ~InterAgentCommunicator();

    // Core messaging functions
    std::optional<std::string> send_message(
        const std::string& from_agent,
        const std::string& to_agent,
        const std::string& message_type,
        const nlohmann::json& content,
        int priority = 3,
        const std::optional<std::string>& correlation_id = std::nullopt,
        const std::optional<std::string>& conversation_id = std::nullopt,
        const std::optional<std::chrono::hours>& expiry_hours = std::nullopt
    );

    std::optional<std::string> send_message_async(
        const std::string& from_agent,
        const std::string& to_agent,
        const std::string& message_type,
        const nlohmann::json& content,
        int priority = 3,
        const std::optional<std::string>& correlation_id = std::nullopt,
        const std::optional<std::string>& conversation_id = std::nullopt,
        const std::optional<std::chrono::hours>& expiry_hours = std::nullopt
    );

    std::optional<std::string> broadcast_message(
        const std::string& from_agent,
        const std::string& message_type,
        const nlohmann::json& content,
        int priority = 3,
        const std::vector<std::string>& excluded_agents = {},
        const std::optional<std::string>& correlation_id = std::nullopt,
        const std::optional<std::chrono::hours>& expiry_hours = std::nullopt
    );

    std::vector<AgentMessage> receive_messages(
        const std::string& agent_id,
        int limit = 10,
        const std::optional<std::string>& message_type = std::nullopt
    );

    std::vector<AgentMessage> get_pending_messages(
        const std::string& agent_id,
        int limit = 50
    );

    bool acknowledge_message(
        const std::string& message_id,
        const std::string& agent_id
    );

    bool mark_message_read(
        const std::string& message_id,
        const std::string& agent_id
    );

    // Conversation management
    std::optional<std::string> start_conversation(
        const std::string& topic,
        const std::vector<std::string>& participant_agents,
        const std::string& priority = "normal",
        const std::optional<nlohmann::json>& metadata = std::nullopt,
        const std::optional<std::chrono::hours>& expiry_hours = std::nullopt
    );

    bool add_message_to_conversation(
        const std::string& message_id,
        const std::string& conversation_id
    );

    std::vector<AgentMessage> get_conversation_messages(
        const std::string& conversation_id,
        int limit = 100
    );

    bool update_conversation_activity(const std::string& conversation_id);

    // Message management
    bool update_message_status(
        const std::string& message_id,
        const std::string& new_status,
        const std::optional<std::string>& error_message = std::nullopt
    );

    bool retry_failed_message(const std::string& message_id);

    std::vector<AgentMessage> get_expired_messages(int limit = 100);

    bool cleanup_expired_messages();

    // Message templates
    bool save_message_template(
        const std::string& template_name,
        const std::string& message_type,
        const nlohmann::json& template_content,
        const std::string& description = "",
        const std::optional<std::string>& created_by = std::nullopt
    );

    std::optional<nlohmann::json> get_message_template(const std::string& template_name);

    std::vector<std::string> list_message_templates();

    // Message types
    bool validate_message_type(const std::string& message_type);

    std::optional<nlohmann::json> get_message_type_schema(const std::string& message_type);

    std::vector<std::string> get_supported_message_types();

    // Statistics and monitoring
    struct CommunicationStats {
        int total_messages_sent = 0;
        int total_messages_delivered = 0;
        int total_messages_failed = 0;
        int pending_messages = 0;
        int active_conversations = 0;
        double average_delivery_time_ms = 0.0;
        double delivery_success_rate = 0.0;
    };

    CommunicationStats get_communication_stats(
        const std::optional<std::string>& agent_id = std::nullopt,
        const std::optional<std::chrono::hours>& hours_back = std::nullopt
    );

    // Queue management for async processing
    void start_message_processor();
    void stop_message_processor();
    bool is_processor_running() const;

    // Configuration
    void set_max_retries(int max_retries);
    void set_retry_delay(std::chrono::seconds delay);
    void set_batch_size(int batch_size);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::string> message_queue_;
    std::atomic<bool> processor_running_;
    std::thread processor_thread_;

    // Scheduler state
    std::chrono::system_clock::time_point last_queue_refresh_{std::chrono::system_clock::now()};
    const std::chrono::seconds queue_refresh_interval_{std::chrono::seconds(5)};

    // Configuration
    int max_retries_ = 3;
    std::chrono::seconds retry_delay_ = std::chrono::seconds(30);
    int batch_size_ = 50;
    int cleanup_batch_size_ = 1000;

    // Internal helper methods
    std::optional<std::string> insert_message(const AgentMessage& message);
    std::optional<AgentMessage> get_message_by_id(const std::string& message_id);
    std::vector<AgentMessage> query_messages(const std::string& where_clause, int limit = 50);
    bool update_message_delivery_status(const std::string& message_id,
                                      const std::string& status,
                                      const std::optional<std::chrono::system_clock::time_point>& timestamp = std::nullopt);
    std::optional<AgentMessage> fetch_next_pending_message();
    MessageDeliveryResult attempt_delivery(const AgentMessage& message);
    void handle_delivery_failure(const AgentMessage& message,
                                 const std::string& error_code,
                                 const std::string& error_message);
    void log_delivery_attempt(const std::string& message_id,
                             int attempt_number,
                             const std::string& error_code = "",
                             const std::string& error_message = "");

    // Message processing thread
    void message_processor_loop();

    // Validation helpers
    bool validate_message_content(const std::string& message_type, const nlohmann::json& content);
    bool is_valid_priority(int priority) const;
    bool is_valid_status(const std::string& status) const;
};

} // namespace regulens

#endif // INTER_AGENT_COMMUNICATOR_HPP
