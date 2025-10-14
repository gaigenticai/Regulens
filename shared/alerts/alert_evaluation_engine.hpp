#ifndef REGULENS_ALERT_EVALUATION_ENGINE_HPP
#define REGULENS_ALERT_EVALUATION_ENGINE_HPP

#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <functional>
#include <chrono>
#include <nlohmann/json.hpp>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"

namespace regulens {
namespace alerts {

class AlertEvaluationEngine {
public:
    AlertEvaluationEngine(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<StructuredLogger> logger
    );
    
    ~AlertEvaluationEngine();
    
    // Engine control
    void start();
    void stop();
    bool is_running() const;
    
    // Configuration
    void set_evaluation_interval(std::chrono::seconds interval);
    std::chrono::seconds get_evaluation_interval() const;
    
    // Manual evaluation trigger
    void trigger_evaluation();
    
    // Metrics collection
    struct EvaluationMetrics {
        uint64_t total_evaluations = 0;
        uint64_t alerts_triggered = 0;
        uint64_t rules_evaluated = 0;
        uint64_t evaluation_errors = 0;
        std::chrono::milliseconds last_evaluation_duration{0};
        std::chrono::system_clock::time_point last_evaluation_time;
    };
    
    EvaluationMetrics get_metrics() const;
    void reset_metrics();

private:
    // Core evaluation loop
    void evaluation_loop();
    void evaluate_all_rules();
    
    // Rule evaluation methods
    void evaluate_threshold_rule(const nlohmann::json& rule, const std::string& rule_id);
    void evaluate_pattern_rule(const nlohmann::json& rule, const std::string& rule_id);
    void evaluate_anomaly_rule(const nlohmann::json& rule, const std::string& rule_id);
    void evaluate_scheduled_rule(const nlohmann::json& rule, const std::string& rule_id);
    
    // Data collection methods
    nlohmann::json collect_metric_data(const std::string& metric_name);
    nlohmann::json collect_transaction_metrics();
    nlohmann::json collect_system_metrics();
    nlohmann::json collect_compliance_metrics();
    nlohmann::json collect_performance_metrics();
    
    // Condition evaluation
    bool evaluate_condition(const nlohmann::json& condition, const nlohmann::json& current_data);
    bool compare_values(double current_value, const std::string& operator_str, double threshold);
    bool evaluate_pattern_match(const std::string& pattern, const nlohmann::json& data);
    bool evaluate_anomaly_detection(const nlohmann::json& baseline, const nlohmann::json& current);
    
    // Alert creation
    void create_alert_incident(const std::string& rule_id, const nlohmann::json& rule, 
                              const nlohmann::json& incident_data);
    
    // Cooldown management
    bool is_rule_in_cooldown(const std::string& rule_id);
    void update_rule_last_triggered(const std::string& rule_id);
    
    // Baseline data for anomaly detection
    nlohmann::json get_baseline_data(const std::string& metric_name);
    
    // Schedule evaluation
    bool is_schedule_time(const std::string& schedule);
    
    // Notification handling
    void trigger_notifications(const std::string& incident_id, const nlohmann::json& rule);
    
    // Failed notification retry
    void retry_failed_notifications();
    void schedule_notification_retry(const std::string& notification_id, int retry_count);
    
    // Member variables
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<StructuredLogger> logger_;
    
    std::atomic<bool> running_;
    std::atomic<bool> should_trigger_evaluation_;
    std::thread evaluation_thread_;
    
    std::chrono::seconds evaluation_interval_;
    
    mutable std::mutex metrics_mutex_;
    EvaluationMetrics metrics_;
    
    // Configuration
    static constexpr std::chrono::seconds DEFAULT_EVALUATION_INTERVAL{30};
    static constexpr int MAX_RETRY_ATTEMPTS = 3;
    static constexpr std::chrono::seconds COOLDOWN_CHECK_INTERVAL{5};
};

// Alert rule types
enum class AlertRuleType {
    THRESHOLD,
    PATTERN,
    ANOMALY,
    SCHEDULED
};

// Alert severity levels
enum class AlertSeverity {
    LOW,
    MEDIUM,
    HIGH,
    CRITICAL
};

// Alert incident status
enum class AlertStatus {
    ACTIVE,
    ACKNOWLEDGED,
    RESOLVED,
    FALSE_POSITIVE
};

// Utility functions
AlertRuleType parse_rule_type(const std::string& type_str);
AlertSeverity parse_severity(const std::string& severity_str);
AlertStatus parse_status(const std::string& status_str);

std::string rule_type_to_string(AlertRuleType type);
std::string severity_to_string(AlertSeverity severity);
std::string status_to_string(AlertStatus status);

} // namespace alerts
} // namespace regulens

#endif // REGULENS_ALERT_EVALUATION_ENGINE_HPP