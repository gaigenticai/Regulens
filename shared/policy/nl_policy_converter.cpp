/**
 * NL Policy Converter Implementation
 * Production-grade natural language to policy conversion with LLM integration
 */

#include "nl_policy_converter.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <uuid/uuid.h>
#include <spdlog/spdlog.h>
#include <regex>

namespace regulens {
namespace policy {

NLPolicyConverter::NLPolicyConverter(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<OpenAIClient> openai_client,
    std::shared_ptr<StructuredLogger> logger
) : db_conn_(db_conn), openai_client_(openai_client), logger_(logger) {

    if (!db_conn_) {
        throw std::runtime_error("Database connection is required for NLPolicyConverter");
    }
    if (!openai_client_) {
        throw std::runtime_error("OpenAI client is required for NLPolicyConverter");
    }
    if (!logger_) {
        throw std::runtime_error("Logger is required for NLPolicyConverter");
    }

    logger_->log(LogLevel::INFO, "NLPolicyConverter initialized with LLM integration");
}

NLPolicyConverter::~NLPolicyConverter() {
    logger_->log(LogLevel::INFO, "NLPolicyConverter shutting down");
}

PolicyConversionResult NLPolicyConverter::convert_natural_language(const PolicyConversionRequest& request) {
    auto start_time = std::chrono::high_resolution_clock::now();

    try {
        log_conversion_attempt(request);

        // Validate request
        if (request.natural_language_input.empty()) {
            return create_fallback_result("Natural language input cannot be empty");
        }

        if (!is_valid_policy_type(request.policy_type)) {
            return create_fallback_result("Invalid policy type: " + request.policy_type);
        }

        // Get template (use default if not specified)
        PolicyTemplate tmpl;
        if (request.template_id) {
            auto template_opt = get_template(*request.template_id);
            if (!template_opt) {
                return create_fallback_result("Template not found: " + *request.template_id);
            }
            tmpl = *template_opt;
        } else {
            // Use default template for policy type
            auto templates = get_available_templates(request.policy_type);
            if (templates.empty()) {
                return create_fallback_result("No templates available for policy type: " + request.policy_type);
            }
            tmpl = templates[0]; // Use first available template
        }

        // Build conversion prompt
        std::string prompt = build_conversion_prompt(request, tmpl);

        // Call LLM for conversion
        PolicyConversionResult result = call_llm_for_conversion(request, prompt);
        result.template_used = tmpl.template_id;

        // Validate generated policy
        if (result.success && request.auto_validate) {
            PolicyValidationResult validation = validate_policy(result.generated_policy, request.policy_type);
            result.validation_errors = validation.errors;
            result.validation_warnings = validation.warnings;

            if (!validation.is_valid) {
                result.confidence_score *= 0.5; // Reduce confidence for invalid policies
                result.status = "draft"; // Keep as draft if validation fails
            }
        }

        // Calculate processing time
        auto end_time = std::chrono::high_resolution_clock::now();
        result.processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        // Store conversion result
        std::string conversion_id = store_conversion_result(request, result);
        result.conversion_id = conversion_id;

        // Update template statistics
        update_template_statistics(tmpl.template_id, result.success, result.confidence_score);

        if (result.success) {
            log_conversion_success(result);
        } else {
            log_conversion_failure(request, result.error_message.value_or("Unknown error"));
        }

        return result;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in convert_natural_language: " + std::string(e.what()));

        auto end_time = std::chrono::high_resolution_clock::now();
        auto processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        PolicyConversionResult error_result = create_fallback_result("Internal conversion error");
        error_result.processing_time = processing_time;
        return error_result;
    }
}

std::string NLPolicyConverter::build_conversion_prompt(const PolicyConversionRequest& request, const PolicyTemplate& tmpl) {
    std::stringstream prompt;

    prompt << "You are an expert policy analyst specializing in converting natural language policy descriptions into structured, machine-readable policy definitions.\n\n";

    prompt << "POLICY TYPE: " << request.policy_type << "\n";
    prompt << "TEMPLATE: " << tmpl.template_name << "\n\n";

    if (!tmpl.template_description.empty()) {
        prompt << "TEMPLATE DESCRIPTION: " << tmpl.template_description << "\n\n";
    }

    prompt << "CONVERSION REQUIREMENTS:\n";
    prompt << "1. Analyze the natural language input carefully\n";
    prompt << "2. Extract key policy elements (conditions, actions, thresholds, etc.)\n";
    prompt << "3. Structure the output according to the expected schema\n";
    prompt << "4. Ensure all required fields are present and valid\n";
    prompt << "5. Use appropriate data types and formats\n";
    prompt << "6. Include clear, descriptive names for all elements\n\n";

    // Add output schema information
    if (!tmpl.output_schema.empty()) {
        prompt << "REQUIRED OUTPUT SCHEMA:\n";
        prompt << tmpl.output_schema.dump(2) << "\n\n";
    }

    // Add examples if available
    if (!tmpl.example_inputs.empty() && !tmpl.example_outputs.empty()) {
        prompt << "EXAMPLES:\n";
        for (size_t i = 0; i < std::min(tmpl.example_inputs.size(), tmpl.example_outputs.size()); ++i) {
            prompt << "Input: " << tmpl.example_inputs[i] << "\n";
            prompt << "Output: " << tmpl.example_outputs[i].dump(2) << "\n\n";
        }
    }

    prompt << "NATURAL LANGUAGE INPUT TO CONVERT:\n";
    prompt << "\"" << request.natural_language_input << "\"\n\n";

    if (request.additional_context) {
        prompt << "ADDITIONAL CONTEXT:\n";
        prompt << request.additional_context->dump(2) << "\n\n";
    }

    prompt << "OUTPUT FORMAT:\n";
    prompt << "Return a valid JSON object that matches the required schema. Include a confidence score (0.0-1.0) indicating how well the conversion captures the intent of the natural language input.\n\n";

    prompt << "IMPORTANT: Ensure the output is valid JSON and contains all required fields for a " << request.policy_type << " policy.\n";

    return prompt.str();
}

PolicyConversionResult NLPolicyConverter::call_llm_for_conversion(const PolicyConversionRequest& request, const std::string& prompt) {
    PolicyConversionResult result;

    try {
        // Prepare OpenAI request
        nlohmann::json openai_request = {
            {"model", default_model_},
            {"messages", nlohmann::json::array({
                {{"role", "system"}, {"content", "You are a policy conversion expert. Always respond with valid JSON."}},
                {{"role", "user"}, {"content", prompt}}
            })},
            {"temperature", 0.1}, // Low temperature for consistent policy generation
            {"max_tokens", 2000},
            {"presence_penalty", 0.0},
            {"frequency_penalty", 0.0},
            {"response_format", {{"type", "json_object"}}}
        };

        // Call OpenAI API
        auto openai_response = openai_client_->chat_completion(openai_request);

        if (openai_response.contains("error")) {
            result.error_message = openai_response["error"]["message"];
            result.success = false;
            return result;
        }

        // Parse response
        std::string llm_response = openai_response["choices"][0]["message"]["content"];

        // Extract usage information
        if (openai_response.contains("usage")) {
            result.tokens_used = openai_response["usage"]["total_tokens"];
            result.cost = calculate_message_cost(
                default_model_,
                openai_response["usage"]["prompt_tokens"],
                openai_response["usage"]["completion_tokens"]
            );
        }

        // Parse LLM response
        PolicyConversionResult parsed_result = parse_llm_response(llm_response, request);

        // Copy parsed data to result
        result.generated_policy = parsed_result.generated_policy;
        result.confidence_score = parsed_result.confidence_score;
        result.metadata = parsed_result.metadata;
        result.success = parsed_result.success;

        if (!result.success) {
            result.error_message = parsed_result.error_message;
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in call_llm_for_conversion: " + std::string(e.what()));
        result.error_message = "LLM conversion failed: " + std::string(e.what());
        result.success = false;
    }

    return result;
}

PolicyConversionResult NLPolicyConverter::parse_llm_response(const std::string& llm_response, const PolicyConversionRequest& request) {
    PolicyConversionResult result;

    try {
        nlohmann::json parsed_response = nlohmann::json::parse(llm_response);

        // Extract policy
        if (parsed_response.contains("policy")) {
            result.generated_policy = parsed_response["policy"];
        } else {
            // Assume the entire response is the policy
            result.generated_policy = parsed_response;
        }

        // Extract confidence score
        if (parsed_response.contains("confidence_score")) {
            result.confidence_score = parsed_response["confidence_score"];
        } else if (parsed_response.contains("confidence")) {
            result.confidence_score = parsed_response["confidence"];
        } else {
            result.confidence_score = 0.5; // Default confidence
        }

        // Ensure confidence is within bounds
        result.confidence_score = std::max(0.0, std::min(1.0, result.confidence_score));

        // Extract metadata if present
        if (parsed_response.contains("metadata")) {
            result.metadata = parsed_response["metadata"];
        }

        // Validate basic policy structure
        if (!result.generated_policy.contains("rule_type") && !result.generated_policy.contains("type")) {
            result.generated_policy["rule_type"] = request.policy_type;
        }

        result.success = true;

    } catch (const nlohmann::json::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to parse LLM response as JSON: " + std::string(e.what()));
        result.error_message = "Invalid JSON response from LLM: " + std::string(e.what());
        result.success = false;
    }

    return result;
}

PolicyValidationResult NLPolicyConverter::validate_policy(const nlohmann::json& policy, const std::string& policy_type) {
    PolicyValidationResult result;

    try {
        if (policy_type == "fraud_rule") {
            result = validate_fraud_rule(policy);
        } else if (policy_type == "compliance_rule") {
            result = validate_compliance_rule(policy);
        } else if (policy_type == "validation_rule") {
            result = validate_validation_rule(policy);
        } else if (policy_type == "risk_rule") {
            result = validate_risk_rule(policy);
        } else {
            result.errors.push_back("Unknown policy type: " + policy_type);
            return result;
        }

        // Calculate validation score based on errors and warnings
        double error_weight = 1.0;
        double warning_weight = 0.3;

        double total_penalty = (result.errors.size() * error_weight) + (result.warnings.size() * warning_weight);
        result.validation_score = std::max(0.0, 1.0 - (total_penalty * 0.2)); // Max penalty of 1.0 reduces score by 0.2

        result.is_valid = result.errors.empty() && (validation_strictness_ < 0.5 || result.warnings.empty());

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in validate_policy: " + std::string(e.what()));
        result.errors.push_back("Validation failed: " + std::string(e.what()));
        result.is_valid = false;
        result.validation_score = 0.0;
    }

    return result;
}

PolicyValidationResult NLPolicyConverter::validate_fraud_rule(const nlohmann::json& policy) {
    PolicyValidationResult result;

    // Required fields for fraud rules
    std::vector<std::string> required_fields = {"rule_type", "name", "description", "conditions", "actions"};

    for (const auto& field : required_fields) {
        if (!policy.contains(field)) {
            result.errors.push_back("Missing required field: " + field);
        }
    }

    // Validate rule_type
    if (policy.contains("rule_type") && policy["rule_type"] != "fraud_rule") {
        result.errors.push_back("rule_type must be 'fraud_rule'");
    }

    // Validate conditions structure
    if (policy.contains("conditions")) {
        if (!policy["conditions"].is_array()) {
            result.errors.push_back("conditions must be an array");
        } else {
            for (size_t i = 0; i < policy["conditions"].size(); ++i) {
                const auto& condition = policy["conditions"][i];
                if (!condition.contains("field") || !condition.contains("operator")) {
                    result.warnings.push_back("Condition " + std::to_string(i) + " missing field or operator");
                }
            }
        }
    }

    // Validate actions structure
    if (policy.contains("actions")) {
        if (!policy["actions"].is_array()) {
            result.errors.push_back("actions must be an array");
        } else {
            for (size_t i = 0; i < policy["actions"].size(); ++i) {
                const auto& action = policy["actions"][i];
                if (!action.contains("type")) {
                    result.warnings.push_back("Action " + std::to_string(i) + " missing type");
                }
            }
        }
    }

    // Suggestions for improvement
    if (policy.contains("severity") && policy["severity"].is_string()) {
        std::string severity = policy["severity"];
        if (severity != "low" && severity != "medium" && severity != "high" && severity != "critical") {
            result.suggestions.push_back("Consider using standard severity levels: low, medium, high, critical");
        }
    }

    return result;
}

PolicyValidationResult NLPolicyConverter::validate_compliance_rule(const nlohmann::json& policy) {
    PolicyValidationResult result;

    // Required fields for compliance rules
    std::vector<std::string> required_fields = {"rule_type", "name", "description", "regulation_reference"};

    for (const auto& field : required_fields) {
        if (!policy.contains(field)) {
            result.errors.push_back("Missing required field: " + field);
        }
    }

    // Validate rule_type
    if (policy.contains("rule_type") && policy["rule_type"] != "compliance_rule") {
        result.errors.push_back("rule_type must be 'compliance_rule'");
    }

    // Validate regulation reference
    if (policy.contains("regulation_reference")) {
        if (!policy["regulation_reference"].is_string() && !policy["regulation_reference"].is_object()) {
            result.warnings.push_back("regulation_reference should be a string or object");
        }
    }

    // Check for monitoring requirements
    if (!policy.contains("monitoring_frequency")) {
        result.suggestions.push_back("Consider adding monitoring_frequency for compliance rules");
    }

    return result;
}

PolicyValidationResult NLPolicyConverter::validate_validation_rule(const nlohmann::json& policy) {
    PolicyValidationResult result;

    // Required fields for validation rules
    std::vector<std::string> required_fields = {"rule_type", "name", "target_field", "validation_type"};

    for (const auto& field : required_fields) {
        if (!policy.contains(field)) {
            result.errors.push_back("Missing required field: " + field);
        }
    }

    // Validate rule_type
    if (policy.contains("rule_type") && policy["rule_type"] != "validation_rule") {
        result.errors.push_back("rule_type must be 'validation_rule'");
    }

    // Validate validation_type
    if (policy.contains("validation_type")) {
        std::string validation_type = policy["validation_type"];
        std::vector<std::string> valid_types = {"regex", "range", "enum", "length", "format", "custom"};

        if (std::find(valid_types.begin(), valid_types.end(), validation_type) == valid_types.end()) {
            result.warnings.push_back("validation_type should be one of: regex, range, enum, length, format, custom");
        }
    }

    return result;
}

PolicyValidationResult NLPolicyConverter::validate_risk_rule(const nlohmann::json& policy) {
    PolicyValidationResult result;

    // Required fields for risk rules
    std::vector<std::string> required_fields = {"rule_type", "name", "risk_factors", "risk_threshold"};

    for (const auto& field : required_fields) {
        if (!policy.contains(field)) {
            result.errors.push_back("Missing required field: " + field);
        }
    }

    // Validate rule_type
    if (policy.contains("rule_type") && policy["rule_type"] != "risk_rule") {
        result.errors.push_back("rule_type must be 'risk_rule'");
    }

    // Validate risk_factors
    if (policy.contains("risk_factors") && !policy["risk_factors"].is_array()) {
        result.errors.push_back("risk_factors must be an array");
    }

    // Validate risk_threshold
    if (policy.contains("risk_threshold")) {
        if (!policy["risk_threshold"].is_number()) {
            result.errors.push_back("risk_threshold must be a number");
        } else {
            double threshold = policy["risk_threshold"];
            if (threshold < 0.0 || threshold > 1.0) {
                result.warnings.push_back("risk_threshold should be between 0.0 and 1.0");
            }
        }
    }

    return result;
}

std::string NLPolicyConverter::store_conversion_result(const PolicyConversionRequest& request, const PolicyConversionResult& result) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) {
            logger_->log(LogLevel::ERROR, "Database connection failed in store_conversion_result");
            return "";
        }

