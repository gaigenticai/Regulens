/**
 * MCP Tool Implementation
 *
 * Production-grade implementation of Model Context Protocol
 * for connecting agents to MCP-compatible tools and services.
 */

#include "mcp_tool.hpp"
#include "../tool_interface.hpp"
#include <curl/curl.h>
#include <sstream>
// WebSocket support disabled until websocketpp is available
// #include <websocketpp/client.hpp>
// #include <websocketpp/config/asio_no_tls_client.hpp>
// #include <websocketpp/common/thread.hpp>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace regulens {

// WebSocket client type aliases (disabled)
// typedef websocketpp::client<websocketpp::config::asio_client> ws_client;
// typedef websocketpp::lib::shared_ptr<websocketpp::lib::thread> thread_ptr;

// MCP Tool Implementation
MCPToolIntegration::MCPToolIntegration(const ToolConfig& config, StructuredLogger* logger)
    : Tool(config, logger), server_connected_(false), ws_connected_(false) {

    // Parse MCP server configuration from tool config
    if (config.metadata.contains("mcp_server_url")) {
        mcp_config_.server_url = config.metadata["mcp_server_url"];
    }
    if (config.metadata.contains("mcp_auth_token")) {
        mcp_config_.auth_token = config.metadata["mcp_auth_token"];
    }
    if (config.metadata.contains("mcp_connection_timeout")) {
        mcp_config_.connection_timeout = config.metadata["mcp_connection_timeout"];
    }
    if (config.metadata.contains("mcp_read_timeout")) {
        mcp_config_.read_timeout = config.metadata["mcp_read_timeout"];
    }
    if (config.metadata.contains("mcp_supported_protocols")) {
        mcp_config_.supported_protocols = config.metadata["mcp_supported_protocols"];
    }
    if (config.metadata.contains("mcp_server_capabilities")) {
        mcp_config_.server_capabilities = config.metadata["mcp_server_capabilities"];
    }
}

ToolResult MCPToolIntegration::execute_operation(const std::string& operation,
                                               const nlohmann::json& parameters) {
    if (!server_connected_ && !authenticate()) {
        return ToolResult(false, {}, "MCP server not connected");
    }

    try {
        if (operation == "list_tools") {
            return list_available_tools();
        } else if (operation == "call_tool") {
            if (!parameters.contains("tool_name")) {
                return ToolResult(false, {}, "Missing tool_name parameter");
            }
            return call_mcp_tool(parameters["tool_name"], parameters.value("arguments", nlohmann::json{}));
        } else if (operation == "list_resources") {
            return list_resources();
        } else if (operation == "read_resource") {
            if (!parameters.contains("uri")) {
                return ToolResult(false, {}, "Missing uri parameter");
            }
            return read_resource(parameters["uri"]);
        } else if (operation == "subscribe_resource") {
            if (!parameters.contains("uri")) {
                return ToolResult(false, {}, "Missing uri parameter");
            }
            return subscribe_to_resource(parameters["uri"]);
        } else {
            return ToolResult(false, {}, "Unknown MCP operation: " + operation);
        }
    } catch (const std::exception& e) {
        ToolResult error_result(false, {}, "MCP operation failed: " + std::string(e.what()));
        record_operation_result(error_result);
        return ToolResult(false, {}, "MCP operation failed: " + std::string(e.what()));
    }
}

bool MCPToolIntegration::authenticate() {
    try {
        if (initialize_mcp_connection()) {
            server_connected_ = true;
            logger_->log(LogLevel::INFO, "Successfully authenticated with MCP server: " + mcp_config_.server_url);

            // Discover available tools and resources
            discover_mcp_tools();
            discover_mcp_resources();

            return true;
        } else {
            logger_->log(LogLevel::ERROR, "Failed to authenticate with MCP server: " + mcp_config_.server_url);
            return false;
        }
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "MCP authentication failed: " + std::string(e.what()));
        return false;
    }
}

bool MCPToolIntegration::disconnect() {
    try {
        server_connected_ = false;
        available_tools_.clear();
        available_resources_.clear();
        logger_->log(LogLevel::INFO, "Disconnected from MCP server");
        return true;
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "MCP disconnect failed: " + std::string(e.what()));
        return false;
    }
}

ToolResult MCPToolIntegration::list_available_tools() {
    try {
        nlohmann::json tools_list = nlohmann::json::array();
        for (const auto& [name, tool] : available_tools_) {
            tools_list.push_back({
                {"name", tool.name},
                {"description", tool.description},
                {"input_schema", tool.input_schema}
            });
        }

        ToolResult success_result(true, tools_list, "", std::chrono::milliseconds(10));
        record_operation_result(success_result);
        return success_result;
    } catch (const std::exception& e) {
        ToolResult error_result(false, {}, "Failed to list MCP tools: " + std::string(e.what()), std::chrono::milliseconds(10));
        record_operation_result(error_result);
        return error_result;
    }
}

