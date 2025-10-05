#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <optional>
#include <vector>
#include <sstream>
#include <nlohmann/json.hpp>

// Include for configuration struct definitions
#include "config_types.hpp"

namespace regulens {

// Forward declarations
class Logger;

// Forward declarations for configuration structs
struct DatabaseConfig;
struct SMTPConfig;

/**
 * @brief Configuration validation result
 */
struct ValidationResult {
    bool valid;
    std::string error_message;

    ValidationResult(bool v = true, std::string msg = "")
        : valid(v), error_message(std::move(msg)) {}
};

/**
 * @brief Environment configuration for different deployment contexts
 */
enum class Environment {
    DEVELOPMENT,
    STAGING,
    PRODUCTION
};

/**
 * @brief Centralized configuration management system
 *
 * Handles loading configuration from environment variables, config files,
 * and provides type-safe access to configuration values with validation.
 */
class ConfigurationManager {
public:
    // Singleton access
    static ConfigurationManager& get_instance();

    // Delete copy/move operations
    ConfigurationManager(const ConfigurationManager&) = delete;
    ConfigurationManager& operator=(const ConfigurationManager&) = delete;
    ConfigurationManager(ConfigurationManager&&) = delete;
    ConfigurationManager& operator=(ConfigurationManager&&) = delete;

    /**
     * @brief Constructor with default initialization
     */
    ConfigurationManager();

    /**
     * @brief Load configuration from command line and environment
     * @param argc Command line argument count
     * @param argv Command line arguments
     * @return true if configuration loaded successfully
     */
    bool initialize(int argc, char* argv[]);

    /**
     * @brief Validate current configuration
     * @return true if configuration is valid
     */
    bool validate_configuration() const;

    /**
     * @brief Get configuration value as string
     * @param key Configuration key
     * @return Optional string value
     */
    std::optional<std::string> get_string(const std::string& key) const;

    /**
     * @brief Get configuration value as integer
     * @param key Configuration key
     * @return Optional integer value
     */
    std::optional<int> get_int(const std::string& key) const;

    /**
     * @brief Get configuration value as boolean
     * @param key Configuration key
     * @return Optional boolean value
     */
    std::optional<bool> get_bool(const std::string& key) const;

    /**
     * @brief Get configuration value as double
     * @param key Configuration key
     * @return Optional double value
     */
    std::optional<double> get_double(const std::string& key) const;

    /**
     * @brief Get current environment
     * @return Current deployment environment
     */
    Environment get_environment() const { return environment_; }

    /**
     * @brief Get configuration as JSON for serialization
     * @return JSON representation of current configuration
     */
    nlohmann::json to_json() const;

    /**
     * @brief Reload configuration from sources
     * @return true if reload successful
     */
    bool reload();

    /**
     * @brief Create database configuration from loaded settings
     * @return DatabaseConfig struct populated with configuration values
     */
    DatabaseConfig get_database_config() const;

    /**
     * @brief Create SMTP configuration from loaded settings
     * @return SMTPConfig struct populated with configuration values
     */
    SMTPConfig get_smtp_config() const;

    /**
     * @brief Get notification email recipients from configuration
     * @return Vector of email addresses for notifications
     */
    std::vector<std::string> get_notification_recipients() const;

    /**
     * @brief Get agent capability configuration
     * @return AgentCapabilityConfig struct with agent settings
     */
    AgentCapabilityConfig get_agent_capability_config() const;

    /**
     * @brief Get LLM configuration for OpenAI
     * @return map of OpenAI LLM configuration
     */
    std::unordered_map<std::string, std::string> get_openai_config() const;

    /**
     * @brief Get LLM configuration for Anthropic
     * @return map of Anthropic LLM configuration
     */
    std::unordered_map<std::string, std::string> get_anthropic_config() const;

private:
    ~ConfigurationManager() = default;

    bool load_from_environment();
    void load_env_var(const char* env_var_name);
    bool load_from_config_file(const std::filesystem::path& config_path);
    bool parse_command_line(int argc, char* argv[]);
    void set_defaults();

    // Configuration storage
    std::unordered_map<std::string, std::string> config_values_;
    Environment environment_{Environment::DEVELOPMENT};
    std::filesystem::path config_file_path_;

