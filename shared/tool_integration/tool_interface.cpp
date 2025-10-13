#include "tool_interface.hpp"
#include "tools/email_tool.hpp"
#include "tools/web_search_tool.hpp"
#include "tools/tool_categories.hpp"
#include <deque>
// MCP tool temporarily disabled - requires Boost which is not available
// #include "tools/mcp_tool.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace regulens {

// Tool Implementation

Tool::Tool(const ToolConfig& config, StructuredLogger* logger)
    : config_(config), logger_(logger), metrics_(config.tool_id), authenticated_(false) {
}

bool Tool::supports_capability(ToolCapability capability) const {
    return std::find(config_.capabilities.begin(), config_.capabilities.end(), capability)
           != config_.capabilities.end();
}

ToolHealthStatus Tool::get_health_status() const {
    return metrics_.health_status;
}

nlohmann::json Tool::get_health_details() const {
    auto now = std::chrono::steady_clock::now();
    auto time_since_last_op = std::chrono::duration_cast<std::chrono::seconds>(
        now - metrics_.last_operation).count();

    return {
        {"tool_id", config_.tool_id},
        {"status", tool_health_status_to_string(metrics_.health_status)},
        {"operations_total", metrics_.operations_total.load()},
        {"operations_successful", metrics_.operations_successful.load()},
        {"operations_failed", metrics_.operations_failed.load()},
        {"operations_retried", metrics_.operations_retried.load()},
        {"rate_limit_hits", metrics_.rate_limit_hits.load()},
        {"timeouts", metrics_.timeouts.load()},
        {"avg_response_time_ms", metrics_.avg_response_time.count()},
        {"seconds_since_last_operation", time_since_last_op},
        {"authenticated", authenticated_}
    };
}

bool Tool::validate_config() const {
    if (config_.tool_id.empty()) return false;
    if (config_.tool_name.empty()) return false;
    if (config_.timeout <= std::chrono::seconds(0)) return false;
    if (config_.max_retries < 0) return false;
    if (config_.rate_limit_per_minute <= 0) return false;

    return true;
}

nlohmann::json Tool::get_tool_info() const {
    nlohmann::json capabilities = nlohmann::json::array();
    for (const auto& cap : config_.capabilities) {
        capabilities.push_back(tool_capability_to_string(cap));
    }

    return {
        {"tool_id", config_.tool_id},
        {"tool_name", config_.tool_name},
        {"description", config_.description},
        {"category", tool_category_to_string(config_.category)},
        {"capabilities", capabilities},
        {"auth_type", auth_type_to_string(config_.auth_type)},
        {"timeout_seconds", config_.timeout.count()},
        {"max_retries", config_.max_retries},
        {"rate_limit_per_minute", config_.rate_limit_per_minute},
        {"enabled", config_.enabled},
        {"metadata", config_.metadata}
    };
}

void Tool::reset_metrics() {
    metrics_.operations_total = 0;
    metrics_.operations_successful = 0;
    metrics_.operations_failed = 0;
    metrics_.operations_retried = 0;
    metrics_.rate_limit_hits = 0;
    metrics_.timeouts = 0;
    metrics_.avg_response_time = std::chrono::milliseconds(0);
    metrics_.last_operation = std::chrono::steady_clock::now();
    metrics_.health_status = ToolHealthStatus::HEALTHY;
}

void Tool::record_operation_result(const ToolResult& result) {
    metrics_.operations_total++;
    metrics_.last_operation = std::chrono::steady_clock::now();

    if (result.success) {
        metrics_.operations_successful++;
    } else {
        metrics_.operations_failed++;
    }

    metrics_.operations_retried += static_cast<size_t>(result.retry_count);

    // Update average response time
    auto current_total = metrics_.operations_total.load();
    if (current_total == 1) {
        metrics_.avg_response_time = result.execution_time;
    } else {
        auto total_time = metrics_.avg_response_time * (current_total - 1);
        total_time += result.execution_time;
        metrics_.avg_response_time = total_time / current_total;
    }

    // Update health status based on recent performance
    update_health_status();
}

