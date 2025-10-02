#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <nlohmann/json.hpp>

#include "../shared/config/configuration_manager.hpp"
#include "../shared/logging/structured_logger.hpp"
#include "../shared/models/regulatory_change.hpp"

namespace regulens {

/**
 * @brief Abstract base class for regulatory data sources
 */
class RegulatorySource {
public:
    RegulatorySource(std::string source_id, std::string name,
                    RegulatorySourceType type,
                    std::shared_ptr<ConfigurationManager> config,
                    std::shared_ptr<StructuredLogger> logger)
        : source_id_(std::move(source_id)), name_(std::move(name)), type_(type),
          config_(config), logger_(logger), last_check_time_(std::chrono::system_clock::now()),
          is_active_(true), consecutive_failures_(0) {}

    virtual ~RegulatorySource() = default;

    /**
     * @brief Initialize the source
     * @return true if initialization successful
     */
    virtual bool initialize() = 0;

    /**
     * @brief Check for new regulatory changes
     * @return List of detected changes
     */
    virtual std::vector<RegulatoryChange> check_for_changes() = 0;

    /**
     * @brief Get source configuration
     * @return JSON configuration
     */
    virtual nlohmann::json get_configuration() const = 0;

    /**
     * @brief Test connectivity to the source
     * @return true if source is accessible
     */
    virtual bool test_connectivity() = 0;

    // Common getters
    const std::string& get_source_id() const { return source_id_; }
    const std::string& get_name() const { return name_; }
    RegulatorySourceType get_type() const { return type_; }
    bool is_active() const { return is_active_; }
    std::chrono::system_clock::time_point get_last_check_time() const { return last_check_time_; }
    size_t get_consecutive_failures() const { return consecutive_failures_; }

    // Common setters
    void set_active(bool active) { is_active_ = active; }
    void update_last_check_time() { last_check_time_ = std::chrono::system_clock::now(); }

    // Error handling
    void record_failure() { consecutive_failures_++; }
    void record_success() { consecutive_failures_ = 0; }

    // Utility methods
    bool should_check() const {
        if (!is_active_) return false;

        auto now = std::chrono::system_clock::now();
        auto time_since_last_check = now - last_check_time_.load();
        return time_since_last_check >= get_check_interval();
    }

    virtual std::chrono::seconds get_check_interval() const {
        return std::chrono::seconds(300); // Default 5 minutes
    }

protected:
    std::string source_id_;
    std::string name_;
    RegulatorySourceType type_;
    std::shared_ptr<ConfigurationManager> config_;
    std::shared_ptr<StructuredLogger> logger_;

    std::atomic<std::chrono::system_clock::time_point> last_check_time_;
    std::atomic<bool> is_active_;
    std::atomic<size_t> consecutive_failures_;
};

/**
 * @brief SEC EDGAR API source
 */
class SecEdgarSource : public RegulatorySource {
public:
    SecEdgarSource(std::shared_ptr<ConfigurationManager> config,
                  std::shared_ptr<StructuredLogger> logger)
        : RegulatorySource("sec_edgar", "SEC EDGAR API",
                          RegulatorySourceType::SEC_EDGAR, config, logger) {}

    bool initialize() override;
    std::vector<RegulatoryChange> check_for_changes() override;
    nlohmann::json get_configuration() const override;
    bool test_connectivity() override;

private:
    std::string api_key_;
    std::string base_url_;
    std::string last_processed_filing_;

    // Helper methods
    std::vector<nlohmann::json> fetch_recent_filings();
    bool is_new_filing(const nlohmann::json& filing);
    bool is_regulatory_filing(const nlohmann::json& filing);
    std::optional<RegulatoryChange> process_filing(const nlohmann::json& filing);
    RegulatoryImpact determine_severity(const std::string& form_type);
    std::chrono::system_clock::time_point parse_filing_date(const std::string& date_str);
    std::vector<std::string> extract_keywords(const nlohmann::json& filing);
    std::string generate_filing_summary(const nlohmann::json& filing);
    void update_last_processed_filing(const std::string& accession);

    // HTTP helpers
    struct HttpResponse {
        int status_code;
        std::string body;
    };
    HttpResponse make_http_request(const std::string& url, const std::string& method);
};

/**
 * @brief FCA Regulatory API source
 */
class FcaRegulatorySource : public RegulatorySource {
public:
    FcaRegulatorySource(std::shared_ptr<ConfigurationManager> config,
                       std::shared_ptr<StructuredLogger> logger)
        : RegulatorySource("fca_regulatory", "FCA Regulatory API",
                          RegulatorySourceType::FCA_REGULATORY, config, logger) {}

