#include "metrics_collector.hpp"
#include <sstream>
#include <iomanip>

namespace regulens {

MetricsCollector::MetricsCollector()
    : running_(false), collection_interval_(std::chrono::seconds(30)),
      collection_cycles_(0), last_collection_time_(std::chrono::system_clock::now()) {}

MetricsCollector::~MetricsCollector() {
    stop_collection();
}

bool MetricsCollector::start_collection() {
    if (running_) {
        return true;
    }

    running_ = true;
    collection_thread_ = std::thread(&MetricsCollector::collection_thread, this);

    return true;
}

void MetricsCollector::stop_collection() {
    if (!running_) {
        return;
    }

    running_ = false;
    if (collection_thread_.joinable()) {
        collection_thread_.join();
    }
}

bool MetricsCollector::register_gauge(const std::string& name, std::function<double()> getter) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    if (gauges_.find(name) != gauges_.end() ||
        counters_.find(name) != counters_.end() ||
        histograms_.find(name) != histograms_.end()) {
        return false; // Metric already exists
    }

    gauges_[name] = 0.0;
    gauge_getters_[name] = std::move(getter);

    return true;
}

bool MetricsCollector::register_counter(const std::string& name) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    if (gauges_.find(name) != gauges_.end() ||
        counters_.find(name) != counters_.end() ||
        histograms_.find(name) != histograms_.end()) {
        return false; // Metric already exists
    }

    counters_[name] = 0.0;

    return true;
}

bool MetricsCollector::register_histogram(const std::string& name, const std::vector<double>& bucket_bounds) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    if (gauges_.find(name) != gauges_.end() ||
        counters_.find(name) != counters_.end() ||
        histograms_.find(name) != histograms_.end()) {
        return false; // Metric already exists
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
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    auto it = counters_.find(name);
    if (it != counters_.end()) {
        it->second += value;
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

    nlohmann::json metrics;

    // Add gauges
    for (const auto& [name, value] : gauges_) {
        metrics[name] = value.load();
    }

    // Add counters
    for (const auto& [name, value] : counters_) {
        metrics[name] = value.load();
    }

    // Add histograms
    for (const auto& [name, hist] : histograms_) {
        nlohmann::json hist_json;
        hist_json["sample_count"] = hist->sample_count.load();
        hist_json["sum"] = hist->sum.load();

        nlohmann::json buckets_json;
        for (const auto& bucket : hist->buckets) {
            buckets_json[std::to_string(bucket.upper_bound)] = bucket.count.load();
        }
        hist_json["buckets"] = buckets_json;

        metrics[name] = hist_json;
    }

    return metrics;
}

nlohmann::json MetricsCollector::get_component_metrics(const std::string& component_name) const {
    auto all_metrics = get_all_metrics();
    nlohmann::json component_metrics;

    for (const auto& [name, value] : all_metrics.items()) {
        if (name.find(component_name) == 0) {
            component_metrics[name] = value;
        }
    }

    return component_metrics;
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

    bool removed = false;
    removed |= gauges_.erase(name) > 0;
    removed |= counters_.erase(name) > 0;
    removed |= histograms_.erase(name) > 0;
    gauge_getters_.erase(name);

    return removed;
}

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
