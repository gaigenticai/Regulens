
#include "dynamic_config_manager.hpp"

#include "../logging/structured_logger.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fmt/core.h>
#include <iomanip>
#include <regex>
#include <spdlog/spdlog.h>
#include <sstream>
#include <unordered_set>

namespace regulens {
namespace {

constexpr const char* kComponent = "DynamicConfigManager";

std::string to_upper_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

std::string to_lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::chrono::system_clock::time_point parse_timestamp(const std::string& timestamp) {
    if (timestamp.empty()) {
        return std::chrono::system_clock::now();
    }

    std::tm tm{};
    std::istringstream ss(timestamp);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) {
        ss.clear();
        ss.str(timestamp);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    }
#if defined(_WIN32)
    time_t time_value = _mkgmtime(&tm);
#else
    time_t time_value = timegm(&tm);
#endif
    if (time_value == -1) {
        return std::chrono::system_clock::now();
    }
    return std::chrono::system_clock::from_time_t(time_value);
}

std::string format_timestamp(const std::chrono::system_clock::time_point& tp) {
    auto time_t_value = std::chrono::system_clock::to_time_t(tp);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t_value), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

nlohmann::json safe_parse_json(const std::string& payload, const nlohmann::json& fallback = nlohmann::json::object()) {
    if (payload.empty()) {
        return fallback;
    }
    try {
        return nlohmann::json::parse(payload);
    } catch (...) {
        return fallback;
    }
}

std::vector<std::string> to_string_vector(const nlohmann::json& node) {
    std::vector<std::string> values;
    if (!node.is_array()) {
        return values;
    }
    for (const auto& item : node) {
        if (item.is_string()) {
            values.push_back(item.get<std::string>());
        }
    }
    return values;
}

std::string scope_to_key(ConfigScope scope) {
    switch (scope) {
        case ConfigScope::GLOBAL: return "GLOBAL";
        case ConfigScope::USER: return "USER";
        case ConfigScope::ORGANIZATION: return "ORGANIZATION";
        case ConfigScope::ENVIRONMENT: return "ENVIRONMENT";
        case ConfigScope::MODULE: return "MODULE";
        default: return "GLOBAL";
    }
}

} // namespace

DynamicConfigManager::DynamicConfigManager(std::shared_ptr<PostgreSQLConnection> db_conn,
                                           std::shared_ptr<StructuredLogger> logger)
    : db_conn_(std::move(db_conn)) {
    if (!db_conn_) {
        throw std::runtime_error("PostgreSQLConnection is required for DynamicConfigManager");
    }

    if (logger) {
        logger_ = std::move(logger);
    } else {
        logger_ = std::shared_ptr<StructuredLogger>(&StructuredLogger::get_instance(), [](StructuredLogger*){});
    }
}

bool DynamicConfigManager::initialize() {
    reload_configs();
    return true;
}

std::string DynamicConfigManager::make_cache_key(const std::string& key, ConfigScope scope) const {
    return scope_to_string(scope) + "::" + key;
}

void DynamicConfigManager::store_in_cache(const ConfigValue& value) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    config_cache_[make_cache_key(value.key, value.metadata.scope)] = value;
    validation_cache_[value.key] = build_validation_context(value.metadata.validation_rules,
                                                            value.metadata.data_type);
}

