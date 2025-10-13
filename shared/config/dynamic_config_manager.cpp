#include "dynamic_config_manager.hpp"
#include "../logging/logging.hpp"
#include <iostream>
#include <sstream>
#include <regex>
#include <algorithm>

namespace regulens {

DynamicConfigManager::DynamicConfigManager(PGconn* db_conn, std::shared_ptr<Logger> logger)
    : db_conn_(db_conn), logger_(logger) {

    if (!logger_) {
        logger_ = std::make_shared<Logger>("DynamicConfigManager");
    }
}

bool DynamicConfigManager::initialize() {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    logger_->info("Initializing DynamicConfigManager");

    if (!load_from_database()) {
        logger_->error("Failed to load configuration from database");
        return false;
    }

    logger_->info("DynamicConfigManager initialized successfully with {} configurations",
                  config_cache_.size());
    return true;
}

bool DynamicConfigManager::update_configuration(const ConfigUpdateRequest& request) {
    // Check permissions
    if (!has_update_permission(request.key, request.user_id)) {
        logger_->warn("User {} does not have permission to update configuration {}", request.user_id, request.key);
        return false;
    }

    // Validate the new value
    auto validation = validate_configuration(request.key, request.value);
    if (!validation.valid) {
        logger_->warn("Configuration validation failed for key {}: {}", request.key, validation.error_message);
            return false;
        }

    // Use validated/suggested value if different
    nlohmann::json final_value = validation.suggested_value.is_null() ? request.value : validation.suggested_value;

    std::lock_guard<std::mutex> lock(db_mutex_);

    // Get current value for history
    auto current_value = get_configuration(request.key);

    // Create metadata if this is a new configuration
    ConfigMetadata metadata;
    auto existing_metadata = get_configuration_metadata(request.key);
    if (existing_metadata) {
        metadata = *existing_metadata;
        } else {
        // Default metadata for new configurations
        metadata.config_type = "string"; // Will be inferred from value
        metadata.description = "Auto-created configuration";
        metadata.is_sensitive = false;
        metadata.validation_rules = nullptr;
        metadata.requires_restart = false;
        metadata.last_updated = "";
        metadata.updated_by = request.user_id;
    }

    // Infer type from value
    if (final_value.is_string()) {
        metadata.config_type = "string";
    } else if (final_value.is_number_integer()) {
        metadata.config_type = "integer";
    } else if (final_value.is_number_float()) {
        metadata.config_type = "float";
    } else if (final_value.is_boolean()) {
        metadata.config_type = "boolean";
    } else {
        metadata.config_type = "json";
    }

    // Normalize the value
    final_value = normalize_value(final_value, metadata.config_type);

    // Save to database
    if (!save_to_database(request.key, final_value, metadata, request.user_id)) {
        logger_->error("Failed to save configuration {} to database", request.key);
        return false;
    }

    // Save to history
    if (!save_history(request.key, current_value.value_or(nullptr), final_value,
                     request.user_id, request.reason, request.source)) {
        logger_->warn("Failed to save configuration history for {}", request.key);
        // Don't fail the update for history save failure
    }

    // Update cache
    {
        std::lock_guard<std::mutex> cache_lock(cache_mutex_);
        config_cache_[request.key] = final_value;
        metadata_cache_[request.key] = metadata;
    }

    // Notify listeners
    notify_config_change(request.key, final_value);

    logger_->info("Configuration {} updated successfully by user {}", request.key, request.user_id);
    return true;
}

std::optional<nlohmann::json> DynamicConfigManager::get_configuration(const std::string& key) {
            std::lock_guard<std::mutex> lock(cache_mutex_);

    auto it = config_cache_.find(key);
            if (it != config_cache_.end()) {
                return it->second;
            }

    return std::nullopt;
        }

std::unordered_map<std::string, nlohmann::json> DynamicConfigManager::get_all_configurations() {
            std::lock_guard<std::mutex> lock(cache_mutex_);
    return config_cache_;
}

std::optional<ConfigMetadata> DynamicConfigManager::get_configuration_metadata(const std::string& key) {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    auto it = metadata_cache_.find(key);
    if (it != metadata_cache_.end()) {
        return it->second;
    }

    return std::nullopt;
}

ConfigValidationResult DynamicConfigManager::validate_configuration(const std::string& key, const nlohmann::json& value) {
    // Get metadata for validation rules
    auto metadata = get_configuration_metadata(key);
    if (!metadata || metadata->validation_rules.is_null()) {
        // No validation rules, accept the value
        return ConfigValidationResult(true);
    }

    const auto& rules = metadata->validation_rules;

    // Type validation
    if (rules.contains("type")) {
        std::string expected_type = rules["type"];
        bool type_valid = false;

        if (expected_type == "string" && value.is_string()) type_valid = true;
        else if (expected_type == "integer" && value.is_number_integer()) type_valid = true;
        else if (expected_type == "float" && (value.is_number_float() || value.is_number_integer())) type_valid = true;
        else if (expected_type == "boolean" && value.is_boolean()) type_valid = true;
        else if (expected_type == "json") type_valid = true;

        if (!type_valid) {
            return ConfigValidationResult(false, "Value type does not match expected type: " + expected_type);
        }
    }

    // Range validation for numbers
    if (value.is_number()) {
        if (rules.contains("min") && value < rules["min"]) {
            nlohmann::json suggested = rules["min"];
            return ConfigValidationResult(false, "Value below minimum allowed", suggested);
        }
        if (rules.contains("max") && value > rules["max"]) {
            nlohmann::json suggested = rules["max"];
            return ConfigValidationResult(false, "Value above maximum allowed", suggested);
        }
    }

    // String validation
    if (value.is_string()) {
        std::string str_value = value;

        if (rules.contains("min_length") && static_cast<int>(str_value.length()) < rules["min_length"]) {
            return ConfigValidationResult(false, "String length below minimum");
        }
        if (rules.contains("max_length") && static_cast<int>(str_value.length()) > rules["max_length"]) {
            return ConfigValidationResult(false, "String length above maximum");
        }

        if (rules.contains("pattern")) {
            std::regex pattern(rules["pattern"]);
            if (!std::regex_match(str_value, pattern)) {
                return ConfigValidationResult(false, "String does not match required pattern");
            }
        }

        if (rules.contains("allowed_values")) {
            bool allowed = false;
            for (const auto& allowed_value : rules["allowed_values"]) {
                if (allowed_value == str_value) {
                    allowed = true;
                    break;
                }
            }
            if (!allowed) {
                return ConfigValidationResult(false, "Value not in allowed values list");
            }
        }
    }

    return ConfigValidationResult(true);
}

std::vector<ConfigHistoryEntry> DynamicConfigManager::get_configuration_history(const std::string& key, int limit) {
    std::vector<ConfigHistoryEntry> history;

    std::string query = R"(
        SELECT history_id, config_key, old_value, new_value, changed_by,
               changed_at, change_reason, change_source
        FROM configuration_history
        WHERE config_key = $1
        ORDER BY changed_at DESC
        LIMIT $2
    )";

