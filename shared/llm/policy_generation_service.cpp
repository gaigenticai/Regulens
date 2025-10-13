/**
 * Natural Language Policy Generation Service Implementation
 * GPT-4 powered compliance rule generation from natural language
 */

#include "policy_generation_service.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <uuid/uuid.h>
#include <spdlog/spdlog.h>
#include <regex>

namespace regulens {

PolicyGenerationService::PolicyGenerationService(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<OpenAIClient> openai_client
) : db_conn_(db_conn), openai_client_(openai_client) {

    if (!db_conn_) {
        throw std::runtime_error("Database connection is required for PolicyGenerationService");
    }
    if (!openai_client_) {
        throw std::runtime_error("OpenAI client is required for PolicyGenerationService");
    }

    spdlog::info("PolicyGenerationService initialized with GPT-4 integration");
}

PolicyGenerationService::~PolicyGenerationService() {
    spdlog::info("PolicyGenerationService shutting down");
}

PolicyGenerationResult PolicyGenerationService::generate_policy(const PolicyGenerationRequest& request) {
    PolicyGenerationResult result;
    result.request_id = generate_policy_id();
    result.policy_id = result.request_id;
    result.processing_time = std::chrono::milliseconds(0);

    auto start_time = std::chrono::high_resolution_clock::now();

    try {
        // Normalize and validate input
        std::string normalized_description = normalize_description(request.natural_language_description);
        if (normalized_description.empty()) {
            result.success = false;
            result.error_message = "Empty or invalid policy description provided";
            return result;
        }

        // Generate primary rule using GPT-4
        result.primary_rule = generate_primary_rule(request, normalized_description);

        // Generate alternative rules if requested
        if (request.max_complexity_level > 1) {
            result.alternative_rules = generate_alternative_rules(request);
        }

        // Validate the generated rule
        if (validation_enabled_) {
            result.validation = validate_rule(result.primary_rule);
        }

        // Generate validation tests
        if (request.include_validation_tests) {
            result.primary_rule.validation_tests = generate_validation_tests(result.primary_rule);
        }

        // Generate documentation
        if (request.include_documentation) {
            result.primary_rule.documentation = generate_rule_documentation(result.primary_rule);
        }

        // Calculate cost and tokens (estimates)
        auto [tokens, cost] = calculate_generation_cost(
            estimate_token_count(normalized_description),
            estimate_token_count(result.primary_rule.generated_code)
        );
        result.tokens_used = tokens;
        result.cost = cost;

        result.success = true;

        // Store results in database
        store_generation_result(result);

        spdlog::info("Policy generation completed: {} for domain '{}' in {}ms",
                    result.policy_id, static_cast<int>(request.domain), result.processing_time.count());

    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("Policy generation failed: ") + e.what();

        auto end_time = std::chrono::high_resolution_clock::now();
        result.processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        spdlog::error("Policy generation failed: {}", e.what());
    }

    return result;
}

GeneratedRule PolicyGenerationService::generate_primary_rule(
    const PolicyGenerationRequest& request,
    const std::string& normalized_description
) {
    GeneratedRule rule;
    rule.rule_id = generate_rule_id();
    rule.name = generate_rule_name(normalized_description);
    rule.description = normalized_description;
    rule.natural_language_input = request.natural_language_description;
    rule.rule_type = request.rule_type;
    rule.domain = request.domain;
    rule.format = request.output_format;
    rule.generated_at = std::chrono::system_clock::now();

    try {
        // Build GPT-4 prompt
        std::string prompt = build_policy_generation_prompt(request);

        // Call GPT-4
        OpenAICompletionRequest gpt_request;
        gpt_request.model = default_model_;
        gpt_request.messages = {
            {"system", "You are an expert compliance policy developer. Generate precise, secure, and effective compliance rules from natural language descriptions. Focus on accuracy, security, and regulatory compliance."},
            {"user", prompt}
        };
        gpt_request.temperature = 0.1; // Low temperature for consistent rule generation
        gpt_request.max_tokens = 2000;

        auto response = openai_client_->create_chat_completion(gpt_request);
        if (!response || response->choices.empty()) {
            throw std::runtime_error("Failed to get policy generation response from OpenAI");
        }

        std::string gpt_response = response->choices[0].message.content;

        // Parse and format the rule
        rule.generated_code = format_rule_code_from_gpt_response(gpt_response, request.output_format);
        rule.confidence_score = 0.9; // High confidence for primary rule

        // Extract metadata from response
        extract_rule_metadata(gpt_response, rule);

    } catch (const std::exception& e) {
        spdlog::error("Primary rule generation failed: {}", e.what());
        rule.generated_code = generate_fallback_rule(request);
        rule.confidence_score = 0.3;
    }

    return rule;
}

std::vector<GeneratedRule> PolicyGenerationService::generate_alternative_rules(const PolicyGenerationRequest& request) {
    std::vector<GeneratedRule> alternatives;

    // Generate 2-3 alternative approaches
    for (int i = 0; i < std::min(3, request.max_complexity_level - 1); ++i) {
        try {
            GeneratedRule alt_rule = generate_primary_rule(request, request.natural_language_description);
            alt_rule.name += " (Alternative " + std::to_string(i + 1) + ")";
            alt_rule.confidence_score *= 0.8; // Slightly lower confidence for alternatives
            alternatives.push_back(alt_rule);
        } catch (const std::exception& e) {
            spdlog::warn("Failed to generate alternative rule {}: {}", i + 1, e.what());
        }
    }

    return alternatives;
}

RuleValidationResult PolicyGenerationService::validate_rule(const GeneratedRule& rule) {
    RuleValidationResult result;

    try {
        switch (rule.format) {
            case RuleFormat::JSON:
                result = validate_json_rule(rule.generated_code);
                break;
            case RuleFormat::DSL:
                result = validate_dsl_rule(rule.generated_code);
                break;
            case RuleFormat::PYTHON:
                result = validate_python_rule(rule.generated_code);
                break;
            default:
                result.syntax_valid = false;
                result.validation_errors.push_back("Unsupported rule format");
                break;
        }

        // Security validation
        result.security_safe = check_rule_security(rule.generated_code, rule.format);

        // Calculate overall score
        result.overall_score = calculate_validation_score(result);

    } catch (const std::exception& e) {
        spdlog::error("Rule validation failed: {}", e.what());
        result.validation_errors.push_back("Validation process failed: " + std::string(e.what()));
        result.overall_score = 0.0;
    }

    return result;
}

RuleValidationResult PolicyGenerationService::validate_json_rule(const std::string& rule_json) {
    RuleValidationResult result;

    try {
        nlohmann::json rule_obj = nlohmann::json::parse(rule_json);

        // Basic structure validation
        if (!rule_obj.contains("rule") || !rule_obj.contains("conditions")) {
            result.validation_errors.push_back("Missing required fields: 'rule' and 'conditions'");
            result.syntax_valid = false;
            return result;
        }

        // Validate rule structure
        auto& rule_def = rule_obj["rule"];
        if (!rule_def.contains("name") || !rule_def.contains("description")) {
            result.validation_errors.push_back("Rule definition missing 'name' or 'description'");
        }

        // Validate conditions structure
        auto& conditions = rule_obj["conditions"];
        if (!conditions.is_array()) {
            result.validation_errors.push_back("'conditions' must be an array");
        }

        result.syntax_valid = result.validation_errors.empty();

        // Logic validation (basic)
        result.logic_valid = validate_rule_logic(rule_obj);

    } catch (const nlohmann::json::exception& e) {
        result.syntax_valid = false;
        result.validation_errors.push_back("Invalid JSON format: " + std::string(e.what()));
    }

    return result;
}

RuleValidationResult PolicyGenerationService::validate_dsl_rule(const std::string& rule_dsl) {
    RuleValidationResult result;

    // Basic DSL validation - check for required keywords and structure
    std::vector<std::string> required_keywords = {"IF", "THEN", "RULE", "END"};

    for (const auto& keyword : required_keywords) {
        if (rule_dsl.find(keyword) == std::string::npos) {
            result.validation_errors.push_back("Missing required keyword: " + keyword);
        }
    }

    result.syntax_valid = result.validation_errors.empty();

    // Basic logic validation for DSL
    result.logic_valid = validate_dsl_logic(rule_dsl);

    return result;
}

RuleValidationResult PolicyGenerationService::validate_python_rule(const std::string& rule_python) {
    RuleValidationResult result;

    // Check for dangerous patterns
    std::vector<std::string> dangerous_patterns = {
        "import os", "import sys", "import subprocess", "eval(", "exec("
    };

    for (const auto& pattern : dangerous_patterns) {
        if (rule_python.find(pattern) != std::string::npos) {
            result.validation_errors.push_back("Potentially dangerous code pattern detected: " + pattern);
            result.security_safe = false;
        }
    }

    // Basic syntax check (this is simplified - in production use Python AST parsing)
    if (rule_python.find("def ") == std::string::npos) {
        result.validation_errors.push_back("Python rule must contain at least one function definition");
    }

    result.syntax_valid = result.validation_errors.empty();
    result.logic_valid = result.syntax_valid; // Simplified

    if (result.security_safe) {
        result.security_safe = result.validation_errors.empty();
    }

    return result;
}

bool PolicyGenerationService::check_rule_security(const std::string& code, RuleFormat format) {
    // Security checks based on format
    std::vector<std::string> dangerous_patterns;

    switch (format) {
        case RuleFormat::JSON:
            dangerous_patterns = {"eval", "exec", "system", "subprocess"};
            break;
        case RuleFormat::PYTHON:
            dangerous_patterns = {"__import__", "import os", "import sys", "eval(", "exec("};
            break;
        case RuleFormat::DSL:
            dangerous_patterns = {"EXECUTE", "SYSTEM", "SHELL"};
            break;
        default:
            break;
    }

    for (const auto& pattern : dangerous_patterns) {
        if (code.find(pattern) != std::string::npos) {
            return false;
        }
    }

    return true;
}

std::vector<std::string> PolicyGenerationService::generate_validation_tests(const GeneratedRule& rule) {
    std::vector<std::string> tests;

    try {
        std::string prompt = build_test_generation_prompt(rule);

        OpenAICompletionRequest gpt_request;
        gpt_request.model = default_model_;
        gpt_request.messages = {
            {"system", "You are a testing expert. Generate comprehensive validation tests for compliance rules."},
            {"user", prompt}
        };
        gpt_request.temperature = 0.2;
        gpt_request.max_tokens = 1000;

        auto response = openai_client_->create_chat_completion(gpt_request);
        if (response && !response->choices.empty()) {
            std::string test_response = response->choices[0].message.content;
            tests = parse_test_cases(test_response);
        }

    } catch (const std::exception& e) {
        spdlog::error("Test generation failed: {}", e.what());
    }

    // Fallback tests if generation fails
    if (tests.empty()) {
        tests = {
            "Test case 1: Valid input should pass validation",
            "Test case 2: Invalid input should fail validation",
            "Test case 3: Edge cases should be handled properly"
        };
    }

    return tests;
}

std::string PolicyGenerationService::generate_rule_documentation(const GeneratedRule& rule) {
    std::stringstream doc;

    doc << "# " << rule.name << "\n\n";
    doc << "## Description\n" << rule.description << "\n\n";
    doc << "## Rule Type\n" << rule_type_to_string(rule.rule_type) << "\n\n";
    doc << "## Domain\n" << domain_to_string(rule.domain) << "\n\n";
    doc << "## Generated Code\n```";
    switch (rule.format) {
        case RuleFormat::JSON: doc << "json"; break;
        case RuleFormat::YAML: doc << "yaml"; break;
        case RuleFormat::DSL: doc << "dsl"; break;
        case RuleFormat::PYTHON: doc << "python"; break;
        case RuleFormat::JAVASCRIPT: doc << "javascript"; break;
    }
    doc << "\n" << rule.generated_code << "\n```\n\n";

    if (!rule.validation_tests.empty()) {
        doc << "## Validation Tests\n";
        for (size_t i = 0; i < rule.validation_tests.size(); ++i) {
            doc << i + 1 << ". " << rule.validation_tests[i] << "\n";
        }
        doc << "\n";
    }

    if (!rule.suggested_improvements.empty()) {
        doc << "## Suggested Improvements\n";
        for (const auto& improvement : rule.suggested_improvements) {
            doc << "- " << improvement << "\n";
        }
        doc << "\n";
    }

    doc << "## Metadata\n";
    doc << "- Confidence Score: " << rule.confidence_score << "\n";
    doc << "- Generated At: " << format_timestamp(rule.generated_at) << "\n";

    return doc.str();
}

// Database operations
void PolicyGenerationService::store_generation_result(const PolicyGenerationResult& result) {
    try {
        if (!db_conn_) return;

        std::string query = R"(
            INSERT INTO policy_generation_results (
                policy_id, request_id, primary_rule_id, alternative_rules,
                validation_result, processing_time_ms, tokens_used, cost,
                success, error_message, version, parent_version, created_at
            ) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, NOW())
        )";