std::optional<ConfigValue> DynamicConfigManager::load_from_cache(const std::string& cache_key) const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = config_cache_.find(cache_key);
    if (it != config_cache_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::string DynamicConfigManager::scope_to_string(ConfigScope scope) const {
    return scope_to_key(scope);
}

std::string DynamicConfigManager::data_type_to_string(ConfigDataType type) const {
    switch (type) {
        case ConfigDataType::STRING: return "string";
        case ConfigDataType::INTEGER: return "integer";
        case ConfigDataType::FLOAT: return "float";
        case ConfigDataType::BOOLEAN: return "boolean";
        case ConfigDataType::JSON: return "json";
        case ConfigDataType::SECRET: return "secret";
        default: return "json";
    }
}

ConfigScope DynamicConfigManager::parse_scope(const std::string& scope) const {
    std::string upper = to_upper_copy(scope);
    if (upper == "USER") return ConfigScope::USER;
    if (upper == "ORGANIZATION") return ConfigScope::ORGANIZATION;
    if (upper == "ENVIRONMENT") return ConfigScope::ENVIRONMENT;
    if (upper == "MODULE") return ConfigScope::MODULE;
    return ConfigScope::GLOBAL;
}

ConfigDataType DynamicConfigManager::parse_data_type(const std::string& type) const {
    std::string lower = to_lower_copy(type);
    if (lower == "string") return ConfigDataType::STRING;
    if (lower == "integer") return ConfigDataType::INTEGER;
    if (lower == "float") return ConfigDataType::FLOAT;
    if (lower == "boolean") return ConfigDataType::BOOLEAN;
    if (lower == "secret") return ConfigDataType::SECRET;
    return ConfigDataType::JSON;
}

ConfigDataType DynamicConfigManager::infer_data_type(const nlohmann::json& value) const {
    if (value.is_boolean()) return ConfigDataType::BOOLEAN;
    if (value.is_number_integer()) return ConfigDataType::INTEGER;
    if (value.is_number_float()) return ConfigDataType::FLOAT;
    if (value.is_string()) return ConfigDataType::STRING;
    return ConfigDataType::JSON;
}

bool DynamicConfigManager::compare_json(const nlohmann::json& lhs, const nlohmann::json& rhs) const {
    if (lhs.type() != rhs.type()) {
        return false;
    }
    return lhs.dump() == rhs.dump();
}

DynamicConfigManager::ValidationContext DynamicConfigManager::build_validation_context(
    const nlohmann::json& metadata_json,
    ConfigDataType data_type) const {
    ValidationContext ctx;
    ctx.data_type = data_type;

    if (!metadata_json.is_object()) {
        return ctx;
    }

    if (metadata_json.contains("numeric")) {
        const auto& numeric = metadata_json["numeric"];
        if (numeric.contains("min") && numeric["min"].is_number()) {
            ctx.min_numeric = numeric["min"].get<double>();
        }
        if (numeric.contains("max") && numeric["max"].is_number()) {
            ctx.max_numeric = numeric["max"].get<double>();
        }
    }

    if (metadata_json.contains("length")) {
        const auto& length_rules = metadata_json["length"];
        if (length_rules.contains("min") && length_rules["min"].is_number_unsigned()) {
            ctx.min_length = length_rules["min"].get<std::size_t>();
        }
        if (length_rules.contains("max") && length_rules["max"].is_number_unsigned()) {
            ctx.max_length = length_rules["max"].get<std::size_t>();
        }
    }

    if (metadata_json.contains("allowed_values") && metadata_json["allowed_values"].is_array()) {
        for (const auto& item : metadata_json["allowed_values"]) {
            if (item.is_string()) {
                ctx.allowed_values.insert(item.get<std::string>());
            }
        }
    }

    if (metadata_json.contains("pattern") && metadata_json["pattern"].is_string()) {
        ctx.regex_pattern = metadata_json["pattern"].get<std::string>();
    }

    ctx.rules = metadata_json;
    return ctx;
}

ConfigValue DynamicConfigManager::hydrate_config_row(const nlohmann::json& row) const {
    ConfigValue config;
    config.key = row.value("config_key", "");
    config.value = safe_parse_json(row.value("config_value", "{}"), nlohmann::json::object());

    auto metadata_json = safe_parse_json(row.value("validation_rules", "{}"), nlohmann::json::object());
    const bool has_metadata_block = metadata_json.contains("metadata");
    const nlohmann::json metadata_block = has_metadata_block ? metadata_json.value("metadata", nlohmann::json::object()) : metadata_json;
    const nlohmann::json rules_block = has_metadata_block ? metadata_json.value("rules", nlohmann::json::object()) : metadata_json.value("rules", nlohmann::json::object());

    ConfigMetadata metadata;
    metadata.data_type = parse_data_type(row.value("config_type", "json"));
    metadata.scope = parse_scope(metadata_block.value("scope", "GLOBAL"));
    metadata.module_name = metadata_block.value("module_name", "");
    metadata.description = row.value("description", metadata_block.value("description", ""));
    metadata.is_sensitive = row.value("is_sensitive", false);
    metadata.requires_restart = row.value("requires_restart", false);
    metadata.tags = to_string_vector(metadata_block.value("tags", nlohmann::json::array()));
    metadata.validation_rules = rules_block;
    metadata.last_updated = row.value("updated_at", "");
    metadata.updated_by = row.value("updated_by", "");
    metadata.version = metadata_block.value("version", metadata_block.value("revision", 1));
    metadata.created_by = metadata_block.contains("created_by") && metadata_block["created_by"].is_string()
        ? std::optional<std::string>(metadata_block["created_by"].get<std::string>())
        : std::nullopt;

    const auto created_at_str = row.value("created_at", "");
    const auto updated_at_str = row.value("updated_at", "");
    metadata.created_at = parse_timestamp(created_at_str);
    metadata.updated_at = parse_timestamp(updated_at_str);

    config.metadata = metadata;
    config.is_encrypted = metadata.is_sensitive;
    config.updated_by = metadata_block.contains("updated_by") && metadata_block["updated_by"].is_string()
        ? std::optional<std::string>(metadata_block["updated_by"].get<std::string>())
        : std::optional<std::string>(metadata.updated_by);
    config.created_by = metadata.created_by;
    config.created_at = metadata.created_at;
    config.updated_at = metadata.updated_at;

    return config;
}

ConfigChangeLog DynamicConfigManager::hydrate_change_log(const nlohmann::json& row) const {
    ConfigChangeLog log_entry;
    log_entry.change_id = row.value("history_id", "");
    log_entry.key = row.value("config_key", "");
    log_entry.old_value = safe_parse_json(row.value("old_value", "null"), nullptr);
    log_entry.new_value = safe_parse_json(row.value("new_value", "null"), nullptr);
    log_entry.changed_by = row.value("changed_by", "");
    log_entry.change_reason = row.value("change_reason", "");
    log_entry.change_source = row.value("change_source", "manual");
    log_entry.version = row.value("version", 1);
    log_entry.scope = parse_scope(row.value("scope", "GLOBAL"));
    log_entry.changed_at = parse_timestamp(row.value("changed_at", ""));
    return log_entry;
}

std::optional<ConfigValue> DynamicConfigManager::fetch_config_from_db(const std::string& key, ConfigScope scope) const {
    const std::string query = R"(
        SELECT
            config_key,
            config_value::text AS config_value,
            config_type,
            description,
            is_sensitive,
            COALESCE(validation_rules::text, '{}') AS validation_rules,
            requires_restart,
            COALESCE(updated_by::text, '') AS updated_by,
            created_at::text AS created_at,
            updated_at::text AS updated_at
        FROM system_configuration
        WHERE config_key = $1
          AND (
            COALESCE((validation_rules -> 'metadata' ->> 'scope'), 'GLOBAL') = $2
          )
    )";

    auto row = db_conn_->execute_query_single(query, {key, scope_to_string(scope)});
    if (!row) {
        return std::nullopt;
    }

    ConfigValue value = hydrate_config_row(*row);
    value.metadata.scope = scope;
    return value;
}

