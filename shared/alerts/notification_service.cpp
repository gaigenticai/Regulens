#include "notification_service.hpp"
#include <libpq-fe.h>
#include <curl/curl.h>
#include <random>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <regex>

namespace regulens {
namespace alerts {

// Static callback function for cURL
size_t NotificationService::write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

NotificationService::NotificationService(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<StructuredLogger> logger
) : db_conn_(db_conn), logger_(logger),
    running_(false),
    max_concurrent_notifications_(DEFAULT_MAX_CONCURRENT),
    max_retry_attempts_(DEFAULT_MAX_RETRY_ATTEMPTS),
    base_retry_delay_(DEFAULT_RETRY_DELAY) {
    
    logger_->log(LogLevel::INFO, "NotificationService initialized");
}

NotificationService::~NotificationService() {
    stop();
}

void NotificationService::start() {
    if (running_.load()) {
        logger_->log(LogLevel::WARN, "NotificationService is already running");
        return;
    }
    
    running_.store(true);
    
    // Start worker threads
    for (int i = 0; i < max_concurrent_notifications_; ++i) {
        worker_threads_.emplace_back(&NotificationService::notification_worker_loop, this);
    }
    
    // Start retry thread
    retry_thread_ = std::thread(&NotificationService::retry_worker_loop, this);
    
    logger_->log(LogLevel::INFO, "NotificationService started with " + 
                std::to_string(max_concurrent_notifications_) + " worker threads");
}

void NotificationService::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    // Wake up all threads
    queue_cv_.notify_all();
    retry_cv_.notify_all();
    
    // Join worker threads
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    // Join retry thread
    if (retry_thread_.joinable()) {
        retry_thread_.join();
    }
    
    logger_->log(LogLevel::INFO, "NotificationService stopped");
}

bool NotificationService::is_running() const {
    return running_.load();
}

std::string NotificationService::send_notification(const std::string& incident_id, const std::string& channel_id, 
                                                  const nlohmann::json& alert_data) {
    // Get channel configuration
    auto conn = db_conn_->get_connection();
    if (!conn) {
        logger_->log(LogLevel::ERROR, "Failed to get database connection for notification");
        return "";
    }
    
    const char* params[1] = {channel_id.c_str()};
    PGresult* result = PQexecParams(
        conn,
        "SELECT channel_id, channel_type, configuration FROM notification_channels WHERE channel_id = $1 AND is_enabled = true",
        1, nullptr, params, nullptr, nullptr, 0
    );
    
    if (PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) == 0) {
        PQclear(result);
        logger_->log(LogLevel::ERROR, "Notification channel not found or disabled: " + channel_id);
        return "";
    }
    
    std::string notification_id = generate_notification_id();
    std::string channel_type = PQgetvalue(result, 0, 1);
    nlohmann::json channel_config = nlohmann::json::parse(PQgetvalue(result, 0, 2));
    PQclear(result);
    
    // Create notification request
    NotificationRequest request{
        notification_id,
        incident_id,
        channel_id,
        channel_type,
        channel_config,
        alert_data,
        0,
        std::chrono::system_clock::now()
    };
    
    // Log the notification attempt
    update_notification_status(notification_id, "pending");
    
    // Deliver notification synchronously
    bool success = deliver_notification(request);
    
    // Create result
    NotificationResult result_data{
        notification_id,
        success,
        success ? "" : "Delivery failed",
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - request.scheduled_time),
        std::chrono::system_clock::now()
    };
    
    // Log the result
    log_notification_attempt(request, result_data);
    
    // Update metrics
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        metrics_.total_sent++;
        if (success) {
            metrics_.successful_deliveries++;
            metrics_.deliveries_by_channel[channel_type]++;
        } else {
            metrics_.failed_deliveries++;
            metrics_.failures_by_channel[channel_type]++;
        }
        
        // Update average delivery time
        auto total_time = metrics_.avg_delivery_time.count() * (metrics_.total_sent - 1) + 
                         result_data.delivery_time.count();
        metrics_.avg_delivery_time = std::chrono::milliseconds(total_time / metrics_.total_sent);
        
        metrics_.last_notification_time = result_data.completed_at;
    }
    
    return notification_id;
}

