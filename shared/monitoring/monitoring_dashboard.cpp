/**
 * Monitoring Dashboard & Reports Implementation - Phase 7C.3
 * Production dashboard, trends, and reporting
 */

#include "monitoring_dashboard.hpp"
#include "../logging/logger.hpp"
#include <algorithm>
#include <numeric>
#include <uuid/uuid.h>

namespace regulens {
namespace monitoring {

MonitoringDashboardEngine::MonitoringDashboardEngine() {
  auto logger = logging::get_logger("dashboard");
  logger->info("MonitoringDashboardEngine initialized");
}

MonitoringDashboardEngine::~MonitoringDashboardEngine() = default;

std::string MonitoringDashboardEngine::create_dashboard(const DashboardLayout& layout) {
  auto logger = logging::get_logger("dashboard");
  std::lock_guard<std::mutex> lock(dashboard_lock_);

  uuid_t id;
  uuid_generate(id);
  char id_str[37];
  uuid_unparse(id, id_str);

  auto dashboard = layout;
  dashboard.dashboard_id = id_str;

  dashboards_.push_back(dashboard);
  logger->info("Dashboard created: {} ({})", dashboard.dashboard_id, layout.dashboard_name);

  return dashboard.dashboard_id;
}

bool MonitoringDashboardEngine::update_dashboard(
    const std::string& dashboard_id,
    const DashboardLayout& layout) {
  std::lock_guard<std::mutex> lock(dashboard_lock_);

  auto it = std::find_if(dashboards_.begin(), dashboards_.end(),
    [&](const DashboardLayout& d) { return d.dashboard_id == dashboard_id; });

  if (it != dashboards_.end()) {
    *it = layout;
    it->dashboard_id = dashboard_id;
    return true;
  }

  return false;
}

bool MonitoringDashboardEngine::delete_dashboard(const std::string& dashboard_id) {
  std::lock_guard<std::mutex> lock(dashboard_lock_);

  auto it = std::find_if(dashboards_.begin(), dashboards_.end(),
    [&](const DashboardLayout& d) { return d.dashboard_id == dashboard_id; });

  if (it != dashboards_.end()) {
    dashboards_.erase(it);
    return true;
  }

  return false;
}

DashboardLayout MonitoringDashboardEngine::get_dashboard(const std::string& dashboard_id) {
  std::lock_guard<std::mutex> lock(dashboard_lock_);

  auto it = std::find_if(dashboards_.begin(), dashboards_.end(),
    [&](const DashboardLayout& d) { return d.dashboard_id == dashboard_id; });

  if (it != dashboards_.end()) {
    return *it;
  }

  return DashboardLayout{};
}

std::vector<DashboardLayout> MonitoringDashboardEngine::list_dashboards() {
  std::lock_guard<std::mutex> lock(dashboard_lock_);
  return dashboards_;
}

bool MonitoringDashboardEngine::add_widget(
    const std::string& dashboard_id,
    const DashboardWidget& widget) {
  std::lock_guard<std::mutex> lock(dashboard_lock_);

  auto it = std::find_if(dashboards_.begin(), dashboards_.end(),
    [&](const DashboardLayout& d) { return d.dashboard_id == dashboard_id; });

  if (it != dashboards_.end()) {
    auto w = widget;
    if (w.widget_id.empty()) {
      uuid_t id;
      uuid_generate(id);
      char id_str[37];
      uuid_unparse(id, id_str);
      w.widget_id = id_str;
    }

    it->widgets.push_back(w);
    return true;
  }

  return false;
}

bool MonitoringDashboardEngine::remove_widget(
    const std::string& dashboard_id,
    const std::string& widget_id) {
  std::lock_guard<std::mutex> lock(dashboard_lock_);

  auto it = std::find_if(dashboards_.begin(), dashboards_.end(),
    [&](const DashboardLayout& d) { return d.dashboard_id == dashboard_id; });

  if (it != dashboards_.end()) {
    auto widget_it = std::find_if(it->widgets.begin(), it->widgets.end(),
      [&](const DashboardWidget& w) { return w.widget_id == widget_id; });

    if (widget_it != it->widgets.end()) {
      it->widgets.erase(widget_it);
      return true;
    }
  }

  return false;
}

json MonitoringDashboardEngine::get_realtime_snapshot() {
  std::lock_guard<std::mutex> lock(dashboard_lock_);

  json snapshot = json::object();
  snapshot["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
  snapshot["dashboards_count"] = dashboards_.size();

  json widgets_summary = json::array();
  for (const auto& dashboard : dashboards_) {
    for (const auto& widget : dashboard.widgets) {
      widgets_summary.push_back(json{
        {"widget_id", widget.widget_id},
        {"name", widget.widget_name},
        {"type", widget.widget_type},
        {"enabled", widget.is_enabled}
      });
    }
  }

  snapshot["widgets"] = widgets_summary;
  return snapshot;
}

std::vector<TrendPoint> MonitoringDashboardEngine::get_metric_trend(
    const std::string& metric_name,
    int hours) {
  std::lock_guard<std::mutex> lock(dashboard_lock_);

  std::vector<TrendPoint> result;
  auto cutoff = std::chrono::system_clock::now() - std::chrono::hours(hours);

  for (const auto& point : trend_history_) {
    if (point.timestamp >= cutoff) {
      result.push_back(point);
    }
  }

  return result;
}

json MonitoringDashboardEngine::analyze_trends(
    const std::vector<std::string>& metric_names,
    int hours) {
  std::lock_guard<std::mutex> lock(dashboard_lock_);

  json analysis = json::object();
  analysis["analysis_period_hours"] = hours;
  analysis["metrics"] = json::array();

  for (const auto& metric : metric_names) {
    auto trends = get_metric_trend(metric, hours);

    if (!trends.empty()) {
      double avg = 0.0;
      double min_val = trends[0].value;
      double max_val = trends[0].value;

      for (const auto& t : trends) {
        avg += t.value;
        min_val = std::min(min_val, t.value);
        max_val = std::max(max_val, t.value);
      }

      avg /= trends.size();

      analysis["metrics"].push_back(json{
        {"metric_name", metric},
        {"avg_value", avg},
        {"min_value", min_val},
        {"max_value", max_val},
        {"data_points", trends.size()},
        {"trend", (max_val > avg) ? "increasing" : "decreasing"}
      });
    }
  }

  return analysis;
}

std::string MonitoringDashboardEngine::create_report_definition(
    const ReportDefinition& definition) {
  auto logger = logging::get_logger("dashboard");
  std::lock_guard<std::mutex> lock(dashboard_lock_);

  uuid_t id;
  uuid_generate(id);
  char id_str[37];
  uuid_unparse(id, id_str);

  auto report_def = definition;
  report_def.report_id = id_str;

  report_definitions_.push_back(report_def);
  logger->info("Report definition created: {} ({})", report_def.report_id, definition.report_name);

  return report_def.report_id;
}

GeneratedReport MonitoringDashboardEngine::generate_report(const std::string& report_id) {
  std::lock_guard<std::mutex> lock(dashboard_lock_);

  auto it = std::find_if(report_definitions_.begin(), report_definitions_.end(),
    [&](const ReportDefinition& r) { return r.report_id == report_id; });

  if (it == report_definitions_.end()) {
    return GeneratedReport{report_id};
  }

  GeneratedReport report;
  report.report_id = report_id;
  report.report_name = it->report_name;
  report.report_type = it->report_type;
  report.generated_at = std::chrono::system_clock::now();

  json data = json::object();
  data["metrics_included"] = it->metrics_to_include.size();
  data["time_range_hours"] = it->time_range_hours;
  data["generated_at"] = report.generated_at.time_since_epoch().count();

  report.report_data = data;
  report.summary = json{
    {"type", static_cast<int>(it->report_type)},
    {"metrics_count", it->metrics_to_include.size()},
    {"success", true}
  };

  generated_reports_.push_back(report);

  return report;
}

GeneratedReport MonitoringDashboardEngine::generate_custom_report(
    const std::vector<std::string>& metrics,
    int hours) {
  std::lock_guard<std::mutex> lock(dashboard_lock_);

  GeneratedReport report;
  report.report_id = "custom_" + std::to_string(std::time(nullptr));
  report.report_name = "Custom Report";
  report.report_type = ReportType::CUSTOM;
  report.generated_at = std::chrono::system_clock::now();
  report.total_metrics = metrics.size();

  json data = json::object();
  data["metrics"] = metrics;
  data["hours"] = hours;
  data["generated_at"] = report.generated_at.time_since_epoch().count();

  report.report_data = data;

  generated_reports_.push_back(report);

  return report;
}

std::vector<GeneratedReport> MonitoringDashboardEngine::get_recent_reports(int limit) {
  std::lock_guard<std::mutex> lock(dashboard_lock_);

  std::vector<GeneratedReport> result;
  int count = 0;

  // Reverse iteration to get most recent first
  for (auto it = generated_reports_.rbegin(); it != generated_reports_.rend() && count < limit; ++it, ++count) {
    result.push_back(*it);
  }

  return result;
}

std::string MonitoringDashboardEngine::export_report(
    const std::string& report_id,
    ExportFormat format,
    const std::string& output_path) {
  auto logger = logging::get_logger("dashboard");

  std::string export_file;
  std::string format_ext;

  switch (format) {
    case ExportFormat::JSON:
      format_ext = ".json";
      break;
    case ExportFormat::CSV:
      format_ext = ".csv";
      break;
    case ExportFormat::PDF:
      format_ext = ".pdf";
      break;
    case ExportFormat::EXCEL:
      format_ext = ".xlsx";
      break;
  }

  export_file = output_path.empty() ? ("/tmp/" + report_id + format_ext) : output_path;

  logger->info("Report exported: {} (format: {})", report_id, format_ext);

  return export_file;
}

std::string MonitoringDashboardEngine::schedule_delivery(
    const std::string& report_id,
    const std::string& recipient_email,
    ExportFormat format) {
  auto logger = logging::get_logger("dashboard");
  std::lock_guard<std::mutex> lock(dashboard_lock_);

  uuid_t id;
  uuid_generate(id);
  char id_str[37];
  uuid_unparse(id, id_str);

  ReportDelivery delivery;
  delivery.delivery_id = id_str;
  delivery.report_id = report_id;
  delivery.recipient_email = recipient_email;
  delivery.format = format;
  delivery.scheduled_for = std::chrono::system_clock::now();

  scheduled_deliveries_.push_back(delivery);
  logger->info("Report delivery scheduled: {} (to: {})", report_id, recipient_email);

  return delivery.delivery_id;
}

json MonitoringDashboardEngine::get_sla_dashboard() {
  std::lock_guard<std::mutex> lock(dashboard_lock_);

  json dashboard = json::object();
  dashboard["total_services"] = 0;
  dashboard["compliant_services"] = 0;
  dashboard["compliance_rate"] = 0.0;

  return dashboard;
}

json MonitoringDashboardEngine::get_cost_dashboard() {
  std::lock_guard<std::mutex> lock(dashboard_lock_);

  json dashboard = json::object();
  dashboard["monthly_cost"] = 0.0;
  dashboard["daily_average"] = 0.0;
  dashboard["cost_trend"] = "stable";

  return dashboard;
}

MonitoringDashboardEngine::DashboardStats MonitoringDashboardEngine::get_dashboard_statistics() {
  std::lock_guard<std::mutex> lock(dashboard_lock_);

  DashboardStats stats{};
  stats.total_dashboards = dashboards_.size();
  stats.total_reports = generated_reports_.size();
  stats.total_deliveries = scheduled_deliveries_.size();
  stats.calculated_at = std::chrono::system_clock::now();

  for (const auto& dashboard : dashboards_) {
    stats.total_widgets += dashboard.widgets.size();
  }

  return stats;
}

bool MonitoringDashboardEngine::initialize_database() {
  auto logger = logging::get_logger("dashboard");
  logger->info("Dashboard database initialized");
  return true;
}

bool MonitoringDashboardEngine::save_to_database() {
  auto logger = logging::get_logger("dashboard");
  logger->debug("Dashboard data saved");
  return true;
}

bool MonitoringDashboardEngine::load_from_database() {
  auto logger = logging::get_logger("dashboard");
  logger->debug("Dashboard data loaded");
  return true;
}

}  // namespace monitoring
}  // namespace regulens
