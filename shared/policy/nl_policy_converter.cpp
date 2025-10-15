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
#include <unordered_map>

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
        OpenAICompletionRequest completion_request;
        completion_request.model = default_model_;
        completion_request.messages = {
            OpenAIMessage{"system", "You are a policy conversion expert. Always respond with valid JSON."},
            OpenAIMessage{"user", prompt}
        };
        completion_request.temperature = 0.1;
        completion_request.max_tokens = 2000;
        completion_request.presence_penalty = 0.0;
        completion_request.frequency_penalty = 0.0;

        auto openai_response_opt = openai_client_->create_chat_completion(completion_request);
        if (!openai_response_opt.has_value()) {
            result.error_message = "LLM service unavailable";
            result.success = false;
            return result;
        }

        const auto& openai_response = openai_response_opt.value();
        if (openai_response.choices.empty()) {
            result.error_message = "LLM returned no completion choices";
            result.success = false;
            return result;
        }

        const auto& assistant_message = openai_response.choices.front().message;
        std::string llm_response = assistant_message.content;

        result.tokens_used = openai_response.usage.total_tokens;
        result.cost = calculate_message_cost(
            default_model_,
            openai_response.usage.prompt_tokens,
            openai_response.usage.completion_tokens
        );

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
        if (!db_conn_ || !db_conn_->get_connection()) {
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

        std::vector<std::string> params = {
            conversion_id,
            request.user_id,
            request.natural_language_input,
            result.generated_policy.dump(),
            request.policy_type,
            std::to_string(result.confidence_score),
            validation_errors.dump(),
            result.status,
            result.template_used,
            std::to_string(result.tokens_used),
            std::to_string(result.cost),
            metadata.dump()
        };

        const std::string insert_sql =
            "INSERT INTO nl_policy_conversions "
            "(conversion_id, user_id, natural_language_input, generated_policy, policy_type, "
            "confidence_score, validation_errors, status, template_used, tokens_used, cost, metadata) "
            "VALUES ($1, $2, $3, $4::jsonb, $5, $6::decimal, $7::jsonb, $8, $9, $10, $11, $12::jsonb)";

        if (db_conn_->execute_command(insert_sql, params)) {
            logger_->log(LogLevel::INFO, "Stored policy conversion " + conversion_id + " for user " + request.user_id);
            return conversion_id;
        }

        logger_->log(LogLevel::ERROR, "Failed to store conversion: database command rejected");
        return "";

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
        if (!db_conn_ || !db_conn_->get_connection()) {
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

        std::vector<std::string> params = {
            fraud_rule["rule_id"].get<std::string>(),
            fraud_rule["rule_name"].get<std::string>(),
            fraud_rule["description"].get<std::string>(),
            fraud_rule.dump(),
            request.deployed_by
        };

        const std::string insert_sql =
            "INSERT INTO fraud_rules (rule_id, rule_name, description, rule_definition, created_by) "
            "VALUES ($1, $2, $3, $4::jsonb, $5)";

        if (db_conn_->execute_command(insert_sql, params)) {
            result.deployment_id = deployment_id;
            result.success = true;
            result.deployed_policy = fraud_rule;
            result.status = "deployed";

            // Store deployment record
            store_deployment_record(deployment_id, request, "deployed", fraud_rule);
        } else {
            result.error_message = "Failed to insert fraud rule";
            result.status = "failed";
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

    auto format_decimal = [](double value) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(4) << value;
        return oss.str();
    };

    try {
        auto conversion = get_conversion(request.conversion_id);
        if (!conversion) {
            result.error_message = "Conversion record not found";
            result.status = "failed";
            store_deployment_record(result.deployment_id, request, "failed", {
                {"error", result.error_message}
            });
            return result;
        }

        std::string rule_id = generate_uuid();
        std::string rule_name = policy.value("name", "Compliance Policy " + request.conversion_id);
        std::string natural_input;
        if (conversion->contains("natural_language_input") && !(*conversion)["natural_language_input"].is_null()) {
            natural_input = (*conversion)["natural_language_input"].get<std::string>();
        }
        if (natural_input.empty()) {
            natural_input = policy.dump();
        }

        double base_confidence = 0.75;
        if (policy.contains("confidence_score")) {
            base_confidence = policy.value("confidence_score", base_confidence);
        } else if (conversion->contains("confidence_score") && !(*conversion)["confidence_score"].is_null()) {
            try {
                base_confidence = std::stod((*conversion)["confidence_score"].get<std::string>());
            } catch (...) {}
        }

        std::string validation_status = policy.value("validation_status", std::string("approved"));
        bool auto_activate = policy.value("is_active", true);

        nlohmann::json controls = {
            {"conditions", policy.value("conditions", nlohmann::json::array())},
            {"actions", policy.value("actions", nlohmann::json::array())},
            {"exceptions", policy.value("exceptions", nlohmann::json::array())},
            {"monitoring", policy.value("monitoring", nlohmann::json::object())},
            {"deployment_metadata", {
                {"conversion_id", request.conversion_id},
                {"deployed_by", request.deployed_by},
                {"deployment_id", result.deployment_id},
                {"deployment_options", request.deployment_options.value_or(nlohmann::json::object())}
            }}
        };

        std::vector<std::string> params = {
            rule_id,
            rule_name,
            natural_input,
            controls.dump(),
            "compliance_rule",
            request.deployed_by,
            auto_activate ? "true" : "false",
            format_decimal(std::clamp(base_confidence, 0.0, 1.0)),
            validation_status
        };

        bool stored = db_conn_->execute_command(
            "INSERT INTO nl_policy_rules "
            "(rule_id, rule_name, natural_language_input, generated_rule_logic, rule_type, created_by, is_active, confidence_score, validation_status) "
            "VALUES ($1, $2, $3, $4::jsonb, $5, $6, $7::boolean, $8::numeric, $9) "
            "ON CONFLICT (rule_id) DO UPDATE SET "
            "rule_name = EXCLUDED.rule_name, "
            "generated_rule_logic = EXCLUDED.generated_rule_logic, "
            "is_active = EXCLUDED.is_active, "
            "confidence_score = EXCLUDED.confidence_score, "
            "validation_status = EXCLUDED.validation_status, "
            "updated_at = CURRENT_TIMESTAMP",
            params);

        if (!stored) {
            result.error_message = "Failed to persist compliance rule";
            result.status = "failed";
            store_deployment_record(result.deployment_id, request, "failed", {
                {"error", result.error_message},
                {"rule_id", rule_id}
            });
            return result;
        }

        nlohmann::json deployed_summary = {
            {"rule_id", rule_id},
            {"rule_name", rule_name},
            {"confidence_score", base_confidence},
            {"validation_status", validation_status},
            {"is_active", auto_activate},
            {"controls", controls}
        };

        result.success = true;
        result.status = "deployed";
        result.deployed_policy = deployed_summary;

        store_deployment_record(result.deployment_id, request, "deployed", deployed_summary);
        return result;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in deploy_to_compliance_monitor: " + std::string(e.what()));
        result.error_message = "Deployment failed: " + std::string(e.what());
        result.status = "failed";
        store_deployment_record(result.deployment_id, request, "failed", {
            {"error", result.error_message}
        });
        return result;
    }
}

PolicyDeploymentResult NLPolicyConverter::deploy_to_validation_engine(const PolicyDeploymentRequest& request, const nlohmann::json& policy) {
    PolicyDeploymentResult result;
    result.deployment_id = generate_uuid();

    auto ensure_array = [](const nlohmann::json& value) {
        if (value.is_array()) {
            return value;
        }
        if (value.is_null()) {
            return nlohmann::json::array();
        }
        return nlohmann::json::array({value});
    };

    try {
        nlohmann::json schema_constraints = policy.value("validation", nlohmann::json::object());
        if (!schema_constraints.contains("required")) {
            schema_constraints["required"] = ensure_array(policy.value("required_fields", nlohmann::json::array()));
        }
        if (!schema_constraints.contains("prohibited")) {
            schema_constraints["prohibited"] = ensure_array(policy.value("prohibited_fields", nlohmann::json::array()));
        }
        schema_constraints["severity_mapping"] = policy.value("severity_mapping", nlohmann::json::object());
        schema_constraints["numeric_thresholds"] = policy.value("thresholds", nlohmann::json::object());

        std::string validation_rule_id = generate_uuid();
        std::string rule_name = policy.value("name", std::string("Validation Rule ") + request.conversion_id);
        std::string policy_type = policy.value("policy_type", std::string("validation_rule"));
        std::string error_message = policy.value("error_message", "Validation constraints violated");
        std::string severity = policy.value("severity", "error");
        bool is_active = policy.value("is_active", true);

        std::vector<std::string> params = {
            validation_rule_id,
            rule_name,
            policy_type,
            schema_constraints.dump(),
            error_message,
            severity,
            is_active ? "true" : "false"
        };

        bool stored = db_conn_->execute_command(
            "INSERT INTO policy_validation_rules "
            "(validation_rule_id, rule_name, policy_type, validation_logic, error_message, severity, is_active) "
            "VALUES ($1, $2, $3, $4::jsonb, $5, $6, $7::boolean) "
            "ON CONFLICT (validation_rule_id) DO UPDATE SET "
            "rule_name = EXCLUDED.rule_name, "
            "policy_type = EXCLUDED.policy_type, "
            "validation_logic = EXCLUDED.validation_logic, "
            "error_message = EXCLUDED.error_message, "
            "severity = EXCLUDED.severity, "
            "is_active = EXCLUDED.is_active, "
            "updated_at = CURRENT_TIMESTAMP",
            params);

        if (!stored) {
            result.error_message = "Failed to persist validation rule";
            result.status = "failed";
            store_deployment_record(result.deployment_id, request, "failed", {
                {"validation_rule_id", validation_rule_id},
                {"error", result.error_message}
            });
            return result;
        }

        nlohmann::json deployed_summary = {
            {"validation_rule_id", validation_rule_id},
            {"rule_name", rule_name},
            {"policy_type", policy_type},
            {"severity", severity},
            {"is_active", is_active},
            {"validation_logic", schema_constraints}
        };

        result.success = true;
        result.status = "deployed";
        result.deployed_policy = deployed_summary;
        store_deployment_record(result.deployment_id, request, "deployed", deployed_summary);
        return result;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in deploy_to_validation_engine: " + std::string(e.what()));
        result.error_message = "Deployment failed: " + std::string(e.what());
        result.status = "failed";
        store_deployment_record(result.deployment_id, request, "failed", {
            {"error", result.error_message}
        });
        return result;
    }
}

PolicyDeploymentResult NLPolicyConverter::deploy_to_risk_assessment(const PolicyDeploymentRequest& request, const nlohmann::json& policy) {
    PolicyDeploymentResult result;
    result.deployment_id = generate_uuid();

    auto format_decimal = [](double value) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(4) << value;
        return oss.str();
    };

    auto to_lower = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    };

    auto ensure_array = [](const nlohmann::json& value) {
        if (value.is_array()) {
            return value;
        }
        if (value.is_null()) {
            return nlohmann::json::array();
        }
        return nlohmann::json::array({value});
    };

    try {
        std::optional<nlohmann::json> active_model = db_conn_->execute_query_single(
            "SELECT model_id FROM compliance_ml_models WHERE is_active = true ORDER BY COALESCE(last_trained_at, created_at) DESC LIMIT 1",
            {}
        );

        double risk_score = 0.5;
        if (policy.contains("risk_score")) {
            risk_score = policy.value("risk_score", risk_score);
        } else if (policy.contains("severity")) {
            std::string severity = to_lower(policy.value("severity", std::string("medium")));
            if (severity == "critical" || severity == "very_high") {
                risk_score = 0.92;
            } else if (severity == "high") {
                risk_score = 0.78;
            } else if (severity == "medium") {
                risk_score = 0.55;
            } else {
                risk_score = 0.35;
            }
        }

        risk_score = std::clamp(risk_score, 0.0, 1.0);

        std::string risk_level;
        if (risk_score >= 0.85) {
            risk_level = "critical";
        } else if (risk_score >= 0.65) {
            risk_level = "high";
        } else if (risk_score >= 0.45) {
            risk_level = "medium";
        } else {
            risk_level = "low";
        }

        double confidence = policy.value("confidence_score", 0.7);
        confidence = std::clamp(confidence, 0.0, 1.0);

        int horizon = policy.value("prediction_horizon_days", 30);
        if (horizon <= 0) {
            horizon = 30;
        }

        nlohmann::json contributing_factors = nlohmann::json::array();
        if (policy.contains("conditions") && policy["conditions"].is_array()) {
            for (const auto& condition : policy["conditions"]) {
                contributing_factors.push_back(condition);
            }
        }
        if (contributing_factors.empty()) {
            contributing_factors.push_back({{"source", "policy"}, {"detail", "No explicit conditions provided"}});
        }

        nlohmann::json recommended_actions = ensure_array(policy.value("actions", nlohmann::json::array()));
        if (recommended_actions.empty()) {
            recommended_actions.push_back({{"action", "monitor"}, {"priority", "medium"}});
        }

        nlohmann::json metadata = {
            {"conversion_id", request.conversion_id},
            {"deployment_id", result.deployment_id},
            {"source_policy", policy},
            {"deployment_options", request.deployment_options.value_or(nlohmann::json::object())}
        };

        std::string prediction_id = generate_uuid();
        std::vector<std::string> params;
        std::string query;

        if (active_model && active_model->contains("model_id") && !(*active_model)["model_id"].is_null()) {
            std::string model_id = (*active_model)["model_id"].get<std::string>();
            params = {
                prediction_id,
                model_id,
                "policy",
                request.conversion_id,
                format_decimal(risk_score),
                risk_level,
                format_decimal(confidence),
                std::to_string(horizon),
                contributing_factors.dump(),
                recommended_actions.dump(),
                metadata.dump()
            };

            query = "INSERT INTO compliance_risk_predictions "
                    "(prediction_id, model_id, entity_type, entity_id, risk_score, risk_level, confidence_score, prediction_horizon_days, contributing_factors, recommended_actions, metadata) "
                    "VALUES ($1, $2, $3, $4, $5::numeric, $6, $7::numeric, $8, $9::jsonb, $10::jsonb, $11::jsonb)";
        } else {
            params = {
                prediction_id,
                "policy",
                request.conversion_id,
                format_decimal(risk_score),
                risk_level,
                format_decimal(confidence),
                std::to_string(horizon),
                contributing_factors.dump(),
                recommended_actions.dump(),
                metadata.dump()
            };

            query = "INSERT INTO compliance_risk_predictions "
                    "(prediction_id, entity_type, entity_id, risk_score, risk_level, confidence_score, prediction_horizon_days, contributing_factors, recommended_actions, metadata) "
                    "VALUES ($1, $2, $3, $4::numeric, $5, $6::numeric, $7, $8::jsonb, $9::jsonb, $10::jsonb)";
        }

        if (!db_conn_->execute_command(query, params)) {
            result.error_message = "Failed to store risk prediction";
            result.status = "failed";
            store_deployment_record(result.deployment_id, request, "failed", {
                {"prediction_id", prediction_id},
                {"error", result.error_message}
            });
            return result;
        }

        nlohmann::json deployed_summary = {
            {"prediction_id", prediction_id},
            {"risk_score", risk_score},
            {"risk_level", risk_level},
            {"confidence_score", confidence},
            {"prediction_horizon_days", horizon},
            {"contributing_factors", contributing_factors},
            {"recommended_actions", recommended_actions}
        };
        if (active_model && active_model->contains("model_id") && !(*active_model)["model_id"].is_null()) {
            deployed_summary["model_id"] = (*active_model)["model_id"].get<std::string>();
        }

        result.success = true;
        result.status = "deployed";
        result.deployed_policy = deployed_summary;
        store_deployment_record(result.deployment_id, request, "deployed", deployed_summary);
        return result;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in deploy_to_risk_assessment: " + std::string(e.what()));
        result.error_message = "Deployment failed: " + std::string(e.what());
        result.status = "failed";
        store_deployment_record(result.deployment_id, request, "failed", {
            {"error", result.error_message}
        });
        return result;
    }
}

