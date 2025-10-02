#include "agent_activity_feed.hpp"

#include <algorithm>
#include <sstream>
#include <iomanip>

namespace regulens {

AgentActivityFeed::AgentActivityFeed(std::shared_ptr<ConfigurationManager> config,
                                   std::shared_ptr<StructuredLogger> logger)
    : config_manager_(config), logger_(logger), running_(false) {
    // Load configuration from environment
    config_.max_events_buffer = static_cast<size_t>(config_manager_->get_int("ACTIVITY_FEED_MAX_BUFFER").value_or(10000));
    config_.max_events_per_agent = static_cast<size_t>(config_manager_->get_int("ACTIVITY_FEED_MAX_PER_AGENT").value_or(1000));
    config_.cleanup_interval = std::chrono::seconds(
        config_manager_->get_int("ACTIVITY_FEED_CLEANUP_INTERVAL_SEC").value_or(300));
    config_.retention_period = std::chrono::hours(
        config_manager_->get_int("ACTIVITY_FEED_RETENTION_HOURS").value_or(24));
    config_.enable_persistence = config_manager_->get_bool("ACTIVITY_FEED_ENABLE_PERSISTENCE").value_or(true);
    config_.max_subscriptions = static_cast<size_t>(config_manager_->get_int("ACTIVITY_FEED_MAX_SUBSCRIPTIONS").value_or(100));

    logger_->info("AgentActivityFeed initialized with buffer size: " +
                 std::to_string(config_.max_events_buffer) + ", retention: " +
                 std::to_string(config_.retention_period.count()) + " hours");
}

AgentActivityFeed::~AgentActivityFeed() {
    shutdown();
}

bool AgentActivityFeed::initialize() {
    logger_->info("Initializing AgentActivityFeed");

    running_ = true;

    // Start cleanup worker thread
    cleanup_thread_ = std::thread(&AgentActivityFeed::cleanup_worker, this);

    logger_->info("AgentActivityFeed initialization complete");
    return true;
}

void AgentActivityFeed::shutdown() {
    if (!running_) return;

    logger_->info("Shutting down AgentActivityFeed");

    running_ = false;

    // Wake up cleanup thread
    {
        std::unique_lock<std::mutex> lock(cleanup_mutex_);
        cleanup_cv_.notify_one();
    }

    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }

    // Clear subscriptions
    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        subscriptions_.clear();
        subscription_callbacks_.clear();
    }

    logger_->info("AgentActivityFeed shutdown complete");
}

bool AgentActivityFeed::record_activity(const AgentActivityEvent& event) {
    try {
        // Thread-safe storage
        {
            std::lock_guard<std::mutex> lock(activities_mutex_);

            // Ensure agent has an activity queue
            if (agent_activities_.find(event.agent_id) == agent_activities_.end()) {
                agent_activities_[event.agent_id] = std::deque<AgentActivityEvent>();
            }

            auto& agent_queue = agent_activities_[event.agent_id];

            // Enforce per-agent limit
            if (agent_queue.size() >= config_.max_events_per_agent) {
                agent_queue.pop_front(); // Remove oldest
            }

            // Add new event
            agent_queue.push_back(event);

            // Enforce global buffer limit (rough approximation)
            size_t total_events = 0;
            for (const auto& [_, queue] : agent_activities_) {
                total_events += queue.size();
            }

            while (total_events > config_.max_events_buffer) {
                // Remove from the agent with the most events
                auto max_it = std::max_element(agent_activities_.begin(), agent_activities_.end(),
                    [](const auto& a, const auto& b) { return a.second.size() < b.second.size(); });

                if (max_it != agent_activities_.end() && !max_it->second.empty()) {
                    max_it->second.pop_front();
                    total_events--;
                } else {
                    break;
                }
            }
        }

        // Update statistics
        update_agent_stats(event);

        // Persist if enabled
        if (config_.enable_persistence) {
            persist_activity(event);
        }

        // Notify subscribers
        notify_subscribers(event);

        logger_->debug("Recorded activity event: {} for agent: {}",
                      event.title, event.agent_id);

        return true;

    } catch (const std::exception& e) {
        logger_->error("Failed to record activity event: {}", e.what());
        return false;
    }
}

std::string AgentActivityFeed::subscribe(const ActivityFeedSubscription& subscription,
                                       std::function<void(const AgentActivityEvent&)> callback) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);

    if (subscriptions_.size() >= config_.max_subscriptions) {
        logger_->warn("Maximum subscriptions reached, cannot add new subscription");
        return "";
    }

    subscriptions_[subscription.subscription_id] = subscription;
    subscription_callbacks_[subscription.subscription_id] = callback;

    logger_->info("Added subscription {} for client {}", subscription.subscription_id,
                 subscription.client_id);

    return subscription.subscription_id;
}