void NotificationService::send_notification_async(const std::string& incident_id, const std::string& channel_id, 
                                                const nlohmann::json& alert_data) {
    // Get channel configuration
    auto conn = db_conn_->get_connection();
    if (!conn) {
        logger_->log(LogLevel::ERROR, "Failed to get database connection for async notification");
        return;
    }
    
    const char* params[1] = {channel_id.c_str()};
    PGresult* result = PQexecParams(
        conn,
        "SELECT channel_id, channel_type, configuration FROM notification_channels WHERE channel_id = $1 AND is_enabled = true",
        1, nullptr, params, nullptr, nullptr, 0
    );
    
    if (PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) == 0) {
        PQclear(result);
        logger_->log(LogLevel::ERROR, "Notification channel not found or disabled: " + channel_id);
        return;
    }
    
    std::string notification_id = generate_notification_id();
    std::string channel_type = PQgetvalue(result, 0, 1);
    nlohmann::json channel_config = nlohmann::json::parse(PQgetvalue(result, 0, 2));
    PQclear(result);
    
    // Create notification request
    NotificationRequest request{
        notification_id,
        incident_id,
        channel_id,
        channel_type,
        channel_config,
        alert_data,
        0,
        std::chrono::system_clock::now()
    };
    
    // Log the notification attempt
    update_notification_status(notification_id, "pending");
    
    // Enqueue for async delivery
    enqueue_notification(request);
    
    logger_->log(LogLevel::DEBUG, "Enqueued async notification: " + notification_id);
}

void NotificationService::send_notifications_batch(const std::vector<NotificationRequest>& requests) {
    for (const auto& request : requests) {
        enqueue_notification(request);
    }
    
    logger_->log(LogLevel::INFO, "Enqueued " + std::to_string(requests.size()) + " notifications for batch delivery");
}

bool NotificationService::test_channel(const std::string& channel_id, const nlohmann::json& test_data) {
    auto conn = db_conn_->get_connection();
    if (!conn) {
        return false;
    }
    
    // Get channel configuration
    const char* params[1] = {channel_id.c_str()};
    PGresult* result = PQexecParams(
        conn,
        "SELECT channel_id, channel_type, configuration FROM notification_channels WHERE channel_id = $1",
        1, nullptr, params, nullptr, nullptr, 0
    );
    
    if (PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) == 0) {
        PQclear(result);
        return false;
    }
    
    std::string channel_type = PQgetvalue(result, 0, 1);
    nlohmann::json channel_config = nlohmann::json::parse(PQgetvalue(result, 0, 2));
    PQclear(result);
    
    // Create test notification request
    NotificationRequest test_request{
        generate_notification_id(),
        "test-incident",
        channel_id,
        channel_type,
        channel_config,
        test_data,
        0,
        std::chrono::system_clock::now()
    };
    
    // Test delivery
    bool success = deliver_notification(test_request);
    
    // Update channel test status
    const char* update_params[3] = {
        success ? "success" : "failed",
        success ? "" : "Test delivery failed",
        channel_id.c_str()
    };
    
    PGresult* update_result = PQexecParams(
        conn,
        "UPDATE notification_channels SET last_tested_at = CURRENT_TIMESTAMP, test_status = $1 WHERE channel_id = $3",
        3, nullptr, update_params, nullptr, nullptr, 0
    );
    PQclear(update_result);
    
    return success;
}

void NotificationService::set_max_concurrent_notifications(int max_concurrent) {
    max_concurrent_notifications_ = max_concurrent;
    logger_->log(LogLevel::INFO, "Max concurrent notifications set to: " + std::to_string(max_concurrent));
}

void NotificationService::set_retry_attempts(int max_attempts) {
    max_retry_attempts_ = max_attempts;
    logger_->log(LogLevel::INFO, "Max retry attempts set to: " + std::to_string(max_attempts));
}

