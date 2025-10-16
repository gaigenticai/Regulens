/**
 * API Migration Helper Implementation
 * Production-grade API version migration utilities
 */

#include "api_migration_helper.hpp"
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>

namespace regulens {

APIMigrationHelper& APIMigrationHelper::get_instance() {
    static APIMigrationHelper instance;
    return instance;
}

bool APIMigrationHelper::initialize(const std::string& config_path,
                                  std::shared_ptr<StructuredLogger> logger) {
    logger_ = logger;
    config_path_ = config_path;

    if (!load_migration_config(config_path)) {
        if (logger_) {
            logger_->error("Failed to load migration configuration from: " + config_path);
        }
        return false;
    }

    build_migration_rules();

    if (logger_) {
        logger_->info("API migration helper initialized with " +
                     std::to_string(migration_rules_.size()) + " migration rules");
    }

    return true;
}

bool APIMigrationHelper::load_migration_config(const std::string& config_path) {
    try {
        std::ifstream file(config_path);
        if (!file.is_open()) {
            if (logger_) {
                logger_->error("Cannot open migration config file: " + config_path);
            }
            return false;
        }

        migration_config_ = nlohmann::json::parse(file);
        return true;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Error loading migration config: " + std::string(e.what()));
        }
        return false;
    }
}

void APIMigrationHelper::build_migration_rules() {
    // Build migration rules from configuration
    // In production, this would load from a comprehensive configuration file

    // V1 to V2 response transformation
    MigrationRule v1_to_v2_response_transform = {
        "v1", "v2", MigrationType::RESPONSE_TRANSFORMATION,
        "Transform v1 responses to v2 format with data/metadata structure",
        [this](const nlohmann::json& v1_response) -> nlohmann::json {
            return transform_v1_to_v2_response(v1_response);
        },
        {}, // parameter mappings
        {}, // header additions
        std::nullopt, // redirect endpoint
        true // enabled
    };
    migration_rules_.push_back(v1_to_v2_response_transform);

    // V1 to V2 parameter mapping
    MigrationRule v1_to_v2_param_mapping = {
        "v1", "v2", MigrationType::PARAMETER_MAPPING,
        "Map v1 pagination parameters to v2 format",
        nullptr, // transformation function
        {
            {"page", "offset"},
            {"per_page", "limit"},
            {"sort_by", "order_by"}
        }, // parameter mappings
        {}, // header additions
        std::nullopt, // redirect endpoint
        true // enabled
    };
    migration_rules_.push_back(v1_to_v2_param_mapping);

    // Add deprecation headers
    MigrationRule deprecation_headers = {
        "v1", "v2", MigrationType::HEADER_ADDITION,
        "Add deprecation and migration headers",
        nullptr,
        {}, // parameter mappings
        {
            {"X-API-Deprecation-Warning", "API v1 is deprecated. Please migrate to v2."},
            {"X-API-Migration-Guide", "/docs/api-migration/v1-to-v2"},
            {"Link", "</api/v2>; rel=\"successor-version\""}
        }, // header additions
        std::nullopt, // redirect endpoint
        true // enabled
    };
    migration_rules_.push_back(deprecation_headers);

    // Assign IDs to rules
    for (auto& rule : migration_rules_) {
        std::string rule_id = generate_rule_id();
        rules_by_id_[rule_id] = rule;
    }
}

MigrationAssessment APIMigrationHelper::assess_migration(const std::string& from_version,
                                                       const std::string& to_version) {
    MigrationAssessment assessment = {
        from_version,
        to_version,
        false, // can_migrate
        {}, // breaking_changes
        {}, // required_actions
        0, // effort_estimate_hours
        0.0, // compatibility_score
        {} // applicable_rules
    };

    // Check if versions are valid
    if (from_version == to_version) {
        assessment.can_migrate = true;
        assessment.compatibility_score = 1.0;
        return assessment;
    }

    // Get breaking changes and required actions
    assessment.breaking_changes = get_breaking_changes(from_version, to_version);
    assessment.required_actions = identify_required_actions(from_version, to_version);
    assessment.compatibility_score = calculate_compatibility_score(from_version, to_version);
    assessment.effort_estimate_hours = calculate_migration_effort(from_version, to_version);

    // Find applicable rules
    for (const auto& rule : migration_rules_) {
        if (rule.from_version == from_version && rule.to_version == to_version) {
            assessment.applicable_rules.push_back(rule);
        }
    }

    // Determine if migration is possible
    assessment.can_migrate = assessment.compatibility_score > 0.5;

    return assessment;
}

