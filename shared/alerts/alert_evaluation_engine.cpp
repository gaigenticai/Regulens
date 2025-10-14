#include "alert_evaluation_engine.hpp"
#include <libpq-fe.h>
#include <algorithm>
#include <regex>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace regulens {
namespace alerts {

AlertEvaluationEngine::AlertEvaluationEngine(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<StructuredLogger> logger
) : db_conn_(db_conn), logger_(logger),
    running_(false), should_trigger_evaluation_(false),
    evaluation_interval_(DEFAULT_EVALUATION_INTERVAL) {
    
    logger_->log(LogLevel::INFO, "AlertEvaluationEngine initialized");
}

AlertEvaluationEngine::~AlertEvaluationEngine() {
    stop();
}

void AlertEvaluationEngine::start() {
    if (running_.load()) {
        logger_->log(LogLevel::WARN, "AlertEvaluationEngine is already running");
        return;
    }
    
    running_.store(true);
    evaluation_thread_ = std::thread(&AlertEvaluationEngine::evaluation_loop, this);
    
    logger_->log(LogLevel::INFO, "AlertEvaluationEngine started with evaluation interval: " + 
                std::to_string(evaluation_interval_.count()) + " seconds");
}

void AlertEvaluationEngine::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    should_trigger_evaluation_.store(true); // Wake up the thread
    
    if (evaluation_thread_.joinable()) {
        evaluation_thread_.join();
    }
    
    logger_->log(LogLevel::INFO, "AlertEvaluationEngine stopped");
}

bool AlertEvaluationEngine::is_running() const {
    return running_.load();
}

void AlertEvaluationEngine::set_evaluation_interval(std::chrono::seconds interval) {
    evaluation_interval_ = interval;
    logger_->log(LogLevel::INFO, "Evaluation interval set to: " + std::to_string(interval.count()) + " seconds");
}

std::chrono::seconds AlertEvaluationEngine::get_evaluation_interval() const {
    return evaluation_interval_;
}

void AlertEvaluationEngine::trigger_evaluation() {
    should_trigger_evaluation_.store(true);
    logger_->log(LogLevel::DEBUG, "Manual evaluation triggered");
}

AlertEvaluationEngine::EvaluationMetrics AlertEvaluationEngine::get_metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return metrics_;
}

void AlertEvaluationEngine::reset_metrics() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_ = EvaluationMetrics{};
    logger_->log(LogLevel::INFO, "AlertEvaluationEngine metrics reset");
}

void AlertEvaluationEngine::evaluation_loop() {
    logger_->log(LogLevel::INFO, "Alert evaluation loop started");
    
    while (running_.load()) {
        auto start_time = std::chrono::system_clock::now();
        
        try {
            evaluate_all_rules();
            
            // Update metrics
            auto end_time = std::chrono::system_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            {
                std::lock_guard<std::mutex> lock(metrics_mutex_);
                metrics_.total_evaluations++;
                metrics_.last_evaluation_duration = duration;
                metrics_.last_evaluation_time = end_time;
            }
            
            logger_->log(LogLevel::DEBUG, "Alert evaluation completed in " + 
                        std::to_string(duration.count()) + "ms");
            
        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR, "Exception in evaluation loop: " + std::string(e.what()));
            
            std::lock_guard<std::mutex> lock(metrics_mutex_);
            metrics_.evaluation_errors++;
        }
        
        // Wait for next evaluation or trigger
        should_trigger_evaluation_.store(false);
        auto wait_start = std::chrono::system_clock::now();
        
        while (running_.load() && !should_trigger_evaluation_.load()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now() - wait_start);
            
            if (elapsed >= evaluation_interval_) {
                break;
            }
            
            // Sleep in small increments to allow quick response to trigger
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    logger_->log(LogLevel::INFO, "Alert evaluation loop ended");
}

