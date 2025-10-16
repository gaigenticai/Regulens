/**
 * Real-Time Agent Collaboration Streaming Service
 * 
 * Production-grade WebSocket streaming for agent decision-making visualization
 * Enables real-time "thinking" streams, confidence breakdowns, and human oversight
 * 
 * Features:
 * - Live agent reasoning stream (ChatGPT-style streaming)
 * - Real-time confidence metrics
 * - Human override capability with decision interruption
 * - Multi-agent collaboration coordination
 * - Session management and persistence
 * 
 * NO STUBS - Full production implementation
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <functional>
#include <nlohmann/json.hpp>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"

namespace regulens {
namespace collaboration {

// Forward declarations
class WebSocketSession;

/**
 * Reasoning step in agent decision-making process
 */
struct ReasoningStep {
    std::string stream_id;
    std::string session_id;
    std::string agent_id;
    std::string agent_name;
    std::string agent_type;
    std::string reasoning_step;
    int step_number;
    std::string step_type; // 'thinking', 'analyzing', 'deciding', 'executing', 'completed', 'error'
    double confidence_score;
    std::chrono::system_clock::time_point timestamp;
    int duration_ms;
    nlohmann::json metadata;
    std::string parent_step_id;
    
    nlohmann::json to_json() const;
    static ReasoningStep from_json(const nlohmann::json& j);
};

/**
 * Confidence metric breakdown for transparency
 */
struct ConfidenceMetric {
    std::string metric_id;
    std::string session_id;
    std::string decision_id;
    std::string stream_id;
    std::string metric_type; // 'data_quality', 'model_confidence', 'rule_match', 'historical_accuracy', 'consensus'
    std::string metric_name;
    double metric_value;
    double weight;
    std::vector<std::string> contributing_factors;
    std::chrono::system_clock::time_point calculated_at;
    
    nlohmann::json to_json() const;
    static ConfidenceMetric from_json(const nlohmann::json& j);
};

/**
 * Human override of agent decision
 */
struct HumanOverride {
    std::string override_id;
    std::string decision_id;
    std::string session_id;
    std::string user_id;
    std::string user_name;
    std::string original_decision;
    std::string override_decision;
    std::string reason;
    std::string justification;
    nlohmann::json impact_assessment;
    std::chrono::system_clock::time_point timestamp;
    nlohmann::json metadata;
    
    nlohmann::json to_json() const;
    static HumanOverride from_json(const nlohmann::json& j);
};

/**
 * Collaboration session for multi-agent coordination
 */
struct CollaborationSession {
    std::string session_id;
    std::string title;
    std::string description;
    std::string objective;
    std::string status; // 'active', 'paused', 'completed', 'archived', 'cancelled'
    std::string created_by;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
    std::chrono::system_clock::time_point started_at;
    std::chrono::system_clock::time_point completed_at;
    std::vector<std::string> agent_ids;
    nlohmann::json context;
    nlohmann::json settings;
    nlohmann::json metadata;
    
    // Runtime state (not persisted)
    std::atomic<int> active_streams{0};
    std::atomic<int> total_steps{0};
    
    nlohmann::json to_json() const;
    static CollaborationSession from_json(const nlohmann::json& j);
};

/**
 * Agent participant in collaboration session
 */
struct CollaborationAgent {
    std::string participant_id;
    std::string session_id;
    std::string agent_id;
    std::string agent_name;
    std::string agent_type;
    std::string role; // 'participant', 'observer', 'facilitator', 'leader'
    std::string status; // 'active', 'inactive', 'disconnected', 'completed'
    std::chrono::system_clock::time_point joined_at;
    std::chrono::system_clock::time_point left_at;
    int contribution_count;
    std::chrono::system_clock::time_point last_activity_at;
    nlohmann::json performance_metrics;
    
    nlohmann::json to_json() const;
    static CollaborationAgent from_json(const nlohmann::json& j);
};

/**
 * WebSocket stream subscriber for real-time updates
 */
class StreamSubscriber {
public:
    using MessageCallback = std::function<void(const nlohmann::json&)>;
    
    StreamSubscriber(const std::string& subscriber_id, const std::string& session_id);
    virtual ~StreamSubscriber() = default;
    
    void send_message(const nlohmann::json& message);
    void set_callback(MessageCallback callback);
    
    const std::string& get_id() const { return subscriber_id_; }
    const std::string& get_session_id() const { return session_id_; }
    bool is_connected() const { return connected_.load(); }
    void disconnect();
    
private:
    std::string subscriber_id_;
    std::string session_id_;
    std::atomic<bool> connected_{true};
    MessageCallback callback_;
    std::mutex callback_mutex_;
};

/**
 * Production-grade Agent Streaming Service
 * 
 * Manages real-time WebSocket streams for agent collaboration
 * NO STUBS - Fully functional implementation
 */
class AgentStreamingService {
public:
    AgentStreamingService(
        std::shared_ptr<ConnectionPool> db_pool,
        StructuredLogger* logger
    );
    
    ~AgentStreamingService();
    
    // Session management
    std::string create_session(
        const std::string& title,
        const std::string& description,
        const std::string& objective,
        const std::vector<std::string>& agent_ids,
        const std::string& created_by,
        const nlohmann::json& context = nlohmann::json::object(),
        const nlohmann::json& settings = nlohmann::json::object()
    );
    