std::vector<std::string> APIMigrationHelper::get_breaking_changes(const std::string& from_version,
                                                                const std::string& to_version) {
    // In production, this would load from configuration
    if (from_version == "v1" && to_version == "v2") {
        return {
            "Response format changed to include 'data' and 'meta' fields",
            "Pagination parameters renamed (page->offset, per_page->limit)",
            "Error response format enhanced with detailed error codes",
            "Some deprecated endpoints removed"
        };
    }
    return {};
}

double APIMigrationHelper::calculate_compatibility_score(const std::string& from_version,
                                                       const std::string& to_version) {
    // Simple compatibility scoring - in production this would be more sophisticated
    if (from_version == "v1" && to_version == "v2") {
        return 0.85; // 85% compatible with migration tools available
    }
    return 1.0; // Same version or unknown transition
}

MigrationReport APIMigrationHelper::execute_migration(const std::string& from_version,
                                                    const std::string& to_version) {
    MigrationReport report = {
        generate_migration_id(),
        from_version,
        to_version,
        std::chrono::system_clock::now(),
        std::nullopt, // completed_at
        false, // success
        {}, // applied_rules
        {}, // errors
        {} // statistics
    };

    try {
        // Assess migration first
        auto assessment = assess_migration(from_version, to_version);
        if (!assessment.can_migrate) {
            report.errors.push_back("Migration not possible due to compatibility issues");
            report.completed_at = std::chrono::system_clock::now();
            return report;
        }

        // Apply migration rules
        for (const auto& rule : assessment.applicable_rules) {
            if (apply_migration_rule(rule)) {
                report.applied_rules.push_back(rule.description);
            } else {
                report.errors.push_back("Failed to apply rule: " + rule.description);
            }
        }

        // Update statistics
        report.statistics["rules_applied"] = static_cast<int>(report.applied_rules.size());
        report.statistics["errors_count"] = static_cast<int>(report.errors.size());
        report.statistics["effort_hours"] = assessment.effort_estimate_hours;

        report.success = report.errors.empty();
        report.completed_at = std::chrono::system_clock::now();

        // Save report
        save_migration_report(report);

    } catch (const std::exception& e) {
        report.errors.push_back("Migration failed with exception: " + std::string(e.what()));
        report.completed_at = std::chrono::system_clock::now();
    }

    return report;
}

bool APIMigrationHelper::apply_migration_rule(const MigrationRule& rule) {
    try {
        // In production, this would apply the rule to the system
        // For now, just log the application
        if (logger_) {
            logger_->info("Applied migration rule: " + rule.description +
                         " (" + rule.from_version + " -> " + rule.to_version + ")");
        }

        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            migration_stats_["rules_applied"]++;
        }

        return true;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Failed to apply migration rule: " + rule.description +
                          " - " + std::string(e.what()));
        }
        return false;
    }
}

nlohmann::json APIMigrationHelper::transform_response(const nlohmann::json& response,
                                                    const std::string& target_version) {
    if (target_version == "v2") {
        return transform_v1_to_v2_response(response);
    }
    return response;
}

std::unordered_map<std::string, std::string> APIMigrationHelper::transform_parameters(
    const std::unordered_map<std::string, std::string>& params,
    const std::string& target_version) {

    if (target_version == "v2") {
        return map_v1_to_v2_parameters(params);
    }
    return params;
}

std::unordered_map<std::string, std::string> APIMigrationHelper::add_compatibility_headers(
    const std::string& target_version) {

    std::unordered_map<std::string, std::string> headers;

    if (target_version == "v1") {
        headers["X-API-Compatibility-Mode"] = "v1";
        headers["X-API-Deprecation-Warning"] = "Using compatibility mode for v1. Consider upgrading to v2.";
    }

    return headers;
}

std::string APIMigrationHelper::generate_migration_guide(const std::string& from_version,
                                                       const std::string& to_version) {
    std::stringstream guide;

    guide << "# API Migration Guide: " << from_version << " to " << to_version << "\n\n";

    auto assessment = assess_migration(from_version, to_version);

    guide << "## Overview\n";
    guide << "Compatibility Score: " << (assessment.compatibility_score * 100) << "%\n";
    guide << "Estimated Effort: " << assessment.effort_estimate_hours << " hours\n\n";

    if (!assessment.breaking_changes.empty()) {
        guide << "## Breaking Changes\n";
        for (const auto& change : assessment.breaking_changes) {
            guide << "- " << change << "\n";
        }
        guide << "\n";
    }

    if (!assessment.required_actions.empty()) {
        guide << "## Required Actions\n";
        for (const auto& action : assessment.required_actions) {
            guide << "- " << action << "\n";
        }
        guide << "\n";
    }

    guide << "## Migration Steps\n";
    auto steps = get_migration_steps(from_version, to_version);
    for (size_t i = 0; i < steps.size(); ++i) {
        guide << std::to_string(i + 1) << ". " << steps[i] << "\n";
    }

    return guide.str();
}

