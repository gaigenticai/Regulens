/**
 * Tool Categories Implementation
 * Analytics, Workflow, Security, and Monitoring Tools
 */

#ifndef TOOL_CATEGORIES_HPP
#define TOOL_CATEGORIES_HPP

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "../tool_interface.hpp"
#include "../../database/postgresql_connection.hpp"
#include "../../logging/structured_logger.hpp"

namespace regulens {

enum class ToolCategory {
    ANALYTICS,
    WORKFLOW,
    SECURITY,
    MONITORING
};

enum class AnalyticsToolType {
    DATA_ANALYZER,
    REPORT_GENERATOR,
    DASHBOARD_BUILDER,
    PREDICTIVE_MODEL,
    STATISTICAL_ANALYZER
};

enum class WorkflowToolType {
    TASK_AUTOMATOR,
    PROCESS_OPTIMIZER,
    SCHEDULER,
    APPROVAL_WORKFLOW,
    DOCUMENT_PROCESSOR
};

enum class SecurityToolType {
    VULNERABILITY_SCANNER,
    COMPLIANCE_CHECKER,
    ACCESS_ANALYZER,
    ENCRYPTION_MANAGER,
    AUDIT_LOGGER
};

enum class MonitoringToolType {
    SYSTEM_MONITOR,
    PERFORMANCE_TRACKER,
    ALERT_MANAGER,
    LOG_AGGREGATOR,
    HEALTH_CHECKER
};

// ============================================================================
// ANALYTICS TOOLS
// ============================================================================

class DataAnalyzerTool : public Tool {
public:
    DataAnalyzerTool(const ToolConfig& config, StructuredLogger* logger);

    ToolResult execute_operation(const std::string& operation, const nlohmann::json& parameters) override;
    bool authenticate() override;
    bool is_authenticated() const override;
    bool disconnect() override;

private:
    ToolResult analyze_dataset(const nlohmann::json& parameters);
    std::shared_ptr<PostgreSQLConnection> db_conn_;
};

class ReportGeneratorTool : public Tool {
public:
    ReportGeneratorTool(const ToolConfig& config, StructuredLogger* logger);

    ToolResult execute_operation(const std::string& operation, const nlohmann::json& parameters) override;
    bool authenticate() override;
    bool is_authenticated() const override;
    bool disconnect() override;

private:
    ToolResult generate_report(const nlohmann::json& parameters);
    std::shared_ptr<PostgreSQLConnection> db_conn_;

    nlohmann::json generate_compliance_report(const std::string& report_type,
                                            const std::string& date_range);
    nlohmann::json generate_fraud_report(const std::string& time_period);
    nlohmann::json generate_performance_report(const std::string& metric_type);
};

class DashboardBuilderTool : public Tool {
public:
    DashboardBuilderTool(const ToolConfig& config, StructuredLogger* logger);

    ToolResult execute_operation(const std::string& operation, const nlohmann::json& parameters) override;
    bool authenticate() override;
    bool is_authenticated() const override;
    bool disconnect() override;

private:
    ToolResult build_dashboard(const nlohmann::json& parameters);
    std::shared_ptr<PostgreSQLConnection> db_conn_;

    nlohmann::json create_executive_dashboard();
    nlohmann::json create_compliance_dashboard();
    nlohmann::json create_risk_dashboard();
};

class PredictiveModelTool : public Tool {
public:
    PredictiveModelTool(const ToolConfig& config, StructuredLogger* logger);

    ToolResult execute_operation(const std::string& operation, const nlohmann::json& parameters) override;
    bool authenticate() override;
    bool is_authenticated() const override;
    bool disconnect() override;

private:
    ToolResult run_prediction(const nlohmann::json& parameters);
    std::shared_ptr<PostgreSQLConnection> db_conn_;

    double predict_fraud_risk(const nlohmann::json& transaction_data);
    double predict_compliance_violation(const nlohmann::json& activity_data);
    nlohmann::json forecast_trends(const std::string& metric, int periods);
};

// ============================================================================
// WORKFLOW TOOLS
// ============================================================================

class TaskAutomatorTool : public Tool {
public:
    TaskAutomatorTool(const ToolConfig& config, StructuredLogger* logger);

    ToolResult execute_operation(const std::string& operation, const nlohmann::json& parameters) override;
    bool authenticate() override;
    bool is_authenticated() const override;
    bool disconnect() override;

private:
    ToolResult automate_task(const nlohmann::json& parameters);
    std::shared_ptr<PostgreSQLConnection> db_conn_;