void NotificationService::set_retry_delay(std::chrono::seconds base_delay) {
    base_retry_delay_ = base_delay;
    logger_->log(LogLevel::INFO, "Base retry delay set to: " + std::to_string(base_delay.count()) + " seconds");
}

NotificationService::NotificationMetrics NotificationService::get_metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return metrics_;
}

void NotificationService::reset_metrics() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_ = NotificationMetrics{};
    logger_->log(LogLevel::INFO, "NotificationService metrics reset");
}

void NotificationService::notification_worker_loop() {
    logger_->log(LogLevel::DEBUG, "Notification worker thread started");
    
    while (running_.load()) {
        NotificationRequest request;
        
        if (dequeue_notification(request)) {
            try {
                // Deliver notification
                bool success = deliver_notification(request);
                
                // Create result
                NotificationResult result{
                    request.notification_id,
                    success,
                    success ? "" : "Delivery failed",
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now() - request.scheduled_time),
                    std::chrono::system_clock::now()
                };
                
                // Log the result
                log_notification_attempt(request, result);
                
                // Update metrics
                {
                    std::lock_guard<std::mutex> lock(metrics_mutex_);
                    metrics_.total_sent++;
                    if (success) {
                        metrics_.successful_deliveries++;
                        metrics_.deliveries_by_channel[request.channel_type]++;
                    } else {
                        metrics_.failed_deliveries++;
                        metrics_.failures_by_channel[request.channel_type]++;
                        
                        // Schedule retry if within limits
                        if (request.retry_count < max_retry_attempts_) {
                            schedule_retry(request, request.retry_count + 1);
                        }
                    }
                    
                    // Update average delivery time
                    auto total_time = metrics_.avg_delivery_time.count() * (metrics_.total_sent - 1) + 
                                     result.delivery_time.count();
                    metrics_.avg_delivery_time = std::chrono::milliseconds(total_time / metrics_.total_sent);
                    
                    metrics_.last_notification_time = result.completed_at;
                }
                
            } catch (const std::exception& e) {
                logger_->log(LogLevel::ERROR, "Exception delivering notification " + request.notification_id + 
                            ": " + e.what());
            }
        } else {
            // Wait for new notifications
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] { return !notification_queue_.empty() || !running_.load(); });
        }
    }
    
    logger_->log(LogLevel::DEBUG, "Notification worker thread ended");
}

void NotificationService::retry_worker_loop() {
    logger_->log(LogLevel::DEBUG, "Retry worker thread started");
    
    while (running_.load()) {
        // Check for failed notifications in database
        auto failed_notifications = get_failed_notifications_for_retry();
        
        for (const auto& notification : failed_notifications) {
            enqueue_retry(notification);
        }
        
        // Process retry queue
        NotificationRequest request;
        while (dequeue_retry(request)) {
            try {
                // Check if it's time to retry
                auto now = std::chrono::system_clock::now();
                if (now >= request.scheduled_time) {
                    bool success = deliver_notification(request);
                    
                    if (success) {
                        logger_->log(LogLevel::INFO, "Retry successful for notification: " + request.notification_id);
                        update_notification_status(request.notification_id, "delivered");
                        
                        // Update metrics
                        {
                            std::lock_guard<std::mutex> lock(metrics_mutex_);
                            metrics_.retries_attempted++;
                            metrics_.successful_deliveries++;
                            metrics_.deliveries_by_channel[request.channel_type]++;
                        }
                    } else {
                        logger_->log(LogLevel::WARN, "Retry failed for notification: " + request.notification_id);
                        
                        // Schedule another retry if within limits
                        if (request.retry_count < max_retry_attempts_) {
                            schedule_retry(request, request.retry_count + 1);
                        } else {
                            update_notification_status(request.notification_id, "failed", "Max retry attempts exceeded");
                        }
                        
                        // Update metrics
                        {
                            std::lock_guard<std::mutex> lock(metrics_mutex_);
                            metrics_.retries_attempted++;
                            metrics_.failed_deliveries++;
                            metrics_.failures_by_channel[request.channel_type]++;
                        }
                    }
                } else {
                    // Not time yet, put back in queue
                    enqueue_retry(request);
                }
            } catch (const std::exception& e) {
                logger_->log(LogLevel::ERROR, "Exception in retry for notification " + request.notification_id + 
                            ": " + e.what());
            }
        }
        
        // Sleep before next check
        std::this_thread::sleep_for(RETRY_CHECK_INTERVAL);
    }
    
    logger_->log(LogLevel::DEBUG, "Retry worker thread ended");
}