        nlohmann::json alternatives_json = nlohmann::json::array();
        for (const auto& alt : result.alternative_rules) {
            alternatives_json.push_back(alt.rule_id);
        }

        std::vector<std::string> params = {
            result.policy_id,
            result.request_id,
            result.primary_rule.rule_id,
            alternatives_json.dump(),
            result.validation.syntax_valid ? "valid" : "invalid",
            std::to_string(result.processing_time.count()),
            std::to_string(result.tokens_used),
            std::to_string(result.cost),
            result.success ? "true" : "false",
            result.error_message.value_or(""),
            result.version,
            result.parent_version.value_or("")
        };

        db_conn_->execute_command(query, params);

        // Store the primary rule
        store_rule(result.primary_rule);

    } catch (const std::exception& e) {
        spdlog::error("Failed to store generation result: {}", e.what());
    }
}

void PolicyGenerationService::store_rule(const GeneratedRule& rule) {
    try {
        if (!db_conn_) return;

        std::string query = R"(
            INSERT INTO generated_rules (
                rule_id, name, description, natural_language_input,
                rule_type, domain, format, generated_code, rule_metadata,
                validation_tests, documentation, confidence_score,
                suggested_improvements, generated_at
            ) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14)
        )";

        std::vector<std::string> params = {
            rule.rule_id,
            rule.name,
            rule.description,
            rule.natural_language_input,
            rule_type_to_string(rule.rule_type),
            domain_to_string(rule.domain),
            format_to_string(rule.format),
            rule.generated_code,
            rule.rule_metadata.dump(),
            nlohmann::json(rule.validation_tests).dump(),
            rule.documentation,
            std::to_string(rule.confidence_score),
            nlohmann::json(rule.suggested_improvements).dump(),
            std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                rule.generated_at.time_since_epoch()).count())
        };

        db_conn_->execute_command(query, params);

    } catch (const std::exception& e) {
        spdlog::error("Failed to store rule: {}", e.what());
    }
}