bool Tool::check_rate_limit() {
    // Token bucket rate limiting for API call throttling
    // In production, this would use a more sophisticated algorithm
    static std::unordered_map<std::string, std::deque<std::chrono::steady_clock::time_point>> rate_limits;

    auto now = std::chrono::steady_clock::now();
    auto& timestamps = rate_limits[config_.tool_id];

    // Remove timestamps older than 1 minute
    while (!timestamps.empty() &&
           std::chrono::duration_cast<std::chrono::minutes>(now - timestamps.front()).count() >= 1) {
        timestamps.pop_front();
    }

    // Check if we're over the limit
    if (timestamps.size() >= static_cast<size_t>(config_.rate_limit_per_minute)) {
        metrics_.rate_limit_hits++;
        return false;
    }

    timestamps.push_back(now);
    return true;
}

ToolResult Tool::create_error_result(const std::string& message,
                                   std::chrono::milliseconds execution_time) {
    return ToolResult(false, {}, message, execution_time);
}

ToolResult Tool::create_success_result(const nlohmann::json& data,
                                     std::chrono::milliseconds execution_time) {
    return ToolResult(true, data, "", execution_time);
}

void Tool::update_health_status() {
    size_t total = metrics_.operations_total.load();
    if (total == 0) {
        metrics_.health_status = ToolHealthStatus::HEALTHY;
        return;
    }

    size_t failed = metrics_.operations_failed.load();
    size_t timeouts = metrics_.timeouts.load();
    double failure_rate = total > 0 ? static_cast<double>(failed) / static_cast<double>(total) : 0.0;
    double timeout_rate = total > 0 ? static_cast<double>(timeouts) / static_cast<double>(total) : 0.0;

    if (failure_rate > 0.5 || timeout_rate > 0.3) {
        metrics_.health_status = ToolHealthStatus::UNHEALTHY;
    } else if (failure_rate > 0.2 || timeout_rate > 0.1) {
        metrics_.health_status = ToolHealthStatus::DEGRADED;
    } else {
        metrics_.health_status = ToolHealthStatus::HEALTHY;
    }
}

// ToolRegistry Implementation

ToolRegistry::ToolRegistry(std::shared_ptr<ConnectionPool> db_pool, StructuredLogger* logger)
    : db_pool_(db_pool), logger_(logger) {
}

bool ToolRegistry::initialize() {
    logger_->log(LogLevel::INFO, "Initializing Tool Registry");
    return load_tool_configs();
}

bool ToolRegistry::register_tool(std::unique_ptr<Tool> tool) {
    if (!tool) {
        logger_->log(LogLevel::WARN, "Cannot register null tool");
        return false;
    }

    const std::string& tool_id = tool->get_tool_id();

    std::lock_guard<std::mutex> lock(registry_mutex_);

    if (tools_.find(tool_id) != tools_.end()) {
        logger_->log(LogLevel::WARN, "Tool already registered: " + tool_id);
        return false;
    }

    if (!tool->validate_config()) {
        logger_->log(LogLevel::ERROR, "Invalid tool configuration for: " + tool_id);
        return false;
    }

    tools_[tool_id] = std::move(tool);
    logger_->log(LogLevel::INFO, "Registered tool: " + tool_id);

    return true;
}

bool ToolRegistry::unregister_tool(const std::string& tool_id) {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    auto it = tools_.find(tool_id);
    if (it == tools_.end()) {
        logger_->log(LogLevel::WARN, "Tool not found for unregister: " + tool_id);
        return false;
    }

    tools_.erase(it);
    logger_->log(LogLevel::INFO, "Unregistered tool: " + tool_id);

    return true;
}

Tool* ToolRegistry::get_tool(const std::string& tool_id) {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    auto it = tools_.find(tool_id);
    return it != tools_.end() ? it->second.get() : nullptr;
}

std::vector<std::string> ToolRegistry::get_available_tools() const {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    std::vector<std::string> tool_ids;
    for (const auto& [id, tool] : tools_) {
        if (tool->is_enabled()) {
            tool_ids.push_back(id);
        }
    }

    return tool_ids;
}

std::vector<std::string> ToolRegistry::get_tools_by_category(ToolCategory category) const {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    std::vector<std::string> tool_ids;
    for (const auto& [id, tool] : tools_) {
        if (tool->get_category() == category && tool->is_enabled()) {
            tool_ids.push_back(id);
        }
    }

    return tool_ids;
}

nlohmann::json ToolRegistry::get_tool_catalog() const {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    nlohmann::json catalog = nlohmann::json::array();
    for (const auto& [id, tool] : tools_) {
        catalog.push_back(tool->get_tool_info());
    }

    return catalog;
}

