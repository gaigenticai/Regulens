/**
 * Tool Integration Layer - Enterprise Tool Integration Framework
 *
 * Standardized interface for connecting agents to external business systems,
 * enabling seamless interaction with ERP, CRM, email, document management,
 * and other enterprise tools.
 *
 * This layer provides:
 * - Standardized tool interfaces and protocols
 * - Authentication and security management
 * - Error handling and retry mechanisms
 * - Monitoring and performance metrics
 * - Configuration management
 * - Rate limiting and throttling
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <atomic>
#include <functional>
#include <nlohmann/json.hpp>
#include "../logging/structured_logger.hpp"
#include "../database/postgresql_connection.hpp"

namespace regulens {

// Tool Categories for organization and discovery
enum class ToolCategory {
    COMMUNICATION,      // Email, chat, messaging
    ERP,               // Enterprise Resource Planning
    CRM,               // Customer Relationship Management
    DMS,               // Document Management System
    STORAGE,           // File storage and cloud storage
    ANALYTICS,         // Business intelligence and reporting
    WORKFLOW,          // Process automation and ticketing
    INTEGRATION,       // API gateways and middleware
    SECURITY,          // Authentication and authorization
    MONITORING,        // System monitoring and alerting
    WEB_SEARCH,        // Web search and information retrieval
    MCP_TOOLS         // Model Context Protocol tools
};

// Tool Capabilities - what operations the tool supports
enum class ToolCapability {
    READ,              // Read/query data
    WRITE,             // Create/modify data
    DELETE,            // Remove data
    EXECUTE,           // Run operations/commands
    SUBSCRIBE,         // Real-time event subscription
    NOTIFY,            // Send notifications
    SEARCH,            // Search/indexing capabilities
    BATCH_PROCESS,     // Bulk operations
    TRANSACTIONAL,     // ACID transaction support
    WEB_SEARCH,        // Web search and information retrieval
    MCP_PROTOCOL       // Model Context Protocol support
};

// Tool Health Status
enum class ToolHealthStatus {
    HEALTHY,
    DEGRADED,
    UNHEALTHY,
    MAINTENANCE,
    OFFLINE
};

// Tool Authentication Types
enum class AuthType {
    NONE,              // No authentication
    BASIC,             // Username/password
    OAUTH2,            // OAuth 2.0 flow
    API_KEY,           // API key authentication
    JWT,               // JSON Web Tokens
    CERTIFICATE,       // Client certificates
    KERBEROS,          // Kerberos authentication
    SAML               // SAML assertions
};

// Tool Operation Result
struct ToolResult {
    bool success;
    nlohmann::json data;
    std::string error_message;
    std::chrono::milliseconds execution_time;
    int retry_count;
    std::unordered_map<std::string, std::string> metadata;

    ToolResult(bool s = false, nlohmann::json d = {}, std::string err = "",
               std::chrono::milliseconds et = std::chrono::milliseconds(0), int rc = 0)
        : success(s), data(std::move(d)), error_message(std::move(err)),
          execution_time(et), retry_count(rc) {}
};

// Tool Configuration
struct ToolConfig {
    std::string tool_id;
    std::string tool_name;
    std::string description;
    ToolCategory category;
    std::vector<ToolCapability> capabilities;
    AuthType auth_type;
    nlohmann::json auth_config;
    nlohmann::json connection_config;
    std::chrono::seconds timeout;
    int max_retries;
    std::chrono::milliseconds retry_delay;
    int rate_limit_per_minute;
    bool enabled;
    nlohmann::json metadata;

    ToolConfig() : timeout(30), max_retries(3), retry_delay(1000),
                   rate_limit_per_minute(60), enabled(true) {}
};

// Tool Metrics for monitoring
struct ToolMetrics {
    std::string tool_id;
    std::atomic<size_t> operations_total;
    std::atomic<size_t> operations_successful;
    std::atomic<size_t> operations_failed;
    std::atomic<size_t> operations_retried;
    std::atomic<size_t> rate_limit_hits;
    std::atomic<size_t> timeouts;
    std::chrono::steady_clock::time_point last_operation;
    std::chrono::milliseconds avg_response_time;
    ToolHealthStatus health_status;

    ToolMetrics(const std::string& id) : tool_id(id), operations_total(0),
        operations_successful(0), operations_failed(0), operations_retried(0),
        rate_limit_hits(0), timeouts(0), health_status(ToolHealthStatus::HEALTHY) {}
};

// Base Tool Interface - All tools must implement this
class Tool {
public:
    Tool(const ToolConfig& config, StructuredLogger* logger);
    virtual ~Tool() = default;

    // Tool identification and metadata
    const std::string& get_tool_id() const { return config_.tool_id; }
    const std::string& get_tool_name() const { return config_.tool_name; }
    ToolCategory get_category() const { return config_.category; }
    bool is_enabled() const { return config_.enabled; }
    void set_enabled(bool enabled) { config_.enabled = enabled; }

    // Capabilities and status
    virtual bool supports_capability(ToolCapability capability) const;
    virtual ToolHealthStatus get_health_status() const;
    virtual nlohmann::json get_health_details() const;

    // Core operations - to be implemented by concrete tools
    virtual ToolResult execute_operation(const std::string& operation,
                                       const nlohmann::json& parameters) = 0;

    // Authentication and connection
    virtual bool authenticate() = 0;
    virtual bool is_authenticated() const = 0;
    virtual bool disconnect() = 0;

    // Configuration and validation
    virtual bool validate_config() const;
    virtual nlohmann::json get_tool_info() const;

    // Metrics and monitoring
    const ToolMetrics& get_metrics() const { return metrics_; }
    void reset_metrics();

private:
    void update_health_status();

protected:
    ToolConfig config_;
    StructuredLogger* logger_;
    ToolMetrics metrics_;
    bool authenticated_;

    // Helper methods for implementations
    void record_operation_result(const ToolResult& result);
    bool check_rate_limit();
    ToolResult create_error_result(const std::string& message,
                                 std::chrono::milliseconds execution_time = std::chrono::milliseconds(0));
    ToolResult create_success_result(const nlohmann::json& data,
                                   std::chrono::milliseconds execution_time = std::chrono::milliseconds(0));
};

// Tool Registry - Manages available tools
class ToolRegistry {
public:
    ToolRegistry(std::shared_ptr<ConnectionPool> db_pool, StructuredLogger* logger);

    // Lifecycle management
    bool initialize();

    // Tool registration and management
    bool register_tool(std::unique_ptr<Tool> tool);
    bool unregister_tool(const std::string& tool_id);
    Tool* get_tool(const std::string& tool_id);
    std::vector<std::string> get_available_tools() const;
    std::vector<std::string> get_tools_by_category(ToolCategory category) const;

    // Tool discovery and information
    nlohmann::json get_tool_catalog() const;
    nlohmann::json get_tool_details(const std::string& tool_id) const;

    // Bulk operations
    bool enable_tool(const std::string& tool_id);
    bool disable_tool(const std::string& tool_id);
    void enable_all_tools();
    void disable_all_tools();

    // Health and monitoring
    nlohmann::json get_system_health() const;
    std::vector<std::string> get_unhealthy_tools() const;

    // Configuration management
    bool update_tool_config(const std::string& tool_id, const ToolConfig& new_config);
    bool reload_tool_configs();

private:
    std::shared_ptr<ConnectionPool> db_pool_;
    StructuredLogger* logger_;
    std::unordered_map<std::string, std::unique_ptr<Tool>> tools_;
    mutable std::mutex registry_mutex_;

    bool persist_tool_config(const ToolConfig& config);
    bool load_tool_configs();
};

// Tool Factory - Creates tool instances from configuration
class ToolFactory {
public:
    static std::unique_ptr<Tool> create_tool(const ToolConfig& config,
                                           StructuredLogger* logger);

    // Pre-built tool types
    static std::unique_ptr<Tool> create_email_tool(const ToolConfig& config,
                                                  StructuredLogger* logger);
    static std::unique_ptr<Tool> create_http_api_tool(const ToolConfig& config,
                                                     StructuredLogger* logger);
    static std::unique_ptr<Tool> create_database_tool(const ToolConfig& config,
                                                     StructuredLogger* logger);
    static std::unique_ptr<Tool> create_file_system_tool(const ToolConfig& config,
                                                        StructuredLogger* logger);
    static std::unique_ptr<Tool> create_slack_tool(const ToolConfig& config,
                                                  StructuredLogger* logger);

    // Tool category factories
    static std::unique_ptr<Tool> create_analytics_tool(const ToolConfig& config,
                                                      StructuredLogger* logger);
    static std::unique_ptr<Tool> create_workflow_tool(const ToolConfig& config,
                                                     StructuredLogger* logger);
    static std::unique_ptr<Tool> create_security_tool(const ToolConfig& config,
                                                     StructuredLogger* logger);
    static std::unique_ptr<Tool> create_monitoring_tool(const ToolConfig& config,
                                                       StructuredLogger* logger);

private:
    ToolFactory() = delete; // Static factory only
};

// Utility functions
std::string tool_category_to_string(ToolCategory category);
std::string tool_capability_to_string(ToolCapability capability);
std::string tool_health_status_to_string(ToolHealthStatus status);
std::string auth_type_to_string(AuthType auth_type);

ToolCategory string_to_tool_category(const std::string& str);
ToolCapability string_to_tool_capability(const std::string& str);
ToolHealthStatus string_to_tool_health_status(const std::string& str);
AuthType string_to_auth_type(const std::string& str);

} // namespace regulens