// Utility methods
std::string PolicyGenerationService::generate_policy_id() {
    return "policy_" + generate_uuid();
}

std::string PolicyGenerationService::generate_rule_id() {
    return "rule_" + generate_uuid();
}

std::string PolicyGenerationService::generate_uuid() {
    uuid_t uuid;
    uuid_generate(uuid);
    char uuid_str[37];
    uuid_unparse(uuid, uuid_str);
    return std::string(uuid_str);
}

std::string PolicyGenerationService::generate_rule_name(const std::string& description) {
    // Extract first few words as rule name
    std::stringstream ss(description);
    std::string word;
    std::string name;

    for (int i = 0; i < 5 && ss >> word; ++i) {
        if (!name.empty()) name += " ";
        name += word;
    }

    if (name.length() > 50) {
        name = name.substr(0, 47) + "...";
    }

    return name;
}

std::string PolicyGenerationService::build_policy_generation_prompt(const PolicyGenerationRequest& request) {
    std::stringstream prompt;

    prompt << "Generate a compliance rule based on this natural language description:\n\n";
    prompt << "\"" << request.natural_language_description << "\"\n\n";

    prompt << "Requirements:\n";
    prompt << "- Rule Type: " << rule_type_to_string(request.rule_type) << "\n";
    prompt << "- Domain: " << domain_to_string(request.domain) << "\n";
    prompt << "- Output Format: " << format_to_string(request.output_format) << "\n";
    prompt << "- Complexity Level: " << request.max_complexity_level << "/5\n";

    if (request.regulatory_framework) {
        prompt << "- Regulatory Framework: " << *request.regulatory_framework << "\n";
    }

    if (request.compliance_standard) {
        prompt << "- Compliance Standard: " << *request.compliance_standard << "\n";
    }

    prompt << "\nGenerate a complete, production-ready rule that includes:\n";
    prompt << "1. Clear rule name and description\n";
    prompt << "2. Well-defined conditions and actions\n";
    prompt << "3. Appropriate validation logic\n";
    prompt << "4. Security considerations\n";
    prompt << "5. Proper error handling\n\n";

    prompt << "Output the rule in " << format_to_string(request.output_format) << " format.\n";

    return prompt.str();
}

