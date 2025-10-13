/**
 * Tool Categories API Handlers
 * REST API endpoints for tool category management and execution
 */

#ifndef TOOL_CATEGORIES_API_HANDLERS_HPP
#define TOOL_CATEGORIES_API_HANDLERS_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "tool_categories.hpp"
#include "../../database/postgresql_connection.hpp"

namespace regulens {

class ToolCategoriesAPIHandlers {
public:
    ToolCategoriesAPIHandlers(
        std::shared_ptr<PostgreSQLConnection> db_conn
    );

    ~ToolCategoriesAPIHandlers();

    // Tool Registry Management
    std::string handle_register_tools(const std::string& request_body, const std::string& user_id);
    std::string handle_get_available_tools(const std::string& user_id);
    std::string handle_get_tools_by_category(const std::string& category, const std::string& user_id);

    // Analytics Tools Endpoints
    std::string handle_analyze_dataset(const std::string& request_body, const std::string& user_id);
    std::string handle_generate_report(const std::string& request_body, const std::string& user_id);
    std::string handle_build_dashboard(const std::string& request_body, const std::string& user_id);
    std::string handle_run_prediction(const std::string& request_body, const std::string& user_id);

    // Workflow Tools Endpoints
    std::string handle_automate_task(const std::string& request_body, const std::string& user_id);
    std::string handle_optimize_process(const std::string& request_body, const std::string& user_id);
    std::string handle_manage_approval(const std::string& request_body, const std::string& user_id);

    // Security Tools Endpoints
    std::string handle_scan_vulnerabilities(const std::string& request_body, const std::string& user_id);
    std::string handle_check_compliance(const std::string& request_body, const std::string& user_id);
    std::string handle_analyze_access(const std::string& request_body, const std::string& user_id);
    std::string handle_log_audit_event(const std::string& request_body, const std::string& user_id);

    // Monitoring Tools Endpoints
    std::string handle_monitor_system(const std::string& request_body, const std::string& user_id);
    std::string handle_track_performance(const std::string& request_body, const std::string& user_id);
    std::string handle_manage_alerts(const std::string& request_body, const std::string& user_id);
    std::string handle_check_health(const std::string& request_body, const std::string& user_id);

    // Generic Tool Execution
    std::string handle_execute_tool(const std::string& tool_name, const std::string& request_body, const std::string& user_id);
    std::string handle_get_tool_info(const std::string& tool_name, const std::string& user_id);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;

    // Tool registry access
    ToolRegistry& tool_registry_;

    // Helper methods
    ToolCategory parse_tool_category(const std::string& category_str);
    nlohmann::json format_tool_result(const ToolResult& result);
    nlohmann::json format_tool_info(const std::string& tool_name, std::shared_ptr<Tool> tool);

    // Request validation
    bool validate_tool_request(const nlohmann::json& request, const std::string& tool_name, std::string& error_message);
    bool validate_user_access(const std::string& user_id, const std::string& tool_name, const std::string& operation);

    // Tool execution helpers
    ToolResult execute_analytics_tool(const std::string& tool_name, const nlohmann::json& parameters);
    ToolResult execute_workflow_tool(const std::string& tool_name, const nlohmann::json& parameters);
    ToolResult execute_security_tool(const std::string& tool_name, const nlohmann::json& parameters);
    ToolResult execute_monitoring_tool(const std::string& tool_name, const nlohmann::json& parameters);

    // Response formatting
    nlohmann::json create_success_response(const nlohmann::json& data = nullptr,
                                         const std::string& message = "");
    nlohmann::json create_error_response(const std::string& message,
                                       int status_code = 400);
    nlohmann::json create_tool_list_response(const std::vector<std::string>& tools);

    // Logging helpers
    void log_tool_execution(const std::string& tool_name, const std::string& user_id,
                          bool success, const std::string& duration = "");

    // Tool result storage
    bool store_tool_execution_result(const std::string& tool_name, const std::string& user_id,
                                   const ToolResult& result);
    std::vector<nlohmann::json> get_tool_execution_history(const std::string& tool_name,
                                                         const std::string& user_id,
                                                         int limit = 10);
};

} // namespace regulens

#endif // TOOL_CATEGORIES_API_HANDLERS_HPP
