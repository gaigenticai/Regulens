/**
 * Event Bus - Enterprise Event-Driven Architecture
 *
 * High-performance, scalable event bus for real-time processing and
 * asynchronous communication across all system components.
 *
 * Features:
 * - Publisher-subscriber pattern
 * - Event routing and filtering
 * - Asynchronous processing queues
 * - Dead letter queues for failed events
 * - Event persistence for critical events
 * - Real-time streaming capabilities
 */

#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <chrono>
#include "event.hpp"
#include "../logging/structured_logger.hpp"
#include "../database/postgresql_connection.hpp"

namespace regulens {

class EventHandler {
public:
    virtual ~EventHandler() = default;
    virtual void handle_event(std::unique_ptr<Event> event) = 0;
    virtual std::vector<EventCategory> get_supported_categories() const = 0;
    virtual std::string get_handler_id() const = 0;
    virtual bool is_active() const = 0;
};

class EventFilter {
public:
    virtual ~EventFilter() = default;
    virtual bool matches(const Event& event) const = 0;
};

class CategoryFilter : public EventFilter {
public:
    explicit CategoryFilter(EventCategory category) : category_(category) {}
    bool matches(const Event& event) const override {
        return event.get_category() == category_;
    }
private:
    EventCategory category_;
};

class SourceFilter : public EventFilter {
public:
    explicit SourceFilter(const std::string& source) : source_(source) {}
    bool matches(const Event& event) const override {
        return event.get_source() == source_;
    }
private:
    std::string source_;
};

class PriorityFilter : public EventFilter {
public:
    PriorityFilter(EventPriority min_priority) : min_priority_(min_priority) {}
    bool matches(const Event& event) const override {
        return static_cast<int>(event.get_priority()) >= static_cast<int>(min_priority_);
    }
private:
    EventPriority min_priority_;
};

class CompositeFilter : public EventFilter {
public:
    void add_filter(std::unique_ptr<EventFilter> filter) {
        filters_.push_back(std::move(filter));
    }

    bool matches(const Event& event) const override {
        for (const auto& filter : filters_) {
            if (!filter->matches(event)) {
                return false;
            }
        }
        return true;
    }
private:
    std::vector<std::unique_ptr<EventFilter>> filters_;
};

class EventBus {
public:
    EventBus(
        std::shared_ptr<ConnectionPool> db_pool,
        StructuredLogger* logger
    );

    ~EventBus();

    // Lifecycle management
    bool initialize();
    void shutdown();
    bool is_running() const { return running_.load(); }

    // Event publishing
    bool publish(std::unique_ptr<Event> event);
    bool publish_batch(std::vector<std::unique_ptr<Event>> events);

    // Event subscription
    bool subscribe(
        std::shared_ptr<EventHandler> handler,
        std::unique_ptr<EventFilter> filter = nullptr
    );

    bool unsubscribe(const std::string& handler_id);

    // Event querying and management
    std::vector<std::unique_ptr<Event>> get_events(
        EventCategory category,
        std::chrono::system_clock::time_point since = std::chrono::system_clock::now() - std::chrono::hours(1)
    );

    std::vector<std::unique_ptr<Event>> get_events_by_source(
        const std::string& source,
        std::chrono::system_clock::time_point since = std::chrono::system_clock::now() - std::chrono::hours(1)
    );

    // Event streaming (WebSocket/real-time)
    void register_stream_handler(
        const std::string& stream_id,
        std::function<void(const Event&)> handler
    );

    void unregister_stream_handler(const std::string& stream_id);

    // Performance and monitoring
    nlohmann::json get_statistics() const;
    void reset_statistics();
    
    // Health check methods for monitoring
    size_t get_pending_event_count() const;
    size_t get_processing_event_count() const;
    size_t get_failed_event_count() const;
    size_t get_queue_capacity() const { return max_queue_size_; }

    // Configuration
    void set_max_queue_size(size_t size) { max_queue_size_ = size; }
    void set_worker_threads(size_t count);
    void set_event_ttl(std::chrono::seconds ttl) { event_ttl_ = ttl; }
    void set_batch_size(size_t size) { batch_size_ = size; }

private:
    // Event processing
    void event_processing_loop();
    void dead_letter_processing_loop();
    void cleanup_expired_events_loop();
    void cleanup_expired_events();

    bool route_event(std::unique_ptr<Event> event);
    void process_dead_letter_queue();
    void persist_critical_event(const Event& event);

    // Internal data structures
    std::shared_ptr<ConnectionPool> db_pool_;
    StructuredLogger* logger_;

    std::atomic<bool> running_;
    std::vector<std::thread> worker_threads_;
    std::thread dead_letter_thread_;
    std::thread cleanup_thread_;

    // Event queues
    std::queue<std::unique_ptr<Event>> event_queue_;
    std::queue<std::unique_ptr<Event>> dead_letter_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    // Event routing
    std::unordered_map<std::string, std::pair<std::shared_ptr<EventHandler>, std::unique_ptr<EventFilter>>> handlers_;
    std::mutex handlers_mutex_;

    // Streaming handlers
    std::unordered_map<std::string, std::function<void(const Event&)>> stream_handlers_;
    std::mutex stream_mutex_;

    // Configuration
    size_t max_queue_size_;
    size_t batch_size_;
    std::chrono::seconds event_ttl_;

    // Statistics
    std::atomic<size_t> events_published_;
    std::atomic<size_t> events_processed_;
    std::atomic<size_t> events_failed_;
    std::atomic<size_t> events_expired_;
    std::atomic<size_t> events_dead_lettered_;
};

// Pre-built event handlers for common use cases

class LoggingEventHandler : public EventHandler {
public:
    LoggingEventHandler(StructuredLogger* logger, const std::string& handler_id)
        : logger_(logger), handler_id_(handler_id) {}

    void handle_event(std::unique_ptr<Event> event) override {
        logger_->log(LogLevel::INFO, "Event received: " + event->to_string() +
                     " | Payload: " + event->get_payload().dump(2));
    }

    std::vector<EventCategory> get_supported_categories() const override {
        return {}; // Handle all categories
    }

    std::string get_handler_id() const override {
        return handler_id_;
    }

    bool is_active() const override {
        return true;
    }

private:
    StructuredLogger* logger_;
    std::string handler_id_;
};

class MetricsEventHandler : public EventHandler {
public:
    MetricsEventHandler(StructuredLogger* logger, const std::string& handler_id)
        : logger_(logger), handler_id_(handler_id) {}

    void handle_event(std::unique_ptr<Event> event) override {
        if (event->get_category() == EventCategory::SYSTEM_PERFORMANCE_METRIC) {
            // Process performance metrics
            const auto& payload = event->get_payload();
            if (payload.contains("metric_name") && payload.contains("value")) {
                std::string metric_name = payload["metric_name"];
                double value = payload["value"];

                logger_->log(LogLevel::INFO, "Performance metric: " + metric_name +
                             " = " + std::to_string(value));
            }
        }
    }

    std::vector<EventCategory> get_supported_categories() const override {
        return {EventCategory::SYSTEM_PERFORMANCE_METRIC};
    }

    std::string get_handler_id() const override {
        return handler_id_;
    }

    bool is_active() const override {
        return true;
    }

private:
    StructuredLogger* logger_;
    std::string handler_id_;
};

} // namespace regulens