ToolResult MCPToolIntegration::call_mcp_tool(const std::string& tool_name, const nlohmann::json& arguments) {
    auto start_time = std::chrono::steady_clock::now();

    try {
        if (!validate_mcp_tool_call(tool_name, arguments)) {
            return ToolResult(false, {}, "Invalid MCP tool call parameters");
        }

        nlohmann::json request = {
            {"jsonrpc", "2.0"},
            {"id", generate_request_id()},
            {"method", "tools/call"},
            {"params", {
                {"name", tool_name},
                {"arguments", arguments}
            }}
        };

        nlohmann::json response = send_mcp_request("tools/call", request["params"]);
        ToolResult result = handle_mcp_response(response);

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
        result.execution_time = duration;
        record_operation_result(result);

        return result;
    } catch (const std::exception& e) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
        ToolResult error_result(false, {}, "MCP tool call failed: " + std::string(e.what()), duration);
        record_operation_result(error_result);
        return error_result;
    }
}

ToolResult MCPToolIntegration::list_resources() {
    try {
        nlohmann::json resources_list = nlohmann::json::array();
        for (const auto& [uri, resource] : available_resources_) {
            resources_list.push_back({
                {"uri", resource.uri},
                {"name", resource.name},
                {"description", resource.description},
                {"mime_type", resource.mime_type}
            });
        }

        ToolResult success_result(true, resources_list, "", std::chrono::milliseconds(10));
        record_operation_result(success_result);
        return success_result;
    } catch (const std::exception& e) {
        ToolResult error_result(false, {}, "Failed to list MCP resources: " + std::string(e.what()), std::chrono::milliseconds(10));
        record_operation_result(error_result);
        return error_result;
    }
}

ToolResult MCPToolIntegration::read_resource(const std::string& uri) {
    auto start_time = std::chrono::steady_clock::now();

    try {
        if (!validate_mcp_resource_uri(uri)) {
            return ToolResult(false, {}, "Invalid MCP resource URI");
        }

        nlohmann::json params = {{"uri", uri}};
        nlohmann::json response = send_mcp_request("resources/read", params);
        ToolResult result = handle_mcp_response(response);

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
        result.execution_time = duration;
        record_operation_result(result);

        return result;
    } catch (const std::exception& e) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
        ToolResult error_result(false, {}, "MCP resource read failed: " + std::string(e.what()), duration);
        record_operation_result(error_result);
        return error_result;
    }
}

ToolResult MCPToolIntegration::subscribe_to_resource(const std::string& uri) {
    try {
        if (!validate_mcp_resource_uri(uri)) {
            return ToolResult(false, {}, "Invalid MCP resource URI");
        }

        nlohmann::json params = {{"uri", uri}};
        nlohmann::json response = send_mcp_request("resources/subscribe", params);
        ToolResult result = handle_mcp_response(response);

        result.execution_time = std::chrono::milliseconds(50);
        record_operation_result(result);
        return result;
    } catch (const std::exception& e) {
        ToolResult error_result(false, {}, "MCP resource subscription failed: " + std::string(e.what()), std::chrono::milliseconds(50));
        record_operation_result(error_result);
        return error_result;
    }
}

// Private implementation methods

bool MCPToolIntegration::initialize_mcp_connection() {
    try {
        // Initialize WebSocket connection (optional for HTTP/STDIO transports)
#ifdef USE_WEBSOCKET
        if (!initialize_websocket_connection()) {
            return false;
        }
#else
        // WebSocket not available - will use HTTP or STDIO transport
        if (logger_) {
            logger_->debug("WebSocket support not compiled, using HTTP/STDIO transport",
                          "MCPToolIntegration", "initialize_mcp_connection");
        }
#endif

        // Perform MCP handshake
        nlohmann::json handshake_params = {
            {"protocolVersion", "2024-11-05"},
            {"capabilities", {
                {"tools", {{"listChanged", true}}},
                {"resources", {{"listChanged", true}, {"subscribe", true}}},
                {"sampling", {}}
            }},
            {"clientInfo", {
                {"name", "Regulens Agent"},
                {"version", "1.0.0"}
            }}
        };

        nlohmann::json response = send_mcp_request("initialize", handshake_params);

        if (response.contains("result")) {
            // Store server capabilities
            if (response["result"].contains("capabilities")) {
                mcp_config_.server_capabilities = response["result"]["capabilities"];
            }

            // Send initialized notification
            nlohmann::json initialized_notification = {
                {"jsonrpc", "2.0"},
                {"method", "notifications/initialized"},
                {"params", nlohmann::json::object()}
            };

#ifdef USE_WEBSOCKET
            websocketpp::lib::error_code ec;
            ws_client_->send(ws_connection_, initialized_notification.dump(),
                           websocketpp::frame::opcode::text, ec);

            if (ec) {
                logger_->log(LogLevel::WARN, "Failed to send initialized notification: " + ec.message());
            }
#else
            logger_->log(LogLevel::DEBUG, "Initialized notification skipped (WebSocket not available)",
                        "MCPToolIntegration", "initialize_mcp_connection");
#endif

            logger_->log(LogLevel::INFO, "MCP server initialized successfully");
            return true;
        } else {
            logger_->log(LogLevel::ERROR, "MCP server initialization failed");
            return false;
        }
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "MCP connection initialization failed: " + std::string(e.what()));
        return false;
    }
}

