/**
 * Production Regulatory Monitor - Enterprise Grade
 *
 * Real-time monitoring of regulatory changes with database persistence,
 * multi-source support, and production-grade error handling.
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <shared/database/postgresql_connection.hpp>
#include <shared/network/http_client.hpp>
#include <shared/logging/structured_logger.hpp>

namespace regulens {

struct RegulatoryChange {
    std::string id;
    std::string source;  // 'SEC', 'FCA', 'ECB', etc.
    std::string title;
    std::string description;
    std::string content_url;
    std::string change_type;  // 'rule', 'guidance', 'policy', etc.
    std::string severity;     // 'LOW', 'MEDIUM', 'HIGH', 'CRITICAL'
    nlohmann::json metadata;
    std::chrono::system_clock::time_point detected_at;
    std::chrono::system_clock::time_point published_at;

    RegulatoryChange() = default;

    nlohmann::json to_json() const;
    static RegulatoryChange from_json(const nlohmann::json& j);
};

struct RegulatorySource {
    std::string id;
    std::string name;
    std::string base_url;
    std::string source_type;  // 'rss', 'html', 'api'
    int check_interval_minutes;
    bool active;
    nlohmann::json scraping_config;
    std::chrono::system_clock::time_point last_check;
    int consecutive_failures;

    RegulatorySource() : check_interval_minutes(30), active(true),
                        consecutive_failures(0) {}

    bool should_check() const;
};

class ProductionRegulatoryMonitor {
public:
    ProductionRegulatoryMonitor(
        std::shared_ptr<ConnectionPool> db_pool,
        std::shared_ptr<HttpClient> http_client,
        StructuredLogger* logger
    );

    ~ProductionRegulatoryMonitor();

    // Lifecycle management
    bool initialize();
    void start_monitoring();
    void stop_monitoring();
    bool is_running() const;

    // Source management
    bool add_source(const RegulatorySource& source);
    bool update_source(const std::string& source_id, const RegulatorySource& source);
    bool remove_source(const std::string& source_id);
    std::vector<RegulatorySource> get_sources() const;

    // Manual operations
    bool force_check_source(const std::string& source_id);
    std::vector<RegulatoryChange> get_recent_changes(int limit = 50) const;
    bool store_change(const RegulatoryChange& change);

    // Statistics and monitoring
    nlohmann::json get_monitoring_stats() const;
    nlohmann::json get_source_stats(const std::string& source_id) const;

private:
    // Core monitoring logic
    void monitoring_loop();
    void check_all_sources();
    void check_source(const std::string& source_id);
    bool process_source_data(const std::string& source_id,
                           const std::string& raw_data,
                           const RegulatorySource& source);

    // Data extraction methods
    std::vector<RegulatoryChange> extract_sec_edgar_changes(const std::string& rss_data);
    std::vector<RegulatoryChange> extract_fca_changes(const std::string& html_data);
    std::vector<RegulatoryChange> extract_ecb_changes(const std::string& xml_data);

    // Database operations
    bool store_regulatory_change(const RegulatoryChange& change);
    bool update_source_last_check(const std::string& source_id);
    bool increment_source_failures(const std::string& source_id);
    bool reset_source_failures(const std::string& source_id);

    // Utility methods
    std::string generate_change_id(const std::string& source, const std::string& title);
    bool is_duplicate_change(const RegulatoryChange& change);
    std::string extract_text_content(const std::string& html, const std::string& selector);
    std::chrono::system_clock::time_point parse_rfc822_date(const std::string& date_str);

    // Member variables
    std::shared_ptr<ConnectionPool> db_pool_;
    std::shared_ptr<HttpClient> http_client_;
    StructuredLogger* logger_;

    std::unordered_map<std::string, RegulatorySource> sources_;
    mutable std::mutex sources_mutex_;

    std::thread monitoring_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;

    // Statistics
    std::atomic<size_t> total_checks_;
    std::atomic<size_t> successful_checks_;
    std::atomic<size_t> failed_checks_;
    std::atomic<size_t> changes_detected_;
    std::atomic<size_t> duplicates_avoided_;

    // Monitoring interval
    const int MONITORING_INTERVAL_SECONDS = 60; // Check every minute
    const int MAX_CONSECUTIVE_FAILURES = 5;     // Disable source after 5 failures
};

} // namespace regulens
