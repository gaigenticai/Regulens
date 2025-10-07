/**
 * Web Scraping Data Source - Enhanced Regulatory Monitoring
 *
 * Production-grade web scraping that builds upon existing regulatory monitoring with:
 * - Intelligent change detection (not just regex matching)
 * - Anti-detection measures (rate limiting, user agents, delays)
 * - Content parsing and structure extraction
 * - Historical comparison and delta analysis
 * - Error recovery and retry logic
 *
 * Retrospective Enhancement: Standardizes and improves existing SEC/FCA scraping
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <regex>
#include <chrono>
#include "../data_ingestion_framework.hpp"
#include "../../network/http_client.hpp"
#include "../../logging/structured_logger.hpp"
#include <nlohmann/json.hpp>

namespace regulens {

enum class ContentType {
    HTML,
    XML,
    JSON,
    RSS_FEED,
    ATOM_FEED
};

enum class ChangeDetectionStrategy {
    CONTENT_HASH,
    STRUCTURE_COMPARISON,
    KEYWORD_MATCHING,
    REGEX_PATTERN,
    DOM_DIFFERENCE
};

struct ScrapingRule {
    std::string rule_name;
    std::string selector; // CSS selector or XPath
    std::string attribute; // Attribute to extract (empty for text content)
    std::string data_type; // "text", "html", "attribute", "json"
    bool required = false;
    std::string default_value;
    std::unordered_map<std::string, std::string> transformations;
};

struct ContentSnapshot {
    std::string url;
    std::string content;
    std::string content_hash;
    std::chrono::system_clock::time_point timestamp;
};

struct WebScrapingConfig {
    std::string base_url;
    std::string start_url;
    std::vector<std::string> allowed_domains;
    std::vector<std::string> disallowed_paths;
    ContentType content_type = ContentType::HTML;
    ChangeDetectionStrategy change_strategy = ChangeDetectionStrategy::CONTENT_HASH;

    // Scraping rules
    std::vector<ScrapingRule> extraction_rules;

    // Anti-detection measures
    std::vector<std::string> user_agents;
    std::chrono::milliseconds delay_between_requests = std::chrono::milliseconds(1000);
    bool randomize_delays = true;
    int max_concurrent_requests = 1;
    long request_timeout_seconds = 30;

    // URL filtering
    std::vector<std::string> url_patterns_whitelist;
    std::vector<std::string> url_patterns_blacklist;
    bool respect_robots_txt = true;

    // Change detection
    std::chrono::hours content_retention_period = std::chrono::hours(24);
    double similarity_threshold = 0.95; // For structure comparison
    std::vector<std::string> keywords_to_monitor;
    std::vector<std::string> change_detection_keywords;
    std::vector<std::string> change_detection_patterns;

    // Error handling
    int max_retries_per_page = 3;
    int max_retries = 3;
    int max_snapshot_history = 1000;
    std::chrono::seconds retry_delay = std::chrono::seconds(5);
    bool follow_redirects = true;
    int max_redirect_depth = 3;

    // Content processing
    bool extract_metadata = true;
    bool store_raw_content = false;
    std::unordered_map<std::string, std::string> custom_headers;
};

class WebScrapingSource : public DataSource {
public:
    WebScrapingSource(const DataIngestionConfig& config,
                     std::shared_ptr<HttpClient> http_client,
                     StructuredLogger* logger);

    ~WebScrapingSource() override;

    // DataSource interface implementation
    bool connect() override;
    void disconnect() override;
    bool is_connected() const override;
    std::vector<nlohmann::json> fetch_data() override;
    bool validate_connection() override;

    // Web scraping specific methods
    void set_scraping_config(const WebScrapingConfig& scraping_config);
    std::vector<nlohmann::json> scrape_page(const std::string& url);
    bool has_content_changed(const std::string& url, const std::string& content);
    nlohmann::json extract_structured_data(const std::string& html_content);

    // Change detection methods
    std::string calculate_content_hash(const std::string& content);
    double calculate_content_similarity(const std::string& old_content, const std::string& new_content);
    std::vector<nlohmann::json> detect_content_changes(const std::string& old_content, const std::string& new_content);

private:
    // Scraping execution
    std::string fetch_page_content(const std::string& url);
    std::vector<std::string> discover_urls(const std::string& content, const std::string& base_url);
    bool is_url_allowed(const std::string& url);

    // Content processing
    nlohmann::json parse_html_content(const std::string& html);
    nlohmann::json parse_xml_content(const std::string& xml);
    nlohmann::json parse_rss_content(const std::string& rss);
    nlohmann::json extract_data_with_rules(const std::string& content);

    // Change detection implementation
    bool detect_changes_by_hash(const std::string& url, const std::string& content);
    bool detect_changes_by_structure(const std::string& url, const std::string& content);
    bool detect_changes_by_keywords(const std::string& url, const std::string& content);
    bool detect_changes_by_regex(const std::string& url, const std::string& content);

    // Data extraction helpers
    std::string extract_by_css_selector(const std::string& html, const std::string& selector, const std::string& attribute = "");
    std::string extract_by_regex(const std::string& content, const std::regex& pattern);
    nlohmann::json extract_json_path(const nlohmann::json& json_data, const std::string& path);

    // Anti-detection measures
    std::string get_random_user_agent();
    void apply_request_delay();
    bool should_respect_robots_txt(const std::string& url);

    // Content storage and comparison
    bool store_content_snapshot(const std::string& url, const std::string& content, const std::string& hash);
    std::optional<std::pair<std::string, std::string>> get_last_content_snapshot(const std::string& url);
    void cleanup_old_snapshots();

    // Error handling and recovery
    std::string handle_http_error(int status_code, const std::string& url);
    bool should_retry_request(int attempt, int status_code);
    void apply_exponential_backoff(int attempt);

    // Metadata extraction
    nlohmann::json extract_page_metadata(const std::string& content, const std::string& url);
    std::vector<std::string> extract_keywords(const std::string& content);
    std::chrono::system_clock::time_point extract_publication_date(const std::string& content);
    std::string extract_title(const std::string& content);
    std::string extract_meta_description(const std::string& content);
    std::vector<nlohmann::json> extract_documents(const std::string& content);

    // Internal state
    WebScrapingConfig scraping_config_;
    bool connected_;
    std::unordered_map<std::string, std::string> last_known_hashes_;
    std::unordered_map<std::string, std::unordered_map<std::string, int>> last_known_structures_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_request_times_;
    std::unordered_map<std::string, std::vector<std::string>> robots_txt_cache_; // base_url -> disallowed paths
    std::unordered_map<std::string, ContentSnapshot> content_snapshots_; // url -> snapshot
    int consecutive_failures_;

    // Performance tracking
    int total_requests_made_;
    int successful_requests_;
    int failed_requests_;
    std::chrono::steady_clock::time_point last_scraping_run_;

    // Constants
    const std::chrono::hours SNAPSHOT_RETENTION_PERIOD = std::chrono::hours(48);
    const int MAX_CONTENT_SIZE = 10 * 1024 * 1024; // 10MB
    const std::chrono::seconds CONNECTION_TIMEOUT = std::chrono::seconds(30);
};

} // namespace regulens