std::vector<ConfigValue> DynamicConfigManager::fetch_configs_with_query(const std::string& query,
                                                                        const std::vector<std::string>& params) const {
    std::vector<ConfigValue> results;
    auto rows = db_conn_->execute_query_multi(query, params);
    results.reserve(rows.size());
    for (const auto& row : rows) {
        results.push_back(hydrate_config_row(row));
    }
    return results;
}

std::optional<ConfigValue> DynamicConfigManager::get_config(const std::string& key, ConfigScope scope) {
    auto cache_key = make_cache_key(key, scope);
    if (auto cached = load_from_cache(cache_key)) {
        return cached;
    }

    auto db_value = fetch_config_from_db(key, scope);
    if (db_value) {
        store_in_cache(*db_value);
    }
    return db_value;
}

bool DynamicConfigManager::persist_config(const ConfigValue& config,
                                          const ConfigValue* previous,
                                          const nlohmann::json& validation_rules,
                                          const std::string& user_id,
                                          const std::string& reason) {
    nlohmann::json metadata_json;
    metadata_json["rules"] = validation_rules;
    metadata_json["metadata"] = {
        {"scope", scope_to_string(config.metadata.scope)},
        {"module_name", config.metadata.module_name},
        {"tags", config.metadata.tags},
        {"version", config.metadata.version},
        {"description", config.metadata.description},
        {"requires_restart", config.metadata.requires_restart},
        {"is_sensitive", config.metadata.is_sensitive},
        {"data_type", data_type_to_string(config.metadata.data_type)},
        {"updated_by", user_id}
    };
    if (config.created_by) {
        metadata_json["metadata"]["created_by"] = *config.created_by;
    }

    const std::string query = R"(
        INSERT INTO system_configuration (
            config_key,
            config_value,
            config_type,
            description,
            is_sensitive,
            validation_rules,
            updated_by,
            requires_restart
        ) VALUES ($1, $2::jsonb, $3, $4, $5, $6::jsonb, $7::uuid, $8)
        ON CONFLICT (config_key) DO UPDATE SET
            config_value = EXCLUDED.config_value,
            config_type = EXCLUDED.config_type,
            description = EXCLUDED.description,
            is_sensitive = EXCLUDED.is_sensitive,
            validation_rules = EXCLUDED.validation_rules,
            updated_by = EXCLUDED.updated_by,
            requires_restart = EXCLUDED.requires_restart,
            last_updated = NOW()
    )";

    bool success = db_conn_->execute_command(query, {
        config.key,
        config.value.dump(),
        data_type_to_string(config.metadata.data_type),
        config.metadata.description,
        config.metadata.is_sensitive ? "true" : "false",
        metadata_json.dump(),
        user_id,
        config.metadata.requires_restart ? "true" : "false"
    });

    if (!success) {
        logger_->error(fmt::format("Failed to persist configuration {}", config.key), kComponent, __func__);
        return false;
    }

    if (previous && !compare_json(previous->value, config.value)) {
        record_history(*previous, config, user_id, reason, "api");
    }

    store_in_cache(config);
    notify_listeners(config);
    return true;
}