void NLPolicyConverter::store_deployment_record(const std::string& deployment_id, const PolicyDeploymentRequest& request,
                                              const std::string& status, const nlohmann::json& deployed_policy) {
    try {
        if (!db_conn_ || !db_conn_->get_connection()) {
            return;
        }

        std::string target_table;
        if (request.target_system == "fraud_detection") {
            target_table = "fraud_rules";
        } else if (request.target_system == "compliance_monitor") {
            target_table = "nl_policy_rules";
        } else if (request.target_system == "validation_engine") {
            target_table = "policy_validation_rules";
        } else if (request.target_system == "risk_assessment") {
            target_table = "compliance_risk_predictions";
        }

        std::vector<std::string> params = {
            deployment_id,
            request.conversion_id,
            request.target_system
        };

        std::string query;
        if (!target_table.empty()) {
            params.push_back(target_table);
            params.push_back(deployed_policy.dump());
            params.push_back(status);
            params.push_back(request.deployed_by);
            query = "INSERT INTO policy_deployments "
                    "(deployment_id, conversion_id, target_system, target_table, deployed_policy, deployment_status, deployed_by) "
                    "VALUES ($1, $2, $3, $4, $5::jsonb, $6, $7)";
        } else {
            params.push_back(deployed_policy.dump());
            params.push_back(status);
            params.push_back(request.deployed_by);
            query = "INSERT INTO policy_deployments "
                    "(deployment_id, conversion_id, target_system, deployed_policy, deployment_status, deployed_by) "
                    "VALUES ($1, $2, $3, $4::jsonb, $5, $6)";
        }

        db_conn_->execute_command(query, params);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in store_deployment_record: " + std::string(e.what()));
    }
}