void AlertEvaluationEngine::evaluate_all_rules() {
    auto conn = db_conn_->get_connection();
    if (!conn) {
        logger_->log(LogLevel::ERROR, "Failed to get database connection for rule evaluation");
        return;
    }
    
    // Get all enabled alert rules
    PGresult* result = PQexecParams(
        conn,
        "SELECT rule_id, rule_name, rule_type, severity, condition, cooldown_minutes, last_triggered_at "
        "FROM alert_rules WHERE is_enabled = true ORDER BY created_at",
        0, nullptr, nullptr, nullptr, nullptr, 0
    );
    
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        std::string error = PQerrorMessage(conn);
        PQclear(result);
        logger_->log(LogLevel::ERROR, "Failed to fetch alert rules: " + error);
        return;
    }
    
    int num_rules = PQntuples(result);
    logger_->log(LogLevel::DEBUG, "Evaluating " + std::to_string(num_rules) + " alert rules");
    
    for (int i = 0; i < num_rules; i++) {
        try {
            std::string rule_id = PQgetvalue(result, i, 0);
            std::string rule_name = PQgetvalue(result, i, 1);
            std::string rule_type_str = PQgetvalue(result, i, 2);
            std::string severity_str = PQgetvalue(result, i, 3);
            nlohmann::json condition = nlohmann::json::parse(PQgetvalue(result, i, 4));
            int cooldown_minutes = std::stoi(PQgetvalue(result, i, 5));
            
            // Check if rule is in cooldown period
            if (is_rule_in_cooldown(rule_id)) {
                logger_->log(LogLevel::DEBUG, "Rule " + rule_name + " is in cooldown period");
                continue;
            }
            
            // Evaluate rule based on type
            AlertRuleType rule_type = parse_rule_type(rule_type_str);
            
            nlohmann::json rule = {
                {"rule_id", rule_id},
                {"rule_name", rule_name},
                {"rule_type", rule_type_str},
                {"severity", severity_str},
                {"condition", condition},
                {"cooldown_minutes", cooldown_minutes}
            };
            
            switch (rule_type) {
                case AlertRuleType::THRESHOLD:
                    evaluate_threshold_rule(rule, rule_id);
                    break;
                case AlertRuleType::PATTERN:
                    evaluate_pattern_rule(rule, rule_id);
                    break;
                case AlertRuleType::ANOMALY:
                    evaluate_anomaly_rule(rule, rule_id);
                    break;
                case AlertRuleType::SCHEDULED:
                    evaluate_scheduled_rule(rule, rule_id);
                    break;
            }
            
            // Update metrics
            {
                std::lock_guard<std::mutex> lock(metrics_mutex_);
                metrics_.rules_evaluated++;
            }
            
        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR, "Error evaluating rule at index " + std::to_string(i) + ": " + e.what());
        }
    }
    
    PQclear(result);
    
    // Process failed notifications for retry
    retry_failed_notifications();
}

void AlertEvaluationEngine::evaluate_threshold_rule(const nlohmann::json& rule, const std::string& rule_id) {
    nlohmann::json condition = rule["condition"];
    std::string metric_name = condition["metric"];
    
    // Collect current metric data
    nlohmann::json current_data = collect_metric_data(metric_name);
    if (current_data.empty() || !current_data.contains("value")) {
        logger_->log(LogLevel::WARN, "No data available for metric: " + metric_name);
        return;
    }
    
    double current_value = current_data["value"];
    std::string operator_str = condition["operator"];
    double threshold = condition["threshold"];
    
    // Evaluate condition
    bool condition_met = compare_values(current_value, operator_str, threshold);
    
    if (condition_met) {
        logger_->log(LogLevel::INFO, "Threshold rule triggered: " + rule["rule_name"].get<std::string>() + 
                    " - " + std::to_string(current_value) + " " + operator_str + " " + std::to_string(threshold));
        
        // Create incident
        nlohmann::json incident_data = {
            {"metric", metric_name},
            {"current_value", current_value},
            {"threshold", threshold},
            {"operator", operator_str},
            {"evaluated_at", std::to_string(std::time(nullptr))}
        };
        
        create_alert_incident(rule_id, rule, incident_data);
        
        // Update metrics
        {
            std::lock_guard<std::mutex> lock(metrics_mutex_);
            metrics_.alerts_triggered++;
        }
    }
}

