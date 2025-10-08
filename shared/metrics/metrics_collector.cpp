#include "metrics_collector.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <thread>

namespace regulens {

namespace {

// Default bucket boundaries for histograms (milliseconds)
constexpr std::array<double, 11> DEFAULT_LATENCY_BUCKETS = {
    0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0
};

// Default collection interval
constexpr auto DEFAULT_COLLECTION_INTERVAL = std::chrono::milliseconds(1000);

/**
 * @brief Sanitize metric name for Prometheus format
 * Ensures metric names comply with Prometheus naming conventions
 */
std::string sanitize_metric_name(const std::string& name) {
    std::string sanitized = name;

    // Replace invalid characters with underscores
    std::replace_if(sanitized.begin(), sanitized.end(),
                   [](char c) { return !std::isalnum(c) && c != '_' && c != ':'; },
                   '_');

    // Ensure it starts with a letter or underscore
    if (!sanitized.empty() && std::isdigit(sanitized[0])) {
        sanitized = "_" + sanitized;
    }

    return sanitized;
}

/**
 * @brief Format double value for Prometheus export
 * Handles special values (NaN, Inf) and removes trailing zeros
 */
std::string format_double(double value) {
    if (std::isnan(value)) {
        return "NaN";
    }
    if (std::isinf(value)) {
        return value > 0 ? "+Inf" : "-Inf";
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6) << value;
    std::string result = oss.str();

    // Remove trailing zeros after decimal point
    if (result.find('.') != std::string::npos) {
        result.erase(result.find_last_not_of('0') + 1);
        if (result.back() == '.') {
            result.pop_back();
        }
    }

    return result;
}

/**
 * @brief Calculate quantile from sorted values
 * Uses linear interpolation between closest ranks
 */
double calculate_quantile(const std::vector<double>& sorted_values, double quantile) {
    if (sorted_values.empty()) {
        return 0.0;
    }

    if (sorted_values.size() == 1) {
        return sorted_values[0];
    }

    double index = quantile * (sorted_values.size() - 1);
    size_t lower_index = static_cast<size_t>(std::floor(index));
    size_t upper_index = static_cast<size_t>(std::ceil(index));

    if (lower_index == upper_index) {
        return sorted_values[lower_index];
    }

    double weight = index - lower_index;
    return sorted_values[lower_index] * (1.0 - weight) + sorted_values[upper_index] * weight;
}

/**
 * @brief Format timestamp as ISO 8601 string
 */
std::string format_timestamp(const std::chrono::system_clock::time_point& tp) {
    auto time_t_value = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t_value), "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return oss.str();
}

/**
 * @brief Escape special characters for InfluxDB line protocol
 */
std::string escape_influx_tag(const std::string& value) {
    std::string result;
    result.reserve(value.size());

    for (char c : value) {
        if (c == ' ' || c == ',' || c == '=' || c == '\\') {
            result += '\\';
        }
        result += c;
    }

    return result;
}

} // anonymous namespace

// Constructor
MetricsCollector::MetricsCollector()
    : running_(false),
      collection_interval_(DEFAULT_COLLECTION_INTERVAL),
      collection_cycles_(0),
      last_collection_time_(std::chrono::system_clock::now()) {
}

// Destructor
MetricsCollector::~MetricsCollector() {
    stop_collection();
}

// Start metrics collection
bool MetricsCollector::start_collection() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        // Already running
        return false;
    }

    try {
        collection_thread_ = std::thread(&MetricsCollector::collection_thread, this);
        return true;
    } catch (const std::exception& e) {
        running_.store(false);
        return false;
    }
}

// Stop metrics collection
void MetricsCollector::stop_collection() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) {
        // Not running
        return;
    }

    if (collection_thread_.joinable()) {
        collection_thread_.join();
    }
}

// Register a gauge metric
bool MetricsCollector::register_gauge(const std::string& name, std::function<double()> getter) {
    if (name.empty() || !getter) {
        return false;
    }

    std::lock_guard<std::mutex> lock(metrics_mutex_);

    // Check if metric already exists
    if (gauges_.find(name) != gauges_.end() ||
        counters_.find(name) != counters_.end() ||
        histograms_.find(name) != histograms_.end()) {
        return false;
    }

    try {
        gauges_[name].store(0.0);
        gauge_getters_[name] = std::move(getter);
        return true;
    } catch (const std::exception& e) {
        // Clean up on error
        gauges_.erase(name);
        gauge_getters_.erase(name);
        return false;
    }
}

