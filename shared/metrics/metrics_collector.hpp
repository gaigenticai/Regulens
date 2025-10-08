#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <mutex>
#include <functional>
#include <chrono>
#include <thread>
#include <nlohmann/json.hpp>

namespace regulens {

/**
 * @brief Metric types supported by the collector
 */
enum class MetricType {
    GAUGE,      // Single value that can go up or down
    COUNTER,    // Monotonically increasing value
    HISTOGRAM,  // Distribution of values
    SUMMARY     // Similar to histogram but with quantiles
};

/**
 * @brief Metric value container
 */
struct MetricValue {
    MetricType type;
    double value;
    std::chrono::system_clock::time_point timestamp;

    MetricValue(MetricType t, double v)
        : type(t), value(v), timestamp(std::chrono::system_clock::now()) {}
};

/**
 * @brief Histogram bucket for distribution metrics
 */
struct HistogramBucket {
    double upper_bound;
    std::atomic<size_t> count{0};

    HistogramBucket(double bound) : upper_bound(bound) {}

    // Copy constructor - atomic can't be copied, so we copy the value
    HistogramBucket(const HistogramBucket& other)
        : upper_bound(other.upper_bound), count(other.count.load()) {}

    // Move constructor
    HistogramBucket(HistogramBucket&& other) noexcept
        : upper_bound(other.upper_bound), count(other.count.load()) {}

    // Copy assignment
    HistogramBucket& operator=(const HistogramBucket& other) {
        if (this != &other) {
            upper_bound = other.upper_bound;
            count.store(other.count.load());
        }
        return *this;
    }

    // Move assignment
    HistogramBucket& operator=(HistogramBucket&& other) noexcept {
        if (this != &other) {
            upper_bound = other.upper_bound;
            count.store(other.count.load());
        }
        return *this;
    }
};

/**
 * @brief Histogram metric data
 */
struct HistogramData {
    std::vector<HistogramBucket> buckets;
    std::atomic<size_t> sample_count{0};
    std::atomic<double> sum{0.0};

    HistogramData(const std::vector<double>& bounds) {
        buckets.reserve(bounds.size());
        for (double bound : bounds) {
            buckets.emplace_back(bound);
        }
    }

    void observe(double value) {
        sample_count.fetch_add(1, std::memory_order_relaxed);
        // Atomic double increment using compare-and-swap
        double old_sum = sum.load(std::memory_order_relaxed);
        double new_sum;
        do {
            new_sum = old_sum + value;
        } while (!sum.compare_exchange_weak(old_sum, new_sum, std::memory_order_release, std::memory_order_relaxed));

        for (auto& bucket : buckets) {
            if (value <= bucket.upper_bound) {
                bucket.count.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }
};

/**
 * @brief Metrics collector for system monitoring
 *
 * Provides thread-safe collection and exposure of system metrics
 * for monitoring agent performance, health, and operational status.
 */
class MetricsCollector {
public:
    MetricsCollector();
    ~MetricsCollector();

    // Prevent copying
    MetricsCollector(const MetricsCollector&) = delete;
    MetricsCollector& operator=(const MetricsCollector&) = delete;

    /**
     * @brief Start metrics collection
     * @return true if started successfully
     */
    bool start_collection();

    /**
     * @brief Stop metrics collection
     */
    void stop_collection();

    /**
     * @brief Register a gauge metric
     * @param name Metric name
     * @param getter Function that returns the current value
     * @return true if registered successfully
     */
    bool register_gauge(const std::string& name, std::function<double()> getter);

    /**
     * @brief Register a counter metric
     * @param name Metric name
     * @return true if registered successfully
     */
    bool register_counter(const std::string& name);

    /**
     * @brief Register a histogram metric
     * @param name Metric name
     * @param bucket_bounds Upper bounds for histogram buckets
     * @return true if registered successfully
     */
    bool register_histogram(const std::string& name, const std::vector<double>& bucket_bounds);

    /**
     * @brief Set gauge value
     * @param name Metric name
     * @param value New value
     */
    void set_gauge(const std::string& name, double value);

    /**
     * @brief Increment counter
     * @param name Metric name
     * @param value Amount to increment (default 1)
     */
    void increment_counter(const std::string& name, double value = 1.0);

    /**
     * @brief Observe histogram value
     * @param name Metric name
     * @param value Value to observe
     */
    void observe_histogram(const std::string& name, double value);

    /**
     * @brief Get current value of a metric
     * @param name Metric name
     * @return Current value or 0 if not found
     */
    double get_value(const std::string& name) const;

    /**
     * @brief Get all metrics as JSON
     * @return JSON representation of all metrics
     */
    nlohmann::json get_all_metrics() const;

    /**
     * @brief Get metrics for a specific component
     * @param component_name Component name
     * @return JSON representation of component metrics
     */
    nlohmann::json get_component_metrics(const std::string& component_name) const;

    /**
     * @brief Reset a counter to zero
     * @param name Metric name
     */
    void reset_counter(const std::string& name);

    /**
     * @brief Remove a metric
     * @param name Metric name
     * @return true if removed successfully
     */
    bool remove_metric(const std::string& name);

    /**
     * @brief Get list of registered metric names
     * @return Vector of metric names
     */
    std::vector<std::string> get_metric_names() const;

    /**
     * @brief Export metrics in Prometheus format
     * @return Prometheus-formatted metrics string
     */
    std::string export_prometheus() const;

private:
    /**
     * @brief Collection thread function
     */
    void collection_thread();

    /**
     * @brief Update gauge metrics from registered getters
     */
    void update_gauges();

    /**
     * @brief Get metric type from name
     * @param name Metric name
     * @return Metric type
     */
    MetricType get_metric_type(const std::string& name) const;

    /**
     * @brief Extract component name from metric name
     * @param metric_name Full metric name
     * @return Component name
     */
    std::string extract_component(const std::string& metric_name) const;

    // Thread safety
    mutable std::mutex metrics_mutex_;

    // Metric storage
    std::unordered_map<std::string, std::atomic<double>> gauges_;
    std::unordered_map<std::string, std::atomic<double>> counters_;
    std::unordered_map<std::string, std::unique_ptr<HistogramData>> histograms_;
    std::unordered_map<std::string, std::function<double()>> gauge_getters_;

    // Collection control
    std::atomic<bool> running_;
    std::thread collection_thread_;
    std::chrono::milliseconds collection_interval_;

    // Collection statistics
    std::atomic<size_t> collection_cycles_;
    std::chrono::system_clock::time_point last_collection_time_;
};

/**
 * @brief Scoped timer for measuring operation duration
 */
class ScopedTimer {
public:
    ScopedTimer(MetricsCollector& collector, const std::string& histogram_name)
        : collector_(collector), histogram_name_(histogram_name),
          start_time_(std::chrono::high_resolution_clock::now()) {}

    ~ScopedTimer() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time_);
        collector_.observe_histogram(histogram_name_,
            static_cast<double>(duration.count()) / 1000.0); // Convert to milliseconds
    }

private:
    MetricsCollector& collector_;
    std::string histogram_name_;
    std::chrono::high_resolution_clock::time_point start_time_;
};

} // namespace regulens


