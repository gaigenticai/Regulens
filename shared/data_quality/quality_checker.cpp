#include "quality_checker.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <vector>

using json = nlohmann::json;

QualityChecker::QualityChecker(std::shared_ptr<regulens::PostgreSQLConnection> db_conn,
                              std::shared_ptr<DataQualityHandlers> handlers,
                              std::shared_ptr<regulens::StructuredLogger> logger)
    : db_conn_(db_conn), handlers_(handlers), logger_(logger),
      running_(false), check_interval_minutes_(15),
      total_checks_run_(0), checks_passed_(0), checks_failed_(0) {
    
    last_check_time_ = std::chrono::steady_clock::now();
    
    // Read check interval from environment variable if available
    const char* interval_env = std::getenv("DATA_QUALITY_CHECK_INTERVAL_MINUTES");
    if (interval_env) {
        try {
            int interval = std::stoi(interval_env);
            if (interval > 0 && interval <= 1440) { // Between 1 minute and 24 hours
                check_interval_minutes_ = interval;
                logger_->info("Data quality check interval set to " +
                           std::to_string(interval) + " minutes from environment variable", "QualityChecker", "QualityChecker");
            }
        } catch (const std::exception& e) {
            logger_->warn("Invalid DATA_QUALITY_CHECK_INTERVAL_MINUTES value, using default", "QualityChecker", "QualityChecker");
        }
    }
}

QualityChecker::~QualityChecker() {
    stop();
}

void QualityChecker::start() {
    if (running_) {
        logger_->warn("Quality checker is already running", "QualityChecker", "start");
        return;
    }
    
    running_ = true;
    checker_thread_ = std::thread(&QualityChecker::checker_loop, this);
    
    logger_->info("Data quality checker started with " +
               std::to_string(check_interval_minutes_) + " minute interval", "QualityChecker", "start");
}

void QualityChecker::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (checker_thread_.joinable()) {
        checker_thread_.join();
    }
    
    logger_->info("Data quality checker stopped", "QualityChecker", "stop");
}

bool QualityChecker::is_running() const {
    return running_;
}

void QualityChecker::set_check_interval_minutes(int minutes) {
    if (minutes <= 0 || minutes > 1440) {
        logger_->warn("Invalid check interval: " + std::to_string(minutes) +
                   ". Must be between 1 and 1440 minutes", "QualityChecker", "set_check_interval_minutes");
        return;
    }
    
    check_interval_minutes_ = minutes;
    logger_->info("Data quality check interval updated to " +
               std::to_string(minutes) + " minutes", "QualityChecker", "set_check_interval_minutes");
}

int QualityChecker::get_check_interval_minutes() const {
    return check_interval_minutes_;
}

void QualityChecker::run_all_checks() {
    logger_->info("Manual trigger: Running all data quality checks", "QualityChecker", "run_all_checks");
    
    try {
        std::vector<std::string> enabled_rules = get_enabled_rules();
        
        if (enabled_rules.empty()) {
            logger_->info("No enabled data quality rules found", "QualityChecker", "run_all_checks");
            return;
        }
        
        logger_->info("Found " + std::to_string(enabled_rules.size()) +
                   " enabled rules to check", "QualityChecker", "run_all_checks");
        
        for (const auto& rule_id : enabled_rules) {
            execute_rule_check(rule_id);
        }
        
        logger_->info("Manual data quality check completed. Total: " +
                   std::to_string(enabled_rules.size()) + " rules", "QualityChecker", "run_all_checks");
        
    } catch (const std::exception& e) {
        logger_->error("Exception in run_all_checks: " + std::string(e.what()), "QualityChecker", "run_all_checks");
    }
}

void QualityChecker::run_check_for_rule(const std::string& rule_id) {
    logger_->info("Manual trigger: Running data quality check for rule: " + rule_id, "QualityChecker", "run_check_for_rule");
    
    try {
        execute_rule_check(rule_id);
        logger_->info("Manual data quality check completed for rule: " + rule_id, "QualityChecker", "run_check_for_rule");
    } catch (const std::exception& e) {
        logger_->error("Exception in run_check_for_rule: " + std::string(e.what()), "QualityChecker", "run_check_for_rule");
    }
}

void QualityChecker::checker_loop() {
    logger_->info("Data quality checker loop started", "QualityChecker", "checker_loop");
    
    while (running_) {
        try {
            // Calculate next check time
            auto next_check = last_check_time_ + 
                            std::chrono::minutes(check_interval_minutes_.load());
            auto now = std::chrono::steady_clock::now();
            
            if (now >= next_check) {
                logger_->info("Executing scheduled data quality checks", "QualityChecker", "checker_loop");
                execute_scheduled_checks();
                last_check_time_ = now;
            }
            
            // Sleep for a short interval to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::seconds(10));
            
        } catch (const std::exception& e) {
            logger_->error("Exception in quality checker loop: " + std::string(e.what()), "QualityChecker", "checker_loop");
            std::this_thread::sleep_for(std::chrono::minutes(1)); // Wait before retrying
        }
    }
    
    logger_->info("Data quality checker loop ended", "QualityChecker", "checker_loop");
}

