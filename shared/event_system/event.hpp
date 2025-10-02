/**
 * Event-Driven Architecture - Core Event System
 *
 * Enterprise-grade event-driven architecture for real-time processing,
 * enabling asynchronous communication between all system components.
 *
 * This is the nervous system of the agentic AI platform, enabling:
 * - Real-time agent communication
 * - Event streaming for regulatory monitoring
 * - Human-AI collaboration workflows
 * - Asynchronous processing pipelines
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <atomic>
#include <nlohmann/json.hpp>
#include "../logging/structured_logger.hpp"

namespace regulens {

// Event Priority Levels
enum class EventPriority {
    LOW,
    NORMAL,
    HIGH,
    CRITICAL,
    URGENT
};

// Event Categories for routing and filtering
enum class EventCategory {
    // Agent Events
    AGENT_DECISION,
    AGENT_STATUS_UPDATE,
    AGENT_ERROR,
    AGENT_LEARNING_UPDATE,

    // Regulatory Events
    REGULATORY_CHANGE_DETECTED,
    REGULATORY_COMPLIANCE_VIOLATION,
    REGULATORY_RISK_ALERT,

    // Transaction Events
    TRANSACTION_PROCESSED,
    TRANSACTION_FLAGGED,
    TRANSACTION_REVIEW_REQUESTED,

    // System Events
    SYSTEM_HEALTH_CHECK,
    SYSTEM_PERFORMANCE_METRIC,
    SYSTEM_ERROR,

    // Human-AI Collaboration Events
    HUMAN_REVIEW_REQUESTED,
    HUMAN_FEEDBACK_RECEIVED,
    HUMAN_DECISION_OVERRIDE,

    // Data Processing Events
    DATA_INGESTION_COMPLETED,
    DATA_PROCESSING_STARTED,
    DATA_QUALITY_ISSUE,

    // Audit & Compliance Events
    AUDIT_TRAIL_UPDATED,
    COMPLIANCE_REPORT_GENERATED,
    SECURITY_INCIDENT_DETECTED
};

// Event Lifecycle States
enum class EventState {
    CREATED,
    PUBLISHED,
    ROUTED,
    PROCESSED,
    FAILED,
    EXPIRED,
    ARCHIVED
};

class Event {
public:
    Event(
        std::string event_id,
        EventCategory category,
        std::string source,
        std::string event_type,
        nlohmann::json payload,
        EventPriority priority = EventPriority::NORMAL
    );

    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;
    Event(Event&& other) noexcept;
    Event& operator=(Event&& other) noexcept;

    ~Event() = default;

    // Event identification
    const std::string& get_event_id() const { return event_id_; }
    const std::string& get_source() const { return source_; }
    const std::string& get_event_type() const { return event_type_; }
    EventCategory get_category() const { return category_; }
    EventPriority get_priority() const { return priority_; }

    // Event data
    const nlohmann::json& get_payload() const { return payload_; }
    nlohmann::json& get_payload() { return payload_; }

    // Event metadata
    const std::chrono::system_clock::time_point& get_created_at() const { return created_at_; }
    const std::chrono::system_clock::time_point& get_expires_at() const { return expires_at_; }

    // Event lifecycle
    EventState get_state() const { return state_.load(); }
    void set_state(EventState state) { state_ = state; }

    // Processing tracking
    int get_retry_count() const { return retry_count_.load(); }
    void increment_retry_count() { retry_count_++; }

    // Event headers/metadata
    void add_header(const std::string& key, const std::string& value);
    std::string get_header(const std::string& key) const;
    const std::unordered_map<std::string, std::string>& get_headers() const { return headers_; }

    // Correlation and tracing
    void set_correlation_id(const std::string& correlation_id) { correlation_id_ = correlation_id; }
    const std::string& get_correlation_id() const { return correlation_id_; }

    void set_trace_id(const std::string& trace_id) { trace_id_ = trace_id; }
    const std::string& get_trace_id() const { return trace_id_; }

    // Serialization
    nlohmann::json to_json() const;
    static std::unique_ptr<Event> from_json(const nlohmann::json& json);

    // Utility methods
    bool is_expired() const;
    void set_expiry(std::chrono::seconds ttl);
    std::string to_string() const;

private:
    std::string event_id_;
    EventCategory category_;
    std::string source_;
    std::string event_type_;
    nlohmann::json payload_;
    EventPriority priority_;

    std::chrono::system_clock::time_point created_at_;
    std::chrono::system_clock::time_point expires_at_;

    std::atomic<EventState> state_;
    std::atomic<int> retry_count_;

    std::unordered_map<std::string, std::string> headers_;
    std::string correlation_id_;
    std::string trace_id_;
};

// Event Factory for creating standardized events
class EventFactory {
public:
    // Agent Events
    static std::unique_ptr<Event> create_agent_decision_event(
        const std::string& agent_id,
        const std::string& decision_id,
        const nlohmann::json& decision_data
    );

    static std::unique_ptr<Event> create_agent_status_event(
        const std::string& agent_id,
        const std::string& status,
        const nlohmann::json& status_data = nlohmann::json::object()
    );

    // Regulatory Events
    static std::unique_ptr<Event> create_regulatory_change_event(
        const std::string& source,
        const std::string& change_id,
        const nlohmann::json& change_data
    );

    static std::unique_ptr<Event> create_compliance_violation_event(
        const std::string& violation_type,
        const std::string& severity,
        const nlohmann::json& violation_data
    );

    // Transaction Events
    static std::unique_ptr<Event> create_transaction_event(
        const std::string& transaction_id,
        const std::string& event_type,
        const nlohmann::json& transaction_data
    );

    // Human-AI Collaboration Events
    static std::unique_ptr<Event> create_human_review_event(
        const std::string& decision_id,
        const std::string& review_reason,
        const nlohmann::json& context_data = nlohmann::json::object()
    );

    // System Events
    static std::unique_ptr<Event> create_system_health_event(
        const std::string& component,
        const std::string& status,
        const nlohmann::json& health_data
    );

    static std::unique_ptr<Event> create_performance_metric_event(
        const std::string& metric_name,
        double value,
        const nlohmann::json& metadata = nlohmann::json::object()
    );

private:
    static std::unique_ptr<Event> create_event(
        EventCategory category,
        const std::string& source,
        const std::string& event_type,
        const nlohmann::json& payload,
        EventPriority priority = EventPriority::NORMAL
    );

    static std::string generate_event_id();
};

// Utility functions
std::string event_category_to_string(EventCategory category);
std::string event_priority_to_string(EventPriority priority);
std::string event_state_to_string(EventState state);

EventCategory string_to_event_category(const std::string& str);
EventPriority string_to_event_priority(const std::string& str);
EventState string_to_event_state(const std::string& str);

} // namespace regulens
