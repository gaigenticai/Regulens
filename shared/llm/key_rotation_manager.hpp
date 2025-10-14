/**
 * Key Rotation Manager
 * Automated key rotation scheduling and execution for LLM API keys
 */

#ifndef REGULENS_KEY_ROTATION_MANAGER_HPP
#define REGULENS_KEY_ROTATION_MANAGER_HPP

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <nlohmann/json.hpp>
#include "llm_key_manager.hpp"
#include "../logging/structured_logger.hpp"

namespace regulens {
namespace llm {

struct RotationSchedule {
    std::string schedule_type; // 'fixed_interval', 'calendar_based', 'usage_based', 'event_based'
    std::chrono::hours interval_hours = std::chrono::hours(24 * 30); // Default 30 days
    std::string calendar_expression; // For cron-like expressions
    long usage_threshold = 1000000; // Tokens or requests
    std::vector<std::string> trigger_events; // Events that trigger rotation
};

struct RotationJob {
    std::string job_id;
    std::string key_id;
    std::string key_name;
    std::string provider;
    RotationSchedule schedule;
    std::chrono::system_clock::time_point next_rotation_at;
    std::chrono::system_clock::time_point last_rotation_at;
    std::string status; // 'scheduled', 'running', 'completed', 'failed', 'paused'
    int rotation_count = 0;
    bool auto_rotate = true;
    nlohmann::json metadata;
};

struct RotationResult {
    std::string job_id;
    std::string key_id;
    bool success = false;
    std::string old_key_last_four;
    std::string new_key_last_four;
    std::optional<std::string> error_message;
    int tokens_used = 0;
    double cost_incurred = 0.0;
    std::chrono::milliseconds duration;
    nlohmann::json metadata;
};

struct RotationConfig {
    bool enabled = true;
    int max_concurrent_rotations = 3;
    int rotation_timeout_seconds = 300;
    int retry_attempts = 3;
    std::chrono::seconds retry_delay = std::chrono::seconds(60);
    bool backup_before_rotation = true;
    int backup_retention_days = 90;
    std::string default_provider_url; // For automated key generation
    nlohmann::json provider_configs; // Provider-specific rotation configs
};

class KeyRotationManager {
public:
    KeyRotationManager(
        std::shared_ptr<LLMKeyManager> key_manager,
        std::shared_ptr<StructuredLogger> logger
    );

    ~KeyRotationManager();

    // Job management
    std::optional<RotationJob> schedule_rotation_job(const std::string& key_id);
    std::optional<RotationJob> get_rotation_job(const std::string& job_id);
    std::vector<RotationJob> get_scheduled_jobs(int limit = 50);
    bool pause_rotation_job(const std::string& job_id);
    bool resume_rotation_job(const std::string& job_id);
    bool cancel_rotation_job(const std::string& job_id);

    // Manual rotation
    RotationResult rotate_key_now(const std::string& key_id, const std::string& new_key = "");

    // Automated rotation
    void start_automated_rotation();
    void stop_automated_rotation();
    bool is_automated_rotation_running() const;

    // Schedule management
    RotationSchedule get_default_schedule(const std::string& provider);
    bool update_rotation_schedule(const std::string& key_id, const RotationSchedule& schedule);

    // Provider integration
    std::string generate_new_key_for_provider(const std::string& provider);
    bool validate_key_with_provider(const std::string& provider, const std::string& key);
    nlohmann::json get_provider_rotation_config(const std::string& provider);

    // Monitoring and reporting
    nlohmann::json get_rotation_statistics(const std::string& time_range = "30d");
    std::vector<RotationResult> get_recent_rotations(int limit = 50);
    std::vector<std::string> get_keys_due_for_rotation(int hours_ahead = 24);

    // Configuration
    void set_rotation_config(const RotationConfig& config);
    RotationConfig get_rotation_config() const;

    // Health checks
    bool perform_health_check();
    nlohmann::json get_health_status();

private:
    std::shared_ptr<LLMKeyManager> key_manager_;
    std::shared_ptr<StructuredLogger> logger_;

    // Configuration
    RotationConfig config_;

    // Background processing
    std::thread rotation_thread_;
    std::mutex rotation_mutex_;
    std::condition_variable rotation_cv_;
    bool running_ = false;
    bool stop_requested_ = false;

    // Internal methods
    std::string generate_job_id();
    RotationSchedule parse_schedule_string(const std::string& schedule_str);
    std::chrono::system_clock::time_point calculate_next_rotation_time(const RotationSchedule& schedule,
                                                                      const std::chrono::system_clock::time_point& last_rotation);

    // Rotation execution
    void rotation_worker_loop();
    void process_pending_rotations();
    RotationResult execute_rotation(const RotationJob& job);
    RotationResult perform_key_rotation(const std::string& key_id, const std::string& new_key);

    // Provider-specific methods
    std::string generate_openai_key();
    std::string generate_anthropic_key();
    std::string generate_google_key();
    std::string generate_azure_key();

    bool test_openai_key(const std::string& key);
    bool test_anthropic_key(const std::string& key);
    bool test_google_key(const std::string& key);
    bool test_azure_key(const std::string& key);

    // Job management
    bool save_rotation_job(const RotationJob& job);
    bool update_rotation_job_status(const std::string& job_id, const std::string& status);
    bool delete_rotation_job(const std::string& job_id);

    // Utility methods
    bool is_rotation_due(const RotationJob& job);
    bool should_rotate_based_on_usage(const std::string& key_id, const RotationSchedule& schedule);
    bool should_rotate_based_on_schedule(const RotationJob& job);

    // HTTP client for provider API calls (would need to be injected)
    // std::shared_ptr<HTTPClient> http_client_;

    // Logging helpers
    void log_rotation_started(const std::string& job_id, const std::string& key_id);
    void log_rotation_completed(const RotationResult& result);
    void log_rotation_failed(const std::string& job_id, const std::string& key_id, const std::string& error);
    void log_provider_api_call(const std::string& provider, const std::string& endpoint, bool success);

    // Metrics collection
    void record_rotation_metric(const std::string& metric_name, double value, const std::string& key_id = "");
    void update_rotation_success_rate(const std::string& provider, bool success);
};

} // namespace llm
} // namespace regulens

#endif // REGULENS_KEY_ROTATION_MANAGER_HPP
