/**
 * LLM Key Manager
 * Production-grade API key management for LLM providers with encryption, rotation, and usage tracking
 */

#ifndef REGULENS_LLM_KEY_MANAGER_HPP
#define REGULENS_LLM_KEY_MANAGER_HPP

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <chrono>
#include <nlohmann/json.hpp>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"

namespace regulens {
namespace llm {

struct LLMKey {
    std::string key_id;
    std::string key_name;
    std::string provider; // 'openai', 'anthropic', 'google', 'azure', 'other'
    std::string model; // Specific model this key is for
    std::string encrypted_key; // Encrypted full key
    std::string key_hash; // SHA-256 hash for verification
    std::string key_last_four; // Last 4 characters for display
    std::string status; // 'active', 'inactive', 'expired', 'compromised', 'rotated'
    std::string created_by;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
    std::optional<std::chrono::system_clock::time_point> expires_at;
    std::optional<std::chrono::system_clock::time_point> last_used_at;
    int usage_count = 0;
    int error_count = 0;
    std::optional<int> rate_limit_remaining;
    std::optional<std::chrono::system_clock::time_point> rate_limit_reset_at;
    std::string rotation_schedule; // 'daily', 'weekly', 'monthly', 'quarterly', 'manual'
    std::optional<std::chrono::system_clock::time_point> last_rotated_at;
    int rotation_reminder_days = 30;
    bool auto_rotate = false;
    std::vector<std::string> tags;
    nlohmann::json metadata;
    bool is_default = false;
    std::string environment; // 'development', 'staging', 'production'
};

struct KeyRotationRecord {
    std::string rotation_id;
    std::string key_id;
    std::string rotated_by;
    std::string rotation_reason; // 'scheduled', 'manual', 'compromised', 'expired'
    std::string rotation_method; // 'automatic', 'manual'
    std::string old_key_last_four;
    std::string new_key_last_four;
    std::string old_key_hash;
    std::string new_key_hash;
    bool rotation_success = true;
    std::optional<std::string> error_message;
    std::chrono::system_clock::time_point rotated_at;
    nlohmann::json metadata;
};

struct KeyUsageStats {
    std::string usage_id;
    std::string key_id;
    std::chrono::system_clock::time_point request_timestamp;
    std::string provider;
    std::string model;
    std::string operation_type; // 'chat_completion', 'embeddings', 'moderation', etc.
    std::optional<int> tokens_used;
    std::optional<double> cost_usd;
    std::optional<int> response_time_ms;
    bool success = true;
    std::optional<std::string> error_type;
    std::optional<std::string> error_message;
    std::string user_id;
    std::string session_id;
    nlohmann::json metadata;
};

struct KeyHealthStatus {
    std::string check_id;
    std::string key_id;
    std::chrono::system_clock::time_point check_timestamp;
    std::string check_type; // 'liveness', 'rate_limit', 'quota'
    std::string status; // 'healthy', 'warning', 'error'
    std::optional<int> response_time_ms;
    std::optional<int> rate_limit_remaining;
    std::optional<double> quota_remaining;
    std::optional<std::string> error_message;
    nlohmann::json metadata;
};

struct KeyAlert {
    std::string alert_id;
    std::string key_id;
    std::string alert_type; // 'expiration', 'rotation_due', 'rate_limit', 'error_rate', 'cost_limit'
    std::string severity; // 'info', 'warning', 'error', 'critical'
    std::string title;
    std::string message;
    std::optional<double> threshold_value;
    std::optional<double> actual_value;
    bool resolved = false;
    std::optional<std::chrono::system_clock::time_point> resolved_at;
    std::optional<std::string> resolved_by;
    std::chrono::system_clock::time_point created_at;
    nlohmann::json metadata;
};

struct CreateKeyRequest {
    std::string key_name;
    std::string provider;
    std::optional<std::string> model;
    std::string plain_key; // Plain text key (will be encrypted)
    std::string created_by;
    std::optional<std::chrono::system_clock::time_point> expires_at;
    std::optional<std::string> rotation_schedule;
    bool auto_rotate = false;
    std::vector<std::string> tags;
    nlohmann::json metadata;
    bool is_default = false;
    std::string environment = "production";
};

struct UpdateKeyRequest {
    std::string key_name;
    std::optional<std::string> model;
    std::optional<std::chrono::system_clock::time_point> expires_at;
    std::optional<std::string> rotation_schedule;
    std::optional<bool> auto_rotate;
    std::optional<std::vector<std::string>> tags;
    std::optional<nlohmann::json> metadata;
    std::optional<bool> is_default;
    std::optional<std::string> status;
};

struct RotateKeyRequest {
    std::string key_id;
    std::string new_plain_key;
    std::string rotated_by;
    std::string rotation_reason = "manual";
    bool backup_old_key = true;
};

class LLMKeyManager {
public:
    LLMKeyManager(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<StructuredLogger> logger
    );

