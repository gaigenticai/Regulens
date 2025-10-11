/**
 * MCP Tool - Model Context Protocol Integration
 *
 * Production-grade implementation of the Model Context Protocol using Boost.Beast
 * for WebSocket communication with MCP-compatible tools and services.
 *
 * Features:
 * - MCP protocol compliance
 * - Tool discovery and registration
 * - Resource access and management
 * - Sampling and prompt handling
 * - Real-time tool communication via WebSocket
 * - Connection management with automatic reconnection
 * - Heartbeat mechanism for connection health
 * - Timeout handling and error recovery
 */

#pragma once

#include "../tool_interface.hpp"
#include "../../config/config_types.hpp"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <thread>
#include <queue>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

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
    std::atomic<bool> server_connected_;
    std::atomic<bool> ws_connected_;
    std::atomic<bool> stop_requested_;

    // Production-grade WebSocket communication using Boost.Beast
    std::shared_ptr<net::io_context> io_context_;
    std::unique_ptr<websocket::stream<beast::tcp_stream>> ws_stream_;
    std::unique_ptr<net::steady_timer> heartbeat_timer_;
    std::unique_ptr<net::steady_timer> reconnect_timer_;
    std::thread io_thread_;
    tcp::resolver::results_type endpoints_;
    
    std::mutex ws_mutex_;
    std::condition_variable response_cv_;
    std::unordered_map<std::string, nlohmann::json> pending_responses_;
    std::queue<std::string> outgoing_messages_;
    
    // Connection management
    std::atomic<int> reconnect_attempts_;
    std::chrono::steady_clock::time_point last_heartbeat_;
    static constexpr int MAX_RECONNECT_ATTEMPTS = 5;
    static constexpr std::chrono::seconds HEARTBEAT_INTERVAL{30};
    static constexpr std::chrono::seconds RECONNECT_DELAY{5};

    // MCP protocol methods
    bool initialize_mcp_connection();
    bool discover_mcp_tools();
    bool discover_mcp_resources();

    // WebSocket connection management
    bool initialize_websocket_connection();
    void websocket_connect();
    void websocket_read_loop();
    void websocket_write_message(const std::string& message);
    void handle_websocket_message(const std::string& message);
    void handle_mcp_notification(const nlohmann::json& notification);
    void start_heartbeat();
    void send_heartbeat();
    void attempt_reconnect();
    void cleanup_websocket();

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
    std::string parse_websocket_url(const std::string& url, std::string& host, std::string& port, std::string& path);
};

// MCP Tool Factory Function
std::unique_ptr<Tool> create_mcp_tool(const ToolConfig& config, StructuredLogger* logger);

// Environment Variable Configuration moved to config_types.hpp

// Global configuration loader
AgentCapabilityConfig load_agent_capability_config();

} // namespace regulens