bool DynamicConfigManager::set_config(const std::string& key,
                                      const nlohmann::json& value,
                                      ConfigScope scope,
                                      const std::string& module_name,
                                      const std::string& user_id,
                                      const std::string& reason,
                                      bool is_encrypted,
                                      bool requires_restart,
                                      const std::string& description,
                                      const std::vector<std::string>& tags,
                                      const nlohmann::json& validation_rules,
                                      std::optional<ConfigDataType> data_type_override) {
    auto existing = get_config(key, scope);

    ConfigValue config;
    config.key = key;
    config.value = value;
    config.metadata.scope = scope;
    config.metadata.module_name = module_name;
    config.metadata.description = description;
    config.metadata.is_sensitive = is_encrypted;
    config.metadata.requires_restart = requires_restart;
    config.metadata.tags = tags;
    config.metadata.validation_rules = validation_rules;
    config.metadata.data_type = data_type_override.value_or(infer_data_type(value));
    config.metadata.version = existing ? existing->metadata.version + 1 : 1;
    config.metadata.updated_by = user_id;
    config.metadata.last_updated = format_timestamp(std::chrono::system_clock::now());
    config.metadata.created_by = existing ? existing->metadata.created_by : std::optional<std::string>(user_id);
    config.created_by = config.metadata.created_by;
    config.is_encrypted = is_encrypted;
    config.updated_by = user_id;
    config.created_at = existing ? existing->created_at : std::chrono::system_clock::now();
    config.updated_at = std::chrono::system_clock::now();
    config.metadata.created_at = config.created_at;
    config.metadata.updated_at = config.updated_at;

    const ConfigValue* previous_ptr = existing ? &(*existing) : nullptr;
    return persist_config(config, previous_ptr, validation_rules, user_id, reason);
}

bool DynamicConfigManager::update_config(const ConfigValue& config,
                                         const std::string& user_id,
                                         const std::string& reason) {
    auto previous = get_config(config.key, config.metadata.scope);
    const ConfigValue* previous_ptr = previous ? &(*previous) : nullptr;
    return persist_config(config, previous_ptr, config.metadata.validation_rules, user_id, reason);
}

