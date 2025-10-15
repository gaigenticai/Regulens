#ifndef REGULENS_NOTIFICATION_SERVICE_HPP
#define REGULENS_NOTIFICATION_SERVICE_HPP

#include <string>
#include <memory>
#include <queue>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <nlohmann/json.hpp>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"

namespace regulens {
namespace alerts {

// Notification request structure
struct NotificationRequest {
    std::string notification_id;
    std::string incident_id;
    std::string channel_id;
    std::string channel_type;
    nlohmann::json channel_config;
    nlohmann::json alert_data;
    int retry_count;
    std::chrono::system_clock::time_point scheduled_time;
};

// Notification result structure
struct NotificationResult {
    std::string notification_id;
    bool success;
    std::string error_message;
    std::chrono::milliseconds delivery_time;
    std::chrono::system_clock::time_point completed_at;
};

// Notification service for handling alert deliveries
class NotificationService {
public:
    NotificationService(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<StructuredLogger> logger
    );
    
    ~NotificationService();
    
    // Service control
    void start();
    void stop();
    bool is_running() const;
    
    // Notification submission
    std::string send_notification(const std::string& incident_id, const std::string& channel_id, 
                                 const nlohmann::json& alert_data);
    void send_notification_async(const std::string& incident_id, const std::string& channel_id, 
                                const nlohmann::json& alert_data);
    
    // Batch operations
    void send_notifications_batch(const std::vector<NotificationRequest>& requests);
    
    // Channel testing
    bool test_channel(const std::string& channel_id, const nlohmann::json& test_data);
    
    // Configuration
    void set_max_concurrent_notifications(int max_concurrent);
    void set_retry_attempts(int max_attempts);
    void set_retry_delay(std::chrono::seconds base_delay);
    
    // Metrics
    struct NotificationMetrics {
        uint64_t total_sent = 0;
        uint64_t successful_deliveries = 0;
        uint64_t failed_deliveries = 0;
        uint64_t retries_attempted = 0;
        std::chrono::milliseconds avg_delivery_time{0};
        std::map<std::string, uint64_t> deliveries_by_channel;
        std::map<std::string, uint64_t> failures_by_channel;
        std::chrono::system_clock::time_point last_notification_time;
    };
    
    NotificationMetrics get_metrics() const;
    void reset_metrics();

private:
    // Worker thread functions
    void notification_worker_loop();
    void retry_worker_loop();
    
    // Notification delivery methods
    bool deliver_notification(const NotificationRequest& request);
    bool send_email_notification(const NotificationRequest& request);
    bool send_webhook_notification(const NotificationRequest& request);
    bool send_slack_notification(const NotificationRequest& request);
    bool send_sms_notification(const NotificationRequest& request);
    bool send_pagerduty_notification(const NotificationRequest& request);
    
    // Database operations
    void log_notification_attempt(const NotificationRequest& request, const NotificationResult& result);
    void update_notification_status(const std::string& notification_id, const std::string& status, 
                                  const std::string& error_message = "");
    std::vector<NotificationRequest> get_failed_notifications_for_retry();
    
    // Retry logic
    void schedule_retry(const NotificationRequest& request, int retry_count);
    std::chrono::seconds calculate_retry_delay(int retry_count) const;
    
    // Queue management
    void enqueue_notification(const NotificationRequest& request);
    bool dequeue_notification(NotificationRequest& request);
    void enqueue_retry(const NotificationRequest& request);
    bool dequeue_retry(NotificationRequest& request);
    
    // Utility methods
    std::string generate_notification_id() const;
    nlohmann::json format_email_payload(const NotificationRequest& request) const;
    nlohmann::json format_webhook_payload(const NotificationRequest& request) const;
    nlohmann::json format_slack_payload(const NotificationRequest& request) const;

    // SMTP email sending
    bool send_smtp_email(const nlohmann::json& email_payload);
    std::string format_sms_message(const NotificationRequest& request) const;
    nlohmann::json format_pagerduty_payload(const NotificationRequest& request) const;
    
    // HTTP helpers
    bool send_http_request(const std::string& url, const nlohmann::json& payload,
                          const std::map<std::string, std::string>& headers = {},
                          std::string* response = nullptr, std::string* error = nullptr);
    
    // Update notification retry time
    void update_notification_retry_time(const std::string& notification_id,
                                       std::chrono::system_clock::time_point retry_time);
    static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp);
    
    // Email helpers
    bool send_smtp_email(const std::string& to, const std::string& subject, 
                        const std::string& body, const nlohmann::json& config);
    
    // Member variables
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<StructuredLogger> logger_;
    
    // Thread management
    std::atomic<bool> running_;
    std::vector<std::thread> worker_threads_;
    std::thread retry_thread_;
    
    // Configuration
    int max_concurrent_notifications_;
    int max_retry_attempts_;
    std::chrono::seconds base_retry_delay_;
    
    // Queues
    std::queue<NotificationRequest> notification_queue_;
    std::queue<NotificationRequest> retry_queue_;
    mutable std::mutex queue_mutex_;
    mutable std::mutex retry_mutex_;
    std::condition_variable queue_cv_;
    std::condition_variable retry_cv_;
    
    // Metrics
    mutable std::mutex metrics_mutex_;
    NotificationMetrics metrics_;
    
    // Constants
    static constexpr int DEFAULT_MAX_CONCURRENT = 5;
    static constexpr int DEFAULT_MAX_RETRY_ATTEMPTS = 3;
    static constexpr std::chrono::seconds DEFAULT_RETRY_DELAY{60};
    static constexpr std::chrono::seconds RETRY_CHECK_INTERVAL{30};
};

// Notification channel types
enum class NotificationChannelType {
    EMAIL,
    WEBHOOK,
    SLACK,
    SMS,
    PAGERDUTY
};

// Notification delivery status
enum class NotificationStatus {
    PENDING,
    SENT,
    DELIVERED,
    FAILED,
    BOUNCED,
    RETRYING
};

// Utility functions
NotificationChannelType parse_channel_type(const std::string& type_str);
std::string channel_type_to_string(NotificationChannelType type);
NotificationStatus parse_notification_status(const std::string& status_str);
std::string notification_status_to_string(NotificationStatus status);

} // namespace alerts
} // namespace regulens

#endif // REGULENS_NOTIFICATION_SERVICE_HPP