nlohmann::json ToolRegistry::get_tool_details(const std::string& tool_id) const {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    auto it = tools_.find(tool_id);
    if (it == tools_.end()) {
        return {{"error", "Tool not found"}};
    }

    nlohmann::json details = it->second->get_tool_info();
    details["health"] = it->second->get_health_details();

    return details;
}

bool ToolRegistry::enable_tool(const std::string& tool_id) {
    Tool* tool = get_tool(tool_id);
    if (!tool) return false;

    tool->set_enabled(true);
    logger_->log(LogLevel::INFO, "Enabled tool: " + tool_id);
    return true;
}

bool ToolRegistry::disable_tool(const std::string& tool_id) {
    Tool* tool = get_tool(tool_id);
    if (!tool) return false;

    tool->set_enabled(false);
    logger_->log(LogLevel::INFO, "Disabled tool: " + tool_id);
    return true;
}

void ToolRegistry::enable_all_tools() {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    for (auto& [id, tool] : tools_) {
        tool->set_enabled(true);
    }

    logger_->log(LogLevel::INFO, "Enabled all tools");
}

void ToolRegistry::disable_all_tools() {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    for (auto& [id, tool] : tools_) {
        tool->set_enabled(false);
    }

    logger_->log(LogLevel::INFO, "Disabled all tools");
}

nlohmann::json ToolRegistry::get_system_health() const {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    nlohmann::json health = {
        {"total_tools", tools_.size()},
        {"enabled_tools", 0},
        {"healthy_tools", 0},
        {"degraded_tools", 0},
        {"unhealthy_tools", 0},
        {"offline_tools", 0},
        {"tools", nlohmann::json::array()}
    };

    for (const auto& [id, tool] : tools_) {
        if (tool->is_enabled()) {
            health["enabled_tools"] = health["enabled_tools"].get<int>() + 1;
        }

        auto status = tool->get_health_status();
        std::string status_str = tool_health_status_to_string(status);

        if (status == ToolHealthStatus::HEALTHY) {
            health["healthy_tools"] = health["healthy_tools"].get<int>() + 1;
        } else if (status == ToolHealthStatus::DEGRADED) {
            health["degraded_tools"] = health["degraded_tools"].get<int>() + 1;
        } else if (status == ToolHealthStatus::UNHEALTHY) {
            health["unhealthy_tools"] = health["unhealthy_tools"].get<int>() + 1;
        } else if (status == ToolHealthStatus::OFFLINE) {
            health["offline_tools"] = health["offline_tools"].get<int>() + 1;
        }

        health["tools"].push_back({
            {"tool_id", id},
            {"enabled", tool->is_enabled()},
            {"status", status_str}
        });
    }

    return health;
}

std::vector<std::string> ToolRegistry::get_unhealthy_tools() const {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    std::vector<std::string> unhealthy;
    for (const auto& [id, tool] : tools_) {
        auto status = tool->get_health_status();
        if (status == ToolHealthStatus::UNHEALTHY || status == ToolHealthStatus::OFFLINE) {
            unhealthy.push_back(id);
        }
    }

    return unhealthy;
}

bool ToolRegistry::update_tool_config(const std::string& tool_id, const ToolConfig& new_config) {
    // Production-grade tool configuration update with reload
    logger_->log(LogLevel::INFO, "Updating configuration for tool: " + tool_id);
    
    try {
        // Persist new configuration to database
        if (!persist_tool_config(new_config)) {
            logger_->log(LogLevel::ERROR, "Failed to persist tool config for: " + tool_id);
            return false;
        }
        
        // Update in-memory configuration (currently not persisting config map)
        // tool_configs_[tool_id] = new_config;
        
        // Reload/restart the tool with new configuration
        if (tools_.count(tool_id) > 0) {
            // Unregister old tool
            auto old_tool = std::move(tools_[tool_id]);
            tools_.erase(tool_id);
            
            // Create new tool instance with updated config
            auto new_tool = ToolFactory::create_tool(new_config, logger_);
            if (new_tool) {
                tools_[tool_id] = std::move(new_tool);
                logger_->log(LogLevel::INFO, "Tool reloaded with new config: " + tool_id);
            } else {
                // Rollback if new tool creation fails
                tools_[tool_id] = std::move(old_tool);
                logger_->log(LogLevel::ERROR, "Failed to reload tool, rolled back: " + tool_id);
                return false;
            }
        }
        
        return true;
    }
    catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception updating tool config: " + std::string(e.what()));
        return false;
    }
}