    nlohmann::json execute_data_ingestion_workflow(const nlohmann::json& config);
    nlohmann::json execute_compliance_check_workflow(const nlohmann::json& config);
    nlohmann::json execute_report_generation_workflow(const nlohmann::json& config);
};

class ProcessOptimizerTool : public Tool {
public:
    ProcessOptimizerTool(const ToolConfig& config, StructuredLogger* logger);

    ToolResult execute_operation(const std::string& operation, const nlohmann::json& parameters) override;
    bool authenticate() override;
    bool is_authenticated() const override;
    bool disconnect() override;

private:
    ToolResult optimize_process(const nlohmann::json& parameters);
    std::shared_ptr<PostgreSQLConnection> db_conn_;

    nlohmann::json analyze_workflow_efficiency(const std::string& process_name);
    nlohmann::json identify_bottlenecks(const std::string& workflow_id);
    nlohmann::json suggest_optimizations(const nlohmann::json& current_process);
};

class ApprovalWorkflowTool : public Tool {
public:
    ApprovalWorkflowTool(const ToolConfig& config, StructuredLogger* logger);

    ToolResult execute_operation(const std::string& operation, const nlohmann::json& parameters) override;
    bool authenticate() override;
    bool is_authenticated() const override;
    bool disconnect() override;

private:
    ToolResult manage_approval(const nlohmann::json& parameters);
    std::shared_ptr<PostgreSQLConnection> db_conn_;

    nlohmann::json create_approval_workflow(const nlohmann::json& request);
    nlohmann::json process_approval_decision(const std::string& workflow_id,
                                           const std::string& decision,
                                           const std::string& comments);
    nlohmann::json get_pending_approvals(const std::string& user_id);
};

// ============================================================================
// SECURITY TOOLS
// ============================================================================

class VulnerabilityScannerTool : public Tool {
public:
    VulnerabilityScannerTool(const ToolConfig& config, StructuredLogger* logger);

    ToolResult execute_operation(const std::string& operation, const nlohmann::json& parameters) override;
    bool authenticate() override;
    bool is_authenticated() const override;
    bool disconnect() override;

private:
    ToolResult scan_vulnerabilities(const nlohmann::json& parameters);
    std::shared_ptr<PostgreSQLConnection> db_conn_;

    nlohmann::json scan_configuration_vulnerabilities();
    nlohmann::json scan_access_control_vulnerabilities();
    nlohmann::json scan_data_exposure_risks();
    nlohmann::json generate_security_recommendations(const std::vector<nlohmann::json>& vulnerabilities);
};

class ComplianceCheckerTool : public Tool {
public:
    ComplianceCheckerTool(const ToolConfig& config, StructuredLogger* logger);

    ToolResult execute_operation(const std::string& operation, const nlohmann::json& parameters) override;
    bool authenticate() override;
    bool is_authenticated() const override;
    bool disconnect() override;

private:
    ToolResult check_compliance(const nlohmann::json& parameters);
    std::shared_ptr<PostgreSQLConnection> db_conn_;

    nlohmann::json check_gdpr_compliance();
    nlohmann::json check_hipaa_compliance();
    nlohmann::json check_soc2_compliance();
    nlohmann::json generate_compliance_report(const std::vector<nlohmann::json>& violations);
};

class AccessAnalyzerTool : public Tool {
public:
    AccessAnalyzerTool(const ToolConfig& config, StructuredLogger* logger);

    ToolResult execute_operation(const std::string& operation, const nlohmann::json& parameters) override;
    bool authenticate() override;
    bool is_authenticated() const override;
    bool disconnect() override;

private:
    ToolResult analyze_access(const nlohmann::json& parameters);
    std::shared_ptr<PostgreSQLConnection> db_conn_;

    nlohmann::json analyze_user_permissions(const std::string& user_id);
    nlohmann::json detect_privilege_escalation_risks();
    nlohmann::json monitor_access_patterns();
    nlohmann::json generate_access_recommendations();
};

class AuditLoggerTool : public Tool {
public:
    AuditLoggerTool(const ToolConfig& config, StructuredLogger* logger);

    ToolResult execute_operation(const std::string& operation, const nlohmann::json& parameters) override;
    bool authenticate() override;
    bool is_authenticated() const override;
    bool disconnect() override;

private:
    ToolResult log_audit_event(const nlohmann::json& parameters);
    std::shared_ptr<PostgreSQLConnection> db_conn_;

