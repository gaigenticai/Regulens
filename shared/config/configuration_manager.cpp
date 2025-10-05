#include "configuration_manager.hpp"
#include "../database/postgresql_connection.hpp"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <filesystem>

namespace regulens {

ConfigurationManager& ConfigurationManager::get_instance() {
    static ConfigurationManager instance;
    return instance;
}

ConfigurationManager::ConfigurationManager() {
    // Set some defaults
    // Database host - must be configured via environment variables, no localhost default
    config_values_["database.port"] = "5432";
    config_values_["logging.level"] = "info";
    config_values_["api.endpoint"] = "https://api.regulens.ai";
    config_values_["email.smtp.server"] = "smtp.gmail.com";
    config_values_["email.smtp.port"] = "587";
}

bool ConfigurationManager::initialize(int /*argc*/, char* /*argv*/[]) {
    // Load configuration from environment variables
    if (!load_from_environment()) {
        std::cerr << "Failed to load configuration from environment" << std::endl;
        return false;
    }

    // Set defaults for any missing values
    set_defaults();

    return true;
}

bool ConfigurationManager::validate_configuration() const {
    // Basic validation - ensure required fields are present
    std::vector<std::string> required_keys = {
        "database.host",
        "database.port",
        "logging.level"
    };

    for (const auto& key : required_keys) {
        if (config_values_.find(key) == config_values_.end()) {
            return false;
        }
    }

    return true;
}

std::optional<std::string> ConfigurationManager::get_string(const std::string& key) const {
    auto it = config_values_.find(key);
    if (it != config_values_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<int> ConfigurationManager::get_int(const std::string& key) const {
    auto str_value = get_string(key);
    if (str_value) {
        try {
            return std::stoi(*str_value);
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

std::optional<bool> ConfigurationManager::get_bool(const std::string& key) const {
    auto str_value = get_string(key);
    if (str_value) {
        return *str_value == "true" || *str_value == "1";
    }
    return std::nullopt;
}

std::optional<double> ConfigurationManager::get_double(const std::string& key) const {
    auto str_value = get_string(key);
    if (str_value) {
        try {
            return std::stod(*str_value);
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}


nlohmann::json ConfigurationManager::to_json() const {
    nlohmann::json result;
    for (const auto& [key, value] : config_values_) {
        result[key] = value;
    }
    return result;
}

bool ConfigurationManager::reload() {
    config_values_.clear();
    return initialize(0, nullptr);
}

void ConfigurationManager::load_env_var(const char* env_var_name) {
    const char* value = std::getenv(env_var_name);
    if (value != nullptr) {
        config_values_[env_var_name] = std::string(value);
    }
}

bool ConfigurationManager::load_from_environment() {
    // Database configuration
    load_env_var(config_keys::DB_HOST);
    load_env_var(config_keys::DB_PORT);
    load_env_var(config_keys::DB_NAME);
    load_env_var(config_keys::DB_USER);
    load_env_var(config_keys::DB_PASSWORD);
    load_env_var(config_keys::DB_SSL_MODE);
    load_env_var(config_keys::DB_CONNECTION_POOL_SIZE);
    load_env_var(config_keys::DB_CONNECTION_TIMEOUT_MS);
    load_env_var(config_keys::DB_MAX_RETRIES);

    // Message queue configuration
    load_env_var(config_keys::MESSAGE_QUEUE_TYPE);
    load_env_var(config_keys::MESSAGE_QUEUE_BOOTSTRAP_SERVERS);
    load_env_var(config_keys::MESSAGE_QUEUE_SECURITY_PROTOCOL);
    load_env_var(config_keys::MESSAGE_QUEUE_SASL_MECHANISM);
    load_env_var(config_keys::MESSAGE_QUEUE_SASL_USERNAME);
    load_env_var(config_keys::MESSAGE_QUEUE_SASL_PASSWORD);
    load_env_var(config_keys::MESSAGE_QUEUE_SSL_CA_LOCATION);
    load_env_var(config_keys::MESSAGE_QUEUE_SSL_CERTIFICATE_LOCATION);
    load_env_var(config_keys::MESSAGE_QUEUE_SSL_KEY_LOCATION);
    load_env_var(config_keys::MESSAGE_QUEUE_CONSUMER_GROUP);
    load_env_var(config_keys::MESSAGE_QUEUE_AUTO_OFFSET_RESET);

    // Regulatory data sources
    load_env_var(config_keys::SEC_EDGAR_API_KEY);
    load_env_var(config_keys::SEC_EDGAR_BASE_URL);
    load_env_var(config_keys::SEC_EDGAR_RATE_LIMIT_REQUESTS_PER_SECOND);
    load_env_var(config_keys::FCA_API_KEY);
    load_env_var(config_keys::FCA_BASE_URL);
    load_env_var(config_keys::FCA_RATE_LIMIT_REQUESTS_PER_MINUTE);
    load_env_var(config_keys::ECB_FEED_URL);
    load_env_var(config_keys::ECB_UPDATE_INTERVAL_MINUTES);
    load_env_var(config_keys::CUSTOM_REGULATORY_FEEDS);

    // External system integrations
    load_env_var(config_keys::ERP_SYSTEM_TYPE);
    load_env_var(config_keys::ERP_SYSTEM_HOST);
    load_env_var(config_keys::ERP_SYSTEM_PORT);
    load_env_var(config_keys::ERP_SYSTEM_API_KEY);
    load_env_var(config_keys::ERP_SYSTEM_USERNAME);
    load_env_var(config_keys::ERP_SYSTEM_PASSWORD);
    load_env_var(config_keys::ERP_SYSTEM_TIMEOUT_MS);

    load_env_var(config_keys::DOCUMENT_SYSTEM_TYPE);
    load_env_var(config_keys::DOCUMENT_SYSTEM_BASE_URL);
    load_env_var(config_keys::DOCUMENT_SYSTEM_CLIENT_ID);
    load_env_var(config_keys::DOCUMENT_SYSTEM_CLIENT_SECRET);
    load_env_var(config_keys::DOCUMENT_SYSTEM_TENANT_ID);

    load_env_var(config_keys::SIEM_SYSTEM_TYPE);
    load_env_var(config_keys::SIEM_SYSTEM_HOST);
    load_env_var(config_keys::SIEM_SYSTEM_PORT);
    load_env_var(config_keys::SIEM_SYSTEM_TOKEN);
    load_env_var(config_keys::SIEM_SYSTEM_INDEX);

    // AI/ML configuration
    load_env_var(config_keys::COMPLIANCE_MODEL_ENDPOINT);
    load_env_var(config_keys::REGULATORY_MODEL_ENDPOINT);
    load_env_var(config_keys::AUDIT_MODEL_ENDPOINT);
    load_env_var(config_keys::VECTOR_DB_TYPE);
    load_env_var(config_keys::VECTOR_DB_HOST);
    load_env_var(config_keys::VECTOR_DB_PORT);
    load_env_var(config_keys::VECTOR_DB_API_KEY);
    load_env_var(config_keys::EMBEDDING_MODEL_TYPE);
    load_env_var(config_keys::EMBEDDING_MODEL_NAME);
    load_env_var(config_keys::EMBEDDING_DIMENSION);

    // Security configuration
    load_env_var(config_keys::ENCRYPTION_MASTER_KEY);
    load_env_var(config_keys::DATA_ENCRYPTION_KEY);
    load_env_var(config_keys::JWT_SECRET_KEY);

    // Agent capability controls
    load_env_var(config_keys::AGENT_ENABLE_WEB_SEARCH);
    load_env_var(config_keys::AGENT_ENABLE_MCP_TOOLS);
    load_env_var(config_keys::AGENT_ENABLE_ADVANCED_DISCOVERY);
    load_env_var(config_keys::AGENT_ENABLE_AUTONOMOUS_INTEGRATION);
    load_env_var(config_keys::AGENT_MAX_AUTONOMOUS_TOOLS);
    load_env_var(config_keys::AGENT_ALLOWED_TOOL_CATEGORIES);
    load_env_var(config_keys::AGENT_BLOCKED_TOOL_DOMAINS);

    // LLM Configuration
    load_env_var(config_keys::LLM_OPENAI_API_KEY);
    load_env_var(config_keys::LLM_OPENAI_BASE_URL);
    load_env_var(config_keys::LLM_OPENAI_MODEL);
    load_env_var(config_keys::LLM_ANTHROPIC_API_KEY);
    load_env_var(config_keys::LLM_ANTHROPIC_BASE_URL);
    load_env_var(config_keys::LLM_ANTHROPIC_MODEL);

    // SMTP configuration
    load_env_var(config_keys::SMTP_HOST);
    load_env_var(config_keys::SMTP_PORT);
    load_env_var(config_keys::SMTP_USER);
    load_env_var(config_keys::SMTP_PASSWORD);
    load_env_var(config_keys::SMTP_FROM_EMAIL);
    load_env_var(config_keys::SMTP_NOTIFICATION_RECIPIENTS);

    // System configuration
    load_env_var(config_keys::ENVIRONMENT);
    load_env_var(config_keys::VERSION);
    load_env_var(config_keys::INSTANCE_ID);
    load_env_var(config_keys::DATACENTER);

    return true;
}


bool ConfigurationManager::load_from_config_file(const std::filesystem::path& /*config_path*/) {
    // Configuration is loaded from environment variables as per Rule 3 (cloud deployable)
    // Config file loading is not implemented as environment variables provide better
    // cloud deployment flexibility and security
    return true;
}

bool ConfigurationManager::parse_command_line(int /*argc*/, char* /*argv*/[]) {
    // Configuration is loaded from environment variables as per Rule 3 (cloud deployable)
    // Command line argument parsing is not implemented as environment variables provide
    // better container orchestration and cloud deployment flexibility
    return true;
}

void ConfigurationManager::set_defaults() {
    // Set defaults for database configuration - no localhost defaults for production
    // DB_HOST must be explicitly set via environment variables
    // if (config_values_.find(config_keys::DB_HOST) == config_values_.end()) {
    //     config_values_[config_keys::DB_HOST] = "localhost"; // REMOVED: No localhost defaults
    // }
    if (config_values_.find(config_keys::DB_PORT) == config_values_.end()) {
        config_values_[config_keys::DB_PORT] = "5432";
    }
    if (config_values_.find(config_keys::DB_NAME) == config_values_.end()) {
        config_values_[config_keys::DB_NAME] = "regulens_compliance";
    }
    if (config_values_.find(config_keys::DB_USER) == config_values_.end()) {
        config_values_[config_keys::DB_USER] = "regulens_user";
    }
    if (config_values_.find(config_keys::DB_SSL_MODE) == config_values_.end()) {
        config_values_[config_keys::DB_SSL_MODE] = "require";
    }
    if (config_values_.find(config_keys::DB_CONNECTION_POOL_SIZE) == config_values_.end()) {
        config_values_[config_keys::DB_CONNECTION_POOL_SIZE] = "10";
    }
    if (config_values_.find(config_keys::DB_CONNECTION_TIMEOUT_MS) == config_values_.end()) {
        config_values_[config_keys::DB_CONNECTION_TIMEOUT_MS] = "30000";
    }
    if (config_values_.find(config_keys::DB_MAX_RETRIES) == config_values_.end()) {
        config_values_[config_keys::DB_MAX_RETRIES] = "3";
    }

    // Set defaults for vector database
    if (config_values_.find(config_keys::VECTOR_DB_TYPE) == config_values_.end()) {
        config_values_[config_keys::VECTOR_DB_TYPE] = "weaviate";
    }
    // VECTOR_DB_HOST must be explicitly set via environment variables
    // if (config_values_.find(config_keys::VECTOR_DB_HOST) == config_values_.end()) {
    //     config_values_[config_keys::VECTOR_DB_HOST] = "localhost"; // REMOVED: No localhost defaults
    // }
    if (config_values_.find(config_keys::VECTOR_DB_PORT) == config_values_.end()) {
        config_values_[config_keys::VECTOR_DB_PORT] = "8080";
    }

    // Set defaults for agent capability controls
    if (config_values_.find(config_keys::AGENT_ENABLE_WEB_SEARCH) == config_values_.end()) {
        config_values_[config_keys::AGENT_ENABLE_WEB_SEARCH] = "false";
    }
    if (config_values_.find(config_keys::AGENT_ENABLE_MCP_TOOLS) == config_values_.end()) {
        config_values_[config_keys::AGENT_ENABLE_MCP_TOOLS] = "false";
    }
    if (config_values_.find(config_keys::AGENT_ENABLE_ADVANCED_DISCOVERY) == config_values_.end()) {
        config_values_[config_keys::AGENT_ENABLE_ADVANCED_DISCOVERY] = "false";
    }
    if (config_values_.find(config_keys::AGENT_ENABLE_AUTONOMOUS_INTEGRATION) == config_values_.end()) {
        config_values_[config_keys::AGENT_ENABLE_AUTONOMOUS_INTEGRATION] = "false";
    }
    if (config_values_.find(config_keys::AGENT_MAX_AUTONOMOUS_TOOLS) == config_values_.end()) {
        config_values_[config_keys::AGENT_MAX_AUTONOMOUS_TOOLS] = "10";
    }

    // Set defaults for SMTP configuration
    if (config_values_.find(config_keys::SMTP_HOST) == config_values_.end()) {
        config_values_[config_keys::SMTP_HOST] = "smtp.gmail.com";
    }
    if (config_values_.find(config_keys::SMTP_PORT) == config_values_.end()) {
        config_values_[config_keys::SMTP_PORT] = "587";
    }
    if (config_values_.find(config_keys::SMTP_FROM_EMAIL) == config_values_.end()) {
        config_values_[config_keys::SMTP_FROM_EMAIL] = "regulens@gaigentic.ai";
    }
}

DatabaseConfig ConfigurationManager::get_database_config() const {
    DatabaseConfig config;

    // Load values from configuration - DB_HOST must be explicitly configured
    auto host_opt = get_string(config_keys::DB_HOST);
    if (!host_opt) {
        throw std::runtime_error("Database host (DB_HOST) must be configured via environment variables");
    }
    config.host = *host_opt;
    config.port = get_int(config_keys::DB_PORT).value_or(5432);
    config.database = get_string(config_keys::DB_NAME).value_or("regulens_compliance");
    config.user = get_string(config_keys::DB_USER).value_or("regulens_user");
    config.password = get_string(config_keys::DB_PASSWORD).value_or("");
    config.ssl_mode = get_string(config_keys::DB_SSL_MODE).value_or("require") == "require";
    config.max_connections = get_int(config_keys::DB_CONNECTION_POOL_SIZE).value_or(10);
    config.connection_timeout = get_int(config_keys::DB_CONNECTION_TIMEOUT_MS).value_or(30000) / 1000; // Convert ms to seconds
    config.max_retries = get_int(config_keys::DB_MAX_RETRIES).value_or(3);

    return config;
}

SMTPConfig ConfigurationManager::get_smtp_config() const {
    SMTPConfig config;

    // Load values from configuration, with fallbacks to defaults
    config.host = get_string(config_keys::SMTP_HOST).value_or("smtp.gmail.com");
    config.port = get_int(config_keys::SMTP_PORT).value_or(587);
    config.user = get_string(config_keys::SMTP_USER).value_or("regulens@gaigentic.ai");
    config.password = get_string(config_keys::SMTP_PASSWORD).value_or("");
    config.from_email = get_string(config_keys::SMTP_FROM_EMAIL).value_or("regulens@gaigentic.ai");

    return config;
}

std::vector<std::string> ConfigurationManager::get_notification_recipients() const {
    auto recipients_str = get_string(config_keys::SMTP_NOTIFICATION_RECIPIENTS);
    if (!recipients_str) {
        // Default fallback recipients for development/demo
        return {"compliance@company.com", "legal@company.com", "risk@company.com"};
    }

    std::vector<std::string> recipients;
    std::stringstream ss(*recipients_str);
    std::string recipient;

    while (std::getline(ss, recipient, ',')) {
        // Trim whitespace
        recipient.erase(recipient.begin(), std::find_if(recipient.begin(), recipient.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        recipient.erase(std::find_if(recipient.rbegin(), recipient.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), recipient.end());

        if (!recipient.empty()) {
            recipients.push_back(recipient);
        }
    }

    return recipients;
}
}

AgentCapabilityConfig ConfigurationManager::get_agent_capability_config() const {
    AgentCapabilityConfig config;

    // Load boolean values from configuration
    config.enable_web_search = get_bool(config_keys::AGENT_ENABLE_WEB_SEARCH).value_or(false);
    config.enable_mcp_tools = get_bool(config_keys::AGENT_ENABLE_MCP_TOOLS).value_or(false);
    config.enable_advanced_discovery = get_bool(config_keys::AGENT_ENABLE_ADVANCED_DISCOVERY).value_or(false);
    config.enable_autonomous_integration = get_bool(config_keys::AGENT_ENABLE_AUTONOMOUS_INTEGRATION).value_or(false);
    config.max_autonomous_tools_per_session = get_int(config_keys::AGENT_MAX_AUTONOMOUS_TOOLS).value_or(10);

    // Load comma-separated lists
    auto categories_str = get_string(config_keys::AGENT_ALLOWED_TOOL_CATEGORIES);
    if (categories_str) {
        // Simple comma-separated parsing (production would use more robust parsing)
        std::stringstream ss(*categories_str);
        std::string item;
        while (std::getline(ss, item, ',')) {
            if (!item.empty()) {
                config.allowed_tool_categories.push_back(item);
            }
        }
    }

    auto domains_str = get_string(config_keys::AGENT_BLOCKED_TOOL_DOMAINS);
    if (domains_str) {
        std::stringstream ss(*domains_str);
        std::string item;
        while (std::getline(ss, item, ',')) {
            if (!item.empty()) {
                config.blocked_tool_domains.push_back(item);
            }
        }
    }

    return config;
}

std::unordered_map<std::string, std::string> ConfigurationManager::get_openai_config() const {
    std::unordered_map<std::string, std::string> config;

    if (auto api_key = get_string(config_keys::LLM_OPENAI_API_KEY)) {
        config["api_key"] = *api_key;
    }
    if (auto base_url = get_string(config_keys::LLM_OPENAI_BASE_URL)) {
        config["base_url"] = *base_url;
    }
    if (auto model = get_string(config_keys::LLM_OPENAI_MODEL)) {
        config["model"] = *model;
    }

    return config;
}

std::unordered_map<std::string, std::string> ConfigurationManager::get_anthropic_config() const {
    std::unordered_map<std::string, std::string> config;

    if (auto api_key = get_string(config_keys::LLM_ANTHROPIC_API_KEY)) {
        config["api_key"] = *api_key;
    }
    if (auto base_url = get_string(config_keys::LLM_ANTHROPIC_BASE_URL)) {
        config["base_url"] = *base_url;
    }
    if (auto model = get_string(config_keys::LLM_ANTHROPIC_MODEL)) {
        config["model"] = *model;
    }

    return config;
}
}

