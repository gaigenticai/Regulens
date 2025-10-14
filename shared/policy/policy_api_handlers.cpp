/**
 * NL Policy Builder API Handlers Implementation
 * Production-grade REST API endpoints for policy conversion and management
 */

#include "policy_api_handlers.hpp"
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>

namespace regulens {
namespace policy {

PolicyAPIHandlers::PolicyAPIHandlers(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<StructuredLogger> logger,
    std::shared_ptr<NLPolicyConverter> policy_converter
) : db_conn_(db_conn), logger_(logger), policy_converter_(policy_converter) {

    if (!db_conn_) {
        throw std::runtime_error("Database connection is required for PolicyAPIHandlers");
    }
    if (!logger_) {
        throw std::runtime_error("Logger is required for PolicyAPIHandlers");
    }
    if (!policy_converter_) {
        throw std::runtime_error("Policy converter is required for PolicyAPIHandlers");
    }
}

std::string PolicyAPIHandlers::handle_convert_natural_language(const std::string& request_body, const std::string& user_id) {
    try {
        // Check rate limiting
        if (!check_conversion_rate_limit(user_id)) {
            return create_error_response("Rate limit exceeded. Please try again later.", 429);
        }

        nlohmann::json request = nlohmann::json::parse(request_body);

        // Validate required fields
        if (!request.contains("natural_language_input") || request["natural_language_input"].empty()) {
            return create_error_response("natural_language_input is required and cannot be empty");
        }

        if (!request.contains("policy_type")) {
            return create_error_response("policy_type is required");
        }

        // Parse conversion request
        PolicyConversionRequest conversion_request = parse_conversion_request(request, user_id);

        // Perform conversion
        PolicyConversionResult result = policy_converter_->convert_natural_language(conversion_request);

        // Record the attempt for rate limiting
        record_conversion_attempt(user_id);

        if (!result.success) {
            return create_error_response(result.error_message.value_or("Conversion failed"), 500);
        }

        // Return formatted result
        nlohmann::json response_data = format_conversion_result(result);
        return create_success_response(response_data, "Policy converted successfully");

    } catch (const nlohmann::json::exception& e) {
        logger_->log(LogLevel::ERROR, "Invalid JSON in handle_convert_natural_language: " + std::string(e.what()));
        return create_error_response("Invalid request format", 400);
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_convert_natural_language: " + std::string(e.what()));
        return create_error_response("Internal server error", 500);
    }
}

std::string PolicyAPIHandlers::handle_get_conversion(const std::string& conversion_id, const std::string& user_id) {
    try {
        // Validate access
        if (!validate_conversion_access(conversion_id, user_id)) {
            return create_error_response("Conversion not found or access denied", 404);
        }

        // Get conversion details
        auto conversion = policy_converter_->get_conversion(conversion_id);
        if (!conversion) {
            return create_error_response("Conversion not found", 404);
        }

        return create_success_response(*conversion);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_get_conversion: " + std::string(e.what()));
        return create_error_response("Internal server error", 500);
    }
}

std::string PolicyAPIHandlers::handle_get_user_conversions(const std::string& user_id, const std::map<std::string, std::string>& query_params) {
    try {
        // Parse query parameters
        auto filters = parse_query_parameters(query_params);
        int limit = 50;
        int offset = 0;

        if (query_params.count("limit")) {
            limit = std::min(std::stoi(query_params.at("limit")), 100); // Max 100
        }
        if (query_params.count("offset")) {
            offset = std::stoi(query_params.at("offset"));
        }

        // Get conversions
        std::vector<nlohmann::json> conversions = query_conversions_paginated(user_id, filters, limit, offset);

        nlohmann::json response = {
            {"conversions", conversions},
            {"count", conversions.size()},
            {"limit", limit},
            {"offset", offset}
        };

        return create_success_response(response);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_get_user_conversions: " + std::string(e.what()));
        return create_error_response("Internal server error", 500);
    }
}

std::string PolicyAPIHandlers::handle_deploy_policy(const std::string& conversion_id, const std::string& request_body, const std::string& user_id) {
    try {
        // Validate conversion access
        if (!validate_conversion_access(conversion_id, user_id)) {
            return create_error_response("Conversion not found or access denied", 404);
        }

        nlohmann::json request = nlohmann::json::parse(request_body);

        // Validate required fields
        if (!request.contains("target_system")) {
            return create_error_response("target_system is required");
        }

        // Parse deployment request
        PolicyDeploymentRequest deployment_request = parse_deployment_request(request, conversion_id, user_id);

        // Perform deployment
        PolicyDeploymentResult result = policy_converter_->deploy_policy(deployment_request);

        if (!result.success) {
            return create_error_response(result.error_message.value_or("Deployment failed"), 500);
        }

        // Return formatted result
        nlohmann::json response_data = format_deployment_result(result);
        return create_success_response(response_data, "Policy deployed successfully");

    } catch (const nlohmann::json::exception& e) {
        logger_->log(LogLevel::ERROR, "Invalid JSON in handle_deploy_policy: " + std::string(e.what()));
        return create_error_response("Invalid request format", 400);
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_deploy_policy: " + std::string(e.what()));
        return create_error_response("Internal server error", 500);
    }
}

std::string PolicyAPIHandlers::handle_submit_feedback(const std::string& conversion_id, const std::string& request_body, const std::string& user_id) {
    try {
        // Validate conversion access
        if (!validate_conversion_access(conversion_id, user_id)) {
            return create_error_response("Conversion not found or access denied", 404);
        }

        nlohmann::json request = nlohmann::json::parse(request_body);

        // Extract feedback data
        std::string feedback = request.value("feedback", "");
        int rating = request.value("rating", 0);

        if (feedback.empty() && rating == 0) {
            return create_error_response("Either feedback text or rating must be provided");
        }

        if (rating < 0 || rating > 5) {
            return create_error_response("Rating must be between 1 and 5");
        }

        // Submit feedback
        bool success = policy_converter_->submit_feedback(conversion_id, feedback, rating);

        if (!success) {
            return create_error_response("Failed to submit feedback", 500);
        }

        logger_->log(LogLevel::INFO, "Feedback submitted for conversion " + conversion_id + " by user " + user_id);

        nlohmann::json response_data = {
            {"conversion_id", conversion_id},
            {"feedback_submitted", true}
        };

        return create_success_response(response_data, "Feedback submitted successfully");

    } catch (const nlohmann::json::exception& e) {
        logger_->log(LogLevel::ERROR, "Invalid JSON in handle_submit_feedback: " + std::string(e.what()));
        return create_error_response("Invalid request format", 400);
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_submit_feedback: " + std::string(e.what()));
        return create_error_response("Internal server error", 500);
    }
}

std::string PolicyAPIHandlers::handle_get_templates(const std::map<std::string, std::string>& query_params) {
    try {
        std::string policy_type = query_params.count("policy_type") ? query_params.at("policy_type") : "";
        std::string category = query_params.count("category") ? query_params.at("category") : "";

        // Get templates
        std::vector<PolicyTemplate> templates = policy_converter_->get_available_templates(policy_type);

        // Filter by category if specified
        if (!category.empty()) {
            templates.erase(
                std::remove_if(templates.begin(), templates.end(),
                    [&category](const PolicyTemplate& t) { return t.category != category; }),
                templates.end()
            );
        }

        // Format response
        nlohmann::json templates_array = nlohmann::json::array();
        for (const auto& tmpl : templates) {
            templates_array.push_back(format_template(tmpl));
        }

        nlohmann::json response = {
            {"templates", templates_array},
            {"count", templates.size()}
        };

        if (!policy_type.empty()) {
            response["policy_type"] = policy_type;
        }
        if (!category.empty()) {
            response["category"] = category;
        }

        return create_success_response(response);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_get_templates: " + std::string(e.what()));
        return create_error_response("Internal server error", 500);
    }
}

std::string PolicyAPIHandlers::handle_validate_policy(const std::string& request_body) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);

