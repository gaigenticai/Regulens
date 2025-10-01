#include "configuration_manager.hpp"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <boost/program_options.hpp>
#include <filesystem>

namespace regulens {

ConfigurationManager& ConfigurationManager::get_instance() {
    static ConfigurationManager instance;
    return instance;
}

bool ConfigurationManager::load_configuration(int argc, char* argv[]) {
    namespace po = boost::program_options;

    try {
        // Command line options
        po::options_description cmd_options("Command line options");
        cmd_options.add_options()
            ("help,h", "produce help message")
            ("config,c", po::value<std::string>()->default_value("config.json"), "configuration file")
            ("env-file,e", po::value<std::string>()->default_value(".env"), "environment file")
            ("validate-only,v", "validate configuration and exit");

        // Parse command line
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, cmd_options), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << cmd_options << std::endl;
            return false;
        }

        // Load environment variables first
        if (!load_from_environment()) {
            std::cerr << "Warning: Failed to load some environment variables" << std::endl;
        }

        // Load from .env file if it exists
        std::string env_file = vm["env-file"].as<std::string>();
        if (std::filesystem::exists(env_file)) {
            if (!load_from_env_file(env_file)) {
                std::cerr << "Warning: Failed to load .env file: " << env_file << std::endl;
            }
        }

        // Load from config file if it exists
        std::string config_file = vm["config"].as<std::string>();
        if (std::filesystem::exists(config_file)) {
            config_file_path_ = config_file;
            if (!load_from_config_file(std::filesystem::path(config_file))) {
                std::cerr << "Warning: Failed to load config file: " << config_file << std::endl;
            }
        }

        // Set defaults for missing values
        set_defaults();

        // Validate if requested
        if (vm.count("validate-only")) {
            ValidationResult validation = validate_configuration();
            if (validation.valid) {
                std::cout << "Configuration validation successful" << std::endl;
                return false; // Exit after validation
            } else {
                std::cerr << "Configuration validation failed: " << validation.error_message << std::endl;
                return false;
            }
        }

        return validate_configuration().valid;

    } catch (const std::exception& e) {
        std::cerr << "Configuration loading error: " << e.what() << std::endl;
        return false;
    }
}