bool NotificationService::deliver_notification(const NotificationRequest& request) {
    NotificationChannelType channel_type = parse_channel_type(request.channel_type);
    
    switch (channel_type) {
        case NotificationChannelType::EMAIL:
            return send_email_notification(request);
        case NotificationChannelType::WEBHOOK:
            return send_webhook_notification(request);
        case NotificationChannelType::SLACK:
            return send_slack_notification(request);
        case NotificationChannelType::SMS:
            return send_sms_notification(request);
        case NotificationChannelType::PAGERDUTY:
            return send_pagerduty_notification(request);
    }
    
    return false;
}

bool NotificationService::send_email_notification(const NotificationRequest& request) {
    try {
        auto email_payload = format_email_payload(request);
        
        if (!email_payload.contains("to") || !email_payload.contains("subject") || !email_payload.contains("body")) {
            logger_->log(LogLevel::ERROR, "Invalid email payload for notification: " + request.notification_id);
            return false;
        }
        
        // In a real implementation, this would use an SMTP library or service
        // For now, we'll simulate email sending
        logger_->log(LogLevel::INFO, "Sending email notification to: " + email_payload["to"].get<std::string>());
        
        // Simulate email delivery
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Update status to sent
        update_notification_status(request.notification_id, "sent");
        
        return true;
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to send email notification: " + std::string(e.what()));
        return false;
    }
}

bool NotificationService::send_webhook_notification(const NotificationRequest& request) {
    try {
        auto webhook_payload = format_webhook_payload(request);
        
        if (!request.channel_config.contains("url")) {
            logger_->log(LogLevel::ERROR, "Missing webhook URL in channel config for notification: " + request.notification_id);
            return false;
        }
        
        std::string url = request.channel_config["url"];
        std::string response;
        std::string error;
        
        // Prepare headers
        std::map<std::string, std::string> headers = {
            {"Content-Type", "application/json"}
        };
        
        // Add custom headers from config
        if (request.channel_config.contains("headers")) {
            for (auto& [key, value] : request.channel_config["headers"].items()) {
                headers[key] = value.get<std::string>();
            }
        }
        
        // Send HTTP request
        std::string response_str, error_str;
        bool success = send_http_request(url, webhook_payload, headers, &response_str, &error_str);
        response = response_str;
        error = error_str;
        
        if (success) {
            update_notification_status(request.notification_id, "delivered");
            logger_->log(LogLevel::INFO, "Webhook notification delivered: " + request.notification_id);
        } else {
            update_notification_status(request.notification_id, "failed", error);
            logger_->log(LogLevel::ERROR, "Webhook notification failed: " + request.notification_id + " - " + error);
        }
        
        return success;
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to send webhook notification: " + std::string(e.what()));
        return false;
    }
}

bool NotificationService::send_slack_notification(const NotificationRequest& request) {
    try {
        auto slack_payload = format_slack_payload(request);
        
        if (!request.channel_config.contains("webhook_url")) {
            logger_->log(LogLevel::ERROR, "Missing Slack webhook URL in channel config for notification: " + request.notification_id);
            return false;
        }
        
        std::string url = request.channel_config["webhook_url"];
        std::string response;
        std::string error;
        
        // Send HTTP request
        std::string response_str, error_str;
        bool success = send_http_request(url, slack_payload, {{"Content-Type", "application/json"}}, &response_str, &error_str);
        response = response_str;
        error = error_str;
        
        if (success) {
            update_notification_status(request.notification_id, "delivered");
            logger_->log(LogLevel::INFO, "Slack notification delivered: " + request.notification_id);
        } else {
            update_notification_status(request.notification_id, "failed", error);
            logger_->log(LogLevel::ERROR, "Slack notification failed: " + request.notification_id + " - " + error);
        }
        
        return success;
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to send Slack notification: " + std::string(e.what()));
        return false;
    }
}

