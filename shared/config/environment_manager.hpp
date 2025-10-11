/**
 * Environment Manager - Header
 * 
 * Production-grade environment variable management and configuration system.
 * Supports environment-specific configs, validation, and secure secret management.
 */

#ifndef REGULENS_ENVIRONMENT_MANAGER_HPP
#define REGULENS_ENVIRONMENT_MANAGER_HPP

#include <string>
#include <map>
#include <vector>
#include <chrono>
#include <memory>
#include <functional>

namespace regulens {

// Environment type
enum class Environment {
    DEVELOPMENT,
    STAGING,
    PRODUCTION,
    TESTING
};

// Configuration value type
enum class ConfigValueType {
    STRING,
    INTEGER,
    BOOLEAN,
    FLOAT,
    SECRET,        // Encrypted secrets
    URL,
    EMAIL,
    PORT,
    FILE_PATH
};

// Configuration entry
struct ConfigEntry {
    std::string key;
    std::string value;
    ConfigValueType type;
    bool required;
    std::string default_value;
    std::string description;
    bool is_secret;
    bool is_validated;
    std::vector<std::string> allowed_values;  // For enum-like configs
};

/**
 * Environment Manager
 * 
 * Comprehensive environment and configuration management.
 * Features:
 * - Environment variable loading from .env files
 * - Hierarchical configuration (system -> environment -> local)
 * - Configuration validation
 * - Type-safe configuration access
 * - Secret encryption/decryption
 * - Configuration hot-reload
 * - Environment-specific overrides
 * - Integration with HashiCorp Vault, AWS Secrets Manager
 * - Configuration versioning
 * - Audit logging of configuration changes
 */
class EnvironmentManager {
public:
    /**
     * Get singleton instance
     */
    static EnvironmentManager& getInstance();

    // Delete copy constructor and assignment
    EnvironmentManager(const EnvironmentManager&) = delete;
    EnvironmentManager& operator=(const EnvironmentManager&) = delete;

    /**
     * Initialize environment manager
     * 
     * @param env_file Path to .env file
     * @param environment Environment type
     * @return true if successful
     */
    bool initialize(
        const std::string& env_file = ".env",
        Environment environment = Environment::PRODUCTION
    );

    /**
     * Load environment variables from file
     * 
     * @param file_path Path to .env file
     * @return true if successful
     */
    bool load_from_file(const std::string& file_path);

    /**
     * Load from multiple files (hierarchical)
     * Loads in order: .env -> .env.{environment} -> .env.local
     * 
     * @param base_path Base directory
     * @return true if successful
     */
    bool load_hierarchical(const std::string& base_path = ".");

    /**
     * Get string value
     * 
     * @param key Configuration key
     * @param default_value Default if not found
     * @return Configuration value
     */
    std::string get(
        const std::string& key,
        const std::string& default_value = ""
    );

    /**
     * Get integer value
     * 
     * @param key Configuration key
     * @param default_value Default if not found
     * @return Configuration value
     */
    int get_int(const std::string& key, int default_value = 0);

    /**
     * Get boolean value
     * 
     * @param key Configuration key
     * @param default_value Default if not found
     * @return Configuration value
     */
    bool get_bool(const std::string& key, bool default_value = false);

    /**
     * Get float value
     * 
     * @param key Configuration key
     * @param default_value Default if not found
     * @return Configuration value
     */
    double get_float(const std::string& key, double default_value = 0.0);

    /**
     * Get secret value (decrypted)
     * 
     * @param key Secret key
     * @return Decrypted secret value
     */
    std::string get_secret(const std::string& key);

    /**
     * Set configuration value
     * 
     * @param key Configuration key
     * @param value Configuration value
     * @param type Value type
     * @param is_secret Whether value is a secret
     * @return true if successful
     */
    bool set(
        const std::string& key,
        const std::string& value,
        ConfigValueType type = ConfigValueType::STRING,
        bool is_secret = false
    );

    /**
     * Check if configuration key exists
     * 
     * @param key Configuration key
     * @return true if exists
     */
    bool has(const std::string& key);

    /**
     * Get all configuration keys
     * 
     * @param include_secrets Whether to include secret keys
     * @return Vector of keys
     */
    std::vector<std::string> get_all_keys(bool include_secrets = false);

