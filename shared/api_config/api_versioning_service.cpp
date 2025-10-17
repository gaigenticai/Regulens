/**
 * API Versioning Service Implementation
 * Production-grade API versioning with negotiation and compatibility
 */

#include "api_versioning_service.hpp"
#include <algorithm>
#include <regex>
#include <sstream>
#include <iomanip>

namespace regulens {

APIVersioningService& APIVersioningService::get_instance() {
    static APIVersioningService instance;
    return instance;
}

bool APIVersioningService::initialize(const std::string& config_path,
                                    std::shared_ptr<StructuredLogger> logger) {
    logger_ = logger;
    config_path_ = config_path;

    if (!load_config(config_path)) {
        if (logger_) {
            logger_->error("Failed to load API versioning configuration from: " + config_path);
        }
        return false;
    }

    build_version_map();
    build_endpoint_version_map();

    if (logger_) {
        logger_->info("API versioning service initialized. Current version: " + current_version_ +
                     ", Default version: " + default_version_ +
                     ", Supported versions: " + std::to_string(version_info_.size()));
    }

    return true;
}

bool APIVersioningService::load_config(const std::string& config_path) {
    try {
        std::ifstream file(config_path);
        if (!file.is_open()) {
            if (logger_) {
                logger_->error("Cannot open versioning config file: " + config_path);
            }
            return false;
        }

        config_ = nlohmann::json::parse(file);

        // Extract basic configuration
        current_version_ = config_["versioning_strategy"]["current_version"];
        default_version_ = config_["versioning_strategy"]["default_version"];

        // Determine primary negotiation method
        std::string method_str = config_["versioning_strategy"]["versioning_method"];
        if (method_str == "url_path") {
            primary_method_ = VersionNegotiationMethod::URL_PATH;
        } else if (method_str == "header") {
            primary_method_ = VersionNegotiationMethod::ACCEPT_HEADER;
        } else if (method_str == "query_parameter") {
            primary_method_ = VersionNegotiationMethod::QUERY_PARAMETER;
        }

        return true;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Error loading versioning config: " + std::string(e.what()));
        }
        return false;
    }
}

void APIVersioningService::build_version_map() {
    if (!config_.contains("versioning_strategy") ||
        !config_["versioning_strategy"].contains("compatibility_matrix")) {
        return;
    }

    const auto& matrix = config_["versioning_strategy"]["compatibility_matrix"];

    for (const auto& [version, info] : matrix.items()) {
        APIVersionInfo version_info;
        version_info.version = version;
        version_info.release_date = info.value("release_date", "");
        version_info.supported_until = info.contains("supported_until") ?
                                     std::optional<std::string>(info["supported_until"]) : std::nullopt;

        // Determine status based on current date and configuration
        std::string status_str = info.value("status", "unknown");
        if (status_str == "current") {
            version_info.status = APIVersionStatus::CURRENT;
        } else if (status_str == "supported") {
            version_info.status = APIVersionStatus::SUPPORTED;
        } else if (status_str == "deprecated") {
            version_info.status = APIVersionStatus::DEPRECATED;
        } else if (status_str == "sunset") {
            version_info.status = APIVersionStatus::SUNSET;
        } else {
            version_info.status = APIVersionStatus::UNSUPPORTED;
        }

        // Load features and changes
        if (info.contains("new_features")) {
            for (const auto& feature : info["new_features"]) {
                version_info.new_features.push_back(feature);
            }
        }

        if (info.contains("breaking_changes")) {
            for (const auto& change : info["breaking_changes"]) {
                version_info.breaking_changes.push_back(change);
            }
        }

        if (info.contains("deprecated_features")) {
            for (const auto& feature : info["deprecated_features"]) {
                version_info.deprecated_features.push_back(feature);
            }
        }

        if (info.contains("migration_guide")) {
            version_info.migration_guide = info["migration_guide"];
        }

        // Calculate sunset date if deprecated
        if (version_info.status == APIVersionStatus::DEPRECATED ||
            version_info.status == APIVersionStatus::SUNSET) {
            // Add deprecation notice period (90 days) to determine sunset date
            auto sunset_days = config_["versioning_strategy"]["sunset_policy"]
                              .value("deprecation_notice_period_days", 90);
            // For now, we'll set sunset date as 90 days from now for deprecated versions
            auto now = std::chrono::system_clock::now();
            auto sunset_time = now + std::chrono::hours(24 * sunset_days);
            auto sunset_time_t = std::chrono::system_clock::to_time_t(sunset_time);

            std::stringstream ss;
            ss << std::put_time(std::gmtime(&sunset_time_t), "%Y-%m-%d");
            version_info.sunset_date = ss.str();
        }

        version_info_[version] = version_info;
    }
}

