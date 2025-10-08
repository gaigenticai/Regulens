#include "metrics_collector.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace regulens {

MetricsCollector::MetricsCollector()
    : running_(false),
      collection_interval_(std::chrono::milliseconds(1000)),
      collection_cycles_(0),
      last_collection_time_(std::chrono::system_clock::now()) {
}

MetricsCollector::~MetricsCollector() {
    stop_collection();
}

bool MetricsCollector::start_collection() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        // Already running
        return false;
    }

    try {
        collection_thread_ = std::thread(&MetricsCollector::collection_thread, this);
        return true;
    } catch (const std::exception&) {
        running_ = false;
        return false;
    }
}

void MetricsCollector::stop_collection() {
    bool expected = true;
    if (running_.compare_exchange_strong(expected, false)) {
        if (collection_thread_.joinable()) {
            collection_thread_.join();
        }
    }
}

bool MetricsCollector::register_gauge(const std::string& name, std::function<double()> getter) {
    if (name.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(metrics_mutex_);

    // Check if metric already exists
    if (gauge_getters_.find(name) != gauge_getters_.end() ||
        counters_.find(name) != counters_.end() ||
        histograms_.find(name) != histograms_.end()) {
        return false;
    }

    gauge_getters_[name] = getter;
    gauges_[name] = 0.0;

    return true;
}

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

    counters_[name] = 0.0;

    return true;
}

bool MetricsCollector::register_histogram(const std::string& name, const std::vector<double>& bucket_bounds) {
    if (name.empty() || bucket_bounds.empty()) {
        return false;
    }

    // Validate bucket bounds are sorted
    for (size_t i = 1; i < bucket_bounds.size(); ++i) {
        if (bucket_bounds[i] <= bucket_bounds[i-1]) {
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

    histograms_[name] = std::make_unique<HistogramData>(bucket_bounds);

    return true;
}

void MetricsCollector::set_gauge(const std::string& name, double value) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    auto it = gauges_.find(name);
    if (it != gauges_.end()) {
        it->second = value;
    }
}

void MetricsCollector::increment_counter(const std::string& name, double value) {
    if (value < 0) {
        return; // Counters can only increase
    }

    std::lock_guard<std::mutex> lock(metrics_mutex_);

    auto it = counters_.find(name);
    if (it != counters_.end()) {
        double current = it->second.load();
        double new_value = current + value;
        it->second.store(new_value);
    }
}

void MetricsCollector::observe_histogram(const std::string& name, double value) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    auto it = histograms_.find(name);
    if (it != histograms_.end()) {
        it->second->observe(value);
    }
}

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

    return 0.0;
}