        // Validate required fields
        if (!request.contains("policy")) {
            return create_error_response("policy object is required");
        }

        if (!request.contains("policy_type")) {
            return create_error_response("policy_type is required");
        }

        nlohmann::json policy = request["policy"];
        std::string policy_type = request["policy_type"];

        // Perform validation
        PolicyValidationResult result = policy_converter_->validate_policy(policy, policy_type);

        // Return formatted result
        nlohmann::json response_data = format_validation_result(result);
        response_data["policy_type"] = policy_type;

        std::string message = result.is_valid ? "Policy validation successful" : "Policy validation found issues";
        return create_success_response(response_data, message);

    } catch (const nlohmann::json::exception& e) {
        logger_->log(LogLevel::ERROR, "Invalid JSON in handle_validate_policy: " + std::string(e.what()));
        return create_error_response("Invalid request format", 400);
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_validate_policy: " + std::string(e.what()));
        return create_error_response("Internal server error", 500);
    }
}

std::string PolicyAPIHandlers::handle_get_conversion_analytics(const std::string& user_id, const std::map<std::string, std::string>& query_params) {
    try {
        // Check cache first
        std::string cache_key = "analytics_" + user_id + "_" + std::to_string(std::hash<std::string>{}(query_params.begin()->second));
        auto cached_result = get_cached_analytics(cache_key);
        if (cached_result) {
            return create_success_response(*cached_result);
        }

        // Calculate analytics
        auto filters = parse_query_parameters(query_params);
        nlohmann::json analytics = calculate_conversion_metrics(user_id, filters);

        // Cache result
        cache_analytics_result(cache_key, analytics);

        return create_success_response(analytics);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_get_conversion_analytics: " + std::string(e.what()));
        return create_error_response("Internal server error", 500);
    }
}