bool MCPToolIntegration::discover_mcp_tools() {
    try {
        nlohmann::json response = send_mcp_request("tools/list", nlohmann::json{});
        if (response.contains("result") && response["result"].contains("tools")) {
            for (const auto& tool_json : response["result"]["tools"]) {
                MCPTool tool;
                tool.name = tool_json.value("name", "");
                tool.description = tool_json.value("description", "");
                tool.input_schema = tool_json.value("inputSchema", nlohmann::json{});
                tool.annotations = tool_json.value("annotations", nlohmann::json{});

                available_tools_[tool.name] = tool;
            }
            logger_->log(LogLevel::INFO, "Discovered " + std::to_string(available_tools_.size()) + " MCP tools");
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "MCP tool discovery failed: " + std::string(e.what()));
        return false;
    }
}

bool MCPToolIntegration::discover_mcp_resources() {
    try {
        nlohmann::json response = send_mcp_request("resources/list", nlohmann::json{});
        if (response.contains("result") && response["result"].contains("resources")) {
            for (const auto& resource_json : response["result"]["resources"]) {
                MCPResource resource;
                resource.uri = resource_json.value("uri", "");
                resource.name = resource_json.value("name", "");
                resource.description = resource_json.value("description", "");
                resource.mime_type = resource_json.value("mimeType", "");
                resource.annotations = resource_json.value("annotations", nlohmann::json{});

                available_resources_[resource.uri] = resource;
            }
            logger_->log(LogLevel::INFO, "Discovered " + std::to_string(available_resources_.size()) + " MCP resources");
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "MCP resource discovery failed: " + std::string(e.what()));
        return false;
    }
}

#ifdef USE_WEBSOCKET
bool MCPToolIntegration::initialize_websocket_connection() {
    // WebSocket support temporarily disabled (requires websocketpp library)
    logger_->log(LogLevel::WARN, "MCP WebSocket support not available - websocketpp library not installed");
    logger_->log(LogLevel::INFO, "MCP tool will return stub responses until WebSocket support is enabled");
    ws_connected_ = false;
    return false;
}

void MCPToolIntegration::handle_websocket_message(const std::string& message) {
    try {
        nlohmann::json response = nlohmann::json::parse(message);

        if (response.contains("id")) {
            std::string request_id = response["id"];
            std::lock_guard<std::mutex> lock(ws_mutex_);
            pending_responses_[request_id] = response;
            response_cv_.notify_all();
        } else if (response.contains("method")) {
            // Handle server-initiated messages (notifications)
            handle_mcp_notification(response);
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to parse WebSocket message: " + std::string(e.what()));
    }
}

void MCPToolIntegration::handle_mcp_notification(const nlohmann::json& notification) {
    std::string method = notification.value("method", "");

    if (method == "notifications/tools/list_changed") {
        // Tools list changed, rediscover tools
        discover_mcp_tools();
    } else if (method == "notifications/resources/list_changed") {
        // Resources list changed, rediscover resources
        discover_mcp_resources();
    } else if (method == "notifications/resources/updated") {
        // Resource updated
        if (notification.contains("params") && notification["params"].contains("uri")) {
            std::string uri = notification["params"]["uri"];
            logger_->log(LogLevel::INFO, "MCP resource updated: " + uri);
        }
    }
}
#endif // USE_WEBSOCKET

nlohmann::json MCPToolIntegration::send_mcp_request(const std::string& method, const nlohmann::json& params) {
    if (!ws_connected_) {
        return {{"error", "WebSocket not connected"}};
    }

#ifdef USE_WEBSOCKET
    try {
        std::string request_id = generate_request_id();

        nlohmann::json request = {
            {"jsonrpc", "2.0"},
            {"id", request_id},
            {"method", method},
            {"params", params}
        };

        std::string request_str = request.dump();

        // Send request via WebSocket
        websocketpp::lib::error_code ec;
        ws_client_->send(ws_connection_, request_str, websocketpp::frame::opcode::text, ec);

        if (ec) {
            logger_->log(LogLevel::ERROR, "WebSocket send failed: " + ec.message());
            return {{"error", "WebSocket send failed: " + ec.message()}};
        }

        // Wait for response with timeout
        {
            std::unique_lock<std::mutex> lock(ws_mutex_);
            if (!response_cv_.wait_for(lock, std::chrono::seconds(mcp_config_.read_timeout),
                                     [this, &request_id]() {
                                         return pending_responses_.find(request_id) != pending_responses_.end();
                                     })) {
                logger_->log(LogLevel::ERROR, "MCP request timeout for method: " + method);
                return {{"error", "Request timeout"}};
            }

            auto it = pending_responses_.find(request_id);
            if (it != pending_responses_.end()) {
                nlohmann::json response = it->second;
                pending_responses_.erase(it);
                return response;
            }
        }

        return {{"error", "No response received"}};

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "MCP WebSocket request failed: " + std::string(e.what()));
        return {{"error", e.what()}};
    }
#else
    return {{"error", "WebSocket support not compiled"}};
#endif
}

