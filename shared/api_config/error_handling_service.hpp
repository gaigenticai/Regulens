/**
 * Error Handling Service
 * Production-grade standardized error handling across all API endpoints
 */

#ifndef ERROR_HANDLING_SERVICE_HPP
#define ERROR_HANDLING_SERVICE_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <memory>
#include <chrono>
#include <nlohmann/json.hpp>
#include "../../shared/error_handler.hpp"
#include "../logging/structured_logger.hpp"
#include "../models/error_handling.hpp"

namespace regulens {

// ErrorCategory is defined in shared/models/error_handling.hpp

struct ErrorCode {
    std::string code;
    ErrorCategory category;
    std::string description;
    int http_status;
    bool retryable;
    std::optional<int> retry_after_seconds;
    std::string user_action;
};

struct ErrorContext {
    std::string request_id;
    std::string method;
    std::string path;
    std::string user_id;
    std::string client_ip;
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> headers;
    std::unordered_map<std::string, std::string> query_params;
    nlohmann::json request_body;
};

struct StandardizedError {
    std::string code;
    std::string message;
    std::optional<std::string> details;
    std::optional<std::string> field;
    std::string timestamp;
    std::string request_id;
    std::string path;
    std::string method;
    std::unordered_map<std::string, std::string> meta;

    // HTTP response fields
    int http_status;
    std::unordered_map<std::string, std::string> headers;
};

struct ErrorResponse {
    int status_code;
    std::string content_type;
    std::string body;
    std::unordered_map<std::string, std::string> headers;
};

class ErrorHandlingService {
public:
    static ErrorHandlingService& get_instance();

    // Initialize with configuration
    bool initialize(const std::string& config_path,
                   std::shared_ptr<StructuredLogger> logger);

    // Error creation and formatting
    StandardizedError create_error(const std::string& error_code,
                                 const std::string& message,
                                 const ErrorContext& context,
                                 const std::optional<std::string>& details = std::nullopt,
                                 const std::optional<std::string>& field = std::nullopt);

    ErrorResponse format_error_response(const StandardizedError& error);

    // Error code management
    std::optional<ErrorCode> get_error_code(const std::string& code);
    std::vector<ErrorCode> get_all_error_codes();
    std::vector<ErrorCode> get_error_codes_by_category(ErrorCategory category);

    // Error logging and tracking
    void log_error(const StandardizedError& error, const ErrorContext& context);
    void track_error_metrics(const std::string& error_code, const std::string& endpoint);

    // Error recovery and retry
    bool is_retryable_error(const std::string& error_code);
    std::optional<int> get_retry_after_seconds(const std::string& error_code);
    ErrorResponse create_retry_response(const std::string& error_code);

    // Localization support
    std::string localize_error_message(const std::string& error_code,
                                     const std::string& language = "en");
    std::vector<std::string> get_supported_languages();

    // Security and compliance
    std::string mask_sensitive_data(const std::string& data);
    bool should_log_error_details(const std::string& error_code);
    std::string generate_error_id();

    // Monitoring and analytics
    std::unordered_map<std::string, int> get_error_statistics(const std::string& time_range = "1h");
    std::vector<std::string> get_top_error_codes(int limit = 10);
    double get_error_rate(const std::string& endpoint, const std::string& time_range = "1h");

    // Configuration management
    bool reload_configuration();
    nlohmann::json get_error_handling_status();

private:
    ErrorHandlingService() = default;
    ~ErrorHandlingService() = default;
    ErrorHandlingService(const ErrorHandlingService&) = delete;
    ErrorHandlingService& operator=(const ErrorHandlingService&) = delete;

    // Configuration loading
    bool load_error_config(const std::string& config_path);
    void build_error_codes_map();
    void build_localization_map();

    // Error formatting helpers
    nlohmann::json format_error_json(const StandardizedError& error);
    std::string format_timestamp(std::chrono::system_clock::time_point tp);
    std::string generate_request_id();

    // Security helpers
    bool contains_sensitive_data(const std::string& data);
    std::string apply_data_masking(const std::string& data);

    // Monitoring helpers
    void update_error_metrics(const std::string& error_code, const std::string& endpoint);
    void cleanup_old_metrics();

    // Configuration data
    std::shared_ptr<StructuredLogger> logger_;
    nlohmann::json error_config_;
    std::string config_path_;

    // Error codes and localization
    std::unordered_map<std::string, ErrorCode> error_codes_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> localized_messages_;

    // Runtime statistics
    std::unordered_map<std::string, int> error_counts_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> last_error_times_;
    std::mutex metrics_mutex_;

    // Request ID generation
    std::atomic<uint64_t> request_id_counter_;
};

} // namespace regulens

#endif // ERROR_HANDLING_SERVICE_HPP
