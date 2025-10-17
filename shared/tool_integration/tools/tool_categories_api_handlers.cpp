/**
 * Tool Categories API Handlers Implementation
 * REST API endpoints for tool category management and execution
 */

#include "tool_categories_api_handlers.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace regulens {

ToolCategoriesAPIHandlers::ToolCategoriesAPIHandlers(
    std::shared_ptr<PostgreSQLConnection> db_conn
) : db_conn_(db_conn), tool_registry_(ToolRegistry::get_instance()) {

    spdlog::info("ToolCategoriesAPIHandlers initialized");
}

ToolCategoriesAPIHandlers::~ToolCategoriesAPIHandlers() {
    spdlog::info("ToolCategoriesAPIHandlers shutting down");
}

std::string ToolCategoriesAPIHandlers::handle_register_tools(const std::string& request_body, const std::string& user_id) {
    try {
        if (!is_admin_user(user_id)) {
            return create_error_response("Admin access required", 403).dump();
        }

        nlohmann::json request = nlohmann::json::parse(request_body);
        std::vector<std::string> categories = request.value("categories", std::vector<std::string>{});

        // Initialize logger (simplified - in real implementation would get from dependency injection)
        StructuredLogger* logger = &StructuredLogger::get_instance();

        int tools_registered = 0;

        if (std::find(categories.begin(), categories.end(), "analytics") != categories.end()) {
            tool_registry_.register_analytics_tools(db_conn_->get_connection(), logger);
            tools_registered += 4;
        }

        if (std::find(categories.begin(), categories.end(), "workflow") != categories.end()) {
            tool_registry_.register_workflow_tools(db_conn_->get_connection(), logger);
            tools_registered += 3;
        }

        if (std::find(categories.begin(), categories.end(), "security") != categories.end()) {
            tool_registry_.register_security_tools(db_conn_->get_connection(), logger);
            tools_registered += 4;
        }

        if (std::find(categories.begin(), categories.end(), "monitoring") != categories.end()) {
            tool_registry_.register_monitoring_tools(db_conn_->get_connection(), logger);
            tools_registered += 4;
        }

        nlohmann::json response_data = {
            {"tools_registered", tools_registered},
            {"categories_registered", categories}
        };

        return create_success_response(response_data, "Tools registered successfully").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_register_tools: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string ToolCategoriesAPIHandlers::handle_get_available_tools(const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "list_tools", "")) {
            return create_error_response("Access denied", 403).dump();
        }

        auto tools = tool_registry_.get_available_tools();

        return create_tool_list_response(tools).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_available_tools: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string ToolCategoriesAPIHandlers::handle_get_tools_by_category(const std::string& category_str, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "list_tools", "")) {
            return create_error_response("Access denied", 403).dump();
        }

        ToolCategory category = parse_tool_category(category_str);
        auto tools = tool_registry_.get_tools_by_category(category);

        nlohmann::json response_data = {
            {"category", category_str},
            {"tools", tools},
            {"count", tools.size()}
        };

        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_tools_by_category: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string ToolCategoriesAPIHandlers::handle_execute_tool(const std::string& tool_name, const std::string& request_body, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, tool_name, "execute")) {
            return create_error_response("Access denied", 403).dump();
        }

        nlohmann::json request = nlohmann::json::parse(request_body);
        std::string validation_error;
        if (!validate_tool_request(request, tool_name, validation_error)) {
            return create_error_response(validation_error).dump();
        }

        // Determine tool category and execute
        ToolResult result;
        if (tool_name == "data_analyzer" || tool_name == "report_generator" ||
            tool_name == "dashboard_builder" || tool_name == "predictive_model") {
            result = execute_analytics_tool(tool_name, request);
        } else if (tool_name == "task_automator" || tool_name == "process_optimizer" ||
                   tool_name == "approval_workflow") {
            result = execute_workflow_tool(tool_name, request);
        } else if (tool_name == "vulnerability_scanner" || tool_name == "compliance_checker" ||
                   tool_name == "access_analyzer" || tool_name == "audit_logger") {
            result = execute_security_tool(tool_name, request);
        } else if (tool_name == "system_monitor" || tool_name == "performance_tracker" ||
                   tool_name == "alert_manager" || tool_name == "health_checker") {
            result = execute_monitoring_tool(tool_name, request);
        } else {
            return create_error_response("Unknown tool: " + tool_name, 404).dump();
        }

        // Store execution result
        store_tool_execution_result(tool_name, user_id, result);

        // Log execution
        log_tool_execution(tool_name, user_id, result.success);

        return format_tool_result(result).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_execute_tool: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string ToolCategoriesAPIHandlers::handle_get_tool_info(const std::string& tool_name, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, tool_name, "info")) {
            return create_error_response("Access denied", 403).dump();
        }

        auto tool = tool_registry_.get_tool(tool_name);
        if (!tool) {
            return create_error_response("Tool not found: " + tool_name, 404).dump();
        }

        nlohmann::json response_data = format_tool_info(tool_name, tool);
        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_tool_info: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