bool ConfigurationManager::load_from_environment() {
    // Load all known configuration keys from environment
    std::vector<std::string> env_keys = {
        config_keys::ENVIRONMENT,
        config_keys::VERSION,
        config_keys::INSTANCE_ID,
        config_keys::DATACENTER,
        // Database
        config_keys::DATABASE_HOST,
        config_keys::DATABASE_PORT,
        config_keys::DATABASE_NAME,
        config_keys::DATABASE_USER,
        config_keys::DATABASE_PASSWORD,
        config_keys::DATABASE_SSL_MODE,
        config_keys::DATABASE_CONNECTION_POOL_SIZE,
        config_keys::DATABASE_CONNECTION_TIMEOUT_MS,
        config_keys::DATABASE_MAX_RETRIES,
        config_keys::DATABASE_READ_REPLICA_HOST,
        config_keys::DATABASE_READ_REPLICA_PORT,
        // Message queue
        config_keys::MESSAGE_QUEUE_TYPE,
        config_keys::MESSAGE_QUEUE_BOOTSTRAP_SERVERS,
        config_keys::MESSAGE_QUEUE_SECURITY_PROTOCOL,
        config_keys::MESSAGE_QUEUE_SASL_MECHANISM,
        config_keys::MESSAGE_QUEUE_SASL_USERNAME,
        config_keys::MESSAGE_QUEUE_SASL_PASSWORD,
        // Regulatory sources
        config_keys::SEC_EDGAR_API_KEY,
        config_keys::SEC_EDGAR_BASE_URL,
        config_keys::SEC_EDGAR_RATE_LIMIT_REQUESTS_PER_SECOND,
        config_keys::FCA_API_KEY,
        config_keys::FCA_BASE_URL,
        config_keys::FCA_RATE_LIMIT_REQUESTS_PER_MINUTE,
        config_keys::ECB_FEED_URL,
        config_keys::ECB_UPDATE_INTERVAL_MINUTES,
        config_keys::CUSTOM_REGULATORY_FEEDS,
        // External systems
        config_keys::ERP_SYSTEM_TYPE,
        config_keys::ERP_SYSTEM_HOST,
        config_keys::ERP_SYSTEM_PORT,
        config_keys::ERP_SYSTEM_API_KEY,
        config_keys::ERP_SYSTEM_USERNAME,
        config_keys::ERP_SYSTEM_PASSWORD,
        config_keys::ERP_SYSTEM_TIMEOUT_MS,
        config_keys::DOCUMENT_SYSTEM_TYPE,
        config_keys::DOCUMENT_SYSTEM_BASE_URL,
        config_keys::DOCUMENT_SYSTEM_CLIENT_ID,
        config_keys::DOCUMENT_SYSTEM_CLIENT_SECRET,
        config_keys::DOCUMENT_SYSTEM_TENANT_ID,
        config_keys::SIEM_SYSTEM_TYPE,
        config_keys::SIEM_SYSTEM_HOST,
        config_keys::SIEM_SYSTEM_PORT,
        config_keys::SIEM_SYSTEM_TOKEN,
        config_keys::SIEM_SYSTEM_INDEX,
        // AI/ML
        config_keys::COMPLIANCE_MODEL_ENDPOINT,
        config_keys::REGULATORY_MODEL_ENDPOINT,
        config_keys::AUDIT_MODEL_ENDPOINT,
        config_keys::VECTOR_DB_TYPE,
        config_keys::VECTOR_DB_HOST,
        config_keys::VECTOR_DB_PORT,
        config_keys::VECTOR_DB_API_KEY,
        config_keys::EMBEDDING_MODEL_TYPE,
        config_keys::EMBEDDING_MODEL_NAME,
        config_keys::EMBEDDING_DIMENSION,
        // Security
        config_keys::ENCRYPTION_MASTER_KEY,
        config_keys::DATA_ENCRYPTION_KEY,
        config_keys::JWT_SECRET_KEY
    };

    bool all_loaded = true;
    for (const auto& key : env_keys) {
        const char* env_value = std::getenv(key.c_str());
        if (env_value) {
            config_values_[key] = std::string(env_value);
        } else {
            all_loaded = false; // Some environment variables might not be set
        }
    }

    return all_loaded; // Return false if any env vars are missing (acceptable)
}

bool ConfigurationManager::load_from_env_file(const std::string& env_file) {
    std::ifstream file(env_file);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Parse KEY=VALUE format
        size_t equals_pos = line.find('=');
        if (equals_pos != std::string::npos) {
            std::string key = line.substr(0, equals_pos);
            std::string value = line.substr(equals_pos + 1);

            // Trim whitespace
            key.erase(key.begin(), std::find_if(key.begin(), key.end(), [](int ch) {
                return !std::isspace(ch);
            }));
            key.erase(std::find_if(key.rbegin(), key.rend(), [](int ch) {
                return !std::isspace(ch);
            }).base(), key.end());

            value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](int ch) {
                return !std::isspace(ch);
            }));
            value.erase(std::find_if(value.rbegin(), value.rend(), [](int ch) {
                return !std::isspace(ch);
            }).base(), value.end());

            if (!key.empty()) {
                config_values_[key] = value;
            }
        }
    }

    return true;
}

