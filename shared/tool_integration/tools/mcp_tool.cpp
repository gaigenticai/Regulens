/**
 * MCP Tool Implementation
 *
 * Production-grade implementation of Model Context Protocol using Boost.Beast
 * for real-time agent tool discovery and communication.
 */

#include "mcp_tool.hpp"
#include "../tool_interface.hpp"
#include <sstream>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <regex>

namespace regulens {

// MCP Tool Implementation with Production-Grade WebSocket Support
MCPToolIntegration::MCPToolIntegration(const ToolConfig& config, StructuredLogger* logger)
    : Tool(config, logger), server_connected_(false), ws_connected_(false), 
      stop_requested_(false), reconnect_attempts_(0) {

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
    
    // Initialize io_context for async operations
    io_context_ = std::make_shared<net::io_context>();
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
        stop_requested_ = true;
        cleanup_websocket();
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
        // Initialize production-grade WebSocket connection using Boost.Beast
        if (!initialize_websocket_connection()) {
            logger_->log(LogLevel::ERROR, "Failed to initialize WebSocket connection");
            return false;
        }

        // Wait for connection to establish
        auto timeout = std::chrono::seconds(mcp_config_.connection_timeout);
        auto start = std::chrono::steady_clock::now();
        
        while (!ws_connected_ && !stop_requested_) {
            if (std::chrono::steady_clock::now() - start > timeout) {
                logger_->log(LogLevel::ERROR, "WebSocket connection timeout");
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (!ws_connected_) {
            return false;
        }

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

            websocket_write_message(initialized_notification.dump());
            
            // Start heartbeat mechanism
            start_heartbeat();

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

// Production-grade WebSocket implementation using Boost.Beast
bool MCPToolIntegration::initialize_websocket_connection() {
    try {
        // Parse WebSocket URL
        std::string host, port, path;
        parse_websocket_url(mcp_config_.server_url, host, port, path);

        if (host.empty() || port.empty()) {
            logger_->log(LogLevel::ERROR, "Invalid WebSocket URL: " + mcp_config_.server_url);
            return false;
        }

        // Resolve the endpoint
        tcp::resolver resolver(*io_context_);
        endpoints_ = resolver.resolve(host, port);

        // Create WebSocket stream
        ws_stream_ = std::make_unique<websocket::stream<beast::tcp_stream>>(*io_context_);

        // Start async connection in separate thread
        io_thread_ = std::thread([this]() {
            websocket_connect();
            io_context_->run();
        });

        logger_->log(LogLevel::INFO, "WebSocket connection initiated to " + host + ":" + port + path);
        return true;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to initialize WebSocket: " + std::string(e.what()));
        return false;
    }
}

void MCPToolIntegration::websocket_connect() {
    try {
        beast::get_lowest_layer(*ws_stream_).expires_after(std::chrono::seconds(mcp_config_.connection_timeout));

        // Connect to the resolved endpoint
        beast::get_lowest_layer(*ws_stream_).connect(endpoints_);

        // Set WebSocket options
        ws_stream_->set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
        ws_stream_->set_option(websocket::stream_base::decorator(
            [this](websocket::request_type& req) {
                req.set(beast::http::field::user_agent, "Regulens-Agent/1.0");
                if (!mcp_config_.auth_token.empty()) {
                    req.set(beast::http::field::authorization, "Bearer " + mcp_config_.auth_token);
                }
            }));

        // Perform WebSocket handshake
        std::string host, port, path;
        parse_websocket_url(mcp_config_.server_url, host, port, path);
        ws_stream_->handshake(host, path);

        ws_connected_ = true;
        reconnect_attempts_ = 0;
        last_heartbeat_ = std::chrono::steady_clock::now();

        logger_->log(LogLevel::INFO, "WebSocket connected successfully");

        // Start read loop
        websocket_read_loop();

    } catch (const beast::system_error& e) {
        ws_connected_ = false;
        logger_->log(LogLevel::ERROR, "WebSocket connection failed: " + std::string(e.what()));
        
        // Attempt reconnection if not stopped
        if (!stop_requested_ && reconnect_attempts_ < MAX_RECONNECT_ATTEMPTS) {
            attempt_reconnect();
        }
    }
}

void MCPToolIntegration::websocket_read_loop() {
    try {
        while (!stop_requested_ && ws_connected_) {
            beast::flat_buffer buffer;
            
            // Set timeout for read operation
            beast::get_lowest_layer(*ws_stream_).expires_after(std::chrono::seconds(mcp_config_.read_timeout));
            
            // Read a message
            ws_stream_->read(buffer);
            
            // Convert to string
            std::string message = beast::buffers_to_string(buffer.data());
            
            // Handle the message
            handle_websocket_message(message);
            
            // Update last heartbeat time
            last_heartbeat_ = std::chrono::steady_clock::now();
        }
    } catch (const beast::system_error& e) {
        if (e.code() != websocket::error::closed) {
            ws_connected_ = false;
            logger_->log(LogLevel::ERROR, "WebSocket read error: " + std::string(e.what()));
            
            // Attempt reconnection
            if (!stop_requested_ && reconnect_attempts_ < MAX_RECONNECT_ATTEMPTS) {
                attempt_reconnect();
            }
        }
    }
}

void MCPToolIntegration::websocket_write_message(const std::string& message) {
    try {
        if (!ws_connected_ || !ws_stream_) {
            logger_->log(LogLevel::WARN, "Cannot write message: WebSocket not connected");
            return;
        }

        std::lock_guard<std::mutex> lock(ws_mutex_);
        
        // Set timeout for write operation
        beast::get_lowest_layer(*ws_stream_).expires_after(std::chrono::seconds(mcp_config_.connection_timeout));
        
        // Send the message
        ws_stream_->write(net::buffer(message));
        
        logger_->log(LogLevel::DEBUG, "WebSocket message sent: " + message.substr(0, 100));

    } catch (const beast::system_error& e) {
        ws_connected_ = false;
        logger_->log(LogLevel::ERROR, "WebSocket write error: " + std::string(e.what()));
        
        if (!stop_requested_) {
            attempt_reconnect();
        }
    }
}

void MCPToolIntegration::handle_websocket_message(const std::string& message) {
    try {
        nlohmann::json response = nlohmann::json::parse(message);

        logger_->log(LogLevel::DEBUG, "Received WebSocket message: " + message.substr(0, 200));

        if (response.contains("id")) {
            // This is a response to a request
            std::string request_id = response["id"];
            std::lock_guard<std::mutex> lock(ws_mutex_);
            pending_responses_[request_id] = response;
            response_cv_.notify_all();
        } else if (response.contains("method")) {
            // Handle server-initiated messages (notifications)
            handle_mcp_notification(response);
        } else {
            logger_->log(LogLevel::WARN, "Received malformed WebSocket message");
        }

    } catch (const nlohmann::json::parse_error& e) {
        logger_->log(LogLevel::ERROR, "Failed to parse WebSocket message: " + std::string(e.what()));
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Error handling WebSocket message: " + std::string(e.what()));
    }
}

void MCPToolIntegration::handle_mcp_notification(const nlohmann::json& notification) {
    std::string method = notification.value("method", "");

    logger_->log(LogLevel::INFO, "Received MCP notification: " + method);

    if (method == "notifications/tools/list_changed") {
        // Tools list changed, rediscover tools
        logger_->log(LogLevel::INFO, "MCP tools list changed, rediscovering...");
        discover_mcp_tools();
    } else if (method == "notifications/resources/list_changed") {
        // Resources list changed, rediscover resources
        logger_->log(LogLevel::INFO, "MCP resources list changed, rediscovering...");
        discover_mcp_resources();
    } else if (method == "notifications/resources/updated") {
        // Resource updated
        if (notification.contains("params") && notification["params"].contains("uri")) {
            std::string uri = notification["params"]["uri"];
            logger_->log(LogLevel::INFO, "MCP resource updated: " + uri);
            
            // Update the resource in our cache if it exists
            if (available_resources_.find(uri) != available_resources_.end()) {
                // Refresh this specific resource
                read_resource(uri);
            }
        }
    } else if (method == "notifications/message") {
        // Server-sent message notification
        if (notification.contains("params")) {
            logger_->log(LogLevel::INFO, "MCP server message: " + notification["params"].dump());
        }
    } else {
        logger_->log(LogLevel::DEBUG, "Unknown MCP notification: " + method);
    }
}

void MCPToolIntegration::start_heartbeat() {
    if (!io_context_) {
        return;
    }

    heartbeat_timer_ = std::make_unique<net::steady_timer>(*io_context_);
    
    // Schedule first heartbeat
    heartbeat_timer_->expires_after(HEARTBEAT_INTERVAL);
    heartbeat_timer_->async_wait([this](const beast::error_code& ec) {
        if (!ec && !stop_requested_) {
            send_heartbeat();
        }
    });
}

void MCPToolIntegration::send_heartbeat() {
    try {
        // Check if connection is still alive
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_heartbeat_);
        
        if (elapsed > HEARTBEAT_INTERVAL * 2) {
            logger_->log(LogLevel::WARN, "No heartbeat received, connection may be dead");
            ws_connected_ = false;
            attempt_reconnect();
            return;
        }

        // Send ping frame
        if (ws_connected_ && ws_stream_) {
            ws_stream_->ping({});
            logger_->log(LogLevel::DEBUG, "Heartbeat ping sent");
        }

        // Schedule next heartbeat
        if (heartbeat_timer_ && !stop_requested_) {
            heartbeat_timer_->expires_after(HEARTBEAT_INTERVAL);
            heartbeat_timer_->async_wait([this](const beast::error_code& ec) {
                if (!ec && !stop_requested_) {
                    send_heartbeat();
                }
            });
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Heartbeat error: " + std::string(e.what()));
    }
}

void MCPToolIntegration::attempt_reconnect() {
    if (stop_requested_ || reconnect_attempts_ >= MAX_RECONNECT_ATTEMPTS) {
        logger_->log(LogLevel::ERROR, "Max reconnection attempts reached, giving up");
        ws_connected_ = false;
        server_connected_ = false;
        return;
    }

    reconnect_attempts_++;
    logger_->log(LogLevel::INFO, "Attempting reconnection (" + std::to_string(reconnect_attempts_) + 
                 "/" + std::to_string(MAX_RECONNECT_ATTEMPTS) + ")");

    // Clean up existing connection
    cleanup_websocket();

    // Wait before reconnecting
    std::this_thread::sleep_for(RECONNECT_DELAY);

    if (!stop_requested_) {
        // Reinitialize connection
        if (initialize_websocket_connection()) {
            logger_->log(LogLevel::INFO, "Reconnection successful");
            
            // Re-authenticate and rediscover
            if (initialize_mcp_connection()) {
                discover_mcp_tools();
                discover_mcp_resources();
            }
        } else {
            logger_->log(LogLevel::ERROR, "Reconnection failed");
        }
    }
}

void MCPToolIntegration::cleanup_websocket() {
    try {
        if (heartbeat_timer_) {
            heartbeat_timer_->cancel();
        }

        if (ws_stream_ && ws_connected_) {
            beast::error_code ec;
            ws_stream_->close(websocket::close_code::normal, ec);
            if (ec) {
                logger_->log(LogLevel::DEBUG, "WebSocket close error: " + ec.message());
            }
        }

        ws_stream_.reset();
        
        if (io_context_) {
            io_context_->stop();
        }

        if (io_thread_.joinable()) {
            io_thread_.join();
        }

        // Restart io_context for potential reconnection
        if (io_context_) {
            io_context_->restart();
        }

        ws_connected_ = false;
        logger_->log(LogLevel::INFO, "WebSocket cleanup completed");

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Cleanup error: " + std::string(e.what()));
    }
}

nlohmann::json MCPToolIntegration::send_mcp_request(const std::string& method, const nlohmann::json& params) {
    if (!ws_connected_) {
        logger_->log(LogLevel::ERROR, "Cannot send MCP request: WebSocket not connected");
        return {{"error", "WebSocket not connected"}};
    }

    try {
        std::string request_id = generate_request_id();

        nlohmann::json request = {
            {"jsonrpc", "2.0"},
            {"id", request_id},
            {"method", method},
            {"params", params}
        };

        std::string request_str = request.dump();

        logger_->log(LogLevel::DEBUG, "Sending MCP request: " + method + " (id: " + request_id + ")");

        // Send request via WebSocket
        websocket_write_message(request_str);

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
                logger_->log(LogLevel::DEBUG, "Received MCP response for: " + method);
                return response;
            }
        }

        return {{"error", "No response received"}};

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "MCP request failed: " + std::string(e.what()));
        return {{"error", e.what()}};
    }
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

std::string MCPToolIntegration::parse_websocket_url(const std::string& url, std::string& host, 
                                                    std::string& port, std::string& path) {
    try {
        // Parse WebSocket URL (ws://host:port/path or wss://host:port/path)
        std::regex ws_regex(R"((wss?):\/\/([^:\/]+)(?::(\d+))?(\/.*)?)", std::regex::icase);
        std::smatch matches;

        if (std::regex_match(url, matches, ws_regex)) {
            std::string protocol = matches[1].str();
            host = matches[2].str();
            port = matches[3].matched ? matches[3].str() : (protocol == "wss" ? "443" : "80");
            path = matches[4].matched ? matches[4].str() : "/";
            
            logger_->log(LogLevel::DEBUG, "Parsed WebSocket URL: " + host + ":" + port + path);
            return protocol;
        } else {
            logger_->log(LogLevel::ERROR, "Invalid WebSocket URL format: " + url);
            return "";
        }
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "URL parsing error: " + std::string(e.what()));
        return "";
    }
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