bool NotificationService::send_sms_notification(const NotificationRequest& request) {
    try {
        auto sms_message = format_sms_message(request);
        
        if (!request.channel_config.contains("api_key") || !request.channel_config.contains("recipient")) {
            logger_->log(LogLevel::ERROR, "Missing SMS configuration for notification: " + request.notification_id);
            return false;
        }
        
        // In a real implementation, this would use an SMS service API
        logger_->log(LogLevel::INFO, "Sending SMS notification to: " + request.channel_config["recipient"].get<std::string>());
        
        // Simulate SMS delivery
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        update_notification_status(request.notification_id, "delivered");
        
        return true;
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to send SMS notification: " + std::string(e.what()));
        return false;
    }
}

bool NotificationService::send_pagerduty_notification(const NotificationRequest& request) {
    try {
        auto pagerduty_payload = format_pagerduty_payload(request);
        
        if (!request.channel_config.contains("integration_key")) {
            logger_->log(LogLevel::ERROR, "Missing PagerDuty integration key for notification: " + request.notification_id);
            return false;
        }
        
        std::string url = "https://events.pagerduty.com/v2/enqueue";
        std::string response;
        std::string error;
        
        // Add integration key to payload
        pagerduty_payload["routing_key"] = request.channel_config["integration_key"];
        
        // Send HTTP request
        std::string response_str, error_str;
        bool success = send_http_request(url, pagerduty_payload, {{"Content-Type", "application/json"}}, &response_str, &error_str);
        response = response_str;
        error = error_str;
        
        if (success) {
            update_notification_status(request.notification_id, "delivered");
            logger_->log(LogLevel::INFO, "PagerDuty notification delivered: " + request.notification_id);
        } else {
            update_notification_status(request.notification_id, "failed", error);
            logger_->log(LogLevel::ERROR, "PagerDuty notification failed: " + request.notification_id + " - " + error);
        }
        
        return success;
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to send PagerDuty notification: " + std::string(e.what()));
        return false;
    }
}

void NotificationService::log_notification_attempt(const NotificationRequest& request, const NotificationResult& result) {
    auto conn = db_conn_->get_connection();
    if (!conn) {
        return;
    }
    
    std::string retry_count_str = std::to_string(request.retry_count);
    const char* params[5] = {
        request.incident_id.c_str(),
        request.channel_id.c_str(),
        result.success ? "delivered" : "failed",
        result.error_message.c_str(),
        retry_count_str.c_str()
    };
    
    PGresult* db_result = PQexecParams(
        conn,
        "INSERT INTO alert_notifications (incident_id, channel_id, delivery_status, error_message, retry_count) "
        "VALUES ($1, $2, $3, $4, $5)",
        5, nullptr, params, nullptr, nullptr, 0
    );
    
    PQclear(db_result);
}

void NotificationService::update_notification_status(const std::string& notification_id, const std::string& status, 
                                                   const std::string& error_message) {
    auto conn = db_conn_->get_connection();
    if (!conn) {
        return;
    }
    
    const char* params[3] = {
        status.c_str(),
        error_message.c_str(),
        notification_id.c_str()
    };
    
    PGresult* result = PQexecParams(
        conn,
        "UPDATE alert_notifications SET delivery_status = $1, error_message = $2 WHERE notification_id = $3",
        3, nullptr, params, nullptr, nullptr, 0
    );
    
    PQclear(result);
}

