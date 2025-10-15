
#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

#include "../database/postgresql_connection.hpp"

namespace regulens {

class StructuredLogger;

enum class ConfigScope {
    GLOBAL,
    USER,
    ORGANIZATION,
    ENVIRONMENT,
    MODULE
};

enum class ConfigDataType {
    STRING,
    INTEGER,
    FLOAT,
    BOOLEAN,
    JSON,
    SECRET
};

struct ConfigValidationResult {
    bool is_valid = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    nlohmann::json normalized_value = nlohmann::json::object();
    std::optional<nlohmann::json> suggested_value;
};

struct ConfigMetadata {
    ConfigDataType data_type = ConfigDataType::JSON;
    ConfigScope scope = ConfigScope::GLOBAL;
    std::string module_name;
    std::string description;
    bool is_sensitive = false;
    bool requires_restart = false;
    std::vector<std::string> tags;
    nlohmann::json validation_rules = nlohmann::json::object();
    std::string last_updated;
    std::string updated_by;
    int version = 1;
    std::optional<std::string> created_by;
    std::chrono::system_clock::time_point created_at{};
    std::chrono::system_clock::time_point updated_at{};
};

struct ConfigUpdateRequest {
    std::string key;
    nlohmann::json value;
    std::string user_id;
    std::string reason;
    std::string source;
    ConfigScope scope = ConfigScope::GLOBAL;
    std::string module_name;
    bool is_encrypted = false;
    bool requires_restart = false;
    std::string description;
    std::vector<std::string> tags;
    nlohmann::json validation_rules = nlohmann::json::object();
    std::optional<ConfigDataType> data_type_override;
};

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

struct ConfigValue {
    std::string key;
    nlohmann::json value;
    ConfigMetadata metadata;
    bool is_encrypted = false;
    std::optional<std::string> updated_by;
    std::optional<std::string> created_by;
    std::chrono::system_clock::time_point created_at{};
    std::chrono::system_clock::time_point updated_at{};
};

struct ConfigChangeLog {
    std::string change_id;
    std::string key;
    ConfigScope scope = ConfigScope::GLOBAL;
    nlohmann::json old_value;
    nlohmann::json new_value;
    std::string changed_by;
    std::string change_reason;
    std::string change_source;
    int version = 1;
    std::chrono::system_clock::time_point changed_at{};
};

class DynamicConfigManager {
public:
    DynamicConfigManager(std::shared_ptr<PostgreSQLConnection> db_conn,
                         std::shared_ptr<StructuredLogger> logger = nullptr);
    ~DynamicConfigManager() = default;

    DynamicConfigManager(const DynamicConfigManager&) = delete;
    DynamicConfigManager& operator=(const DynamicConfigManager&) = delete;
    DynamicConfigManager(DynamicConfigManager&&) = delete;
    DynamicConfigManager& operator=(DynamicConfigManager&&) = delete;

    bool initialize();

    std::optional<ConfigValue> get_config(const std::string& key,
                                          ConfigScope scope = ConfigScope::GLOBAL);

    bool set_config(const std::string& key,
                    const nlohmann::json& value,
                    ConfigScope scope,
                    const std::string& module_name,
                    const std::string& user_id,
                    const std::string& reason,
                    bool is_encrypted = false,
                    bool requires_restart = false,
                    const std::string& description = "",
                    const std::vector<std::string>& tags = {},
                    const nlohmann::json& validation_rules = nlohmann::json::object(),
                    std::optional<ConfigDataType> data_type_override = std::nullopt);

    bool update_config(const ConfigValue& config,
                       const std::string& user_id,
                       const std::string& reason);

    bool delete_config(const std::string& key,
                       ConfigScope scope,
                       const std::string& user_id);

    std::vector<ConfigValue> get_configs_by_scope(ConfigScope scope);
    std::vector<ConfigValue> get_configs_by_module(const std::string& module_name);

