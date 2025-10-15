/**
 * NL Policy Builder API Handlers Implementation
 * Production-grade REST API endpoints for policy conversion and management
 */

#include "policy_api_handlers.hpp"
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <algorithm>

namespace regulens {
namespace policy {

namespace {

struct AnalyticsCacheEntry {
    std::chrono::steady_clock::time_point expires_at;
    nlohmann::json payload;
};

std::mutex g_analytics_cache_mutex;
std::unordered_map<std::string, AnalyticsCacheEntry> g_analytics_cache;
constexpr std::chrono::seconds kDefaultAnalyticsTtl{300};

double safe_to_double(const nlohmann::json& value, double fallback = 0.0) {
    try {
        if (value.is_number()) {
            return value.get<double>();
        }
        if (value.is_string()) {
            return std::stod(value.get<std::string>());
        }
    } catch (...) {
        return fallback;
    }
    return fallback;
}

std::string build_analytics_cache_key(const std::string& base, const std::map<std::string, std::string>& filters) {
    std::ostringstream oss;
    oss << base;
    for (const auto& [key, value] : filters) {
        oss << '|' << key << '=' << value;
    }
    return oss.str();
}

std::string sanitize_sort_column(const std::string& candidate) {
    static const std::unordered_map<std::string, std::string> allowed = {
        {"created_at", "created_at"},
        {"updated_at", "updated_at"},
        {"confidence_score", "confidence_score"},
        {"status", "status"}
    };

    auto it = allowed.find(candidate);
    if (it != allowed.end()) {
        return it->second;
    }
    return "created_at";
}

std::string sanitize_sort_direction(const std::string& candidate) {
    std::string lower = candidate;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower == "asc" ? "ASC" : "DESC";
}

} // namespace

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
        auto filters = parse_query_parameters(query_params);
        std::string cache_key = build_analytics_cache_key(user_id + ":conversion_analytics", filters);

        if (!filters.empty()) {
            auto cached_result = get_cached_analytics(cache_key);
            if (cached_result) {
                return create_success_response(*cached_result);
            }
        }

        nlohmann::json analytics = calculate_conversion_metrics(user_id, filters);

        if (!filters.empty()) {
            cache_analytics_result(cache_key, analytics, kDefaultAnalyticsTtl.count());
        }

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
    std::vector<nlohmann::json> results;

