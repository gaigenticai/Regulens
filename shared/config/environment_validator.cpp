#include "environment_validator.hpp"
#include <regex>
#include <cstdlib>
#include <iostream>

namespace regulens {

EnvironmentValidator::EnvironmentValidator(StructuredLogger* logger)
    : logger_(logger) {
    load_default_rules();
}

void EnvironmentValidator::load_default_rules() {
    // System configuration
    add_validation_rule(ValidationRule{
        "REGULENS_ENVIRONMENT",
        "Application environment",
        true,
        "production",
        {"development", "staging", "production", "testing"},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "REGULENS_VERSION",
        "Application version",
        false,
        "1.0.0",
        {},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "REGULENS_INSTANCE_ID",
        "Unique instance identifier",
        false,
        "default",
        {},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "REGULENS_DATACENTER",
        "Data center location",
        false,
        "us-east-1",
        {},
        nullptr
    });

    // Primary Database configuration
    add_validation_rule(ValidationRule{
        "DB_HOST",
        "Primary database host",
        true,
        "",
        {},
        [this](const std::string& value) {
            std::string env = get_validated_value("REGULENS_ENVIRONMENT");
            if (env == "production" && (value == "localhost" || value == "127.0.0.1")) {
                return false; // No localhost in production
            }
            return validate_hostname_or_ip(value);
        }
    });

    add_validation_rule(ValidationRule{
        "DB_PORT",
        "Database port",
        false,
        "5432",
        {},
        [this](const std::string& value) { return validate_numeric_range(value, 1, 65535); }
    });

    add_validation_rule(ValidationRule{
        "DB_NAME",
        "Database name",
        true,
        "regulens_compliance",
        {},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "DB_USER",
        "Database username",
        true,
        "regulens_user",
        {},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "DB_PASSWORD",
        "Database password",
        true,
        "",
        {},
        [this](const std::string& value) {
            std::string env = get_validated_value("REGULENS_ENVIRONMENT");
            if (env == "production") {
                return value.length() >= 12 && has_mixed_case(value) && has_digits(value);
            }
            return !value.empty();
        }
    });

    add_validation_rule(ValidationRule{
        "DB_SSL_MODE",
        "SSL mode for database connections",
        false,
        "require",
        {"disable", "require", "verify-ca", "verify-full"},
        [this](const std::string& value) {
            std::string env = get_validated_value("REGULENS_ENVIRONMENT");
            if (env == "production" && value == "disable") {
                return false; // SSL required in production
            }
            return true;
        }
    });

    add_validation_rule(ValidationRule{
        "DB_CONNECTION_POOL_SIZE",
        "Connection pool size",
        false,
        "10",
        {},
        [this](const std::string& value) { return validate_numeric_range(value, 1, 100); }
    });

    add_validation_rule(ValidationRule{
        "DB_CONNECTION_TIMEOUT_MS",
        "Connection timeout in milliseconds",
        false,
        "30000",
        {},
        [this](const std::string& value) { return validate_numeric_range(value, 1000, 120000); }
    });

    add_validation_rule(ValidationRule{
        "DB_MAX_RETRIES",
        "Maximum connection retry attempts",
        false,
        "3",
        {},
        [this](const std::string& value) { return validate_numeric_range(value, 0, 10); }
    });

    // Read Replica Database (optional)
    add_validation_rule(ValidationRule{
        "DATABASE_READ_REPLICA_HOST",
        "Read replica database host",
        false,
        "",
        {},
        [this](const std::string& value) {
            if (value.empty()) return true; // Optional
            std::string env = get_validated_value("REGULENS_ENVIRONMENT");
            if (env == "production" && (value == "localhost" || value == "127.0.0.1")) {
                return false;
            }
            return validate_hostname_or_ip(value);
        }
    });

    add_validation_rule(ValidationRule{
        "DATABASE_READ_REPLICA_PORT",
        "Read replica database port",
        false,
        "5432",
        {},
        [this](const std::string& value) { return validate_numeric_range(value, 1, 65535); }
    });

    // Audit Database configuration
    add_validation_rule(ValidationRule{
        "AUDIT_DB_HOST",
        "Audit database host",
        true,
        "",
        {},
        [this](const std::string& value) {
            std::string env = get_validated_value("REGULENS_ENVIRONMENT");
            if (env == "production" && (value == "localhost" || value == "127.0.0.1")) {
                return false;
            }
            return validate_hostname_or_ip(value);
        }
    });

    add_validation_rule(ValidationRule{
        "AUDIT_DB_PORT",
        "Audit database port",
        false,
        "5432",
        {},
        [this](const std::string& value) { return validate_numeric_range(value, 1, 65535); }
    });

    add_validation_rule(ValidationRule{
        "AUDIT_DB_NAME",
        "Audit database name",
        true,
        "regulens_audit",
        {},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "AUDIT_DB_USER",
        "Audit database username",
        true,
        "regulens_audit_user",
        {},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "AUDIT_DB_PASSWORD",
        "Audit database password",
        true,
        "",
        {},
        [this](const std::string& value) {
            std::string env = get_validated_value("REGULENS_ENVIRONMENT");
            if (env == "production") {
                return value.length() >= 12 && has_mixed_case(value) && has_digits(value);
            }
            return !value.empty();
        }
    });

    // Message Queue configuration
    add_validation_rule(ValidationRule{
        "MESSAGE_QUEUE_TYPE",
        "Message queue type",
        false,
        "kafka",
        {"kafka", "redis", "rabbitmq"},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "MESSAGE_QUEUE_BOOTSTRAP_SERVERS",
        "Message queue bootstrap servers",
        false,
        "localhost:9092",
        {},
        [this](const std::string& value) {
            std::string env = get_validated_value("REGULENS_ENVIRONMENT");
            if (env == "production" && value.find("localhost") != std::string::npos) {
                return false;
            }
            return true;
        }
    });

    add_validation_rule(ValidationRule{
        "MESSAGE_QUEUE_SECURITY_PROTOCOL",
        "Message queue security protocol",
        false,
        "SASL_SSL",
        {"PLAINTEXT", "SSL", "SASL_PLAINTEXT", "SASL_SSL"},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "MESSAGE_QUEUE_SASL_MECHANISM",
        "SASL mechanism for message queue",
        false,
        "PLAIN",
        {"PLAIN", "GSSAPI", "SCRAM-SHA-256", "SCRAM-SHA-512"},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "MESSAGE_QUEUE_SASL_USERNAME",
        "SASL username for message queue",
        false,
        "",
        {},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "MESSAGE_QUEUE_SASL_PASSWORD",
        "SASL password for message queue",
        false,
        "",
        {},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "MESSAGE_QUEUE_SSL_CA_LOCATION",
        "SSL CA location for message queue",
        false,
        "",
        {},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "MESSAGE_QUEUE_SSL_CERTIFICATE_LOCATION",
        "SSL certificate location for message queue",
        false,
        "",
        {},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "MESSAGE_QUEUE_SSL_KEY_LOCATION",
        "SSL key location for message queue",
        false,
        "",
        {},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "MESSAGE_QUEUE_CONSUMER_GROUP",
        "Consumer group for message queue",
        false,
        "regulens_agents",
        {},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "MESSAGE_QUEUE_AUTO_OFFSET_RESET",
        "Auto offset reset for message queue",
        false,
        "earliest",
        {"earliest", "latest", "none"},
        nullptr
    });

    // Regulatory API configuration
    add_validation_rule(ValidationRule{
        "SEC_EDGAR_API_KEY",
        "SEC EDGAR API key",
        false,
        "",
        {},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "SEC_EDGAR_BASE_URL",
        "SEC EDGAR base URL",
        false,
        "https://www.sec.gov/edgar",
        {},
        [this](const std::string& value) { return validate_url_format(value); }
    });

    add_validation_rule(ValidationRule{
        "SEC_EDGAR_RATE_LIMIT_REQUESTS_PER_SECOND",
        "SEC EDGAR rate limit",
        false,
        "10",
        {},
        [this](const std::string& value) { return validate_numeric_range(value, 1, 100); }
    });

    add_validation_rule(ValidationRule{
        "FCA_API_KEY",
        "FCA API key",
        false,
        "",
        {},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "FCA_BASE_URL",
        "FCA base URL",
        false,
        "https://api.fca.org.uk",
        {},
        [this](const std::string& value) { return validate_url_format(value); }
    });

    add_validation_rule(ValidationRule{
        "FCA_RATE_LIMIT_REQUESTS_PER_MINUTE",
        "FCA rate limit",
        false,
        "60",
        {},
        [this](const std::string& value) { return validate_numeric_range(value, 1, 1000); }
    });

    add_validation_rule(ValidationRule{
        "ECB_FEED_URL",
        "ECB feed URL",
        false,
        "https://www.ecb.europa.eu/rss/announcements.xml",
        {},
        [this](const std::string& value) { return validate_url_format(value); }
    });

    add_validation_rule(ValidationRule{
        "ECB_UPDATE_INTERVAL_MINUTES",
        "ECB update interval",
        false,
        "15",
        {},
        [this](const std::string& value) { return validate_numeric_range(value, 1, 1440); }
    });

    add_validation_rule(ValidationRule{
        "CUSTOM_REGULATORY_FEEDS",
        "Custom regulatory feeds (JSON)",
        false,
        "",
        {},
        nullptr
    });

    // External System Integration
    add_validation_rule(ValidationRule{
        "ERP_SYSTEM_TYPE",
        "ERP system type",
        false,
        "sap",
        {"sap", "oracle", "microsoft", "custom"},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "ERP_SYSTEM_HOST",
        "ERP system host",
        false,
        "erp.company.com",
        {},
        [this](const std::string& value) { return validate_hostname_or_ip(value); }
    });

    add_validation_rule(ValidationRule{
        "ERP_SYSTEM_PORT",
        "ERP system port",
        false,
        "443",
        {},
        [this](const std::string& value) { return validate_numeric_range(value, 1, 65535); }
    });

    add_validation_rule(ValidationRule{
        "ERP_SYSTEM_API_KEY",
        "ERP system API key",
        false,
        "",
        {},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "ERP_SYSTEM_USERNAME",
        "ERP system username",
        false,
        "",
        {},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "ERP_SYSTEM_PASSWORD",
        "ERP system password",
        false,
        "",
        {},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "ERP_SYSTEM_TIMEOUT_MS",
        "ERP system timeout",
        false,
        "30000",
        {},
        [this](const std::string& value) { return validate_numeric_range(value, 1000, 120000); }
    });

    add_validation_rule(ValidationRule{
        "DOCUMENT_SYSTEM_TYPE",
        "Document system type",
        false,
        "sharepoint",
        {"sharepoint", "onedrive", "dropbox", "box", "custom"},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "DOCUMENT_SYSTEM_BASE_URL",
        "Document system base URL",
        false,
        "https://company.sharepoint.com",
        {},
        [this](const std::string& value) { return validate_url_format(value); }
    });

    add_validation_rule(ValidationRule{
        "DOCUMENT_SYSTEM_CLIENT_ID",
        "Document system client ID",
        false,
        "",
        {},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "DOCUMENT_SYSTEM_CLIENT_SECRET",
        "Document system client secret",
        false,
        "",
        {},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "DOCUMENT_SYSTEM_TENANT_ID",
        "Document system tenant ID",
        false,
        "",
        {},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "SIEM_SYSTEM_TYPE",
        "SIEM system type",
        false,
        "splunk",
        {"splunk", "elasticsearch", "sumologic", "custom"},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "SIEM_SYSTEM_HOST",
        "SIEM system host",
        false,
        "siem.company.com",
        {},
        [this](const std::string& value) { return validate_hostname_or_ip(value); }
    });

    add_validation_rule(ValidationRule{
        "SIEM_SYSTEM_PORT",
        "SIEM system port",
        false,
        "8089",
        {},
        [this](const std::string& value) { return validate_numeric_range(value, 1, 65535); }
    });

    add_validation_rule(ValidationRule{
        "SIEM_SYSTEM_TOKEN",
        "SIEM system token",
        false,
        "",
        {},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "SIEM_SYSTEM_INDEX",
        "SIEM system index",
        false,
        "compliance_events",
        {},
        nullptr
    });

    // Model Endpoints
    add_validation_rule(ValidationRule{
        "COMPLIANCE_MODEL_ENDPOINT",
        "Compliance model endpoint",
        false,
        "http://localhost:8501/v1/models/compliance_model",
        {},
        [this](const std::string& value) {
            if (value.empty()) return true;
            std::string env = get_validated_value("REGULENS_ENVIRONMENT");
            if (env == "production" && value.find("localhost") != std::string::npos) {
                return false;
            }
            return validate_url_format(value);
        }
    });

    add_validation_rule(ValidationRule{
        "REGULATORY_MODEL_ENDPOINT",
        "Regulatory model endpoint",
        false,
        "http://localhost:8501/v1/models/regulatory_model",
        {},
        [this](const std::string& value) {
            if (value.empty()) return true;
            std::string env = get_validated_value("REGULENS_ENVIRONMENT");
            if (env == "production" && value.find("localhost") != std::string::npos) {
                return false;
            }
            return validate_url_format(value);
        }
    });

    add_validation_rule(ValidationRule{
        "AUDIT_MODEL_ENDPOINT",
        "Audit model endpoint",
        false,
        "http://localhost:8501/v1/models/audit_model",
        {},
        [this](const std::string& value) {
            if (value.empty()) return true;
            std::string env = get_validated_value("REGULENS_ENVIRONMENT");
            if (env == "production" && value.find("localhost") != std::string::npos) {
                return false;
            }
            return validate_url_format(value);
        }
    });

    // Vector Database
    add_validation_rule(ValidationRule{
        "VECTOR_DB_TYPE",
        "Vector database type",
        false,
        "weaviate",
        {"weaviate", "pinecone", "qdrant", "milvus", "chroma"},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "VECTOR_DB_HOST",
        "Vector database host",
        false,
        "localhost",
        {},
        [this](const std::string& value) {
            std::string env = get_validated_value("REGULENS_ENVIRONMENT");
            if (env == "production" && (value == "localhost" || value == "127.0.0.1")) {
                return false;
            }
            return validate_hostname_or_ip(value);
        }
    });

    add_validation_rule(ValidationRule{
        "VECTOR_DB_PORT",
        "Vector database port",
        false,
        "8080",
        {},
        [this](const std::string& value) { return validate_numeric_range(value, 1, 65535); }
    });

    add_validation_rule(ValidationRule{
        "VECTOR_DB_API_KEY",
        "Vector database API key",
        false,
        "",
        {},
        nullptr
    });

    // Embedding Configuration
    add_validation_rule(ValidationRule{
        "EMBEDDING_MODEL_TYPE",
        "Embedding model type",
        false,
        "sentence-transformers",
        {"sentence-transformers", "openai", "cohere", "huggingface"},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "EMBEDDING_MODEL_NAME",
        "Embedding model name",
        false,
        "all-MiniLM-L6-v2",
        {},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "EMBEDDING_DIMENSION",
        "Embedding dimension",
        false,
        "384",
        {},
        [this](const std::string& value) { return validate_numeric_range(value, 64, 4096); }
    });

    // Encryption Keys
    add_validation_rule(ValidationRule{
        "ENCRYPTION_MASTER_KEY",
        "Master encryption key",
        true,
        "",
        {},
        [this](const std::string& value) {
            std::string env = get_validated_value("REGULENS_ENVIRONMENT");
            if (env == "production") {
                return value.length() >= 32 && has_mixed_case(value) && has_digits(value) && has_special_chars(value);
            }
            return value.length() >= 16;
        }
    });

    add_validation_rule(ValidationRule{
        "DATA_ENCRYPTION_KEY",
        "Data encryption key",
        true,
        "",
        {},
        [this](const std::string& value) {
            std::string env = get_validated_value("REGULENS_ENVIRONMENT");
            if (env == "production") {
                return value.length() >= 32 && has_mixed_case(value) && has_digits(value) && has_special_chars(value);
            }
            return value.length() >= 16;
        }
    });

    // JWT Security
    add_validation_rule(ValidationRule{
        "JWT_SECRET_KEY",
        "JWT signing key",
        true,
        "",
        {},
        [this](const std::string& value) {
            std::string env = get_validated_value("REGULENS_ENVIRONMENT");
            if (env == "production") {
                return value.length() >= 64 && has_mixed_case(value) && has_digits(value) && has_special_chars(value);
            }
            return value.length() >= 32;
        }
    });

    // Agent Capabilities
    add_validation_rule(ValidationRule{
        "AGENT_ENABLE_WEB_SEARCH",
        "Enable web search capabilities",
        false,
        "false",
        {"true", "false"},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "AGENT_ENABLE_MCP_TOOLS",
        "Enable MCP tools",
        false,
        "false",
        {"true", "false"},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "AGENT_ENABLE_ADVANCED_DISCOVERY",
        "Enable advanced agent discovery",
        false,
        "false",
        {"true", "false"},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "AGENT_ENABLE_AUTONOMOUS_INTEGRATION",
        "Enable autonomous tool integration",
        false,
        "false",
        {"true", "false"},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "AGENT_MAX_AUTONOMOUS_TOOLS",
        "Maximum autonomous tools per session",
        false,
        "5",
        {},
        [this](const std::string& value) { return validate_numeric_range(value, 1, 50); }
    });

    add_validation_rule(ValidationRule{
        "AGENT_ALLOWED_TOOL_CATEGORIES",
        "Allowed tool categories (comma-separated)",
        false,
        "COMMUNICATION,ERP,CRM,DMS,STORAGE,ANALYTICS,WORKFLOW,INTEGRATION,SECURITY,MONITORING",
        {},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "AGENT_BLOCKED_TOOL_DOMAINS",
        "Blocked tool domains (comma-separated)",
        false,
        "",
        {},
        nullptr
    });

    // SMTP Configuration
    add_validation_rule(ValidationRule{
        "SMTP_HOST",
        "SMTP server host",
        false,
        "smtp.gmail.com",
        {},
        [this](const std::string& value) { return validate_hostname_or_ip(value); }
    });

    add_validation_rule(ValidationRule{
        "SMTP_PORT",
        "SMTP server port",
        false,
        "587",
        {},
        [this](const std::string& value) { return validate_numeric_range(value, 1, 65535); }
    });

    add_validation_rule(ValidationRule{
        "SMTP_USER",
        "SMTP username",
        false,
        "",
        {},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "SMTP_PASSWORD",
        "SMTP password",
        false,
        "",
        {},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "SMTP_FROM_EMAIL",
        "SMTP from email address",
        false,
        "regulens@gaigentic.ai",
        {},
        [this](const std::string& value) { return validate_email_format(value); }
    });

    // LLM Configuration
    add_validation_rule(ValidationRule{
        "LLM_OPENAI_API_KEY",
        "OpenAI API key",
        false,
        "",
        {},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "LLM_OPENAI_BASE_URL",
        "OpenAI base URL",
        false,
        "https://api.openai.com/v1",
        {},
        [this](const std::string& value) { return validate_url_format(value); }
    });

    add_validation_rule(ValidationRule{
        "LLM_OPENAI_MODEL",
        "OpenAI model",
        false,
        "gpt-4-turbo-preview",
        {},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "LLM_ANTHROPIC_API_KEY",
        "Anthropic API key",
        false,
        "",
        {},
        nullptr
    });

    add_validation_rule(ValidationRule{
        "LLM_ANTHROPIC_BASE_URL",
        "Anthropic base URL",
        false,
        "https://api.anthropic.com",
        {},
        [this](const std::string& value) { return validate_url_format(value); }
    });

    add_validation_rule(ValidationRule{
        "LLM_ANTHROPIC_MODEL",
        "Anthropic model",
        false,
        "claude-3-sonnet-20240229",
        {},
        nullptr
    });
}

void EnvironmentValidator::add_validation_rule(const ValidationRule& rule) {
    validation_rules_[rule.name] = rule;
}

EnvironmentValidator::ValidationResult EnvironmentValidator::validate_all() {
    ValidationResult result;

    if (logger_) {
        logger_->log(LogLevel::INFO, "Starting comprehensive environment validation");
    }

    for (const auto& [key, rule] : validation_rules_) {
        std::string value = get_env_or_default(key, rule.default_value);

        // Check required fields
        if (rule.required && value.empty()) {
            result.errors.push_back("Required environment variable '" + key + "' is not set");
            result.valid = false;
            continue;
        }

        // Check allowed values
        if (!rule.allowed_values.empty()) {
            bool allowed = false;
            for (const auto& allowed_val : rule.allowed_values) {
                if (value == allowed_val) {
                    allowed = true;
                    break;
                }
            }
            if (!allowed) {
                result.errors.push_back("Environment variable '" + key + "' has invalid value '" + value +
                                      "'. Allowed values: " + join_vector(rule.allowed_values, ", "));
                result.valid = false;
            }
        }

        // Custom validation
        if (rule.custom_validator && !rule.custom_validator(value)) {
            result.errors.push_back("Environment variable '" + key + "' failed custom validation");
            result.valid = false;
        }

        // Store validated value
        result.validated_config[key] = value;
    }

    // Run category-specific validations
    if (!validate_database_config()) {
        result.errors.push_back("Database configuration validation failed - check host, credentials, and SSL settings");
        result.valid = false;
    }

    if (!validate_llm_config()) {
        result.errors.push_back("LLM configuration validation failed - production requires at least one LLM provider");
        result.valid = false;
    }

    if (!validate_agent_config()) {
        result.errors.push_back("Agent configuration validation failed - check agent capability flags and dependencies");
        result.valid = false;
    }

    if (!validate_security_config()) {
        result.errors.push_back("Security configuration validation failed - check encryption keys and JWT settings");
        result.valid = false;
    }

    if (!validate_cloud_deployment_config()) {
        result.errors.push_back("Cloud deployment validation failed - no localhost allowed in production");
        result.valid = false;
    }

    if (!validate_dependency_config()) {
        result.errors.push_back("Dependency validation failed - check feature dependencies (LLM for agents, etc.)");
        result.valid = false;
    }

    if (logger_) {
        if (result.valid) {
            logger_->log(LogLevel::INFO, "Environment validation completed successfully");
        } else {
            logger_->log(LogLevel::ERROR, "Environment validation failed with " +
                        std::to_string(result.errors.size()) + " errors");
        }
    }

    return result;
}

bool EnvironmentValidator::validate_single(const std::string& key) {
    auto it = validation_rules_.find(key);
    if (it == validation_rules_.end()) {
        return true; // Unknown key, consider valid
    }

    const auto& rule = it->second;
    std::string value = get_env_or_default(key, rule.default_value);

    // Basic validation
    if (rule.required && value.empty()) {
        return false;
    }

    if (!rule.allowed_values.empty()) {
        return std::find(rule.allowed_values.begin(), rule.allowed_values.end(), value) != rule.allowed_values.end();
    }

    if (rule.custom_validator) {
        return rule.custom_validator(value);
    }

    return true;
}

std::string EnvironmentValidator::get_validated_value(const std::string& key) {
    auto it = validation_rules_.find(key);
    if (it != validation_rules_.end()) {
        return get_env_or_default(key, it->second.default_value);
    }
    return get_env_or_default(key);
}

bool EnvironmentValidator::is_required_set(const std::string& key) {
    auto it = validation_rules_.find(key);
    if (it != validation_rules_.end() && it->second.required) {
        return !get_env_or_default(key).empty();
    }
    return true;
}

nlohmann::json EnvironmentValidator::get_all_validated_config() {
    nlohmann::json config;
    for (const auto& [key, rule] : validation_rules_) {
        config[key] = get_validated_value(key);
    }
    return config;
}

// Private validation methods
bool EnvironmentValidator::validate_database_config() {
    std::string env = get_validated_value("REGULENS_ENVIRONMENT");

    // Primary database validation
    std::string host = get_validated_value("DB_HOST");
    std::string port = get_validated_value("DB_PORT");
    std::string name = get_validated_value("DB_NAME");
    std::string user = get_validated_value("DB_USER");
    std::string password = get_validated_value("DB_PASSWORD");
    std::string ssl_mode = get_validated_value("DB_SSL_MODE");

    if (host.empty() || name.empty() || user.empty()) {
        return false;
    }

    // Production requirements
    if (env == "production") {
        if (password.empty()) return false;
        if (ssl_mode == "disable") return false;
        if (host == "localhost" || host == "127.0.0.1") return false;
    }

    // Audit database validation
    std::string audit_host = get_validated_value("AUDIT_DB_HOST");
    std::string audit_user = get_validated_value("AUDIT_DB_USER");
    std::string audit_password = get_validated_value("AUDIT_DB_PASSWORD");

    if (audit_host.empty() || audit_user.empty()) {
        return false;
    }

    if (env == "production") {
        if (audit_password.empty()) return false;
        if (audit_host == "localhost" || audit_host == "127.0.0.1") return false;
    }

    return true;
}

bool EnvironmentValidator::validate_llm_config() {
    // At least one LLM provider should be configured for full functionality
    bool has_openai = !get_validated_value("LLM_OPENAI_API_KEY").empty();
    bool has_anthropic = !get_validated_value("LLM_ANTHROPIC_API_KEY").empty();

    // If LLM features are enabled, at least one provider should be configured
    std::string env = get_validated_value("REGULENS_ENVIRONMENT");
    if (env == "production" && !has_openai && !has_anthropic) {
        return false; // Production requires at least one LLM provider
    }

    return true;
}

bool EnvironmentValidator::validate_agent_config() {
    // Validate agent capability flags
    std::vector<std::string> agent_flags = {
        "AGENT_ENABLE_WEB_SEARCH",
        "AGENT_ENABLE_MCP_TOOLS",
        "AGENT_ENABLE_ADVANCED_DISCOVERY",
        "AGENT_ENABLE_AUTONOMOUS_INTEGRATION"
    };

    for (const auto& flag : agent_flags) {
        std::string value = get_validated_value(flag);
        if (value != "true" && value != "false") {
            return false;
        }
    }

    // If autonomous integration is enabled, check dependencies
    if (get_validated_value("AGENT_ENABLE_AUTONOMOUS_INTEGRATION") == "true") {
        std::string max_tools = get_validated_value("AGENT_MAX_AUTONOMOUS_TOOLS");
        try {
            int max = std::stoi(max_tools);
            if (max < 1 || max > 50) return false;
        } catch (...) {
            return false;
        }
    }

    return true;
}

bool EnvironmentValidator::validate_security_config() {
    std::string env = get_validated_value("REGULENS_ENVIRONMENT");

    // Encryption keys are required
    std::string master_key = get_validated_value("ENCRYPTION_MASTER_KEY");
    std::string data_key = get_validated_value("DATA_ENCRYPTION_KEY");
    std::string jwt_key = get_validated_value("JWT_SECRET_KEY");

    if (master_key.empty() || data_key.empty() || jwt_key.empty()) {
        return false;
    }

    // Production security requirements
    if (env == "production") {
        if (master_key.length() < 32 || !has_mixed_case(master_key) ||
            !has_digits(master_key) || !has_special_chars(master_key)) {
            return false;
        }
        if (data_key.length() < 32 || !has_mixed_case(data_key) ||
            !has_digits(data_key) || !has_special_chars(data_key)) {
            return false;
        }
        if (jwt_key.length() < 64 || !has_mixed_case(jwt_key) ||
            !has_digits(jwt_key) || !has_special_chars(jwt_key)) {
            return false;
        }
    }

    return true;
}

bool EnvironmentValidator::validate_cloud_deployment_config() {
    std::string env = get_validated_value("REGULENS_ENVIRONMENT");

    // Skip cloud validation for development/testing
    if (env == "development" || env == "testing") {
        return true;
    }

    // Production cloud deployment validations
    if (env == "production") {
        // No localhost references in production
        std::vector<std::string> host_vars = {
            "DB_HOST", "AUDIT_DB_HOST", "VECTOR_DB_HOST",
            "ERP_SYSTEM_HOST", "SIEM_SYSTEM_HOST", "SMTP_HOST"
        };

        for (const auto& var : host_vars) {
            std::string value = get_validated_value(var);
            if (!value.empty() && (value == "localhost" || value == "127.0.0.1" ||
                                   value.find("localhost") != std::string::npos)) {
                return false;
            }
        }

        // Model endpoints should not use localhost
        std::vector<std::string> endpoint_vars = {
            "COMPLIANCE_MODEL_ENDPOINT", "REGULATORY_MODEL_ENDPOINT", "AUDIT_MODEL_ENDPOINT"
        };

        for (const auto& var : endpoint_vars) {
            std::string value = get_validated_value(var);
            if (!value.empty() && value.find("localhost") != std::string::npos) {
                return false;
            }
        }

        // Message queue should not use localhost
        std::string mq_servers = get_validated_value("MESSAGE_QUEUE_BOOTSTRAP_SERVERS");
        if (!mq_servers.empty() && mq_servers.find("localhost") != std::string::npos) {
            return false;
        }
    }

    return true;
}

bool EnvironmentValidator::validate_dependency_config() {
    // Validate that if certain features are enabled, their dependencies are configured

    // If MCP tools are enabled, check if agent capabilities allow it
    if (get_validated_value("AGENT_ENABLE_MCP_TOOLS") == "true") {
        // MCP tools require autonomous integration capability
        if (get_validated_value("AGENT_ENABLE_AUTONOMOUS_INTEGRATION") != "true") {
            return false;
        }
    }

    // If agent features are enabled, ensure LLM is configured
    std::string env = get_validated_value("REGULENS_ENVIRONMENT");
    if (env == "production") {
        bool agent_features_enabled = (
            get_validated_value("AGENT_ENABLE_WEB_SEARCH") == "true" ||
            get_validated_value("AGENT_ENABLE_MCP_TOOLS") == "true" ||
            get_validated_value("AGENT_ENABLE_ADVANCED_DISCOVERY") == "true" ||
            get_validated_value("AGENT_ENABLE_AUTONOMOUS_INTEGRATION") == "true"
        );

        bool llm_configured = (
            !get_validated_value("LLM_OPENAI_API_KEY").empty() ||
            !get_validated_value("LLM_ANTHROPIC_API_KEY").empty()
        );

        if (agent_features_enabled && !llm_configured) {
            return false;
        }
    }

    return true;
}

// Helper methods
std::string EnvironmentValidator::get_env_or_default(const std::string& key, const std::string& default_val) {
    const char* value = std::getenv(key.c_str());
    return value ? std::string(value) : default_val;
}

bool EnvironmentValidator::validate_numeric_range(const std::string& value, int min_val, int max_val) {
    try {
        int num = std::stoi(value);
        return num >= min_val && num <= max_val;
    } catch (...) {
        return false;
    }
}

bool EnvironmentValidator::validate_url_format(const std::string& value) {
    std::regex url_regex(R"(^https?://[^\s/$.?#].[^\s]*$)");
    return std::regex_match(value, url_regex);
}

bool EnvironmentValidator::validate_email_format(const std::string& value) {
    std::regex email_regex(R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
    return std::regex_match(value, email_regex);
}

bool EnvironmentValidator::validate_hostname_or_ip(const std::string& value) {
    if (value.empty()) return false;

    // Check if it's an IP address (basic validation)
    std::regex ip_regex(R"(^(\d{1,3}\.){3}\d{1,3}$)");
    if (std::regex_match(value, ip_regex)) {
        // Validate each octet
        std::stringstream ss(value);
        std::string octet;
        while (std::getline(ss, octet, '.')) {
            try {
                int num = std::stoi(octet);
                if (num < 0 || num > 255) return false;
            } catch (...) {
                return false;
            }
        }
        return true;
    }

    // Check if it's a valid hostname
    std::regex hostname_regex(R"(^[a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?(\.[a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?)*$)");
    return std::regex_match(value, hostname_regex) && value.length() <= 253;
}

bool EnvironmentValidator::has_mixed_case(const std::string& value) {
    bool has_lower = false, has_upper = false;
    for (char c : value) {
        if (std::islower(c)) has_lower = true;
        if (std::isupper(c)) has_upper = true;
        if (has_lower && has_upper) return true;
    }
    return has_lower && has_upper;
}

bool EnvironmentValidator::has_digits(const std::string& value) {
    for (char c : value) {
        if (std::isdigit(c)) return true;
    }
    return false;
}

bool EnvironmentValidator::has_special_chars(const std::string& value) {
    for (char c : value) {
        if (!std::isalnum(c)) return true;
    }
    return false;
}

std::string EnvironmentValidator::join_vector(const std::vector<std::string>& vec, const std::string& delimiter) {
    std::string result;
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) result += delimiter;
        result += vec[i];
    }
    return result;
}

} // namespace regulens
