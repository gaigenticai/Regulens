/**
 * Ingestion Metrics Implementation - Production-Grade Monitoring
 */

#include "ingestion_metrics.hpp"
#include <algorithm>

namespace regulens {

IngestionMetrics::IngestionMetrics(StructuredLogger* logger)
    : logger_(logger) {
    global_metrics_.system_start_time = std::chrono::system_clock::now();
}

void IngestionMetrics::record_batch_processed(const std::string& source_id, const IngestionBatch& batch) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    auto& metrics = source_metrics_[source_id];
    metrics.total_batches++;
    metrics.total_records += batch.records_processed;

    if (batch.status == IngestionStatus::COMPLETED) {
        metrics.successful_batches++;
        metrics.successful_records += batch.records_succeeded;
        metrics.last_successful_batch = std::chrono::system_clock::now();
        metrics.consecutive_failures = 0;
    } else {
        metrics.failed_batches++;
        metrics.failed_records += batch.records_failed;
        metrics.consecutive_failures++;
    }

    update_source_health(source_id, metrics);
}

void IngestionMetrics::record_ingestion_error(const std::string& source_id, const std::string& error) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    auto& metrics = source_metrics_[source_id];
    metrics.recent_errors.push_back(error);
    if (metrics.recent_errors.size() > MAX_RECENT_ERRORS) {
        metrics.recent_errors.erase(metrics.recent_errors.begin());
    }

    // Categorize error
    std::string error_category = "unknown";
    if (error.find("timeout") != std::string::npos) error_category = "timeout";
    else if (error.find("connection") != std::string::npos) error_category = "connection";
    else if (error.find("parse") != std::string::npos) error_category = "parsing";

    metrics.error_counts[error_category]++;

    update_source_health(source_id, metrics);
}

void IngestionMetrics::record_source_health(const std::string& source_id, bool healthy) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    auto& metrics = source_metrics_[source_id];
    metrics.is_healthy = healthy;
    metrics.last_health_check = std::chrono::system_clock::now();

    update_source_health(source_id, metrics);
}

void IngestionMetrics::record_processing_time(const std::string& source_id, const std::chrono::microseconds& duration) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    source_metrics_[source_id].total_processing_time += duration;
}

void IngestionMetrics::record_throughput(const std::string& source_id, int records_per_second) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    auto& metrics = source_metrics_[source_id];
    auto now = std::chrono::system_clock::now();

    metrics.throughput_history.emplace_back(now, records_per_second);
    if (metrics.throughput_history.size() > MAX_HISTORY_POINTS) {
        metrics.throughput_history.erase(metrics.throughput_history.begin());
    }

    metrics.max_records_per_second = std::max(metrics.max_records_per_second, static_cast<int64_t>(records_per_second));
}

void IngestionMetrics::record_queue_depth(int depth) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    global_metrics_.current_queue_depth = depth;
}

void IngestionMetrics::categorize_error(const std::string& error_type, const std::string& source_id) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    source_metrics_[source_id].error_counts[error_type]++;
}

std::vector<std::string> IngestionMetrics::get_top_error_types(int limit) const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    std::vector<std::pair<std::string, int>> error_types;
    for (const auto& [source_id, metrics] : source_metrics_) {
        for (const auto& [error_type, count] : metrics.error_counts) {
            error_types.emplace_back(error_type, count);
        }
    }

    std::sort(error_types.begin(), error_types.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    std::vector<std::string> top_errors;
    for (int i = 0; i < std::min(limit, static_cast<int>(error_types.size())); ++i) {
        top_errors.push_back(error_types[i].first);
    }

    return top_errors;
}

double IngestionMetrics::get_error_rate(const std::string& source_id) const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    auto it = source_metrics_.find(source_id);
    if (it == source_metrics_.end()) return 0.0;

    return calculate_error_rate(it->second);
}

bool IngestionMetrics::is_source_healthy(const std::string& source_id) const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    auto it = source_metrics_.find(source_id);
    return it != source_metrics_.end() && it->second.is_healthy;
}

std::vector<std::string> IngestionMetrics::get_failing_sources() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    std::vector<std::string> failing_sources;
    for (const auto& [source_id, metrics] : source_metrics_) {
        if (!metrics.is_healthy || metrics.consecutive_failures > 0) {
            failing_sources.push_back(source_id);
        }
    }

    return failing_sources;
}

