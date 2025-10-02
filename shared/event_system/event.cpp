#include "event.hpp"
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace regulens {

Event::Event(
    std::string event_id,
    EventCategory category,
    std::string source,
    std::string event_type,
    nlohmann::json payload,
    EventPriority priority
) : event_id_(std::move(event_id)),
    category_(category),
    source_(std::move(source)),
    event_type_(std::move(event_type)),
    payload_(std::move(payload)),
    priority_(priority),
    created_at_(std::chrono::system_clock::now()),
    expires_at_(std::chrono::system_clock::time_point::max()),
    state_(EventState::CREATED),
    retry_count_(0) {
}

Event::Event(Event&& other) noexcept
    : event_id_(std::move(other.event_id_)),
      category_(other.category_),
      source_(std::move(other.source_)),
      event_type_(std::move(other.event_type_)),
      payload_(std::move(other.payload_)),
      priority_(other.priority_),
      created_at_(other.created_at_),
      expires_at_(other.expires_at_),
      state_(other.state_.load()),
      retry_count_(other.retry_count_.load()),
      headers_(std::move(other.headers_)),
      correlation_id_(std::move(other.correlation_id_)),
      trace_id_(std::move(other.trace_id_)) {
}

Event& Event::operator=(Event&& other) noexcept {
    if (this != &other) {
        event_id_ = std::move(other.event_id_);
        category_ = other.category_;
        source_ = std::move(other.source_);
        event_type_ = std::move(other.event_type_);
        payload_ = std::move(other.payload_);
        priority_ = other.priority_;
        created_at_ = other.created_at_;
        expires_at_ = other.expires_at_;
        state_.store(other.state_.load());
        retry_count_.store(other.retry_count_.load());
        headers_ = std::move(other.headers_);
        correlation_id_ = std::move(other.correlation_id_);
        trace_id_ = std::move(other.trace_id_);
    }
    return *this;
}

void Event::add_header(const std::string& key, const std::string& value) {
    headers_[key] = value;
}

std::string Event::get_header(const std::string& key) const {
    auto it = headers_.find(key);
    return it != headers_.end() ? it->second : "";
}

bool Event::is_expired() const {
    return std::chrono::system_clock::now() > expires_at_;
}

void Event::set_expiry(std::chrono::seconds ttl) {
    expires_at_ = created_at_ + ttl;
}

nlohmann::json Event::to_json() const {
    nlohmann::json json_event = {
        {"event_id", event_id_},
        {"category", event_category_to_string(category_)},
        {"source", source_},
        {"event_type", event_type_},
        {"payload", payload_},
        {"priority", event_priority_to_string(priority_)},
        {"state", event_state_to_string(get_state())},
        {"retry_count", get_retry_count()},
        {"created_at", std::chrono::duration_cast<std::chrono::milliseconds>(
            created_at_.time_since_epoch()).count()},
        {"expires_at", std::chrono::duration_cast<std::chrono::milliseconds>(
            expires_at_.time_since_epoch()).count()},
        {"headers", headers_},
        {"correlation_id", correlation_id_},
        {"trace_id", trace_id_}
    };

    return json_event;
}

std::unique_ptr<Event> Event::from_json(const nlohmann::json& json) {
    if (!json.contains("event_id") || !json.contains("category") ||
        !json.contains("source") || !json.contains("event_type")) {
        return nullptr;
    }

    auto event = std::make_unique<Event>(
        json["event_id"],
        string_to_event_category(json["category"]),
        json["source"],
        json["event_type"],
        json["payload"],
        json.contains("priority") ? string_to_event_priority(json["priority"]) : EventPriority::NORMAL
    );

    if (json.contains("state")) {
        event->set_state(string_to_event_state(json["state"]));
    }

    if (json.contains("retry_count")) {
        int retry_count = json["retry_count"];
        for (int i = 0; i < retry_count; ++i) {
            event->increment_retry_count();
        }
    }

    if (json.contains("headers")) {
        for (const auto& [key, value] : json["headers"].items()) {
            event->add_header(key, value);
        }
    }

    if (json.contains("correlation_id")) {
        event->set_correlation_id(json["correlation_id"]);
    }

    if (json.contains("trace_id")) {
        event->set_trace_id(json["trace_id"]);
    }

    if (json.contains("created_at")) {
        auto created_ms = std::chrono::milliseconds(json["created_at"]);
        event->created_at_ = std::chrono::system_clock::time_point(created_ms);
    }

    if (json.contains("expires_at")) {
        auto expires_ms = std::chrono::milliseconds(json["expires_at"]);
        event->expires_at_ = std::chrono::system_clock::time_point(expires_ms);
    }

    return event;
}

std::string Event::to_string() const {
    std::stringstream ss;
    ss << "[" << event_category_to_string(category_) << "] "
       << source_ << " -> " << event_type_
       << " (priority: " << event_priority_to_string(priority_)
       << ", state: " << event_state_to_string(get_state()) << ")";
    return ss.str();
}

// EventFactory Implementation

std::unique_ptr<Event> EventFactory::create_agent_decision_event(
    const std::string& agent_id,
    const std::string& decision_id,
    const nlohmann::json& decision_data
) {
    nlohmann::json payload = {
        {"agent_id", agent_id},
        {"decision_id", decision_id},
        {"decision_data", decision_data}
    };

    return create_event(
        EventCategory::AGENT_DECISION,
        agent_id,
        "AGENT_DECISION_MADE",
        payload,
        EventPriority::HIGH
    );
}

std::unique_ptr<Event> EventFactory::create_agent_status_event(
    const std::string& agent_id,
    const std::string& status,
    const nlohmann::json& status_data
) {
    nlohmann::json payload = {
        {"agent_id", agent_id},
        {"status", status},
        {"status_data", status_data}
    };

    EventPriority priority = (status == "ERROR" || status == "CRITICAL") ?
        EventPriority::HIGH : EventPriority::NORMAL;

    return create_event(
        EventCategory::AGENT_STATUS_UPDATE,
        agent_id,
        "AGENT_STATUS_CHANGE",
        payload,
        priority
    );
}

std::unique_ptr<Event> EventFactory::create_regulatory_change_event(
    const std::string& source,
    const std::string& change_id,
    const nlohmann::json& change_data
) {
    nlohmann::json payload = {
        {"source", source},
        {"change_id", change_id},
        {"change_data", change_data}
    };

    return create_event(
        EventCategory::REGULATORY_CHANGE_DETECTED,
        source,
        "REGULATORY_CHANGE_DETECTED",
        payload,
        EventPriority::CRITICAL
    );
}

std::unique_ptr<Event> EventFactory::create_compliance_violation_event(
    const std::string& violation_type,
    const std::string& severity,
    const nlohmann::json& violation_data
) {
    nlohmann::json payload = {
        {"violation_type", violation_type},
        {"severity", severity},
        {"violation_data", violation_data}
    };

    EventPriority priority = (severity == "CRITICAL") ? EventPriority::URGENT :
                             (severity == "HIGH") ? EventPriority::CRITICAL :
                             (severity == "MEDIUM") ? EventPriority::HIGH : EventPriority::NORMAL;

    return create_event(
        EventCategory::REGULATORY_COMPLIANCE_VIOLATION,
        "COMPLIANCE_SYSTEM",
        "COMPLIANCE_VIOLATION_DETECTED",
        payload,
        priority
    );
}

std::unique_ptr<Event> EventFactory::create_transaction_event(
    const std::string& transaction_id,
    const std::string& event_type,
    const nlohmann::json& transaction_data
) {
    nlohmann::json payload = {
        {"transaction_id", transaction_id},
        {"event_type", event_type},
        {"transaction_data", transaction_data}
    };

    EventPriority priority = (event_type == "FLAGGED" || event_type == "REVIEW_REQUESTED") ?
        EventPriority::HIGH : EventPriority::NORMAL;

    return create_event(
        EventCategory::TRANSACTION_PROCESSED,
        "TRANSACTION_PROCESSOR",
        "TRANSACTION_EVENT",
        payload,
        priority
    );
}

std::unique_ptr<Event> EventFactory::create_human_review_event(
    const std::string& decision_id,
    const std::string& review_reason,
    const nlohmann::json& context_data
) {
    nlohmann::json payload = {
        {"decision_id", decision_id},
        {"review_reason", review_reason},
        {"context_data", context_data}
    };

    return create_event(
        EventCategory::HUMAN_REVIEW_REQUESTED,
        "DECISION_SYSTEM",
        "HUMAN_REVIEW_REQUESTED",
        payload,
        EventPriority::HIGH
    );
}

std::unique_ptr<Event> EventFactory::create_system_health_event(
    const std::string& component,
    const std::string& status,
    const nlohmann::json& health_data
) {
    nlohmann::json payload = {
        {"component", component},
        {"status", status},
        {"health_data", health_data},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };

    EventPriority priority = (status == "CRITICAL" || status == "DOWN") ?
        EventPriority::CRITICAL : EventPriority::LOW;

    return create_event(
        EventCategory::SYSTEM_HEALTH_CHECK,
        component,
        "SYSTEM_HEALTH_UPDATE",
        payload,
        priority
    );
}

std::unique_ptr<Event> EventFactory::create_performance_metric_event(
    const std::string& metric_name,
    double value,
    const nlohmann::json& metadata
) {
    nlohmann::json payload = {
        {"metric_name", metric_name},
        {"value", value},
        {"metadata", metadata},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };

    return create_event(
        EventCategory::SYSTEM_PERFORMANCE_METRIC,
        "METRICS_COLLECTOR",
        "PERFORMANCE_METRIC",
        payload,
        EventPriority::LOW
    );
}

std::unique_ptr<Event> EventFactory::create_event(
    EventCategory category,
    const std::string& source,
    const std::string& event_type,
    const nlohmann::json& payload,
    EventPriority priority
) {
    return std::make_unique<Event>(
        generate_event_id(),
        category,
        source,
        event_type,
        payload,
        priority
    );
}

std::string EventFactory::generate_event_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    std::stringstream ss;
    ss << std::hex;

    for (int i = 0; i < 8; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; i++) ss << dis(gen);
    ss << "-4";  // Version 4 UUID
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    ss << dis2(gen);  // Variant
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 12; i++) ss << dis(gen);

    return "evt-" + ss.str();
}