// Analytics tool handlers
std::string ToolCategoriesAPIHandlers::handle_analyze_dataset(const std::string& request_body, const std::string& user_id) {
    return handle_execute_tool("data_analyzer", request_body, user_id);
}

std::string ToolCategoriesAPIHandlers::handle_generate_report(const std::string& request_body, const std::string& user_id) {
    return handle_execute_tool("report_generator", request_body, user_id);
}

std::string ToolCategoriesAPIHandlers::handle_build_dashboard(const std::string& request_body, const std::string& user_id) {
    return handle_execute_tool("dashboard_builder", request_body, user_id);
}

std::string ToolCategoriesAPIHandlers::handle_run_prediction(const std::string& request_body, const std::string& user_id) {
    return handle_execute_tool("predictive_model", request_body, user_id);
}

// Workflow tool handlers
std::string ToolCategoriesAPIHandlers::handle_automate_task(const std::string& request_body, const std::string& user_id) {
    return handle_execute_tool("task_automator", request_body, user_id);
}

std::string ToolCategoriesAPIHandlers::handle_optimize_process(const std::string& request_body, const std::string& user_id) {
    return handle_execute_tool("process_optimizer", request_body, user_id);
}

std::string ToolCategoriesAPIHandlers::handle_manage_approval(const std::string& request_body, const std::string& user_id) {
    return handle_execute_tool("approval_workflow", request_body, user_id);
}

// Security tool handlers
std::string ToolCategoriesAPIHandlers::handle_scan_vulnerabilities(const std::string& request_body, const std::string& user_id) {
    return handle_execute_tool("vulnerability_scanner", request_body, user_id);
}

std::string ToolCategoriesAPIHandlers::handle_check_compliance(const std::string& request_body, const std::string& user_id) {
    return handle_execute_tool("compliance_checker", request_body, user_id);
}

std::string ToolCategoriesAPIHandlers::handle_analyze_access(const std::string& request_body, const std::string& user_id) {
    return handle_execute_tool("access_analyzer", request_body, user_id);
}

std::string ToolCategoriesAPIHandlers::handle_log_audit_event(const std::string& request_body, const std::string& user_id) {
    return handle_execute_tool("audit_logger", request_body, user_id);
}

// Monitoring tool handlers
std::string ToolCategoriesAPIHandlers::handle_monitor_system(const std::string& request_body, const std::string& user_id) {
    return handle_execute_tool("system_monitor", request_body, user_id);
}

std::string ToolCategoriesAPIHandlers::handle_track_performance(const std::string& request_body, const std::string& user_id) {
    return handle_execute_tool("performance_tracker", request_body, user_id);
}

std::string ToolCategoriesAPIHandlers::handle_manage_alerts(const std::string& request_body, const std::string& user_id) {
    return handle_execute_tool("alert_manager", request_body, user_id);
}

std::string ToolCategoriesAPIHandlers::handle_check_health(const std::string& request_body, const std::string& user_id) {
    return handle_execute_tool("health_checker", request_body, user_id);
}

// Helper method implementations

ToolCategory ToolCategoriesAPIHandlers::parse_tool_category(const std::string& category_str) {
    if (category_str == "analytics") return ToolCategory::ANALYTICS;
    if (category_str == "workflow") return ToolCategory::WORKFLOW;
    if (category_str == "security") return ToolCategory::SECURITY;
    if (category_str == "monitoring") return ToolCategory::MONITORING;
    return ToolCategory::ANALYTICS; // default
}

nlohmann::json ToolCategoriesAPIHandlers::format_tool_result(const ToolResult& result) {
    nlohmann::json response = {
        {"success", result.success},
        {"data", result.data},
        {"execution_time_ms", result.execution_time.count()},
        {"retry_count", result.retry_count}
    };

    if (!result.error_message.empty()) {
        response["error_message"] = result.error_message;
    }

    if (!result.metadata.empty()) {
        response["metadata"] = result.metadata;
    }

    return create_success_response(response);
}

