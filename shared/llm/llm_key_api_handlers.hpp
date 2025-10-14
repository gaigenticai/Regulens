/**
 * LLM Key API Handlers
 * REST API endpoints for LLM key management, rotation, and analytics
 */

#ifndef REGULENS_LLM_KEY_API_HANDLERS_HPP
#define REGULENS_LLM_KEY_API_HANDLERS_HPP

#include <string>
#include <map>
#include <memory>
#include <libpq-fe.h>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"
#include "llm_key_manager.hpp"
#include "key_rotation_manager.hpp"

namespace regulens {
namespace llm {

class LLMKeyAPIHandlers {
public:
    LLMKeyAPIHandlers(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<StructuredLogger> logger,
        std::shared_ptr<LLMKeyManager> key_manager,
        std::shared_ptr<KeyRotationManager> rotation_manager
    );

    // Key management endpoints
    std::string handle_create_key(const std::string& request_body, const std::string& user_id);
    std::string handle_get_keys(const std::string& user_id, const std::map<std::string, std::string>& query_params);
    std::string handle_get_key(const std::string& key_id, const std::string& user_id);
    std::string handle_update_key(const std::string& key_id, const std::string& request_body, const std::string& user_id);
    std::string handle_delete_key(const std::string& key_id, const std::string& user_id);

    // Key rotation endpoints
    std::string handle_rotate_key(const std::string& key_id, const std::string& request_body, const std::string& user_id);
    std::string handle_get_rotation_history(const std::string& key_id, const std::string& user_id, const std::map<std::string, std::string>& query_params);

    // Usage and analytics endpoints
    std::string handle_get_usage_stats(const std::string& key_id, const std::string& user_id, const std::map<std::string, std::string>& query_params);
    std::string handle_get_provider_usage(const std::string& user_id, const std::map<std::string, std::string>& query_params);
    std::string handle_record_usage(const std::string& request_body, const std::string& user_id);

    // Health and monitoring endpoints
    std::string handle_check_key_health(const std::string& key_id, const std::string& user_id);
    std::string handle_get_health_history(const std::string& key_id, const std::string& user_id, const std::map<std::string, std::string>& query_params);

    // Alert management endpoints
    std::string handle_get_alerts(const std::string& user_id, const std::map<std::string, std::string>& query_params);
    std::string handle_resolve_alert(const std::string& alert_id, const std::string& user_id);

    // Rotation job management endpoints
    std::string handle_get_rotation_jobs(const std::string& user_id, const std::map<std::string, std::string>& query_params);
    std::string handle_pause_rotation_job(const std::string& job_id, const std::string& user_id);
    std::string handle_resume_rotation_job(const std::string& job_id, const std::string& user_id);

    // Administrative endpoints
    std::string handle_get_system_analytics(const std::string& user_id, const std::map<std::string, std::string>& query_params);
    std::string handle_process_scheduled_rotations(const std::string& user_id);
    std::string handle_cleanup_old_data(const std::string& request_body, const std::string& user_id);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<LLMKeyManager> key_manager_;
    std::shared_ptr<KeyRotationManager> rotation_manager_;

    // Helper methods
    CreateKeyRequest parse_create_key_request(const nlohmann::json& request_json, const std::string& user_id);
    UpdateKeyRequest parse_update_key_request(const nlohmann::json& request_json);
    RotateKeyRequest parse_rotate_key_request(const nlohmann::json& request_json, const std::string& key_id, const std::string& user_id);

    nlohmann::json format_key_response(const LLMKey& key);
    nlohmann::json format_rotation_record(const KeyRotationRecord& record);
    nlohmann::json format_usage_stats(const KeyUsageStats& stats);
    nlohmann::json format_health_status(const KeyHealthStatus& health);
    nlohmann::json format_alert(const KeyAlert& alert);
    nlohmann::json format_rotation_job(const RotationJob& job);

    bool validate_key_access(const std::string& key_id, const std::string& user_id);
    bool validate_admin_access(const std::string& user_id);
    bool validate_key_data(const nlohmann::json& key_data);

    std::string create_error_response(const std::string& message, int status_code = 400);
    std::string create_success_response(const nlohmann::json& data, const std::string& message = "");

    // Database query helpers
    std::vector<LLMKey> query_user_keys(const std::string& user_id, const std::map<std::string, std::string>& filters, int limit, int offset);
    std::vector<KeyUsageStats> query_usage_stats(const std::string& key_id, const std::map<std::string, std::string>& filters, int limit);
    std::vector<KeyAlert> query_active_alerts(const std::string& user_id, int limit, int offset);

    // Analytics helpers
    nlohmann::json calculate_key_usage_analytics(const std::string& key_id, const std::string& time_range);
    nlohmann::json calculate_provider_usage_summary(const std::string& user_id, const std::string& time_range);
    nlohmann::json calculate_system_usage_analytics(const std::string& time_range);

    // Validation helpers
    bool is_valid_key_format(const std::string& key, const std::string& provider);
    bool is_valid_rotation_schedule(const std::string& schedule);
    bool is_valid_provider(const std::string& provider);

    // Utility methods
    std::map<std::string, std::string> parse_query_parameters(const std::map<std::string, std::string>& query_params);
    std::string mask_key_display(const std::string& full_key);
    std::chrono::system_clock::time_point parse_expiration_date(const std::string& date_str);

    // Security helpers
    bool check_rate_limit(const std::string& user_id, const std::string& operation);
    void record_security_event(const std::string& event_type, const std::string& user_id, const nlohmann::json& details);

    // Background processing helpers
    void trigger_background_key_check(const std::string& key_id);
    void trigger_usage_aggregation();

    // Caching helpers
    std::optional<nlohmann::json> get_cached_analytics(const std::string& cache_key, const std::string& user_id);
    void cache_analytics_result(const std::string& cache_key, const std::string& user_id, const nlohmann::json& data, int ttl_seconds = 300);

    // Metrics and monitoring
    void record_api_metrics(const std::string& endpoint, const std::string& user_id, double response_time_ms, bool success);
    void update_key_health_metrics(const std::string& key_id, const std::string& check_type, bool healthy);
};

} // namespace llm
} // namespace regulens

#endif // REGULENS_LLM_KEY_API_HANDLERS_HPP