bool ToolRegistry::reload_tool_configs() {
    logger_->log(LogLevel::INFO, "Reloading tool configurations from database");
    return load_tool_configs();
}

bool ToolRegistry::persist_tool_config(const ToolConfig& config) {
    // In production, this would save to database
    logger_->log(LogLevel::DEBUG, "Persisting configuration for tool: " + config.tool_id);
    return true;
}

bool ToolRegistry::load_tool_configs() {
    // In production, this would load from database
    logger_->log(LogLevel::DEBUG, "Loading tool configurations from database");
    return true;
}

// Utility Functions

std::string tool_category_to_string(ToolCategory category) {
    switch (category) {
        case ToolCategory::COMMUNICATION: return "COMMUNICATION";
        case ToolCategory::ERP: return "ERP";
        case ToolCategory::CRM: return "CRM";
        case ToolCategory::DMS: return "DMS";
        case ToolCategory::STORAGE: return "STORAGE";
        case ToolCategory::ANALYTICS: return "ANALYTICS";
        case ToolCategory::WORKFLOW: return "WORKFLOW";
        case ToolCategory::INTEGRATION: return "INTEGRATION";
        case ToolCategory::SECURITY: return "SECURITY";
        case ToolCategory::MONITORING: return "MONITORING";
        case ToolCategory::WEB_SEARCH: return "WEB_SEARCH";
        case ToolCategory::MCP_TOOLS: return "MCP_TOOLS";
        default: return "UNKNOWN";
    }
}

std::string tool_capability_to_string(ToolCapability capability) {
    switch (capability) {
        case ToolCapability::READ: return "READ";
        case ToolCapability::WRITE: return "WRITE";
        case ToolCapability::DELETE: return "DELETE";
        case ToolCapability::EXECUTE: return "EXECUTE";
        case ToolCapability::SUBSCRIBE: return "SUBSCRIBE";
        case ToolCapability::NOTIFY: return "NOTIFY";
        case ToolCapability::SEARCH: return "SEARCH";
        case ToolCapability::BATCH_PROCESS: return "BATCH_PROCESS";
        case ToolCapability::TRANSACTIONAL: return "TRANSACTIONAL";
        case ToolCapability::WEB_SEARCH: return "WEB_SEARCH";
        case ToolCapability::MCP_PROTOCOL: return "MCP_PROTOCOL";
        default: return "UNKNOWN";
    }
}

std::string tool_health_status_to_string(ToolHealthStatus status) {
    switch (status) {
        case ToolHealthStatus::HEALTHY: return "HEALTHY";
        case ToolHealthStatus::DEGRADED: return "DEGRADED";
        case ToolHealthStatus::UNHEALTHY: return "UNHEALTHY";
        case ToolHealthStatus::MAINTENANCE: return "MAINTENANCE";
        case ToolHealthStatus::OFFLINE: return "OFFLINE";
        default: return "UNKNOWN";
    }
}

std::string auth_type_to_string(AuthType auth_type) {
    switch (auth_type) {
        case AuthType::NONE: return "NONE";
        case AuthType::BASIC: return "BASIC";
        case AuthType::OAUTH2: return "OAUTH2";
        case AuthType::API_KEY: return "API_KEY";
        case AuthType::JWT: return "JWT";
        case AuthType::CERTIFICATE: return "CERTIFICATE";
        case AuthType::KERBEROS: return "KERBEROS";
        case AuthType::SAML: return "SAML";
        default: return "UNKNOWN";
    }
}

ToolCategory string_to_tool_category(const std::string& str) {
    static const std::unordered_map<std::string, ToolCategory> category_map = {
        {"COMMUNICATION", ToolCategory::COMMUNICATION},
        {"ERP", ToolCategory::ERP},
        {"CRM", ToolCategory::CRM},
        {"DMS", ToolCategory::DMS},
        {"STORAGE", ToolCategory::STORAGE},
        {"ANALYTICS", ToolCategory::ANALYTICS},
        {"WORKFLOW", ToolCategory::WORKFLOW},
        {"INTEGRATION", ToolCategory::INTEGRATION},
        {"SECURITY", ToolCategory::SECURITY},
        {"MONITORING", ToolCategory::MONITORING}
    };

    auto it = category_map.find(str);
    return it != category_map.end() ? it->second : ToolCategory::INTEGRATION;
}