        std::string conversion_id = generate_uuid();
        nlohmann::json validation_errors = nlohmann::json::array();
        for (const auto& error : result.validation_errors) {
            validation_errors.push_back(error);
        }

        nlohmann::json metadata = result.metadata;
        metadata["processing_time_ms"] = result.processing_time.count();
        metadata["tokens_used"] = result.tokens_used;
        metadata["cost"] = result.cost;
        metadata["template_used"] = result.template_used;

        const char* params[12] = {
            conversion_id.c_str(),
            request.user_id.c_str(),
            request.natural_language_input.c_str(),
            result.generated_policy.dump().c_str(),
            request.policy_type.c_str(),
            std::to_string(result.confidence_score).c_str(),
            validation_errors.dump().c_str(),
            result.status.c_str(),
            result.template_used.c_str(),
            std::to_string(result.tokens_used).c_str(),
            std::to_string(result.cost).c_str(),
            metadata.dump().c_str()
        };

        PGresult* insert_result = PQexecParams(
            conn,
            "INSERT INTO nl_policy_conversions "
            "(conversion_id, user_id, natural_language_input, generated_policy, policy_type, "
            "confidence_score, validation_errors, status, template_used, tokens_used, cost, metadata) "
            "VALUES ($1, $2, $3, $4::jsonb, $5, $6::decimal, $7::jsonb, $8, $9, $10, $11, $12::jsonb)",
            12, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(insert_result) == PGRES_COMMAND_OK) {
            logger_->log(LogLevel::INFO, "Stored policy conversion " + conversion_id + " for user " + request.user_id);
            PQclear(insert_result);
            return conversion_id;
        } else {
            logger_->log(LogLevel::ERROR, "Failed to store conversion: " + std::string(PQresultErrorMessage(insert_result)));
            PQclear(insert_result);
            return "";
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in store_conversion_result: " + std::string(e.what()));
        return "";
    }
}

PolicyDeploymentResult NLPolicyConverter::deploy_policy(const PolicyDeploymentRequest& request) {
    PolicyDeploymentResult result;

    try {
        log_deployment_attempt(request);

        // Get conversion details
        auto conversion = get_conversion(request.conversion_id);
        if (!conversion) {
            result.error_message = "Conversion not found: " + request.conversion_id;
            result.success = false;
            log_deployment_result(result);
            return result;
        }

        // Validate conversion status
        std::string status = (*conversion)["status"];
        if (status != "approved") {
            result.error_message = "Conversion must be approved before deployment. Current status: " + status;
            result.success = false;
            log_deployment_result(result);
            return result;
        }

        nlohmann::json policy = (*conversion)["generated_policy"];

        // Deploy based on target system
        if (request.target_system == "fraud_detection") {
            result = deploy_to_fraud_detection(request, policy);
        } else if (request.target_system == "compliance_monitor") {
            result = deploy_to_compliance_monitor(request, policy);
        } else if (request.target_system == "validation_engine") {
            result = deploy_to_validation_engine(request, policy);
        } else if (request.target_system == "risk_assessment") {
            result = deploy_to_risk_assessment(request, policy);
        } else {
            result.error_message = "Unknown target system: " + request.target_system;
            result.success = false;
        }

        // Update conversion status if deployment successful
        if (result.success) {
            update_conversion_status(request.conversion_id, "deployed");
        }

        log_deployment_result(result);
        return result;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in deploy_policy: " + std::string(e.what()));
        result.error_message = "Deployment failed: " + std::string(e.what());
        result.success = false;
        return result;
    }
}

PolicyDeploymentResult NLPolicyConverter::deploy_to_fraud_detection(const PolicyDeploymentRequest& request, const nlohmann::json& policy) {
    PolicyDeploymentResult result;

    try {
        auto conn = db_conn_->get_connection();
        if (!conn) {
            result.error_message = "Database connection failed";
            return result;
        }

        std::string deployment_id = generate_uuid();

        // Convert policy to fraud rule format
        nlohmann::json fraud_rule = {
            {"rule_id", generate_uuid()},
            {"rule_name", policy.value("name", "Generated Fraud Rule")},
            {"description", policy.value("description", "")},
            {"rule_type", "fraud_detection"},
            {"conditions", policy.value("conditions", nlohmann::json::array())},
            {"actions", policy.value("actions", nlohmann::json::array())},
            {"severity", policy.value("severity", "medium")},
            {"is_active", true},
            {"created_by", request.deployed_by},
            {"source", "nl_policy_converter"},
            {"source_conversion_id", request.conversion_id}
        };

        // Insert into fraud_rules table (assuming it exists)
        const char* params[5] = {
            fraud_rule["rule_id"].get<std::string>().c_str(),
            fraud_rule["rule_name"].get<std::string>().c_str(),
            fraud_rule["description"].get<std::string>().c_str(),
            fraud_rule.dump().c_str(),
            request.deployed_by.c_str()
        };

        // Note: This assumes a fraud_rules table exists. In a real implementation,
        // you would check if the table exists and handle deployment accordingly.
        PGresult* insert_result = PQexecParams(
            conn,
            "INSERT INTO fraud_rules (rule_id, rule_name, description, rule_definition, created_by) "
            "VALUES ($1, $2, $3, $4::jsonb, $5)",
            5, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(insert_result) == PGRES_COMMAND_OK) {
            result.deployment_id = deployment_id;
            result.success = true;
            result.deployed_policy = fraud_rule;
            result.status = "deployed";

            // Store deployment record
            store_deployment_record(deployment_id, request, "deployed", fraud_rule);

            PQclear(insert_result);
        } else {
            result.error_message = "Failed to insert fraud rule: " + std::string(PQresultErrorMessage(insert_result));
            result.status = "failed";
            PQclear(insert_result);
        }

    } catch (const std::exception& e) {
        result.error_message = "Exception during fraud detection deployment: " + std::string(e.what());
        result.status = "failed";
    }

    return result;
}

// Placeholder implementations for other deployment methods
PolicyDeploymentResult NLPolicyConverter::deploy_to_compliance_monitor(const PolicyDeploymentRequest& request, const nlohmann::json& policy) {
    PolicyDeploymentResult result;
    result.deployment_id = generate_uuid();
    result.success = true;
    result.deployed_policy = policy;
    result.status = "deployed";
    store_deployment_record(result.deployment_id, request, "deployed", policy);
    return result;
}

PolicyDeploymentResult NLPolicyConverter::deploy_to_validation_engine(const PolicyDeploymentRequest& request, const nlohmann::json& policy) {
    PolicyDeploymentResult result;
    result.deployment_id = generate_uuid();
    result.success = true;
    result.deployed_policy = policy;
    result.status = "deployed";
    store_deployment_record(result.deployment_id, request, "deployed", policy);
    return result;
}

PolicyDeploymentResult NLPolicyConverter::deploy_to_risk_assessment(const PolicyDeploymentRequest& request, const nlohmann::json& policy) {
    PolicyDeploymentResult result;
    result.deployment_id = generate_uuid();
    result.success = true;
    result.deployed_policy = policy;
    result.status = "deployed";
    store_deployment_record(result.deployment_id, request, "deployed", policy);
    return result;
}

void NLPolicyConverter::store_deployment_record(const std::string& deployment_id, const PolicyDeploymentRequest& request,
                                              const std::string& status, const nlohmann::json& deployed_policy) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) return;

        const char* params[6] = {
            deployment_id.c_str(),
            request.conversion_id.c_str(),
            request.target_system.c_str(),
            deployed_policy.dump().c_str(),
            status.c_str(),
            request.deployed_by.c_str()
        };

        PGresult* result = PQexecParams(
            conn,
            "INSERT INTO policy_deployments "
            "(deployment_id, conversion_id, target_system, deployed_policy, deployment_status, deployed_by) "
            "VALUES ($1, $2, $3, $4::jsonb, $5, $6)",
            6, nullptr, params, nullptr, nullptr, 0
        );

        PQclear(result);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in store_deployment_record: " + std::string(e.what()));
    }
}