    bool initialize() override;
    std::vector<RegulatoryChange> check_for_changes() override;
    nlohmann::json get_configuration() const override;
    bool test_connectivity() override;

private:
    std::string api_key_;
    std::string base_url_;
    std::string last_update_timestamp_;

    // Helper methods
    std::vector<nlohmann::json> fetch_regulatory_updates();
    bool is_new_update(const nlohmann::json& update);
    std::optional<RegulatoryChange> process_update(const nlohmann::json& update);
    RegulatoryImpact determine_fca_severity(const std::string& update_type);
    std::chrono::system_clock::time_point parse_publish_date(const std::string& date_str);
    std::vector<std::string> extract_fca_keywords(const nlohmann::json& update);
    std::string generate_fca_summary(const nlohmann::json& update);
    void update_last_timestamp(const std::string& timestamp);

    // HTTP helpers
    struct HttpResponse {
        int status_code;
        std::string body;
    };
    HttpResponse make_http_request(const std::string& url, const std::string& method);
};

/**
 * @brief ECB Announcements RSS source
 */
class EcbAnnouncementsSource : public RegulatorySource {
public:
    EcbAnnouncementsSource(std::shared_ptr<ConfigurationManager> config,
                          std::shared_ptr<StructuredLogger> logger)
        : RegulatorySource("ecb_announcements", "ECB Announcements RSS",
                          RegulatorySourceType::ECB_ANNOUNCEMENTS, config, logger) {}

    bool initialize() override;
    std::vector<RegulatoryChange> check_for_changes() override;
    nlohmann::json get_configuration() const override;
    bool test_connectivity() override;

    std::chrono::seconds get_check_interval() const override {
        return std::chrono::seconds(900); // 15 minutes for RSS feeds
    }

private:
    std::string rss_url_;
    std::string last_processed_guid_;
};

/**
 * @brief Custom regulatory feed source
 */
class CustomFeedSource : public RegulatorySource {
public:
    CustomFeedSource(std::string source_id, std::string name,
                    const nlohmann::json& config,
                    std::shared_ptr<ConfigurationManager> config_mgr,
                    std::shared_ptr<StructuredLogger> logger)
        : RegulatorySource(source_id, name, RegulatorySourceType::CUSTOM_FEED,
                          config_mgr, logger), feed_config_(config) {}

    bool initialize() override;
    std::vector<RegulatoryChange> check_for_changes() override;
    nlohmann::json get_configuration() const override;
    bool test_connectivity() override;

private:
    nlohmann::json feed_config_;
    std::string feed_url_;
    std::string feed_type_; // RSS, API, HTML
    std::unordered_map<std::string, std::string> headers_;
};

/**
 * @brief Web scraping source for regulatory websites
 */
class WebScrapingSource : public RegulatorySource {
public:
    WebScrapingSource(std::string source_id, std::string name,
                     const nlohmann::json& config,
                     std::shared_ptr<ConfigurationManager> config_mgr,
                     std::shared_ptr<StructuredLogger> logger)
        : RegulatorySource(source_id, name, RegulatorySourceType::WEB_SCRAPING,
                          config_mgr, logger), scraping_config_(config) {}

    bool initialize() override;
    std::vector<RegulatoryChange> check_for_changes() override;
    nlohmann::json get_configuration() const override;
    bool test_connectivity() override;

private:
    nlohmann::json scraping_config_;
    std::string target_url_;
    std::vector<std::string> selectors_; // CSS selectors for content extraction
    std::string last_content_hash_;
};

/**
 * @brief Factory for creating regulatory sources
 */
class RegulatorySourceFactory {
public:
    static std::shared_ptr<RegulatorySource> create_source(
        RegulatorySourceType type,
        const nlohmann::json& config,
        std::shared_ptr<ConfigurationManager> config_mgr,
        std::shared_ptr<StructuredLogger> logger
    );

    static std::shared_ptr<RegulatorySource> create_custom_source(
        const std::string& source_id,
        const std::string& name,
        const nlohmann::json& config,
        std::shared_ptr<ConfigurationManager> config_mgr,
        std::shared_ptr<StructuredLogger> logger
    );

private:
    static std::string generate_source_id(const std::string& base_name);
};

} // namespace regulens