bool AgentActivityFeed::unsubscribe(const std::string& subscription_id) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);

    auto sub_it = subscriptions_.find(subscription_id);
    auto cb_it = subscription_callbacks_.find(subscription_id);

    if (sub_it != subscriptions_.end()) {
        subscriptions_.erase(sub_it);
        logger_->info("Removed subscription {}", subscription_id);
    }

    if (cb_it != subscription_callbacks_.end()) {
        subscription_callbacks_.erase(cb_it);
    }

    return true;
}

std::vector<AgentActivityEvent> AgentActivityFeed::query_activities(const ActivityFeedFilter& filter) {
    std::vector<AgentActivityEvent> results;
    std::lock_guard<std::mutex> lock(activities_mutex_);

    for (const auto& [agent_id, agent_queue] : agent_activities_) {
        // Check agent filter
        if (!filter.agent_ids.empty() &&
            std::find(filter.agent_ids.begin(), filter.agent_ids.end(), agent_id) == filter.agent_ids.end()) {
            continue;
        }

        for (const auto& event : agent_queue) {
            if (matches_filter(event, filter)) {
                results.push_back(event);
            }
        }
    }

    // Sort by timestamp (newest first by default)
    std::sort(results.begin(), results.end(),
        [ascending = filter.ascending_order](const AgentActivityEvent& a, const AgentActivityEvent& b) {
            if (ascending) {
                return a.timestamp < b.timestamp;
            } else {
                return a.timestamp > b.timestamp;
            }
        });

    // Apply result limit
    if (results.size() > filter.max_results) {
        results.erase(results.begin() + static_cast<ptrdiff_t>(filter.max_results), results.end());
    }

    logger_->debug("Query returned " + std::to_string(results.size()) + " activities");
    return results;
}

std::optional<AgentActivityStats> AgentActivityFeed::get_agent_stats(const std::string& agent_id) {
    std::lock_guard<std::mutex> lock(activities_mutex_);

    auto it = agent_stats_.find(agent_id);
    if (it != agent_stats_.end()) {
        return it->second;
    }

    return std::nullopt;
}

nlohmann::json AgentActivityFeed::get_feed_stats() {
    std::lock_guard<std::mutex> lock(activities_mutex_);

    size_t total_events = 0;
    size_t total_agents = agent_activities_.size();
    size_t total_subscriptions = subscriptions_.size();

    std::unordered_map<int, size_t> global_activity_counts;

    for (const auto& [agent_id, queue] : agent_activities_) {
        total_events += queue.size();

        auto stats_it = agent_stats_.find(agent_id);
        if (stats_it != agent_stats_.end()) {
            for (const auto& [type, count] : stats_it->second.activity_type_counts) {
                global_activity_counts[type] += count;
            }
        }
    }

    nlohmann::json activity_counts_json;
    for (const auto& [type, count] : global_activity_counts) {
        activity_counts_json[std::to_string(type)] = count;
    }

    return {
        {"total_events", total_events},
        {"total_agents", total_agents},
        {"total_subscriptions", total_subscriptions},
        {"activity_type_counts", activity_counts_json},
        {"config", config_.to_json()},
        {"uptime_seconds", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - std::chrono::system_clock::now()).count()} // Placeholder
    };
}

std::string AgentActivityFeed::export_activities(const ActivityFeedFilter& filter, const std::string& format) {
    auto activities = query_activities(filter);

    if (format == "json") {
        nlohmann::json export_json = nlohmann::json::array();
        for (const auto& activity : activities) {
            export_json.push_back(activity.to_json());
        }
        return export_json.dump(2);

    } else if (format == "csv") {
        std::stringstream csv;
        csv << "event_id,agent_id,activity_type,severity,title,description,timestamp\n";

        for (const auto& activity : activities) {
            csv << activity.event_id << ","
                << activity.agent_id << ","
                << static_cast<int>(activity.activity_type) << ","
                << static_cast<int>(activity.severity) << ","
                << "\"" << activity.title << "\","
                << "\"" << activity.description << "\","
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                    activity.timestamp.time_since_epoch()).count() << "\n";
        }

        return csv.str();

    } else {
        logger_->warn("Unsupported export format: {}", format);
        return "";
    }
}

size_t AgentActivityFeed::cleanup_old_activities() {
    std::lock_guard<std::mutex> lock(activities_mutex_);

    auto cutoff_time = get_cutoff_time();
    size_t removed_count = 0;

    for (auto& [agent_id, queue] : agent_activities_) {
        // Remove old events
        while (!queue.empty() && queue.front().timestamp < cutoff_time) {
            queue.pop_front();
            removed_count++;
        }
    }

    // Remove empty agent queues
    for (auto it = agent_activities_.begin(); it != agent_activities_.end(); ) {
        if (it->second.empty()) {
            it = agent_activities_.erase(it);
        } else {
            ++it;
        }
    }

    logger_->info("Cleaned up " + std::to_string(removed_count) + " old activities");
    return removed_count;
}

