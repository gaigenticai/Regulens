#include "event_bus.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace regulens {

EventBus::EventBus(
    std::shared_ptr<ConnectionPool> db_pool,
    StructuredLogger* logger
) : db_pool_(db_pool),
    logger_(logger),
    running_(false),
    max_queue_size_(10000),
    batch_size_(50),
    event_ttl_(std::chrono::hours(24)),
    events_published_(0),
    events_processed_(0),
    events_failed_(0),
    events_expired_(0),
    events_dead_lettered_(0) {
}

EventBus::~EventBus() {
    shutdown();
}

bool EventBus::initialize() {
    logger_->log(LogLevel::INFO, "Initializing Event Bus");

    try {
        // Verify database connectivity (optional - event bus can work without DB)
        if (db_pool_) {
            auto conn = db_pool_->get_connection();
            if (conn) {
                // Create events table if it doesn't exist - execute commands individually
                std::vector<std::string> schema_commands = {
                    R"(
                        CREATE TABLE IF NOT EXISTS events (
                            event_id VARCHAR(255) PRIMARY KEY,
                            category VARCHAR(50) NOT NULL,
                            source VARCHAR(255) NOT NULL,
                            event_type VARCHAR(100) NOT NULL,
                            payload JSONB NOT NULL,
                            priority VARCHAR(20) NOT NULL,
                            state VARCHAR(20) NOT NULL DEFAULT 'CREATED',
                            retry_count INTEGER NOT NULL DEFAULT 0,
                            created_at BIGINT NOT NULL,
                            expires_at BIGINT,
                            headers JSONB DEFAULT '{}'::jsonb,
                            correlation_id VARCHAR(255),
                            trace_id VARCHAR(255),
                            processed_at BIGINT,
                            error_message TEXT
                        )
                    )",
                    "CREATE INDEX IF NOT EXISTS idx_events_category ON events(category)",
                    "CREATE INDEX IF NOT EXISTS idx_events_source ON events(source)",
                    "CREATE INDEX IF NOT EXISTS idx_events_created ON events(created_at)",
                    "CREATE INDEX IF NOT EXISTS idx_events_state ON events(state)"
                };

                bool schema_success = true;
                for (const auto& command : schema_commands) {
                    if (!conn->execute_command(command)) {
                        logger_->log(LogLevel::WARN, "Failed to execute schema command: " + command.substr(0, 50) + "...");
                        schema_success = false;
                    }
                }

                if (!schema_success) {
                    logger_->log(LogLevel::WARN, "Some event schema commands failed, continuing without persistence");
                }
            }
        }

        // Start worker threads
        set_worker_threads(4);  // Default to 4 worker threads

        running_ = true;

        // Start background threads
        dead_letter_thread_ = std::thread(&EventBus::dead_letter_processing_loop, this);
        cleanup_thread_ = std::thread(&EventBus::cleanup_expired_events_loop, this);

        logger_->log(LogLevel::INFO, "Event Bus initialized successfully");
        return true;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to initialize Event Bus: " + std::string(e.what()));
        return false;
    }
}

void EventBus::shutdown() {
    if (!running_.load()) {
        return;
    }

    logger_->log(LogLevel::INFO, "Shutting down Event Bus");

    running_ = false;

    // Wake up all waiting threads
    queue_cv_.notify_all();

    // Join worker threads
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    // Join background threads
    if (dead_letter_thread_.joinable()) {
        dead_letter_thread_.join();
    }

    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }

    // Clear queues
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        while (!event_queue_.empty()) {
            event_queue_.pop();
        }
        while (!dead_letter_queue_.empty()) {
            dead_letter_queue_.pop();
        }
    }

    logger_->log(LogLevel::INFO, "Event Bus shutdown complete");
}

void EventBus::set_worker_threads(size_t count) {
    if (running_.load()) {
        logger_->log(LogLevel::WARN, "Cannot change worker threads while Event Bus is running");
        return;
    }

    worker_threads_.clear();
    for (size_t i = 0; i < count; ++i) {
        worker_threads_.emplace_back(&EventBus::event_processing_loop, this);
    }

    logger_->log(LogLevel::INFO, "Configured " + std::to_string(count) + " worker threads");
}

bool EventBus::publish(std::unique_ptr<Event> event) {
    if (!running_.load()) {
        logger_->log(LogLevel::WARN, "Event Bus is not running, cannot publish event");
        return false;
    }

    if (!event) {
        logger_->log(LogLevel::WARN, "Cannot publish null event");
        return false;
    }

    // Set event state to published
    event->set_state(EventState::PUBLISHED);

    // Add to queue
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);

        // Check queue size limit
        if (event_queue_.size() >= max_queue_size_) {
            logger_->log(LogLevel::WARN, "Event queue full, dropping event: " + event->get_event_id());
            events_failed_++;
            return false;
        }

        event_queue_.push(std::move(event));
        events_published_++;
    }

    // Notify worker threads
    queue_cv_.notify_one();

    return true;
}