nlohmann::json ToolCategoriesAPIHandlers::format_tool_info(const std::string& tool_name, Tool* tool) {
    return {
        {"tool_name", tool_name},
        {"description", tool->get_description()},
        {"required_parameters", tool->get_required_parameters()}
    };
}

bool ToolCategoriesAPIHandlers::validate_tool_request(const nlohmann::json& request, const std::string& tool_name, std::string& error_message) {
    auto tool = tool_registry_.get_tool(tool_name);
    if (!tool) {
        error_message = "Tool not found: " + tool_name;
        return false;
    }

    auto required_params = tool->get_required_parameters();
    for (const auto& param : required_params) {
        if (!request.contains(param)) {
            error_message = "Missing required parameter: " + param;
            return false;
        }
    }

    return true;
}

bool ToolCategoriesAPIHandlers::validate_user_access(const std::string& user_id, const std::string& tool_name, const std::string& operation) {
    if (user_id.empty()) {
        spdlog::warn("Access denied: empty user_id");
        return false;
    }

    try {
        if (!db_conn_) {
            spdlog::error("Database connection not available for access validation");
            return false;
        }

        // Query user permissions from database
        std::string query = R"(
            SELECT p.operation, p.resource_type, p.resource_id, p.permission_level
            FROM user_permissions p
            INNER JOIN users u ON u.id = p.user_id
            WHERE u.user_id = $1 AND u.is_active = true AND p.is_active = true
        )";

        auto results = db_conn_->execute_query_multi(query, {user_id});

        // Check if user has permission for this tool/operation
        for (const auto& row : results) {
            std::string perm_operation = row.at("operation");
            std::string perm_resource_id = row.at("resource_id");
            std::string perm_resource_type = row.at("resource_type");

            // Check for wildcard permission or exact match
            if (perm_operation == operation || perm_operation == "*" || perm_operation == "execute_tool") {
                // Check resource-specific permissions (tool name match)
                if (perm_resource_type == "tool" && (perm_resource_id == tool_name || perm_resource_id == "*")) {
                    spdlog::debug("Access granted for user: {} tool: {} operation: {}",
                                 user_id, tool_name, operation);
                    return true;
                }
            }
        }

        spdlog::warn("Access denied for user: {} tool: {} operation: {}",
                    user_id, tool_name, operation);
        return false;

    } catch (const std::exception& e) {
        spdlog::error("Access validation error: {}", e.what());
        return false; // Fail closed on errors
    }
}

ToolResult ToolCategoriesAPIHandlers::execute_analytics_tool(const std::string& tool_name, const nlohmann::json& parameters) {
    auto tool = tool_registry_.get_tool(tool_name);
    if (!tool) {
        return ToolResult{false, nlohmann::json{{"tool_name", tool_name}}, "Tool not found"};
    }

    std::string operation;
    if (tool_name == "data_analyzer") {
        operation = "analyze_dataset";
    } else if (tool_name == "report_generator") {
        operation = "generate_report";
    } else if (tool_name == "dashboard_builder") {
        operation = "build_dashboard";
    } else if (tool_name == "predictive_model") {
        operation = "run_prediction";
    } else {
        return ToolResult{false, nlohmann::json{{"tool_name", tool_name}}, "Unknown analytics tool"};
    }

    return tool->execute_operation(operation, parameters);
}

ToolResult ToolCategoriesAPIHandlers::execute_workflow_tool(const std::string& tool_name, const nlohmann::json& parameters) {
    auto tool = tool_registry_.get_tool(tool_name);
    if (!tool) {
        return ToolResult{false, nlohmann::json{{"tool_name", tool_name}}, "Tool not found"};
    }

    std::string operation;
    if (tool_name == "task_automator") {
        operation = "automate_task";
    } else if (tool_name == "process_optimizer") {
        operation = "optimize_process";
    } else if (tool_name == "approval_workflow") {
        operation = "manage_approval";
    } else {
        return ToolResult{false, nlohmann::json{{"tool_name", tool_name}}, "Unknown workflow tool"};
    }

    return tool->execute_operation(operation, parameters);
}

ToolResult ToolCategoriesAPIHandlers::execute_security_tool(const std::string& tool_name, const nlohmann::json& parameters) {
    auto tool = tool_registry_.get_tool(tool_name);
    if (!tool) {
        return ToolResult{false, nlohmann::json{{"tool_name", tool_name}}, "Tool not found"};
    }

    std::string operation;
    if (tool_name == "vulnerability_scanner") {
        operation = "scan_vulnerabilities";
    } else if (tool_name == "compliance_checker") {
        operation = "check_compliance";
    } else if (tool_name == "access_analyzer") {
        operation = "analyze_access";
    } else if (tool_name == "audit_logger") {
        operation = "log_audit_event";
    } else {
        return ToolResult{false, nlohmann::json{{"tool_name", tool_name}}, "Unknown security tool"};
    }

    return tool->execute_operation(operation, parameters);
}