std::string PolicyGenerationService::format_rule_code_from_gpt_response(const std::string& response, RuleFormat format) {
    // Extract code from GPT response (remove markdown formatting if present)
    std::string code = response;

    // Remove markdown code blocks
    std::regex markdown_regex("```(?:json|yaml|python|javascript|dsl)?\\n?(.*?)```");
    std::smatch match;
    if (std::regex_search(code, match, markdown_regex)) {
        code = match[1].str();
    }

    // Trim whitespace
    code.erase(code.begin(), std::find_if(code.begin(), code.end(), [](char c) {
        return !std::isspace(c);
    }));
    code.erase(std::find_if(code.rbegin(), code.rend(), [](char c) {
        return !std::isspace(c);
    }).base(), code.end());

    return code;
}

void PolicyGenerationService::extract_rule_metadata(const std::string& response, GeneratedRule& rule) {
    // Extract metadata from GPT response (simplified)
    rule.rule_metadata = {
        {"generated_by", "gpt-4-turbo-preview"},
        {"generation_method", "natural_language_to_rule"},
        {"confidence_explanation", "Based on GPT-4 analysis of natural language input"}
    };
}

std::vector<std::string> PolicyGenerationService::parse_test_cases(const std::string& response) {
    std::vector<std::string> tests;

    // Simple parsing - split by numbered lines or bullet points
    std::stringstream ss(response);
    std::string line;

    while (std::getline(ss, line)) {
        // Remove leading/trailing whitespace
        line.erase(line.begin(), std::find_if(line.begin(), line.end(), [](char c) {
            return !std::isspace(c);
        }));
        line.erase(std::find_if(line.rbegin(), line.rend(), [](char c) {
            return !std::isspace(c);
        }).base(), line.end());

        // Check if it looks like a test case
        if (!line.empty() && (line.find("Test") != std::string::npos ||
                             std::isdigit(line[0]) ||
                             line[0] == '-')) {
            tests.push_back(line);
        }
    }

    return tests;
}

