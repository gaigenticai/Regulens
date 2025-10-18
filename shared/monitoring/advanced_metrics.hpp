/**
 * Advanced Metrics Collection Engine - Phase 7C.2
 * Business, Technical, and Cost metrics collection & analysis
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>
#include <chrono>
#include <deque>

namespace regulens {
namespace monitoring {

using json = nlohmann::json;

// Metric types
enum class MetricCategory {
  BUSINESS,      // Decision quality, rule accuracy, etc.
  TECHNICAL,     // Latency, throughput, error rates
  COST,          // Compute, storage, API calls
  INFRASTRUCTURE // CPU, memory, disk, network
};

// Business Metrics
struct BusinessMetrics {
  double decision_accuracy = 0.0;      // Overall accuracy %
  double rule_effectiveness = 0.0;     // Rules working correctly %
  int total_decisions = 0;
  int successful_decisions = 0;
  int failed_decisions = 0;
  double avg_decision_confidence = 0.0;
  double ensemble_vs_single_win_rate = 0.0;
  double false_positive_rate = 0.0;
  double false_negative_rate = 0.0;
};

// Technical Metrics
struct TechnicalMetrics {
  double p50_latency_ms = 0.0;
  double p95_latency_ms = 0.0;
  double p99_latency_ms = 0.0;
  double avg_latency_ms = 0.0;
  int throughput_requests_per_second = 0;
  double error_rate = 0.0;  // %
  double success_rate = 0.0;  // %
  int total_requests = 0;
  int failed_requests = 0;
  double cache_hit_rate = 0.0;
};

// Cost Metrics
struct CostMetrics {
  double compute_cost_per_hour = 0.0;
  double storage_cost_per_month = 0.0;
  double api_call_cost = 0.0;
  double total_monthly_cost = 0.0;
  long compute_units_used = 0;
  long storage_gb_used = 0;
  long api_calls_made = 0;
  double cost_per_decision = 0.0;
};

// Metric point with dimensions
struct MetricPoint {
  std::string metric_name;
  double value = 0.0;
  MetricCategory category;
  std::map<std::string, std::string> dimensions;  // service, region, env, etc.
  std::chrono::system_clock::time_point recorded_at;
};

// Aggregated metrics (5min, 1hr, 1day buckets)
struct MetricBucket {
  std::chrono::system_clock::time_point bucket_start;
  std::chrono::system_clock::time_point bucket_end;
  double avg_value = 0.0;
  double min_value = 0.0;
  double max_value = 0.0;
  double p50 = 0.0;
  double p95 = 0.0;
  double p99 = 0.0;
  int sample_count = 0;
};

// SLA (Service Level Agreement) definition
struct SLADefinition {
  std::string service_name;
  double availability_target = 99.9;  // %
  double latency_p99_target_ms = 100.0;
  double error_rate_target = 0.1;  // %
  int measurement_window_minutes = 60;
};

// SLA compliance record
struct SLACompliance {
  std::string service_name;
  std::chrono::system_clock::time_point measurement_period;
  double actual_availability = 0.0;
  double actual_latency_p99_ms = 0.0;
  double actual_error_rate = 0.0;
  bool is_compliant = false;
  json violations;  // Details of violations
};

class AdvancedMetricsEngine {
public:
  AdvancedMetricsEngine();
  ~AdvancedMetricsEngine();

  // Recording metrics
  bool record_business_metric(const BusinessMetrics& metrics, const std::string& service = "all");
  bool record_technical_metric(const TechnicalMetrics& metrics, const std::string& service = "all");
  bool record_cost_metric(const CostMetrics& metrics, const std::string& service = "all");
  bool record_custom_metric(const MetricPoint& point);

  // Retrieving metrics
  BusinessMetrics get_business_metrics(int minutes = 60);
  TechnicalMetrics get_technical_metrics(int minutes = 60);
  CostMetrics get_cost_metrics(int months = 1);

  // Aggregation
  std::vector<MetricBucket> aggregate_metrics(
      const std::string& metric_name,
      int bucket_size_minutes = 5);

  json get_time_series(
      const std::string& metric_name,
      int hours = 24);

  // SLA Management
  bool register_sla(const SLADefinition& sla);
  SLACompliance check_sla_compliance(const std::string& service_name);
  json get_sla_report(int days = 30);

  // Analytics
  struct MetricsStats {
    double avg_decision_accuracy;
    double avg_latency_p99_ms;
    double avg_cost_per_decision;
    double availability_percentage;
    int total_decisions;
    int total_requests;
    json top_performing_rules;
    json slowest_operations;
    json cost_breakdown;
    std::chrono::system_clock::time_point calculated_at;
  };

  MetricsStats get_metrics_statistics(int days = 30);

  // Anomaly detection in metrics
  bool is_metric_anomalous(const std::string& metric_name, double current_value);
  std::vector<std::string> get_anomalous_metrics();

  // Cost optimization
  json get_cost_optimization_recommendations();
  json get_performance_optimization_recommendations();

  // Database operations
  bool initialize_database();
  bool save_to_database();
  bool load_from_database();

private:
  std::deque<MetricPoint> metric_history_;
  std::vector<BusinessMetrics> business_metrics_;
  std::vector<TechnicalMetrics> technical_metrics_;
  std::vector<CostMetrics> cost_metrics_;
  std::vector<SLADefinition> sla_definitions_;
  std::vector<SLACompliance> sla_history_;

  std::mutex metrics_lock_;

  // Internal helpers
  double calculate_percentile(const std::vector<double>& values, double percentile);
  std::vector<double> extract_values(
      const std::string& metric_name,
      int minutes = 60);
  bool check_metric_anomaly(double current, double historical_avg, double std_dev);
};

}  // namespace monitoring
}  // namespace regulens
