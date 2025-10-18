/**
 * Error Handling Service Implementation
 * Production-grade standardized error handling
 */

#include "error_handling_service.hpp"
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <regex>

namespace regulens {

ErrorHandlingService& ErrorHandlingService::get_instance() {
    static ErrorHandlingService instance;
    return instance;
}

bool ErrorHandlingService::initialize(const std::string& config_path,
                                    std::shared_ptr<StructuredLogger> logger) {
    logger_ = logger;
    config_path_ = config_path;
    request_id_counter_ = 0;

    if (!load_error_config(config_path)) {
        if (logger_) {
            logger_->error("Failed to load error handling configuration from: " + config_path);
        }
        return false;
    }

    build_error_codes_map();
    build_localization_map();

    if (logger_) {
        logger_->info("Error handling service initialized with " +
                     std::to_string(error_codes_.size()) + " error codes");
    }

    return true;
}

bool ErrorHandlingService::load_error_config(const std::string& config_path) {
    try {
        std::ifstream file(config_path);
        if (!file.is_open()) {
            if (logger_) {
                logger_->error("Cannot open error config file: " + config_path);
            }
            return false;
        }

        error_config_ = nlohmann::json::parse(file);
        return true;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Error loading error config: " + std::string(e.what()));
        }
        return false;
    }
}

void ErrorHandlingService::build_error_codes_map() {
    if (!error_config_.contains("error_handling") ||
        !error_config_["error_handling"].contains("error_codes")) {
        return;
    }

    const auto& codes = error_config_["error_handling"]["error_codes"];
    const auto& status_mapping = error_config_["error_handling"]["standard_format"]["http_status_mapping"];

    for (const auto& [code, config] : codes.items()) {
        ErrorCode error_code;
        error_code.code = code;
        error_code.description = config.value("description", "");
        error_code.user_action = config.value("user_action", "");
        error_code.retryable = config.value("retryable", false);

        if (config.contains("retry_after_seconds")) {
            error_code.retry_after_seconds = config["retry_after_seconds"];
        }

        // Map category
        std::string category_str = config.value("category", "");
        if (category_str == "client_error") {
            error_code.category = ErrorCategory::VALIDATION;
        } else if (category_str == "authentication_error") {
            error_code.category = ErrorCategory::SECURITY;
        } else if (category_str == "authorization_error") {
            error_code.category = ErrorCategory::SECURITY;
        } else if (category_str == "server_error") {
            error_code.category = ErrorCategory::PROCESSING;
        } else if (category_str == "network_error") {
            error_code.category = ErrorCategory::NETWORK;
        } else if (category_str == "rate_limit_error") {
            error_code.category = ErrorCategory::RESOURCE;
        } else if (category_str == "validation_error") {
            error_code.category = ErrorCategory::VALIDATION;
        } else if (category_str == "external_service_error") {
            error_code.category = ErrorCategory::EXTERNAL_API;
        }

        // Map HTTP status
        if (status_mapping.contains(code)) {
            error_code.http_status = status_mapping[code];
        } else {
            // Default status based on category
            switch (error_code.category) {
                case ErrorCategory::VALIDATION:
                    error_code.http_status = 400;
                    break;
                case ErrorCategory::SECURITY:
                    error_code.http_status = 401;
                    break;
                case ErrorCategory::RESOURCE:
                    error_code.http_status = 429;
                    break;
                case ErrorCategory::PROCESSING:
                case ErrorCategory::NETWORK:
                case ErrorCategory::EXTERNAL_API:
                default:
                    error_code.http_status = 500;
                    break;
            }
        }

        error_codes_[code] = error_code;
    }
}

void ErrorHandlingService::build_localization_map() {
    if (!error_config_.contains("error_handling") ||
        !error_config_["error_handling"].contains("error_localization") ||
        !error_config_["error_handling"]["error_localization"].contains("message_templates")) {
        return;
    }

    const auto& templates = error_config_["error_handling"]["error_localization"]["message_templates"];

    for (const auto& [error_code, translations] : templates.items()) {
        std::unordered_map<std::string, std::string> code_translations;
        for (const auto& [lang, message] : translations.items()) {
            code_translations[lang] = message;
        }
        localized_messages_[error_code] = code_translations;
    }
}

StandardizedError ErrorHandlingService::create_error(const std::string& error_code,
                                                   const std::string& message,
                                                   const ErrorContext& context,
                                                   const std::optional<std::string>& details,
                                                   const std::optional<std::string>& field) {
    StandardizedError error;

    auto error_code_info = get_error_code(error_code);
    if (!error_code_info) {
        // Fallback for unknown error codes
        error.code = "INTERNAL_ERROR";
        error.http_status = 500;
    } else {
        error.code = error_code;
        error.http_status = error_code_info->http_status;
    }

    error.message = message;
    error.details = details;
    error.field = field;
    error.timestamp = format_timestamp(context.timestamp);
    error.request_id = context.request_id.empty() ? generate_request_id() : context.request_id;
    error.path = context.path;
    error.method = context.method;

    // Add metadata
    error.meta["version"] = "v1"; // Could be dynamic based on context
    error.meta["user_id"] = context.user_id;
    error.meta["client_ip"] = context.client_ip;

    // Add deprecation warnings if applicable
    // This would be integrated with the versioning service in production

    return error;
}