bool DynamicConfigManager::delete_config(const std::string& key,
                                         ConfigScope scope,
                                         const std::string& user_id) {
    auto existing = get_config(key, scope);
    if (!existing) {
        return true;
    }

    const std::string query = R"(
        DELETE FROM system_configuration
        WHERE config_key = $1
          AND COALESCE((validation_rules -> 'metadata' ->> 'scope'), 'GLOBAL') = $2
    )";

    bool success = db_conn_->execute_command(query, {key, scope_to_string(scope)});
    if (!success) {
        logger_->error(fmt::format("Failed to delete configuration {}", key), kComponent, __func__);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        config_cache_.erase(make_cache_key(key, scope));
        validation_cache_.erase(key);
    }

    ConfigValue tombstone = *existing;
    tombstone.value = nullptr;
    tombstone.metadata.version += 1;
    tombstone.updated_at = std::chrono::system_clock::now();
    tombstone.metadata.updated_at = tombstone.updated_at;
    tombstone.metadata.last_updated = format_timestamp(tombstone.updated_at);
    tombstone.metadata.updated_by = user_id;
    tombstone.updated_by = user_id;
    record_history(*existing, tombstone, user_id, "deleted", "api");
    notify_listeners(tombstone);
    return true;
}

std::vector<ConfigValue> DynamicConfigManager::get_configs_by_scope(ConfigScope scope) {
    const std::string query = R"(
        SELECT
            config_key,
            config_value::text AS config_value,
            config_type,
            description,
            is_sensitive,
            COALESCE(validation_rules::text, '{}') AS validation_rules,
            requires_restart,
            COALESCE(updated_by::text, '') AS updated_by,
            created_at::text AS created_at,
            updated_at::text AS updated_at
        FROM system_configuration
        WHERE COALESCE((validation_rules -> 'metadata' ->> 'scope'), 'GLOBAL') = $1
    )";

    auto configs = fetch_configs_with_query(query, {scope_to_string(scope)});
    for (auto& config : configs) {
        config.metadata.scope = scope;
        store_in_cache(config);
    }
    return configs;
}

std::vector<ConfigValue> DynamicConfigManager::get_configs_by_module(const std::string& module_name) {
    const std::string query = R"(
        SELECT
            config_key,
            config_value::text AS config_value,
            config_type,
            description,
            is_sensitive,
            COALESCE(validation_rules::text, '{}') AS validation_rules,
            requires_restart,
            COALESCE(updated_by::text, '') AS updated_by,
            created_at::text AS created_at,
            updated_at::text AS updated_at
        FROM system_configuration
        WHERE COALESCE((validation_rules -> 'metadata' ->> 'module_name'), '') = $1
    )";

    auto configs = fetch_configs_with_query(query, {module_name});
    for (auto& config : configs) {
        store_in_cache(config);
    }
    return configs;
}

std::vector<ConfigChangeLog> DynamicConfigManager::get_config_history(
    const std::string& key,
    std::optional<std::chrono::system_clock::time_point> since,
    int limit) {
    std::string query = R"(
        SELECT
            history_id,
            config_key,
            old_value::text AS old_value,
            new_value::text AS new_value,
            changed_by::text AS changed_by,
            change_reason,
            change_source,
            COALESCE(metadata->>'scope', 'GLOBAL') AS scope,
            COALESCE(metadata->>'version', '1') AS version,
            changed_at::text AS changed_at
        FROM configuration_history
        WHERE config_key = $1
    )";

    std::vector<std::string> params{key};
    if (since) {
        query += " AND changed_at >= $2";
        params.push_back(format_timestamp(*since));
    }
    query += " ORDER BY changed_at DESC LIMIT " + std::to_string(std::max(1, limit));

    std::vector<ConfigChangeLog> history;
    auto rows = db_conn_->execute_query_multi(query, params);
    history.reserve(rows.size());
    for (const auto& row : rows) {
        history.push_back(hydrate_change_log(row));
    }
    return history;
}