std::vector<PolicyTemplate> NLPolicyConverter::get_available_templates(const std::string& policy_type) {
    std::vector<PolicyTemplate> templates;

    try {
        auto conn = db_conn_->get_connection();
        if (!conn) return templates;

        std::string query = "SELECT template_id, template_name, template_description, policy_type, "
                           "template_prompt, input_schema, output_schema, example_inputs, example_outputs, "
                           "is_active, usage_count, success_rate, average_confidence, category "
                           "FROM policy_templates WHERE is_active = true";

        std::vector<const char*> params;
        if (!policy_type.empty()) {
            query += " AND policy_type = $1";
            params.push_back(policy_type.c_str());
        }

        query += " ORDER BY usage_count DESC, success_rate DESC";

        PGresult* result = PQexecParams(
            conn, query.c_str(), params.size(), nullptr,
            params.data(), nullptr, nullptr, 0
        );

        if (PQresultStatus(result) == PGRES_TUPLES_OK) {
            int rows = PQntuples(result);
            for (int i = 0; i < rows; ++i) {
                PolicyTemplate tmpl;
                tmpl.template_id = PQgetvalue(result, i, 0);
                tmpl.template_name = PQgetvalue(result, i, 1);
                tmpl.template_description = PQgetvalue(result, i, 2) ? PQgetvalue(result, i, 2) : "";
                tmpl.policy_type = PQgetvalue(result, i, 3);
                tmpl.template_prompt = PQgetvalue(result, i, 4);

                if (PQgetvalue(result, i, 5)) tmpl.input_schema = nlohmann::json::parse(PQgetvalue(result, i, 5));
                if (PQgetvalue(result, i, 6)) tmpl.output_schema = nlohmann::json::parse(PQgetvalue(result, i, 6));
                if (PQgetvalue(result, i, 7)) tmpl.example_inputs = nlohmann::json::parse(PQgetvalue(result, i, 7));
                if (PQgetvalue(result, i, 8)) tmpl.example_outputs = nlohmann::json::parse(PQgetvalue(result, i, 8));

                tmpl.is_active = std::string(PQgetvalue(result, i, 9)) == "t";
                tmpl.usage_count = std::atoi(PQgetvalue(result, i, 10));
                tmpl.success_rate = std::atof(PQgetvalue(result, i, 11));
                tmpl.average_confidence = std::atof(PQgetvalue(result, i, 12));
                tmpl.category = PQgetvalue(result, i, 13) ? PQgetvalue(result, i, 13) : "";

                templates.push_back(tmpl);
            }
        }

        PQclear(result);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in get_available_templates: " + std::string(e.what()));
    }

    return templates;
}