void QualityChecker::execute_scheduled_checks() {
    try {
        std::vector<std::string> enabled_rules = get_enabled_rules();
        
        if (enabled_rules.empty()) {
            logger_->debug("No enabled data quality rules found for scheduled check", "QualityChecker", "execute_scheduled_checks");
            return;
        }
        
        logger_->info("Executing scheduled checks for " +
                   std::to_string(enabled_rules.size()) + " enabled rules", "QualityChecker", "execute_scheduled_checks");
        
        int passed_count = 0;
        int failed_count = 0;
        
        for (const auto& rule_id : enabled_rules) {
            try {
                execute_rule_check(rule_id);
                
                // Get the latest check result to determine pass/fail
                const char* result_query = "SELECT status FROM data_quality_checks "
                                         "WHERE rule_id = $1 ORDER BY check_timestamp DESC LIMIT 1";
                
                std::vector<std::string> params = {rule_id};
                auto result = db_conn_->execute_query(result_query, params);
                
                if (!result.rows.empty()) {
                    std::string status = result.rows[0].at("status");
                    if (status == "passed") {
                        passed_count++;
                    } else {
                        failed_count++;
                    }
                }
                
            } catch (const std::exception& e) {
                logger_->error("Failed to execute check for rule " + rule_id +
                           ": " + std::string(e.what()), "QualityChecker", "execute_scheduled_checks");
                failed_count++;
            }
        }
        
        total_checks_run_ += enabled_rules.size();
        checks_passed_ += passed_count;
        checks_failed_ += failed_count;
        
        logger_->info("Scheduled checks completed. Passed: " +
                   std::to_string(passed_count) + ", Failed: " + std::to_string(failed_count) +
                   ", Total: " + std::to_string(enabled_rules.size()), "QualityChecker", "execute_scheduled_checks");
        
    } catch (const std::exception& e) {
        logger_->error("Exception in execute_scheduled_checks: " + std::string(e.what()), "QualityChecker", "execute_scheduled_checks");
    }
}

std::vector<std::string> QualityChecker::get_enabled_rules() {
    std::vector<std::string> enabled_rules;
    
    try {
        const char* query = "SELECT rule_id FROM data_quality_rules WHERE is_enabled = true";
        auto result = db_conn_->execute_query(query, {});
        
        for (const auto& row : result.rows) {
            enabled_rules.push_back(row.at("rule_id"));
        }
        
    } catch (const std::exception& e) {
        logger_->error("Exception in get_enabled_rules: " + std::string(e.what()), "QualityChecker", "get_enabled_rules");
    }
    
    return enabled_rules;
}

void QualityChecker::execute_rule_check(const std::string& rule_id) {
    try {
        // Create a mock headers map for the handlers
        std::map<std::string, std::string> headers;
        headers["authorization"] = "Bearer system_check";
        
        // Run the quality check
        std::string result = handlers_->run_quality_check(rule_id, headers);
        
        // Parse result to check for errors
        json result_json = json::parse(result);
        
        if (result_json.contains("success") && result_json["success"].get<bool>()) {
            if (result_json.contains("data")) {
                auto data = result_json["data"];
                if (data.contains("quality_score")) {
                    double quality_score = data["quality_score"];
                    logger_->debug("Quality check for rule " + rule_id +
                               " completed with score: " + std::to_string(quality_score), "QualityChecker", "execute_rule_check");
                    
                    // Check if we should trigger an alert
                    if (should_trigger_alert(rule_id, quality_score)) {
                        trigger_alert_for_failed_check(rule_id, result);
                    }
                }
            }
            
            log_check_result(rule_id, result);
        } else {
            logger_->error("Quality check failed for rule " + rule_id +
                       ": " + result_json.value("error", "Unknown error"), "QualityChecker", "execute_rule_check");
        }
        
    } catch (const std::exception& e) {
        logger_->error("Exception in execute_rule_check for rule " + rule_id +
                   ": " + std::string(e.what()), "QualityChecker", "execute_rule_check");
    }
}

