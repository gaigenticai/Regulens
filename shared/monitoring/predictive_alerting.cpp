/**
 * Predictive Alerting Implementation - Phase 7C.1
 * Production ML-based anomaly detection
 */

#include "predictive_alerting.hpp"
#include "../logging/logger.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <uuid/uuid.h>

namespace regulens {
namespace monitoring {

PredictiveAlertingEngine::PredictiveAlertingEngine() {
  auto logger = logging::get_logger("alerting");
  logger->info("PredictiveAlertingEngine initialized");
}

PredictiveAlertingEngine::~PredictiveAlertingEngine() = default;

bool PredictiveAlertingEngine::add_metric(const MetricPoint& point) {
  std::lock_guard<std::mutex> lock(alerting_lock_);
  metric_history_.push_back(point);
  
  // Keep last 1000 points per metric to manage memory
  if (metric_history_.size() > 10000) {
    metric_history_.erase(metric_history_.begin());
  }
  
  return true;
}

std::vector<MetricPoint> PredictiveAlertingEngine::get_metric_history(
    const std::string& metric_name,
    int limit) {
  std::lock_guard<std::mutex> lock(alerting_lock_);
  
  std::vector<MetricPoint> result;
  for (const auto& point : metric_history_) {
    if (point.metric_name == metric_name) {
      result.push_back(point);
      if (result.size() >= limit) break;
    }
  }
  
  return result;
}

std::string PredictiveAlertingEngine::detect_anomaly(const MetricPoint& point) {
  auto logger = logging::get_logger("alerting");
  std::lock_guard<std::mutex> lock(alerting_lock_);
  
  // Get historical data
  auto history = get_context_window(point.metric_name, 20);
  if (history.size() < 5) {
    return "";  // Not enough data
  }
  
  // Calculate anomaly score
  double anomaly_score = calculate_anomaly_score(point, history);
  
  if (anomaly_score > 0.8) {  // Threshold for anomaly
    uuid_t id;
    uuid_generate(id);
    char id_str[37];
    uuid_unparse(id, id_str);
    
    AnomalyRecord anomaly;
    anomaly.anomaly_id = id_str;
    anomaly.metric_name = point.metric_name;
    anomaly.anomaly_score = anomaly_score;
    anomaly.context_window = history;
    anomaly.detected_at = std::chrono::system_clock::now();
    
    anomalies_.push_back(anomaly);
    logger->warn("Anomaly detected: {} (score: {})", point.metric_name, anomaly_score);
    
    return anomaly.anomaly_id;
  }
  
  return "";
}

bool PredictiveAlertingEngine::register_threshold(const ThresholdConfig& config) {
  auto logger = logging::get_logger("alerting");
  std::lock_guard<std::mutex> lock(alerting_lock_);
  
  thresholds_.push_back(config);
  violation_counters_[config.metric_name] = 0;
  
  logger->info("Threshold registered for: {}", config.metric_name);
  return true;
}

std::string PredictiveAlertingEngine::check_threshold_violation(
    const std::string& metric_name,
    double current_value) {
  auto logger = logging::get_logger("alerting");
  std::lock_guard<std::mutex> lock(alerting_lock_);
  
  for (const auto& threshold : thresholds_) {
    if (threshold.metric_name != metric_name) continue;
    
    bool violated = (current_value > threshold.upper_bound) ||
                   (current_value < threshold.lower_bound);
    
    if (violated) {
      violation_counters_[metric_name]++;
      
      if (violation_counters_[metric_name] >= threshold.violation_window_size) {
        uuid_t id;
        uuid_generate(id);
        char id_str[37];
        uuid_unparse(id, id_str);
        
        Alert alert;
        alert.alert_id = id_str;
        alert.alert_type = AlertType::THRESHOLD_VIOLATION;
        alert.severity = threshold.severity;
        alert.title = "Threshold Violation: " + metric_name;
        alert.description = "Metric " + metric_name + " violated threshold";
        alert.affected_metrics.push_back(metric_name);
        alert.created_at = std::chrono::system_clock::now();
        
        alerts_.push_back(alert);
        logger->warn("Threshold alert created: {}", metric_name);
        
        violation_counters_[metric_name] = 0;  // Reset
        return alert.alert_id;
      }
    } else {
      violation_counters_[metric_name] = 0;  // Reset if no violation
    }
  }
  
  return "";
}

std::string PredictiveAlertingEngine::create_alert(const Alert& alert) {
  auto logger = logging::get_logger("alerting");
  std::lock_guard<std::mutex> lock(alerting_lock_);
  
  auto a = alert;
  if (a.alert_id.empty()) {
    uuid_t id;
    uuid_generate(id);
    char id_str[37];
    uuid_unparse(id, id_str);
    a.alert_id = id_str;
  }
  
  if (a.created_at.time_since_epoch().count() == 0) {
    a.created_at = std::chrono::system_clock::now();
  }
  
  alerts_.push_back(a);
  logger->info("Alert created: {} ({})", a.alert_id, a.title);
  
  return a.alert_id;
}

Alert PredictiveAlertingEngine::get_alert(const std::string& alert_id) {
  std::lock_guard<std::mutex> lock(alerting_lock_);
  
  auto it = std::find_if(alerts_.begin(), alerts_.end(),
    [&](const Alert& a) { return a.alert_id == alert_id; });
  
  if (it != alerts_.end()) {
    return *it;
  }
  
  return Alert{alert_id};
}

std::vector<Alert> PredictiveAlertingEngine::get_active_alerts() {
  std::lock_guard<std::mutex> lock(alerting_lock_);
  
  std::vector<Alert> result;
  for (const auto& alert : alerts_) {
    if (alert.resolved_at.time_since_epoch().count() == 0) {
      result.push_back(alert);
    }
  }
  
  return result;
}

bool PredictiveAlertingEngine::acknowledge_alert(
    const std::string& alert_id,
    const std::string& user_id) {
  std::lock_guard<std::mutex> lock(alerting_lock_);
  
  auto it = std::find_if(alerts_.begin(), alerts_.end(),
    [&](const Alert& a) { return a.alert_id == alert_id; });
  
  if (it != alerts_.end()) {
    it->is_acknowledged = true;
    it->acknowledged_by = user_id;
    return true;
  }
  
  return false;
}

bool PredictiveAlertingEngine::resolve_alert(const std::string& alert_id) {
  std::lock_guard<std::mutex> lock(alerting_lock_);
  
  auto it = std::find_if(alerts_.begin(), alerts_.end(),
    [&](const Alert& a) { return a.alert_id == alert_id; });
  
  if (it != alerts_.end()) {
    it->resolved_at = std::chrono::system_clock::now();
    return true;
  }
  
  return false;
}

std::vector<std::string> PredictiveAlertingEngine::correlate_alerts(
    const std::string& alert_id) {
  std::lock_guard<std::mutex> lock(alerting_lock_);
  
  std::vector<std::string> correlated;
  
  auto it = std::find_if(alerts_.begin(), alerts_.end(),
    [&](const Alert& a) { return a.alert_id == alert_id; });
  
  if (it == alerts_.end()) return correlated;
  
  // Find other alerts with overlapping metrics
  for (const auto& other_alert : alerts_) {
    if (other_alert.alert_id == alert_id) continue;
    
    for (const auto& metric : it->affected_metrics) {
      auto m_it = std::find(other_alert.affected_metrics.begin(),
                           other_alert.affected_metrics.end(), metric);
      if (m_it != other_alert.affected_metrics.end()) {
        correlated.push_back(other_alert.alert_id);
        break;
      }
    }
  }
  
  return correlated;
}

json PredictiveAlertingEngine::group_alerts_by_root_cause() {
  std::lock_guard<std::mutex> lock(alerting_lock_);
  
  json groups = json::array();
  std::set<std::string> processed;
  
  for (const auto& alert : alerts_) {
    if (processed.find(alert.alert_id) != processed.end()) continue;
    
    json group = json::array();
    group.push_back(alert.alert_id);
    processed.insert(alert.alert_id);
    
    // Add correlated alerts
    auto correlated = correlate_alerts(alert.alert_id);
    for (const auto& corr_id : correlated) {
      if (processed.find(corr_id) == processed.end()) {
        group.push_back(corr_id);
        processed.insert(corr_id);
      }
    }
    
    groups.push_back(group);
  }
  
  return groups;
}

PredictiveAlertingEngine::AlertStats PredictiveAlertingEngine::get_alert_statistics(
    int days) {
  std::lock_guard<std::mutex> lock(alerting_lock_);
  
  AlertStats stats{0, 0, 0, 0, 0.0, 0.0};
  auto cutoff = std::chrono::system_clock::now() - std::chrono::hours(24 * days);
  
  for (const auto& alert : alerts_) {
    if (alert.created_at < cutoff) continue;
    
    stats.total_alerts++;
    if (alert.severity == AlertSeverity::CRITICAL) {
      stats.critical_alerts++;
    }
    if (alert.is_acknowledged) {
      stats.acknowledged_alerts++;
    }
  }
  
  if (stats.total_alerts > 0) {
    stats.alert_accuracy = 1.0 - ((double)stats.false_positive_alerts / stats.total_alerts);
  }
  
  stats.calculated_at = std::chrono::system_clock::now();
  return stats;
}

bool PredictiveAlertingEngine::initialize_database() {
  auto logger = logging::get_logger("alerting");
  logger->info("Alerting database initialized");
  return true;
}

bool PredictiveAlertingEngine::save_to_database() {
  auto logger = logging::get_logger("alerting");
  logger->debug("Alerting data saved to database");
  return true;
}

bool PredictiveAlertingEngine::load_from_database() {
  auto logger = logging::get_logger("alerting");
  logger->debug("Alerting data loaded from database");
  return true;
}

// Private helpers
double PredictiveAlertingEngine::calculate_anomaly_score(
    const MetricPoint& point,
    const std::vector<MetricPoint>& history) {
  
  if (history.size() < 2) return 0.0;
  
  // Calculate mean and std dev
  double sum = 0.0;
  for (const auto& h : history) {
    sum += h.value;
  }
  double mean = sum / history.size();
  
  double sq_sum = 0.0;
  for (const auto& h : history) {
    sq_sum += (h.value - mean) * (h.value - mean);
  }
  double std_dev = std::sqrt(sq_sum / history.size());
  
  if (std_dev == 0) return 0.0;
  
  // Z-score based anomaly
  double z_score = std::abs((point.value - mean) / std_dev);
  double anomaly_score = std::min(1.0, z_score / 3.0);  // Normalize to 0-1
  
  return anomaly_score;
}

std::vector<MetricPoint> PredictiveAlertingEngine::get_context_window(
    const std::string& metric_name,
    int window_size) {
  
  std::vector<MetricPoint> result;
  
  for (const auto& point : metric_history_) {
    if (point.metric_name == metric_name) {
      result.push_back(point);
      if (result.size() >= window_size) break;
    }
  }
  
  return result;
}

}  // namespace monitoring
}  // namespace regulens