bool DynamicConfigManager::register_config_schema(const std::string& key,
                                                  ConfigDataType data_type,
                                                  const nlohmann::json& validation_rules,
                                                  const std::string& description,
                                                  ConfigScope scope,
                                                  const std::string& module_name,
                                                  const std::string& user_id) {
    auto existing = get_config(key, scope);
    ConfigValue schema_entry;
    schema_entry.key = key;
    schema_entry.value = existing ? existing->value : nlohmann::json::object();
    schema_entry.metadata.scope = scope;
    schema_entry.metadata.module_name = module_name;
    schema_entry.metadata.description = description;
    schema_entry.metadata.is_sensitive = existing ? existing->metadata.is_sensitive : false;
    schema_entry.metadata.requires_restart = existing ? existing->metadata.requires_restart : false;
    schema_entry.metadata.tags = existing ? existing->metadata.tags : std::vector<std::string>{};
    schema_entry.metadata.validation_rules = validation_rules;
    schema_entry.metadata.data_type = data_type;
    schema_entry.metadata.version = existing ? existing->metadata.version + 1 : 1;
    schema_entry.metadata.created_by = existing ? existing->metadata.created_by : std::optional<std::string>(user_id);
    schema_entry.created_by = schema_entry.metadata.created_by;
    schema_entry.metadata.updated_by = user_id;
    schema_entry.updated_by = user_id;
    schema_entry.created_at = existing ? existing->created_at : std::chrono::system_clock::now();
    schema_entry.updated_at = std::chrono::system_clock::now();
    schema_entry.metadata.created_at = schema_entry.created_at;
    schema_entry.metadata.updated_at = schema_entry.updated_at;
    schema_entry.metadata.last_updated = format_timestamp(schema_entry.updated_at);

    const ConfigValue* previous_ptr = existing ? &(*existing) : nullptr;
    return persist_config(schema_entry, previous_ptr, validation_rules, user_id, "schema update");
}

ConfigValidationResult DynamicConfigManager::validate_config_value(
    const std::string& key,
    const nlohmann::json& value,
    std::optional<ConfigDataType> override_type) const {
    ConfigValidationResult result;
    result.normalized_value = value;

    ValidationContext ctx;
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = validation_cache_.find(key);
        if (it != validation_cache_.end()) {
            ctx = it->second;
        }
    }

    if (override_type) {
        ctx.data_type = *override_type;
    }

    switch (ctx.data_type) {
        case ConfigDataType::STRING:
            if (!value.is_string()) {
                result.is_valid = false;
                result.errors.push_back("Value must be a string");
            }
            break;
        case ConfigDataType::INTEGER:
            if (!value.is_number_integer()) {
                result.is_valid = false;
                result.errors.push_back("Value must be an integer");
            }
            break;
        case ConfigDataType::FLOAT:
            if (!value.is_number()) {
                result.is_valid = false;
                result.errors.push_back("Value must be numeric");
            }
            break;
        case ConfigDataType::BOOLEAN:
            if (!value.is_boolean()) {
                result.is_valid = false;
                result.errors.push_back("Value must be boolean");
            }
            break;
        case ConfigDataType::SECRET:
        case ConfigDataType::JSON:
        default:
            break;
    }

    if (!result.is_valid) {
        return result;
    }

    if (ctx.min_numeric && value.is_number()) {
        if (value.get<double>() < *ctx.min_numeric) {
            result.is_valid = false;
            result.errors.push_back("Value below minimum threshold");
        }
    }

    if (ctx.max_numeric && value.is_number()) {
        if (value.get<double>() > *ctx.max_numeric) {
            result.is_valid = false;
            result.errors.push_back("Value exceeds maximum threshold");
        }
    }

    if (value.is_string()) {
        const auto length = value.get<std::string>().size();
        if (ctx.min_length && length < *ctx.min_length) {
            result.is_valid = false;
            result.errors.push_back("String shorter than allowed minimum");
        }
        if (ctx.max_length && length > *ctx.max_length) {
            result.is_valid = false;
            result.errors.push_back("String longer than allowed maximum");
        }
        if (ctx.regex_pattern) {
            try {
                std::regex pattern(*ctx.regex_pattern);
                if (!std::regex_match(value.get<std::string>(), pattern)) {
                    result.is_valid = false;
                    result.errors.push_back("String does not match required pattern");
                }
            } catch (const std::exception& e) {
                result.warnings.push_back(std::string("Invalid validation regex: ") + e.what());
            }
        }
    }

    if (!ctx.allowed_values.empty()) {
        if (!value.is_string() || ctx.allowed_values.find(value.get<std::string>()) == ctx.allowed_values.end()) {
            result.is_valid = false;
            result.errors.push_back("Value not in allowed set");
        }
    }

    return result;
}