    ~LLMKeyManager();

    // Key management
    std::optional<LLMKey> create_key(const CreateKeyRequest& request);
    std::optional<LLMKey> get_key(const std::string& key_id);
    std::vector<LLMKey> get_keys(const std::string& user_id = "", const std::string& provider = "",
                                const std::string& status = "", int limit = 50, int offset = 0);
    bool update_key(const std::string& key_id, const UpdateKeyRequest& request);
    bool delete_key(const std::string& key_id, const std::string& deleted_by);

    // Key retrieval for use (decrypted)
    std::optional<std::string> get_decrypted_key(const std::string& key_id);
    std::optional<LLMKey> get_active_key_for_provider(const std::string& provider,
                                                     const std::string& environment = "production");

    // Key rotation
    std::optional<KeyRotationRecord> rotate_key(const RotateKeyRequest& request);
    std::vector<KeyRotationRecord> get_rotation_history(const std::string& key_id, int limit = 50);

    // Usage tracking
    bool record_usage(const KeyUsageStats& usage);
    std::vector<KeyUsageStats> get_usage_history(const std::string& key_id, int limit = 100);
    nlohmann::json get_usage_analytics(const std::string& key_id,
                                     const std::optional<std::string>& time_range = std::nullopt);

    // Health monitoring
    KeyHealthStatus check_key_health(const std::string& key_id, const std::string& check_type = "liveness");
    std::vector<KeyHealthStatus> get_health_history(const std::string& key_id, int limit = 50);

    // Alerts and notifications
    std::optional<KeyAlert> create_alert(const KeyAlert& alert);
    std::vector<KeyAlert> get_active_alerts(const std::string& key_id = "");
    bool resolve_alert(const std::string& alert_id, const std::string& resolved_by);

    // Background tasks
    void process_scheduled_rotations();
    void check_key_expirations();
    void update_daily_usage_stats();
    void cleanup_old_data(int retention_days = 365);

    // Analytics and reporting
    nlohmann::json get_provider_usage_summary(const std::string& time_range = "30d");
    nlohmann::json get_key_performance_metrics(const std::string& key_id);
    std::vector<std::string> get_keys_due_for_rotation(int days_ahead = 30);

    // Configuration
    void set_encryption_key(const std::string& key);
    void set_max_keys_per_user(int max_keys);
    void set_default_rotation_schedule(const std::string& schedule);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<StructuredLogger> logger_;

    // Configuration
    std::string encryption_key_;
    int max_keys_per_user_ = 50;
    std::string default_rotation_schedule_ = "monthly";
    int usage_aggregation_interval_hours_ = 24;

    // Internal methods
    std::string generate_uuid();
    std::string encrypt_key(const std::string& plain_key);
    std::string decrypt_key(const std::string& encrypted_key);
    std::string hash_key(const std::string& key);
    std::string get_key_last_four(const std::string& key);

    std::optional<LLMKey> query_key(const std::string& key_id);
    bool update_key_status(const std::string& key_id, const std::string& status);
    bool increment_usage_count(const std::string& key_id, bool success);
    bool update_rate_limits(const std::string& key_id, int remaining, const std::chrono::system_clock::time_point& reset_at);

    // Rotation helpers
    std::string generate_new_key_for_provider(const std::string& provider);
    bool backup_key(const std::string& key_id, const std::string& backup_type, const std::string& created_by);
    bool validate_key_format(const std::string& key, const std::string& provider);

    // Analytics helpers
    void aggregate_daily_usage();
    nlohmann::json calculate_key_metrics(const std::string& key_id, const std::string& time_range);
    std::vector<std::string> find_expired_keys();
    std::vector<std::string> find_keys_needing_rotation();

    // Alert generation
    void check_and_create_expiration_alerts();
    void check_and_create_usage_alerts();
    void check_and_create_health_alerts();

    // Utility methods
    bool is_valid_provider(const std::string& provider);
    bool is_valid_status(const std::string& status);
    bool is_valid_rotation_schedule(const std::string& schedule);
    std::chrono::system_clock::time_point parse_time_range(const std::string& time_range);

    // Encryption utilities
    std::string base64_encode(const std::string& input);
    std::string base64_decode(const std::string& input);

    // Logging helpers
    void log_key_creation(const std::string& key_id, const CreateKeyRequest& request);
    void log_key_rotation(const KeyRotationRecord& record);
    void log_key_deletion(const std::string& key_id, const std::string& deleted_by);
    void log_usage_anomaly(const std::string& key_id, const std::string& anomaly_type, double value);
};

} // namespace llm
} // namespace regulens

#endif // REGULENS_LLM_KEY_MANAGER_HPP