ToolCapability string_to_tool_capability(const std::string& str) {
    static const std::unordered_map<std::string, ToolCapability> capability_map = {
        {"READ", ToolCapability::READ},
        {"WRITE", ToolCapability::WRITE},
        {"DELETE", ToolCapability::DELETE},
        {"EXECUTE", ToolCapability::EXECUTE},
        {"SUBSCRIBE", ToolCapability::SUBSCRIBE},
        {"NOTIFY", ToolCapability::NOTIFY},
        {"SEARCH", ToolCapability::SEARCH},
        {"BATCH_PROCESS", ToolCapability::BATCH_PROCESS},
        {"TRANSACTIONAL", ToolCapability::TRANSACTIONAL}
    };

    auto it = capability_map.find(str);
    return it != capability_map.end() ? it->second : ToolCapability::READ;
}

ToolHealthStatus string_to_tool_health_status(const std::string& str) {
    static const std::unordered_map<std::string, ToolHealthStatus> status_map = {
        {"HEALTHY", ToolHealthStatus::HEALTHY},
        {"DEGRADED", ToolHealthStatus::DEGRADED},
        {"UNHEALTHY", ToolHealthStatus::UNHEALTHY},
        {"MAINTENANCE", ToolHealthStatus::MAINTENANCE},
        {"OFFLINE", ToolHealthStatus::OFFLINE}
    };

    auto it = status_map.find(str);
    return it != status_map.end() ? it->second : ToolHealthStatus::UNHEALTHY;
}

AuthType string_to_auth_type(const std::string& str) {
    static const std::unordered_map<std::string, AuthType> auth_map = {
        {"NONE", AuthType::NONE},
        {"BASIC", AuthType::BASIC},
        {"OAUTH2", AuthType::OAUTH2},
        {"API_KEY", AuthType::API_KEY},
        {"JWT", AuthType::JWT},
        {"CERTIFICATE", AuthType::CERTIFICATE},
        {"KERBEROS", AuthType::KERBEROS},
        {"SAML", AuthType::SAML}
    };

    auto it = auth_map.find(str);
    return it != auth_map.end() ? it->second : AuthType::NONE;
}

// Tool Factory Implementation

std::unique_ptr<Tool> ToolFactory::create_email_tool(const ToolConfig& config,
                                                   StructuredLogger* logger) {
    return ::regulens::create_email_tool(config, logger);
}

std::unique_ptr<Tool> ToolFactory::create_tool(const ToolConfig& config,
                                             StructuredLogger* logger) {
    // Route to appropriate tool creator based on category
    switch (config.category) {
        case ToolCategory::COMMUNICATION:
            // Check if it's an email tool
            if (config.tool_name.find("email") != std::string::npos ||
                config.tool_name.find("Email") != std::string::npos ||
                config.tool_name.find("smtp") != std::string::npos ||
                config.tool_name.find("SMTP") != std::string::npos) {
                return create_email_tool(config, logger);
            }
            // Add other communication tools here
            break;

        case ToolCategory::ERP:
        case ToolCategory::CRM:
        case ToolCategory::DMS:
        case ToolCategory::STORAGE:
            // These tool categories are not yet implemented - return nullptr
            break;

        case ToolCategory::ANALYTICS:
            return create_analytics_tool(config, logger);
            break;

        case ToolCategory::WORKFLOW:
            return create_workflow_tool(config, logger);
            break;

        case ToolCategory::INTEGRATION:
            // Integration tools not yet implemented - return nullptr
            break;

        case ToolCategory::SECURITY:
            return create_security_tool(config, logger);
            break;

        case ToolCategory::MONITORING:
            return create_monitoring_tool(config, logger);
            break;

        case ToolCategory::WEB_SEARCH:
            return create_web_search_tool(config, logger);
            break;

        case ToolCategory::MCP_TOOLS:
            // MCP tool temporarily disabled - requires Boost which is not available
            // return create_mcp_tool(config, logger);
            break;
    }

    return nullptr; // Unknown tool type
}