void AgentActivityFeed::update_agent_stats(const AgentActivityEvent& event) {
    auto& stats = agent_stats_[event.agent_id];
    stats.agent_id = event.agent_id;
    stats.total_activities++;
    stats.last_activity = event.timestamp;

    // Update activity type counts
    stats.activity_type_counts[static_cast<int>(event.activity_type)]++;

    // Update severity counts
    if (event.severity == ActivitySeverity::ERROR) {
        stats.error_count++;
    } else if (event.severity == ActivitySeverity::WARNING) {
        stats.warning_count++;
    }

    // Update time-based counts (simplified)
    auto now = std::chrono::system_clock::now();
    auto one_hour_ago = now - std::chrono::hours(1);
    auto one_day_ago = now - std::chrono::hours(24);

    if (event.timestamp > one_hour_ago) {
        stats.activities_last_hour++;
    }
    if (event.timestamp > one_day_ago) {
        stats.activities_last_24h++;
    }
}

void AgentActivityFeed::notify_subscribers(const AgentActivityEvent& event) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);

    for (const auto& [sub_id, subscription] : subscriptions_) {
        if (matches_filter(event, subscription.filter)) {
            auto callback_it = subscription_callbacks_.find(sub_id);
            if (callback_it != subscription_callbacks_.end()) {
                try {
                    callback_it->second(event);
                } catch (const std::exception& e) {
                    logger_->error("Error in subscription callback for {}: {}", sub_id, e.what());
                }
            }
        }
    }
}

bool AgentActivityFeed::matches_filter(const AgentActivityEvent& event, const ActivityFeedFilter& filter) {
    // Time range check
    if (event.timestamp < filter.start_time || event.timestamp > filter.end_time) {
        return false;
    }

    // Activity type filter
    if (!filter.activity_types.empty()) {
        bool type_match = false;
        for (auto type : filter.activity_types) {
            if (event.activity_type == type) {
                type_match = true;
                break;
            }
        }
        if (!type_match) return false;
    }

    // Severity filter
    if (!filter.severities.empty()) {
        bool severity_match = false;
        for (auto severity : filter.severities) {
            if (event.severity == severity) {
                severity_match = true;
                break;
            }
        }
        if (!severity_match) return false;
    }

    // Metadata filters
    for (const auto& [key, value] : filter.metadata_filters) {
        auto meta_it = event.metadata.find(key);
        if (meta_it == event.metadata.end() || meta_it->second != value) {
            return false;
        }
    }

    return true;
}

void AgentActivityFeed::cleanup_worker() {
    logger_->info("Activity feed cleanup worker started");

    while (running_) {
        std::unique_lock<std::mutex> lock(cleanup_mutex_);

        // Wait for cleanup interval or shutdown signal
        if (cleanup_cv_.wait_for(lock, config_.cleanup_interval,
                                [this]() { return !running_; })) {
            // Shutdown signal received
            break;
        }

        if (!running_) break;

        // Perform cleanup
        try {
            cleanup_old_activities();
        } catch (const std::exception& e) {
            logger_->error("Error during activity cleanup: {}", e.what());
        }
    }

    logger_->info("Activity feed cleanup worker stopped");
}

bool AgentActivityFeed::persist_activity(const AgentActivityEvent& event) {
    try {
        // Production-grade database persistence for agent activity events
        if (!db_connection_) {
            logger_->error("Database connection not available for activity persistence");
            return false;
        }

        auto conn = db_connection_->get_connection();
        if (!conn) {
            logger_->error("Failed to get database connection for activity persistence");
            return false;
        }

        pqxx::work txn(*conn);

        // Insert agent activity event
        txn.exec_params(
            "INSERT INTO agent_activity_events "
            "(event_id, agent_type, agent_name, event_type, event_category, description, "
            "event_metadata, entity_id, entity_type, severity, occurred_at, processed_at) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12) "
            "ON CONFLICT (event_id) DO UPDATE SET "
            "processed_at = EXCLUDED.processed_at, "
            "event_metadata = EXCLUDED.event_metadata",
            event.event_id,
            event.agent_type,
            event.agent_name,
            event.event_type,
            event.event_category,
            event.description,
            nlohmann::json(event.event_metadata).dump(),
            event.entity_id.value_or(""),
            event.entity_type.value_or(""),
            event.severity,
            std::chrono::system_clock::to_time_t(event.occurred_at),
            event.processed_at ? std::chrono::system_clock::to_time_t(*event.processed_at) : nullptr
        );

        txn.commit();

        logger_->debug("Persisted agent activity event: {} ({})", event.event_id, event.description);
        return true;

    } catch (const std::exception& e) {
        logger_->error("Failed to persist agent activity event {}: {}", event.event_id, e.what());
        return false;
    }
}