// Register a counter metric
bool MetricsCollector::register_counter(const std::string& name) {
    if (name.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(metrics_mutex_);

    // Check if metric already exists
    if (gauges_.find(name) != gauges_.end() ||
        counters_.find(name) != counters_.end() ||
        histograms_.find(name) != histograms_.end()) {
        return false;
    }

    try {
        counters_[name].store(0.0);
        return true;
    } catch (const std::exception& e) {
        counters_.erase(name);
        return false;
    }
}

// Register a histogram metric
bool MetricsCollector::register_histogram(const std::string& name, const std::vector<double>& bucket_bounds) {
    if (name.empty() || bucket_bounds.empty()) {
        return false;
    }

    // Validate bucket bounds are sorted and positive
    for (size_t i = 0; i < bucket_bounds.size(); ++i) {
        if (bucket_bounds[i] <= 0 || std::isnan(bucket_bounds[i]) || std::isinf(bucket_bounds[i])) {
            return false;
        }
        if (i > 0 && bucket_bounds[i] <= bucket_bounds[i-1]) {
            return false;
        }
    }

    std::lock_guard<std::mutex> lock(metrics_mutex_);

    // Check if metric already exists
    if (gauges_.find(name) != gauges_.end() ||
        counters_.find(name) != counters_.end() ||
        histograms_.find(name) != histograms_.end()) {
        return false;
    }

    try {
        // Create histogram with +Inf bucket at the end
        std::vector<double> bounds = bucket_bounds;
        bounds.push_back(std::numeric_limits<double>::infinity());

        histograms_[name] = std::make_unique<HistogramData>(bounds);
        return true;
    } catch (const std::exception& e) {
        histograms_.erase(name);
        return false;
    }
}

// Set gauge value
void MetricsCollector::set_gauge(const std::string& name, double value) {
    if (std::isnan(value)) {
        return;
    }

    std::lock_guard<std::mutex> lock(metrics_mutex_);

    auto it = gauges_.find(name);
    if (it != gauges_.end()) {
        it->second.store(value);
    }
}

// Increment counter
void MetricsCollector::increment_counter(const std::string& name, double value) {
    if (value < 0 || std::isnan(value) || std::isinf(value)) {
        return;
    }

    std::lock_guard<std::mutex> lock(metrics_mutex_);

    auto it = counters_.find(name);
    if (it != counters_.end()) {
        double current = it->second.load();
        while (!it->second.compare_exchange_weak(current, current + value)) {
            // Retry until successful
        }
    }
}

// Observe histogram value
void MetricsCollector::observe_histogram(const std::string& name, double value) {
    if (std::isnan(value) || std::isinf(value)) {
        return;
    }

    std::lock_guard<std::mutex> lock(metrics_mutex_);

    auto it = histograms_.find(name);
    if (it != histograms_.end() && it->second) {
        it->second->observe(value);
    }
}

// Get current value of a metric
double MetricsCollector::get_value(const std::string& name) const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    // Check gauges
    auto gauge_it = gauges_.find(name);
    if (gauge_it != gauges_.end()) {
        return gauge_it->second.load();
    }

    // Check counters
    auto counter_it = counters_.find(name);
    if (counter_it != counters_.end()) {
        return counter_it->second.load();
    }

    // Check histograms - return sample count
    auto hist_it = histograms_.find(name);
    if (hist_it != histograms_.end() && hist_it->second) {
        return static_cast<double>(hist_it->second->sample_count.load());
    }

    return 0.0;
}