    std::vector<ConfigChangeLog> get_config_history(const std::string& key,
                                                    std::optional<std::chrono::system_clock::time_point> since = std::nullopt,
                                                    int limit = 50);

    bool register_config_schema(const std::string& key,
                                ConfigDataType data_type,
                                const nlohmann::json& validation_rules,
                                const std::string& description,
                                ConfigScope scope,
                                const std::string& module_name,
                                const std::string& user_id);

    ConfigValidationResult validate_config_value(const std::string& key,
                                                 const nlohmann::json& value,
                                                 std::optional<ConfigDataType> override_type = std::nullopt) const;

    void reload_configs();

    nlohmann::json get_config_usage_stats();
    std::vector<std::pair<std::string, int>> get_most_changed_configs(int limit);

    std::string scope_to_string(ConfigScope scope) const;
    std::string data_type_to_string(ConfigDataType type) const;
    ConfigScope parse_scope(const std::string& scope) const;
    ConfigDataType parse_data_type(const std::string& type) const;

    void register_change_listener(std::function<void(const ConfigValue&)> listener);

    bool update_configuration(const ConfigUpdateRequest& request);
    std::optional<nlohmann::json> get_configuration(const std::string& key);
    std::unordered_map<std::string, nlohmann::json> get_all_configurations();
    std::optional<ConfigMetadata> get_configuration_metadata(const std::string& key);
    ConfigValidationResult validate_configuration(const std::string& key, const nlohmann::json& value);
    std::vector<ConfigHistoryEntry> get_configuration_history_legacy(const std::string& key, int limit = 50);
    bool rollback_configuration(const std::string& history_id, const std::string& user_id, const std::string& reason);
    bool delete_configuration(const std::string& key, const std::string& user_id, const std::string& reason);
    bool has_update_permission(const std::string& key, const std::string& user_id);
    bool refresh_cache();
    std::vector<std::string> get_restart_required_configs();

private:
    struct ValidationContext {
        ConfigDataType data_type = ConfigDataType::JSON;
        nlohmann::json rules = nlohmann::json::object();
        std::optional<double> min_numeric;
        std::optional<double> max_numeric;
        std::optional<std::size_t> min_length;
        std::optional<std::size_t> max_length;
        std::unordered_set<std::string> allowed_values;
        std::optional<std::string> regex_pattern;
    };

    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<StructuredLogger> logger_;

    mutable std::mutex cache_mutex_;
    std::unordered_map<std::string, ConfigValue> config_cache_;
    std::unordered_map<std::string, ValidationContext> validation_cache_;

    mutable std::mutex listener_mutex_;
    std::vector<std::function<void(const ConfigValue&)>> change_listeners_;

    std::string make_cache_key(const std::string& key, ConfigScope scope) const;
    void store_in_cache(const ConfigValue& value);
    std::optional<ConfigValue> load_from_cache(const std::string& cache_key) const;

    std::optional<ConfigValue> fetch_config_from_db(const std::string& key, ConfigScope scope) const;
    std::vector<ConfigValue> fetch_configs_with_query(const std::string& query,
                                                      const std::vector<std::string>& params) const;

    bool persist_config(const ConfigValue& config,
                        const ConfigValue* previous,
                        const nlohmann::json& validation_rules,
                        const std::string& user_id,
                        const std::string& reason);

    void record_history(const ConfigValue& previous,
                        const ConfigValue& current,
                        const std::string& user_id,
                        const std::string& reason,
                        const std::string& source);

    ConfigChangeLog hydrate_change_log(const nlohmann::json& row) const;
    ConfigValue hydrate_config_row(const nlohmann::json& row) const;

    ValidationContext build_validation_context(const nlohmann::json& metadata_json,
                                               ConfigDataType data_type) const;

    ConfigDataType infer_data_type(const nlohmann::json& value) const;
    bool compare_json(const nlohmann::json& lhs, const nlohmann::json& rhs) const;

    void notify_listeners(const ConfigValue& value);
};

} // namespace regulens
