/**
 * Predictive Alerting Engine - Phase 7C.1
 * ML-based anomaly detection, correlation, smart grouping
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>
#include <chrono>
#include <memory>

namespace regulens {
namespace monitoring {

using json = nlohmann::json;

// Alert severity levels
enum class AlertSeverity {
  INFO = 0,
  WARNING = 1,
  ERROR = 2,
  CRITICAL = 3
};

// Alert types
enum class AlertType {
  THRESHOLD_VIOLATION,
  ANOMALY_DETECTED,
  PATTERN_CHANGE,
  CORRELATION_ALERT,
  PREDICTION_WARNING
};

// Metric point for anomaly detection
struct MetricPoint {
  std::string metric_name;
  double value = 0.0;
  std::chrono::system_clock::time_point timestamp;
  std::map<std::string, std::string> tags;
};

// Anomaly record
struct AnomalyRecord {
  std::string anomaly_id;
  std::string metric_name;
  double anomaly_score = 0.0;  // 0-1
  double threshold = 0.0;
  std::vector<MetricPoint> context_window;  // Last N points
  bool is_confirmed = false;
  std::chrono::system_clock::time_point detected_at;
};

// Alert record
struct Alert {
  std::string alert_id;
  AlertType alert_type;
  AlertSeverity severity;
  std::string title;
  std::string description;
  std::vector<std::string> affected_metrics;
  std::vector<std::string> correlated_alerts;  // Related alerts
  bool is_grouped = false;
  std::string group_id;  // For alert grouping
  bool is_acknowledged = false;
  std::string acknowledged_by;
  std::chrono::system_clock::time_point created_at;
  std::chrono::system_clock::time_point resolved_at;
};

// Threshold configuration
struct ThresholdConfig {
  std::string metric_name;
  double upper_bound = 0.0;
  double lower_bound = 0.0;
  int violation_window_size = 5;  // How many violations to trigger alert
  AlertSeverity severity = AlertSeverity::WARNING;
};

// Prediction
struct AlertPrediction {
  std::string prediction_id;
  std::string metric_name;
  double probability = 0.0;  // Probability of alert in next period
  std::string predicted_condition;  // What might happen
  int lead_time_minutes = 0;  // Minutes before expected alert
  std::chrono::system_clock::time_point predicted_at;
  std::chrono::system_clock::time_point predicted_occurrence_time;
};

class PredictiveAlertingEngine {
public:
  PredictiveAlertingEngine();
  ~PredictiveAlertingEngine();

  // Metric Collection
  bool add_metric(const MetricPoint& point);
  std::vector<MetricPoint> get_metric_history(
      const std::string& metric_name,
      int limit = 100);

  // Anomaly Detection (ML-based)
  std::string detect_anomaly(const MetricPoint& point);
  std::vector<AnomalyRecord> get_recent_anomalies(int limit = 10);
  bool confirm_anomaly(const std::string& anomaly_id);

  // Threshold-based Alerting
  bool register_threshold(const ThresholdConfig& config);
  std::string check_threshold_violation(
      const std::string& metric_name,
      double current_value);

  // Alert Management
  std::string create_alert(const Alert& alert);
  Alert get_alert(const std::string& alert_id);
  std::vector<Alert> get_active_alerts();
  bool acknowledge_alert(const std::string& alert_id, const std::string& user_id);
  bool resolve_alert(const std::string& alert_id);

  // Alert Correlation (find related alerts)
  std::vector<std::string> correlate_alerts(const std::string& alert_id);
  json get_alert_correlation_graph();

  // Smart Alert Grouping
  json group_alerts_by_root_cause();
  std::vector<std::vector<std::string>> get_alert_groups();
  bool suppress_duplicate_alert(const Alert& alert, int window_minutes = 5);

  // Predictive Alerting (ML-based predictions)
  std::string predict_alert(const std::string& metric_name);
  std::vector<AlertPrediction> get_alert_predictions(int limit = 10);

  // Statistics
  struct AlertStats {
    int total_alerts;
    int critical_alerts;
    int acknowledged_alerts;
    int false_positive_alerts;
    double alert_accuracy;
    double correlation_strength;
    std::chrono::system_clock::time_point calculated_at;
  };

  AlertStats get_alert_statistics(int days = 30);

  // Database operations
  bool initialize_database();
  bool save_to_database();
  bool load_from_database();

private:
  std::vector<MetricPoint> metric_history_;
  std::vector<AnomalyRecord> anomalies_;
  std::vector<Alert> alerts_;
  std::vector<ThresholdConfig> thresholds_;
  std::map<std::string, int> violation_counters_;  // For threshold violations

  std::mutex alerting_lock_;

  // ML helpers
  double calculate_anomaly_score(
      const MetricPoint& point,
      const std::vector<MetricPoint>& history);
  std::vector<MetricPoint> get_context_window(
      const std::string& metric_name,
      int window_size = 20);
  bool is_seasonal_pattern(const std::vector<MetricPoint>& history);
};

}  // namespace monitoring
}  // namespace regulens