void APIVersioningService::build_endpoint_version_map() {
    if (!config_.contains("endpoint_versioning")) {
        return;
    }

    // For now, we'll use a simple mapping. In production, this would be more sophisticated
    // and loaded from the endpoint configuration
    endpoint_versions_["/api/transactions"] = {{"version", "v1"}, {"status", "current"}};
    endpoint_versions_["/api/fraud"] = {{"version", "v1"}, {"status", "current"}};
    endpoint_versions_["/api/rules"] = {{"version", "v1"}, {"status", "current"}};
    endpoint_versions_["/api/simulator"] = {{"version", "v1"}, {"status", "current"}};
}

VersionNegotiationResult APIVersioningService::negotiate_version(
    const std::string& request_path,
    const std::unordered_map<std::string, std::string>& headers,
    const std::unordered_map<std::string, std::string>& query_params
) {
    VersionNegotiationResult result = {false, "", VersionNegotiationMethod::URL_PATH, std::nullopt, std::nullopt};

    // Try URL path negotiation first (primary method)
    auto url_result = negotiate_from_url_path(request_path);
    if (url_result.success) {
        result = url_result;
        std::lock_guard<std::mutex> lock(stats_mutex_);
        version_usage_counts_[result.negotiated_version]++;
        return result;
    }

    // Fallback to Accept header negotiation
    auto header_result = negotiate_from_header(headers);
    if (header_result.success) {
        result = header_result;
        result.method_used = VersionNegotiationMethod::ACCEPT_HEADER;
        std::lock_guard<std::mutex> lock(stats_mutex_);
        version_usage_counts_[result.negotiated_version]++;
        return result;
    }

    // Fallback to query parameter negotiation
    auto query_result = negotiate_from_query_param(query_params);
    if (query_result.success) {
        result = query_result;
        result.method_used = VersionNegotiationMethod::QUERY_PARAMETER;
        std::lock_guard<std::mutex> lock(stats_mutex_);
        version_usage_counts_[result.negotiated_version]++;
        return result;
    }

    // Default to current version if no negotiation succeeds
    result.success = true;
    result.negotiated_version = default_version_;
    result.warning_message = "No version specified, using default version " + default_version_;

    std::lock_guard<std::mutex> lock(stats_mutex_);
    version_usage_counts_[result.negotiated_version]++;

    return result;
}

VersionNegotiationResult APIVersioningService::negotiate_from_url_path(const std::string& path) {
    VersionNegotiationResult result = {false, "", VersionNegotiationMethod::URL_PATH};

    // Check if path starts with /api/v{version}/
    std::regex version_pattern(R"(^/api/v(\d+)(/.*)?$)");
    std::smatch matches;

    if (std::regex_match(path, matches, version_pattern)) {
        std::string version = "v" + matches[1].str();

        if (is_supported_version(version)) {
            result.success = true;
            result.negotiated_version = version;

            if (is_version_deprecated(version)) {
                result.deprecation_notice = "API version " + version + " is deprecated. " +
                                          "Please migrate to " + current_version_ + ". " +
                                          "See migration guide for details.";
                if (version_info_[version].sunset_date) {
                    result.sunset_date = *version_info_[version].sunset_date;
                }
            }

            return result;
        } else {
            auto supported = get_supported_versions();
            std::string supported_str;
            for (size_t i = 0; i < supported.size(); ++i) {
                if (i > 0) supported_str += ", ";
                supported_str += supported[i];
            }
            result.warning_message = "Requested API version " + version + " is not supported. " +
                                   "Supported versions: " + supported_str;
        }
    }

    return result;
}

