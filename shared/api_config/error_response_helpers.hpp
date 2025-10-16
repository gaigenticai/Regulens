/**
 * Error Response Helper Functions
 * Production-grade helper functions for consistent error responses across all API handlers
 */

#ifndef ERROR_RESPONSE_HELPERS_HPP
#define ERROR_RESPONSE_HELPERS_HPP

#include <string>
#include <unordered_map>
#include <optional>
#include <nlohmann/json.hpp>
#include "error_handling_service.hpp"

namespace regulens {

// Error creation shortcuts for common scenarios
ErrorContext create_error_context(
    const std::string& request_id,
    const std::string& method,
    const std::string& path,
    const std::string& user_id = "",
    const std::string& client_ip = "",
    const std::unordered_map<std::string, std::string>& headers = {},
    const std::unordered_map<std::string, std::string>& query_params = {},
    const nlohmann::json& request_body = nullptr
);

// Common error response creators
HTTPResponse create_validation_error(
    const std::string& message,
    const ErrorContext& context,
    const std::optional<std::string>& field = std::nullopt,
    const std::optional<std::string>& details = std::nullopt
);

HTTPResponse create_authentication_error(
    const ErrorContext& context,
    const std::string& message = "Authentication required"
);

HTTPResponse create_authorization_error(
    const ErrorContext& context,
    const std::string& message = "Insufficient permissions"
);

HTTPResponse create_not_found_error(
    const ErrorContext& context,
    const std::string& resource_type,
    const std::string& resource_id = ""
);

HTTPResponse create_conflict_error(
    const ErrorContext& context,
    const std::string& message,
    const std::optional<std::string>& details = std::nullopt
);

HTTPResponse create_rate_limit_error(
    const ErrorContext& context,
    int retry_after_seconds = 60
);

HTTPResponse create_internal_error(
    const ErrorContext& context,
    const std::string& message = "Internal server error",
    const std::optional<std::string>& details = std::nullopt
);

HTTPResponse create_service_unavailable_error(
    const ErrorContext& context,
    const std::string& message = "Service temporarily unavailable"
);

HTTPResponse create_maintenance_error(
    const ErrorContext& context
);

// Database-specific errors
HTTPResponse create_database_error(
    const ErrorContext& context,
    const std::string& operation = "database operation"
);

// External service errors
HTTPResponse create_external_service_error(
    const ErrorContext& context,
    const std::string& service_name,
    const std::string& operation = ""
);

// Network errors
HTTPResponse create_network_error(
    const ErrorContext& context,
    const std::string& operation = "network request"
);

// Generic error with custom code
HTTPResponse create_custom_error(
    const std::string& error_code,
    const std::string& message,
    const ErrorContext& context,
    const std::optional<std::string>& details = std::nullopt,
    const std::optional<std::string>& field = std::nullopt
);

// Success responses (for consistency)
HTTPResponse create_success_response(
    const nlohmann::json& data = nullptr,
    const std::string& message = "success",
    const std::unordered_map<std::string, std::string>& meta = {}
);

HTTPResponse create_created_response(
    const nlohmann::json& data = nullptr,
    const std::string& message = "created"
);

HTTPResponse create_no_content_response();

// Pagination helpers
HTTPResponse create_paginated_response(
    const nlohmann::json& items,
    int total_count,
    int page,
    int limit,
    const std::string& base_url
);

// Helper functions for common validation scenarios
HTTPResponse create_missing_field_error(
    const ErrorContext& context,
    const std::string& field_name
);

HTTPResponse create_invalid_field_error(
    const ErrorContext& context,
    const std::string& field_name,
    const std::string& expected_format,
    const std::string& provided_value = ""
);

HTTPResponse create_duplicate_resource_error(
    const ErrorContext& context,
    const std::string& resource_type,
    const std::string& identifier
);

HTTPResponse create_resource_not_modified_error(
    const ErrorContext& context,
    const std::string& last_modified
);

// Utility functions
std::string extract_request_id_from_headers(
    const std::unordered_map<std::string, std::string>& headers
);

std::string extract_client_ip_from_headers(
    const std::unordered_map<std::string, std::string>& headers
);

std::string extract_user_id_from_context(
    const std::unordered_map<std::string, std::string>& headers
);

bool should_include_error_details(
    const std::string& error_code,
    const std::string& environment = "production"
);

// Error logging helper
void log_api_error(
    const std::string& error_code,
    const std::string& message,
    const ErrorContext& context,
    const std::optional<std::string>& details = std::nullopt
);

// Error metrics helper
void track_api_error(
    const std::string& error_code,
    const std::string& endpoint_path
);

} // namespace regulens

#endif // ERROR_RESPONSE_HELPERS_HPP