void DynamicConfigManager::reload_configs() {
    const std::string query = R"(
        SELECT
            config_key,
            config_value::text AS config_value,
            config_type,
            description,
            is_sensitive,
            COALESCE(validation_rules::text, '{}') AS validation_rules,
            requires_restart,
            COALESCE(updated_by::text, '') AS updated_by,
            created_at::text AS created_at,
            updated_at::text AS updated_at
        FROM system_configuration
    )";

    auto configs = fetch_configs_with_query(query, {});
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        config_cache_.clear();
        validation_cache_.clear();
        for (auto& config : configs) {
            config_cache_[make_cache_key(config.key, config.metadata.scope)] = config;
            validation_cache_[config.key] = build_validation_context(config.metadata.validation_rules,
                                                                     config.metadata.data_type);
        }
    }
}

nlohmann::json DynamicConfigManager::get_config_usage_stats() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    nlohmann::json stats = nlohmann::json::object();
    std::unordered_map<std::string, int> scope_counts;
    std::unordered_map<std::string, int> type_counts;
    int sensitive = 0;

    for (const auto& [key, config] : config_cache_) {
        scope_counts[scope_to_string(config.metadata.scope)]++;
        type_counts[data_type_to_string(config.metadata.data_type)]++;
        if (config.metadata.is_sensitive) {
            ++sensitive;
        }
    }

    stats["total_configs"] = static_cast<int>(config_cache_.size());
    stats["scopes"] = scope_counts;
    stats["types"] = type_counts;
    stats["sensitive_configs"] = sensitive;
    return stats;
}

std::vector<std::pair<std::string, int>> DynamicConfigManager::get_most_changed_configs(int limit) {
    const std::string query = R"(
        SELECT config_key, COUNT(*) AS change_count
        FROM configuration_history
        GROUP BY config_key
        ORDER BY change_count DESC
        LIMIT $1
    )";

    auto rows = db_conn_->execute_query(query, {std::to_string(std::max(1, limit))});
    std::vector<std::pair<std::string, int>> results;
    results.reserve(rows.rows.size());
    for (const auto& row : rows.rows) {
        const auto key_it = row.find("config_key");
        const auto count_it = row.find("change_count");
        if (key_it != row.end() && count_it != row.end()) {
            results.emplace_back(key_it->second, std::stoi(count_it->second));
        }
    }
    return results;
}

void DynamicConfigManager::register_change_listener(std::function<void(const ConfigValue&)> listener) {
    std::lock_guard<std::mutex> lock(listener_mutex_);
    change_listeners_.push_back(std::move(listener));
}

void DynamicConfigManager::notify_listeners(const ConfigValue& value) {
    std::lock_guard<std::mutex> lock(listener_mutex_);
    for (const auto& listener : change_listeners_) {
        try {
            listener(value);
        } catch (const std::exception& e) {
            logger_->warn(fmt::format("Change listener failed: {}", e.what()), kComponent, __func__);
        }
    }
}

void DynamicConfigManager::record_history(const ConfigValue& previous,
                                          const ConfigValue& current,
                                          const std::string& user_id,
                                          const std::string& reason,
                                          const std::string& source) {
    const std::string query = R"(
        INSERT INTO configuration_history (
            config_key,
            old_value,
            new_value,
            changed_by,
            change_reason,
            change_source,
            metadata
        ) VALUES ($1, $2::jsonb, $3::jsonb, $4::uuid, $5, $6, $7::jsonb)
    )";

    nlohmann::json metadata = {
        {"scope", scope_to_string(previous.metadata.scope)},
        {"version", current.metadata.version},
        {"module_name", previous.metadata.module_name}
    };

    db_conn_->execute_command(query, {
        previous.key,
        previous.value.dump(),
        current.value.dump(),
        user_id,
        reason,
        source,
        metadata.dump()
    });
}