// Get all metrics as JSON
nlohmann::json MetricsCollector::get_all_metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    nlohmann::json result;
    auto now = std::chrono::system_clock::now();
    result["timestamp"] = std::chrono::system_clock::to_time_t(now);
    result["timestamp_iso"] = format_timestamp(now);
    result["collection_cycles"] = collection_cycles_.load();

    // Add gauges
    nlohmann::json gauges_json = nlohmann::json::object();
    for (const auto& [name, value] : gauges_) {
        gauges_json[name] = {
            {"type", "gauge"},
            {"value", value.load()}
        };
    }
    if (!gauges_json.empty()) {
        result["gauges"] = gauges_json;
    }

    // Add counters
    nlohmann::json counters_json = nlohmann::json::object();
    for (const auto& [name, value] : counters_) {
        counters_json[name] = {
            {"type", "counter"},
            {"value", value.load()}
        };
    }
    if (!counters_json.empty()) {
        result["counters"] = counters_json;
    }

    // Add histograms
    nlohmann::json histograms_json = nlohmann::json::object();
    for (const auto& [name, histogram] : histograms_) {
        if (!histogram) continue;

        nlohmann::json hist_data;
        hist_data["type"] = "histogram";
        hist_data["sample_count"] = histogram->sample_count.load();
        hist_data["sum"] = histogram->sum.load();

        // Calculate mean
        size_t count = histogram->sample_count.load();
        if (count > 0) {
            hist_data["mean"] = histogram->sum.load() / count;
        } else {
            hist_data["mean"] = 0.0;
        }

        // Add buckets
        nlohmann::json buckets = nlohmann::json::array();
        for (const auto& bucket : histogram->buckets) {
            nlohmann::json bucket_data;
            if (std::isinf(bucket.upper_bound)) {
                bucket_data["upper_bound"] = "+Inf";
            } else {
                bucket_data["upper_bound"] = bucket.upper_bound;
            }
            bucket_data["count"] = bucket.count.load();
            buckets.push_back(bucket_data);
        }
        hist_data["buckets"] = buckets;

        histograms_json[name] = hist_data;
    }
    if (!histograms_json.empty()) {
        result["histograms"] = histograms_json;
    }

    return result;
}

// Get metrics for a specific component
nlohmann::json MetricsCollector::get_component_metrics(const std::string& component_name) const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    nlohmann::json result;
    auto now = std::chrono::system_clock::now();
    result["component"] = component_name;
    result["timestamp"] = std::chrono::system_clock::to_time_t(now);
    result["timestamp_iso"] = format_timestamp(now);

    // Filter metrics by component name prefix
    std::string prefix = component_name + "_";

    // Add matching gauges
    nlohmann::json gauges_json = nlohmann::json::object();
    for (const auto& [name, value] : gauges_) {
        if (name.find(prefix) == 0 || extract_component(name) == component_name) {
            gauges_json[name] = {
                {"type", "gauge"},
                {"value", value.load()}
            };
        }
    }
    if (!gauges_json.empty()) {
        result["gauges"] = gauges_json;
    }

    // Add matching counters
    nlohmann::json counters_json = nlohmann::json::object();
    for (const auto& [name, value] : counters_) {
        if (name.find(prefix) == 0 || extract_component(name) == component_name) {
            counters_json[name] = {
                {"type", "counter"},
                {"value", value.load()}
            };
        }
    }
    if (!counters_json.empty()) {
        result["counters"] = counters_json;
    }

    // Add matching histograms
    nlohmann::json histograms_json = nlohmann::json::object();
    for (const auto& [name, histogram] : histograms_) {
        if ((name.find(prefix) == 0 || extract_component(name) == component_name) && histogram) {
            nlohmann::json hist_data;
            hist_data["type"] = "histogram";
            hist_data["sample_count"] = histogram->sample_count.load();
            hist_data["sum"] = histogram->sum.load();

            size_t count = histogram->sample_count.load();
            if (count > 0) {
                hist_data["mean"] = histogram->sum.load() / count;
            } else {
                hist_data["mean"] = 0.0;
            }

            nlohmann::json buckets = nlohmann::json::array();
            for (const auto& bucket : histogram->buckets) {
                nlohmann::json bucket_data;
                if (std::isinf(bucket.upper_bound)) {
                    bucket_data["upper_bound"] = "+Inf";
                } else {
                    bucket_data["upper_bound"] = bucket.upper_bound;
                }
                bucket_data["count"] = bucket.count.load();
                buckets.push_back(bucket_data);
            }
            hist_data["buckets"] = buckets;

            histograms_json[name] = hist_data;
        }
    }
    if (!histograms_json.empty()) {
        result["histograms"] = histograms_json;
    }

    return result;
}

// Reset a counter to zero
void MetricsCollector::reset_counter(const std::string& name) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    auto it = counters_.find(name);
    if (it != counters_.end()) {
        it->second.store(0.0);
    }
}

