/**
 * Performance Monitor - Header
 * 
 * Production-grade performance monitoring for database queries and API responses.
 * Provides real-time metrics, query optimization recommendations, and performance analytics.
 */

#ifndef REGULENS_PERFORMANCE_MONITOR_HPP
#define REGULENS_PERFORMANCE_MONITOR_HPP

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <memory>
#include <mutex>
#include <functional>

namespace regulens {

// Performance metric types
enum class MetricType {
    DATABASE_QUERY,
    API_REQUEST,
    CACHE_HIT,
    CACHE_MISS,
    EXTERNAL_API_CALL,
    BACKGROUND_JOB,
    WEBSOCKET_MESSAGE
};

// Performance level
enum class PerformanceLevel {
    EXCELLENT,      // < 50ms
    GOOD,          // 50-200ms
    ACCEPTABLE,    // 200-500ms
    SLOW,          // 500-1000ms
    VERY_SLOW      // > 1000ms
};

// Query execution plan
struct QueryExecutionPlan {
    std::string query;
    std::string execution_plan;
    int estimated_cost;
    int actual_cost;
    std::vector<std::string> optimization_suggestions;
    std::vector<std::string> missing_indexes;
    std::vector<std::string> unused_indexes;
    bool needs_optimization;
};

// Performance metric
struct PerformanceMetric {
    std::string metric_id;
    MetricType type;
    std::string operation;
    std::chrono::system_clock::time_point timestamp;
    int duration_ms;
    bool success;
    std::string error_message;
    std::map<std::string, std::string> metadata;
    PerformanceLevel level;
};

// Performance statistics
struct PerformanceStats {
    std::string operation;
    int total_calls;
    int successful_calls;
    int failed_calls;
    double avg_duration_ms;
    int min_duration_ms;
    int max_duration_ms;
    double p50_duration_ms;  // Median
    double p95_duration_ms;
    double p99_duration_ms;
    std::chrono::system_clock::time_point first_call;
    std::chrono::system_clock::time_point last_call;
};

// Slow query log entry
struct SlowQueryLog {
    std::string query;
    int execution_time_ms;
    std::chrono::system_clock::time_point timestamp;
    std::string calling_function;
    int rows_examined;
    int rows_returned;
    std::string execution_plan;
};

/**
 * Performance Monitor
 * 
 * Comprehensive performance monitoring system for production deployments.
 * Features:
 * - Real-time performance tracking
 * - Query performance analysis
 * - API response time monitoring
 * - Automatic slow query detection
 * - Query optimization recommendations
 * - Performance regression detection
 * - Distributed tracing integration
 * - Prometheus metrics export
 */
class PerformanceMonitor {
public:
    /**
     * Constructor
     * 
     * @param db_connection Database connection string
     * @param slow_query_threshold_ms Threshold for slow query logging (default: 1000ms)
     */
    explicit PerformanceMonitor(
        const std::string& db_connection = "",
        int slow_query_threshold_ms = 1000
    );

    ~PerformanceMonitor();

    /**
     * Initialize performance monitor
     * 
     * @return true if successful
     */
    bool initialize();

    /**
     * Start tracking an operation
     * 
     * @param type Metric type
     * @param operation Operation name
     * @return Metric ID for ending the operation
     */
    std::string start_tracking(MetricType type, const std::string& operation);

    /**
     * End tracking an operation
     * 
     * @param metric_id Metric ID from start_tracking
     * @param success Whether operation succeeded
     * @param error_message Error message if failed
     * @param metadata Additional metadata
     */
    void end_tracking(
        const std::string& metric_id,
        bool success = true,
        const std::string& error_message = "",
        const std::map<std::string, std::string>& metadata = {}
    );

    /**
     * Track database query
     * 
     * @param query SQL query
     * @param duration_ms Execution time
     * @param rows_affected Rows affected/returned
     * @param success Whether query succeeded
     */
    void track_query(
        const std::string& query,
        int duration_ms,
        int rows_affected = 0,
        bool success = true
    );

    /**
     * Track API request
     * 
     * @param endpoint API endpoint
     * @param method HTTP method
     * @param status_code HTTP status code
     * @param duration_ms Response time
     */
    void track_api_request(
        const std::string& endpoint,
        const std::string& method,
        int status_code,
        int duration_ms
    );

    /**
     * Analyze query performance
     * 
     * @param query SQL query
     * @return Query execution plan with optimization suggestions
     */
    QueryExecutionPlan analyze_query(const std::string& query);

    /**
     * Get slow queries
     * 
     * @param limit Maximum number of queries to return
     * @return Vector of slow queries
     */
    std::vector<SlowQueryLog> get_slow_queries(int limit = 100);