    const char* param_values[2] = { key.c_str(), std::to_string(limit).c_str() };
    int param_lengths[2] = { static_cast<int>(key.length()), 0 };
    int param_formats[2] = { 0, 0 };

    PGresult* result = execute_query(query, param_values, param_lengths, param_formats, 2);
    if (!result) {
        return history;
    }

    int rows = PQntuples(result);
    for (int i = 0; i < rows; ++i) {
        ConfigHistoryEntry entry;
        entry.history_id = PQgetvalue(result, i, 0);
        entry.config_key = PQgetvalue(result, i, 1);
        entry.old_value = PQgetvalue(result, i, 2) ? nlohmann::json::parse(PQgetvalue(result, i, 2)) : nullptr;
        entry.new_value = PQgetvalue(result, i, 3) ? nlohmann::json::parse(PQgetvalue(result, i, 3)) : nullptr;
        entry.changed_by = PQgetvalue(result, i, 4);
        entry.changed_at = PQgetvalue(result, i, 5);
        entry.change_reason = PQgetvalue(result, i, 6) ? PQgetvalue(result, i, 6) : "";
        entry.change_source = PQgetvalue(result, i, 7) ? PQgetvalue(result, i, 7) : "";

        history.push_back(entry);
    }

    PQclear(result);
    return history;
}

bool DynamicConfigManager::rollback_configuration(const std::string& history_id, const std::string& user_id, const std::string& reason) {
    // Get the history entry
    std::string query = R"(
        SELECT config_key, old_value, new_value
        FROM configuration_history
        WHERE history_id = $1
    )";

    const char* param_values[1] = { history_id.c_str() };
    int param_lengths[1] = { static_cast<int>(history_id.length()) };
    int param_formats[1] = { 0 };

    PGresult* result = execute_query(query, param_values, param_lengths, param_formats, 1);
    if (!result || PQntuples(result) == 0) {
        if (result) PQclear(result);
        logger_->error("History entry {} not found", history_id);
        return false;
    }

    std::string config_key = PQgetvalue(result, 0, 0);
    nlohmann::json rollback_value;

    // Check if we have an old value to rollback to
    if (PQgetvalue(result, 0, 1) && std::string(PQgetvalue(result, 0, 1)) != "null") {
        rollback_value = nlohmann::json::parse(PQgetvalue(result, 0, 1));
    } else {
        // If no old value, this means we're rolling back the initial creation, so delete
        PQclear(result);
        return delete_configuration(config_key, user_id, "Rollback to before initial creation: " + reason);
    }

    PQclear(result);

    // Perform the rollback as a regular update
    ConfigUpdateRequest rollback_request{
        config_key,
        rollback_value,
        user_id,
        "Rollback: " + reason,
        "api"
    };

    return update_configuration(rollback_request);
}