    nlohmann::json log_security_event(const std::string& event_type,
                                    const std::string& severity,
                                    const nlohmann::json& details);
    nlohmann::json log_compliance_event(const std::string& regulation,
                                      const std::string& status,
                                      const nlohmann::json& details);
    nlohmann::json query_audit_logs(const std::string& filter_criteria);
};

// ============================================================================
// MONITORING TOOLS
// ============================================================================

class SystemMonitorTool : public Tool {
public:
    SystemMonitorTool(const ToolConfig& config, StructuredLogger* logger);

    ToolResult execute_operation(const std::string& operation, const nlohmann::json& parameters) override;
    bool authenticate() override;
    bool is_authenticated() const override;
    bool disconnect() override;

private:
    ToolResult monitor_system(const nlohmann::json& parameters);
    std::shared_ptr<PostgreSQLConnection> db_conn_;

    nlohmann::json collect_system_metrics();
    nlohmann::json monitor_database_performance();
    nlohmann::json track_api_usage();
    nlohmann::json analyze_system_health();
};

class PerformanceTrackerTool : public Tool {
public:
    PerformanceTrackerTool(const ToolConfig& config, StructuredLogger* logger);

    ToolResult execute_operation(const std::string& operation, const nlohmann::json& parameters) override;
    bool authenticate() override;
    bool is_authenticated() const override;
    bool disconnect() override;

private:
    ToolResult track_performance(const nlohmann::json& parameters);
    std::shared_ptr<PostgreSQLConnection> db_conn_;

    nlohmann::json measure_rule_engine_performance();
    nlohmann::json track_api_response_times();
    nlohmann::json monitor_memory_usage();
    nlohmann::json analyze_performance_trends();
};

class AlertManagerTool : public Tool {
public:
    AlertManagerTool(const ToolConfig& config, StructuredLogger* logger);

    ToolResult execute_operation(const std::string& operation, const nlohmann::json& parameters) override;
    bool authenticate() override;
    bool is_authenticated() const override;
    bool disconnect() override;

private:
    ToolResult manage_alerts(const nlohmann::json& parameters);
    std::shared_ptr<PostgreSQLConnection> db_conn_;

    nlohmann::json create_alert_rule(const nlohmann::json& rule_config);
    nlohmann::json trigger_alert(const std::string& alert_type,
                               const std::string& severity,
                               const nlohmann::json& details);
    nlohmann::json get_active_alerts();
    nlohmann::json acknowledge_alert(const std::string& alert_id);
};

class HealthCheckerTool : public Tool {
public:
    HealthCheckerTool(const ToolConfig& config, StructuredLogger* logger);

    ToolResult execute_operation(const std::string& operation, const nlohmann::json& parameters) override;
    bool authenticate() override;
    bool is_authenticated() const override;
    bool disconnect() override;

private:
    ToolResult check_health(const nlohmann::json& parameters);
    std::shared_ptr<PostgreSQLConnection> db_conn_;

    nlohmann::json check_database_health();
    nlohmann::json check_service_health();
    nlohmann::json validate_configuration_health();
    nlohmann::json perform_system_diagnostics();
};

// ============================================================================
// TOOL REGISTRY AND FACTORY
// ============================================================================

class ToolRegistry {
public:
    static ToolRegistry& get_instance();

    void register_analytics_tools(std::shared_ptr<PostgreSQLConnection> db_conn,
                                 std::shared_ptr<StructuredLogger> logger);
    void register_workflow_tools(std::shared_ptr<PostgreSQLConnection> db_conn,
                                std::shared_ptr<StructuredLogger> logger);
    void register_security_tools(std::shared_ptr<PostgreSQLConnection> db_conn,
                                std::shared_ptr<StructuredLogger> logger);
    void register_monitoring_tools(std::shared_ptr<PostgreSQLConnection> db_conn,
                                  std::shared_ptr<StructuredLogger> logger);

    std::shared_ptr<Tool> get_tool(const std::string& tool_name);
    std::vector<std::string> get_available_tools();
    std::vector<std::string> get_tools_by_category(ToolCategory category);

private:
    ToolRegistry() = default;
    std::unordered_map<std::string, std::shared_ptr<Tool>> tools_;
    std::mutex registry_mutex_;
};

} // namespace regulens

#endif // TOOL_CATEGORIES_HPP
