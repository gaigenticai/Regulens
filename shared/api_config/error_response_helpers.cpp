/**
 * Error Response Helper Functions Implementation
 * Production-grade helper functions for consistent error responses
 */

#include "error_response_helpers.hpp"

namespace regulens {

// Error creation shortcuts for common scenarios
ErrorContext create_error_context(
    const std::string& request_id,
    const std::string& method,
    const std::string& path,
    const std::string& user_id,
    const std::string& client_ip,
    const std::unordered_map<std::string, std::string>& headers,
    const std::unordered_map<std::string, std::string>& query_params,
    const nlohmann::json& request_body
) {
    ErrorContext context;
    context.request_id = request_id;
    context.method = method;
    context.path = path;
    context.user_id = user_id;
    context.client_ip = client_ip;
    context.timestamp = std::chrono::system_clock::now();
    context.headers = headers;
    context.query_params = query_params;
    context.request_body = request_body;

    return context;
}

HTTPResponse create_validation_error(
    const std::string& message,
    const ErrorContext& context,
    const std::optional<std::string>& field,
    const std::optional<std::string>& details
) {
    auto& error_service = ErrorHandlingService::get_instance();

    auto error = error_service.create_error(
        "VALIDATION_ERROR",
        message,
        context,
        details,
        field
    );

    log_api_error("VALIDATION_ERROR", message, context, details);
    track_api_error("VALIDATION_ERROR", context.path);

    return error_service.format_error_response(error);
}

HTTPResponse create_authentication_error(
    const ErrorContext& context,
    const std::string& message
) {
    auto& error_service = ErrorHandlingService::get_instance();

    auto error = error_service.create_error(
        "AUTHENTICATION_ERROR",
        message,
        context
    );

    log_api_error("AUTHENTICATION_ERROR", message, context);
    track_api_error("AUTHENTICATION_ERROR", context.path);

    return error_service.format_error_response(error);
}

HTTPResponse create_authorization_error(
    const ErrorContext& context,
    const std::string& message
) {
    auto& error_service = ErrorHandlingService::get_instance();

    auto error = error_service.create_error(
        "AUTHORIZATION_ERROR",
        message,
        context
    );

    log_api_error("AUTHORIZATION_ERROR", message, context);
    track_api_error("AUTHORIZATION_ERROR", context.path);

    return error_service.format_error_response(error);
}

HTTPResponse create_not_found_error(
    const ErrorContext& context,
    const std::string& resource_type,
    const std::string& resource_id
) {
    std::string message = resource_type + " not found";
    if (!resource_id.empty()) {
        message += ": " + resource_id;
    }

    auto& error_service = ErrorHandlingService::get_instance();

    auto error = error_service.create_error(
        "NOT_FOUND",
        message,
        context
    );

    log_api_error("NOT_FOUND", message, context);
    track_api_error("NOT_FOUND", context.path);

    return error_service.format_error_response(error);
}

HTTPResponse create_conflict_error(
    const ErrorContext& context,
    const std::string& message,
    const std::optional<std::string>& details
) {
    auto& error_service = ErrorHandlingService::get_instance();

    auto error = error_service.create_error(
        "CONFLICT",
        message,
        context,
        details
    );

    log_api_error("CONFLICT", message, context, details);
    track_api_error("CONFLICT", context.path);

    return error_service.format_error_response(error);
}

HTTPResponse create_rate_limit_error(
    const ErrorContext& context,
    int retry_after_seconds
) {
    auto& error_service = ErrorHandlingService::get_instance();

    std::string details = "Rate limit exceeded. Retry after " + std::to_string(retry_after_seconds) + " seconds.";

    auto error = error_service.create_error(
        "RATE_LIMIT_EXCEEDED",
        "Too many requests",
        context,
        details
    );

    log_api_error("RATE_LIMIT_EXCEEDED", "Rate limit exceeded", context, details);
    track_api_error("RATE_LIMIT_EXCEEDED", context.path);

    auto response = error_service.format_error_response(error);
    response.headers["Retry-After"] = std::to_string(retry_after_seconds);

    return response;
}

HTTPResponse create_internal_error(
    const ErrorContext& context,
    const std::string& message,
    const std::optional<std::string>& details
) {
    auto& error_service = ErrorHandlingService::get_instance();

    auto error = error_service.create_error(
        "INTERNAL_ERROR",
        message,
        context,
        should_include_error_details("INTERNAL_ERROR") ? details : std::nullopt
    );

    log_api_error("INTERNAL_ERROR", message, context, details);
    track_api_error("INTERNAL_ERROR", context.path);

    return error_service.format_error_response(error);
}

HTTPResponse create_service_unavailable_error(
    const ErrorContext& context,
    const std::string& message
) {
    auto& error_service = ErrorHandlingService::get_instance();

    auto error = error_service.create_error(
        "SERVICE_UNAVAILABLE",
        message,
        context
    );

    log_api_error("SERVICE_UNAVAILABLE", message, context);
    track_api_error("SERVICE_UNAVAILABLE", context.path);

    return error_service.format_error_response(error);
}

HTTPResponse create_maintenance_error(
    const ErrorContext& context
) {
    auto& error_service = ErrorHandlingService::get_instance();

    std::string message = "Service is currently under maintenance";
    std::string details = "Scheduled maintenance is in progress. Please try again later.";

    auto error = error_service.create_error(
        "MAINTENANCE_MODE",
        message,
        context,
        details
    );

    log_api_error("MAINTENANCE_MODE", message, context, details);
    track_api_error("MAINTENANCE_MODE", context.path);

    auto response = error_service.format_error_response(error);
    response.headers["Retry-After"] = "3600"; // 1 hour

    return response;
}

HTTPResponse create_database_error(
    const ErrorContext& context,
    const std::string& operation
) {
    std::string message = "Database operation failed";
    std::string details = operation + " could not be completed due to a database error";

    auto& error_service = ErrorHandlingService::get_instance();

    auto error = error_service.create_error(
        "DATABASE_ERROR",
        message,
        context,
        details
    );

    log_api_error("DATABASE_ERROR", message, context, details);
    track_api_error("DATABASE_ERROR", context.path);

    return error_service.format_error_response(error);
}

HTTPResponse create_external_service_error(
    const ErrorContext& context,
    const std::string& service_name,
    const std::string& operation
) {
    std::string message = "External service error";
    std::string details = service_name + " service is unavailable";
    if (!operation.empty()) {
        details += " for " + operation;
    }

    auto& error_service = ErrorHandlingService::get_instance();

    auto error = error_service.create_error(
        "EXTERNAL_SERVICE_ERROR",
        message,
        context,
        details
    );

    log_api_error("EXTERNAL_SERVICE_ERROR", message, context, details);
    track_api_error("EXTERNAL_SERVICE_ERROR", context.path);

    return error_service.format_error_response(error);
}

HTTPResponse create_network_error(
    const ErrorContext& context,
    const std::string& operation
) {
    std::string message = "Network connectivity error";
    std::string details = "Failed to " + operation + " due to network issues";

    auto& error_service = ErrorHandlingService::get_instance();

    auto error = error_service.create_error(
        "NETWORK_ERROR",
        message,
        context,
        details
    );

    log_api_error("NETWORK_ERROR", message, context, details);
    track_api_error("NETWORK_ERROR", context.path);

    return error_service.format_error_response(error);
}

HTTPResponse create_custom_error(
    const std::string& error_code,
    const std::string& message,
    const ErrorContext& context,
    const std::optional<std::string>& details,
    const std::optional<std::string>& field
) {
    auto& error_service = ErrorHandlingService::get_instance();

    auto error = error_service.create_error(
        error_code,
        message,
        context,
        details,
        field
    );

    log_api_error(error_code, message, context, details);
    track_api_error(error_code, context.path);

    return error_service.format_error_response(error);
}

HTTPResponse create_success_response(
    const nlohmann::json& data,
    const std::string& message,
    const std::unordered_map<std::string, std::string>& meta
) {
    HTTPResponse response;
    response.status_code = 200;
    response.content_type = "application/json";

    nlohmann::json response_body;
    response_body["status"] = message;
    response_body["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();

    if (!data.is_null()) {
        response_body["data"] = data;
    }

    if (!meta.empty()) {
        response_body["meta"] = meta;
    }

    response.body = response_body.dump(2);
    return response;
}

HTTPResponse create_created_response(
    const nlohmann::json& data,
    const std::string& message
) {
    HTTPResponse response;
    response.status_code = 201;
    response.content_type = "application/json";

    nlohmann::json response_body;
    response_body["status"] = message;
    response_body["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();

    if (!data.is_null()) {
        response_body["data"] = data;
    }

    response.body = response_body.dump(2);
    return response;
}

HTTPResponse create_no_content_response() {
    HTTPResponse response;
    response.status_code = 204;
    response.content_type = "application/json";
    response.body = "";
    return response;
}

HTTPResponse create_paginated_response(
    const nlohmann::json& items,
    int total_count,
    int page,
    int limit,
    const std::string& base_url
) {
    HTTPResponse response;
    response.status_code = 200;
    response.content_type = "application/json";

    nlohmann::json response_body;
    response_body["status"] = "success";
    response_body["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
    response_body["data"] = items;

    // Add pagination metadata
    nlohmann::json pagination;
    pagination["total_count"] = total_count;
    pagination["page"] = page;
    pagination["limit"] = limit;
    pagination["total_pages"] = (total_count + limit - 1) / limit; // Ceiling division
    pagination["has_next"] = (page * limit) < total_count;
    pagination["has_prev"] = page > 1;

    response_body["meta"]["pagination"] = pagination;

    response.body = response_body.dump(2);
    return response;
}

HTTPResponse create_missing_field_error(
    const ErrorContext& context,
    const std::string& field_name
) {
    std::string message = "Required field is missing";
    std::string details = "The field '" + field_name + "' is required but was not provided";

    return create_validation_error(message, context, field_name, details);
}

HTTPResponse create_invalid_field_error(
    const ErrorContext& context,
    const std::string& field_name,
    const std::string& expected_format,
    const std::string& provided_value
) {
    std::string message = "Field has invalid format";
    std::string details = "Field '" + field_name + "' must be " + expected_format;
    if (!provided_value.empty()) {
        details += ". Provided value: " + provided_value;
    }

    return create_validation_error(message, context, field_name, details);
}

HTTPResponse create_duplicate_resource_error(
    const ErrorContext& context,
    const std::string& resource_type,
    const std::string& identifier
) {
    std::string message = resource_type + " already exists";
    std::string details = "A " + resource_type + " with identifier '" + identifier + "' already exists";

    return create_conflict_error(context, message, details);
}

HTTPResponse create_resource_not_modified_error(
    const ErrorContext& context,
    const std::string& last_modified
) {
    HTTPResponse response;
    response.status_code = 304; // Not Modified
    response.content_type = "application/json";
    response.body = "";
    response.headers["Last-Modified"] = last_modified;

    return response;
}

std::string extract_request_id_from_headers(
    const std::unordered_map<std::string, std::string>& headers
) {
    auto it = headers.find("x-request-id");
    if (it != headers.end()) {
        return it->second;
    }

    it = headers.find("request-id");
    if (it != headers.end()) {
        return it->second;
    }

    // Generate a new request ID if none found
    return ErrorHandlingService::get_instance().generate_error_id();
}

std::string extract_client_ip_from_headers(
    const std::unordered_map<std::string, std::string>& headers
) {
    // Try various headers that might contain the client IP
    std::vector<std::string> ip_headers = {
        "x-forwarded-for",
        "x-real-ip",
        "x-client-ip",
        "cf-connecting-ip",
        "forwarded"
    };

    for (const auto& header : ip_headers) {
        auto it = headers.find(header);
        if (it != headers.end() && !it->second.empty()) {
            // Take the first IP if there are multiple (comma-separated)
            size_t comma_pos = it->second.find(',');
            if (comma_pos != std::string::npos) {
                return it->second.substr(0, comma_pos);
            }
            return it->second;
        }
    }

    return "unknown";
}

std::string extract_user_id_from_context(
    const std::unordered_map<std::string, std::string>& headers
) {
    // Try to extract user ID from JWT token or other auth headers
    // This is a simplified implementation
    auto auth_it = headers.find("authorization");
    if (auth_it != headers.end() && auth_it->second.find("Bearer ") == 0) {
        // In a real implementation, you'd decode the JWT to extract the user ID
        return "authenticated_user";
    }

    return "anonymous";
}

bool should_include_error_details(
    const std::string& error_code,
    const std::string& environment
) {
    // In production, be more restrictive about error details
    if (environment == "production") {
        return error_code == "VALIDATION_ERROR" || error_code == "NOT_FOUND";
    }

    // In development/staging, include more details
    return true;
}

void log_api_error(
    const std::string& error_code,
    const std::string& message,
    const ErrorContext& context,
    const std::optional<std::string>& details
) {
    ErrorHandlingService::get_instance().log_error(
        ErrorHandlingService::get_instance().create_error(
            error_code,
            message,
            context,
            details
        ),
        context
    );
}

void track_api_error(
    const std::string& error_code,
    const std::string& endpoint_path
) {
    ErrorHandlingService::get_instance().track_error_metrics(error_code, endpoint_path);
}

} // namespace regulens