    /**
     * Get performance statistics for an operation
     * 
     * @param operation Operation name
     * @param time_window_minutes Time window for statistics
     * @return Performance statistics
     */
    PerformanceStats get_statistics(
        const std::string& operation,
        int time_window_minutes = 60
    );

    /**
     * Get all performance statistics
     * 
     * @param time_window_minutes Time window
     * @return Map of operation -> statistics
     */
    std::map<std::string, PerformanceStats> get_all_statistics(int time_window_minutes = 60);

    /**
     * Get performance summary
     * 
     * @return JSON performance summary
     */
    std::string get_performance_summary();

    /**
     * Detect performance regressions
     * Compares current performance with baseline
     * 
     * @param operation Operation to check
     * @param threshold_percentage Regression threshold (e.g., 20 = 20% slower)
     * @return true if regression detected
     */
    bool detect_regression(const std::string& operation, double threshold_percentage = 20.0);

    /**
     * Set performance baseline
     * Captures current performance as baseline for regression detection
     * 
     * @param operation Operation name (empty for all operations)
     */
    void set_baseline(const std::string& operation = "");

    /**
     * Get optimization recommendations
     * 
     * @return Vector of optimization recommendations
     */
    std::vector<std::string> get_optimization_recommendations();

    /**
     * Get missing database indexes
     * Analyzes queries to find missing indexes
     * 
     * @return Vector of index recommendations
     */
    std::vector<std::string> get_missing_indexes();

    /**
     * Get unused database indexes
     * Finds indexes that are never used
     * 
     * @return Vector of unused index names
     */
    std::vector<std::string> get_unused_indexes();

    /**
     * Export metrics to Prometheus format
     * 
     * @return Prometheus-formatted metrics
     */
    std::string export_prometheus_metrics();

    /**
     * Get performance alerts
     * Returns operations that exceed performance thresholds
     * 
     * @return Vector of performance alert messages
     */
    std::vector<std::string> get_performance_alerts();

    /**
     * Clear old metrics
     * Removes metrics older than retention period
     * 
     * @param retention_hours Retention period in hours
     * @return Number of metrics cleared
     */
    int clear_old_metrics(int retention_hours = 24);

    /**
     * Set slow query threshold
     * 
     * @param threshold_ms Threshold in milliseconds
     */
    void set_slow_query_threshold(int threshold_ms);

    /**
     * Enable/disable automatic query analysis
     * 
     * @param enabled Whether to enable
     */
    void set_auto_analysis_enabled(bool enabled);

private:
    std::string db_connection_;
    int slow_query_threshold_ms_;
    bool auto_analysis_enabled_;
    bool initialized_;

    std::vector<PerformanceMetric> metrics_;
    std::map<std::string, PerformanceMetric> active_operations_;
    std::map<std::string, PerformanceStats> baselines_;
    std::vector<SlowQueryLog> slow_queries_;
    
    std::mutex metrics_mutex_;
    std::mutex queries_mutex_;

    /**
     * Generate metric ID
     */
    std::string generate_metric_id();

    /**
     * Calculate performance level
     */
    PerformanceLevel calculate_performance_level(int duration_ms, MetricType type);

    /**
     * Calculate percentile
     */
    double calculate_percentile(const std::vector<int>& durations, double percentile);

    /**
     * Analyze query execution plan
     */
    QueryExecutionPlan analyze_query_plan(const std::string& query);

    /**
     * Extract table names from query
     */
    std::vector<std::string> extract_table_names(const std::string& query);

    /**
     * Check if query needs index
     */
    bool query_needs_index(const std::string& query);

    /**
     * Get query statistics from database
     */
    std::map<std::string, int> get_query_statistics(const std::string& query);

    /**
     * Log slow query
     */
    void log_slow_query(const SlowQueryLog& log);

    /**
     * Persist metrics to database
     */
    bool persist_metrics();

    /**
     * Load metrics from database
     */
    bool load_metrics();
};

/**
 * RAII Performance Tracker
 * Automatically tracks operation duration
 */
class PerformanceTracker {
public:
    PerformanceTracker(
        PerformanceMonitor* monitor,
        MetricType type,
        const std::string& operation
    );

    ~PerformanceTracker();

    void set_success(bool success);
    void set_error(const std::string& error);
    void add_metadata(const std::string& key, const std::string& value);

private:
    PerformanceMonitor* monitor_;
    std::string metric_id_;
    bool success_;
    std::string error_message_;
    std::map<std::string, std::string> metadata_;
};

} // namespace regulens

#endif // REGULENS_PERFORMANCE_MONITOR_HPP