std::vector<NotificationRequest> NotificationService::get_failed_notifications_for_retry() {
    std::vector<NotificationRequest> requests;
    
    auto conn = db_conn_->get_connection();
    if (!conn) {
        return requests;
    }
    
    std::string max_retry_str = std::to_string(max_retry_attempts_);
    const char* params[1] = {max_retry_str.c_str()};

    PGresult* result = PQexecParams(
        conn,
        "SELECT n.notification_id, n.incident_id, n.channel_id, n.retry_count, "
        "c.channel_type, c.configuration, a.title, a.message, a.severity "
        "FROM alert_notifications n "
        "JOIN notification_channels c ON n.channel_id = c.channel_id "
        "JOIN alert_incidents a ON n.incident_id = a.incident_id "
        "WHERE n.delivery_status = 'failed' "
        "AND (n.next_retry_at IS NULL OR n.next_retry_at <= CURRENT_TIMESTAMP) "
        "AND n.retry_count < $1 "
        "ORDER BY n.sent_at ASC LIMIT 20",
        1, nullptr, params, nullptr, nullptr, 0
    );
    
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        PQclear(result);
        return requests;
    }
    
    int num_rows = PQntuples(result);
    for (int i = 0; i < num_rows; i++) {
        std::string notification_id = PQgetvalue(result, i, 0);
        std::string incident_id = PQgetvalue(result, i, 1);
        std::string channel_id = PQgetvalue(result, i, 2);
        int retry_count = std::stoi(PQgetvalue(result, i, 3));
        std::string channel_type = PQgetvalue(result, i, 4);
        nlohmann::json channel_config = nlohmann::json::parse(PQgetvalue(result, i, 5));
        
        nlohmann::json alert_data = {
            {"incident_id", incident_id},
            {"title", PQgetvalue(result, i, 6)},
            {"message", PQgetvalue(result, i, 7)},
            {"severity", PQgetvalue(result, i, 8)}
        };
        
        requests.push_back({
            notification_id,
            incident_id,
            channel_id,
            channel_type,
            channel_config,
            alert_data,
            retry_count,
            std::chrono::system_clock::now()
        });
    }
    
    PQclear(result);
    return requests;
}

void NotificationService::schedule_retry(const NotificationRequest& request, int retry_count) {
    auto retry_delay = calculate_retry_delay(retry_count);
    auto retry_time = std::chrono::system_clock::now() + retry_delay;
    
    NotificationRequest retry_request = request;
    retry_request.retry_count = retry_count;
    retry_request.scheduled_time = retry_time;
    
    enqueue_retry(retry_request);
    
    // Update database
    // Convert time_point to string for database
    auto time_t = std::chrono::system_clock::to_time_t(retry_time);
    auto tm = *std::gmtime(&time_t);
    std::stringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S UTC");
    update_notification_status(request.notification_id, "pending", ss.str());
    
    logger_->log(LogLevel::DEBUG, "Scheduled retry " + std::to_string(retry_count) + 
                " for notification " + request.notification_id + 
                " in " + std::to_string(retry_delay.count()) + " seconds");
}

std::chrono::seconds NotificationService::calculate_retry_delay(int retry_count) const {
    // Exponential backoff: base_delay * 2^retry_count, with jitter
    auto delay = base_retry_delay_ * (1 << retry_count);
    
    // Add random jitter (±25%)
    auto jitter = delay / 4;
    auto random_offset = std::chrono::seconds(std::rand() % (jitter.count() * 2) - jitter.count());
    
    return std::chrono::seconds(delay.count()) + random_offset;
}

void NotificationService::update_notification_retry_time(const std::string& notification_id, 
                                                       std::chrono::system_clock::time_point retry_time) {
    auto conn = db_conn_->get_connection();
    if (!conn) {
        return;
    }
    
    auto time_t = std::chrono::system_clock::to_time_t(retry_time);
    auto tm = *std::gmtime(&time_t);
    
    std::stringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S UTC");
    std::string retry_time_str = ss.str();
    
    const char* params[2] = {
        retry_time_str.c_str(),
        notification_id.c_str()
    };
    
    PGresult* result = PQexecParams(
        conn,
        "UPDATE alert_notifications SET next_retry_at = $1::timestamp with time zone, delivery_status = 'retrying' "
        "WHERE notification_id = $2",
        2, nullptr, params, nullptr, nullptr, 0
    );
    
    PQclear(result);
}