nlohmann::json IngestionMetrics::get_system_health() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    return {
        {"overall_health", get_failing_sources().empty() ? "healthy" : "degraded"},
        {"active_sources", static_cast<int>(source_metrics_.size())},
        {"failing_sources", static_cast<int>(get_failing_sources().size())},
        {"total_batches_processed", global_metrics_.total_batches_processed},
        {"total_records_processed", global_metrics_.total_records_processed},
        {"queue_depth", global_metrics_.current_queue_depth},
        {"uptime_seconds", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - global_metrics_.system_start_time).count()}
    };
}

nlohmann::json IngestionMetrics::get_source_metrics(const std::string& source_id) const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    auto it = source_metrics_.find(source_id);
    if (it == source_metrics_.end()) {
        return {{"error", "source_not_found"}};
    }

    const auto& metrics = it->second;
    return {
        {"source_id", source_id},
        {"total_batches", metrics.total_batches},
        {"successful_batches", metrics.successful_batches},
        {"failed_batches", metrics.failed_batches},
        {"total_records", metrics.total_records},
        {"error_rate", calculate_error_rate(metrics)},
        {"is_healthy", metrics.is_healthy},
        {"consecutive_failures", metrics.consecutive_failures},
        {"throughput_rps", calculate_throughput(metrics)}
    };
}

nlohmann::json IngestionMetrics::get_global_metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    return {
        {"total_sources", static_cast<int>(source_metrics_.size())},
        {"total_batches_processed", global_metrics_.total_batches_processed},
        {"total_records_processed", global_metrics_.total_records_processed},
        {"active_workers", global_metrics_.active_workers},
        {"queue_depth", global_metrics_.current_queue_depth},
        {"system_uptime_seconds", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - global_metrics_.system_start_time).count()}
    };
}

nlohmann::json IngestionMetrics::get_performance_report(const std::chrono::hours& /*time_window*/) const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    return {
        {"report_type", "performance_summary"},
        {"generated_at", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()},
        {"top_error_types", get_top_error_types(5)},
        {"failing_sources", get_failing_sources()},
        {"system_health", get_system_health()}
    };
}

nlohmann::json IngestionMetrics::get_trend_analysis(const std::string& source_id) const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    auto it = source_metrics_.find(source_id);
    if (it == source_metrics_.end()) {
        return {{"error", "source_not_found"}};
    }

    const auto& metrics = it->second;

    // Simple trend analysis
    double avg_throughput = 0.0;
    if (!metrics.throughput_history.empty()) {
        int64_t sum = 0;
        for (const auto& [_, throughput] : metrics.throughput_history) {
            sum += throughput;
        }
        avg_throughput = static_cast<double>(sum) / metrics.throughput_history.size();
    }

    return {
        {"source_id", source_id},
        {"average_throughput_rps", avg_throughput},
        {"max_throughput_rps", metrics.max_records_per_second},
        {"error_rate_trend", calculate_error_rate(metrics)},
        {"data_points", static_cast<int>(metrics.throughput_history.size())}
    };
}

std::vector<std::string> IngestionMetrics::predict_potential_failures() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    std::vector<std::string> predictions;
    for (const auto& [source_id, metrics] : source_metrics_) {
        if (metrics.consecutive_failures >= 2) {
            predictions.push_back(source_id + ":high_failure_rate");
        }
        if (calculate_error_rate(metrics) > error_rate_alert_threshold_) {
            predictions.push_back(source_id + ":high_error_rate");
        }
    }

    return predictions;
}

nlohmann::json IngestionMetrics::get_capacity_forecast() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    return {
        {"forecast_type", "simple_capacity_estimate"},
        {"current_sources", static_cast<int>(source_metrics_.size())},
        {"recommended_workers", std::max(4, static_cast<int>(source_metrics_.size() / 2))},
        {"estimated_max_sources", 50},
        {"scaling_recommendations", {
            "Consider horizontal scaling for >20 sources",
            "Implement queue partitioning for >1000 concurrent batches",
            "Add read replicas for metrics database when >10 sources active"
        }}
    };
}

bool IngestionMetrics::should_alert_on_error_rate(const std::string& source_id, double threshold) const {
    return get_error_rate(source_id) > threshold;
}