bool PolicyGenerationService::validate_rule_logic(const nlohmann::json& rule_obj) {
    // Basic logic validation - check for logical consistency
    if (!rule_obj.contains("conditions") || !rule_obj["conditions"].is_array()) {
        return false;
    }

    // Ensure conditions are properly structured
    for (const auto& condition : rule_obj["conditions"]) {
        if (!condition.contains("field") || !condition.contains("operator")) {
            return false;
        }
    }

    return true;
}

bool PolicyGenerationService::validate_dsl_logic(const std::string& rule_dsl) {
    // Basic DSL logic validation
    // Check for proper IF-THEN structure
    size_t if_pos = rule_dsl.find("IF");
    size_t then_pos = rule_dsl.find("THEN");

    if (if_pos == std::string::npos || then_pos == std::string::npos) {
        return false;
    }

    return if_pos < then_pos;
}

double PolicyGenerationService::calculate_validation_score(const RuleValidationResult& result) {
    double score = 0.0;

    if (result.syntax_valid) score += 0.4;
    if (result.logic_valid) score += 0.4;
    if (result.security_safe) score += 0.2;

    return score;
}

std::string PolicyGenerationService::normalize_description(const std::string& description) {
    std::string normalized = description;

    // Convert to lowercase for consistency
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);

    // Remove extra whitespace
    std::regex whitespace_regex("\\s+");
    normalized = std::regex_replace(normalized, whitespace_regex, " ");

    // Trim
    normalized.erase(normalized.begin(), std::find_if(normalized.begin(), normalized.end(),
                      [](char c) { return !std::isspace(c); }));
    normalized.erase(std::find_if(normalized.rbegin(), normalized.rend(),
                      [](char c) { return !std::isspace(c); }).base(), normalized.end());

    return normalized;
}