bool DynamicConfigManager::update_configuration(const ConfigUpdateRequest& request) {
    return set_config(request.key,
                      request.value,
                      request.scope,
                      request.module_name,
                      request.user_id,
                      request.reason,
                      request.is_encrypted,
                      request.requires_restart,
                      request.description,
                      request.tags,
                      request.validation_rules,
                      request.data_type_override);
}

std::optional<nlohmann::json> DynamicConfigManager::get_configuration(const std::string& key) {
    auto config = get_config(key, ConfigScope::GLOBAL);
    if (!config) {
        return std::nullopt;
    }
    return config->value;
}

std::unordered_map<std::string, nlohmann::json> DynamicConfigManager::get_all_configurations() {
    std::unordered_map<std::string, nlohmann::json> configs;
    auto all_configs = get_configs_by_scope(ConfigScope::GLOBAL);
    for (const auto& config : all_configs) {
        configs[config.key] = config.value;
    }
    return configs;
}

std::optional<ConfigMetadata> DynamicConfigManager::get_configuration_metadata(const std::string& key) {
    auto config = get_config(key, ConfigScope::GLOBAL);
    if (!config) {
        return std::nullopt;
    }
    return config->metadata;
}

ConfigValidationResult DynamicConfigManager::validate_configuration(const std::string& key,
                                                                    const nlohmann::json& value) {
    return validate_config_value(key, value, std::nullopt);
}

std::vector<ConfigHistoryEntry> DynamicConfigManager::get_configuration_history_legacy(const std::string& key,
                                                                                       int limit) {
    auto history = get_config_history(key, std::nullopt, limit);
    std::vector<ConfigHistoryEntry> legacy;
    legacy.reserve(history.size());
    for (const auto& entry : history) {
        ConfigHistoryEntry legacy_entry;
        legacy_entry.history_id = entry.change_id;
        legacy_entry.config_key = entry.key;
        legacy_entry.old_value = entry.old_value;
        legacy_entry.new_value = entry.new_value;
        legacy_entry.changed_by = entry.changed_by;
        legacy_entry.change_reason = entry.change_reason;
        legacy_entry.change_source = entry.change_source;
        legacy_entry.changed_at = format_timestamp(entry.changed_at);
        legacy.push_back(std::move(legacy_entry));
    }
    return legacy;
}

bool DynamicConfigManager::rollback_configuration(const std::string& history_id,
                                                  const std::string& user_id,
                                                  const std::string& reason) {
    const std::string query = R"(
        SELECT
            config_key,
            COALESCE(old_value::text, 'null') AS old_value,
            COALESCE(metadata->>'scope', 'GLOBAL') AS scope
        FROM configuration_history
        WHERE history_id = $1
    )";

    auto row = db_conn_->execute_query_single(query, {history_id});
    if (!row) {
        logger_->warn(fmt::format("History entry {} not found for rollback", history_id), kComponent, __func__);
        return false;
    }

    const std::string key = row->value("config_key", "");
    const ConfigScope scope = parse_scope(row->value("scope", "GLOBAL"));
    nlohmann::json value = safe_parse_json(row->value("old_value", "null"), nullptr);
    return set_config(key,
                      value,
                      scope,
                      "rollback",
                      user_id,
                      reason,
                      false,
                      false,
                      "Rolled back configuration",
                      {},
                      nlohmann::json::object());
}

bool DynamicConfigManager::delete_configuration(const std::string& key,
                                                const std::string& user_id,
                                                const std::string& reason) {
    return delete_config(key, ConfigScope::GLOBAL, user_id);
}

bool DynamicConfigManager::has_update_permission(const std::string& key, const std::string& user_id) {
    (void)key;
    return !user_id.empty();
}

bool DynamicConfigManager::refresh_cache() {
    reload_configs();
    return true;
}

std::vector<std::string> DynamicConfigManager::get_restart_required_configs() {
    std::vector<std::string> keys;
    std::lock_guard<std::mutex> lock(cache_mutex_);
    for (const auto& [cache_key, config] : config_cache_) {
        if (config.metadata.requires_restart) {
            keys.push_back(config.key);
        }
    }
    return keys;
}

} // namespace regulens