// Utility Functions

std::string event_category_to_string(EventCategory category) {
    switch (category) {
        case EventCategory::AGENT_DECISION: return "AGENT_DECISION";
        case EventCategory::AGENT_STATUS_UPDATE: return "AGENT_STATUS_UPDATE";
        case EventCategory::AGENT_ERROR: return "AGENT_ERROR";
        case EventCategory::AGENT_LEARNING_UPDATE: return "AGENT_LEARNING_UPDATE";
        case EventCategory::REGULATORY_CHANGE_DETECTED: return "REGULATORY_CHANGE_DETECTED";
        case EventCategory::REGULATORY_COMPLIANCE_VIOLATION: return "REGULATORY_COMPLIANCE_VIOLATION";
        case EventCategory::REGULATORY_RISK_ALERT: return "REGULATORY_RISK_ALERT";
        case EventCategory::TRANSACTION_PROCESSED: return "TRANSACTION_PROCESSED";
        case EventCategory::TRANSACTION_FLAGGED: return "TRANSACTION_FLAGGED";
        case EventCategory::TRANSACTION_REVIEW_REQUESTED: return "TRANSACTION_REVIEW_REQUESTED";
        case EventCategory::SYSTEM_HEALTH_CHECK: return "SYSTEM_HEALTH_CHECK";
        case EventCategory::SYSTEM_PERFORMANCE_METRIC: return "SYSTEM_PERFORMANCE_METRIC";
        case EventCategory::SYSTEM_ERROR: return "SYSTEM_ERROR";
        case EventCategory::HUMAN_REVIEW_REQUESTED: return "HUMAN_REVIEW_REQUESTED";
        case EventCategory::HUMAN_FEEDBACK_RECEIVED: return "HUMAN_FEEDBACK_RECEIVED";
        case EventCategory::HUMAN_DECISION_OVERRIDE: return "HUMAN_DECISION_OVERRIDE";
        case EventCategory::DATA_INGESTION_COMPLETED: return "DATA_INGESTION_COMPLETED";
        case EventCategory::DATA_PROCESSING_STARTED: return "DATA_PROCESSING_STARTED";
        case EventCategory::DATA_QUALITY_ISSUE: return "DATA_QUALITY_ISSUE";
        case EventCategory::AUDIT_TRAIL_UPDATED: return "AUDIT_TRAIL_UPDATED";
        case EventCategory::COMPLIANCE_REPORT_GENERATED: return "COMPLIANCE_REPORT_GENERATED";
        case EventCategory::SECURITY_INCIDENT_DETECTED: return "SECURITY_INCIDENT_DETECTED";
        default: return "UNKNOWN";
    }
}