std::vector<AgentActivityEvent> AgentActivityFeed::load_activities_from_persistence(const ActivityFeedFilter& filter) {
    try {
        // Production-grade database loading for agent activity events
        if (!db_connection_) {
            logger_->error("Database connection not available for activity loading");
            return {};
        }

        auto conn = db_connection_->get_connection();
        if (!conn) {
            logger_->error("Failed to get database connection for activity loading");
            return {};
        }

        // Build query with filters
        std::string query = "SELECT event_id, agent_type, agent_name, event_type, event_category, "
                           "description, event_metadata, entity_id, entity_type, severity, "
                           "occurred_at, processed_at FROM agent_activity_events WHERE 1=1";

        std::vector<std::string> params;

        if (!filter.agent_type.empty()) {
            query += " AND agent_type = $" + std::to_string(params.size() + 1);
            params.push_back(filter.agent_type);
        }

        if (!filter.agent_name.empty()) {
            query += " AND agent_name = $" + std::to_string(params.size() + 1);
            params.push_back(filter.agent_name);
        }

        if (!filter.event_category.empty()) {
            query += " AND event_category = $" + std::to_string(params.size() + 1);
            params.push_back(filter.event_category);
        }

        if (!filter.entity_id.empty()) {
            query += " AND entity_id = $" + std::to_string(params.size() + 1);
            params.push_back(filter.entity_id);
        }

        if (!filter.entity_type.empty()) {
            query += " AND entity_type = $" + std::to_string(params.size() + 1);
            params.push_back(filter.entity_type);
        }

        if (filter.start_time) {
            query += " AND occurred_at >= $" + std::to_string(params.size() + 1);
            params.push_back(std::to_string(std::chrono::system_clock::to_time_t(*filter.start_time)));
        }

        if (filter.end_time) {
            query += " AND occurred_at <= $" + std::to_string(params.size() + 1);
            params.push_back(std::to_string(std::chrono::system_clock::to_time_t(*filter.end_time)));
        }

        // Add severity filter
        if (!filter.severity_levels.empty()) {
            query += " AND severity IN (";
            for (size_t i = 0; i < filter.severity_levels.size(); ++i) {
                if (i > 0) query += ",";
                query += "$" + std::to_string(params.size() + 1);
                params.push_back(filter.severity_levels[i]);
            }
            query += ")";
        }

        // Add ordering and limit
        query += " ORDER BY occurred_at DESC";

        if (filter.limit > 0) {
            query += " LIMIT $" + std::to_string(params.size() + 1);
            params.push_back(std::to_string(filter.limit));
        }

        pqxx::work txn(*conn);
        pqxx::result result = txn.exec_params(query, params.begin(), params.end());

        std::vector<AgentActivityEvent> activities;
        activities.reserve(result.size());

        for (const auto& row : result) {
            AgentActivityEvent event;

            event.event_id = row["event_id"].as<std::string>();
            event.agent_type = row["agent_type"].as<std::string>();
            event.agent_name = row["agent_name"].as<std::string>();
            event.event_type = row["event_type"].as<std::string>();
            event.event_category = row["event_category"].as<std::string>();
            event.description = row["description"].as<std::string>();
            event.severity = row["severity"].as<std::string>();

            // Parse optional fields
            if (!row["entity_id"].is_null()) {
                event.entity_id = row["entity_id"].as<std::string>();
            }
            if (!row["entity_type"].is_null()) {
                event.entity_type = row["entity_type"].as<std::string>();
            }

            // Parse metadata JSON
            if (!row["event_metadata"].is_null()) {
                try {
                    std::string metadata_str = row["event_metadata"].as<std::string>();
                    event.event_metadata = nlohmann::json::parse(metadata_str);
                } catch (const std::exception& e) {
                    logger_->warn("Failed to parse event metadata for {}: {}", event.event_id, e.what());
                    event.event_metadata = nlohmann::json::object();
                }
            } else {
                event.event_metadata = nlohmann::json::object();
            }

            // Parse timestamps
            event.occurred_at = std::chrono::system_clock::from_time_t(row["occurred_at"].as<time_t>());

            if (!row["processed_at"].is_null()) {
                event.processed_at = std::chrono::system_clock::from_time_t(row["processed_at"].as<time_t>());
            }

            activities.push_back(event);
        }

        txn.commit();

        logger_->debug("Loaded {} agent activity events from persistence", activities.size());
        return activities;

    } catch (const std::exception& e) {
        logger_->error("Failed to load agent activities from persistence: {}", e.what());
        return {};
    }
}

std::chrono::system_clock::time_point AgentActivityFeed::get_cutoff_time() const {
    return std::chrono::system_clock::now() - config_.retention_period;
}

} // namespace regulens