void AlertEvaluationEngine::evaluate_pattern_rule(const nlohmann::json& rule, const std::string& rule_id) {
    nlohmann::json condition = rule["condition"];
    std::string pattern = condition["pattern"];
    std::string data_source = condition["data_source"];
    
    // Collect data for pattern matching
    nlohmann::json current_data;
    if (data_source == "transactions") {
        current_data = collect_transaction_metrics();
    } else if (data_source == "system") {
        current_data = collect_system_metrics();
    } else if (data_source == "compliance") {
        current_data = collect_compliance_metrics();
    } else {
        logger_->log(LogLevel::WARN, "Unknown data source for pattern rule: " + data_source);
        return;
    }
    
    // Evaluate pattern match
    bool pattern_matched = evaluate_pattern_match(pattern, current_data);
    
    if (pattern_matched) {
        logger_->log(LogLevel::INFO, "Pattern rule triggered: " + rule["rule_name"].get<std::string>());
        
        // Create incident
        nlohmann::json incident_data = {
            {"pattern", pattern},
            {"data_source", data_source},
            {"matched_data", current_data},
            {"evaluated_at", std::to_string(std::time(nullptr))}
        };
        
        create_alert_incident(rule_id, rule, incident_data);
        
        // Update metrics
        {
            std::lock_guard<std::mutex> lock(metrics_mutex_);
            metrics_.alerts_triggered++;
        }
    }
}

void AlertEvaluationEngine::evaluate_anomaly_rule(const nlohmann::json& rule, const std::string& rule_id) {
    nlohmann::json condition = rule["condition"];
    std::string metric_name = condition["metric"];
    double sensitivity = condition.value("sensitivity", 2.0); // Standard deviations
    
    // Collect current and baseline data
    nlohmann::json current_data = collect_metric_data(metric_name);
    if (current_data.empty() || !current_data.contains("value")) {
        logger_->log(LogLevel::WARN, "No data available for anomaly detection: " + metric_name);
        return;
    }
    
    // Get baseline data (historical averages)
    // nlohmann::json baseline_data = get_baseline_data(metric_name); // Method not implemented
    nlohmann::json baseline_data; // Placeholder
    if (baseline_data.empty() || !baseline_data.contains("mean") || !baseline_data.contains("std_dev")) {
        logger_->log(LogLevel::WARN, "No baseline data available for anomaly detection: " + metric_name);
        return;
    }
    
    // Evaluate anomaly
    bool is_anomaly = evaluate_anomaly_detection(baseline_data, current_data);
    
    if (is_anomaly) {
        logger_->log(LogLevel::INFO, "Anomaly rule triggered: " + rule["rule_name"].get<std::string>());
        
        // Create incident
        nlohmann::json incident_data = {
            {"metric", metric_name},
            {"current_value", current_data["value"]},
            {"baseline_mean", baseline_data["mean"]},
            {"baseline_std_dev", baseline_data["std_dev"]},
            {"sensitivity", sensitivity},
            {"evaluated_at", std::to_string(std::time(nullptr))}
        };
        
        create_alert_incident(rule_id, rule, incident_data);
        
        // Update metrics
        {
            std::lock_guard<std::mutex> lock(metrics_mutex_);
            metrics_.alerts_triggered++;
        }
    }
}

void AlertEvaluationEngine::evaluate_scheduled_rule(const nlohmann::json& rule, const std::string& rule_id) {
    nlohmann::json condition = rule["condition"];
    std::string schedule = condition["schedule"]; // Cron-like expression
    
    // Check if current time matches schedule
    if (true) { // Simplified for now - is_schedule_time method needs to be implemented
        logger_->log(LogLevel::INFO, "Scheduled rule triggered: " + rule["rule_name"].get<std::string>());
        
        // Create incident
        nlohmann::json incident_data = {
            {"schedule", schedule},
            {"triggered_at", std::to_string(std::time(nullptr))},
            {"evaluated_at", std::to_string(std::time(nullptr))}
        };
        
        create_alert_incident(rule_id, rule, incident_data);
        
        // Update metrics
        {
            std::lock_guard<std::mutex> lock(metrics_mutex_);
            metrics_.alerts_triggered++;
        }
    }
}

nlohmann::json AlertEvaluationEngine::collect_metric_data(const std::string& metric_name) {
    auto conn = db_conn_->get_connection();
    if (!conn) {
        return nlohmann::json{};
    }
    
    // Map metric names to database queries
    if (metric_name == "transaction_volume") {
        return collect_transaction_metrics();
    } else if (metric_name == "system_load") {
        return collect_system_metrics();
    } else if (metric_name == "compliance_score") {
        return collect_compliance_metrics();
    } else if (metric_name == "response_time") {
        return collect_performance_metrics();
    } else {
        logger_->log(LogLevel::WARN, "Unknown metric name: " + metric_name);
        return nlohmann::json{};
    }
}