    CollaborationSession get_session(const std::string& session_id);
    std::vector<CollaborationSession> list_sessions(
        const std::string& status_filter = "",
        int limit = 50,
        int offset = 0
    );
    
    bool update_session_status(const std::string& session_id, const std::string& new_status);
    bool complete_session(const std::string& session_id);
    bool archive_session(const std::string& session_id);
    
    // Agent participation
    std::string add_agent_to_session(
        const std::string& session_id,
        const std::string& agent_id,
        const std::string& agent_name,
        const std::string& agent_type,
        const std::string& role = "participant"
    );
    
    bool remove_agent_from_session(const std::string& session_id, const std::string& agent_id);
    std::vector<CollaborationAgent> get_session_agents(const std::string& session_id);
    bool update_agent_activity(const std::string& session_id, const std::string& agent_id);
    
    // Reasoning stream
    std::string stream_reasoning_step(
        const std::string& session_id,
        const std::string& agent_id,
        const std::string& agent_name,
        const std::string& agent_type,
        const std::string& reasoning_step,
        int step_number,
        const std::string& step_type,
        double confidence_score,
        int duration_ms = 0,
        const nlohmann::json& metadata = nlohmann::json::object(),
        const std::string& parent_step_id = ""
    );
    
    std::vector<ReasoningStep> get_reasoning_stream(
        const std::string& session_id,
        int limit = 100,
        int offset = 0
    );
    
    std::vector<ReasoningStep> get_agent_reasoning_stream(
        const std::string& session_id,
        const std::string& agent_id,
        int limit = 100
    );
    
    // Confidence metrics
    std::string record_confidence_metric(
        const std::string& session_id,
        const std::string& decision_id,
        const std::string& stream_id,
        const std::string& metric_type,
        const std::string& metric_name,
        double metric_value,
        double weight,
        const std::vector<std::string>& contributing_factors
    );
    
    std::vector<ConfidenceMetric> get_confidence_breakdown(
        const std::string& session_id,
        const std::string& decision_id = ""
    );
    
    double calculate_aggregate_confidence(const std::string& session_id);
    
    // Human override
    std::string record_human_override(
        const std::string& session_id,
        const std::string& decision_id,
        const std::string& user_id,
        const std::string& user_name,
        const std::string& original_decision,
        const std::string& override_decision,
        const std::string& reason,
        const std::string& justification = "",
        const nlohmann::json& impact_assessment = nlohmann::json::object()
    );
    
    std::vector<HumanOverride> get_session_overrides(
        const std::string& session_id,
        int limit = 50
    );
    
    HumanOverride get_override_by_id(const std::string& override_id);
    
    // Real-time streaming
    std::string subscribe_to_session(
        const std::string& session_id,
        const std::string& subscriber_id,
        StreamSubscriber::MessageCallback callback
    );
    
    bool unsubscribe_from_session(const std::string& session_id, const std::string& subscriber_id);
    
    void broadcast_to_session(const std::string& session_id, const nlohmann::json& message);
    
    // Activity logging
    void log_activity(
        const std::string& session_id,
        const std::string& activity_type,
        const std::string& actor_id,
        const std::string& actor_type,
        const std::string& description,
        const nlohmann::json& details = nlohmann::json::object()
    );
    
    std::vector<nlohmann::json> get_activity_log(
        const std::string& session_id,
        int limit = 100,
        int offset = 0
    );
    
    // Statistics and monitoring
    nlohmann::json get_session_summary(const std::string& session_id);
    nlohmann::json get_dashboard_stats();
    void refresh_session_summaries();
    
    // Health and metrics
    bool is_healthy() const { return healthy_.load(); }
    nlohmann::json get_metrics() const;
    
private:
    std::shared_ptr<ConnectionPool> db_pool_;
    StructuredLogger* logger_;
    std::atomic<bool> healthy_{true};
    
    // Subscriber management
    std::unordered_map<std::string, std::vector<std::shared_ptr<StreamSubscriber>>> session_subscribers_;
    mutable std::mutex subscribers_mutex_;
    
    // Session cache for performance
    std::unordered_map<std::string, CollaborationSession> session_cache_;
    mutable std::mutex cache_mutex_;
    
    // Metrics
    std::atomic<uint64_t> total_sessions_created_{0};
    std::atomic<uint64_t> total_reasoning_steps_{0};
    std::atomic<uint64_t> total_overrides_{0};
    std::atomic<uint64_t> total_broadcasts_{0};
    
    // Helper methods
    std::string generate_uuid();
    void update_session_cache(const CollaborationSession& session);
    void remove_from_cache(const std::string& session_id);
    PGconn* get_db_connection();
    void release_db_connection(PGconn* conn);
    std::string escape_string(PGconn* conn, const std::string& str);
    
    // Database operations
    bool persist_session(const CollaborationSession& session);
    bool persist_reasoning_step(const ReasoningStep& step);
    bool persist_confidence_metric(const ConfidenceMetric& metric);
    bool persist_human_override(const HumanOverride& override_data);
    bool persist_collaboration_agent(const CollaborationAgent& agent);
};

} // namespace collaboration
} // namespace regulens