    try {
        std::map<std::string, std::string> normalized_filters = parse_query_parameters(filters);

        int safe_limit = std::clamp(limit, 1, 200);
        int safe_offset = std::max(0, offset);

        std::ostringstream sql;
        sql << "SELECT conversion_id, user_id, policy_type, status, confidence_score, created_at, updated_at, generated_policy, validation_errors, usage_count, last_used_at "
               "FROM nl_policy_conversions WHERE user_id = $1";

        std::vector<std::string> params;
        params.push_back(user_id);
        int param_index = 2;

        if (normalized_filters.count("status")) {
            sql << " AND status = $" << param_index;
            params.push_back(normalized_filters["status"]);
            ++param_index;
        }

        if (normalized_filters.count("policy_type")) {
            sql << " AND policy_type = $" << param_index;
            params.push_back(normalized_filters["policy_type"]);
            ++param_index;
        }

        if (normalized_filters.count("start_date")) {
            sql << " AND created_at >= $" << param_index << "::timestamptz";
            params.push_back(normalized_filters["start_date"]);
            ++param_index;
        }

        if (normalized_filters.count("end_date")) {
            sql << " AND created_at <= $" << param_index << "::timestamptz";
            params.push_back(normalized_filters["end_date"]);
            ++param_index;
        }

        if (normalized_filters.count("min_confidence")) {
            sql << " AND confidence_score >= $" << param_index << "::numeric";
            params.push_back(normalized_filters["min_confidence"]);
            ++param_index;
        }

        if (normalized_filters.count("max_confidence")) {
            sql << " AND confidence_score <= $" << param_index << "::numeric";
            params.push_back(normalized_filters["max_confidence"]);
            ++param_index;
        }

        if (normalized_filters.count("search")) {
            sql << " AND (natural_language_input ILIKE $" << param_index
                << " OR generated_policy::text ILIKE $" << param_index << ")";
            params.push_back('%' + normalized_filters["search"] + '%');
            ++param_index;
        }

        std::string sort_column = sanitize_sort_column(normalized_filters.count("sort_by") ? normalized_filters["sort_by"] : "created_at");
        std::string sort_direction = sanitize_sort_direction(normalized_filters.count("sort_direction") ? normalized_filters["sort_direction"] : "desc");

        sql << " ORDER BY " << sort_column << ' ' << sort_direction;
        sql << " LIMIT " << safe_limit << " OFFSET " << safe_offset;

        auto rows = db_conn_->execute_query_multi(sql.str(), params);
        results.reserve(rows.size());

        for (const auto& row : rows) {
            nlohmann::json item;
            item["conversion_id"] = row.value("conversion_id", "");
            item["policy_type"] = row.value("policy_type", "");
            item["status"] = row.value("status", "");
            item["confidence_score"] = row.contains("confidence_score") ? safe_to_double(row["confidence_score"]) : 0.0;
            item["created_at"] = row.value("created_at", "");
            item["updated_at"] = row.value("updated_at", "");
            try {
                item["usage_count"] = row.contains("usage_count") && !row["usage_count"].is_null()
                    ? std::stoi(row["usage_count"].get<std::string>())
                    : 0;
            } catch (...) {
                item["usage_count"] = 0;
            }
            item["last_used_at"] = row.value("last_used_at", "");

            const auto generated_policy_raw = row.value("generated_policy", std::string{});
            if (!generated_policy_raw.empty()) {
                try {
                    item["generated_policy"] = nlohmann::json::parse(generated_policy_raw);
                } catch (...) {
                    item["generated_policy"] = generated_policy_raw;
                }
            }

            const auto validation_errors_raw = row.value("validation_errors", std::string{});
            if (!validation_errors_raw.empty()) {
                try {
                    item["validation_errors"] = nlohmann::json::parse(validation_errors_raw);
                } catch (...) {
                    item["validation_errors"] = nlohmann::json::array();
                }
            }

            results.push_back(std::move(item));
        }

        return results;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in query_conversions_paginated: " + std::string(e.what()));
        return results;
    }
}

std::optional<nlohmann::json> PolicyAPIHandlers::query_conversion_details(const std::string& conversion_id) {
    try {
        auto conversion_row = db_conn_->execute_query_single(
            "SELECT conversion_id, user_id, natural_language_input, generated_policy, policy_type, confidence_score, status, created_at, updated_at, validation_errors, usage_count, last_used_at "
            "FROM nl_policy_conversions WHERE conversion_id = $1",
            {conversion_id}
        );

        if (!conversion_row) {
            return std::nullopt;
        }

        nlohmann::json details;
        const auto& row = *conversion_row;

        details["conversion_id"] = row.value("conversion_id", "");
        details["user_id"] = row.value("user_id", "");
        details["policy_type"] = row.value("policy_type", "");
        details["status"] = row.value("status", "");
        details["created_at"] = row.value("created_at", "");
        details["updated_at"] = row.value("updated_at", "");
        try {
            details["usage_count"] = std::stoi(row.value("usage_count", "0"));
        } catch (...) {
            details["usage_count"] = 0;
        }
        details["last_used_at"] = row.value("last_used_at", "");
        details["natural_language_input"] = row.value("natural_language_input", "");
        details["confidence_score"] = row.contains("confidence_score") ? safe_to_double(row["confidence_score"]) : 0.0;

        const auto generated_policy_raw = row.value("generated_policy", std::string{});
        if (!generated_policy_raw.empty()) {
            try {
                details["generated_policy"] = nlohmann::json::parse(generated_policy_raw);
            } catch (...) {
                details["generated_policy"] = generated_policy_raw;
            }
        }

        const auto validation_errors_raw = row.value("validation_errors", std::string{});
        if (!validation_errors_raw.empty()) {
            try {
                details["validation_errors"] = nlohmann::json::parse(validation_errors_raw);
            } catch (...) {
                details["validation_errors"] = nlohmann::json::array();
            }
        }

        auto deployments = db_conn_->execute_query_multi(
            "SELECT deployment_id, target_system, target_table, deployment_status, deployed_policy, deployed_at, rollback_at, rollback_reason "
            "FROM policy_deployments WHERE conversion_id = $1 ORDER BY deployed_at DESC NULLS LAST LIMIT 20",
            {conversion_id}
        );

        nlohmann::json deployment_array = nlohmann::json::array();
        for (const auto& deployment_row : deployments) {
            nlohmann::json deployment;
            deployment["deployment_id"] = deployment_row.value("deployment_id", "");
            deployment["target_system"] = deployment_row.value("target_system", "");
            deployment["target_table"] = deployment_row.value("target_table", "");
            deployment["deployment_status"] = deployment_row.value("deployment_status", "");
            deployment["deployed_at"] = deployment_row.value("deployed_at", "");
            deployment["rollback_at"] = deployment_row.value("rollback_at", "");
            deployment["rollback_reason"] = deployment_row.value("rollback_reason", "");

            try {
            const auto deployed_policy_raw = deployment_row.value("deployed_policy", std::string{});
            if (!deployed_policy_raw.empty()) {
                try {
                    deployment["deployed_policy"] = nlohmann::json::parse(deployed_policy_raw);
                } catch (...) {
                    deployment["deployed_policy"] = deployed_policy_raw;
                }
            }
            } catch (...) {
                deployment["deployed_policy"] = deployment_row.value("deployed_policy", "");
            }

            deployment_array.push_back(std::move(deployment));
        }

        details["deployments"] = deployment_array;
        return details;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in query_conversion_details: " + std::string(e.what()));
        return std::nullopt;
    }
}

nlohmann::json PolicyAPIHandlers::calculate_conversion_metrics(const std::string& user_id, const std::map<std::string, std::string>& filters) {
    try {
        std::map<std::string, std::string> normalized_filters = parse_query_parameters(filters);

        auto overview_row = db_conn_->execute_query_single(
            "SELECT COUNT(*) AS total_conversions, "
            "COUNT(*) FILTER (WHERE status = 'approved') AS approved_conversions, "
            "COUNT(*) FILTER (WHERE status = 'rejected') AS rejected_conversions, "
            "AVG(confidence_score) AS avg_confidence, "
            "MAX(updated_at) AS last_updated "
            "FROM nl_policy_conversions WHERE user_id = $1",
            {user_id}
        );

        nlohmann::json metrics;
        if (overview_row) {
            auto parse_int = [](const nlohmann::json& value) {
                try {
                    if (value.is_string()) {
                        return std::stoi(value.get<std::string>());
                    }
                } catch (...) {}
                return 0;
            };

            metrics["total_conversions"] = parse_int((*overview_row)["total_conversions"]);
            metrics["approved_conversions"] = parse_int((*overview_row)["approved_conversions"]);
            metrics["rejected_conversions"] = parse_int((*overview_row)["rejected_conversions"]);
            metrics["average_confidence"] = overview_row->contains("avg_confidence") ? safe_to_double((*overview_row)["avg_confidence"]) : 0.0;
            metrics["last_updated_at"] = overview_row->value("last_updated", "");
        }

        auto recent_row = db_conn_->execute_query_single(
            "SELECT COUNT(*) AS recent_conversions, AVG(confidence_score) AS recent_confidence "
            "FROM nl_policy_conversions WHERE user_id = $1 AND created_at >= NOW() - INTERVAL '30 days'",
            {user_id}
        );

        if (recent_row) {
            try {
                metrics["recent_conversions"] = std::stoi(recent_row->value("recent_conversions", "0"));
            } catch (...) {
                metrics["recent_conversions"] = 0;
            }
            metrics["recent_average_confidence"] = recent_row->contains("recent_confidence") ? safe_to_double((*recent_row)["recent_confidence"]) : 0.0;
        }

        auto status_breakdown = db_conn_->execute_query_multi(
            "SELECT status, COUNT(*) AS count FROM nl_policy_conversions WHERE user_id = $1 GROUP BY status",
            {user_id}
        );

        nlohmann::json status_metrics = nlohmann::json::object();
        for (const auto& row : status_breakdown) {
            int count = 0;
            try {
                count = std::stoi(row.value("count", "0"));
            } catch (...) {}
            status_metrics[row.value("status", "unknown")] = count;
        }
        metrics["status_breakdown"] = status_metrics;

        auto type_breakdown = db_conn_->execute_query_multi(
            "SELECT policy_type, COUNT(*) AS count, AVG(confidence_score) AS avg_confidence "
            "FROM nl_policy_conversions WHERE user_id = $1 GROUP BY policy_type",
            {user_id}
        );

        nlohmann::json policy_type_metrics = nlohmann::json::array();
        for (const auto& row : type_breakdown) {
            nlohmann::json entry;
            entry["policy_type"] = row.value("policy_type", "");
            try {
                entry["count"] = std::stoi(row.value("count", "0"));
            } catch (...) {
                entry["count"] = 0;
            }
            entry["average_confidence"] = row.contains("avg_confidence") ? safe_to_double(row["avg_confidence"]) : 0.0;
            policy_type_metrics.push_back(std::move(entry));
        }
        metrics["policy_type_breakdown"] = policy_type_metrics;

        if (!normalized_filters.empty()) {
            std::string cache_key = build_analytics_cache_key(user_id + ":conversion_metrics", normalized_filters);
            cache_analytics_result(cache_key, metrics, kDefaultAnalyticsTtl.count());
        }

        return metrics;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in calculate_conversion_metrics: " + std::string(e.what()));
        return nlohmann::json::object();
    }
}

nlohmann::json PolicyAPIHandlers::calculate_template_popularity() {
    try {
        auto rows = db_conn_->execute_query_multi(
            "SELECT template_id, template_name, category, usage_count, success_rate, average_confidence "
            "FROM policy_templates WHERE is_active = true ORDER BY usage_count DESC, success_rate DESC NULLS LAST LIMIT 25",
            {}
        );

        nlohmann::json templates = nlohmann::json::array();
        for (const auto& row : rows) {
            nlohmann::json entry;
            entry["template_id"] = row.value("template_id", "");
            entry["template_name"] = row.value("template_name", "");
            entry["category"] = row.value("category", "");
            try {
                entry["usage_count"] = std::stoi(row.value("usage_count", "0"));
            } catch (...) {
                entry["usage_count"] = 0;
            }
            entry["success_rate"] = row.contains("success_rate") ? safe_to_double(row["success_rate"]) : 0.0;
            entry["average_confidence"] = row.contains("average_confidence") ? safe_to_double(row["average_confidence"]) : 0.0;
            templates.push_back(std::move(entry));
        }

        return templates;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in calculate_template_popularity: " + std::string(e.what()));
        return nlohmann::json::array();
    }
}

nlohmann::json PolicyAPIHandlers::calculate_policy_type_success_rates() {
    try {
        auto rows = db_conn_->execute_query_multi(
            "SELECT policy_type, COUNT(*) AS total, "
            "COUNT(*) FILTER (WHERE status = 'approved') AS approved, "
            "COUNT(*) FILTER (WHERE status = 'rejected') AS rejected, "
            "AVG(confidence_score) AS avg_confidence "
            "FROM nl_policy_conversions GROUP BY policy_type",
            {}
        );

        nlohmann::json data = nlohmann::json::array();
        for (const auto& row : rows) {
            nlohmann::json entry;
            entry["policy_type"] = row.value("policy_type", "");
            try {
                entry["total"] = std::stoi(row.value("total", "0"));
            } catch (...) {
                entry["total"] = 0;
            }
            try {
                entry["approved"] = std::stoi(row.value("approved", "0"));
            } catch (...) {
                entry["approved"] = 0;
            }
            try {
                entry["rejected"] = std::stoi(row.value("rejected", "0"));
            } catch (...) {
                entry["rejected"] = 0;
            }
            entry["avg_confidence"] = row.contains("avg_confidence") ? safe_to_double(row["avg_confidence"]) : 0.0;
            data.push_back(std::move(entry));
        }

        return data;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in calculate_policy_type_success_rates: " + std::string(e.what()));
        return nlohmann::json::array();
    }
}

bool PolicyAPIHandlers::check_conversion_rate_limit(const std::string& user_id) {
    try {
        auto row = db_conn_->execute_query_single(
            "SELECT COUNT(*) AS recent_requests FROM nl_policy_conversions WHERE user_id = $1 AND created_at >= NOW() - INTERVAL '1 minute'",
            {user_id}
        );

        int threshold_per_minute = 20;
        if (row && row->contains("recent_requests")) {
            int requests = 0;
            try {
                requests = std::stoi(row->value("recent_requests", "0"));
            } catch (...) {
                requests = 0;
            }
            if (requests >= threshold_per_minute) {
                return false;
            }
        }

        return true;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::WARN, "Rate limit check failed, defaulting to allow: " + std::string(e.what()));
        return true;
    }
}

