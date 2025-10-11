/**
 * Security Monitor - Header
 * 
 * Production-grade security monitoring and alerting system.
 * Detects and alerts on suspicious activities, security threats, and compliance violations.
 */

#ifndef REGULENS_SECURITY_MONITOR_HPP
#define REGULENS_SECURITY_MONITOR_HPP

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <memory>
#include <functional>
#include <mutex>

namespace regulens {

// Security event severity
enum class SecuritySeverity {
    LOW,
    MEDIUM,
    HIGH,
    CRITICAL
};

// Security event types
enum class SecurityEventType {
    FAILED_LOGIN,
    BRUTE_FORCE_ATTEMPT,
    SQL_INJECTION_ATTEMPT,
    XSS_ATTEMPT,
    UNAUTHORIZED_ACCESS,
    PRIVILEGE_ESCALATION,
    DATA_EXFILTRATION,
    SUSPICIOUS_API_CALL,
    ANOMALOUS_BEHAVIOR,
    MALFORMED_REQUEST,
    RATE_LIMIT_EXCEEDED,
    INVALID_TOKEN,
    EXPIRED_CERTIFICATE,
    WEAK_CREDENTIALS,
    CONCURRENT_SESSION_VIOLATION
};

// Security event
struct SecurityEvent {
    std::string event_id;
    SecurityEventType type;
    SecuritySeverity severity;
    std::string description;
    std::string source_ip;
    std::string user_id;
    std::string username;
    std::string endpoint;
    std::string user_agent;
    std::chrono::system_clock::time_point timestamp;
    std::map<std::string, std::string> metadata;
    bool alert_sent;
    bool investigated;
    bool false_positive;
};

// Alert rule
struct SecurityAlertRule {
    std::string rule_id;
    std::string name;
    std::string description;
    SecurityEventType event_type;
    SecuritySeverity min_severity;
    int threshold_count;                        // Number of events
    int time_window_minutes;                    // Within time window
    bool enabled;
    std::vector<std::string> notification_channels;  // email, sms, slack, etc.
    std::vector<std::string> recipients;
};

// Security statistics
struct SecurityStats {
    int total_events;
    int critical_events;
    int high_events;
    int medium_events;
    int low_events;
    int alerts_triggered;
    int false_positives;
    std::map<SecurityEventType, int> events_by_type;
    std::chrono::system_clock::time_point period_start;
    std::chrono::system_clock::time_point period_end;
};

// Threat detection result
struct ThreatDetection {
    bool threat_detected;
    SecuritySeverity severity;
    std::string threat_type;
    std::string description;
    double confidence_score;  // 0.0 to 1.0
    std::vector<std::string> indicators;
    std::vector<std::string> recommended_actions;
};

/**
 * Security Monitor
 * 
 * Comprehensive security monitoring and alerting system.
 * Features:
 * - Real-time threat detection
 * - Anomaly detection using ML
 * - Brute force protection
 * - SQL injection detection
 * - XSS attack detection
 * - DDoS detection
 * - Data exfiltration monitoring
 * - Compliance violation detection
 * - Automated incident response
 * - Security analytics and reporting
 */
class SecurityMonitor {
public:
    /**
     * Constructor
     * 
     * @param db_connection Database connection string
     */
    explicit SecurityMonitor(const std::string& db_connection = "");

    ~SecurityMonitor();

    /**
     * Initialize security monitor
     * 
     * @return true if successful
     */
    bool initialize();

    /**
     * Log security event
     * 
     * @param event Security event
     * @return Event ID
     */
    std::string log_event(const SecurityEvent& event);

    /**
     * Detect SQL injection
     * 
     * @param input User input to check
     * @param context Context (query, parameter, etc.)
     * @return Threat detection result
     */
    ThreatDetection detect_sql_injection(
        const std::string& input,
        const std::string& context = ""
    );

    /**
     * Detect XSS attack
     * 
     * @param input User input to check
     * @param context Context
     * @return Threat detection result
     */
    ThreatDetection detect_xss(
        const std::string& input,
        const std::string& context = ""
    );

    /**
     * Detect brute force attack
     * 
     * @param source_ip Source IP address
     * @param username Username
     * @param failed_attempts Number of recent failed attempts
     * @return Threat detection result
     */
    ThreatDetection detect_brute_force(
        const std::string& source_ip,
        const std::string& username,
        int failed_attempts
    );

    /**
     * Detect anomalous behavior
     * Uses machine learning to detect unusual patterns
     * 
     * @param user_id User ID
     * @param activity Activity description
     * @param metadata Activity metadata
     * @return Threat detection result
     */
    ThreatDetection detect_anomalous_behavior(
        const std::string& user_id,
        const std::string& activity,
        const std::map<std::string, std::string>& metadata
    );

    /**
     * Detect data exfiltration
     * 
     * @param user_id User ID
     * @param data_size_bytes Size of data accessed
     * @param endpoint Endpoint accessed
     * @return Threat detection result
     */
    ThreatDetection detect_data_exfiltration(
        const std::string& user_id,
        size_t data_size_bytes,
        const std::string& endpoint
    );

    /**
     * Block IP address
     * 
     * @param ip_address IP to block
     * @param duration_minutes Block duration (0 = permanent)
     * @param reason Block reason
     * @return true if successful
     */
    bool block_ip(
        const std::string& ip_address,
        int duration_minutes = 0,
        const std::string& reason = ""
    );

    /**
     * Unblock IP address
     * 
     * @param ip_address IP to unblock
     * @return true if successful
     */
    bool unblock_ip(const std::string& ip_address);

