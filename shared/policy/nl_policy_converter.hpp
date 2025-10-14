/**
 * NL Policy Converter Service
 * Converts natural language policy descriptions to structured policy definitions using LLM
 * Production-grade policy generation with validation and deployment tracking
 */

#ifndef REGULENS_NL_POLICY_CONVERTER_HPP
#define REGULENS_NL_POLICY_CONVERTER_HPP

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <chrono>
#include <nlohmann/json.hpp>
#include "../database/postgresql_connection.hpp"
#include "../llm/openai_client.hpp"
#include "../logging/structured_logger.hpp"

namespace regulens {
namespace policy {

struct PolicyTemplate {
    std::string template_id;
    std::string template_name;
    std::string template_description;
    std::string policy_type; // 'fraud_rule', 'compliance_rule', 'validation_rule', 'risk_rule'
    std::string template_prompt;
    nlohmann::json input_schema;
    nlohmann::json output_schema;
    std::vector<std::string> example_inputs;
    std::vector<nlohmann::json> example_outputs;
    bool is_active = true;
    std::string category; // 'financial', 'compliance', 'security', 'operational'
    int usage_count = 0;
    double success_rate = 0.0;
    double average_confidence = 0.0;
};

struct PolicyConversionRequest {
    std::string natural_language_input;
    std::string policy_type;
    std::string user_id;
    std::optional<std::string> template_id;
    std::optional<nlohmann::json> additional_context;
    std::optional<std::string> target_system;
    bool auto_validate = true;
    int max_retries = 2;
};

struct PolicyConversionResult {
    std::string conversion_id;
    nlohmann::json generated_policy;
    double confidence_score = 0.0;
    std::vector<std::string> validation_errors;
    std::vector<std::string> validation_warnings;
    std::string status; // 'draft', 'approved', 'deployed', 'rejected'
    std::chrono::milliseconds processing_time;
    int tokens_used = 0;
    double cost = 0.0;
    std::string template_used;
    nlohmann::json metadata;
    bool success = true;
    std::optional<std::string> error_message;
};

struct PolicyValidationResult {
    bool is_valid = false;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::vector<std::string> suggestions;
    double validation_score = 0.0; // 0-1 scale
};

struct PolicyDeploymentRequest {
    std::string conversion_id;
    std::string target_system; // 'fraud_detection', 'compliance_monitor', 'validation_engine', 'risk_assessment'
    std::string deployed_by;
    std::optional<nlohmann::json> deployment_options;
};

struct PolicyDeploymentResult {
    std::string deployment_id;
    bool success = false;
    std::optional<std::string> error_message;
    std::optional<nlohmann::json> deployed_policy;
    std::string status; // 'pending', 'deployed', 'failed', 'rolled_back'
};

class NLPolicyConverter {
public:
    NLPolicyConverter(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<OpenAIClient> openai_client,
        std::shared_ptr<StructuredLogger> logger
    );

    ~NLPolicyConverter();

    // Core conversion functionality
    PolicyConversionResult convert_natural_language(const PolicyConversionRequest& request);

    // Policy validation
    PolicyValidationResult validate_policy(const nlohmann::json& policy, const std::string& policy_type);

    // Template management
    std::vector<PolicyTemplate> get_available_templates(const std::string& policy_type = "");
    std::optional<PolicyTemplate> get_template(const std::string& template_id);
    bool create_template(const PolicyTemplate& template_data, const std::string& user_id);
    bool update_template_usage(const std::string& template_id, bool success, double confidence);

    // Policy deployment
    PolicyDeploymentResult deploy_policy(const PolicyDeploymentRequest& request);

    // Conversion history and feedback
    std::vector<nlohmann::json> get_user_conversions(const std::string& user_id, int limit = 50, int offset = 0);
    std::optional<nlohmann::json> get_conversion(const std::string& conversion_id);
    bool submit_feedback(const std::string& conversion_id, const std::string& feedback, int rating = 0);
    bool update_conversion_status(const std::string& conversion_id, const std::string& status,
                                  const std::optional<std::string>& reason = std::nullopt);

    // Analytics and insights
    nlohmann::json get_conversion_analytics(const std::string& user_id,
                                          const std::optional<std::string>& time_range = std::nullopt);
    std::vector<std::string> get_popular_templates(int limit = 10);
    nlohmann::json get_success_rates_by_policy_type();

    // Configuration
    void set_default_model(const std::string& model);
    void set_validation_strictness(double strictness); // 0.0 - 1.0
    void set_max_retries(int max_retries);
    void set_template_cache_enabled(bool enabled);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<OpenAIClient> openai_client_;
    std::shared_ptr<StructuredLogger> logger_;

    // Configuration
    std::string default_model_ = "gpt-4-turbo-preview";
    double validation_strictness_ = 0.8; // 0.0 = lenient, 1.0 = strict
    int max_retries_ = 2;
    bool template_cache_enabled_ = true;
    int template_cache_ttl_hours_ = 24;

    // Internal methods
    std::string build_conversion_prompt(const PolicyConversionRequest& request, const PolicyTemplate& tmpl);
    PolicyConversionResult call_llm_for_conversion(const PolicyConversionRequest& request, const std::string& prompt);
    PolicyConversionResult parse_llm_response(const std::string& llm_response, const PolicyConversionRequest& request);

    PolicyValidationResult validate_fraud_rule(const nlohmann::json& policy);
    PolicyValidationResult validate_compliance_rule(const nlohmann::json& policy);
    PolicyValidationResult validate_validation_rule(const nlohmann::json& policy);
    PolicyValidationResult validate_risk_rule(const nlohmann::json& policy);

    std::string store_conversion_result(const PolicyConversionRequest& request, const PolicyConversionResult& result);
    bool update_template_statistics(const std::string& template_id, bool success, double confidence);

    // Template caching
    std::optional<PolicyTemplate> get_cached_template(const std::string& template_id);
    void cache_template(const std::string& template_id, const PolicyTemplate& tmpl);

    // Deployment methods
    PolicyDeploymentResult deploy_to_fraud_detection(const PolicyDeploymentRequest& request, const nlohmann::json& policy);
    PolicyDeploymentResult deploy_to_compliance_monitor(const PolicyDeploymentRequest& request, const nlohmann::json& policy);
    PolicyDeploymentResult deploy_to_validation_engine(const PolicyDeploymentRequest& request, const nlohmann::json& policy);
    PolicyDeploymentResult deploy_to_risk_assessment(const PolicyDeploymentRequest& request, const nlohmann::json& policy);

    // Utility methods
    std::string generate_uuid();
    nlohmann::json create_default_policy_structure(const std::string& policy_type);
    bool is_valid_policy_type(const std::string& policy_type);
    double calculate_confidence_score(const nlohmann::json& policy, const std::string& policy_type);
    std::vector<std::string> extract_policy_keywords(const std::string& natural_language);

    // Error handling and fallback
    PolicyConversionResult create_fallback_result(const std::string& error_message);
    PolicyValidationResult create_validation_error(const std::string& error_message);

    // Logging helpers
    void log_conversion_attempt(const PolicyConversionRequest& request);
    void log_conversion_success(const PolicyConversionResult& result);
    void log_conversion_failure(const PolicyConversionRequest& request, const std::string& error);
    void log_deployment_attempt(const PolicyDeploymentRequest& request);
    void log_deployment_result(const PolicyDeploymentResult& result);
};

} // namespace policy
} // namespace regulens

#endif // REGULENS_NL_POLICY_CONVERTER_HPP