std::vector<nlohmann::json> NLPolicyConverter::get_user_conversions(const std::string& user_id, int limit, int offset) {
    std::vector<nlohmann::json> conversions;

    try {
        auto conn = db_conn_->get_connection();
        if (!conn) return conversions;

        const char* params[3] = {
            user_id.c_str(),
            std::to_string(limit).c_str(),
            std::to_string(offset).c_str()
        };

        PGresult* result = PQexecParams(
            conn,
            "SELECT conversion_id, natural_language_input, policy_type, confidence_score, "
            "status, created_at, feedback_rating "
            "FROM nl_policy_conversions "
            "WHERE user_id = $1 "
            "ORDER BY created_at DESC "
            "LIMIT $2 OFFSET $3",
            3, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) == PGRES_TUPLES_OK) {
            int rows = PQntuples(result);
            for (int i = 0; i < rows; ++i) {
                nlohmann::json conversion = {
                    {"conversion_id", PQgetvalue(result, i, 0)},
                    {"natural_language_input", PQgetvalue(result, i, 1)},
                    {"policy_type", PQgetvalue(result, i, 2)},
                    {"confidence_score", std::atof(PQgetvalue(result, i, 3))},
                    {"status", PQgetvalue(result, i, 4)},
                    {"created_at", PQgetvalue(result, i, 5)},
                    {"feedback_rating", PQgetvalue(result, i, 6) ? std::atoi(PQgetvalue(result, i, 6)) : 0}
                };
                conversions.push_back(conversion);
            }
        }

        PQclear(result);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in get_user_conversions: " + std::string(e.what()));
    }

    return conversions;
}