ToolResult ToolCategoriesAPIHandlers::execute_monitoring_tool(const std::string& tool_name, const nlohmann::json& parameters) {
    auto tool = tool_registry_.get_tool(tool_name);
    if (!tool) {
        return ToolResult{false, nlohmann::json{{"tool_name", tool_name}}, "Tool not found"};
    }

    std::string operation;
    if (tool_name == "system_monitor") {
        operation = "monitor_system";
    } else if (tool_name == "performance_tracker") {
        operation = "track_performance";
    } else if (tool_name == "alert_manager") {
        operation = "manage_alerts";
    } else if (tool_name == "health_checker") {
        operation = "check_health";
    } else {
        return ToolResult{false, nlohmann::json{{"tool_name", tool_name}}, "Unknown monitoring tool"};
    }

    return tool->execute_operation(operation, parameters);

    return ToolResult{false, nlohmann::json{{"tool_name", tool_name}}, "Unknown monitoring tool"};
}

nlohmann::json ToolCategoriesAPIHandlers::create_success_response(const nlohmann::json& data, const std::string& message) {
    nlohmann::json response = {
        {"success", true},
        {"status_code", 200}
    };

    if (!message.empty()) {
        response["message"] = message;
    }

    if (data.is_object() || data.is_array()) {
        response["data"] = data;
    }

    return response;
}

nlohmann::json ToolCategoriesAPIHandlers::create_error_response(const std::string& message, int status_code) {
    return {
        {"success", false},
        {"status_code", status_code},
        {"error", message}
    };
}

nlohmann::json ToolCategoriesAPIHandlers::create_tool_list_response(const std::vector<std::string>& tools) {
    nlohmann::json response = {
        {"success", true},
        {"status_code", 200},
        {"data", {
            {"tools", tools},
            {"total_count", tools.size()}
        }}
    };

    return response;
}

void ToolCategoriesAPIHandlers::log_tool_execution(const std::string& tool_name, const std::string& user_id, bool success, const std::string& duration) {
    spdlog::info("Tool execution: {} by user {} - success: {}, duration: {}",
                 tool_name, user_id, success, duration);
}

bool ToolCategoriesAPIHandlers::store_tool_execution_result(const std::string& tool_name, const std::string& user_id, const ToolResult& result) {
    try {
        if (!db_conn_) return false;

        std::string query = R"(
            INSERT INTO tool_execution_results (
                tool_name, user_id, success, message, data, error_details, executed_at
            ) VALUES ($1, $2, $3, $4, $5, $6, NOW())
        )";

        std::vector<std::string> params = {
            tool_name,
            user_id,
            result.success ? "true" : "false",
            result.error_message,
            result.data.dump(),
            result.metadata.empty() ? "{}" : result.metadata.begin()->second // placeholder for error_details
        };

        return db_conn_->execute_command(query, params);

    } catch (const std::exception& e) {
        spdlog::error("Exception in store_tool_execution_result: {}", e.what());
        return false;
    }
}

bool ToolCategoriesAPIHandlers::is_admin_user(const std::string& user_id) {
    if (user_id.empty()) {
        return false;
    }

    try {
        if (!db_conn_) {
            spdlog::error("Database connection not available for admin check");
            return false;
        }

        // Query user role from database
        std::string query = R"(
            SELECT r.role_name, r.role_level
            FROM user_roles ur
            INNER JOIN roles r ON r.id = ur.role_id
            INNER JOIN users u ON u.id = ur.user_id
            WHERE u.user_id = $1 AND ur.is_active = true AND u.is_active = true
            ORDER BY r.role_level DESC
            LIMIT 1
        )";

        auto results = db_conn_->execute_query_multi(query, {user_id});

        if (!results.empty()) {
            const auto& row = results[0];
            std::string role_name = row.at("role_name");
            int role_level = row["role_level"];

            // Admin role has highest privilege level (level >= 90)
            // Or role name is explicitly "administrator" or "super_admin"
            if (role_name == "administrator" || role_name == "super_admin" || role_level >= 90) {
                spdlog::debug("Admin access confirmed for user: {} role: {}", user_id, role_name);
                return true;
            }
        }

        return false;

    } catch (const std::exception& e) {
        spdlog::error("Admin check error: {}", e.what());
        return false; // Fail closed on errors
    }
}

} // namespace regulens