std::string event_priority_to_string(EventPriority priority) {
    switch (priority) {
        case EventPriority::LOW: return "LOW";
        case EventPriority::NORMAL: return "NORMAL";
        case EventPriority::HIGH: return "HIGH";
        case EventPriority::CRITICAL: return "CRITICAL";
        case EventPriority::URGENT: return "URGENT";
        default: return "NORMAL";
    }
}

std::string event_state_to_string(EventState state) {
    switch (state) {
        case EventState::CREATED: return "CREATED";
        case EventState::PUBLISHED: return "PUBLISHED";
        case EventState::ROUTED: return "ROUTED";
        case EventState::PROCESSED: return "PROCESSED";
        case EventState::FAILED: return "FAILED";
        case EventState::EXPIRED: return "EXPIRED";
        case EventState::ARCHIVED: return "ARCHIVED";
        default: return "CREATED";
    }
}

EventCategory string_to_event_category(const std::string& str) {
    static const std::unordered_map<std::string, EventCategory> category_map = {
        {"AGENT_DECISION", EventCategory::AGENT_DECISION},
        {"AGENT_STATUS_UPDATE", EventCategory::AGENT_STATUS_UPDATE},
        {"AGENT_ERROR", EventCategory::AGENT_ERROR},
        {"AGENT_LEARNING_UPDATE", EventCategory::AGENT_LEARNING_UPDATE},
        {"REGULATORY_CHANGE_DETECTED", EventCategory::REGULATORY_CHANGE_DETECTED},
        {"REGULATORY_COMPLIANCE_VIOLATION", EventCategory::REGULATORY_COMPLIANCE_VIOLATION},
        {"REGULATORY_RISK_ALERT", EventCategory::REGULATORY_RISK_ALERT},
        {"TRANSACTION_PROCESSED", EventCategory::TRANSACTION_PROCESSED},
        {"TRANSACTION_FLAGGED", EventCategory::TRANSACTION_FLAGGED},
        {"TRANSACTION_REVIEW_REQUESTED", EventCategory::TRANSACTION_REVIEW_REQUESTED},
        {"SYSTEM_HEALTH_CHECK", EventCategory::SYSTEM_HEALTH_CHECK},
        {"SYSTEM_PERFORMANCE_METRIC", EventCategory::SYSTEM_PERFORMANCE_METRIC},
        {"SYSTEM_ERROR", EventCategory::SYSTEM_ERROR},
        {"HUMAN_REVIEW_REQUESTED", EventCategory::HUMAN_REVIEW_REQUESTED},
        {"HUMAN_FEEDBACK_RECEIVED", EventCategory::HUMAN_FEEDBACK_RECEIVED},
        {"HUMAN_DECISION_OVERRIDE", EventCategory::HUMAN_DECISION_OVERRIDE},
        {"DATA_INGESTION_COMPLETED", EventCategory::DATA_INGESTION_COMPLETED},
        {"DATA_PROCESSING_STARTED", EventCategory::DATA_PROCESSING_STARTED},
        {"DATA_QUALITY_ISSUE", EventCategory::DATA_QUALITY_ISSUE},
        {"AUDIT_TRAIL_UPDATED", EventCategory::AUDIT_TRAIL_UPDATED},
        {"COMPLIANCE_REPORT_GENERATED", EventCategory::COMPLIANCE_REPORT_GENERATED},
        {"SECURITY_INCIDENT_DETECTED", EventCategory::SECURITY_INCIDENT_DETECTED}
    };

    auto it = category_map.find(str);
    return it != category_map.end() ? it->second : EventCategory::SYSTEM_ERROR;
}

EventPriority string_to_event_priority(const std::string& str) {
    static const std::unordered_map<std::string, EventPriority> priority_map = {
        {"LOW", EventPriority::LOW},
        {"NORMAL", EventPriority::NORMAL},
        {"HIGH", EventPriority::HIGH},
        {"CRITICAL", EventPriority::CRITICAL},
        {"URGENT", EventPriority::URGENT}
    };

    auto it = priority_map.find(str);
    return it != priority_map.end() ? it->second : EventPriority::NORMAL;
}

EventState string_to_event_state(const std::string& str) {
    static const std::unordered_map<std::string, EventState> state_map = {
        {"CREATED", EventState::CREATED},
        {"PUBLISHED", EventState::PUBLISHED},
        {"ROUTED", EventState::ROUTED},
        {"PROCESSED", EventState::PROCESSED},
        {"FAILED", EventState::FAILED},
        {"EXPIRED", EventState::EXPIRED},
        {"ARCHIVED", EventState::ARCHIVED}
    };

    auto it = state_map.find(str);
    return it != state_map.end() ? it->second : EventState::CREATED;
}

} // namespace regulens