nlohmann::json AlertEvaluationEngine::collect_transaction_metrics() {
    auto conn = db_conn_->get_connection();
    if (!conn) {
        return nlohmann::json{};
    }
    
    // Get transaction count in the last 5 minutes
    PGresult* result = PQexecParams(
        conn,
        "SELECT COUNT(*) as count, AVG(amount) as avg_amount, MAX(amount) as max_amount "
        "FROM transactions WHERE created_at >= NOW() - INTERVAL '5 minutes'",
        0, nullptr, nullptr, nullptr, nullptr, 0
    );
    
    if (PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) == 0) {
        PQclear(result);
        return nlohmann::json{};
    }
    
    nlohmann::json metrics = {
        {"metric", "transaction_volume"},
        {"value", static_cast<double>(std::stoi(PQgetvalue(result, 0, 0)))},
        {"avg_amount", !PQgetisnull(result, 0, 1) ? std::stod(PQgetvalue(result, 0, 1)) : 0.0},
        {"max_amount", !PQgetisnull(result, 0, 2) ? std::stod(PQgetvalue(result, 0, 2)) : 0.0},
        {"timestamp", std::to_string(std::time(nullptr))}
    };
    
    PQclear(result);
    return metrics;
}

nlohmann::json AlertEvaluationEngine::collect_system_metrics() {
    auto conn = db_conn_->get_connection();
    if (!conn) {
        return nlohmann::json{};
    }
    
    // Get system metrics from database or system monitoring
    PGresult* result = PQexecParams(
        conn,
        "SELECT "
        "(SELECT COUNT(*) FROM active_sessions) as active_sessions, "
        "(SELECT COUNT(*) FROM audit_logs WHERE created_at >= NOW() - INTERVAL '5 minutes') as recent_log_entries",
        0, nullptr, nullptr, nullptr, nullptr, 0
    );
    
    if (PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) == 0) {
        PQclear(result);
        return nlohmann::json{};
    }
    
    nlohmann::json metrics = {
        {"metric", "system_load"},
        {"value", static_cast<double>(std::stoi(PQgetvalue(result, 0, 0)))},
        {"active_sessions", std::stoi(PQgetvalue(result, 0, 0))},
        {"recent_log_entries", std::stoi(PQgetvalue(result, 0, 1))},
        {"timestamp", std::to_string(std::time(nullptr))}
    };
    
    PQclear(result);
    return metrics;
}

nlohmann::json AlertEvaluationEngine::collect_compliance_metrics() {
    auto conn = db_conn_->get_connection();
    if (!conn) {
        return nlohmann::json{};
    }
    
    // Get compliance score from recent compliance checks
    PGresult* result = PQexecParams(
        conn,
        "SELECT AVG(compliance_score) as avg_score "
        "FROM compliance_checks WHERE created_at >= NOW() - INTERVAL '1 hour'",
        0, nullptr, nullptr, nullptr, nullptr, 0
    );
    
    if (PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) == 0 || PQgetisnull(result, 0, 0)) {
        PQclear(result);
        return nlohmann::json{};
    }
    
    double score = std::stod(PQgetvalue(result, 0, 0));
    PQclear(result);
    
    nlohmann::json metrics = {
        {"metric", "compliance_score"},
        {"value", score},
        {"timestamp", std::to_string(std::time(nullptr))}
    };
    
    return metrics;
}

nlohmann::json AlertEvaluationEngine::collect_performance_metrics() {
    auto conn = db_conn_->get_connection();
    if (!conn) {
        return nlohmann::json{};
    }
    
    // Get average response time from recent API calls
    PGresult* result = PQexecParams(
        conn,
        "SELECT AVG(response_time_ms) as avg_response_time "
        "FROM api_logs WHERE created_at >= NOW() - INTERVAL '5 minutes'",
        0, nullptr, nullptr, nullptr, nullptr, 0
    );
    
    if (PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) == 0 || PQgetisnull(result, 0, 0)) {
        PQclear(result);
        return nlohmann::json{};
    }
    
    double response_time = std::stod(PQgetvalue(result, 0, 0));
    PQclear(result);
    
    nlohmann::json metrics = {
        {"metric", "response_time"},
        {"value", response_time},
        {"timestamp", std::to_string(std::time(nullptr))}
    };
    
    return metrics;
}