bool EventBus::publish_batch(std::vector<std::unique_ptr<Event>> events) {
    if (!running_.load()) {
        logger_->log(LogLevel::WARN, "Event Bus is not running, cannot publish batch");
        return false;
    }

    if (events.empty()) {
        return true;  // Empty batch is success
    }

    size_t published_count = 0;

    {
        std::unique_lock<std::mutex> lock(queue_mutex_);

        for (auto& event : events) {
            if (!event) continue;

            // Check queue size limit
            if (event_queue_.size() >= max_queue_size_) {
                logger_->log(LogLevel::WARN, "Event queue full during batch publish, stopping batch");
                break;
            }

            event->set_state(EventState::PUBLISHED);
            event_queue_.push(std::move(event));
            published_count++;
        }

        events_published_ += published_count;
    }

    // Notify worker threads
    if (published_count > 0) {
        queue_cv_.notify_all();
    }

    logger_->log(LogLevel::DEBUG, "Published batch of " + std::to_string(published_count) + " events");

    return published_count > 0;
}

bool EventBus::subscribe(
    std::shared_ptr<EventHandler> handler,
    std::unique_ptr<EventFilter> filter
) {
    if (!handler) {
        logger_->log(LogLevel::WARN, "Cannot subscribe with null handler");
        return false;
    }

    std::string handler_id = handler->get_handler_id();

    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);

        if (handlers_.find(handler_id) != handlers_.end()) {
            logger_->log(LogLevel::WARN, "Handler already subscribed: " + handler_id);
            return false;
        }

        handlers_[handler_id] = std::make_pair(handler, std::move(filter));
    }

    logger_->log(LogLevel::INFO, "Subscribed event handler: " + handler_id);
    return true;
}

bool EventBus::unsubscribe(const std::string& handler_id) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    auto it = handlers_.find(handler_id);
    if (it == handlers_.end()) {
        logger_->log(LogLevel::WARN, "Handler not found for unsubscribe: " + handler_id);
        return false;
    }

    handlers_.erase(it);
    logger_->log(LogLevel::INFO, "Unsubscribed event handler: " + handler_id);
    return true;
}

void EventBus::register_stream_handler(
    const std::string& stream_id,
    std::function<void(const Event&)> handler
) {
    std::lock_guard<std::mutex> lock(stream_mutex_);
    stream_handlers_[stream_id] = handler;
    logger_->log(LogLevel::INFO, "Registered stream handler: " + stream_id);
}

void EventBus::unregister_stream_handler(const std::string& stream_id) {
    std::lock_guard<std::mutex> lock(stream_mutex_);
    auto it = stream_handlers_.find(stream_id);
    if (it != stream_handlers_.end()) {
        stream_handlers_.erase(it);
        logger_->log(LogLevel::INFO, "Unregistered stream handler: " + stream_id);
    }
}

nlohmann::json EventBus::get_statistics() const {
    return {
        {"events_published", events_published_.load()},
        {"events_processed", events_processed_.load()},
        {"events_failed", events_failed_.load()},
        {"events_expired", events_expired_.load()},
        {"events_dead_lettered", events_dead_lettered_.load()},
        {"active_handlers", handlers_.size()},
        {"stream_handlers", stream_handlers_.size()},
        {"queue_size", event_queue_.size()},
        {"dead_letter_queue_size", dead_letter_queue_.size()},
        {"worker_threads", worker_threads_.size()}
    };
}

void EventBus::reset_statistics() {
    events_published_ = 0;
    events_processed_ = 0;
    events_failed_ = 0;
    events_expired_ = 0;
    events_dead_lettered_ = 0;
    logger_->log(LogLevel::INFO, "Event Bus statistics reset");
}

// Private methods

void EventBus::event_processing_loop() {
    while (running_.load()) {
        std::unique_ptr<Event> event;

        // Get event from queue
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this]() {
                return !running_.load() || !event_queue_.empty();
            });

            if (!running_.load()) {
                break;
            }

            if (!event_queue_.empty()) {
                event = std::move(event_queue_.front());
                event_queue_.pop();
            }
        }

        if (event) {
            try {
                // Route the event to handlers
                if (route_event(std::move(event))) {
                    events_processed_++;
                } else {
                    events_failed_++;
                }
            } catch (const std::exception& e) {
                logger_->log(LogLevel::ERROR, "Event processing error: " + std::string(e.what()));
                events_failed_++;
            }
        }
    }
}