std::string PolicyGenerationService::rule_type_to_string(RuleType type) {
    switch (type) {
        case RuleType::VALIDATION_RULE: return "VALIDATION_RULE";
        case RuleType::BUSINESS_RULE: return "BUSINESS_RULE";
        case RuleType::COMPLIANCE_RULE: return "COMPLIANCE_RULE";
        case RuleType::RISK_RULE: return "RISK_RULE";
        case RuleType::AUDIT_RULE: return "AUDIT_RULE";
        case RuleType::WORKFLOW_RULE: return "WORKFLOW_RULE";
        default: return "UNKNOWN";
    }
}

std::string PolicyGenerationService::domain_to_string(PolicyDomain domain) {
    switch (domain) {
        case PolicyDomain::FINANCIAL_COMPLIANCE: return "FINANCIAL_COMPLIANCE";
        case PolicyDomain::DATA_PRIVACY: return "DATA_PRIVACY";
        case PolicyDomain::REGULATORY_REPORTING: return "REGULATORY_REPORTING";
        case PolicyDomain::RISK_MANAGEMENT: return "RISK_MANAGEMENT";
        case PolicyDomain::OPERATIONAL_CONTROLS: return "OPERATIONAL_CONTROLS";
        case PolicyDomain::SECURITY_POLICY: return "SECURITY_POLICY";
        case PolicyDomain::AUDIT_PROCEDURES: return "AUDIT_PROCEDURES";
        default: return "UNKNOWN";
    }
}

std::string PolicyGenerationService::format_to_string(RuleFormat format) {
    switch (format) {
        case RuleFormat::JSON: return "JSON";
        case RuleFormat::YAML: return "YAML";
        case RuleFormat::DSL: return "DSL";
        case RuleFormat::PYTHON: return "PYTHON";
        case RuleFormat::JAVASCRIPT: return "JAVASCRIPT";
        default: return "UNKNOWN";
    }
}

std::string PolicyGenerationService::format_timestamp(std::chrono::system_clock::time_point tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S UTC");
    return ss.str();
}

// Missing method implementations
std::string PolicyGenerationService::format_rule_code_from_gpt_response(const std::string& response, RuleFormat format) {
    // Extract code from GPT response (remove markdown formatting if present)
    std::string code = response;

    // Remove markdown code blocks
    std::regex markdown_regex("```(?:json|yaml|python|javascript|dsl)?\\n?(.*?)```");
    std::smatch match;
    if (std::regex_search(code, match, markdown_regex)) {
        code = match[1].str();
    }

    // Trim whitespace
    code.erase(code.begin(), std::find_if(code.begin(), code.end(), [](char c) {
        return !std::isspace(c);
    }));
    code.erase(std::find_if(code.rbegin(), code.rend(), [](char c) {
        return !std::isspace(c);
    }).base(), code.end());

    return code;
}