void QualityChecker::log_check_result(const std::string& rule_id, const std::string& result) {
    try {
        json result_json = json::parse(result);
        
        if (result_json.contains("data")) {
            auto data = result_json["data"];
            std::string status = data.value("status", "unknown");
            double quality_score = data.value("quality_score", 0.0);
            int records_checked = data.value("records_checked", 0);
            int records_failed = data.value("records_failed", 0);
            
            if (status == "failed") {
                logger_->warn("Quality check result for rule " + rule_id +
                       " - Status: " + status +
                       ", Score: " + std::to_string(quality_score) +
                       ", Records: " + std::to_string(records_checked) +
                       ", Failed: " + std::to_string(records_failed), "QualityChecker", "log_check_result");
            } else {
                logger_->info("Quality check result for rule " + rule_id +
                       " - Status: " + status +
                       ", Score: " + std::to_string(quality_score) +
                       ", Records: " + std::to_string(records_checked) +
                       ", Failed: " + std::to_string(records_failed), "QualityChecker", "log_check_result");
            }
        }
        
    } catch (const std::exception& e) {
        logger_->error("Exception in log_check_result: " + std::string(e.what()), "QualityChecker", "log_check_result");
    }
}

void QualityChecker::trigger_alert_for_failed_check(const std::string& rule_id, 
                                                   const std::string& check_result) {
    try {
        logger_->info("Triggering alert for failed quality check: " + rule_id, "QualityChecker", "trigger_alert_for_failed_check");
        
        // Get rule details for the alert
        const char* rule_query = "SELECT rule_name, data_source, rule_type, severity "
                                "FROM data_quality_rules WHERE rule_id = $1";
        
        std::vector<std::string> params = {rule_id};
        auto rule_result = db_conn_->execute_query(rule_query, params);
        
        if (rule_result.rows.empty()) {
            logger_->error("Failed to get rule details for alert", "QualityChecker", "trigger_alert_for_failed_check");
            return;
        }
        
        std::string rule_name = rule_result.rows[0].at("rule_name");
        std::string data_source = rule_result.rows[0].at("data_source");
        std::string rule_type = rule_result.rows[0].at("rule_type");
        std::string severity = rule_result.rows[0].at("severity");
        
        // Parse check result for details
        json check_json = json::parse(check_result);
        auto data = check_json["data"];
        double quality_score = data.value("quality_score", 0.0);
        int records_failed = data.value("records_failed", 0);
        
        // Create alert incident
        const char* alert_query = "INSERT INTO alert_incidents (rule_id, severity, title, message, "
                                 "incident_data, status, triggered_at) "
                                 "VALUES ($1, $2, $3, $4, $5, 'active', NOW())";
        
        std::string title = "Data Quality Check Failed: " + rule_name;
        std::string message = "Data quality rule '" + rule_name + "' failed with score " + 
                             std::to_string(quality_score) + "%. " + 
                             std::to_string(records_failed) + " records failed validation.";
        
        json incident_data = {
            {"rule_id", rule_id},
            {"rule_name", rule_name},
            {"data_source", data_source},
            {"rule_type", rule_type},
            {"quality_score", quality_score},
            {"records_failed", records_failed},
            {"check_result", check_json["data"]}
        };
        
        std::string incident_data_str = incident_data.dump();
        
        std::vector<std::string> alert_params = {
            rule_id,
            severity,
            title,
            message,
            incident_data_str
        };
        
        auto alert_result = db_conn_->execute_query(alert_query, alert_params);
        
        if (alert_result.rows.empty()) {
            logger_->error("Failed to create alert incident", "QualityChecker", "evaluate_rule");
            return;
        }
        
        logger_->info("Alert incident created for failed quality check: " + rule_id, "QualityChecker", "trigger_alert_for_failed_check");
        
    } catch (const std::exception& e) {
        logger_->error("Exception in trigger_alert_for_failed_check: " + std::string(e.what()), "QualityChecker", "trigger_alert_for_failed_check");
    }
}

bool QualityChecker::should_trigger_alert(const std::string& rule_id, double quality_score) {
    // Check if the quality score is below the threshold
    const char* threshold_env = std::getenv("DATA_QUALITY_MIN_SCORE_THRESHOLD");
    double min_threshold = 80.0; // Default threshold
    
    if (threshold_env) {
        try {
            min_threshold = std::stod(threshold_env);
        } catch (const std::exception& e) {
            logger_->warn("Invalid DATA_QUALITY_MIN_SCORE_THRESHOLD value, using default", "QualityChecker", "should_trigger_alert");
        }
    }
    
    if (quality_score < min_threshold) {
        // Check if we've recently sent an alert for this rule to avoid spam
        try {
            const char* recent_alert_query = "SELECT COUNT(*) FROM alert_incidents "
                                           "WHERE rule_id = $1 AND triggered_at > NOW() - INTERVAL '1 hour'";
            
            std::vector<std::string> params = {rule_id};
            auto result = db_conn_->execute_query(recent_alert_query, params);
            
            if (!result.rows.empty()) {
                int recent_count = std::stoi(result.rows[0].at("count"));
                
                // Only trigger alert if we haven't sent one in the last hour
                return recent_count == 0;
            }
        } catch (const std::exception& e) {
            logger_->error("Exception checking recent alerts: " + std::string(e.what()), "QualityChecker", "should_trigger_alert");
        }
        
        return true;
    }
    
    return false;
}