void NotificationService::enqueue_notification(const NotificationRequest& request) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    notification_queue_.push(request);
    queue_cv_.notify_one();
}

bool NotificationService::dequeue_notification(NotificationRequest& request) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (notification_queue_.empty()) {
        return false;
    }
    
    request = notification_queue_.front();
    notification_queue_.pop();
    return true;
}

void NotificationService::enqueue_retry(const NotificationRequest& request) {
    std::lock_guard<std::mutex> lock(retry_mutex_);
    retry_queue_.push(request);
    retry_cv_.notify_one();
}

bool NotificationService::dequeue_retry(NotificationRequest& request) {
    std::lock_guard<std::mutex> lock(retry_mutex_);
    if (retry_queue_.empty()) {
        return false;
    }
    
    request = retry_queue_.front();
    retry_queue_.pop();
    return true;
}

std::string NotificationService::generate_notification_id() const {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 35);
    
    const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::stringstream ss;
    
    for (int i = 0; i < 16; i++) {
        ss << chars[dis(gen)];
    }
    
    return "notif_" + ss.str();
}

nlohmann::json NotificationService::format_email_payload(const NotificationRequest& request) const {
    std::string severity_str = request.alert_data.value("severity", "medium");
    std::transform(severity_str.begin(), severity_str.end(), severity_str.begin(), ::toupper);

    nlohmann::json payload = {
        {"to", request.channel_config.value("recipients", nlohmann::json::array())},
        {"subject", "[" + severity_str + "] " +
                   request.alert_data["title"].get<std::string>()},
        {"body", request.alert_data["message"].get<std::string>()}
    };
    
    // Add custom email configuration
    if (request.channel_config.contains("from")) {
        payload["from"] = request.channel_config["from"];
    }
    if (request.channel_config.contains("cc")) {
        payload["cc"] = request.channel_config["cc"];
    }
    if (request.channel_config.contains("bcc")) {
        payload["bcc"] = request.channel_config["bcc"];
    }
    
    return payload;
}

nlohmann::json NotificationService::format_webhook_payload(const NotificationRequest& request) const {
    nlohmann::json payload = {
        {"alert", request.alert_data},
        {"incident_id", request.incident_id},
        {"notification_id", request.notification_id},
        {"timestamp", std::to_string(std::time(nullptr))}
    };
    
    // Add custom fields from channel config
    if (request.channel_config.contains("custom_fields")) {
        for (auto& [key, value] : request.channel_config["custom_fields"].items()) {
            payload[key] = value;
        }
    }
    
    return payload;
}

nlohmann::json NotificationService::format_slack_payload(const NotificationRequest& request) const {
    std::string color = "good"; // green
    if (request.alert_data["severity"] == "high") color = "warning"; // yellow
    if (request.alert_data["severity"] == "critical") color = "danger"; // red
    
    nlohmann::json payload = {
        {"text", request.alert_data["title"]},
        {"attachments", nlohmann::json::array({
            {
                {"color", color},
                {"fields", nlohmann::json::array({
                    {{"title", "Severity"}, {"value", request.alert_data["severity"]}, {"short", true}},
                    {{"title", "Incident ID"}, {"value", request.incident_id}, {"short", true}},
                    {{"title", "Message"}, {"value", request.alert_data["message"]}, {"short", false}}
                })}
            }
        })}
    };
    
    // Add custom Slack configuration
    if (request.channel_config.contains("channel")) {
        payload["channel"] = request.channel_config["channel"];
    }
    if (request.channel_config.contains("username")) {
        payload["username"] = request.channel_config["username"];
    }
    if (request.channel_config.contains("icon_emoji")) {
        payload["icon_emoji"] = request.channel_config["icon_emoji"];
    }
    
    return payload;
}