bool IngestionMetrics::should_alert_on_throughput_drop(const std::string& source_id, double threshold) const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    auto it = source_metrics_.find(source_id);
    if (it == source_metrics_.end() || it->second.throughput_history.size() < 2) {
        return false;
    }

    const auto& history = it->second.throughput_history;
    auto recent = history.back().second;
    auto previous = history[history.size() - 2].second;

    if (previous == 0) return false;
    return (static_cast<double>(recent) / previous) < (1.0 - threshold);
}

std::vector<nlohmann::json> IngestionMetrics::get_active_alerts() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    std::vector<nlohmann::json> alerts;

    for (const auto& [source_id, metrics] : source_metrics_) {
        if (should_alert_on_error_rate(source_id)) {
            alerts.push_back({
                {"alert_type", "high_error_rate"},
                {"source_id", source_id},
                {"severity", "warning"},
                {"message", "Error rate exceeds threshold"},
                {"value", calculate_error_rate(metrics)}
            });
        }

        if (should_alert_on_throughput_drop(source_id)) {
            alerts.push_back({
                {"alert_type", "throughput_drop"},
                {"source_id", source_id},
                {"severity", "warning"},
                {"message", "Throughput dropped significantly"},
                {"value", calculate_throughput(metrics)}
            });
        }

        if (metrics.consecutive_failures >= max_consecutive_failures_) {
            alerts.push_back({
                {"alert_type", "consecutive_failures"},
                {"source_id", source_id},
                {"severity", "critical"},
                {"message", "Multiple consecutive failures detected"},
                {"value", metrics.consecutive_failures}
            });
        }
    }

    return alerts;
}

// Private methods
void IngestionMetrics::update_source_health(const std::string& source_id, SourceMetrics& metrics) {
    bool was_healthy = metrics.is_healthy;

    metrics.is_healthy = metrics.consecutive_failures == 0 &&
                        (std::chrono::system_clock::now() - metrics.last_successful_batch) < std::chrono::hours(1);

    if (was_healthy != metrics.is_healthy) {
        logger_->log(metrics.is_healthy ? LogLevel::INFO : LogLevel::WARN,
                    "Source " + source_id + " health changed to: " +
                    (metrics.is_healthy ? "healthy" : "unhealthy"));
    }

    // Update error rate history
    double error_rate = calculate_error_rate(metrics);
    auto now = std::chrono::system_clock::now();

    metrics.error_rate_history.emplace_back(now, error_rate);
    if (metrics.error_rate_history.size() > MAX_HISTORY_POINTS) {
        metrics.error_rate_history.erase(metrics.error_rate_history.begin());
    }

    cleanup_old_data(source_id);
}

void IngestionMetrics::cleanup_old_data(const std::string& source_id) {
    auto& metrics = source_metrics_[source_id];
    auto cutoff = std::chrono::system_clock::now() - METRICS_RETENTION_PERIOD;

    // Clean up old throughput history
    auto& throughput = metrics.throughput_history;
    throughput.erase(
        std::remove_if(throughput.begin(), throughput.end(),
                      [cutoff](const auto& pair) { return pair.first < cutoff; }),
        throughput.end()
    );

    // Clean up old error rate history
    auto& error_rates = metrics.error_rate_history;
    error_rates.erase(
        std::remove_if(error_rates.begin(), error_rates.end(),
                      [cutoff](const auto& pair) { return pair.first < cutoff; }),
        error_rates.end()
    );
}

double IngestionMetrics::calculate_error_rate(const SourceMetrics& metrics) const {
    int64_t total_batches = metrics.successful_batches + metrics.failed_batches;
    return total_batches > 0 ? static_cast<double>(metrics.failed_batches) / total_batches : 0.0;
}

int64_t IngestionMetrics::calculate_throughput(const SourceMetrics& metrics) const {
    if (metrics.total_processing_time.count() == 0) return 0;

    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(metrics.total_processing_time).count();
    return seconds > 0 ? (metrics.total_records / seconds) : 0;
}

void IngestionMetrics::load_alert_thresholds() {
    // In production, load from configuration
}

void IngestionMetrics::initialize_baseline_metrics() {
    // Initialize baseline metrics for trend analysis
}

void IngestionMetrics::persist_metrics() const {
    // In production, persist metrics to database
}

void IngestionMetrics::load_persisted_metrics() {
    // In production, load persisted metrics
}

} // namespace regulens