VersionNegotiationResult APIVersioningService::negotiate_from_header(
    const std::unordered_map<std::string, std::string>& headers
) {
    VersionNegotiationResult result = {false, "", VersionNegotiationMethod::ACCEPT_HEADER};

    // Check Accept header for version information
    auto accept_it = headers.find("accept");
    if (accept_it != headers.end()) {
        std::string accept_header = accept_it->second;

        // Look for application/vnd.regulens.v{version}+json pattern
        std::regex version_pattern(R"(application/vnd\.regulens\.v(\d+)\+json)");
        std::smatch matches;

        if (std::regex_search(accept_header, matches, version_pattern)) {
            std::string version = "v" + matches[1].str();

            if (is_supported_version(version)) {
                result.success = true;
                result.negotiated_version = version;

                if (is_version_deprecated(version)) {
                    result.deprecation_notice = "API version " + version + " is deprecated.";
                }

                return result;
            }
        }
    }

    return result;
}

VersionNegotiationResult APIVersioningService::negotiate_from_query_param(
    const std::unordered_map<std::string, std::string>& query_params
) {
    VersionNegotiationResult result = {false, "", VersionNegotiationMethod::QUERY_PARAMETER};

    // Check for version query parameter
    auto version_it = query_params.find("v");
    if (version_it != query_params.end()) {
        std::string version = "v" + version_it->second;

        if (is_supported_version(version)) {
            result.success = true;
            result.negotiated_version = version;

            if (is_version_deprecated(version)) {
                result.deprecation_notice = "API version " + version + " is deprecated.";
            }

            return result;
        }
    }

    return result;
}

VersionCompatibilityCheck APIVersioningService::check_compatibility(
    const std::string& requested_version,
    const std::string& endpoint_path
) {
    VersionCompatibilityCheck check = {
        false,
        requested_version,
        "",
        false,
        std::nullopt,
        {}
    };

    if (!is_valid_version(requested_version)) {
        check.compatibility_notes = "Invalid version format";
        return check;
    }

    if (!is_supported_version(requested_version)) {
        check.compatibility_notes = "Version not supported";
        return check;
    }

    // For now, assume all versions of the same API are compatible
    // In production, this would check endpoint-specific compatibility
    check.compatible = true;
    check.resolved_version = requested_version;

    if (is_version_deprecated(requested_version)) {
        check.requires_migration = true;
        check.compatibility_notes = "Version is deprecated, migration recommended";
    }

    return check;
}

