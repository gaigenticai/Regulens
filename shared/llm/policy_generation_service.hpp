/**
 * Natural Language Policy Generation Service
 * GPT-4 powered compliance rule generation from natural language
 */

#ifndef POLICY_GENERATION_SERVICE_HPP
#define POLICY_GENERATION_SERVICE_HPP

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <chrono>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "../database/postgresql_connection.hpp"
#include "openai_client.hpp"

namespace regulens {

enum class RuleFormat {
    JSON,
    YAML,
    DSL,  // Domain Specific Language
    PYTHON,
    JAVASCRIPT
};

enum class RuleType {
    VALIDATION_RULE,
    BUSINESS_RULE,
    COMPLIANCE_RULE,
    RISK_RULE,
    AUDIT_RULE,
    WORKFLOW_RULE
};

enum class PolicyDomain {
    FINANCIAL_COMPLIANCE,
    DATA_PRIVACY,
    REGULATORY_REPORTING,
    RISK_MANAGEMENT,
    OPERATIONAL_CONTROLS,
    SECURITY_POLICY,
    AUDIT_PROCEDURES
};

struct PolicyGenerationRequest {
    std::string natural_language_description;
    RuleType rule_type = RuleType::COMPLIANCE_RULE;
    PolicyDomain domain = PolicyDomain::FINANCIAL_COMPLIANCE;
    RuleFormat output_format = RuleFormat::JSON;
    std::optional<std::string> existing_rules_context;
    std::optional<std::string> regulatory_framework;
    std::optional<std::string> compliance_standard;
    bool include_validation_tests = true;
    bool include_documentation = true;
    int max_complexity_level = 3; // 1-5 scale
};

struct GeneratedRule {
    std::string rule_id;
    std::string name;
    std::string description;
    std::string natural_language_input;
    RuleType rule_type;
    PolicyDomain domain;
    RuleFormat format;
    std::string generated_code;
    nlohmann::json rule_metadata;
    std::vector<std::string> validation_tests;
    std::string documentation;
    double confidence_score = 0.0;
    std::vector<std::string> suggested_improvements;
    std::chrono::system_clock::time_point generated_at;
};

struct RuleValidationResult {
    bool syntax_valid = false;
    bool logic_valid = false;
    bool security_safe = false;
    std::vector<std::string> validation_errors;
    std::vector<std::string> warnings;
    std::vector<std::string> test_results;
    double overall_score = 0.0;
};

struct PolicyGenerationResult {
    std::string request_id;
    std::string policy_id;
    GeneratedRule primary_rule;
    std::vector<GeneratedRule> alternative_rules;
    RuleValidationResult validation;
    std::chrono::milliseconds processing_time;
    double cost = 0.0;
    int tokens_used = 0;
    bool success = true;
    std::optional<std::string> error_message;

    // Version control
    std::string version = "1.0.0";
    std::optional<std::string> parent_version;
};

struct RuleDeploymentRequest {
    std::string rule_id;
    std::string target_environment; // "development", "staging", "production"
    std::string deployed_by;
    std::optional<std::string> review_comments;
    bool requires_approval = true;
};

struct RuleDeploymentResult {
    bool success = false;
    std::string deployment_id;
    std::string status; // "pending_approval", "deployed", "failed"
    std::chrono::system_clock::time_point deployed_at;
    std::optional<std::string> error_message;
};

class PolicyGenerationService {
public:
    PolicyGenerationService(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<OpenAIClient> openai_client
    );

    ~PolicyGenerationService();

    // Core policy generation methods
    PolicyGenerationResult generate_policy(const PolicyGenerationRequest& request);
    GeneratedRule generate_primary_rule(const PolicyGenerationRequest& request, const std::string& normalized_description);
    std::vector<GeneratedRule> generate_alternative_rules(const PolicyGenerationRequest& request);

    // Rule validation and testing
    RuleValidationResult validate_rule(const GeneratedRule& rule);
    RuleValidationResult validate_rule_code(const std::string& code, RuleFormat format);