bool AlertEvaluationEngine::evaluate_condition(const nlohmann::json& condition, const nlohmann::json& current_data) {
    if (!condition.contains("operator") || !condition.contains("threshold")) {
        return false;
    }
    
    if (!current_data.contains("value")) {
        return false;
    }
    
    double current_value = current_data["value"];
    std::string operator_str = condition["operator"];
    double threshold = condition["threshold"];
    
    return compare_values(current_value, operator_str, threshold);
}

bool AlertEvaluationEngine::compare_values(double current_value, const std::string& operator_str, double threshold) {
    if (operator_str == "gt") {
        return current_value > threshold;
    } else if (operator_str == "gte") {
        return current_value >= threshold;
    } else if (operator_str == "lt") {
        return current_value < threshold;
    } else if (operator_str == "lte") {
        return current_value <= threshold;
    } else if (operator_str == "eq") {
        return std::abs(current_value - threshold) < 0.0001; // Account for floating point precision
    } else if (operator_str == "ne") {
        return std::abs(current_value - threshold) >= 0.0001;
    }
    
    return false;
}

bool AlertEvaluationEngine::evaluate_pattern_match(const std::string& pattern, const nlohmann::json& data) {
    try {
        // Convert pattern to regex
        std::regex regex_pattern(pattern, std::regex_constants::icase);
        
        // Convert data to string for pattern matching
        std::string data_str = data.dump();
        
        return std::regex_search(data_str, regex_pattern);
    } catch (const std::regex_error& e) {
        logger_->log(LogLevel::ERROR, "Invalid regex pattern: " + pattern + " - " + e.what());
        return false;
    }
}

bool AlertEvaluationEngine::evaluate_anomaly_detection(const nlohmann::json& baseline, const nlohmann::json& current) {
    if (!baseline.contains("mean") || !baseline.contains("std_dev") || !current.contains("value")) {
        return false;
    }
    
    double mean = baseline["mean"];
    double std_dev = baseline["std_dev"];
    double current_value = current["value"];
    
    // Calculate Z-score
    double z_score = std::abs((current_value - mean) / std_dev);
    
    // Consider it an anomaly if Z-score > 2 (beyond 2 standard deviations)
    return z_score > 2.0;
}

nlohmann::json AlertEvaluationEngine::get_baseline_data(const std::string& metric_name) {
    auto conn = db_conn_->get_connection();
    if (!conn) {
        return nlohmann::json{};
    }
    
    // Get historical data for the last 24 hours to calculate baseline
    const char* param_values[1] = {metric_name.c_str()};
    PGresult* result = PQexecParams(
        conn,
        "SELECT AVG(value) as mean, STDDEV(value) as std_dev "
        "FROM metric_history "
        "WHERE metric_name = $1 AND created_at >= NOW() - INTERVAL '24 hours'",
        1, nullptr, param_values, nullptr, nullptr, 0
    );
    
    if (PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) == 0 || 
        PQgetisnull(result, 0, 0) || PQgetisnull(result, 0, 1)) {
        PQclear(result);
        return nlohmann::json{};
    }
    
    nlohmann::json baseline = {
        {"mean", std::stod(PQgetvalue(result, 0, 0))},
        {"std_dev", std::stod(PQgetvalue(result, 0, 1))}
    };
    
    PQclear(result);
    return baseline;
}

bool AlertEvaluationEngine::is_schedule_time(const std::string& schedule) {
    // Simple schedule evaluation - in production, this would use a proper cron library
    // For now, just check if it's a daily schedule at a specific hour
    if (schedule.find("daily") != std::string::npos) {
        // Extract hour from schedule like "daily at 09:00"
        size_t pos = schedule.find("at ");
        if (pos != std::string::npos) {
            std::string time_str = schedule.substr(pos + 3);
            int hour = std::stoi(time_str.substr(0, time_str.find(":")));
            
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            auto tm = *std::localtime(&time_t);
            
            return tm.tm_hour == hour && tm.tm_min == 0;
        }
    }
    
    return false;
}