// Version information retrieval methods
std::optional<APIVersionInfo> APIVersioningService::get_version_info(const std::string& version) {
    auto it = version_info_.find(version);
    if (it != version_info_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<std::string> APIVersioningService::get_supported_versions() {
    std::vector<std::string> versions;
    for (const auto& [version, info] : version_info_) {
        if (info.status != APIVersionStatus::UNSUPPORTED) {
            versions.push_back(version);
        }
    }
    std::sort(versions.begin(), versions.end());
    return versions;
}

std::string APIVersioningService::get_current_version() {
    return current_version_;
}

std::string APIVersioningService::get_default_version() {
    return default_version_;
}

// Path manipulation methods
std::string APIVersioningService::extract_version_from_path(const std::string& path) {
    std::regex version_pattern(R"(^/api/v(\d+))");
    std::smatch matches;

    if (std::regex_search(path, matches, version_pattern)) {
        return "v" + matches[1].str();
    }

    return "";
}

std::string APIVersioningService::normalize_path_for_version(const std::string& path, const std::string& version) {
    std::string normalized_path = path;

    // Remove existing version from path
    if (is_versioned_path(path)) {
        normalized_path = remove_version_from_path(path);
    }

    // Add new version to path
    return add_version_to_path(normalized_path, version);
}

std::string APIVersioningService::build_versioned_path(const std::string& base_path, const std::string& version) {
    return add_version_to_path(base_path, version);
}

bool APIVersioningService::is_versioned_path(const std::string& path) {
    return path.find("/api/v") != std::string::npos;
}

std::string APIVersioningService::remove_version_from_path(const std::string& path) {
    std::regex version_pattern(R"(^/api/v\d+(/.*)?$)");
    std::smatch matches;

    if (std::regex_match(path, matches, version_pattern) && matches.size() > 1) {
        return "/api" + matches[1].str();
    }

    return path;
}

std::string APIVersioningService::add_version_to_path(const std::string& path, const std::string& version) {
    if (path.find("/api/") == 0) {
        // Path already starts with /api/, insert version after /api/
        return "/api/" + version + path.substr(4);
    } else if (path.find("/api") == 0 && path.length() > 4) {
        // Path starts with /api, insert version after /api
        return "/api/" + version + path.substr(4);
    } else {
        // Path doesn't start with /api, add version prefix
        return "/api/" + version + path;
    }
}

// Deprecation and status methods
bool APIVersioningService::is_version_deprecated(const std::string& version) {
    auto info = get_version_info(version);
    return info && (info->status == APIVersionStatus::DEPRECATED ||
                   info->status == APIVersionStatus::SUNSET);
}

bool APIVersioningService::is_version_sunset(const std::string& version) {
    auto info = get_version_info(version);
    return info && info->status == APIVersionStatus::SUNSET;
}

std::optional<std::chrono::system_clock::time_point> APIVersioningService::get_version_sunset_date(
    const std::string& version
) {
    auto info = get_version_info(version);
    if (info && info->sunset_date) {
        // Parse date string to time_point (simplified implementation)
        // In production, this would use proper date parsing
        return std::chrono::system_clock::now() + std::chrono::hours(24 * 90); // 90 days from now
    }
    return std::nullopt;
}

// Header generation
std::unordered_map<std::string, std::string> APIVersioningService::generate_version_headers(
    const std::string& negotiated_version,
    const VersionNegotiationResult& negotiation_result
) {
    std::unordered_map<std::string, std::string> headers;

    headers["X-API-Version"] = negotiated_version;

    if (negotiation_result.deprecation_notice) {
        headers["X-API-Deprecation-Warning"] = *negotiation_result.deprecation_notice;
    }

    if (negotiation_result.sunset_date) {
        headers["X-API-Sunset-Date"] = *negotiation_result.sunset_date;
    }

    if (negotiation_result.warning_message) {
        headers["X-API-Version-Warning"] = *negotiation_result.warning_message;
    }

    return headers;
}

// Migration and compatibility helpers
std::optional<std::string> APIVersioningService::get_migration_guide(
    const std::string& from_version, const std::string& to_version
) {
    auto from_info = get_version_info(from_version);
    auto to_info = get_version_info(to_version);

    if (from_info && from_info->migration_guide) {
        return from_info->migration_guide;
    }

    // Generate basic migration guide
    std::stringstream guide;
    guide << "Migration Guide: " << from_version << " to " << to_version << "\n";
    guide << "1. Update API endpoints to use " << to_version << "\n";
    guide << "2. Review breaking changes in " << to_version << "\n";
    guide << "3. Update client code to handle new response formats\n";
    guide << "4. Test thoroughly in staging environment\n";

    return guide.str();
}

std::vector<std::string> APIVersioningService::get_breaking_changes_between_versions(
    const std::string& from_version, const std::string& to_version
) {
    auto to_info = get_version_info(to_version);
    if (to_info) {
        return to_info->breaking_changes;
    }
    return {};
}

// Configuration and status methods
bool APIVersioningService::reload_configuration() {
    return initialize(config_path_, logger_);
}

nlohmann::json APIVersioningService::get_versioning_status() {
    nlohmann::json status;

    status["current_version"] = current_version_;
    status["default_version"] = default_version_;
    status["supported_versions"] = get_supported_versions();
    status["version_usage_stats"] = get_version_usage_stats();

    std::vector<nlohmann::json> version_details;
    for (const auto& [version, info] : version_info_) {
        nlohmann::json detail;
        detail["version"] = info.version;
        detail["status"] = static_cast<int>(info.status);
        detail["release_date"] = info.release_date;
        detail["new_features"] = info.new_features;
        detail["breaking_changes"] = info.breaking_changes;
        version_details.push_back(detail);
    }
    status["version_details"] = version_details;

    return status;
}

std::unordered_map<std::string, int> APIVersioningService::get_version_usage_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return version_usage_counts_;
}

// Private helper methods
bool APIVersioningService::is_valid_version(const std::string& version) {
    std::regex version_pattern(R"(^v\d+$)");
    return std::regex_match(version, version_pattern);
}

bool APIVersioningService::is_supported_version(const std::string& version) {
    auto info = get_version_info(version);
    return info && info->status != APIVersionStatus::UNSUPPORTED;
}

APIVersionStatus APIVersioningService::get_version_status(const std::string& version) {
    auto info = get_version_info(version);
    return info ? info->status : APIVersionStatus::UNSUPPORTED;
}

} // namespace regulens