std::string NLPolicyConverter::generate_uuid() {
    uuid_t uuid;
    uuid_generate(uuid);

    char uuid_str[37];
    uuid_unparse_lower(uuid, uuid_str);

    return std::string(uuid_str);
}

double NLPolicyConverter::calculate_message_cost(const std::string& model, int input_tokens, int output_tokens) {
    // Simplified cost calculation - should be updated with actual pricing
    double input_cost_per_token = 0.0000015;  // Approximate for GPT-4
    double output_cost_per_token = 0.000002;

    return (input_tokens * input_cost_per_token) + (output_tokens * output_cost_per_token);
}

PolicyConversionResult NLPolicyConverter::create_fallback_result(const std::string& error_message) {
    PolicyConversionResult result;
    result.success = false;
    result.error_message = error_message;
    result.confidence_score = 0.0;
    result.processing_time = std::chrono::milliseconds(0);
    result.status = "failed";
    return result;
}

bool NLPolicyConverter::is_valid_policy_type(const std::string& policy_type) {
    std::vector<std::string> valid_types = {"fraud_rule", "compliance_rule", "validation_rule", "risk_rule"};
    return std::find(valid_types.begin(), valid_types.end(), policy_type) != valid_types.end();
}

// Placeholder implementations for remaining methods
std::optional<PolicyTemplate> NLPolicyConverter::get_template(const std::string& template_id) {
    // Implementation would query database for specific template
    return std::nullopt;
}