void PolicyAPIHandlers::record_conversion_attempt(const std::string& user_id) {
    try {
        nlohmann::json parameters = {
            {"user_id", user_id}
        };
        nlohmann::json result = {
            {"action", "conversion_attempt"}
        };

        db_conn_->execute_command(
            "INSERT INTO tool_usage_logs (tool_name, parameters, result, success, execution_time_ms) "
            "VALUES ($1, $2::jsonb, $3::jsonb, $4::boolean, $5)",
            {"policy_conversion", parameters.dump(), result.dump(), "true", "0"}
        );

    } catch (const std::exception& e) {
        logger_->log(LogLevel::WARN, "Failed to record conversion attempt: " + std::string(e.what()));
    }
}

std::optional<nlohmann::json> PolicyAPIHandlers::get_cached_analytics(const std::string& cache_key) {
    std::lock_guard<std::mutex> lock(g_analytics_cache_mutex);
    auto it = g_analytics_cache.find(cache_key);
    if (it == g_analytics_cache.end()) {
        return std::nullopt;
    }

    if (std::chrono::steady_clock::now() > it->second.expires_at) {
        g_analytics_cache.erase(it);
        return std::nullopt;
    }

    return it->second.payload;
}

void PolicyAPIHandlers::cache_analytics_result(const std::string& cache_key, const nlohmann::json& data, int ttl_seconds) {
    std::lock_guard<std::mutex> lock(g_analytics_cache_mutex);
    AnalyticsCacheEntry entry;
    entry.payload = data;
    entry.expires_at = std::chrono::steady_clock::now() + std::chrono::seconds(ttl_seconds > 0 ? ttl_seconds : kDefaultAnalyticsTtl.count());
    g_analytics_cache[cache_key] = std::move(entry);
}

std::map<std::string, std::string> PolicyAPIHandlers::parse_query_parameters(const std::map<std::string, std::string>& query_params) {
    std::map<std::string, std::string> normalized;

    for (const auto& [key, value] : query_params) {
        if (value.empty()) {
            continue;
        }

        if (key == "status" || key == "policy_type" || key == "search") {
            normalized[key] = value;
        } else if (key == "start_date" || key == "end_date") {
            normalized[key] = value;
        } else if (key == "min_confidence" || key == "max_confidence") {
            try {
                double numeric = std::stod(value);
                numeric = std::clamp(numeric, 0.0, 1.0);
                normalized[key] = std::to_string(numeric);
            } catch (...) {}
        } else if (key == "sort_by") {
            normalized[key] = value;
        } else if (key == "sort_direction") {
            normalized[key] = value;
        }
    }

    return normalized;
}

} // namespace policy
} // namespace regulens
