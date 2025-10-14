/**
 * Dynamic Configuration API Handlers
 * REST API endpoints for database-driven configuration management
 */

#ifndef DYNAMIC_CONFIG_API_HANDLERS_HPP
#define DYNAMIC_CONFIG_API_HANDLERS_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>
#include "dynamic_config_manager.hpp"
#include "../database/postgresql_connection.hpp"
#include "../security/access_control_service.hpp"

namespace regulens {

class DynamicConfigAPIHandlers {
public:
    DynamicConfigAPIHandlers(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<DynamicConfigManager> config_manager
    );

    ~DynamicConfigAPIHandlers();

    // Configuration CRUD endpoints
    std::string handle_get_config(const std::string& key, const std::string& scope, const std::string& user_id);
    std::string handle_set_config(const std::string& request_body, const std::string& user_id);
    std::string handle_update_config(const std::string& key, const std::string& scope, const std::string& request_body, const std::string& user_id);
    std::string handle_delete_config(const std::string& key, const std::string& scope, const std::string& user_id);

    // Bulk configuration endpoints
    std::string handle_get_configs_by_scope(const std::string& scope, const std::string& user_id);
    std::string handle_get_configs_by_module(const std::string& module, const std::string& user_id);
    std::string handle_bulk_set_configs(const std::string& request_body, const std::string& user_id);

    // Configuration schema endpoints
    std::string handle_register_config_schema(const std::string& request_body, const std::string& user_id);
    std::string handle_get_config_schemas(const std::string& user_id);
    std::string handle_validate_config_value(const std::string& request_body, const std::string& user_id);

    // Configuration change management endpoints
    std::string handle_get_config_history(const std::string& key, const std::string& scope, const std::string& query_params, const std::string& user_id);
    std::string handle_rollback_config_change(const std::string& change_id, const std::string& user_id);

    // Configuration environment management endpoints
    std::string handle_set_environment_config(const std::string& environment, const std::string& request_body, const std::string& user_id);
    std::string handle_get_environment_configs(const std::string& environment, const std::string& user_id);

    // Security and encryption endpoints
    std::string handle_encrypt_config(const std::string& key, const std::string& scope, const std::string& user_id);
    std::string handle_decrypt_config(const std::string& key, const std::string& scope, const std::string& user_id);

    // Configuration management endpoints
    std::string handle_reload_configs(const std::string& user_id);
    std::string handle_export_configs(const std::string& scope, const std::string& query_params, const std::string& user_id);
    std::string handle_import_configs(const std::string& request_body, const std::string& user_id);

    // Monitoring and analytics endpoints
    std::string handle_get_config_stats(const std::string& user_id);
    std::string handle_get_config_usage_stats(const std::string& user_id);
    std::string handle_get_most_changed_configs(const std::string& user_id);

    // Configuration backup endpoints
    std::string handle_create_config_backup(const std::string& backup_name, const std::string& user_id);
    std::string handle_restore_config_backup(const std::string& backup_name, const std::string& user_id);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<DynamicConfigManager> config_manager_;
    AccessControlService access_control_;

    // Helper methods
    ConfigScope parse_scope_param(const std::string& scope_str);
    nlohmann::json format_config_value(const ConfigValue& config);
    nlohmann::json format_config_change(const ConfigChangeLog& change);
    nlohmann::json format_validation_result(const ConfigValidationResult& result);

    // Query parameter parsing
    std::unordered_map<std::string, std::string> parse_query_params(const std::string& query_string);
    int parse_int_param(const std::string& value, int default_value = 0);
    bool parse_bool_param(const std::string& value, bool default_value = false);

    // Request validation
    bool validate_config_request(const nlohmann::json& request, std::string& error_message);
    bool validate_user_access(const std::string& user_id, const std::string& operation, const std::string& key = "");
    bool validate_scope_access(const std::string& user_id, ConfigScope scope);

    // Response formatting
    nlohmann::json create_success_response(const nlohmann::json& data = nullptr,
                                         const std::string& message = "");
    nlohmann::json create_error_response(const std::string& message,
                                       int status_code = 400);
    nlohmann::json create_paginated_response(const std::vector<nlohmann::json>& items,
                                           int total_count,
                                           int page,
                                           int page_size);

    // Bulk operation helpers
    std::vector<std::pair<std::string, nlohmann::json>> parse_bulk_config_request(const nlohmann::json& request);
    nlohmann::json format_bulk_operation_results(const std::vector<std::pair<std::string, bool>>& results);

    // File operation helpers
    std::string get_backup_filename(const std::string& backup_name);
    bool validate_backup_filename(const std::string& filename);

    // Utility methods
    std::string get_config_operation_description(const std::string& operation, const std::string& key);
    bool is_admin_user(const std::string& user_id);
    std::vector<std::string> get_user_allowed_scopes(const std::string& user_id);
};

} // namespace regulens

#endif // DYNAMIC_CONFIG_API_HANDLERS_HPP