bool NLPolicyConverter::create_template(const PolicyTemplate& template_data, const std::string& user_id) {
    // Implementation would insert new template into database
    return false;
}

bool NLPolicyConverter::update_template_usage(const std::string& template_id, bool success, double confidence) {
    // Implementation would update template statistics
    return false;
}

std::optional<nlohmann::json> NLPolicyConverter::get_conversion(const std::string& conversion_id) {
    // Implementation would query database for conversion details
    return std::nullopt;
}

bool NLPolicyConverter::submit_feedback(const std::string& conversion_id, const std::string& feedback, int rating) {
    // Implementation would update conversion with feedback
    return false;
}

bool NLPolicyConverter::update_conversion_status(const std::string& conversion_id, const std::string& status,
                                               const std::optional<std::string>& reason) {
    // Implementation would update conversion status
    return false;
}

nlohmann::json NLPolicyConverter::get_conversion_analytics(const std::string& user_id,
                                                        const std::optional<std::string>& time_range) {
    // Implementation would calculate analytics
    return {};
}

std::vector<std::string> NLPolicyConverter::get_popular_templates(int limit) {
    // Implementation would return popular template IDs
    return {};
}

nlohmann::json NLPolicyConverter::get_success_rates_by_policy_type() {
    // Implementation would calculate success rates
    return {};
}