    // Logger for configuration operations
    std::shared_ptr<Logger> logger_;
};

/**
 * @brief Configuration key constants
 */
namespace config_keys {
// System configuration
inline constexpr const char* ENVIRONMENT = "REGULENS_ENVIRONMENT";
inline constexpr const char* VERSION = "REGULENS_VERSION";
inline constexpr const char* INSTANCE_ID = "REGULENS_INSTANCE_ID";
inline constexpr const char* DATACENTER = "REGULENS_DATACENTER";

// Database configuration
inline constexpr const char* DB_HOST = "DB_HOST";
inline constexpr const char* DB_PORT = "DB_PORT";
inline constexpr const char* DB_NAME = "DB_NAME";
inline constexpr const char* DB_USER = "DB_USER";
inline constexpr const char* DB_PASSWORD = "DB_PASSWORD";
inline constexpr const char* DB_SSL_MODE = "DB_SSL_MODE";
inline constexpr const char* DB_CONNECTION_POOL_SIZE = "DB_CONNECTION_POOL_SIZE";
inline constexpr const char* DB_CONNECTION_TIMEOUT_MS = "DB_CONNECTION_TIMEOUT_MS";
inline constexpr const char* DB_MAX_RETRIES = "DB_MAX_RETRIES";

// Message queue configuration
inline constexpr const char* MESSAGE_QUEUE_TYPE = "MESSAGE_QUEUE_TYPE";
inline constexpr const char* MESSAGE_QUEUE_BOOTSTRAP_SERVERS = "MESSAGE_QUEUE_BOOTSTRAP_SERVERS";
inline constexpr const char* MESSAGE_QUEUE_SECURITY_PROTOCOL = "MESSAGE_QUEUE_SECURITY_PROTOCOL";
inline constexpr const char* MESSAGE_QUEUE_SASL_MECHANISM = "MESSAGE_QUEUE_SASL_MECHANISM";
inline constexpr const char* MESSAGE_QUEUE_SASL_USERNAME = "MESSAGE_QUEUE_SASL_USERNAME";
inline constexpr const char* MESSAGE_QUEUE_SASL_PASSWORD = "MESSAGE_QUEUE_SASL_PASSWORD";
inline constexpr const char* MESSAGE_QUEUE_SSL_CA_LOCATION = "MESSAGE_QUEUE_SSL_CA_LOCATION";
inline constexpr const char* MESSAGE_QUEUE_SSL_CERTIFICATE_LOCATION = "MESSAGE_QUEUE_SSL_CERTIFICATE_LOCATION";
inline constexpr const char* MESSAGE_QUEUE_SSL_KEY_LOCATION = "MESSAGE_QUEUE_SSL_KEY_LOCATION";
inline constexpr const char* MESSAGE_QUEUE_CONSUMER_GROUP = "MESSAGE_QUEUE_CONSUMER_GROUP";
inline constexpr const char* MESSAGE_QUEUE_AUTO_OFFSET_RESET = "MESSAGE_QUEUE_AUTO_OFFSET_RESET";

// Regulatory data sources
inline constexpr const char* SEC_EDGAR_API_KEY = "SEC_EDGAR_API_KEY";
inline constexpr const char* SEC_EDGAR_BASE_URL = "SEC_EDGAR_BASE_URL";
inline constexpr const char* SEC_EDGAR_RATE_LIMIT_REQUESTS_PER_SECOND = "SEC_EDGAR_RATE_LIMIT_REQUESTS_PER_SECOND";

inline constexpr const char* FCA_API_KEY = "FCA_API_KEY";
inline constexpr const char* FCA_BASE_URL = "FCA_BASE_URL";
inline constexpr const char* FCA_RATE_LIMIT_REQUESTS_PER_MINUTE = "FCA_RATE_LIMIT_REQUESTS_PER_MINUTE";

inline constexpr const char* ECB_FEED_URL = "ECB_FEED_URL";
inline constexpr const char* ECB_UPDATE_INTERVAL_MINUTES = "ECB_UPDATE_INTERVAL_MINUTES";

inline constexpr const char* CUSTOM_REGULATORY_FEEDS = "CUSTOM_REGULATORY_FEEDS";

// External system integrations
inline constexpr const char* ERP_SYSTEM_TYPE = "ERP_SYSTEM_TYPE";
inline constexpr const char* ERP_SYSTEM_HOST = "ERP_SYSTEM_HOST";
inline constexpr const char* ERP_SYSTEM_PORT = "ERP_SYSTEM_PORT";
inline constexpr const char* ERP_SYSTEM_API_KEY = "ERP_SYSTEM_API_KEY";
inline constexpr const char* ERP_SYSTEM_USERNAME = "ERP_SYSTEM_USERNAME";
inline constexpr const char* ERP_SYSTEM_PASSWORD = "ERP_SYSTEM_PASSWORD";
inline constexpr const char* ERP_SYSTEM_TIMEOUT_MS = "ERP_SYSTEM_TIMEOUT_MS";

inline constexpr const char* DOCUMENT_SYSTEM_TYPE = "DOCUMENT_SYSTEM_TYPE";
inline constexpr const char* DOCUMENT_SYSTEM_BASE_URL = "DOCUMENT_SYSTEM_BASE_URL";
inline constexpr const char* DOCUMENT_SYSTEM_CLIENT_ID = "DOCUMENT_SYSTEM_CLIENT_ID";
inline constexpr const char* DOCUMENT_SYSTEM_CLIENT_SECRET = "DOCUMENT_SYSTEM_CLIENT_SECRET";
inline constexpr const char* DOCUMENT_SYSTEM_TENANT_ID = "DOCUMENT_SYSTEM_TENANT_ID";

inline constexpr const char* SIEM_SYSTEM_TYPE = "SIEM_SYSTEM_TYPE";
inline constexpr const char* SIEM_SYSTEM_HOST = "SIEM_SYSTEM_HOST";
inline constexpr const char* SIEM_SYSTEM_PORT = "SIEM_SYSTEM_PORT";
inline constexpr const char* SIEM_SYSTEM_TOKEN = "SIEM_SYSTEM_TOKEN";
inline constexpr const char* SIEM_SYSTEM_INDEX = "SIEM_SYSTEM_INDEX";

// AI/ML configuration
inline constexpr const char* COMPLIANCE_MODEL_ENDPOINT = "COMPLIANCE_MODEL_ENDPOINT";
inline constexpr const char* REGULATORY_MODEL_ENDPOINT = "REGULATORY_MODEL_ENDPOINT";
inline constexpr const char* AUDIT_MODEL_ENDPOINT = "AUDIT_MODEL_ENDPOINT";

inline constexpr const char* VECTOR_DB_TYPE = "VECTOR_DB_TYPE";
inline constexpr const char* VECTOR_DB_HOST = "VECTOR_DB_HOST";
inline constexpr const char* VECTOR_DB_PORT = "VECTOR_DB_PORT";
inline constexpr const char* VECTOR_DB_API_KEY = "VECTOR_DB_API_KEY";

inline constexpr const char* EMBEDDING_MODEL_TYPE = "EMBEDDING_MODEL_TYPE";
inline constexpr const char* EMBEDDING_MODEL_NAME = "EMBEDDING_MODEL_NAME";
inline constexpr const char* EMBEDDING_DIMENSION = "EMBEDDING_DIMENSION";

// Security configuration
inline constexpr const char* ENCRYPTION_MASTER_KEY = "ENCRYPTION_MASTER_KEY";
inline constexpr const char* DATA_ENCRYPTION_KEY = "DATA_ENCRYPTION_KEY";

// SMTP configuration
// Agent capability controls
inline constexpr const char* AGENT_ENABLE_WEB_SEARCH = "AGENT_ENABLE_WEB_SEARCH";
inline constexpr const char* AGENT_ENABLE_MCP_TOOLS = "AGENT_ENABLE_MCP_TOOLS";
inline constexpr const char* AGENT_ENABLE_ADVANCED_DISCOVERY = "AGENT_ENABLE_ADVANCED_DISCOVERY";
inline constexpr const char* AGENT_ENABLE_AUTONOMOUS_INTEGRATION = "AGENT_ENABLE_AUTONOMOUS_INTEGRATION";
inline constexpr const char* AGENT_MAX_AUTONOMOUS_TOOLS = "AGENT_MAX_AUTONOMOUS_TOOLS";
inline constexpr const char* AGENT_ALLOWED_TOOL_CATEGORIES = "AGENT_ALLOWED_TOOL_CATEGORIES";
inline constexpr const char* AGENT_BLOCKED_TOOL_DOMAINS = "AGENT_BLOCKED_TOOL_DOMAINS";

// LLM Configuration
inline constexpr const char* LLM_OPENAI_API_KEY = "LLM_OPENAI_API_KEY";
inline constexpr const char* LLM_OPENAI_BASE_URL = "LLM_OPENAI_BASE_URL";
inline constexpr const char* LLM_OPENAI_MODEL = "LLM_OPENAI_MODEL";
inline constexpr const char* LLM_ANTHROPIC_API_KEY = "LLM_ANTHROPIC_API_KEY";
inline constexpr const char* LLM_ANTHROPIC_BASE_URL = "LLM_ANTHROPIC_BASE_URL";
inline constexpr const char* LLM_ANTHROPIC_MODEL = "LLM_ANTHROPIC_MODEL";

// SMTP configuration
inline constexpr const char* SMTP_HOST = "SMTP_HOST";
inline constexpr const char* SMTP_PORT = "SMTP_PORT";
inline constexpr const char* SMTP_USER = "SMTP_USER";
inline constexpr const char* SMTP_PASSWORD = "SMTP_PASSWORD";
inline constexpr const char* SMTP_FROM_EMAIL = "SMTP_FROM_EMAIL";
inline constexpr const char* SMTP_NOTIFICATION_RECIPIENTS = "SMTP_NOTIFICATION_RECIPIENTS";

inline constexpr const char* JWT_SECRET_KEY = "JWT_SECRET_KEY";
}

} // namespace regulens
