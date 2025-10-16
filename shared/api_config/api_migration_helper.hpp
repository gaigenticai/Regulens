/**
 * API Migration Helper
 * Production-grade utilities for API version migration and compatibility
 */

#ifndef API_MIGRATION_HELPER_HPP
#define API_MIGRATION_HELPER_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <functional>
#include <nlohmann/json.hpp>
#include "../../shared/error_handler.hpp"
#include "../../shared/structured_logger.hpp"

namespace regulens {

enum class MigrationType {
    PARAMETER_MAPPING,
    RESPONSE_TRANSFORMATION,
    HEADER_ADDITION,
    ENDPOINT_REDIRECT,
    FEATURE_FLAG
};

struct MigrationRule {
    std::string from_version;
    std::string to_version;
    MigrationType type;
    std::string description;
    std::function<nlohmann::json(const nlohmann::json&)> transformation;
    std::unordered_map<std::string, std::string> parameter_mappings;
    std::unordered_map<std::string, std::string> header_additions;
    std::optional<std::string> redirect_endpoint;
    bool enabled = true;
};

struct MigrationAssessment {
    std::string current_version;
    std::string target_version;
    bool can_migrate;
    std::vector<std::string> breaking_changes;
    std::vector<std::string> required_actions;
    int effort_estimate_hours;
    double compatibility_score; // 0.0 to 1.0
    std::vector<MigrationRule> applicable_rules;
};

struct MigrationReport {
    std::string migration_id;
    std::string from_version;
    std::string to_version;
    std::chrono::system_clock::time_point started_at;
    std::optional<std::chrono::system_clock::time_point> completed_at;
    bool success;
    std::vector<std::string> applied_rules;
    std::vector<std::string> errors;
    std::unordered_map<std::string, int> statistics;
};

class APIMigrationHelper {
public:
    static APIMigrationHelper& get_instance();

    // Initialize with migration configuration
    bool initialize(const std::string& config_path,
                   std::shared_ptr<StructuredLogger> logger);

    // Migration assessment
    MigrationAssessment assess_migration(const std::string& from_version,
                                       const std::string& to_version);
    std::vector<std::string> get_breaking_changes(const std::string& from_version,
                                                const std::string& to_version);
    double calculate_compatibility_score(const std::string& from_version,
                                       const std::string& to_version);

    // Migration execution
    MigrationReport execute_migration(const std::string& from_version,
                                    const std::string& to_version);
    bool apply_migration_rule(const MigrationRule& rule);
    bool rollback_migration(const std::string& migration_id);

    // Compatibility transformations
    nlohmann::json transform_response(const nlohmann::json& response,
                                    const std::string& target_version);
    std::unordered_map<std::string, std::string> transform_parameters(
        const std::unordered_map<std::string, std::string>& params,
        const std::string& target_version);
    std::unordered_map<std::string, std::string> add_compatibility_headers(
        const std::string& target_version);

    // Migration utilities
    std::string generate_migration_guide(const std::string& from_version,
                                       const std::string& to_version);
    std::vector<std::string> get_migration_steps(const std::string& from_version,
                                               const std::string& to_version);
    bool validate_migration_prerequisites(const std::string& from_version,
                                        const std::string& to_version);

    // Migration monitoring
    std::vector<MigrationReport> get_migration_history();
    MigrationReport get_migration_status(const std::string& migration_id);
    std::unordered_map<std::string, int> get_migration_statistics();

    // Configuration management
    bool add_migration_rule(const MigrationRule& rule);
    bool update_migration_rule(const std::string& rule_id, const MigrationRule& rule);
    bool remove_migration_rule(const std::string& rule_id);
    std::vector<MigrationRule> get_migration_rules(const std::string& from_version = "",
                                                  const std::string& to_version = "");

private:
    APIMigrationHelper() = default;
    ~APIMigrationHelper() = default;
    APIMigrationHelper(const APIMigrationHelper&) = delete;
    APIMigrationHelper& operator=(const APIMigrationHelper&) = delete;

    // Configuration loading
    bool load_migration_config(const std::string& config_path);
    void build_migration_rules();

    // Migration rule management
    std::string generate_rule_id();
    bool validate_migration_rule(const MigrationRule& rule);

    // Transformation helpers
    nlohmann::json transform_v1_to_v2_response(const nlohmann::json& v1_response);
    std::unordered_map<std::string, std::string> map_v1_to_v2_parameters(
        const std::unordered_map<std::string, std::string>& v1_params);
    nlohmann::json add_response_metadata(const nlohmann::json& response);

    // Assessment helpers
    int calculate_migration_effort(const std::string& from_version,
                                 const std::string& to_version);
    std::vector<std::string> identify_required_actions(const std::string& from_version,
                                                     const std::string& to_version);

    // Utility methods
    std::string generate_migration_id();
    std::string get_current_timestamp();
    bool save_migration_report(const MigrationReport& report);

    // Configuration data
    std::shared_ptr<StructuredLogger> logger_;
    nlohmann::json migration_config_;
    std::string config_path_;

    // Migration rules storage
    std::vector<MigrationRule> migration_rules_;
    std::unordered_map<std::string, MigrationRule> rules_by_id_;

    // Migration reports storage
    std::vector<MigrationReport> migration_reports_;
    std::unordered_map<std::string, MigrationReport> reports_by_id_;

    // Statistics
    std::unordered_map<std::string, int> migration_stats_;
    std::mutex stats_mutex_;
};

} // namespace regulens

#endif // API_MIGRATION_HELPER_HPP
