/**
 * MCP Tool - Model Context Protocol Integration
 *
 * Implementation of the Model Context Protocol for connecting
 * agents to MCP-compatible tools and services.
 *
 * Features:
 * - MCP protocol compliance
 * - Tool discovery and registration
 * - Resource access and management
 * - Sampling and prompt handling
 * - Real-time tool communication
 */

#pragma once

#include "../tool_interface.hpp"
#include "../../config/config_types.hpp"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace regulens {

// MCP Protocol Structures
struct MCPTool {
    std::string name;
    std::string description;
    nlohmann::json input_schema;
    nlohmann::json annotations;

    MCPTool() = default;
    MCPTool(const std::string& n, const std::string& desc)
        : name(n), description(desc) {}
};

struct MCPResource {
    std::string uri;
    std::string name;
    std::string description;
    std::string mime_type;
    nlohmann::json annotations;

    MCPResource() = default;
    MCPResource(const std::string& u, const std::string& n, const std::string& desc)
        : uri(u), name(n), description(desc) {}
};

struct MCPResourceTemplate {
    std::string uri_template;
    std::string name;
    std::string description;
    nlohmann::json annotations;

    MCPResourceTemplate() = default;
    MCPResourceTemplate(const std::string& uri, const std::string& n, const std::string& desc)
        : uri_template(uri), name(n), description(desc) {}
};

// MCP Server Configuration
struct MCPServerConfig {
    std::string server_url;
    std::string auth_token;
    int connection_timeout;
    int read_timeout;
    std::vector<std::string> supported_protocols;
    nlohmann::json server_capabilities;

    MCPServerConfig() : connection_timeout(30), read_timeout(60) {}
};

// MCP Tool Implementation
class MCPToolIntegration : public Tool {
public:
    MCPToolIntegration(const ToolConfig& config, StructuredLogger* logger);

    // Tool interface implementation
    ToolResult execute_operation(const std::string& operation,
                               const nlohmann::json& parameters) override;

    bool authenticate() override;
    bool is_authenticated() const override { return server_connected_; }
    bool disconnect() override;

    // MCP-specific operations
    ToolResult list_available_tools();
    ToolResult call_mcp_tool(const std::string& tool_name, const nlohmann::json& arguments);
    ToolResult list_resources();
    ToolResult read_resource(const std::string& uri);
    ToolResult subscribe_to_resource(const std::string& uri);

private:
    MCPServerConfig mcp_config_;
    std::unordered_map<std::string, MCPTool> available_tools_;
    std::unordered_map<std::string, MCPResource> available_resources_;
    bool server_connected_;

    // MCP protocol methods
    bool initialize_mcp_connection();
    bool discover_mcp_tools();
    bool discover_mcp_resources();

    // Communication methods
    nlohmann::json send_mcp_request(const std::string& method, const nlohmann::json& params);
    ToolResult handle_mcp_response(const nlohmann::json& response);

    // Error handling
    ToolResult create_mcp_error(const std::string& message, const nlohmann::json& details = {});

    // Protocol validation
    bool validate_mcp_tool_call(const std::string& tool_name, const nlohmann::json& arguments);
    bool validate_mcp_resource_uri(const std::string& uri);

    // Helper methods
    std::string generate_request_id();
    static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp);
};

// MCP Tool Factory Function
std::unique_ptr<Tool> create_mcp_tool(const ToolConfig& config, StructuredLogger* logger);

// Environment Variable Configuration moved to config_types.hpp

// Global configuration loader
AgentCapabilityConfig load_agent_capability_config();

} // namespace regulens
