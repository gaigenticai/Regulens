/**
 * Ingestion Metrics - Production-Grade Monitoring and Analytics
 *
 * Comprehensive metrics collection for data ingestion framework with:
 * - Real-time performance monitoring
 * - Source health tracking
 * - Error rate analysis
 * - Throughput and latency metrics
 * - Predictive alerting capabilities
 *
 * Retrospective Enhancement: Provides monitoring for existing systems
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <chrono>
#include "data_ingestion_framework.hpp"
#include "../logging/structured_logger.hpp"
#include <nlohmann/json.hpp>

namespace regulens {

class IngestionMetrics {
public:
    IngestionMetrics(StructuredLogger* logger);

    // Batch processing metrics
    void record_batch_processed(const std::string& source_id, const IngestionBatch& batch);
    void record_ingestion_error(const std::string& source_id, const std::string& error);
    void record_source_health(const std::string& source_id, bool healthy);

    // Performance metrics
    void record_processing_time(const std::string& source_id, const std::chrono::microseconds& duration);
    void record_throughput(const std::string& source_id, int records_per_second);
    void record_queue_depth(int depth);

    // Error analysis
    void categorize_error(const std::string& error_type, const std::string& source_id);
    std::vector<std::string> get_top_error_types(int limit = 10) const;
    double get_error_rate(const std::string& source_id) const;

    // Health monitoring
    bool is_source_healthy(const std::string& source_id) const;
    std::vector<std::string> get_failing_sources() const;
    nlohmann::json get_system_health() const;

    // Analytics and reporting
    nlohmann::json get_source_metrics(const std::string& source_id) const;
    nlohmann::json get_global_metrics() const;
    nlohmann::json get_performance_report(const std::chrono::hours& time_window = std::chrono::hours(24)) const;

    // Predictive analytics
    nlohmann::json get_trend_analysis(const std::string& source_id) const;
    std::vector<std::string> predict_potential_failures() const;
    nlohmann::json get_capacity_forecast() const;

    // Alerting
    bool should_alert_on_error_rate(const std::string& source_id, double threshold = 0.05) const;
    bool should_alert_on_throughput_drop(const std::string& source_id, double threshold = 0.5) const;
    std::vector<nlohmann::json> get_active_alerts() const;

private:
    struct SourceMetrics {
        // Processing metrics
        int64_t total_batches = 0;
        int64_t successful_batches = 0;
        int64_t failed_batches = 0;
        int64_t total_records = 0;
        int64_t successful_records = 0;
        int64_t failed_records = 0;

        // Performance metrics
        std::chrono::microseconds total_processing_time = std::chrono::microseconds(0);
        int64_t max_records_per_second = 0;
        int64_t avg_records_per_second = 0;

        // Health metrics
        bool is_healthy = true;
        int consecutive_failures = 0;
        std::chrono::system_clock::time_point last_successful_batch;
        std::chrono::system_clock::time_point last_health_check;

        // Error tracking
        std::unordered_map<std::string, int> error_counts;
        std::vector<std::string> recent_errors;

        // Trend analysis
        std::vector<std::pair<std::chrono::system_clock::time_point, int64_t>> throughput_history;
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> error_rate_history;
    };

    struct GlobalMetrics {
        int64_t total_batches_processed = 0;
        int64_t total_records_processed = 0;
        int current_queue_depth = 0;
        int active_workers = 0;
        std::chrono::system_clock::time_point system_start_time;
    };

    // Internal methods
    void update_source_health(const std::string& source_id, SourceMetrics& metrics);
    void cleanup_old_data(const std::string& source_id);
    double calculate_error_rate(const SourceMetrics& metrics) const;
    int64_t calculate_throughput(const SourceMetrics& metrics) const;

    // Configuration
    void load_alert_thresholds();
    void initialize_baseline_metrics();

    // Data persistence (simplified)
    void persist_metrics() const;
    void load_persisted_metrics();

    StructuredLogger* logger_;
    mutable std::mutex metrics_mutex_;

    std::unordered_map<std::string, SourceMetrics> source_metrics_;
    GlobalMetrics global_metrics_;

    // Alert thresholds
    double error_rate_alert_threshold_ = 0.05; // 5%
    double throughput_drop_threshold_ = 0.5;   // 50% drop
    int max_consecutive_failures_ = 5;
    std::chrono::minutes health_check_interval_ = std::chrono::minutes(5);

    // Constants
    const std::chrono::hours METRICS_RETENTION_PERIOD = std::chrono::hours(24);
    const int MAX_RECENT_ERRORS = 100;
    const int MAX_HISTORY_POINTS = 1000;
};

} // namespace regulens
