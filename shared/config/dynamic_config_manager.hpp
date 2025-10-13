#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <optional>
#include <mutex>
#include <nlohmann/json.hpp>
#include <libpq-fe.h>

// Forward declarations
namespace regulens {
class Logger;

/**
 * @brief Result of configuration validation
 */
struct ConfigValidationResult {
    bool valid;
    std::string error_message;
    nlohmann::json suggested_value;

    ConfigValidationResult(bool v = true, std::string msg = "", nlohmann::json suggested = nullptr)
        : valid(v), error_message(std::move(msg)), suggested_value(std::move(suggested)) {}
};

/**
 * @brief Configuration metadata
 */
struct ConfigMetadata {
    std::string config_type;
    std::string description;
    bool is_sensitive;
    nlohmann::json validation_rules;
    bool requires_restart;
    std::string last_updated;
    std::string updated_by;
};

/**
 * @brief Configuration update request
 */
struct ConfigUpdateRequest {
    std::string key;
    nlohmann::json value;
    std::string user_id;
    std::string reason;
    std::string source;
};

/**
 * @brief Configuration history entry
 */
struct ConfigHistoryEntry {
    std::string history_id;
    std::string config_key;
    nlohmann::json old_value;
    nlohmann::json new_value;
    std::string changed_by;
    std::string changed_at;
    std::string change_reason;
    std::string change_source;
};

/**
 * @brief Dynamic Configuration Manager for runtime configuration updates
 *
 * Provides database-backed configuration management with:
 * - Runtime configuration updates without restart
 * - Audit trail for all changes
 * - Configuration validation
 * - Role-based access control
 * - Configuration history and rollback
 */
class DynamicConfigManager {
public:
    /**
     * @brief Constructor with database connection
     * @param db_conn PostgreSQL database connection
     * @param logger Optional logger instance
     */
    DynamicConfigManager(PGconn* db_conn, std::shared_ptr<Logger> logger = nullptr);

    /**
     * @brief Destructor
     */
    ~DynamicConfigManager() = default;

    // Delete copy/move operations
    DynamicConfigManager(const DynamicConfigManager&) = delete;
    DynamicConfigManager& operator=(const DynamicConfigManager&) = delete;
    DynamicConfigManager(DynamicConfigManager&&) = delete;
    DynamicConfigManager& operator=(DynamicConfigManager&&) = delete;

    /**
     * @brief Initialize the configuration manager by loading from database
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Update configuration value
     * @param request Configuration update request
     * @return true if update successful
     */
    bool update_configuration(const ConfigUpdateRequest& request);

    /**
     * @brief Get configuration value by key
     * @param key Configuration key
     * @return Optional JSON value
     */
    std::optional<nlohmann::json> get_configuration(const std::string& key);

    /**
     * @brief Get all configuration values
     * @return Map of all configuration key-value pairs
     */
    std::unordered_map<std::string, nlohmann::json> get_all_configurations();

    /**
     * @brief Get configuration metadata
     * @param key Configuration key
     * @return Optional metadata
     */
    std::optional<ConfigMetadata> get_configuration_metadata(const std::string& key);

    /**
     * @brief Validate configuration value against rules
     * @param key Configuration key
     * @param value Value to validate
     * @return Validation result
     */
    ConfigValidationResult validate_configuration(const std::string& key, const nlohmann::json& value);

    /**
     * @brief Get configuration history for a key
     * @param key Configuration key
     * @param limit Maximum number of entries to return
     * @return Vector of history entries
     */
    std::vector<ConfigHistoryEntry> get_configuration_history(const std::string& key, int limit = 50);

    /**
     * @brief Rollback configuration to previous value
     * @param history_id History entry ID to rollback to
     * @param user_id User performing the rollback
     * @param reason Reason for rollback
     * @return true if rollback successful
     */
    bool rollback_configuration(const std::string& history_id, const std::string& user_id, const std::string& reason);

    /**
     * @brief Delete configuration
     * @param key Configuration key to delete
     * @param user_id User performing the deletion
     * @param reason Reason for deletion
     * @return true if deletion successful
     */
    bool delete_configuration(const std::string& key, const std::string& user_id, const std::string& reason);

    /**
     * @brief Check if user has permission to update configuration
     * @param key Configuration key
     * @param user_id User ID
     * @return true if user has permission
     */
    bool has_update_permission(const std::string& key, const std::string& user_id);

    /**
     * @brief Refresh configuration cache from database
     * @return true if refresh successful
     */
    bool refresh_cache();

    /**
     * @brief Get configurations that require restart
     * @return Vector of configuration keys that require restart
     */
    std::vector<std::string> get_restart_required_configs();

    /**
     * @brief Notify all registered listeners of configuration change
     * @param key Configuration key that changed
     * @param value New value
     */
    void notify_config_change(const std::string& key, const nlohmann::json& value);

    /**
     * @brief Register a configuration change listener
     * @param listener Function to call on configuration changes
     */
    void register_change_listener(std::function<void(const std::string&, const nlohmann::json&)> listener);

private:
    /**
     * @brief Load configuration from database
     */
    bool load_from_database();

    /**
     * @brief Save configuration to database
     * @param key Configuration key
     * @param value Configuration value
     * @param metadata Configuration metadata
     * @param user_id User making the change
     * @return true if save successful
     */
    bool save_to_database(const std::string& key, const nlohmann::json& value,
                         const ConfigMetadata& metadata, const std::string& user_id);

    /**
     * @brief Save configuration history
     * @param key Configuration key
     * @param old_value Previous value
     * @param new_value New value
     * @param user_id User making the change
     * @param reason Reason for change
     * @param source Change source
     * @return true if history save successful
     */
    bool save_history(const std::string& key, const nlohmann::json& old_value,
                     const nlohmann::json& new_value, const std::string& user_id,
                     const std::string& reason, const std::string& source);

    /**
     * @brief Execute SQL query with error handling
     * @param query SQL query
     * @param param_values Parameter values
     * @param param_lengths Parameter lengths
     * @param param_formats Parameter formats
     * @param n_params Number of parameters
     * @return PGresult pointer (caller must PQclear it)
     */
    PGresult* execute_query(const std::string& query, const char* const* param_values = nullptr,
                           const int* param_lengths = nullptr, const int* param_formats = nullptr,
                           int n_params = 0);

    /**
     * @brief Convert JSON value to appropriate type
     * @param value JSON value
     * @param type Configuration type
     * @return Converted value
     */
    nlohmann::json normalize_value(const nlohmann::json& value, const std::string& type);

    // Database connection
    PGconn* db_conn_;

    // Logger
    std::shared_ptr<Logger> logger_;

    // Configuration cache
    std::unordered_map<std::string, nlohmann::json> config_cache_;
    std::unordered_map<std::string, ConfigMetadata> metadata_cache_;

    // Thread safety
    std::mutex cache_mutex_;
    std::mutex db_mutex_;

    // Change listeners
    std::vector<std::function<void(const std::string&, const nlohmann::json&)>> change_listeners_;
};

} // namespace regulens