// Remove a metric
bool MetricsCollector::remove_metric(const std::string& name) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    bool removed = false;

    // Try to remove from each collection
    auto gauge_it = gauges_.find(name);
    if (gauge_it != gauges_.end()) {
        gauges_.erase(gauge_it);
        gauge_getters_.erase(name);
        removed = true;
    }

    auto counter_it = counters_.find(name);
    if (counter_it != counters_.end()) {
        counters_.erase(counter_it);
        removed = true;
    }

    auto hist_it = histograms_.find(name);
    if (hist_it != histograms_.end()) {
        histograms_.erase(hist_it);
        removed = true;
    }

    return removed;
}

// Get list of registered metric names
std::vector<std::string> MetricsCollector::get_metric_names() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    std::vector<std::string> names;
    names.reserve(gauges_.size() + counters_.size() + histograms_.size());

    for (const auto& [name, _] : gauges_) {
        names.push_back(name);
    }

    for (const auto& [name, _] : counters_) {
        names.push_back(name);
    }

    for (const auto& [name, _] : histograms_) {
        names.push_back(name);
    }

    std::sort(names.begin(), names.end());

    return names;
}

// Export metrics in Prometheus format
std::string MetricsCollector::export_prometheus() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    std::ostringstream output;

    // Export gauges
    for (const auto& [name, value] : gauges_) {
        std::string sanitized_name = sanitize_metric_name(name);
        output << "# TYPE " << sanitized_name << " gauge\n";
        output << sanitized_name << " " << format_double(value.load()) << "\n";
    }

    // Export counters
    for (const auto& [name, value] : counters_) {
        std::string sanitized_name = sanitize_metric_name(name);
        output << "# TYPE " << sanitized_name << " counter\n";
        output << sanitized_name << " " << format_double(value.load()) << "\n";
    }

    // Export histograms
    for (const auto& [name, histogram] : histograms_) {
        if (!histogram) continue;

        std::string sanitized_name = sanitize_metric_name(name);
        output << "# TYPE " << sanitized_name << " histogram\n";

        // Export buckets (cumulative)
        size_t cumulative_count = 0;
        for (const auto& bucket : histogram->buckets) {
            cumulative_count += bucket.count.load();

            output << sanitized_name << "_bucket{le=\"";
            if (std::isinf(bucket.upper_bound)) {
                output << "+Inf";
            } else {
                output << format_double(bucket.upper_bound);
            }
            output << "\"} " << cumulative_count << "\n";
        }

        // Export count and sum
        output << sanitized_name << "_count " << histogram->sample_count.load() << "\n";
        output << sanitized_name << "_sum " << format_double(histogram->sum.load()) << "\n";
    }

    return output.str();
}

// Collection thread function
void MetricsCollector::collection_thread() {
    while (running_.load()) {
        auto start_time = std::chrono::steady_clock::now();

        // Update gauge metrics from registered getters
        update_gauges();

        // Update collection statistics
        collection_cycles_++;
        last_collection_time_ = std::chrono::system_clock::now();

        // Sleep for the remaining interval
        auto end_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        auto remaining = collection_interval_ - elapsed;

        if (remaining.count() > 0) {
            std::this_thread::sleep_for(remaining);
        }
    }
}

// Update gauge metrics from registered getters
void MetricsCollector::update_gauges() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    for (const auto& [name, getter] : gauge_getters_) {
        try {
            double value = getter();
            if (!std::isnan(value)) {
                auto it = gauges_.find(name);
                if (it != gauges_.end()) {
                    it->second.store(value);
                }
            }
        } catch (const std::exception& e) {
            // Silently ignore errors from gauge getters
            // In production, you might want to log this
        }
    }
}

// Get metric type from name
MetricType MetricsCollector::get_metric_type(const std::string& name) const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    if (gauges_.find(name) != gauges_.end()) {
        return MetricType::GAUGE;
    }

    if (counters_.find(name) != counters_.end()) {
        return MetricType::COUNTER;
    }

    if (histograms_.find(name) != histograms_.end()) {
        return MetricType::HISTOGRAM;
    }

    // Default to gauge if not found
    return MetricType::GAUGE;
}

// Extract component name from metric name
std::string MetricsCollector::extract_component(const std::string& metric_name) const {
    // Try underscore separator first (component_metric_name)
    size_t underscore_pos = metric_name.find('_');
    if (underscore_pos != std::string::npos) {
        return metric_name.substr(0, underscore_pos);
    }

    // Try dot separator (component.metric.name)
    size_t dot_pos = metric_name.find('.');
    if (dot_pos != std::string::npos) {
        return metric_name.substr(0, dot_pos);
    }

    // No separator found, return the whole name
    return metric_name;
}

} // namespace regulens