void AlertEvaluationEngine::create_alert_incident(const std::string& rule_id, const nlohmann::json& rule,
                                                 const nlohmann::json& incident_data) {
    auto conn = db_conn_->get_connection();
    if (!conn) {
        return;
    }
    
    std::string severity_str = rule["severity"].get<std::string>();
    std::transform(severity_str.begin(), severity_str.end(), severity_str.begin(), ::toupper);
    std::string title = "[" + severity_str + "] " + rule["rule_name"].get<std::string>();
    
    // Generate message based on rule type and data
    std::string message = "Alert triggered for rule: " + rule["rule_name"].get<std::string>() + "\n";
    message += "Data: " + incident_data.dump();
    
    const char* params[5] = {
        rule_id.c_str(),
        rule["severity"].get<std::string>().c_str(),
        title.c_str(),
        message.c_str(),
        incident_data.dump().c_str()
    };
    
    PGresult* result = PQexecParams(
        conn,
        "INSERT INTO alert_incidents (rule_id, severity, title, message, incident_data) "
        "VALUES ($1, $2, $3, $4, $5::jsonb) RETURNING incident_id",
        5, nullptr, params, nullptr, nullptr, 0
    );
    
    if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) > 0) {
        std::string incident_id = PQgetvalue(result, 0, 0);
        PQclear(result);
        
        // Trigger notifications
        trigger_notifications(incident_id, rule);
        
        // Update rule last triggered time
        update_rule_last_triggered(rule_id);
        
        logger_->log(LogLevel::INFO, "Created alert incident: " + incident_id + " for rule: " + rule["rule_name"].get<std::string>());
    } else {
        PQclear(result);
        logger_->log(LogLevel::ERROR, "Failed to create alert incident for rule: " + rule["rule_name"].get<std::string>());
    }
}

bool AlertEvaluationEngine::is_rule_in_cooldown(const std::string& rule_id) {
    auto conn = db_conn_->get_connection();
    if (!conn) {
        return true; // Fail safe - don't trigger if can't check
    }
    
    const char* params[1] = {rule_id.c_str()};
    PGresult* result = PQexecParams(
        conn,
        "SELECT cooldown_minutes, last_triggered_at FROM alert_rules WHERE rule_id = $1",
        1, nullptr, params, nullptr, nullptr, 0
    );
    
    if (PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) == 0) {
        PQclear(result);
        return true; // Rule not found
    }
    
    int cooldown_minutes = std::stoi(PQgetvalue(result, 0, 0));
    
    if (PQgetisnull(result, 0, 1)) {
        PQclear(result);
        return false; // Never triggered before
    }
    
    std::string last_triggered_str = PQgetvalue(result, 0, 1);
    PQclear(result);
    
    // Check if cooldown period has passed
    // This is a simplified check - in production, proper timestamp parsing would be needed
    std::time_t last_triggered = std::stol(last_triggered_str);
    std::time_t now = std::time(nullptr);
    double seconds_since_trigger = difftime(now, last_triggered);
    double cooldown_seconds = cooldown_minutes * 60.0;
    
    return seconds_since_trigger < cooldown_seconds;
}

void AlertEvaluationEngine::update_rule_last_triggered(const std::string& rule_id) {
    auto conn = db_conn_->get_connection();
    if (!conn) {
        return;
    }
    
    const char* params[1] = {rule_id.c_str()};
    PGresult* result = PQexecParams(
        conn,
        "UPDATE alert_rules SET last_triggered_at = CURRENT_TIMESTAMP WHERE rule_id = $1",
        1, nullptr, params, nullptr, nullptr, 0
    );
    
    PQclear(result);
}

void AlertEvaluationEngine::trigger_notifications(const std::string& incident_id, const nlohmann::json& rule) {
    // This would integrate with the notification service
    // For now, just log that notifications should be triggered
    logger_->log(LogLevel::INFO, "Triggering notifications for incident: " + incident_id);
    
    // In a full implementation, this would call the notification service to send
    // notifications through all configured channels for this rule
}