void PolicyGenerationService::extract_rule_metadata(const std::string& response, GeneratedRule& rule) {
    // Extract metadata from GPT response (simplified)
    rule.rule_metadata = {
        {"generated_by", "gpt-4-turbo-preview"},
        {"generation_method", "natural_language_to_rule"},
        {"confidence_explanation", "Based on GPT-4 analysis of natural language input"}
    };
}

std::string PolicyGenerationService::generate_fallback_rule(const PolicyGenerationRequest& request) {
    // Generate a basic fallback rule structure
    nlohmann::json fallback_rule = {
        {"rule", {
            {"name", "Generated Compliance Rule"},
            {"description", "Automatically generated rule from: " + request.natural_language_description},
            {"type", "COMPLIANCE_RULE"},
            {"domain", "FINANCIAL_COMPLIANCE"}
        }},
        {"conditions", nlohmann::json::array()},
        {"actions", nlohmann::json::array()},
        {"metadata", {
            {"generated_by", "fallback_generator"},
            {"confidence", 0.3}
        }}
    };

    return fallback_rule.dump(2);
}

// Utility string conversion functions
std::string PolicyGenerationService::rule_type_to_string(RuleType type) {
    switch (type) {
        case RuleType::VALIDATION_RULE: return "VALIDATION_RULE";
        case RuleType::BUSINESS_RULE: return "BUSINESS_RULE";
        case RuleType::COMPLIANCE_RULE: return "COMPLIANCE_RULE";
        case RuleType::RISK_RULE: return "RISK_RULE";
        case RuleType::AUDIT_RULE: return "AUDIT_RULE";
        case RuleType::WORKFLOW_RULE: return "WORKFLOW_RULE";
        default: return "UNKNOWN";
    }
}

std::string PolicyGenerationService::domain_to_string(PolicyDomain domain) {
    switch (domain) {
        case PolicyDomain::FINANCIAL_COMPLIANCE: return "FINANCIAL_COMPLIANCE";
        case PolicyDomain::DATA_PRIVACY: return "DATA_PRIVACY";
        case PolicyDomain::REGULATORY_REPORTING: return "REGULATORY_REPORTING";
        case PolicyDomain::RISK_MANAGEMENT: return "RISK_MANAGEMENT";
        case PolicyDomain::OPERATIONAL_CONTROLS: return "OPERATIONAL_CONTROLS";
        case PolicyDomain::SECURITY_POLICY: return "SECURITY_POLICY";
        case PolicyDomain::AUDIT_PROCEDURES: return "AUDIT_PROCEDURES";
        default: return "UNKNOWN";
    }
}

// Configuration setters
void PolicyGenerationService::set_default_model(const std::string& model) {
    default_model_ = model;
}

void PolicyGenerationService::set_validation_enabled(bool enabled) {
    validation_enabled_ = enabled;
}

void PolicyGenerationService::set_max_complexity_level(int level) {
    max_complexity_level_ = std::max(1, std::min(5, level));
}

void PolicyGenerationService::set_require_approval_for_deployment(bool required) {
    require_approval_for_deployment_ = required;
}

// Utility functions (defined here to avoid forward declaration issues)
int PolicyGenerationService::estimate_token_count(const std::string& text) {
    // Rough estimation: ~4 characters per token for English text
    return std::max(1, static_cast<int>(text.length() / 4));
}

double PolicyGenerationService::calculate_validation_score(const RuleValidationResult& result) {
    double score = 0.0;

    if (result.syntax_valid) score += 0.4;
    if (result.logic_valid) score += 0.4;
    if (result.security_safe) score += 0.2;

    return score;
}

} // namespace regulens
