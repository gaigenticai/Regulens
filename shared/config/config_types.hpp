#pragma once

#include <string>

namespace regulens {

/**
 * @brief Database configuration struct
 */
struct DatabaseConfig {
    std::string host; // Must be explicitly configured - no localhost default
    int port = 5432;
    std::string database = "regulens";
    std::string user = "regulens";
    std::string password;
    int max_connections = 10;
    int min_connections = 2;
    int connection_timeout = 30;
    bool ssl_mode = true;
    int max_retries = 3;
};

/**
 * @brief SMTP configuration struct
 */
struct SMTPConfig {
    std::string host = "smtp.gmail.com";
    int port = 587;
    std::string user = "regulens@gaigentic.ai";
    std::string password;
    std::string from_email = "regulens@gaigentic.ai";
};

struct AgentCapabilityConfig {
    bool enable_web_search = false;
    bool enable_mcp_tools = false;
    bool enable_advanced_discovery = false;
    bool enable_autonomous_integration = false;
    int max_autonomous_tools_per_session = 10;
    std::vector<std::string> allowed_tool_categories;
    std::vector<std::string> blocked_tool_domains;
};

} // namespace regulens