HTTPResponse ErrorHandlingService::format_error_response(const StandardizedError& error) {
    HTTPResponse response;

    response.status_code = error.http_status;
    response.content_type = "application/json";

    // Add standard headers
    response.headers["X-Error-Code"] = error.code;
    response.headers["X-Request-ID"] = error.request_id;
    response.headers["Content-Type"] = response.content_type;

    // Add retry-after header if applicable
    if (auto retry_after = get_retry_after_seconds(error.code)) {
        response.headers["Retry-After"] = std::to_string(*retry_after);
    }

    // Format response body
    nlohmann::json error_json = format_error_json(error);
    response.body = error_json.dump(2);

    return response;
}

nlohmann::json ErrorHandlingService::format_error_json(const StandardizedError& error) {
    nlohmann::json json;

    // Error object
    nlohmann::json error_obj;
    error_obj["code"] = error.code;
    error_obj["message"] = error.message;

    if (error.details) {
        error_obj["details"] = *error.details;
    }
    if (error.field) {
        error_obj["field"] = *error.field;
    }

    error_obj["timestamp"] = error.timestamp;
    error_obj["request_id"] = error.request_id;
    error_obj["path"] = error.path;
    error_obj["method"] = error.method;

    json["error"] = error_obj;

    // Metadata
    if (!error.meta.empty()) {
        json["meta"] = error.meta;
    }

    return json;
}

std::optional<ErrorCode> ErrorHandlingService::get_error_code(const std::string& code) {
    auto it = error_codes_.find(code);
    if (it != error_codes_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<ErrorCode> ErrorHandlingService::get_all_error_codes() {
    std::vector<ErrorCode> codes;
    for (const auto& [code, info] : error_codes_) {
        codes.push_back(info);
    }
    return codes;
}

std::vector<ErrorCode> ErrorHandlingService::get_error_codes_by_category(ErrorCategory category) {
    std::vector<ErrorCode> codes;
    for (const auto& [code, info] : error_codes_) {
        if (info.category == category) {
            codes.push_back(info);
        }
    }
    return codes;
}

void ErrorHandlingService::log_error(const StandardizedError& error, const ErrorContext& context) {
    if (!logger_) return;

    // Determine log level based on error type
    std::string log_level = "error";
    if (error.http_status >= 400 && error.http_status < 500) {
        log_level = "warn";
    } else if (error.code == "NOT_FOUND") {
        log_level = "info";
    }

    // Create log entry
    nlohmann::json log_entry;
    log_entry["error_code"] = error.code;
    log_entry["message"] = error.message;
    log_entry["http_status"] = error.http_status;
    log_entry["request_id"] = error.request_id;
    log_entry["path"] = error.path;
    log_entry["method"] = error.method;
    log_entry["user_id"] = context.user_id;
    log_entry["client_ip"] = context.client_ip;
    log_entry["timestamp"] = error.timestamp;

    if (error.details && should_log_error_details(error.code)) {
        log_entry["details"] = mask_sensitive_data(*error.details);
    }

    // Mask sensitive data in request context
    if (!context.request_body.is_null()) {
        log_entry["request_body"] = mask_sensitive_data(context.request_body.dump());
    }

    if (log_level == "error") {
        logger_->error("API Error", log_entry);
    } else if (log_level == "warn") {
        logger_->warn("API Warning", log_entry);
    } else {
        logger_->info("API Info", log_entry);
    }
}

void ErrorHandlingService::track_error_metrics(const std::string& error_code, const std::string& endpoint) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    std::string key = error_code + ":" + endpoint;
    error_counts_[key]++;
    last_error_times_[key] = std::chrono::system_clock::now();

    // Cleanup old metrics periodically
    static auto last_cleanup = std::chrono::system_clock::now();
    auto now = std::chrono::system_clock::now();
    if (std::chrono::duration_cast<std::chrono::hours>(now - last_cleanup).count() >= 1) {
        cleanup_old_metrics();
        last_cleanup = now;
    }
}

bool ErrorHandlingService::is_retryable_error(const std::string& error_code) {
    auto error_info = get_error_code(error_code);
    return error_info && error_info->retryable;
}

std::optional<int> ErrorHandlingService::get_retry_after_seconds(const std::string& error_code) {
    auto error_info = get_error_code(error_code);
    return error_info ? error_info->retry_after_seconds : std::nullopt;
}

ErrorResponse ErrorHandlingService::create_retry_response(const std::string& error_code) {
    auto retry_after = get_retry_after_seconds(error_code);

    ErrorContext context;
    context.timestamp = std::chrono::system_clock::now();

    auto error = create_error(
        error_code,
        localize_error_message(error_code),
        context,
        "Operation can be retried after the specified delay"
    );

    auto http_response = format_error_response(error);
    
    // Convert HTTPResponse to ErrorResponse
    ErrorResponse response;
    response.status_code = http_response.status_code;
    response.content_type = http_response.content_type;
    response.body = http_response.body;
    response.headers = http_response.headers;

    if (retry_after) {
        response.headers["Retry-After"] = std::to_string(*retry_after);
    }

    return response;
}

std::string ErrorHandlingService::localize_error_message(const std::string& error_code,
                                                       const std::string& language) {
    auto it = localized_messages_.find(error_code);
    if (it != localized_messages_.end()) {
        auto lang_it = it->second.find(language);
        if (lang_it != it->second.end()) {
            return lang_it->second;
        }

        // Fallback to English
        auto en_it = it->second.find("en");
        if (en_it != it->second.end()) {
            return en_it->second;
        }
    }

    // Fallback to error code description
    auto error_info = get_error_code(error_code);
    return error_info ? error_info->description : "An error occurred";
}

std::vector<std::string> ErrorHandlingService::get_supported_languages() {
    return {"en", "es", "fr", "de", "zh", "ja"};
}

std::string ErrorHandlingService::mask_sensitive_data(const std::string& data) {
    if (!contains_sensitive_data(data)) {
        return data;
    }

    return apply_data_masking(data);
}

bool ErrorHandlingService::should_log_error_details(const std::string& error_code) {
    // Don't log detailed information for authentication errors
    return error_code != "AUTHENTICATION_ERROR" && error_code != "AUTHORIZATION_ERROR";
}

std::string ErrorHandlingService::generate_error_id() {
    return generate_request_id();
}

std::unordered_map<std::string, int> ErrorHandlingService::get_error_statistics(const std::string& time_range) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return error_counts_;
}