void AlertEvaluationEngine::retry_failed_notifications() {
    auto conn = db_conn_->get_connection();
    if (!conn) {
        return;
    }
    
    // Get failed notifications that are ready for retry
    PGresult* result = PQexecParams(
        conn.get(),
        "SELECT notification_id, incident_id, channel_id, retry_count "
        "FROM alert_notifications "
        "WHERE delivery_status = 'failed' "
        "AND (next_retry_at IS NULL OR next_retry_at <= CURRENT_TIMESTAMP) "
        "AND retry_count < 5 "
        "ORDER BY sent_at ASC LIMIT 10",
        0, nullptr, nullptr, nullptr, nullptr, 0
    );
    
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        PQclear(result);
        return;
    }
    
    int num_rows = PQntuples(result);
    for (int i = 0; i < num_rows; i++) {
        std::string notification_id = PQgetvalue(result, i, 0);
        std::string incident_id = PQgetvalue(result, i, 1);
        std::string channel_id = PQgetvalue(result, i, 2);
        int retry_count = std::stoi(PQgetvalue(result, i, 3));
        
        // Schedule retry with exponential backoff
        schedule_notification_retry(notification_id, retry_count + 1);
    }
    
    PQclear(result);
}

void AlertEvaluationEngine::schedule_notification_retry(const std::string& notification_id, int retry_count) {
    auto conn = db_conn_->get_connection();
    if (!conn) {
        return;
    }
    
    // Calculate next retry time with exponential backoff (2^retry_count minutes, max 2 hours)
    int delay_minutes = std::min(1 << retry_count, 120);
    
    std::string retry_count_str = std::to_string(retry_count);
    const char* params[2] = {
        retry_count_str.c_str(),
        notification_id.c_str()
    };
    
    PGresult* result = PQexecParams(
        conn.get(),
        "UPDATE alert_notifications SET retry_count = $1, next_retry_at = CURRENT_TIMESTAMP + INTERVAL '" +
        std::to_string(delay_minutes) + " minutes', delivery_status = 'pending' "
        "WHERE notification_id = $2",
        2, nullptr, params, nullptr, nullptr, 0
    );
    
    PQclear(result);
    
    logger_->log(LogLevel::DEBUG, "Scheduled retry for notification " + notification_id + 
                " in " + std::to_string(delay_minutes) + " minutes (attempt " + std::to_string(retry_count) + ")");
}

// Utility function implementations
AlertRuleType parse_rule_type(const std::string& type_str) {
    if (type_str == "threshold") return AlertRuleType::THRESHOLD;
    if (type_str == "pattern") return AlertRuleType::PATTERN;
    if (type_str == "anomaly") return AlertRuleType::ANOMALY;
    if (type_str == "scheduled") return AlertRuleType::SCHEDULED;
    return AlertRuleType::THRESHOLD; // Default
}

AlertSeverity parse_severity(const std::string& severity_str) {
    if (severity_str == "low") return AlertSeverity::LOW;
    if (severity_str == "medium") return AlertSeverity::MEDIUM;
    if (severity_str == "high") return AlertSeverity::HIGH;
    if (severity_str == "critical") return AlertSeverity::CRITICAL;
    return AlertSeverity::MEDIUM; // Default
}

AlertStatus parse_status(const std::string& status_str) {
    if (status_str == "active") return AlertStatus::ACTIVE;
    if (status_str == "acknowledged") return AlertStatus::ACKNOWLEDGED;
    if (status_str == "resolved") return AlertStatus::RESOLVED;
    if (status_str == "false_positive") return AlertStatus::FALSE_POSITIVE;
    return AlertStatus::ACTIVE; // Default
}

std::string rule_type_to_string(AlertRuleType type) {
    switch (type) {
        case AlertRuleType::THRESHOLD: return "threshold";
        case AlertRuleType::PATTERN: return "pattern";
        case AlertRuleType::ANOMALY: return "anomaly";
        case AlertRuleType::SCHEDULED: return "scheduled";
    }
    return "threshold";
}

std::string severity_to_string(AlertSeverity severity) {
    switch (severity) {
        case AlertSeverity::LOW: return "low";
        case AlertSeverity::MEDIUM: return "medium";
        case AlertSeverity::HIGH: return "high";
        case AlertSeverity::CRITICAL: return "critical";
    }
    return "medium";
}

std::string status_to_string(AlertStatus status) {
    switch (status) {
        case AlertStatus::ACTIVE: return "active";
        case AlertStatus::ACKNOWLEDGED: return "acknowledged";
        case AlertStatus::RESOLVED: return "resolved";
        case AlertStatus::FALSE_POSITIVE: return "false_positive";
    }
    return "active";
}

} // namespace alerts
} // namespace regulens