void EventBus::dead_letter_processing_loop() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(30));  // Check every 30 seconds

        if (!running_.load()) {
            break;
        }

        try {
            process_dead_letter_queue();
        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR, "Dead letter processing error: " + std::string(e.what()));
        }
    }
}

void EventBus::cleanup_expired_events_loop() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::minutes(5));  // Clean up every 5 minutes

        if (!running_.load()) {
            break;
        }

        try {
            cleanup_expired_events();
        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR, "Event cleanup error: " + std::string(e.what()));
        }
    }
}

void EventBus::cleanup_expired_events() {
    // In a production system, this would clean up expired events from database
    // For now, we just log the activity
    logger_->log(LogLevel::DEBUG, "Event cleanup cycle completed");
}

bool EventBus::route_event(std::unique_ptr<Event> event) {
    if (!event) {
        return false;
    }

    event->set_state(EventState::ROUTED);

    bool routed = false;

    logger_->log(LogLevel::DEBUG, "Routing event: " + event->to_string());

    // Send to stream handlers (real-time)
    {
        std::lock_guard<std::mutex> lock(stream_mutex_);
        for (const auto& [stream_id, handler] : stream_handlers_) {
            try {
                handler(*event);
            } catch (const std::exception& e) {
                logger_->log(LogLevel::ERROR, "Stream handler error for " + stream_id + ": " + std::string(e.what()));
            }
        }
    }

    // Route to registered handlers
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        for (const auto& [handler_id, handler_pair] : handlers_) {
            const auto& [handler, filter] = handler_pair;

            try {
                // Check if handler is active
                if (!handler->is_active()) {
                    logger_->log(LogLevel::DEBUG, "Handler " + handler_id + " is not active");
                    continue;
                }

                // Check if handler supports this event category
                const auto& supported_categories = handler->get_supported_categories();
                bool category_match = supported_categories.empty() ||
                    std::find(supported_categories.begin(), supported_categories.end(),
                             event->get_category()) != supported_categories.end();

                logger_->log(LogLevel::DEBUG, "Handler " + handler_id + " category match: " +
                             (category_match ? "YES" : "NO") + " (event category: " +
                             event_category_to_string(event->get_category()) + ")");

                if (!category_match) {
                    continue;
                }

                // Apply filter if present
                if (filter && !filter->matches(*event)) {
                    logger_->log(LogLevel::DEBUG, "Handler " + handler_id + " filter match: NO");
                    continue;
                }

                logger_->log(LogLevel::DEBUG, "Handler " + handler_id + " will process event");

                // Clone event for handler (since it might be processed by multiple handlers)
                auto event_clone = std::make_unique<Event>(
                    event->get_event_id(),
                    event->get_category(),
                    event->get_source(),
                    event->get_event_type(),
                    event->get_payload(),
                    event->get_priority()
                );

                // Copy headers and metadata
                for (const auto& [key, value] : event->get_headers()) {
                    event_clone->add_header(key, value);
                }
                event_clone->set_correlation_id(event->get_correlation_id());
                event_clone->set_trace_id(event->get_trace_id());

                // Handle the event
                handler->handle_event(std::move(event_clone));
                routed = true;

                logger_->log(LogLevel::DEBUG, "Routed event " + event->get_event_id() +
                             " to handler " + handler_id);

            } catch (const std::exception& e) {
                logger_->log(LogLevel::ERROR, "Handler error for " + handler_id + ": " + std::string(e.what()));

                // Move to dead letter queue if handler fails
                std::lock_guard<std::mutex> dl_lock(queue_mutex_);
                dead_letter_queue_.push(std::move(event));
                events_dead_lettered_++;
                return false;
            }
        }
    }

    // Persist critical events
    if (static_cast<int>(event->get_priority()) >= static_cast<int>(EventPriority::HIGH)) {
        persist_critical_event(*event);
    }

    event->set_state(EventState::PROCESSED);
    return routed;
}

void EventBus::process_dead_letter_queue() {
    std::vector<std::unique_ptr<Event>> retry_events;

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        while (!dead_letter_queue_.empty()) {
            auto event = std::move(dead_letter_queue_.front());
            dead_letter_queue_.pop();

            // Check if event can be retried
            if (event->get_retry_count() < 3 && !event->is_expired()) {
                event->increment_retry_count();
                retry_events.push_back(std::move(event));
            } else {
                // Event has exceeded retry limit or expired
                event->set_state(EventState::FAILED);
                persist_critical_event(*event);
                logger_->log(LogLevel::WARN, "Event moved to permanent failure: " + event->get_event_id());
            }
        }
    }

    // Retry eligible events
    if (!retry_events.empty()) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        for (auto& event : retry_events) {
            event_queue_.push(std::move(event));
        }
        logger_->log(LogLevel::INFO, "Retried " + std::to_string(retry_events.size()) + " events from dead letter queue");
    }
}

