/**
 * API Versioning Service
 * Production-grade API versioning with negotiation, routing, and compatibility
 */

#ifndef API_VERSIONING_SERVICE_HPP
#define API_VERSIONING_SERVICE_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <chrono>
#include <memory>
#include <nlohmann/json.hpp>
#include "../../shared/error_handler.hpp"
#include "../../shared/structured_logger.hpp"

namespace regulens {

enum class APIVersionStatus {
    CURRENT,      // Latest stable version
    SUPPORTED,    // Still supported but not latest
    DEPRECATED,   // Deprecated but still functional
    SUNSET,       // Will be removed soon
    UNSUPPORTED   // No longer supported
};

enum class VersionNegotiationMethod {
    URL_PATH,           // /api/v1/resource
    ACCEPT_HEADER,      // Accept: application/vnd.regulens.v1+json
    QUERY_PARAMETER,    // /api/resource?v=1
    CUSTOM_HEADER       // X-API-Version: v1
};

struct APIVersionInfo {
    std::string version;
    APIVersionStatus status;
    std::string release_date;
    std::optional<std::string> supported_until;
    std::optional<std::string> sunset_date;
    std::vector<std::string> new_features;
    std::vector<std::string> breaking_changes;
    std::vector<std::string> deprecated_features;
    std::optional<std::string> migration_guide;
};

struct VersionNegotiationResult {
    bool success;
    std::string negotiated_version;
    VersionNegotiationMethod method_used;
    std::optional<std::string> warning_message;
    std::optional<std::string> deprecation_notice;
    std::optional<std::string> sunset_date;
};

struct VersionCompatibilityCheck {
    bool compatible;
    std::string requested_version;
    std::string resolved_version;
    bool requires_migration;
    std::optional<std::string> compatibility_notes;
    std::vector<std::string> breaking_changes;
};

class APIVersioningService {
public:
    static APIVersioningService& get_instance();

    // Initialize with configuration
    bool initialize(const std::string& config_path,
                   std::shared_ptr<StructuredLogger> logger);

    // Version negotiation methods
    VersionNegotiationResult negotiate_version(
        const std::string& request_path,
        const std::unordered_map<std::string, std::string>& headers,
        const std::unordered_map<std::string, std::string>& query_params
    );

    // Version compatibility checking
    VersionCompatibilityCheck check_compatibility(
        const std::string& requested_version,
        const std::string& endpoint_path
    );

    // Version information retrieval
    std::optional<APIVersionInfo> get_version_info(const std::string& version);
    std::vector<std::string> get_supported_versions();
    std::string get_current_version();
    std::string get_default_version();

    // Version routing helpers
    std::string extract_version_from_path(const std::string& path);
    std::string normalize_path_for_version(const std::string& path, const std::string& version);
    std::string build_versioned_path(const std::string& base_path, const std::string& version);

    // Deprecation and sunset handling
    bool is_version_deprecated(const std::string& version);
    bool is_version_sunset(const std::string& version);
    std::optional<std::chrono::system_clock::time_point> get_version_sunset_date(const std::string& version);

    // Header generation for responses
    std::unordered_map<std::string, std::string> generate_version_headers(
        const std::string& negotiated_version,
        const VersionNegotiationResult& negotiation_result
    );

    // Migration and compatibility helpers
    std::optional<std::string> get_migration_guide(const std::string& from_version, const std::string& to_version);
    std::vector<std::string> get_breaking_changes_between_versions(const std::string& from_version, const std::string& to_version);

    // Configuration and status
    bool reload_configuration();
    nlohmann::json get_versioning_status();
    std::unordered_map<std::string, int> get_version_usage_stats();

private:
    APIVersioningService() = default;
    ~APIVersioningService() = default;
    APIVersioningService(const APIVersioningService&) = delete;
    APIVersioningService& operator=(const APIVersioningService&) = delete;

    // Configuration loading
    bool load_config(const std::string& config_path);
    void build_version_map();
    void build_endpoint_version_map();

    // Version negotiation helpers
    VersionNegotiationResult negotiate_from_url_path(const std::string& path);
    VersionNegotiationResult negotiate_from_header(const std::unordered_map<std::string, std::string>& headers);
    VersionNegotiationResult negotiate_from_query_param(const std::unordered_map<std::string, std::string>& query_params);

    // Validation methods
    bool is_valid_version(const std::string& version);
    bool is_supported_version(const std::string& version);
    APIVersionStatus get_version_status(const std::string& version);

    // Path manipulation
    std::string remove_version_from_path(const std::string& path);
    std::string add_version_to_path(const std::string& path, const std::string& version);
    bool is_versioned_path(const std::string& path);

    // Configuration data
    std::shared_ptr<StructuredLogger> logger_;
    nlohmann::json config_;
    std::string config_path_;

    // Cached data structures
    std::unordered_map<std::string, APIVersionInfo> version_info_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> endpoint_versions_;
    std::string current_version_;
    std::string default_version_;
    VersionNegotiationMethod primary_method_;

    // Runtime statistics
    std::unordered_map<std::string, int> version_usage_counts_;
    std::mutex stats_mutex_;
};

} // namespace regulens

#endif // API_VERSIONING_SERVICE_HPP