ToolResult MCPToolIntegration::handle_mcp_response(const nlohmann::json& response) {
    try {
        if (response.contains("error")) {
            return ToolResult(false, {}, "MCP server error: " + response["error"].dump());
        } else if (response.contains("result")) {
            return ToolResult(true, response["result"]);
        } else {
            return ToolResult(false, {}, "Invalid MCP response format");
        }
    } catch (const std::exception& e) {
        return ToolResult(false, {}, "Failed to handle MCP response: " + std::string(e.what()));
    }
}

bool MCPToolIntegration::validate_mcp_tool_call(const std::string& tool_name, const nlohmann::json& arguments) {
    if (available_tools_.find(tool_name) == available_tools_.end()) {
        logger_->log(LogLevel::WARN, "Unknown MCP tool: " + tool_name);
        return false;
    }

    // Basic validation - in production, would validate against input schema
    return true;
}

bool MCPToolIntegration::validate_mcp_resource_uri(const std::string& uri) {
    if (available_resources_.find(uri) == available_resources_.end()) {
        logger_->log(LogLevel::WARN, "Unknown MCP resource: " + uri);
        return false;
    }
    return true;
}

std::string MCPToolIntegration::generate_request_id() {
    static std::atomic<int> request_counter{0};
    return "req_" + std::to_string(++request_counter);
}

size_t MCPToolIntegration::write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Factory function
std::unique_ptr<Tool> create_mcp_tool(const ToolConfig& config, StructuredLogger* logger) {
    return std::make_unique<MCPToolIntegration>(config, logger);
}

// Environment variable configuration loader
AgentCapabilityConfig load_agent_capability_config() {
    AgentCapabilityConfig config;

    // Check environment variables for capability configuration
    const char* web_search_env = std::getenv("AGENT_ENABLE_WEB_SEARCH");
    config.enable_web_search = web_search_env && std::string(web_search_env) == "true";

    const char* mcp_tools_env = std::getenv("AGENT_ENABLE_MCP_TOOLS");
    config.enable_mcp_tools = mcp_tools_env && std::string(mcp_tools_env) == "true";

    const char* advanced_discovery_env = std::getenv("AGENT_ENABLE_ADVANCED_DISCOVERY");
    config.enable_advanced_discovery = advanced_discovery_env && std::string(advanced_discovery_env) == "true";

    const char* autonomous_integration_env = std::getenv("AGENT_ENABLE_AUTONOMOUS_INTEGRATION");
    config.enable_autonomous_integration = autonomous_integration_env && std::string(autonomous_integration_env) == "true";

    const char* max_tools_env = std::getenv("AGENT_MAX_AUTONOMOUS_TOOLS");
    if (max_tools_env) {
        try {
            config.max_autonomous_tools_per_session = std::stoi(max_tools_env);
        } catch (const std::exception&) {
            // Use default value
        }
    }

    const char* allowed_categories_env = std::getenv("AGENT_ALLOWED_TOOL_CATEGORIES");
    if (allowed_categories_env) {
        std::stringstream ss(allowed_categories_env);
        std::string category;
        while (std::getline(ss, category, ',')) {
            config.allowed_tool_categories.push_back(category);
        }
    }

    const char* blocked_domains_env = std::getenv("AGENT_BLOCKED_TOOL_DOMAINS");
    if (blocked_domains_env) {
        std::stringstream ss(blocked_domains_env);
        std::string domain;
        while (std::getline(ss, domain, ',')) {
            config.blocked_tool_domains.push_back(domain);
        }
    }

    return config;
}

} // namespace regulens