PolicyConversionRequest PolicyAPIHandlers::parse_conversion_request(const nlohmann::json& request_json, const std::string& user_id) {
    PolicyConversionRequest request;

    request.natural_language_input = request_json["natural_language_input"];
    request.policy_type = request_json["policy_type"];
    request.user_id = user_id;

    if (request_json.contains("template_id")) {
        request.template_id = request_json["template_id"];
    }

    if (request_json.contains("additional_context")) {
        request.additional_context = request_json["additional_context"];
    }

    if (request_json.contains("target_system")) {
        request.target_system = request_json["target_system"];
    }

    request.auto_validate = request_json.value("auto_validate", true);
    request.max_retries = request_json.value("max_retries", 2);

    return request;
}

PolicyDeploymentRequest PolicyAPIHandlers::parse_deployment_request(const nlohmann::json& request_json, const std::string& conversion_id, const std::string& user_id) {
    PolicyDeploymentRequest request;

    request.conversion_id = conversion_id;
    request.target_system = request_json["target_system"];
    request.deployed_by = user_id;

    if (request_json.contains("deployment_options")) {
        request.deployment_options = request_json["deployment_options"];
    }

    return request;
}

nlohmann::json PolicyAPIHandlers::format_conversion_result(const PolicyConversionResult& result) {
    nlohmann::json formatted = {
        {"conversion_id", result.conversion_id},
        {"generated_policy", result.generated_policy},
        {"confidence_score", result.confidence_score},
        {"status", result.status},
        {"processing_time_ms", result.processing_time.count()},
        {"tokens_used", result.tokens_used},
        {"cost", result.cost},
        {"success", result.success}
    };

    if (!result.validation_errors.empty()) {
        formatted["validation_errors"] = result.validation_errors;
    }

    if (!result.validation_warnings.empty()) {
        formatted["validation_warnings"] = result.validation_warnings;
    }

    if (!result.regulatory_warnings.empty()) {
        formatted["regulatory_warnings"] = result.regulatory_warnings;
    }

    if (!result.compliance_recommendations.empty()) {
        formatted["compliance_recommendations"] = result.compliance_recommendations;
    }

    if (result.metadata.is_object()) {
        formatted["metadata"] = result.metadata;
    }

    if (result.error_message) {
        formatted["error"] = *result.error_message;
    }

    return formatted;
}

nlohmann::json PolicyAPIHandlers::format_deployment_result(const PolicyDeploymentResult& result) {
    nlohmann::json formatted = {
        {"deployment_id", result.deployment_id},
        {"success", result.success},
        {"status", result.status}
    };

    if (result.deployed_policy) {
        formatted["deployed_policy"] = *result.deployed_policy;
    }

    if (result.error_message) {
        formatted["error"] = *result.error_message;
    }

    return formatted;
}