    /**
     * Check if IP is blocked
     * 
     * @param ip_address IP to check
     * @return true if blocked
     */
    bool is_ip_blocked(const std::string& ip_address);

    /**
     * Add alert rule
     * 
     * @param rule Alert rule
     * @return Rule ID
     */
    std::string add_alert_rule(const SecurityAlertRule& rule);

    /**
     * Remove alert rule
     * 
     * @param rule_id Rule ID
     * @return true if successful
     */
    bool remove_alert_rule(const std::string& rule_id);

    /**
     * Check and trigger alerts
     * Evaluates alert rules against recent events
     * 
     * @return Number of alerts triggered
     */
    int check_and_trigger_alerts();

    /**
     * Send security alert
     * 
     * @param alert_message Alert message
     * @param severity Severity level
     * @param recipients Recipients
     * @param channels Notification channels
     * @return true if alert sent
     */
    bool send_alert(
        const std::string& alert_message,
        SecuritySeverity severity,
        const std::vector<std::string>& recipients,
        const std::vector<std::string>& channels = {"email"}
    );

    /**
     * Get security events
     * 
     * @param time_window_minutes Time window
     * @param severity_filter Optional severity filter
     * @param type_filter Optional type filter
     * @return Vector of security events
     */
    std::vector<SecurityEvent> get_events(
        int time_window_minutes = 60,
        SecuritySeverity severity_filter = SecuritySeverity::LOW,
        SecurityEventType type_filter = SecurityEventType::FAILED_LOGIN
    );

    /**
     * Get security statistics
     * 
     * @param time_window_minutes Time window
     * @return Security statistics
     */
    SecurityStats get_statistics(int time_window_minutes = 60);

    /**
     * Get active threats
     * Returns ongoing security threats
     * 
     * @return Vector of threat descriptions
     */
    std::vector<std::string> get_active_threats();

    /**
     * Mark event as false positive
     * 
     * @param event_id Event ID
     * @param reason Reason for false positive
     * @return true if successful
     */
    bool mark_false_positive(const std::string& event_id, const std::string& reason);

    /**
     * Get security dashboard data
     * 
     * @return JSON dashboard data
     */
    std::string get_dashboard_data();

    /**
     * Generate security report
     * 
     * @param start_time Report start time
     * @param end_time Report end time
     * @return JSON security report
     */
    std::string generate_security_report(
        const std::chrono::system_clock::time_point& start_time,
        const std::chrono::system_clock::time_point& end_time
    );

    /**
     * Export events to SIEM
     * 
     * @param siem_type SIEM type (splunk, elk, etc.)
     * @param siem_config SIEM configuration
     * @return Number of events exported
     */
    int export_to_siem(
        const std::string& siem_type,
        const std::map<std::string, std::string>& siem_config
    );

    /**
     * Perform security scan
     * Scans system for security vulnerabilities
     * 
     * @return Vector of discovered vulnerabilities
     */
    std::vector<std::string> perform_security_scan();

    /**
     * Check compliance
     * Checks for compliance with security standards
     * 
     * @param standard Standard name (SOX, GDPR, PCI-DSS, etc.)
     * @return Compliance report
     */
    std::string check_compliance(const std::string& standard);

private:
    std::string db_connection_;
    bool initialized_;

    std::vector<SecurityEvent> events_;
    std::vector<SecurityAlertRule> alert_rules_;
    std::map<std::string, std::chrono::system_clock::time_point> blocked_ips_;
    std::map<std::string, int> failed_login_counts_;  // IP -> count
    
    std::mutex events_mutex_;
    std::mutex rules_mutex_;
    std::mutex blocks_mutex_;

    /**
     * Generate event ID
     */
    std::string generate_event_id();

    /**
     * Evaluate alert rule
     */
    bool evaluate_alert_rule(const SecurityAlertRule& rule);

    /**
     * Persist event to database
     */
    bool persist_event(const SecurityEvent& event);

    /**
     * Load events from database
     */
    bool load_events(int time_window_minutes);

    /**
     * Check SQL injection patterns
     */
    bool has_sql_injection_pattern(const std::string& input);

    /**
     * Check XSS patterns
     */
    bool has_xss_pattern(const std::string& input);

    /**
     * Calculate threat confidence
     */
    double calculate_threat_confidence(
        const std::vector<std::string>& indicators
    );

    /**
     * Get user behavior baseline
     */
    std::map<std::string, double> get_user_baseline(const std::string& user_id);

    /**
     * Calculate anomaly score
     */
    double calculate_anomaly_score(
        const std::map<std::string, double>& current_behavior,
        const std::map<std::string, double>& baseline
    );

    /**
     * Send email alert
     */
    bool send_email_alert(
        const std::string& recipient,
        const std::string& subject,
        const std::string& body
    );

    /**
     * Send SMS alert
     */
    bool send_sms_alert(
        const std::string& phone_number,
        const std::string& message
    );

    /**
     * Send Slack alert
     */
    bool send_slack_alert(
        const std::string& webhook_url,
        const std::string& message
    );

    /**
     * Clean up old events
     */
    int cleanup_old_events(int retention_days);

    /**
     * Update failed login counter
     */
    void update_failed_login_counter(const std::string& ip_address);

    /**
     * Reset failed login counter
     */
    void reset_failed_login_counter(const std::string& ip_address);
};

} // namespace regulens

#endif // REGULENS_SECURITY_MONITOR_HPP

