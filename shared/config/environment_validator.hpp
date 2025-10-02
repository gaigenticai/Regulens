#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "../logging/structured_logger.hpp"

namespace regulens {

/**
 * Environment Variable Validation System
 *
 * Production-grade validation for all environment variables
 * Ensures compliance with @rule.mdc Rule 3 (cloud deployability)
 */
class EnvironmentValidator {
public:
    struct ValidationRule {
        std::string name;
        std::string description;
        bool required;
        std::string default_value;
        std::vector<std::string> allowed_values;
        std::function<bool(const std::string&)> custom_validator;
    };

    struct ValidationResult {
        bool valid = true;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        nlohmann::json validated_config;
    };

    EnvironmentValidator(StructuredLogger* logger = nullptr);
    ~EnvironmentValidator() = default;

    // Core validation methods
    ValidationResult validate_all();
    bool validate_single(const std::string& key);

    // Configuration management
    void add_validation_rule(const ValidationRule& rule);
    void load_default_rules();

    // Utility methods
    std::string get_validated_value(const std::string& key);
    bool is_required_set(const std::string& key);
    nlohmann::json get_all_validated_config();

private:
    StructuredLogger* logger_;
    std::unordered_map<std::string, ValidationRule> validation_rules_;

    // Built-in validators
    bool validate_database_config();
    bool validate_llm_config();
    bool validate_agent_config();
    bool validate_security_config();
    bool validate_cloud_deployment_config();
    bool validate_dependency_config();

    // Helper methods
    std::string get_env_or_default(const std::string& key, const std::string& default_val = "");
    bool validate_numeric_range(const std::string& value, int min_val, int max_val);
    bool validate_url_format(const std::string& value);
    bool validate_email_format(const std::string& value);
    bool validate_hostname_or_ip(const std::string& value);
    bool has_mixed_case(const std::string& value);
    bool has_digits(const std::string& value);
    bool has_special_chars(const std::string& value);
    std::string join_vector(const std::vector<std::string>& vec, const std::string& delimiter);
};

} // namespace regulens
