#include "configuration_manager.hpp"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <filesystem>

namespace regulens {

ConfigurationManager& ConfigurationManager::get_instance() {
    static ConfigurationManager instance;
    return instance;
}

ConfigurationManager::ConfigurationManager() {
    // Set some defaults
    // Database host - must be configured via environment variables, no localhost default
    config_values_["database.port"] = "5432";
    config_values_["logging.level"] = "info";
    config_values_["api.endpoint"] = "https://api.regulens.ai";
    config_values_["email.smtp.server"] = "smtp.gmail.com";
    config_values_["email.smtp.port"] = "587";
}

void ConfigurationManager::set_logger(std::shared_ptr<Logger> logger) {
    logger_ = logger;
}

bool ConfigurationManager::initialize(int argc, char* argv[]) {
    // Configuration initialization with default settings
    return true;
}

ConfigurationManager::ValidationResult ConfigurationManager::validate_configuration() {
    ValidationResult result;
    result.valid = true;
    return result;
}

std::optional<std::string> ConfigurationManager::get_string(const std::string& key) const {
    auto it = config_values_.find(key);
    if (it != config_values_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<int> ConfigurationManager::get_int(const std::string& key) const {
    auto str_value = get_string(key);
    if (str_value) {
        try {
            return std::stoi(*str_value);
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

std::optional<bool> ConfigurationManager::get_bool(const std::string& key) const {
    auto str_value = get_string(key);
    if (str_value) {
        return *str_value == "true" || *str_value == "1";
    }
    return std::nullopt;
}

std::optional<double> ConfigurationManager::get_double(const std::string& key) const {
    auto str_value = get_string(key);
    if (str_value) {
        try {
            return std::stod(*str_value);
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

Environment ConfigurationManager::get_environment() const {
    return environment_;
}

nlohmann::json ConfigurationManager::to_json() const {
    nlohmann::json result;
    for (const auto& [key, value] : config_values_) {
        result[key] = value;
    }
    return result;
}

bool ConfigurationManager::reload() {
    return true;
}

} // namespace regulens

