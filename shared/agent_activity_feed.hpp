#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <nlohmann/json.hpp>

#include "models/agent_activity.hpp"
#include "logging/structured_logger.hpp"
#include "config/configuration_manager.hpp"
#include "database/postgresql_connection.hpp"

namespace regulens {

/**
 * @brief Real-time agent activity feed system
 *
 * Collects, stores, and streams agent activities in real-time.
 * Provides APIs for subscribing to activity feeds and retrieving
 * historical activity data with filtering and search capabilities.
 */
class AgentActivityFeed {
public:
    AgentActivityFeed(std::shared_ptr<ConfigurationManager> config,
                     std::shared_ptr<StructuredLogger> logger);

    ~AgentActivityFeed();

    /**
     * @brief Initialize the activity feed system
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Shutdown the activity feed system
     */
    void shutdown();

    /**
     * @brief Record a new agent activity event
     * @param event The activity event to record
     * @return true if recorded successfully
     */
    bool record_activity(const AgentActivityEvent& event);

    /**
     * @brief Subscribe to real-time activity feed
     * @param subscription Subscription details
     * @param callback Function to call when new activities match the filter
     * @return subscription ID if successful, empty string otherwise
     */
    std::string subscribe(const ActivityFeedSubscription& subscription,
                         std::function<void(const AgentActivityEvent&)> callback);

    /**
     * @brief Unsubscribe from activity feed
     * @param subscription_id ID of subscription to remove
     * @return true if unsubscribed successfully
     */
    bool unsubscribe(const std::string& subscription_id);

    /**
     * @brief Query historical activities with filtering
     * @param filter Filtering and search criteria
     * @return Vector of matching activity events
     */
    std::vector<AgentActivityEvent> query_activities(const ActivityFeedFilter& filter);

    /**
     * @brief Get activity statistics for an agent
     * @param agent_id Agent to get statistics for
     * @return Activity statistics
     */
    std::optional<AgentActivityStats> get_agent_stats(const std::string& agent_id);

    /**
     * @brief Get overall activity feed statistics
     * @return System-wide activity statistics
     */
    nlohmann::json get_feed_stats();

    /**
     * @brief Export activities for backup/analysis
     * @param filter Filter criteria for export
     * @param format Export format ("json", "csv", "xml")
     * @return Exported data as string
     */
    std::string export_activities(const ActivityFeedFilter& filter, const std::string& format = "json");

    /**
     * @brief Force cleanup of old activities
     * @return Number of activities cleaned up
     */
    size_t cleanup_old_activities();

    // Configuration access
    const ActivityFeedConfig& get_config() const { return config_; }

private:
    std::shared_ptr<ConfigurationManager> config_manager_;
    std::shared_ptr<StructuredLogger> logger_;

    ActivityFeedConfig config_;

    // Thread-safe storage
    std::mutex activities_mutex_;
    std::unordered_map<std::string, std::deque<AgentActivityEvent>> agent_activities_; // agent_id -> events
    std::unordered_map<std::string, AgentActivityStats> agent_stats_;

    // Subscription management
    std::mutex subscriptions_mutex_;
    std::unordered_map<std::string, ActivityFeedSubscription> subscriptions_;
    std::unordered_map<std::string, std::function<void(const AgentActivityEvent&)>> subscription_callbacks_;

    // Background processing
    std::thread cleanup_thread_;
    std::atomic<bool> running_;
    std::mutex cleanup_mutex_;
    std::condition_variable cleanup_cv_;

    // Database persistence (when enabled)
    std::unique_ptr<PostgreSQLConnection> db_connection_;

    // Helper methods
    void update_agent_stats(const AgentActivityEvent& event);
    void notify_subscribers(const AgentActivityEvent& event);
    bool matches_filter(const AgentActivityEvent& event, const ActivityFeedFilter& filter);
    void cleanup_worker();

    // Persistence methods (when enabled)
    bool persist_activity(const AgentActivityEvent& event);
    std::vector<AgentActivityEvent> load_activities_from_persistence(const ActivityFeedFilter& filter);