std::vector<std::string> ErrorHandlingService::get_top_error_codes(int limit) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    std::vector<std::pair<std::string, int>> sorted_counts;
    for (const auto& [key, count] : error_counts_) {
        sorted_counts.emplace_back(key, count);
    }

    std::sort(sorted_counts.begin(), sorted_counts.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    std::vector<std::string> top_codes;
    for (int i = 0; i < std::min(limit, static_cast<int>(sorted_counts.size())); ++i) {
        top_codes.push_back(sorted_counts[i].first);
    }

    return top_codes;
}

double ErrorHandlingService::get_error_rate(const std::string& endpoint, const std::string& time_range) {
    // Simplified implementation - in production this would calculate actual rates
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    int error_count = 0;
    for (const auto& [key, count] : error_counts_) {
        if (key.find(":" + endpoint) != std::string::npos) {
            error_count += count;
        }
    }

    // Assume 1000 total requests for calculation (simplified)
    return error_count > 0 ? (static_cast<double>(error_count) / 1000.0) * 100.0 : 0.0;
}

bool ErrorHandlingService::reload_configuration() {
    return initialize(config_path_, logger_);
}

nlohmann::json ErrorHandlingService::get_error_handling_status() {
    nlohmann::json status;

    status["total_error_codes"] = error_codes_.size();
    status["supported_languages"] = get_supported_languages();
    status["error_statistics"] = get_error_statistics();
    status["top_error_codes"] = get_top_error_codes(5);

    return status;
}

// Private helper methods
std::string ErrorHandlingService::format_timestamp(std::chrono::system_clock::time_point tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::string ErrorHandlingService::generate_request_id() {
    uint64_t id = ++request_id_counter_;
    std::stringstream ss;
    ss << "req_" << std::hex << std::setfill('0') << std::setw(16) << id;
    return ss.str();
}

bool ErrorHandlingService::contains_sensitive_data(const std::string& data) {
    std::vector<std::string> sensitive_patterns = {
        "password", "token", "secret", "key", "authorization",
        "credit_card", "ssn", "social_security"
    };

    std::string lower_data = data;
    std::transform(lower_data.begin(), lower_data.end(), lower_data.begin(), ::tolower);

    for (const auto& pattern : sensitive_patterns) {
        if (lower_data.find(pattern) != std::string::npos) {
            return true;
        }
    }

    return false;
}

std::string ErrorHandlingService::apply_data_masking(const std::string& data) {
    // Simple masking - replace sensitive content with asterisks
    std::string masked = data;

    // Mask JSON string values that might contain sensitive data
    // Use custom delimiter to avoid conflicts with the closing ")
    std::string pattern = R"===("((?:password|token|secret|key)[^"]*)":\s*"[^"]*")===";
    std::regex sensitive_regex(pattern);
    masked = std::regex_replace(masked, sensitive_regex, R"("$1": "********")");

    return masked;
}

void ErrorHandlingService::update_error_metrics(const std::string& error_code, const std::string& endpoint) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    std::string key = error_code + ":" + endpoint;
    error_counts_[key]++;
}

void ErrorHandlingService::cleanup_old_metrics() {
    auto now = std::chrono::system_clock::now();
    auto cutoff = now - std::chrono::hours(24); // Keep metrics for 24 hours

    for (auto it = last_error_times_.begin(); it != last_error_times_.end(); ) {
        if (it->second < cutoff) {
            error_counts_.erase(it->first);
            it = last_error_times_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace regulens
