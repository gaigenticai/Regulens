/**
 * Dynamic Configuration API Handlers Implementation
 * REST API endpoints for database-driven configuration management
 */

#include "dynamic_config_api_handlers.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>

namespace regulens {

DynamicConfigAPIHandlers::DynamicConfigAPIHandlers(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<DynamicConfigManager> config_manager
) : db_conn_(db_conn), config_manager_(config_manager), access_control_(db_conn) {

    if (!db_conn_) {
        throw std::runtime_error("Database connection is required for DynamicConfigAPIHandlers");
    }
    if (!config_manager_) {
        throw std::runtime_error("DynamicConfigManager is required for DynamicConfigAPIHandlers");
    }

    spdlog::info("DynamicConfigAPIHandlers initialized");
}

DynamicConfigAPIHandlers::~DynamicConfigAPIHandlers() {
    spdlog::info("DynamicConfigAPIHandlers shutting down");
}

std::string DynamicConfigAPIHandlers::handle_get_config(const std::string& key, const std::string& scope_str, const std::string& user_id) {
    try {
        ConfigScope scope = parse_scope_param(scope_str);

        if (!validate_user_access(user_id, "get_config", key)) {
            return create_error_response("Access denied", 403).dump();
        }

        if (!validate_scope_access(user_id, scope)) {
            return create_error_response("Scope access denied", 403).dump();
        }

        auto config_opt = config_manager_->get_config(key, scope);
        if (!config_opt) {
            return create_error_response("Configuration not found", 404).dump();
        }

        nlohmann::json response_data = format_config_value(*config_opt);
        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_config: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string DynamicConfigAPIHandlers::handle_set_config(const std::string& request_body, const std::string& user_id) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);
        std::string validation_error;
        if (!validate_config_request(request, validation_error)) {
            return create_error_response(validation_error).dump();
        }

        std::string key = request.value("key", "");
        std::string scope_str = request.value("scope", "GLOBAL");
        ConfigScope scope = parse_scope_param(scope_str);
        nlohmann::json value = request["value"];
        std::string module_name = request.value("module_name", "");
        std::string change_reason = request.value("change_reason", "");
        bool is_encrypted = request.value("is_encrypted", false);
        bool requires_restart = request.value("requires_restart", false);
        std::string description = request.value("description", "");

        std::vector<std::string> tags;
        if (request.contains("tags") && request["tags"].is_array()) {
            for (const auto& tag : request["tags"]) {
                if (tag.is_string()) {
                    tags.push_back(tag.get<std::string>());
                }
            }
        }

        nlohmann::json validation_rules = request.value("validation_rules", nlohmann::json::object());
        std::optional<ConfigDataType> data_type_override;
        if (request.contains("data_type") && request["data_type"].is_string()) {
            data_type_override = config_manager_->parse_data_type(request["data_type"].get<std::string>());
        }

        if (!validate_user_access(user_id, "set_config", key)) {
            return create_error_response("Access denied", 403).dump();
        }

        if (!validate_scope_access(user_id, scope)) {
            return create_error_response("Scope access denied", 403).dump();
        }

        bool success = config_manager_->set_config(key,
                                                  value,
                                                  scope,
                                                  module_name,
                                                  user_id,
                                                  change_reason,
                                                  is_encrypted,
                                                  requires_restart,
                                                  description,
                                                  tags,
                                                  validation_rules,
                                                  data_type_override);

        if (!success) {
            return create_error_response("Failed to set configuration").dump();
        }

        nlohmann::json response_data = {
            {"key", key},
            {"scope", scope_str},
            {"operation", "set"}
        };

        return create_success_response(response_data, "Configuration set successfully").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_set_config: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string DynamicConfigAPIHandlers::handle_update_config(const std::string& key, const std::string& scope_str, const std::string& request_body, const std::string& user_id) {
    try {
        ConfigScope scope = parse_scope_param(scope_str);
        nlohmann::json request = nlohmann::json::parse(request_body);

        if (!request.contains("value")) {
            return create_error_response("Missing 'value' field").dump();
        }

        if (!validate_user_access(user_id, "update_config", key)) {
            return create_error_response("Access denied", 403).dump();
        }

        if (!validate_scope_access(user_id, scope)) {
            return create_error_response("Scope access denied", 403).dump();
        }

        nlohmann::json value = request["value"];
        std::string module_name = request.value("module_name", "");
        std::string change_reason = request.value("change_reason", "Updated via API");
        bool is_encrypted = request.value("is_encrypted", false);
        bool requires_restart = request.value("requires_restart", false);
        std::string description = request.value("description", "");

        std::vector<std::string> tags;
        if (request.contains("tags") && request["tags"].is_array()) {
            for (const auto& tag : request["tags"]) {
                if (tag.is_string()) {
                    tags.push_back(tag.get<std::string>());
                }
            }
        }

        nlohmann::json validation_rules = request.value("validation_rules", nlohmann::json::object());
        std::optional<ConfigDataType> data_type_override;
        if (request.contains("data_type") && request["data_type"].is_string()) {
            data_type_override = config_manager_->parse_data_type(request["data_type"].get<std::string>());
        }

        bool success = config_manager_->set_config(key,
                                                  value,
                                                  scope,
                                                  module_name,
                                                  user_id,
                                                  change_reason,
                                                  is_encrypted,
                                                  requires_restart,
                                                  description,
                                                  tags,
                                                  validation_rules,
                                                  data_type_override);

        if (!success) {
            return create_error_response("Failed to update configuration").dump();
        }

        nlohmann::json response_data = {
            {"key", key},
            {"scope", scope_str},
            {"operation", "update"}
        };

        return create_success_response(response_data, "Configuration updated successfully").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_update_config: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string DynamicConfigAPIHandlers::handle_delete_config(const std::string& key, const std::string& scope_str, const std::string& user_id) {
    try {
        ConfigScope scope = parse_scope_param(scope_str);

        if (!validate_user_access(user_id, "delete_config", key)) {
            return create_error_response("Access denied", 403).dump();
        }

        if (!validate_scope_access(user_id, scope)) {
            return create_error_response("Scope access denied", 403).dump();
        }

        bool success = config_manager_->delete_config(key, scope, user_id);

        if (!success) {
            return create_error_response("Failed to delete configuration or configuration not found", 404).dump();
        }

        nlohmann::json response_data = {
            {"key", key},
            {"scope", scope_str},
            {"operation", "delete"}
        };

        return create_success_response(response_data, "Configuration deleted successfully").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_delete_config: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string DynamicConfigAPIHandlers::handle_get_configs_by_scope(const std::string& scope_str, const std::string& user_id) {
    try {
        ConfigScope scope = parse_scope_param(scope_str);

        if (!validate_user_access(user_id, "get_configs_by_scope")) {
            return create_error_response("Access denied", 403).dump();
        }

        if (!validate_scope_access(user_id, scope)) {
            return create_error_response("Scope access denied", 403).dump();
        }

        auto configs = config_manager_->get_configs_by_scope(scope);

        std::vector<nlohmann::json> formatted_configs;
        formatted_configs.reserve(configs.size());
        for (const auto& config : configs) {
            formatted_configs.push_back(format_config_value(config));
        }

        nlohmann::json response_data = {
            {"scope", scope_str},
            {"configs", formatted_configs},
            {"total_count", formatted_configs.size()}
        };

        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_configs_by_scope: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string DynamicConfigAPIHandlers::handle_get_configs_by_module(const std::string& module, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "get_configs_by_module")) {
            return create_error_response("Access denied", 403).dump();
        }

        auto configs = config_manager_->get_configs_by_module(module);

        std::vector<nlohmann::json> formatted_configs;
        formatted_configs.reserve(configs.size());
        for (const auto& config : configs) {
            formatted_configs.push_back(format_config_value(config));
        }

        nlohmann::json response_data = {
            {"module", module},
            {"configs", formatted_configs},
            {"total_count", formatted_configs.size()}
        };

        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_configs_by_module: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string DynamicConfigAPIHandlers::handle_register_config_schema(const std::string& request_body, const std::string& user_id) {
    try {
        if (!is_admin_user(user_id)) {
            return create_error_response("Admin access required", 403).dump();
        }

        nlohmann::json request = nlohmann::json::parse(request_body);

        if (!request.contains("key") || !request.contains("data_type") || !request.contains("description")) {
            return create_error_response("Missing required fields: key, data_type, description").dump();
        }

        std::string key = request["key"];
        std::string data_type_str = request["data_type"];
        std::string description = request["description"];
        std::string scope_str = request.value("scope", "GLOBAL");
        ConfigScope scope = parse_scope_param(scope_str);

        ConfigDataType data_type = config_manager_->parse_data_type(data_type_str);

        nlohmann::json validation_rules = nlohmann::json::object();
        if (request.contains("validation_regex") && request["validation_regex"].is_string()) {
            validation_rules["pattern"] = request["validation_regex"].get<std::string>();
        }
        if (request.contains("allowed_values") && request["allowed_values"].is_array()) {
            validation_rules["allowed_values"] = request["allowed_values"];
        }
        if (request.contains("numeric_constraints") && request["numeric_constraints"].is_object()) {
            validation_rules["numeric"] = request["numeric_constraints"];
        }
        if (request.contains("length_constraints") && request["length_constraints"].is_object()) {
            validation_rules["length"] = request["length_constraints"];
        }

        std::string module_name = request.value("module_name", "");

        bool success = config_manager_->register_config_schema(
            key,
            data_type,
            validation_rules,
            description,
            scope,
            module_name,
            user_id
        );

        if (!success) {
            return create_error_response("Failed to register configuration schema").dump();
        }

        nlohmann::json response_data = {
            {"key", key},
            {"data_type", data_type_str},
            {"scope", scope_str},
            {"operation", "register_schema"}
        };

        return create_success_response(response_data, "Configuration schema registered successfully").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_register_config_schema: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string DynamicConfigAPIHandlers::handle_validate_config_value(const std::string& request_body, const std::string& user_id) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);

        if (!request.contains("key") || !request.contains("value")) {
            return create_error_response("Missing required fields: key, value").dump();
        }

        if (!validate_user_access(user_id, "validate_config")) {
            return create_error_response("Access denied", 403).dump();
        }

        std::string key = request["key"];
        nlohmann::json value = request["value"];

        std::optional<ConfigDataType> type_override;
        if (request.contains("data_type") && request["data_type"].is_string()) {
            type_override = config_manager_->parse_data_type(request["data_type"].get<std::string>());
        }

        auto validation_result = config_manager_->validate_config_value(key, value, type_override);

        nlohmann::json response_data = format_validation_result(validation_result);
        response_data["key"] = key;

        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_validate_config_value: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string DynamicConfigAPIHandlers::handle_get_config_history(const std::string& key, const std::string& scope_str, const std::string& query_params, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "get_config_history", key)) {
            return create_error_response("Access denied", 403).dump();
        }

        auto params = parse_query_params(query_params);
        int limit = parse_int_param(params["limit"], 50);
        limit = std::min(200, std::max(1, limit)); // Cap at 200

        std::optional<std::chrono::system_clock::time_point> since;
        if (params.count("since")) {
            // Parse timestamp (simplified)
            try {
                long long timestamp = std::stoll(params["since"]);
                since = std::chrono::system_clock::time_point(std::chrono::seconds(timestamp));
            } catch (...) {
                // Invalid timestamp, ignore
            }
        }

        ConfigScope history_scope = parse_scope_param(scope_str);
        auto history = config_manager_->get_config_history(key, since, limit);

        std::vector<nlohmann::json> formatted_history;
        formatted_history.reserve(history.size());
        for (const auto& change : history) {
            if (change.scope == history_scope) {
                formatted_history.push_back(format_config_change(change));
            }
        }

        nlohmann::json response_data = create_paginated_response(
            formatted_history,
            static_cast<int>(formatted_history.size()),
            1,
            limit
        );
        response_data["key"] = key;
        response_data["scope"] = scope_str;

        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_config_history: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string DynamicConfigAPIHandlers::handle_reload_configs(const std::string& user_id) {
    try {
        if (!is_admin_user(user_id)) {
            return create_error_response("Admin access required", 403).dump();
        }

        config_manager_->reload_configs();

        return create_success_response(nullptr, "Configuration cache reloaded successfully").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_reload_configs: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string DynamicConfigAPIHandlers::handle_get_config_stats(const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "get_config_stats")) {
            return create_error_response("Access denied", 403).dump();
        }

        auto usage_stats = config_manager_->get_config_usage_stats();
        auto most_changed = config_manager_->get_most_changed_configs(10);

        nlohmann::json response_data = {
            {"usage_stats", usage_stats},
            {"most_changed_configs", most_changed}
        };

        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_config_stats: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

// Helper method implementations

ConfigScope DynamicConfigAPIHandlers::parse_scope_param(const std::string& scope_str) {
    if (scope_str == "USER") return ConfigScope::USER;
    if (scope_str == "ORGANIZATION") return ConfigScope::ORGANIZATION;
    if (scope_str == "ENVIRONMENT") return ConfigScope::ENVIRONMENT;
    if (scope_str == "MODULE") return ConfigScope::MODULE;
    return ConfigScope::GLOBAL;
}

nlohmann::json DynamicConfigAPIHandlers::format_config_value(const ConfigValue& config) {
    return {
        {"key", config.key},
        {"value", config.value},
        {"data_type", config_manager_->data_type_to_string(config.metadata.data_type)},
        {"scope", config_manager_->scope_to_string(config.metadata.scope)},
        {"module_name", config.metadata.module_name},
        {"description", config.metadata.description},
        {"is_encrypted", config.is_encrypted},
        {"version", config.metadata.version},
        {"requires_restart", config.metadata.requires_restart},
        {"tags", config.metadata.tags},
        {"updated_by", config.updated_by.value_or("")},
        {"created_at", std::chrono::duration_cast<std::chrono::seconds>(
            config.created_at.time_since_epoch()).count()},
        {"updated_at", std::chrono::duration_cast<std::chrono::seconds>(
            config.updated_at.time_since_epoch()).count()}
    };
}

nlohmann::json DynamicConfigAPIHandlers::format_config_change(const ConfigChangeLog& change) {
    return {
        {"change_id", change.change_id},
        {"config_key", change.key},
        {"old_value", change.old_value},
        {"new_value", change.new_value},
        {"changed_by", change.changed_by},
        {"change_reason", change.change_reason},
        {"change_source", change.change_source},
        {"scope", config_manager_->scope_to_string(change.scope)},
        {"version", change.version},
        {"changed_at", std::chrono::duration_cast<std::chrono::seconds>(
            change.changed_at.time_since_epoch()).count()}
    };
}

nlohmann::json DynamicConfigAPIHandlers::format_validation_result(const ConfigValidationResult& result) {
    return {
        {"is_valid", result.is_valid},
        {"errors", result.errors},
        {"warnings", result.warnings}
    };
}

std::unordered_map<std::string, std::string> DynamicConfigAPIHandlers::parse_query_params(const std::string& query_string) {
    std::unordered_map<std::string, std::string> params;

    if (query_string.empty()) return params;

    std::stringstream ss(query_string);
    std::string pair;

    while (std::getline(ss, pair, '&')) {
        size_t equals_pos = pair.find('=');
        if (equals_pos != std::string::npos) {
            std::string key = pair.substr(0, equals_pos);
            std::string value = pair.substr(equals_pos + 1);
            params[key] = value;
        }
    }

    return params;
}

int DynamicConfigAPIHandlers::parse_int_param(const std::string& value, int default_value) {
    try {
        return std::stoi(value);
    } catch (...) {
        return default_value;
    }
}

bool DynamicConfigAPIHandlers::parse_bool_param(const std::string& value, bool default_value) {
    if (value == "true" || value == "1") return true;
    if (value == "false" || value == "0") return false;
    return default_value;
}

bool DynamicConfigAPIHandlers::validate_config_request(const nlohmann::json& request, std::string& error_message) {
    if (!request.contains("key")) {
        error_message = "Missing 'key' field";
        return false;
    }

    if (!request.contains("value")) {
        error_message = "Missing 'value' field";
        return false;
    }

    std::string key = request["key"];
    if (key.empty()) {
        error_message = "Key cannot be empty";
        return false;
    }

    if (key.length() > 255) {
        error_message = "Key too long (maximum 255 characters)";
        return false;
    }

    return true;
}

bool DynamicConfigAPIHandlers::validate_user_access(const std::string& user_id, const std::string& operation, const std::string& key) {
    if (user_id.empty() || operation.empty()) {
        return false;
    }

    if (access_control_.is_admin(user_id)) {
        return true;
    }

    std::vector<AccessControlService::PermissionQuery> queries = {
        {operation, "dynamic_config", key, 0},
        {operation, "configuration", key, 0},
        {operation, "dynamic_config", "*", 0},
        {operation, "configuration", "*", 0},
        {"manage_dynamic_config", "", "", 0},
        {"manage_configurations", "", "", 0},
        {operation, "", "", 0}
    };

    if (!key.empty()) {
        queries.push_back({"manage_dynamic_config", "dynamic_config", key, 0});
    }

    return access_control_.has_any_permission(user_id, queries);
}

bool DynamicConfigAPIHandlers::validate_scope_access(const std::string& user_id, ConfigScope scope) {
    if (user_id.empty()) {
        return false;
    }

    if (access_control_.is_admin(user_id)) {
        return true;
    }

    const std::string scope_name = config_manager_->scope_to_string(scope);
    if (scope_name.empty()) {
        return false;
    }

    if (access_control_.has_scope_access(user_id, scope_name)) {
        return true;
    }

    return access_control_.has_scope_access(user_id, "*");
}

nlohmann::json DynamicConfigAPIHandlers::create_success_response(const nlohmann::json& data, const std::string& message) {
    nlohmann::json response = {
        {"success", true},
        {"status_code", 200}
    };

    if (!message.empty()) {
        response["message"] = message;
    }

    if (data.is_object() || data.is_array()) {
        response["data"] = data;
    }

    return response;
}

nlohmann::json DynamicConfigAPIHandlers::create_error_response(const std::string& message, int status_code) {
    return {
        {"success", false},
        {"status_code", status_code},
        {"error", message}
    };
}

nlohmann::json DynamicConfigAPIHandlers::create_paginated_response(
    const std::vector<nlohmann::json>& items,
    int total_count,
    int page,
    int page_size
) {
    int total_pages = (total_count + page_size - 1) / page_size;

    return {
        {"items", items},
        {"pagination", {
            {"page", page},
            {"page_size", page_size},
            {"total_count", total_count},
            {"total_pages", total_pages},
            {"has_next", page < total_pages},
            {"has_prev", page > 1}
        }}
    };
}

bool DynamicConfigAPIHandlers::is_admin_user(const std::string& user_id) {
    return access_control_.is_admin(user_id);
}

std::vector<std::string> DynamicConfigAPIHandlers::get_user_allowed_scopes(const std::string& user_id) {
    auto scopes = access_control_.get_user_scopes(user_id);

    if (scopes.empty() && access_control_.is_admin(user_id)) {
        scopes = {"GLOBAL", "ENVIRONMENT", "MODULE", "ORGANIZATION", "USER"};
    }

    return scopes;
}

} // namespace regulens