// Logging helper implementations
void NLPolicyConverter::log_conversion_attempt(const PolicyConversionRequest& request) {
    logger_->log(LogLevel::INFO, "Policy conversion attempt",
        {{"user_id", request.user_id},
         {"policy_type", request.policy_type},
         {"input_length", std::to_string(request.natural_language_input.length())}});
}

void NLPolicyConverter::log_conversion_success(const PolicyConversionResult& result) {
    logger_->log(LogLevel::INFO, "Policy conversion success",
        {{"conversion_id", result.conversion_id},
         {"confidence_score", std::to_string(result.confidence_score)},
         {"tokens_used", std::to_string(result.tokens_used)},
         {"processing_time_ms", std::to_string(result.processing_time.count())}});
}

void NLPolicyConverter::log_conversion_failure(const PolicyConversionRequest& request, const std::string& error) {
    logger_->log(LogLevel::ERROR, "Policy conversion failure",
        {{"user_id", request.user_id},
         {"policy_type", request.policy_type},
         {"error", error}});
}

void NLPolicyConverter::log_deployment_attempt(const PolicyDeploymentRequest& request) {
    logger_->log(LogLevel::INFO, "Policy deployment attempt",
        {{"conversion_id", request.conversion_id},
         {"target_system", request.target_system},
         {"deployed_by", request.deployed_by}});
}

void NLPolicyConverter::log_deployment_result(const PolicyDeploymentResult& result) {
    logger_->log(result.success ? LogLevel::INFO : LogLevel::ERROR,
        "Policy deployment result",
        {{"deployment_id", result.deployment_id},
         {"success", result.success ? "true" : "false"},
         {"status", result.status}});
}

} // namespace policy
} // namespace regulens
