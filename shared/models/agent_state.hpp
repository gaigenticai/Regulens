#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <atomic>
#include "nlohmann/json.hpp"

namespace regulens {

/**
 * @brief Agent operational states
 */
enum class AgentState {
    INITIALIZING,
    READY,
    ACTIVE,
    BUSY,
    ERROR,
    SHUTDOWN,
    MAINTENANCE
};

/**
 * @brief Agent health status
 */
enum class AgentHealth {
    HEALTHY,
    DEGRADED,
    UNHEALTHY,
    CRITICAL
};

/**
 * @brief Agent capabilities
 */
struct AgentCapabilities {
    std::vector<std::string> supported_event_types;  // Event types this agent can handle
    std::vector<std::string> supported_actions;      // Actions this agent can perform
    std::vector<std::string> knowledge_domains;      // Knowledge domains agent specializes in
    bool real_time_processing;                       // Can handle real-time events
    bool batch_processing;                          // Can handle batch processing
    int max_concurrent_tasks;                       // Maximum concurrent tasks

    nlohmann::json to_json() const {
        return {
            {"supported_event_types", supported_event_types},
            {"supported_actions", supported_actions},
            {"knowledge_domains", knowledge_domains},
            {"real_time_processing", real_time_processing},
            {"batch_processing", batch_processing},
            {"max_concurrent_tasks", max_concurrent_tasks}
        };
    }
};

/**
 * @brief Agent performance metrics
 */
struct AgentMetrics {
    std::atomic<size_t> tasks_processed{0};
    std::atomic<size_t> tasks_failed{0};
    std::atomic<size_t> tasks_in_progress{0};
    std::atomic<double> average_processing_time_ms{0.0};
    std::atomic<double> success_rate{1.0};
    std::chrono::system_clock::time_point last_task_time;
    std::chrono::system_clock::time_point startup_time;

    AgentMetrics() : last_task_time(std::chrono::system_clock::now()),
                    startup_time(std::chrono::system_clock::now()) {}

    nlohmann::json to_json() const {
        return {
            {"tasks_processed", tasks_processed.load()},
            {"tasks_failed", tasks_failed.load()},
            {"tasks_in_progress", tasks_in_progress.load()},
            {"average_processing_time_ms", average_processing_time_ms.load()},
            {"success_rate", success_rate.load()},
            {"last_task_time", std::chrono::duration_cast<std::chrono::milliseconds>(
                last_task_time.time_since_epoch()).count()},
            {"startup_time", std::chrono::duration_cast<std::chrono::milliseconds>(
                startup_time.time_since_epoch()).count()},
            {"uptime_seconds", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now() - startup_time).count()}
        };
    }
};

/**
 * @brief Agent status information
 */
struct AgentStatus {
    std::string agent_type;
    std::string agent_name;
    AgentState state;
    AgentHealth health;
    AgentCapabilities capabilities;
    AgentMetrics metrics;
    std::string last_error;
    std::chrono::system_clock::time_point last_health_check;
    bool enabled;

    AgentStatus(std::string type = "", std::string name = "")
        : agent_type(std::move(type)), agent_name(std::move(name)),
          state(AgentState::INITIALIZING), health(AgentHealth::HEALTHY),
          last_health_check(std::chrono::system_clock::now()), enabled(true) {}

    nlohmann::json to_json() const {
        return {
            {"agent_type", agent_type},
            {"agent_name", agent_name},
            {"state", static_cast<int>(state)},
            {"health", static_cast<int>(health)},
            {"capabilities", capabilities.to_json()},
            {"metrics", metrics.to_json()},
            {"last_error", last_error},
            {"last_health_check", std::chrono::duration_cast<std::chrono::milliseconds>(
                last_health_check.time_since_epoch()).count()},
            {"enabled", enabled}
        };
    }

    bool is_operational() const {
        return state == AgentState::READY || state == AgentState::ACTIVE || state == AgentState::BUSY;
    }

    bool needs_attention() const {
        return health == AgentHealth::CRITICAL ||
               health == AgentHealth::UNHEALTHY ||
               state == AgentState::ERROR;
    }
};

/**
 * @brief Agent configuration
 */
struct AgentConfiguration {
    std::string agent_type;
    std::string agent_name;
    std::unordered_map<std::string, std::string> parameters;
    bool enabled;
    Priority default_priority;
    std::chrono::milliseconds timeout_ms;

    AgentConfiguration(std::string type = "", std::string name = "")
        : agent_type(std::move(type)), agent_name(std::move(name)),
          enabled(true), default_priority(Priority::NORMAL),
          timeout_ms(std::chrono::seconds(30)) {}

    nlohmann::json to_json() const {
        nlohmann::json params_json;
        for (const auto& [key, value] : parameters) {
            params_json[key] = value;
        }

        return {
            {"agent_type", agent_type},
            {"agent_name", agent_name},
            {"parameters", params_json},
            {"enabled", enabled},
            {"default_priority", static_cast<int>(default_priority)},
            {"timeout_ms", timeout_ms.count()}
        };
    }

    static std::optional<AgentConfiguration> from_json(const nlohmann::json& json) {
        try {
            AgentConfiguration config(
                json["agent_type"].get<std::string>(),
                json["agent_name"].get<std::string>()
            );

            config.enabled = json["enabled"].get<bool>();
            config.default_priority = static_cast<Priority>(json["default_priority"].get<int>());
            config.timeout_ms = std::chrono::milliseconds(json["timeout_ms"].get<int>());

            if (json.contains("parameters")) {
                for (const auto& [key, value] : json["parameters"].items()) {
                    config.parameters[key] = value.get<std::string>();
                }
            }

            return config;
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }
};

// Helper functions
inline std::string agent_state_to_string(AgentState state) {
    switch (state) {
        case AgentState::INITIALIZING: return "INITIALIZING";
        case AgentState::READY: return "READY";
        case AgentState::ACTIVE: return "ACTIVE";
        case AgentState::BUSY: return "BUSY";
        case AgentState::ERROR: return "ERROR";
        case AgentState::SHUTDOWN: return "SHUTDOWN";
        case AgentState::MAINTENANCE: return "MAINTENANCE";
        default: return "UNKNOWN";
    }
}

inline std::string agent_health_to_string(AgentHealth health) {
    switch (health) {
        case AgentHealth::HEALTHY: return "HEALTHY";
        case AgentHealth::DEGRADED: return "DEGRADED";
        case AgentHealth::UNHEALTHY: return "UNHEALTHY";
        case AgentHealth::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

inline Priority string_to_priority(const std::string& str) {
    if (str == "LOW") return Priority::LOW;
    if (str == "NORMAL") return Priority::NORMAL;
    if (str == "HIGH") return Priority::HIGH;
    if (str == "CRITICAL") return Priority::CRITICAL;
    return Priority::NORMAL; // default
}

} // namespace regulens