    // Rule management
    std::optional<GeneratedRule> get_rule(const std::string& rule_id);
    std::vector<GeneratedRule> get_rules_by_domain(PolicyDomain domain, int limit = 50);
    std::vector<GeneratedRule> search_rules(const std::string& query, int limit = 20);
    bool update_rule(const std::string& rule_id, const nlohmann::json& updates);
    bool delete_rule(const std::string& rule_id);

    // Version control and deployment
    RuleDeploymentResult deploy_rule(const RuleDeploymentRequest& request);
    std::vector<PolicyGenerationResult> get_rule_history(const std::string& rule_id);
    std::optional<PolicyGenerationResult> get_rule_version(const std::string& rule_id, const std::string& version);

    // Rule templates and examples
    std::vector<nlohmann::json> get_rule_templates(PolicyDomain domain);
    std::vector<std::string> get_example_descriptions(PolicyDomain domain);

    // Analytics and reporting
    std::unordered_map<std::string, int> get_generation_stats();
    std::vector<std::pair<std::string, int>> get_most_used_templates();

    // Configuration
    void set_default_model(const std::string& model);
    void set_validation_enabled(bool enabled);
    void set_max_complexity_level(int level);
    void set_require_approval_for_deployment(bool required);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<OpenAIClient> openai_client_;

    // Configuration
    std::string default_model_ = "gpt-4-turbo-preview";
    bool validation_enabled_ = true;
    int max_complexity_level_ = 3;
    bool require_approval_for_deployment_ = true;

    // Internal methods
    std::string generate_policy_id();
    std::string generate_rule_id();

    // GPT-4 prompt generation
    std::string build_policy_generation_prompt(const PolicyGenerationRequest& request);
    std::string build_rule_validation_prompt(const GeneratedRule& rule);
    std::string build_test_generation_prompt(const GeneratedRule& rule);

    // Rule generation and parsing
    GeneratedRule generate_rule_from_gpt4_response(const std::string& response, const PolicyGenerationRequest& request);
    std::string format_rule_code(const nlohmann::json& rule_definition, RuleFormat format);
    std::string format_rule_code_from_gpt_response(const std::string& response, RuleFormat format);
    void extract_rule_metadata(const std::string& response, GeneratedRule& rule);

    // Validation methods
    RuleValidationResult validate_json_rule(const std::string& rule_json);
    RuleValidationResult validate_dsl_rule(const std::string& rule_dsl);
    RuleValidationResult validate_python_rule(const std::string& rule_python);
    bool check_rule_security(const std::string& code, RuleFormat format);

    // Test generation
    std::vector<std::string> generate_validation_tests(const GeneratedRule& rule);
    bool execute_test(const std::string& test_code, RuleFormat format);

    // Database operations
    void store_generation_result(const PolicyGenerationResult& result);
    void store_rule(const GeneratedRule& rule);
    void store_deployment(const RuleDeploymentResult& deployment);
    std::optional<PolicyGenerationResult> load_generation_result(const std::string& policy_id);
    std::vector<PolicyGenerationResult> load_rule_history(const std::string& rule_id);

    // Utility methods
    std::string normalize_description(const std::string& description);
    std::string generate_rule_name(const std::string& description);
    std::string get_domain_context(PolicyDomain domain);
    std::string get_rule_type_requirements(RuleType type);
    double calculate_rule_complexity(const std::string& code);
    std::string generate_rule_documentation(const GeneratedRule& rule);
    std::string rule_type_to_string(RuleType type);
    std::string domain_to_string(PolicyDomain domain);

    // Cost and token tracking
    std::pair<int, double> calculate_generation_cost(int input_tokens, int output_tokens);
    void update_generation_stats(const PolicyGenerationResult& result);
    int estimate_token_count(const std::string& text);
    double calculate_validation_score(const RuleValidationResult& result);
};

} // namespace regulens

#endif // POLICY_GENERATION_SERVICE_HPP