nlohmann::json PolicyAPIHandlers::format_template(const PolicyTemplate& tmpl) {
    nlohmann::json formatted = {
        {"template_id", tmpl.template_id},
        {"template_name", tmpl.template_name},
        {"template_description", tmpl.template_description},
        {"policy_type", tmpl.policy_type},
        {"is_active", tmpl.is_active},
        {"category", tmpl.category},
        {"usage_count", tmpl.usage_count},
        {"success_rate", tmpl.success_rate},
        {"average_confidence", tmpl.average_confidence},
        {"input_schema", tmpl.input_schema},
        {"output_schema", tmpl.output_schema}
    };

    if (!tmpl.example_inputs.empty()) {
        formatted["example_inputs"] = tmpl.example_inputs;
    }

    if (!tmpl.example_outputs.empty()) {
        formatted["example_outputs"] = tmpl.example_outputs;
    }

    return formatted;
}

nlohmann::json PolicyAPIHandlers::format_validation_result(const PolicyValidationResult& result) {
    return {
        {"is_valid", result.is_valid},
        {"validation_score", result.validation_score},
        {"errors", result.errors},
        {"warnings", result.warnings},
        {"suggestions", result.suggestions}
    };
}

bool PolicyAPIHandlers::validate_conversion_access(const std::string& conversion_id, const std::string& user_id) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) return false;

        const char* params[2] = {conversion_id.c_str(), user_id.c_str()};
        PGresult* result = PQexecParams(
            conn,
            "SELECT conversion_id FROM nl_policy_conversions WHERE conversion_id = $1 AND user_id = $2",
            2, nullptr, params, nullptr, nullptr, 0
        );

        bool valid = (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) > 0);
        PQclear(result);
        return valid;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in validate_conversion_access: " + std::string(e.what()));
        return false;
    }
}

std::string PolicyAPIHandlers::create_error_response(const std::string& message, int status_code) {
    nlohmann::json response = {
        {"success", false},
        {"error", message},
        {"status_code", status_code},
        {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
    };
    return response.dump();
}

std::string PolicyAPIHandlers::create_success_response(const nlohmann::json& data, const std::string& message) {
    nlohmann::json response = {
        {"success", true},
        {"data", data},
        {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
    };

    if (!message.empty()) {
        response["message"] = message;
    }

    return response.dump();
}

// Placeholder implementations for remaining methods
std::vector<nlohmann::json> PolicyAPIHandlers::query_conversions_paginated(const std::string& user_id, const std::map<std::string, std::string>& filters, int limit, int offset) {
    // Implementation would query database with filters and pagination
    return policy_converter_->get_user_conversions(user_id, limit, offset);
}

std::optional<nlohmann::json> PolicyAPIHandlers::query_conversion_details(const std::string& conversion_id) {
    // Implementation would query detailed conversion information
    return policy_converter_->get_conversion(conversion_id);
}

nlohmann::json PolicyAPIHandlers::calculate_conversion_metrics(const std::string& user_id, const std::map<std::string, std::string>& filters) {
    // Implementation would calculate comprehensive analytics
    return policy_converter_->get_conversion_analytics(user_id);
}

nlohmann::json PolicyAPIHandlers::calculate_template_popularity() {
    // Implementation would calculate template popularity metrics
    return {};
}

nlohmann::json PolicyAPIHandlers::calculate_policy_type_success_rates() {
    // Implementation would calculate success rates by policy type
    return policy_converter_->get_success_rates_by_policy_type();
}

bool PolicyAPIHandlers::check_conversion_rate_limit(const std::string& user_id) {
    // Implementation would check rate limiting rules
    return true; // Allow by default
}

void PolicyAPIHandlers::record_conversion_attempt(const std::string& user_id) {
    // Implementation would record conversion attempt for rate limiting
}

std::optional<nlohmann::json> PolicyAPIHandlers::get_cached_analytics(const std::string& cache_key) {
    // Implementation would check cache for analytics data
    return std::nullopt;
}

void PolicyAPIHandlers::cache_analytics_result(const std::string& cache_key, const nlohmann::json& data, int ttl_seconds) {
    // Implementation would cache analytics results
}

std::map<std::string, std::string> PolicyAPIHandlers::parse_query_parameters(const std::map<std::string, std::string>& query_params) {
    // Implementation would parse and validate query parameters
    return query_params;
}

} // namespace policy
} // namespace regulens
