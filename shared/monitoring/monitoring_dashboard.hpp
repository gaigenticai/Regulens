/**
 * Monitoring Dashboard & Reports Engine - Phase 7C.3
 * Real-time monitoring, trend analysis, custom reports, exports
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>
#include <chrono>

namespace regulens {
namespace monitoring {

using json = nlohmann::json;

// Report types
enum class ReportType {
  DAILY_SUMMARY,
  WEEKLY_TRENDS,
  MONTHLY_ANALYSIS,
  CUSTOM
};

// Export formats
enum class ExportFormat {
  JSON,
  CSV,
  PDF,
  EXCEL
};

// Dashboard widget
struct DashboardWidget {
  std::string widget_id;
  std::string widget_name;
  std::string widget_type;  // chart, metric, table, heatmap
  json configuration;  // Chart config, dimensions, etc.
  std::vector<std::string> metric_names;
  int refresh_interval_seconds = 60;
  bool is_enabled = true;
};

// Dashboard layout
struct DashboardLayout {
  std::string dashboard_id;
  std::string dashboard_name;
  std::string description;
  std::vector<DashboardWidget> widgets;
  int columns = 4;
  json layout_config;  // Grid layout
};

// Trend data point
struct TrendPoint {
  std::chrono::system_clock::time_point timestamp;
  double value = 0.0;
  double avg_value = 0.0;
  double max_value = 0.0;
  double min_value = 0.0;
};

// Report definition
struct ReportDefinition {
  std::string report_id;
  std::string report_name;
  ReportType report_type;
  std::vector<std::string> metrics_to_include;
  std::vector<std::string> dimensions;
  int time_range_hours = 24;
  std::string schedule;  // Cron for scheduled reports
  bool is_enabled = true;
};

// Generated report
struct GeneratedReport {
  std::string report_id;
  std::string report_name;
  ReportType report_type;
  json report_data;
  json summary;
  int total_metrics = 0;
  int total_records = 0;
  std::chrono::system_clock::time_point generated_at;
};

// Report delivery
struct ReportDelivery {
  std::string delivery_id;
  std::string report_id;
  std::string recipient_email;
  ExportFormat format;
  bool delivered = false;
  std::chrono::system_clock::time_point scheduled_for;
  std::chrono::system_clock::time_point delivered_at;
};

class MonitoringDashboardEngine {
public:
  MonitoringDashboardEngine();
  ~MonitoringDashboardEngine();

  // Dashboard Management
  std::string create_dashboard(const DashboardLayout& layout);
  bool update_dashboard(const std::string& dashboard_id, const DashboardLayout& layout);
  bool delete_dashboard(const std::string& dashboard_id);
  DashboardLayout get_dashboard(const std::string& dashboard_id);
  std::vector<DashboardLayout> list_dashboards();

  // Widget Management
  bool add_widget(const std::string& dashboard_id, const DashboardWidget& widget);
  bool remove_widget(const std::string& dashboard_id, const std::string& widget_id);
  bool update_widget(const std::string& widget_id, const DashboardWidget& widget);

  // Real-time Data
  json get_realtime_snapshot();  // Current state of all metrics
  std::map<std::string, json> get_widget_data(const std::string& widget_id);

  // Trend Analysis
  std::vector<TrendPoint> get_metric_trend(
      const std::string& metric_name,
      int hours = 24);

  json analyze_trends(
      const std::vector<std::string>& metric_names,
      int hours = 24);

  // Anomaly Detection in Trends
  std::vector<TrendPoint> detect_trend_anomalies(
      const std::string& metric_name,
      int hours = 24);

  // Report Generation
  std::string create_report_definition(const ReportDefinition& definition);
  GeneratedReport generate_report(const std::string& report_id);
  GeneratedReport generate_custom_report(
      const std::vector<std::string>& metrics,
      int hours = 24);
  std::vector<GeneratedReport> get_recent_reports(int limit = 10);

  // Report Scheduling
  bool schedule_report(const ReportDefinition& definition);
  bool unschedule_report(const std::string& report_id);

  // Report Delivery & Export
  std::string export_report(
      const std::string& report_id,
      ExportFormat format,
      const std::string& output_path = "");

  std::string schedule_delivery(
      const std::string& report_id,
      const std::string& recipient_email,
      ExportFormat format);

  std::vector<ReportDelivery> get_scheduled_deliveries();
  bool execute_pending_deliveries();

  // Historical Data
  json get_historical_comparison(
      const std::string& metric_name,
      int current_hours = 24,
      int historical_hours = 24);

  json get_year_over_year_comparison(const std::string& metric_name);

  // SLA Dashboard
  json get_sla_dashboard();
  json get_sla_trends(int days = 30);

  // Cost Dashboard
  json get_cost_dashboard();
  json get_cost_forecast(int months = 3);

  // Statistics
  struct DashboardStats {
    int total_dashboards;
    int total_widgets;
    int total_reports;
    int total_deliveries;
    double avg_widget_refresh_interval_seconds;
    json most_viewed_metrics;
    json most_alerting_metrics;
    std::chrono::system_clock::time_point calculated_at;
  };

  DashboardStats get_dashboard_statistics();

  // Database operations
  bool initialize_database();
  bool save_to_database();
  bool load_from_database();

private:
  std::vector<DashboardLayout> dashboards_;
  std::vector<ReportDefinition> report_definitions_;
  std::vector<GeneratedReport> generated_reports_;
  std::vector<ReportDelivery> scheduled_deliveries_;
  std::vector<TrendPoint> trend_history_;

  std::mutex dashboard_lock_;

  // Internal helpers
  json aggregate_metric_data(
      const std::string& metric_name,
      int hours);
  std::vector<TrendPoint> calculate_moving_average(
      const std::vector<TrendPoint>& points,
      int window_size);
  bool detect_trend_change(
      const std::vector<TrendPoint>& points);
};

}  // namespace monitoring
}  // namespace regulens