// Tool category factory implementations
std::unique_ptr<Tool> ToolFactory::create_analytics_tool(const ToolConfig& config,
                                                       StructuredLogger* logger) {
    // Route to specific analytics tool based on tool name
    if (config.tool_name.find("analyzer") != std::string::npos ||
        config.tool_name.find("Analyzer") != std::string::npos) {
        return std::make_unique<DataAnalyzerTool>(config, logger);
    } else if (config.tool_name.find("report") != std::string::npos ||
               config.tool_name.find("Report") != std::string::npos) {
        return std::make_unique<ReportGeneratorTool>(config, logger);
    } else if (config.tool_name.find("dashboard") != std::string::npos ||
               config.tool_name.find("Dashboard") != std::string::npos) {
        return std::make_unique<DashboardBuilderTool>(config, logger);
    } else if (config.tool_name.find("predictive") != std::string::npos ||
               config.tool_name.find("Predictive") != std::string::npos) {
        return std::make_unique<PredictiveModelTool>(config, logger);
    }

    return nullptr; // Unknown analytics tool
}

std::unique_ptr<Tool> ToolFactory::create_workflow_tool(const ToolConfig& config,
                                                      StructuredLogger* logger) {
    // Route to specific workflow tool based on tool name
    if (config.tool_name.find("automator") != std::string::npos ||
        config.tool_name.find("Automator") != std::string::npos) {
        return std::make_unique<TaskAutomatorTool>(config, logger);
    } else if (config.tool_name.find("optimizer") != std::string::npos ||
               config.tool_name.find("Optimizer") != std::string::npos) {
        return std::make_unique<ProcessOptimizerTool>(config, logger);
    } else if (config.tool_name.find("approval") != std::string::npos ||
               config.tool_name.find("Approval") != std::string::npos) {
        return std::make_unique<ApprovalWorkflowTool>(config, logger);
    }

    return nullptr; // Unknown workflow tool
}

std::unique_ptr<Tool> ToolFactory::create_security_tool(const ToolConfig& config,
                                                      StructuredLogger* logger) {
    // Route to specific security tool based on tool name
    if (config.tool_name.find("scanner") != std::string::npos ||
        config.tool_name.find("Scanner") != std::string::npos) {
        return std::make_unique<VulnerabilityScannerTool>(config, logger);
    } else if (config.tool_name.find("compliance") != std::string::npos ||
               config.tool_name.find("Compliance") != std::string::npos) {
        return std::make_unique<ComplianceCheckerTool>(config, logger);
    } else if (config.tool_name.find("access") != std::string::npos ||
               config.tool_name.find("Access") != std::string::npos) {
        return std::make_unique<AccessAnalyzerTool>(config, logger);
    } else if (config.tool_name.find("audit") != std::string::npos ||
               config.tool_name.find("Audit") != std::string::npos) {
        return std::make_unique<AuditLoggerTool>(config, logger);
    }

    return nullptr; // Unknown security tool
}

std::unique_ptr<Tool> ToolFactory::create_monitoring_tool(const ToolConfig& config,
                                                        StructuredLogger* logger) {
    // Route to specific monitoring tool based on tool name
    if (config.tool_name.find("monitor") != std::string::npos ||
        config.tool_name.find("Monitor") != std::string::npos) {
        return std::make_unique<SystemMonitorTool>(config, logger);
    } else if (config.tool_name.find("tracker") != std::string::npos ||
               config.tool_name.find("Tracker") != std::string::npos) {
        return std::make_unique<PerformanceTrackerTool>(config, logger);
    } else if (config.tool_name.find("alert") != std::string::npos ||
               config.tool_name.find("Alert") != std::string::npos) {
        return std::make_unique<AlertManagerTool>(config, logger);
    } else if (config.tool_name.find("health") != std::string::npos ||
               config.tool_name.find("Health") != std::string::npos) {
        return std::make_unique<HealthCheckerTool>(config, logger);
    }

    return nullptr; // Unknown monitoring tool
}

// Forward declarations for tool creation functions
// create_email_tool is declared in email_tool.hpp and implemented in email_tool.cpp
// create_web_search_tool is declared in web_search_tool.hpp and implemented in web_search_tool.cpp
// create_mcp_tool is declared in mcp_tool.hpp and implemented in mcp_tool.cpp

} // namespace regulens