    // Statistics helpers
    void update_activity_counts();
    std::chrono::system_clock::time_point get_cutoff_time() const;
};

// Convenience functions for creating common activity events
namespace activity_events {

/**
 * @brief Create agent started event
 */
inline AgentActivityEvent agent_started(const std::string& agent_id, const std::string& agent_type) {
    return AgentActivityEvent(agent_id, AgentActivityType::AGENT_STARTED,
                            ActivitySeverity::INFO,
                            "Agent Started",
                            "Agent " + agent_id + " (" + agent_type + ") has started successfully");
}

/**
 * @brief Create agent stopped event
 */
inline AgentActivityEvent agent_stopped(const std::string& agent_id) {
    return AgentActivityEvent(agent_id, AgentActivityType::AGENT_STOPPED,
                            ActivitySeverity::INFO,
                            "Agent Stopped",
                            "Agent " + agent_id + " has stopped");
}

/**
 * @brief Create agent error event
 */
inline AgentActivityEvent agent_error(const std::string& agent_id, const std::string& error_msg) {
    AgentActivityEvent event(agent_id, AgentActivityType::AGENT_ERROR,
                           ActivitySeverity::ERROR,
                           "Agent Error",
                           error_msg);
    event.metadata["error_message"] = error_msg;
    return event;
}

/**
 * @brief Create decision made event
 */
inline AgentActivityEvent decision_made(const std::string& agent_id, const AgentDecision& decision) {
    AgentActivityEvent event(agent_id, AgentActivityType::DECISION_MADE,
                           ActivitySeverity::INFO,
                           "Decision Made",
                           "Agent made a decision: " + decision_type_to_string(decision.get_type()));
    event.decision = decision;
    return event;
}

/**
 * @brief Create task started event
 */
inline AgentActivityEvent task_started(const std::string& agent_id, const std::string& task_id,
                                     const std::string& event_id) {
    AgentActivityEvent event(agent_id, AgentActivityType::TASK_STARTED,
                           ActivitySeverity::INFO,
                           "Task Started",
                           "Agent started processing task " + task_id + " for event " + event_id);
    event.metadata["task_id"] = task_id;
    event.metadata["event_id"] = event_id;
    return event;
}

/**
 * @brief Create task completed event
 */
inline AgentActivityEvent task_completed(const std::string& agent_id, const std::string& task_id,
                                       std::chrono::milliseconds processing_time) {
    AgentActivityEvent event(agent_id, AgentActivityType::TASK_COMPLETED,
                           ActivitySeverity::INFO,
                           "Task Completed",
                           "Agent completed task " + task_id);
    event.metadata["task_id"] = task_id;
    event.metadata["processing_time_ms"] = std::to_string(processing_time.count());
    return event;
}

/**
 * @brief Create task failed event
 */
inline AgentActivityEvent task_failed(const std::string& agent_id, const std::string& task_id,
                                    const std::string& error_msg) {
    AgentActivityEvent event(agent_id, AgentActivityType::TASK_FAILED,
                           ActivitySeverity::ERROR,
                           "Task Failed",
                           "Agent failed to process task " + task_id + ": " + error_msg);
    event.metadata["task_id"] = task_id;
    event.metadata["error_message"] = error_msg;
    return event;
}

/**
 * @brief Create event received event
 */
inline AgentActivityEvent event_received(const std::string& agent_id, const std::string& event_id,
                                       const std::string& event_type) {
    AgentActivityEvent event(agent_id, AgentActivityType::EVENT_RECEIVED,
                           ActivitySeverity::INFO,
                           "Event Received",
                           "Agent received event " + event_id + " of type " + event_type);
    event.metadata["event_id"] = event_id;
    event.metadata["event_type"] = event_type;
    return event;
}

/**
 * @brief Create state changed event
 */
inline AgentActivityEvent state_changed(const std::string& agent_id, const std::string& old_state,
                                      const std::string& new_state) {
    AgentActivityEvent event(agent_id, AgentActivityType::STATE_CHANGED,
                           ActivitySeverity::INFO,
                           "State Changed",
                           "Agent state changed from " + old_state + " to " + new_state);
    event.metadata["old_state"] = old_state;
    event.metadata["new_state"] = new_state;
    return event;
}

} // namespace activity_events

} // namespace regulens