bool DynamicConfigManager::delete_configuration(const std::string& key, const std::string& user_id, const std::string& reason) {
    // Check permissions
    if (!has_update_permission(key, user_id)) {
        logger_->warn("User {} does not have permission to delete configuration {}", user_id, key);
        return false;
    }

    std::lock_guard<std::mutex> lock(db_mutex_);

    // Get current value for history
    auto current_value = get_configuration(key);
    if (!current_value) {
        return true; // Already deleted
    }

    // Save to history before deletion
    save_history(key, *current_value, nullptr, user_id, reason, "api");

    // Delete from database
    std::string query = "DELETE FROM system_configuration WHERE config_key = $1";

    const char* param_values[1] = { key.c_str() };
    int param_lengths[1] = { static_cast<int>(key.length()) };
    int param_formats[1] = { 0 };

    PGresult* result = execute_query(query, param_values, param_lengths, param_formats, 1);
    if (!result) {
        return false;
    }

    bool success = (PQresultStatus(result) == PGRES_COMMAND_OK);
    PQclear(result);

    if (success) {
        // Update cache
        std::lock_guard<std::mutex> cache_lock(cache_mutex_);
        config_cache_.erase(key);
        metadata_cache_.erase(key);

        // Notify listeners with null value to indicate deletion
        notify_config_change(key, nullptr);

        logger_->info("Configuration {} deleted successfully by user {}", key, user_id);
    }

    return success;
}

bool DynamicConfigManager::has_update_permission(const std::string& key, const std::string& user_id) {
    // For now, allow all authenticated users to update configurations
    // In a production system, this would check user roles and permissions
    // TODO: Implement proper role-based access control
    return !user_id.empty();
}

bool DynamicConfigManager::refresh_cache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    return load_from_database();
}

std::vector<std::string> DynamicConfigManager::get_restart_required_configs() {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    std::vector<std::string> restart_required;
    for (const auto& [key, metadata] : metadata_cache_) {
        if (metadata.requires_restart) {
            restart_required.push_back(key);
        }
    }

    return restart_required;
}

void DynamicConfigManager::notify_config_change(const std::string& key, const nlohmann::json& value) {
    for (const auto& listener : change_listeners_) {
        try {
            listener(key, value);
    } catch (const std::exception& e) {
            logger_->error("Configuration change listener threw exception: {}", e.what());
        }
    }
}

void DynamicConfigManager::register_change_listener(std::function<void(const std::string&, const nlohmann::json&)> listener) {
    change_listeners_.push_back(listener);
}

bool DynamicConfigManager::load_from_database() {
        std::string query = R"(
        SELECT config_key, config_value, config_type, description, is_sensitive,
               validation_rules, last_updated, updated_by, requires_restart
        FROM system_configuration
        ORDER BY config_key
    )";

    PGresult* result = execute_query(query);
    if (!result) {
        return false;
    }

    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        logger_->error("Failed to load configuration from database: {}", PQresultErrorMessage(result));
        PQclear(result);
        return false;
    }

    int rows = PQntuples(result);
    config_cache_.clear();
    metadata_cache_.clear();

    for (int i = 0; i < rows; ++i) {
        std::string key = PQgetvalue(result, i, 0);
        std::string value_str = PQgetvalue(result, i, 1);

        try {
            nlohmann::json value = nlohmann::json::parse(value_str);
            config_cache_[key] = value;

            ConfigMetadata metadata;
            metadata.config_type = PQgetvalue(result, i, 2);
            metadata.description = PQgetvalue(result, i, 3) ? PQgetvalue(result, i, 3) : "";
            metadata.is_sensitive = (std::string(PQgetvalue(result, i, 4)) == "t");
            metadata.validation_rules = PQgetvalue(result, i, 5) ?
                nlohmann::json::parse(PQgetvalue(result, i, 5)) : nullptr;
            metadata.last_updated = PQgetvalue(result, i, 6) ? PQgetvalue(result, i, 6) : "";
            metadata.updated_by = PQgetvalue(result, i, 7) ? PQgetvalue(result, i, 7) : "";
            metadata.requires_restart = (std::string(PQgetvalue(result, i, 8)) == "t");

            metadata_cache_[key] = metadata;
    } catch (const std::exception& e) {
            logger_->warn("Failed to parse configuration value for key {}: {}", key, e.what());
        }
    }

    PQclear(result);
    return true;
}

