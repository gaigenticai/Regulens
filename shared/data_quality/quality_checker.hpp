#ifndef QUALITY_CHECKER_HPP
#define QUALITY_CHECKER_HPP

#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <memory>
#include <map>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"
#include "data_quality_handlers.hpp"

class QualityChecker {
public:
    QualityChecker(std::shared_ptr<regulens::PostgreSQLConnection> db_conn,
                  std::shared_ptr<DataQualityHandlers> handlers,
                  std::shared_ptr<regulens::StructuredLogger> logger);
    ~QualityChecker();

    // Control methods
    void start();
    void stop();
    bool is_running() const;

    // Configuration
    void set_check_interval_minutes(int minutes);
    int get_check_interval_minutes() const;

    // Manual trigger
    void run_all_checks();
    void run_check_for_rule(const std::string& rule_id);

private:
    std::shared_ptr<regulens::PostgreSQLConnection> db_conn_;
    std::shared_ptr<DataQualityHandlers> handlers_;
    std::shared_ptr<regulens::StructuredLogger> logger_;

    // Threading
    std::atomic<bool> running_;
    std::thread checker_thread_;
    std::atomic<int> check_interval_minutes_;

    // Internal methods
    void checker_loop();
    void execute_scheduled_checks();
    std::vector<std::string> get_enabled_rules();
    void execute_rule_check(const std::string& rule_id);
    void log_check_result(const std::string& rule_id, const std::string& result);
    
    // Statistics
    std::atomic<int> total_checks_run_;
    std::atomic<int> checks_passed_;
    std::atomic<int> checks_failed_;
    std::chrono::steady_clock::time_point last_check_time_;
    
    // Alert integration
    void trigger_alert_for_failed_check(const std::string& rule_id, 
                                       const std::string& check_result);
    bool should_trigger_alert(const std::string& rule_id, double quality_score);
};

#endif // QUALITY_CHECKER_HPP