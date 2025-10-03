#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <nlohmann/json.hpp>

#include "agent_decision.hpp"

namespace regulens {

/**
 * @brief Types of agent activities that can be tracked
 */
enum class AgentActivityType {
    AGENT_STARTED,           // Agent initialization completed
    AGENT_STOPPED,           // Agent shutdown
    AGENT_ERROR,             // Agent encountered an error
    AGENT_HEALTH_CHANGE,     // Agent health status changed
    DECISION_MADE,           // Agent made a decision
    TASK_STARTED,            // Agent started processing a task
    TASK_COMPLETED,          // Agent completed a task
    TASK_FAILED,             // Agent task failed
    EVENT_RECEIVED,          // Agent received an event
    EVENT_PROCESSED,         // Agent processed an event
    STATE_CHANGED,           // Agent state changed
    METRICS_UPDATED,         // Agent metrics were updated
    CONFIGURATION_CHANGED,   // Agent configuration changed
    LEARNING_OCCURRED        // Agent performed learning/update
};

/**
 * @brief Severity levels for activity events
 */
enum class ActivitySeverity {
    INFO,      // General information
    WARNING,   // Warning conditions
    ERROR,     // Error conditions
    CRITICAL   // Critical issues requiring attention
};

/**
 * @brief Agent activity event structure
 */
struct AgentActivityEvent {
    std::string event_id;
    std::string agent_id;
    AgentActivityType activity_type;
    ActivitySeverity severity;
    std::string title;
    std::string description;
    std::chrono::system_clock::time_point timestamp;

    // Activity-specific data
    std::unordered_map<std::string, std::string> metadata;
    std::optional<AgentDecision> decision;  // For decision-related activities
    std::optional<nlohmann::json> metrics;   // For metrics-related activities

    AgentActivityEvent() = default;

    AgentActivityEvent(std::string agent, AgentActivityType type,
                      ActivitySeverity sev, std::string ttl, std::string desc = "")
        : agent_id(std::move(agent)), activity_type(type), severity(sev),
          title(std::move(ttl)), description(std::move(desc)),
          timestamp(std::chrono::system_clock::now()) {
        event_id = generate_event_id(agent_id, activity_type, timestamp);
    }

    nlohmann::json to_json() const {
        nlohmann::json metadata_json;
        for (const auto& [key, value] : metadata) {
            metadata_json[key] = value;
        }

        nlohmann::json result = {
            {"event_id", event_id},
            {"agent_id", agent_id},
            {"activity_type", static_cast<int>(activity_type)},
            {"severity", static_cast<int>(severity)},
            {"title", title},
            {"description", description},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp.time_since_epoch()).count()},
            {"metadata", metadata_json}
        };

        if (decision) {
            result["decision"] = decision->to_json();
        }

        if (metrics) {
            result["metrics"] = *metrics;
        }

        return result;
    }

    // Helper for creating event IDs
    static std::string generate_event_id(const std::string& agent_id,
                                       AgentActivityType type,
                                       std::chrono::system_clock::time_point ts) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            ts.time_since_epoch()).count();
        return "activity_" + agent_id + "_" + std::to_string(static_cast<int>(type)) +
               "_" + std::to_string(ms);
    }
};

/**
 * @brief Activity feed filtering and search criteria
 */
struct ActivityFeedFilter {
    std::vector<std::string> agent_ids;           // Filter by specific agents
    std::vector<AgentActivityType> activity_types; // Filter by activity types
    std::vector<ActivitySeverity> severities;      // Filter by severity levels
    std::chrono::system_clock::time_point start_time; // Time range start
    std::chrono::system_clock::time_point end_time;   // Time range end
    std::unordered_map<std::string, std::string> metadata_filters; // Metadata key-value filters
    size_t max_results = 100;                      // Maximum number of results
    bool ascending_order = false;                  // Sort order (false = newest first)

    ActivityFeedFilter()
        : start_time(std::chrono::system_clock::now() - std::chrono::hours(24)),
          end_time(std::chrono::system_clock::now()) {}
};

/**
 * @brief Real-time activity feed subscription
 */
struct ActivityFeedSubscription {
    std::string subscription_id;
    std::string client_id;
    ActivityFeedFilter filter;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_activity;

    ActivityFeedSubscription() = default;

    ActivityFeedSubscription(std::string sub_id, std::string client,
                           ActivityFeedFilter flt = ActivityFeedFilter())
        : subscription_id(std::move(sub_id)), client_id(std::move(client)),
          filter(std::move(flt)),
          created_at(std::chrono::system_clock::now()),
          last_activity(created_at) {}
};

/**
 * @brief Agent activity statistics
 */
struct AgentActivityStats {
    std::string agent_id;
    size_t total_activities = 0;
    size_t activities_last_hour = 0;
    size_t activities_last_24h = 0;
    size_t error_count = 0;
    size_t warning_count = 0;
    std::chrono::system_clock::time_point last_activity;
    std::unordered_map<int, size_t> activity_type_counts; // ActivityType -> count

    nlohmann::json to_json() const {
        nlohmann::json type_counts_json;
        for (const auto& [type, count] : activity_type_counts) {
            type_counts_json[std::to_string(type)] = count;
        }

        return {
            {"agent_id", agent_id},
            {"total_activities", total_activities},
            {"activities_last_hour", activities_last_hour},
            {"activities_last_24h", activities_last_24h},
            {"error_count", error_count},
            {"warning_count", warning_count},
            {"last_activity", std::chrono::duration_cast<std::chrono::milliseconds>(
                last_activity.time_since_epoch()).count()},
            {"activity_type_counts", type_counts_json}
        };
    }
};

/**
 * @brief Real-time activity feed configuration
 */
struct ActivityFeedConfig {
    size_t max_events_buffer = 10000;      // Maximum events to keep in memory
    size_t max_events_per_agent = 1000;    // Maximum events per agent
    std::chrono::seconds cleanup_interval = std::chrono::seconds(300); // Cleanup interval
    std::chrono::hours retention_period = std::chrono::hours(24);      // How long to keep events
    bool enable_persistence = true;        // Whether to persist events to database
    size_t max_subscriptions = 100;        // Maximum concurrent subscriptions

    nlohmann::json to_json() const {
        return {
            {"max_events_buffer", max_events_buffer},
            {"max_events_per_agent", max_events_per_agent},
            {"cleanup_interval_seconds", cleanup_interval.count()},
            {"retention_period_hours", retention_period.count()},
            {"enable_persistence", enable_persistence},
            {"max_subscriptions", max_subscriptions}
        };
    }
};

} // namespace regulens