bool DynamicConfigManager::save_to_database(const std::string& key, const nlohmann::json& value,
                                          const ConfigMetadata& metadata, const std::string& user_id) {
    std::string query = R"(
        INSERT INTO system_configuration (
            config_key, config_value, config_type, description, is_sensitive,
            validation_rules, updated_by, requires_restart
        ) VALUES ($1, $2, $3, $4, $5, $6, $7, $8)
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

    std::string value_str = value.dump();
    std::string validation_rules_str = metadata.validation_rules.is_null() ? "null" :
        metadata.validation_rules.dump();

    const char* param_values[8] = {
        key.c_str(),
        value_str.c_str(),
        metadata.config_type.c_str(),
        metadata.description.c_str(),
        metadata.is_sensitive ? "true" : "false",
        validation_rules_str.c_str(),
        user_id.c_str(),
        metadata.requires_restart ? "true" : "false"
    };

    int param_lengths[8] = {
        static_cast<int>(key.length()),
        static_cast<int>(value_str.length()),
        static_cast<int>(metadata.config_type.length()),
        static_cast<int>(metadata.description.length()),
        0, 0, 0, 0
    };

    int param_formats[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    PGresult* result = execute_query(query, param_values, param_lengths, param_formats, 8);
    if (!result) {
        return false;
    }

    bool success = (PQresultStatus(result) == PGRES_COMMAND_OK);
    PQclear(result);

    return success;
}

bool DynamicConfigManager::save_history(const std::string& key, const nlohmann::json& old_value,
                                       const nlohmann::json& new_value, const std::string& user_id,
                                       const std::string& reason, const std::string& source) {
        std::string query = R"(
        INSERT INTO configuration_history (
            config_key, old_value, new_value, changed_by, change_reason, change_source
        ) VALUES ($1, $2, $3, $4, $5, $6)
    )";

    std::string old_value_str = old_value.is_null() ? "null" : old_value.dump();
    std::string new_value_str = new_value.is_null() ? "null" : new_value.dump();

    const char* param_values[6] = {
        key.c_str(),
        old_value_str.c_str(),
        new_value_str.c_str(),
        user_id.c_str(),
        reason.c_str(),
        source.c_str()
    };

    int param_lengths[6] = {
        static_cast<int>(key.length()),
        static_cast<int>(old_value_str.length()),
        static_cast<int>(new_value_str.length()),
        static_cast<int>(user_id.length()),
        static_cast<int>(reason.length()),
        static_cast<int>(source.length())
    };

    int param_formats[6] = { 0, 0, 0, 0, 0, 0 };

    PGresult* result = execute_query(query, param_values, param_lengths, param_formats, 6);
    if (!result) {
        return false;
    }

    bool success = (PQresultStatus(result) == PGRES_COMMAND_OK);
    PQclear(result);

    return success;
}

PGresult* DynamicConfigManager::execute_query(const std::string& query, const char* const* param_values,
                                             const int* param_lengths, const int* param_formats, int n_params) {
    PGresult* result = PQexecParams(db_conn_, query.c_str(), n_params, nullptr,
                                   param_values, param_lengths, param_formats, 0);

    if (!result) {
        logger_->error("Database query failed: connection lost");
        return nullptr;
    }

    if (PQresultStatus(result) == PGRES_FATAL_ERROR) {
        logger_->error("Database query failed: {}", PQresultErrorMessage(result));
        PQclear(result);
        return nullptr;
    }

    return result;
}

nlohmann::json DynamicConfigManager::normalize_value(const nlohmann::json& value, const std::string& type) {
    if (type == "string") {
        return value.is_string() ? value : nlohmann::json(value.dump());
    } else if (type == "integer") {
        return value.is_number_integer() ? value : nlohmann::json(static_cast<int>(value));
    } else if (type == "float") {
        return value.is_number_float() ? value : nlohmann::json(static_cast<double>(value));
    } else if (type == "boolean") {
        return value.is_boolean() ? value : nlohmann::json(!value.is_null() && value != 0 && value != "false");
    }

    // For json type or unknown types, return as-is
    return value;
}

} // namespace regulens