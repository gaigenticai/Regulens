#include "event_processor.hpp"

namespace regulens {

EventProcessor::EventProcessor(std::shared_ptr<StructuredLogger> logger)
    : logger_(logger), running_(false) {}

EventProcessor::~EventProcessor() {
    shutdown();
}

bool EventProcessor::initialize() {
    running_ = true;
    processor_thread_ = std::thread(&EventProcessor::processing_thread, this);
    logger_->info("Event processor initialized");
    return true;
}

void EventProcessor::shutdown() {
    running_ = false;
    queue_cv_.notify_all();

    if (processor_thread_.joinable()) {
        processor_thread_.join();
    }

    logger_->info("Event processor shutdown");
}

void EventProcessor::enqueue_event(const ComplianceEvent& event) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        event_queue_.push(event);
    }
    queue_cv_.notify_one();
}

std::optional<ComplianceEvent> EventProcessor::dequeue_event() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (event_queue_.empty()) {
        return std::nullopt;
    }

    ComplianceEvent event = event_queue_.front();
    event_queue_.pop();
    return event;
}

size_t EventProcessor::queue_size() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return event_queue_.size();
}

void EventProcessor::processing_thread() {
    while (running_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_cv_.wait(lock, [this]() { return !running_ || !event_queue_.empty(); });

        if (!running_) break;

        // Process events here if needed
        // For now, just log queue size periodically
        static size_t last_size = 0;
        size_t current_size = event_queue_.size();
        if (current_size != last_size && current_size % 10 == 0) {
            logger_->info("Event queue size: {}", current_size);
            last_size = current_size;
        }
    }
}

} // namespace regulens