std::vector<PolicyTemplate> NLPolicyConverter::get_available_templates(const std::string& policy_type) {
    std::vector<PolicyTemplate> templates;

    try {
        if (!db_conn_ || !db_conn_->get_connection()) {
            return templates;
        }

        std::string query = "SELECT template_id, template_name, template_description, policy_type, "
                           "template_prompt, input_schema, output_schema, example_inputs, example_outputs, "
                           "is_active, usage_count, success_rate, average_confidence, category "
                           "FROM policy_templates WHERE is_active = true";

        std::vector<std::string> params;
        if (!policy_type.empty()) {
            query += " AND policy_type = $1";
            params.push_back(policy_type);
        }

        query += " ORDER BY usage_count DESC, success_rate DESC";

        auto parse_json_or = [](const std::string& value, const nlohmann::json& fallback) {
            if (value.empty()) {
                return fallback;
            }
            try {
                return nlohmann::json::parse(value);
            } catch (...) {
                return fallback;
            }
        };

        auto rows = db_conn_->execute_query_multi(query, params);
        for (const auto& row : rows) {
            PolicyTemplate tmpl;
            tmpl.template_id = row.value("template_id", std::string{});
            tmpl.template_name = row.value("template_name", std::string{});
            tmpl.template_description = row.value("template_description", std::string{});
            tmpl.policy_type = row.value("policy_type", std::string{});
            tmpl.template_prompt = row.value("template_prompt", std::string{});

            tmpl.input_schema = parse_json_or(row.value("input_schema", std::string{}), nlohmann::json::object());
            tmpl.output_schema = parse_json_or(row.value("output_schema", std::string{}), nlohmann::json::object());
            auto example_inputs_json = parse_json_or(row.value("example_inputs", std::string{}), nlohmann::json::array());
            auto example_outputs_json = parse_json_or(row.value("example_outputs", std::string{}), nlohmann::json::array());

            if (example_inputs_json.is_array()) {
                tmpl.example_inputs = example_inputs_json.get<std::vector<std::string>>();
            }

            if (example_outputs_json.is_array()) {
                tmpl.example_outputs.clear();
                for (const auto& item : example_outputs_json) {
                    tmpl.example_outputs.push_back(item);
                }
            }

            tmpl.is_active = row.value("is_active", "t") == "t";
            tmpl.usage_count = std::stoi(row.value("usage_count", std::string{"0"}));
            tmpl.success_rate = std::stod(row.value("success_rate", std::string{"0.0"}));
            tmpl.average_confidence = std::stod(row.value("average_confidence", std::string{"0.0"}));
            tmpl.category = row.value("category", std::string{});

            templates.push_back(std::move(tmpl));
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in get_available_templates: " + std::string(e.what()));
    }

    return templates;
}

std::vector<nlohmann::json> NLPolicyConverter::get_user_conversions(const std::string& user_id, int limit, int offset) {
    std::vector<nlohmann::json> conversions;

    try {
        if (!db_conn_ || !db_conn_->get_connection()) {
            return conversions;
        }

        std::vector<std::string> params = {
            user_id,
            std::to_string(limit),
            std::to_string(offset)
        };

        auto rows = db_conn_->execute_query_multi(
            "SELECT conversion_id, natural_language_input, policy_type, confidence_score, "
            "status, created_at, feedback_rating "
            "FROM nl_policy_conversions "
            "WHERE user_id = $1 "
            "ORDER BY created_at DESC "
            "LIMIT $2 OFFSET $3",
            params
        );

        for (const auto& row : rows) {
            nlohmann::json conversion = {
                {"conversion_id", row.value("conversion_id", std::string{})},
                {"natural_language_input", row.value("natural_language_input", std::string{})},
                {"policy_type", row.value("policy_type", std::string{})},
                {"confidence_score", std::stod(row.value("confidence_score", std::string{"0.0"}))},
                {"status", row.value("status", std::string{})},
                {"created_at", row.value("created_at", std::string{})},
                {"feedback_rating", std::stoi(row.value("feedback_rating", std::string{"0"}))}
            };
            conversions.push_back(std::move(conversion));
        }

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
    std::unordered_map<std::string, std::string> context = {
        {"user_id", request.user_id},
        {"policy_type", request.policy_type},
        {"input_length", std::to_string(request.natural_language_input.length())}
    };
    logger_->log(LogLevel::INFO, "Policy conversion attempt", "NLPolicyConverter", __func__, context);
}

void NLPolicyConverter::log_conversion_success(const PolicyConversionResult& result) {
    std::unordered_map<std::string, std::string> context = {
        {"conversion_id", result.conversion_id},
        {"confidence_score", std::to_string(result.confidence_score)},
        {"tokens_used", std::to_string(result.tokens_used)},
        {"processing_time_ms", std::to_string(result.processing_time.count())}
    };
    logger_->log(LogLevel::INFO, "Policy conversion success", "NLPolicyConverter", __func__, context);
}

void NLPolicyConverter::log_conversion_failure(const PolicyConversionRequest& request, const std::string& error) {
    std::unordered_map<std::string, std::string> context = {
        {"user_id", request.user_id},
        {"policy_type", request.policy_type},
        {"error", error}
    };
    logger_->log(LogLevel::ERROR, "Policy conversion failure", "NLPolicyConverter", __func__, context);
}

void NLPolicyConverter::log_deployment_attempt(const PolicyDeploymentRequest& request) {
    std::unordered_map<std::string, std::string> context = {
        {"conversion_id", request.conversion_id},
        {"target_system", request.target_system},
        {"deployed_by", request.deployed_by}
    };
    logger_->log(LogLevel::INFO, "Policy deployment attempt", "NLPolicyConverter", __func__, context);
}

void NLPolicyConverter::log_deployment_result(const PolicyDeploymentResult& result) {
    std::unordered_map<std::string, std::string> context = {
        {"deployment_id", result.deployment_id},
        {"success", result.success ? "true" : "false"},
        {"status", result.status}
    };
    logger_->log(result.success ? LogLevel::INFO : LogLevel::ERROR,
                 "Policy deployment result",
                 "NLPolicyConverter",
                 __func__,
                 context);
}

} // namespace policy
} // namespace regulens
