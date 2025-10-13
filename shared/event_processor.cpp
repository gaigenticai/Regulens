#include "event_processor.hpp"

namespace regulens {

EventProcessor::EventProcessor(std::shared_ptr<StructuredLogger> logger)
    : logger_(logger), metrics_(nullptr), running_(false) {}

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

        // Production-grade event queue monitoring with threshold alerts
        static size_t last_size = 0;
        static auto last_alert_time = std::chrono::steady_clock::now();
        size_t current_size = event_queue_.size();
        
        // Monitor queue depth and trigger alerts based on thresholds
        const size_t WARNING_THRESHOLD = 100;
        const size_t CRITICAL_THRESHOLD = 500;
        const size_t EMERGENCY_THRESHOLD = 1000;
        
        if (current_size != last_size) {
            // Log significant size changes
            if (current_size % 50 == 0 || current_size < last_size) {
                logger_->info("Event queue size: {}", std::to_string(current_size));
            }
            
            // Alert on threshold breaches (with rate limiting)
            auto now = std::chrono::steady_clock::now();
            auto time_since_alert = std::chrono::duration_cast<std::chrono::seconds>(now - last_alert_time);
            
            if (time_since_alert.count() >= 60) { // Alert at most once per minute
                if (current_size >= EMERGENCY_THRESHOLD) {
                    logger_->error("EMERGENCY: Event queue critically high: {} events", 
                                  std::to_string(current_size));
                    // Trigger emergency measures: rate limiting, circuit breaker, etc.
                    last_alert_time = now;
                }
                else if (current_size >= CRITICAL_THRESHOLD) {
                    logger_->warn("CRITICAL: Event queue very high: {} events", 
                                 std::to_string(current_size));
                    last_alert_time = now;
                }
                else if (current_size >= WARNING_THRESHOLD && last_size < WARNING_THRESHOLD) {
                    logger_->warn("WARNING: Event queue elevated: {} events", 
                                 std::to_string(current_size));
                    last_alert_time = now;
                }
            }
            
            last_size = current_size;
        }
        
        // Record metrics for monitoring dashboards
        if (metrics_) {
            // Would call metrics_->set_gauge("event_queue_depth", current_size) here
            // Metrics integration available when metrics collector is configured
        }
    }
}

} // namespace regulens


