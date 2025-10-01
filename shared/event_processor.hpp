#pragma once

#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include "shared/models/compliance_event.hpp"
#include "shared/logging/structured_logger.hpp"

namespace regulens {

/**
 * @brief Event processing system for handling compliance events
 */
class EventProcessor {
public:
    EventProcessor(std::shared_ptr<StructuredLogger> logger);
    ~EventProcessor();

    bool initialize();
    void shutdown();

    void enqueue_event(const ComplianceEvent& event);
    std::optional<ComplianceEvent> dequeue_event();
    size_t queue_size() const;

private:
    void processing_thread();

    std::shared_ptr<StructuredLogger> logger_;
    std::queue<ComplianceEvent> event_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::thread processor_thread_;
    std::atomic<bool> running_;
};

} // namespace regulens