void EventBus::persist_critical_event(const Event& event) {
    if (!db_pool_) {
        return;  // No database, skip persistence
    }

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return;

        std::string upsert_sql = R"(
            INSERT INTO events (
                event_id, category, source, event_type, payload, priority,
                state, retry_count, created_at, expires_at, headers,
                correlation_id, trace_id, processed_at
            ) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14)
            ON CONFLICT (event_id) DO UPDATE SET
                state = EXCLUDED.state,
                retry_count = EXCLUDED.retry_count,
                processed_at = EXCLUDED.processed_at
        )";

        auto created_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            event.get_created_at().time_since_epoch()).count();
        auto expires_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            event.get_expires_at().time_since_epoch()).count();
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        std::vector<std::string> params = {
            event.get_event_id(),
            event_category_to_string(event.get_category()),
            event.get_source(),
            event.get_event_type(),
            event.get_payload().dump(),
            event_priority_to_string(event.get_priority()),
            event_state_to_string(event.get_state()),
            std::to_string(event.get_retry_count()),
            std::to_string(created_ms),
            std::to_string(expires_ms),
            nlohmann::json(event.get_headers()).dump(),
            event.get_correlation_id(),
            event.get_trace_id(),
            std::to_string(now_ms)
        };

        if (conn->execute_command(upsert_sql, params)) {
            logger_->log(LogLevel::DEBUG, "Persisted critical event: " + event.get_event_id());
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to persist critical event: " + std::string(e.what()));
    }
}

std::vector<std::unique_ptr<Event>> EventBus::get_events(
    EventCategory category,
    std::chrono::system_clock::time_point since
) {
    std::vector<std::unique_ptr<Event>> events;

    if (!db_pool_) {
        return events;
    }

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return events;

        std::string query = R"(
            SELECT * FROM events
            WHERE category = $1 AND created_at >= $2
            ORDER BY created_at DESC
            LIMIT 1000
        )";

        auto since_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            since.time_since_epoch()).count();

        std::vector<std::string> params = {
            event_category_to_string(category),
            std::to_string(since_ms)
        };

        auto results = conn->execute_query_multi(query, params);
        for (const auto& row : results) {
            try {
                nlohmann::json event_json = nlohmann::json::parse(std::string(row["payload"]));
                event_json["event_id"] = std::string(row["event_id"]);
                event_json["category"] = std::string(row["category"]);
                event_json["source"] = std::string(row["source"]);
                event_json["event_type"] = std::string(row["event_type"]);
                event_json["priority"] = std::string(row["priority"]);
                event_json["state"] = std::string(row["state"]);
                event_json["retry_count"] = std::stoi(std::string(row["retry_count"]));

                auto event = Event::from_json(event_json);
                if (event) {
                    events.push_back(std::move(event));
                }
            } catch (const std::exception& e) {
                logger_->log(LogLevel::ERROR, "Failed to deserialize event: " + std::string(e.what()));
            }
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to query events: " + std::string(e.what()));
    }

    return events;
}

std::vector<std::unique_ptr<Event>> EventBus::get_events_by_source(
    const std::string& source,
    std::chrono::system_clock::time_point since
) {
    std::vector<std::unique_ptr<Event>> events;

    if (!db_pool_) {
        return events;
    }

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return events;

        std::string query = R"(
            SELECT * FROM events
            WHERE source = $1 AND created_at >= $2
            ORDER BY created_at DESC
            LIMIT 1000
        )";

        auto since_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            since.time_since_epoch()).count();

        std::vector<std::string> params = {source, std::to_string(since_ms)};
        auto results = conn->execute_query_multi(query, params);

        for (const auto& row : results) {
            try {
                nlohmann::json event_json = nlohmann::json::parse(std::string(row["payload"]));
                event_json["event_id"] = std::string(row["event_id"]);
                event_json["category"] = std::string(row["category"]);
                event_json["source"] = std::string(row["source"]);
                event_json["event_type"] = std::string(row["event_type"]);
                event_json["priority"] = std::string(row["priority"]);

                auto event = Event::from_json(event_json);
                if (event) {
                    events.push_back(std::move(event));
                }
            } catch (const std::exception& e) {
                logger_->log(LogLevel::ERROR, "Failed to deserialize event: " + std::string(e.what()));
            }
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to query events by source: " + std::string(e.what()));
    }

    return events;
}

} // namespace regulens