nlohmann::json MetricsCollector::get_all_metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    nlohmann::json result = nlohmann::json::object();

    // Add gauges
    for (const auto& [name, value] : gauges_) {
        result[name] = {
            {"type", "gauge"},
            {"value", value.load()},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
    }

    // Add counters
    for (const auto& [name, value] : counters_) {
        result[name] = {
            {"type", "counter"},
            {"value", value.load()},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
    }

    // Add histograms
    for (const auto& [name, histogram] : histograms_) {
        nlohmann::json buckets = nlohmann::json::array();
        for (const auto& bucket : histogram->buckets) {
            buckets.push_back({
                {"upper_bound", bucket.upper_bound},
                {"count", bucket.count.load()}
            });
        }

        result[name] = {
            {"type", "histogram"},
            {"sample_count", histogram->sample_count.load()},
            {"sum", histogram->sum.load()},
            {"buckets", buckets},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
    }

    return result;
}

nlohmann::json MetricsCollector::get_component_metrics(const std::string& component_name) const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    nlohmann::json result = nlohmann::json::object();
    std::string prefix = component_name + "_";

    // Filter gauges
    for (const auto& [name, value] : gauges_) {
        if (name.find(prefix) == 0 || extract_component(name) == component_name) {
            result[name] = {
                {"type", "gauge"},
                {"value", value.load()},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
        }
    }

    // Filter counters
    for (const auto& [name, value] : counters_) {
        if (name.find(prefix) == 0 || extract_component(name) == component_name) {
            result[name] = {
                {"type", "counter"},
                {"value", value.load()},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
        }
    }

    // Filter histograms
    for (const auto& [name, histogram] : histograms_) {
        if (name.find(prefix) == 0 || extract_component(name) == component_name) {
            nlohmann::json buckets = nlohmann::json::array();
            for (const auto& bucket : histogram->buckets) {
                buckets.push_back({
                    {"upper_bound", bucket.upper_bound},
                    {"count", bucket.count.load()}
                });
            }

            result[name] = {
                {"type", "histogram"},
                {"sample_count", histogram->sample_count.load()},
                {"sum", histogram->sum.load()},
                {"buckets", buckets},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
        }
    }

    return result;
}

void MetricsCollector::reset_counter(const std::string& name) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    auto it = counters_.find(name);
    if (it != counters_.end()) {
        it->second = 0.0;
    }
}

bool MetricsCollector::remove_metric(const std::string& name) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    // Try to remove from each storage
    bool removed = false;

    if (gauges_.erase(name) > 0) {
        gauge_getters_.erase(name);
        removed = true;
    }

    if (counters_.erase(name) > 0) {
        removed = true;
    }

    if (histograms_.erase(name) > 0) {
        removed = true;
    }

    return removed;
}

std::vector<std::string> MetricsCollector::get_metric_names() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    std::vector<std::string> names;

    // Collect all metric names
    for (const auto& [name, _] : gauges_) {
        names.push_back(name);
    }

    for (const auto& [name, _] : counters_) {
        names.push_back(name);
    }

    for (const auto& [name, _] : histograms_) {
        names.push_back(name);
    }

    // Sort for consistent output
    std::sort(names.begin(), names.end());

    return names;
}

std::string MetricsCollector::export_prometheus() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    std::stringstream output;

    // Export gauges
    for (const auto& [name, value] : gauges_) {
        output << "# HELP " << name << " Gauge metric\n";
        output << "# TYPE " << name << " gauge\n";
        output << name << " " << std::fixed << std::setprecision(6) << value.load() << "\n";
    }

    // Export counters
    for (const auto& [name, value] : counters_) {
        output << "# HELP " << name << " Counter metric\n";
        output << "# TYPE " << name << " counter\n";
        output << name << " " << std::fixed << std::setprecision(6) << value.load() << "\n";
    }

    // Export histograms
    for (const auto& [name, hist] : histograms_) {
        output << "# HELP " << name << " Histogram metric\n";
        output << "# TYPE " << name << " histogram\n";

        // Buckets
        for (const auto& bucket : hist->buckets) {
            output << name << "_bucket{le=\"" << bucket.upper_bound << "\"} "
                   << bucket.count.load() << "\n";
        }
        output << name << "_bucket{le=\"+Inf\"} " << hist->sample_count.load() << "\n";

        // Sum and count
        output << name << "_sum " << std::fixed << std::setprecision(6) << hist->sum.load() << "\n";
        output << name << "_count " << hist->sample_count.load() << "\n";
    }

    return output.str();
}

void MetricsCollector::collection_thread() {
    while (running_) {
        std::this_thread::sleep_for(collection_interval_);

        if (!running_) break;

        update_gauges();

        collection_cycles_++;
        last_collection_time_ = std::chrono::system_clock::now();
    }
}

void MetricsCollector::update_gauges() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    for (const auto& [name, getter] : gauge_getters_) {
        try {
            double value = getter();
            auto it = gauges_.find(name);
            if (it != gauges_.end()) {
                it->second = value;
            }
        } catch (const std::exception&) {
            // Log error but continue
        }
    }
}

MetricType MetricsCollector::get_metric_type(const std::string& name) const {
    if (gauges_.find(name) != gauges_.end()) return MetricType::GAUGE;
    if (counters_.find(name) != counters_.end()) return MetricType::COUNTER;
    if (histograms_.find(name) != histograms_.end()) return MetricType::HISTOGRAM;
    return MetricType::GAUGE; // Default
}

std::string MetricsCollector::extract_component(const std::string& metric_name) const {
    size_t dot_pos = metric_name.find('.');
    if (dot_pos != std::string::npos) {
        return metric_name.substr(0, dot_pos);
    }
    return "unknown";
}

} // namespace regulens