    /**
     * Register required configuration
     * 
     * @param key Configuration key
     * @param type Value type
     * @param description Description
     * @param default_value Default value
     * @return true if successful
     */
    bool register_required(
        const std::string& key,
        ConfigValueType type,
        const std::string& description,
        const std::string& default_value = ""
    );

    /**
     * Validate all configurations
     * Checks that all required configs are present and valid
     * 
     * @return true if all valid
     */
    bool validate_all();

    /**
     * Get validation errors
     * 
     * @return Vector of validation error messages
     */
    std::vector<std::string> get_validation_errors();

    /**
     * Export configuration
     * 
     * @param include_secrets Whether to include secrets (encrypted)
     * @return JSON configuration
     */
    std::string export_config(bool include_secrets = false);

    /**
     * Import configuration from JSON
     * 
     * @param json_config JSON configuration
     * @return true if successful
     */
    bool import_config(const std::string& json_config);

    /**
     * Set encryption key for secrets
     * 
     * @param encryption_key Encryption key (must be 32 bytes for AES-256)
     * @return true if successful
     */
    bool set_encryption_key(const std::string& encryption_key);

    /**
     * Load secrets from external store
     * 
     * @param store_type Store type (vault, aws, azure, gcp)
     * @param store_config Store configuration
     * @return true if successful
     */
    bool load_secrets_from_store(
        const std::string& store_type,
        const std::map<std::string, std::string>& store_config
    );

    /**
     * Reload configuration
     * Hot-reloads configuration from files
     * 
     * @return true if successful
     */
    bool reload();

    /**
     * Get current environment
     * 
     * @return Environment type
     */
    Environment get_environment();

    /**
     * Set environment
     * 
     * @param environment Environment type
     */
    void set_environment(Environment environment);

    /**
     * Get environment name
     * 
     * @return Environment name string
     */
    std::string get_environment_name();

    /**
     * Enable configuration watch
     * Automatically reloads when files change
     * 
     * @param enabled Whether to enable
     * @return true if successful
     */
    bool enable_watch(bool enabled = true);

    /**
     * Get configuration change history
     * 
     * @return Vector of configuration changes
     */
    std::vector<std::string> get_change_history();

    /**
     * Clear configuration
     * Removes all loaded configuration
     */
    void clear();

private:
    EnvironmentManager() = default;
    ~EnvironmentManager() = default;

    Environment environment_;
    std::string encryption_key_;
    bool initialized_;
    bool watch_enabled_;

    std::map<std::string, ConfigEntry> configurations_;
    std::vector<std::string> validation_errors_;
    std::vector<std::string> change_history_;

    /**
     * Parse .env file
     */
    bool parse_env_file(const std::string& file_path);

    /**
     * Parse environment line
     */
    bool parse_env_line(const std::string& line);

    /**
     * Expand environment variables
     * Handles ${VAR} and $VAR syntax
     */
    std::string expand_variables(const std::string& value);

    /**
     * Validate configuration entry
     */
    bool validate_entry(const ConfigEntry& entry);

    /**
     * Encrypt secret
     */
    std::string encrypt_secret(const std::string& secret);

    /**
     * Decrypt secret
     */
    std::string decrypt_secret(const std::string& encrypted_secret);

    /**
     * Load from HashiCorp Vault
     */
    bool load_from_vault(const std::map<std::string, std::string>& config);

    /**
     * Load from AWS Secrets Manager
     */
    bool load_from_aws_secrets(const std::map<std::string, std::string>& config);

    /**
     * Load from Azure Key Vault
     */
    bool load_from_azure_keyvault(const std::map<std::string, std::string>& config);

    /**
     * Load from GCP Secret Manager
     */
    bool load_from_gcp_secrets(const std::map<std::string, std::string>& config);

    /**
     * Watch for file changes
     */
    void watch_files();

    /**
     * Log configuration change
     */
    void log_change(const std::string& key, const std::string& old_value, const std::string& new_value);
};

/**
 * Helper macro for getting configuration values
 */
#define ENV_GET(key, default) regulens::EnvironmentManager::getInstance().get(key, default)
#define ENV_GET_INT(key, default) regulens::EnvironmentManager::getInstance().get_int(key, default)
#define ENV_GET_BOOL(key, default) regulens::EnvironmentManager::getInstance().get_bool(key, default)
#define ENV_GET_SECRET(key) regulens::EnvironmentManager::getInstance().get_secret(key)

} // namespace regulens

#endif // REGULENS_ENVIRONMENT_MANAGER_HPP