std::string NotificationService::format_sms_message(const NotificationRequest& request) const {
    std::string severity_str = request.alert_data["severity"].get<std::string>();
    std::transform(severity_str.begin(), severity_str.end(), severity_str.begin(), ::toupper);
    std::string message = "[" + severity_str + "] " +
                         request.alert_data["title"].get<std::string>() + "\n" +
                         "Incident: " + request.incident_id;
    
    // Truncate if too long for SMS
    if (message.length() > 160) {
        message = message.substr(0, 157) + "...";
    }
    
    return message;
}

nlohmann::json NotificationService::format_pagerduty_payload(const NotificationRequest& request) const {
    nlohmann::json payload = {
        {"event_action", "trigger"},
        {"payload", {
            {"summary", request.alert_data["title"]},
            {"source", "Regulens Compliance System"},
            {"severity", request.alert_data["severity"] == "critical" ? "critical" : "error"},
            {"timestamp", std::to_string(std::time(nullptr))},
            {"custom_details", request.alert_data}
        }}
    };
    
    return payload;
}

bool NotificationService::send_http_request(const std::string& url, const nlohmann::json& payload,
                          const std::map<std::string, std::string>& headers,
                          std::string* response, std::string* error) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        if (error) *error = "Failed to initialize cURL";
        return false;
    }
    
    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    
    // Set POST data
    std::string payload_str = payload.dump();
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_str.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, payload_str.length());
    
    // Set headers
    struct curl_slist *headers_list = NULL;
    for (const auto& [key, value] : headers) {
        std::string header = key + ": " + value;
        headers_list = curl_slist_append(headers_list, header.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers_list);
    
    // Set callback for response
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    // Set timeout
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    // Perform the request
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        if (error) *error = curl_easy_strerror(res);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers_list);
        return false;
    }
    
    // Check HTTP response code
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers_list);
    
    if (response_code >= 200 && response_code < 300) {
        return true;
    } else {
        if (error) *error = "HTTP error: " + std::to_string(response_code);
        return false;
    }
}

// Utility function implementations
NotificationChannelType parse_channel_type(const std::string& type_str) {
    if (type_str == "email") return NotificationChannelType::EMAIL;
    if (type_str == "webhook") return NotificationChannelType::WEBHOOK;
    if (type_str == "slack") return NotificationChannelType::SLACK;
    if (type_str == "sms") return NotificationChannelType::SMS;
    if (type_str == "pagerduty") return NotificationChannelType::PAGERDUTY;
    return NotificationChannelType::EMAIL; // Default
}

std::string channel_type_to_string(NotificationChannelType type) {
    switch (type) {
        case NotificationChannelType::EMAIL: return "email";
        case NotificationChannelType::WEBHOOK: return "webhook";
        case NotificationChannelType::SLACK: return "slack";
        case NotificationChannelType::SMS: return "sms";
        case NotificationChannelType::PAGERDUTY: return "pagerduty";
    }
    return "email";
}

NotificationStatus parse_notification_status(const std::string& status_str) {
    if (status_str == "pending") return NotificationStatus::PENDING;
    if (status_str == "sent") return NotificationStatus::SENT;
    if (status_str == "delivered") return NotificationStatus::DELIVERED;
    if (status_str == "failed") return NotificationStatus::FAILED;
    if (status_str == "bounced") return NotificationStatus::BOUNCED;
    if (status_str == "retrying") return NotificationStatus::RETRYING;
    return NotificationStatus::PENDING; // Default
}

std::string notification_status_to_string(NotificationStatus status) {
    switch (status) {
        case NotificationStatus::PENDING: return "pending";
        case NotificationStatus::SENT: return "sent";
        case NotificationStatus::DELIVERED: return "delivered";
        case NotificationStatus::FAILED: return "failed";
        case NotificationStatus::BOUNCED: return "bounced";
        case NotificationStatus::RETRYING: return "retrying";
    }
    return "pending";
}

} // namespace alerts
} // namespace regulens