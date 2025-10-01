#pragma once

#include <string>
#include <unordered_map>
#include <variant>
#include <chrono>
#include "nlohmann/json.hpp"

namespace regulens {

/**
 * @brief Types of compliance events that can trigger agent actions
 */
enum class EventType {
    // Transaction events
    TRANSACTION_INITIATED,
    TRANSACTION_COMPLETED,
    TRANSACTION_FAILED,
    SUSPICIOUS_ACTIVITY_DETECTED,

    // Regulatory events
    REGULATORY_CHANGE_DETECTED,
    COMPLIANCE_DEADLINE_APPROACHING,
    REGULATORY_REPORT_DUE,

    // Audit events
    AUDIT_LOG_ENTRY,
    SYSTEM_ACCESS_ATTEMPT,
    CONFIGURATION_CHANGE,

    // External events
    REGULATORY_API_UPDATE,
    MARKET_DATA_UPDATE,
    CUSTOMER_COMPLAINT,

    // System events
    AGENT_HEALTH_CHECK,
    SYSTEM_PERFORMANCE_ALERT,
    DATA_QUALITY_ISSUE
};

/**
 * @brief Event severity levels
 */
enum class EventSeverity {
    LOW,
    MEDIUM,
    HIGH,
    CRITICAL
};

/**
 * @brief Event source information
 */
struct EventSource {
    std::string source_type;    // "transaction_system", "regulatory_api", "audit_log", etc.
    std::string source_id;      // Unique identifier for the source
    std::string location;       // Geographic or system location

    nlohmann::json to_json() const {
        return {
            {"source_type", source_type},
            {"source_id", source_id},
            {"location", location}
        };
    }
};

/**
 * @brief Event metadata container
 */
using EventMetadata = std::unordered_map<std::string, std::variant<std::string, int, double, bool>>;

/**
 * @brief Core compliance event structure
 *
 * Represents any event that may trigger agentic AI analysis or action
 * in the compliance monitoring system.
 */
class ComplianceEvent {
public:
    ComplianceEvent(EventType type, EventSeverity severity, std::string description,
                   EventSource source, EventMetadata metadata = {})
        : event_type_(type), severity_(severity), description_(std::move(description)),
          source_(std::move(source)), metadata_(std::move(metadata)),
          timestamp_(std::chrono::system_clock::now()),
          event_id_(generate_event_id()) {}

    // Getters
    EventType get_type() const { return event_type_; }
    EventSeverity get_severity() const { return severity_; }
    const std::string& get_description() const { return description_; }
    const EventSource& get_source() const { return source_; }
    const EventMetadata& get_metadata() const { return metadata_; }
    std::chrono::system_clock::time_point get_timestamp() const { return timestamp_; }
    const std::string& get_event_id() const { return event_id_; }

    // Metadata access helpers
    template<typename T>
    std::optional<T> get_metadata_value(const std::string& key) const {
        auto it = metadata_.find(key);
        if (it != metadata_.end()) {
            try {
                return std::get<T>(it->second);
            } catch (const std::bad_variant_access&) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    // Metadata setters
    template<typename T>
    void set_metadata_value(const std::string& key, T value) {
        metadata_[key] = value;
    }

    // JSON serialization
    nlohmann::json to_json() const {
        nlohmann::json metadata_json;
        for (const auto& [key, value] : metadata_) {
            std::visit([&](const auto& v) {
                metadata_json[key] = v;
            }, value);
        }

        return {
            {"event_id", event_id_},
            {"event_type", static_cast<int>(event_type_)},
            {"severity", static_cast<int>(severity_)},
            {"description", description_},
            {"source", source_.to_json()},
            {"metadata", metadata_json},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp_.time_since_epoch()).count()}
        };
    }

    // JSON deserialization
    static std::optional<ComplianceEvent> from_json(const nlohmann::json& json) {
        try {
            EventMetadata metadata;
            if (json.contains("metadata")) {
                for (const auto& [key, value] : json["metadata"].items()) {
                    if (value.is_string()) {
                        metadata[key] = value.get<std::string>();
                    } else if (value.is_number_integer()) {
                        metadata[key] = value.get<int>();
                    } else if (value.is_number_float()) {
                        metadata[key] = value.get<double>();
                    } else if (value.is_boolean()) {
                        metadata[key] = value.get<bool>();
                    }
                }
            }

            EventSource source = {
                json["source"]["source_type"].get<std::string>(),
                json["source"]["source_id"].get<std::string>(),
                json["source"]["location"].get<std::string>()
            };

            auto timestamp_ms = json["timestamp"].get<int64_t>();
            auto timestamp = std::chrono::system_clock::time_point(
                std::chrono::milliseconds(timestamp_ms));

            ComplianceEvent event(
                static_cast<EventType>(json["event_type"].get<int>()),
                static_cast<EventSeverity>(json["severity"].get<int>()),
                json["description"].get<std::string>(),
                source,
                metadata
            );

            // Restore original timestamp and ID
            event.timestamp_ = timestamp;
            event.event_id_ = json["event_id"].get<std::string>();

            return event;
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }

private:
    static std::string generate_event_id() {
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count();
        return "evt_" + std::to_string(timestamp) + "_" + std::to_string(++counter);
    }

    EventType event_type_;
    EventSeverity severity_;
    std::string description_;
    EventSource source_;
    EventMetadata metadata_;
    std::chrono::system_clock::time_point timestamp_;
    std::string event_id_;
};

// Event type to string conversion for logging
inline std::string event_type_to_string(EventType type) {
    switch (type) {
        case EventType::TRANSACTION_INITIATED: return "TRANSACTION_INITIATED";
        case EventType::TRANSACTION_COMPLETED: return "TRANSACTION_COMPLETED";
        case EventType::TRANSACTION_FAILED: return "TRANSACTION_FAILED";
        case EventType::SUSPICIOUS_ACTIVITY_DETECTED: return "SUSPICIOUS_ACTIVITY_DETECTED";
        case EventType::REGULATORY_CHANGE_DETECTED: return "REGULATORY_CHANGE_DETECTED";
        case EventType::COMPLIANCE_DEADLINE_APPROACHING: return "COMPLIANCE_DEADLINE_APPROACHING";
        case EventType::REGULATORY_REPORT_DUE: return "REGULATORY_REPORT_DUE";
        case EventType::AUDIT_LOG_ENTRY: return "AUDIT_LOG_ENTRY";
        case EventType::SYSTEM_ACCESS_ATTEMPT: return "SYSTEM_ACCESS_ATTEMPT";
        case EventType::CONFIGURATION_CHANGE: return "CONFIGURATION_CHANGE";
        case EventType::REGULATORY_API_UPDATE: return "REGULATORY_API_UPDATE";
        case EventType::MARKET_DATA_UPDATE: return "MARKET_DATA_UPDATE";
        case EventType::CUSTOMER_COMPLAINT: return "CUSTOMER_COMPLAINT";
        case EventType::AGENT_HEALTH_CHECK: return "AGENT_HEALTH_CHECK";
        case EventType::SYSTEM_PERFORMANCE_ALERT: return "SYSTEM_PERFORMANCE_ALERT";
        case EventType::DATA_QUALITY_ISSUE: return "DATA_QUALITY_ISSUE";
        default: return "UNKNOWN";
    }
}

} // namespace regulens