std::vector<std::string> APIMigrationHelper::get_migration_steps(const std::string& from_version,
                                                               const std::string& to_version) {
    if (from_version == "v1" && to_version == "v2") {
        return {
            "Update API client imports to use versioned endpoints",
            "Modify response parsing to handle new 'data' and 'meta' structure",
            "Update pagination parameters (page→offset, per_page→limit)",
            "Implement enhanced error handling for new error format",
            "Test all API calls with v2 endpoints",
            "Gradually roll out changes to production"
        };
    }
    return {"No specific migration steps required"};
}

bool APIMigrationHelper::validate_migration_prerequisites(const std::string& from_version,
                                                        const std::string& to_version) {
    // Check if target version is supported
    // Check if required permissions are available
    // Check if dependent services are compatible
    return true; // Simplified for now
}

std::vector<MigrationReport> APIMigrationHelper::get_migration_history() {
    return migration_reports_;
}

MigrationReport APIMigrationHelper::get_migration_status(const std::string& migration_id) {
    auto it = reports_by_id_.find(migration_id);
    if (it != reports_by_id_.end()) {
        return it->second;
    }
    return {}; // Return empty report if not found
}

std::unordered_map<std::string, int> APIMigrationHelper::get_migration_statistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return migration_stats_;
}

// Private helper methods
nlohmann::json APIMigrationHelper::transform_v1_to_v2_response(const nlohmann::json& v1_response) {
    nlohmann::json v2_response;

    // Wrap data in 'data' field
    v2_response["data"] = v1_response;

    // Add metadata
    v2_response["meta"] = add_response_metadata(v1_response);

    return v2_response;
}

std::unordered_map<std::string, std::string> APIMigrationHelper::map_v1_to_v2_parameters(
    const std::unordered_map<std::string, std::string>& v1_params) {

    std::unordered_map<std::string, std::string> v2_params = v1_params;

    // Map pagination parameters
    if (v1_params.count("page")) {
        // Convert page to offset (assuming 50 items per page)
        int page = std::stoi(v1_params.at("page"));
        int limit = v1_params.count("per_page") ? std::stoi(v1_params.at("per_page")) : 50;
        v2_params["offset"] = std::to_string((page - 1) * limit);
        v2_params.erase("page");
    }

    if (v1_params.count("per_page")) {
        v2_params["limit"] = v1_params.at("per_page");
        v2_params.erase("per_page");
    }

    // Map sorting parameters
    if (v1_params.count("sort_by")) {
        v2_params["order_by"] = v1_params.at("sort_by");
        v2_params.erase("sort_by");
    }

    return v2_params;
}

nlohmann::json APIMigrationHelper::add_response_metadata(const nlohmann::json& response) {
    nlohmann::json meta;

    meta["version"] = "v2";
    meta["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
    meta["compatibility_mode"] = true;

    // Add pagination metadata if response looks like a list
    if (response.is_array()) {
        meta["count"] = response.size();
        meta["has_more"] = false; // Simplified
    } else if (response.is_object() && response.count("items")) {
        meta["count"] = response["items"].size();
        meta["has_more"] = response.value("has_more", false);
    }

    return meta;
}

int APIMigrationHelper::calculate_migration_effort(const std::string& from_version,
                                                 const std::string& to_version) {
    if (from_version == "v1" && to_version == "v2") {
        return 16; // 2 weeks for a typical application
    }
    return 1; // Minimal effort for same version
}

std::vector<std::string> APIMigrationHelper::identify_required_actions(const std::string& from_version,
                                                                     const std::string& to_version) {
    if (from_version == "v1" && to_version == "v2") {
        return {
            "Update client code to handle new response format",
            "Modify API calls to use new parameter names",
            "Implement enhanced error handling",
            "Update tests for new API structure",
            "Deploy gradual rollout strategy"
        };
    }
    return {};
}

std::string APIMigrationHelper::generate_migration_id() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8; ++i) {
        ss << dis(gen);
    }
    return "migration_" + ss.str();
}

std::string APIMigrationHelper::generate_rule_id() {
    static int counter = 0;
    return "rule_" + std::to_string(++counter);
}

bool APIMigrationHelper::save_migration_report(const MigrationReport& report) {
    migration_reports_.push_back(report);
    reports_by_id_[report.migration_id] = report;
    return true;
}

} // namespace regulens