bool ConfigurationManager::load_from_config_file(const std::filesystem::path& config_path) {
    try {
        std::ifstream file(config_path);
        if (!file.is_open()) {
            return false;
        }

        nlohmann::json config_json;
        file >> config_json;

        // Flatten JSON into key-value pairs
        flatten_json(config_json, "");

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void ConfigurationManager::flatten_json(const nlohmann::json& json, const std::string& prefix) {
    for (auto it = json.begin(); it != json.end(); ++it) {
        std::string key = prefix.empty() ? it.key() : prefix + "." + it.key();

        if (it->is_object()) {
            flatten_json(*it, key);
        } else {
            config_values_[key] = it->dump();
        }
    }
}

void ConfigurationManager::set_defaults() {
    // Set default values for required configuration that might be missing

    // System defaults
    if (config_values_.find(config_keys::ENVIRONMENT) == config_values_.end()) {
        config_values_[config_keys::ENVIRONMENT] = "development";
    }

    if (config_values_.find(config_keys::VERSION) == config_values_.end()) {
        config_values_[config_keys::VERSION] = REGULENS_VERSION;
    }

    if (config_values_.find(config_keys::INSTANCE_ID) == config_values_.end()) {
        config_values_[config_keys::INSTANCE_ID] = "default";
    }

    // Database defaults
    if (config_values_.find(config_keys::DATABASE_HOST) == config_values_.end()) {
        config_values_[config_keys::DATABASE_HOST] = "localhost";
    }

    if (config_values_.find(config_keys::DATABASE_PORT) == config_values_.end()) {
        config_values_[config_keys::DATABASE_PORT] = "5432";
    }

    if (config_values_.find(config_keys::DATABASE_CONNECTION_POOL_SIZE) == config_values_.end()) {
        config_values_[config_keys::DATABASE_CONNECTION_POOL_SIZE] = "10";
    }

    // Add more defaults as needed...
}

ValidationResult ConfigurationManager::validate_configuration() const {
    // Required configuration validation
    std::vector<std::string> required_keys = {
        config_keys::ENVIRONMENT,
        config_keys::DATABASE_HOST,
        config_keys::DATABASE_NAME,
        config_keys::DATABASE_USER
    };

    for (const auto& key : required_keys) {
        if (config_values_.find(key) == config_values_.end() ||
            config_values_.at(key).empty()) {
            return ValidationResult(false, "Missing required configuration: " + key);
        }
    }

    // Environment-specific validation
    std::string environment = get_string(config_keys::ENVIRONMENT).value_or("development");
    if (environment == "production") {
        // Production-specific validations
        if (!get_string(config_keys::DATABASE_PASSWORD)) {
            return ValidationResult(false, "Database password required in production");
        }

        if (!get_string(config_keys::ENCRYPTION_MASTER_KEY)) {
            return ValidationResult(false, "Encryption master key required in production");
        }
    }

    // Validate port numbers
    if (auto port_str = get_string(config_keys::DATABASE_PORT)) {
        try {
            int port = std::stoi(*port_str);
            if (port < 1 || port > 65535) {
                return ValidationResult(false, "Invalid database port number");
            }
        } catch (...) {
            return ValidationResult(false, "Invalid database port format");
        }
    }

    return ValidationResult(true);
}

bool ConfigurationManager::reload() {
    // Clear existing configuration except environment variables
    auto env_vars = config_values_;
    config_values_.clear();

    // Reload from sources
    load_from_environment();

    if (!config_file_path_.empty() && std::filesystem::exists(config_file_path_)) {
        load_from_config_file(config_file_path_);
    }

    set_defaults();

    return validate_configuration().valid;
}

// Getter implementations
std::optional<std::string> ConfigurationManager::get_string(const std::string& key) const {
    auto it = config_values_.find(key);
    return (it != config_values_.end()) ? std::optional<std::string>(it->second) : std::nullopt;
}

std::optional<int> ConfigurationManager::get_int(const std::string& key) const {
    if (auto str_val = get_string(key)) {
        try {
            return std::stoi(*str_val);
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

std::optional<bool> ConfigurationManager::get_bool(const std::string& key) const {
    if (auto str_val = get_string(key)) {
        std::string val = *str_val;
        std::transform(val.begin(), val.end(), val.begin(), ::tolower);
        return (val == "true" || val == "1" || val == "yes" || val == "on");
    }
    return std::nullopt;
}

std::optional<double> ConfigurationManager::get_double(const std::string& key) const {
    if (auto str_val = get_string(key)) {
        try {
            return std::stod(*str_val);
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

Environment ConfigurationManager::get_environment() const {
    auto env_str = get_string(config_keys::ENVIRONMENT);
    if (!env_str) return Environment::DEVELOPMENT;

    if (*env_str == "staging") return Environment::STAGING;
    if (*env_str == "production") return Environment::PRODUCTION;
    return Environment::DEVELOPMENT;
}

nlohmann::json ConfigurationManager::to_json() const {
    nlohmann::json json;
    for (const auto& [key, value] : config_values_) {
        json[key] = value;
    }
    return json;
}

} // namespace regulens
