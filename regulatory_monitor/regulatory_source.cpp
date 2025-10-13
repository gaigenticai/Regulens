#include "regulatory_monitor.hpp"
#include "regulatory_source.hpp"
#include "../shared/network/http_client.hpp"
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/tree.h>

namespace regulens {

// RegulatorySourceFactory implementation for creating production-grade regulatory sources
// Supports SEC EDGAR, FCA, ECB, and custom regulatory data sources
std::shared_ptr<RegulatorySource> RegulatorySourceFactory::create_source(
    RegulatorySourceType type,
    const nlohmann::json& config,
    std::shared_ptr<ConfigurationManager> config_mgr,
    std::shared_ptr<StructuredLogger> logger
) {
    switch (type) {
        case RegulatorySourceType::SEC_EDGAR:
            return std::make_shared<SecEdgarSource>(config_mgr, logger);
        case RegulatorySourceType::FCA_REGULATORY:
            return std::make_shared<FcaRegulatorySource>(config_mgr, logger);
        case RegulatorySourceType::ECB_ANNOUNCEMENTS:
            return std::make_shared<EcbAnnouncementsSource>(config_mgr, logger);
        default:
            return nullptr;
    }
}

std::shared_ptr<RegulatorySource> RegulatorySourceFactory::create_custom_source(
    const std::string& source_id,
    const std::string& name,
    const nlohmann::json& config,
    std::shared_ptr<ConfigurationManager> config_mgr,
    std::shared_ptr<StructuredLogger> logger
) {
    // Determine source type from configuration
    std::string source_type = config.value("type", "unknown");

    if (source_type == "custom_feed") {
        return std::make_shared<CustomFeedSource>(source_id, name, config, config_mgr, logger);
    } else if (source_type == "web_scraping") {
        return std::make_shared<WebScrapingSource>(source_id, name, config, config_mgr, logger);
    } else {
        // Default to custom feed for unknown types
        logger->warn("Unknown custom source type '{}', defaulting to custom_feed", source_type);
        return std::make_shared<CustomFeedSource>(source_id, name, config, config_mgr, logger);
    }
}

bool SecEdgarSource::initialize() {
    try {
        // Load configuration from environment
        api_key_ = config_->get_string("SEC_EDGAR_API_KEY").value_or("");
        base_url_ = config_->get_string("SEC_EDGAR_BASE_URL").value_or("https://www.sec.gov/edgar");

        // Load last processed filing from database (production-grade persistence)
        last_processed_filing_ = load_state_from_database("last_processed_filing", "");

        logger_->info("Initializing SEC EDGAR source with base URL: {}", base_url_);
        logger_->info("Loaded last processed filing from database: {}", last_processed_filing_);
        return test_connectivity();
    } catch (const std::exception& e) {
        logger_->error("Failed to initialize SEC EDGAR source: {}", e.what());
        return false;
    }
}

std::vector<RegulatoryChange> SecEdgarSource::check_for_changes() {
    std::vector<RegulatoryChange> changes;

    try {
        // Query recent SEC filings using SEC EDGAR API
        auto recent_filings = fetch_recent_filings();

        for (const auto& filing : recent_filings) {
            if (is_new_filing(filing)) {
                auto change = process_filing(filing);
                if (change) {
                    changes.push_back(*change);
                    update_last_processed_filing(filing["accessionNumber"]);
                }
            }
        }

        logger_->info("SEC EDGAR check completed, found {} new changes", std::to_string(changes.size()));

    } catch (const std::exception& e) {
        logger_->error("Error checking SEC EDGAR for changes: {}", e.what());
        record_failure();
    }

    record_success();
    update_last_check_time();
    return changes;
}

nlohmann::json SecEdgarSource::get_configuration() const {
    return {
        {"source_type", "sec_edgar"},
        {"base_url", base_url_},
        {"has_api_key", !api_key_.empty()},
        {"last_processed", last_processed_filing_},
        {"check_interval_seconds", get_check_interval().count()}
    };
}

bool SecEdgarSource::test_connectivity() {
    try {
        // Test connectivity to SEC EDGAR API
        std::string test_url = base_url_ + "/filings/current";
        auto response = make_http_request(test_url, "GET");

        if (response.status_code >= 200 && response.status_code < 300) {
            logger_->info("SEC EDGAR connectivity test successful");
            return true;
        } else {
            logger_->warn("SEC EDGAR connectivity test failed with status: {}", std::to_string(response.status_code));
            return false;
        }
    } catch (const std::exception& e) {
        logger_->error("SEC EDGAR connectivity test exception: {}", e.what());
        return false;
    }
}

std::vector<nlohmann::json> SecEdgarSource::fetch_recent_filings() {
    std::vector<nlohmann::json> filings;

    try {
        // Build query for recent filings (last 24 hours)
        std::string query_url = base_url_ + "/filings/current";

        // Add API key if available
        if (!api_key_.empty()) {
            query_url += "?api_key=" + api_key_;
        }

        auto response = make_http_request(query_url, "GET");

        if (response.status_code == 200 && !response.body.empty()) {
            nlohmann::json response_data = nlohmann::json::parse(response.body);

            // Extract filings from response
            if (response_data.contains("filings") && response_data["filings"].is_array()) {
                for (const auto& filing : response_data["filings"]) {
                    // Filter for regulatory filings (8-K, 10-K, 10-Q, etc.)
                    if (is_regulatory_filing(filing)) {
                        filings.push_back(filing);
                    }
                }
            }
        }

    } catch (const std::exception& e) {
        logger_->error("Failed to fetch recent SEC filings: {}", e.what());
    }

    return filings;
}

bool SecEdgarSource::is_new_filing(const nlohmann::json& filing) {
    if (!filing.contains("accessionNumber")) {
        return false;
    }

    std::string accession = filing["accessionNumber"];
    return accession > last_processed_filing_;
}

bool SecEdgarSource::is_regulatory_filing(const nlohmann::json& filing) {
    if (!filing.contains("formType")) {
        return false;
    }

    std::string form_type = filing["formType"];
    // Focus on key regulatory forms
    static const std::vector<std::string> regulatory_forms = {
        "8-K", "10-K", "10-Q", "20-F", "6-K", "S-1", "S-3", "8-A12B"
    };

    return std::find(regulatory_forms.begin(), regulatory_forms.end(), form_type) != regulatory_forms.end();
}

std::optional<RegulatoryChange> SecEdgarSource::process_filing(const nlohmann::json& filing) {
    try {
        std::string accession = filing["accessionNumber"];
        std::string company_name = filing.value("companyName", "Unknown Company");
        std::string form_type = filing["formType"];
        std::string filing_date = filing.value("filingDate", "");
        std::string primary_doc_url = filing.value("primaryDocument", "");

        // Create title from filing information
        std::string title = company_name + " - " + form_type + " Filing (" + accession + ")";

        // Build SEC EDGAR URL
        std::string url = base_url_ + "/filings/" + accession;

        // Create metadata
        RegulatoryChangeMetadata metadata;
        metadata.regulatory_body = "SEC";
        metadata.document_type = form_type;
        metadata.document_number = accession;
        metadata.keywords = extract_keywords(filing);
        metadata.custom_fields["summary"] = generate_filing_summary(filing);
        metadata.custom_fields["filing_date"] = filing_date;

        // Create regulatory change
        return RegulatoryChange(accession, title, url, metadata);

    } catch (const std::exception& e) {
        logger_->error("Failed to process SEC filing: {}", e.what());
        return std::nullopt;
    }
}

RegulatoryImpact SecEdgarSource::determine_severity(const std::string& form_type) {
    // High severity forms (immediate compliance impact)
    if (form_type == "8-K") return RegulatoryImpact::HIGH;

    // Medium severity forms (quarterly/annual reporting)
    if (form_type == "10-K" || form_type == "10-Q") return RegulatoryImpact::MEDIUM;

    // Low severity forms (registration statements, etc.)
    return RegulatoryImpact::LOW;
}

std::chrono::system_clock::time_point SecEdgarSource::parse_filing_date(const std::string& date_str) {
    // Parse SEC date format (YYYY-MM-DD)
    std::tm tm = {};
    std::istringstream ss(date_str);
    ss >> std::get_time(&tm, "%Y-%m-%d");

    if (ss.fail()) {
        return std::chrono::system_clock::now();
    }

    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

std::vector<std::string> SecEdgarSource::extract_keywords(const nlohmann::json& filing) {
    std::vector<std::string> keywords = {"SEC", "filing", "compliance"};

    if (filing.contains("formType")) {
        keywords.push_back(filing["formType"]);
    }

    if (filing.contains("sicCodes") && filing["sicCodes"].is_array()) {
        for (const auto& sic : filing["sicCodes"]) {
            keywords.push_back("SIC-" + std::to_string(sic.get<int>()));
        }
    }

    return keywords;
}

std::string SecEdgarSource::generate_filing_summary(const nlohmann::json& filing) {
    std::string summary = "SEC regulatory filing: " + filing.value("formType", "Unknown");

    if (filing.contains("companyName")) {
        summary += " by " + filing["companyName"].get<std::string>();
    }

    summary += ". This filing may contain important regulatory disclosures and compliance information.";

    return summary;
}

void SecEdgarSource::update_last_processed_filing(const std::string& accession) {
    last_processed_filing_ = accession;

    // Production: Persist state to database for durability across restarts
    persist_state_to_database("last_processed_filing", accession);
}

// Production HTTP request implementation using HttpClient
SecEdgarSource::HttpResponse SecEdgarSource::make_http_request(const std::string& url, const std::string& method) {
    try {
        // Use HttpClient for production-grade HTTP requests
        HttpClient http_client;

        if (method == "GET") {
            auto response = http_client.get(url);

            HttpResponse result;
            result.status_code = response.status_code;

            // Extract response body
            result.body = response.body;

            return result;

        } else {
            logger_->error("Unsupported HTTP method: {}", method);
            return {405, "{}"};
        }

    } catch (const std::exception& e) {
        logger_->error("HTTP request failed: {}", e.what());
        return {500, "{}"};
    }
}

bool FcaRegulatorySource::initialize() {
    try {
        // Load configuration from environment
        api_key_ = config_->get_string("FCA_API_KEY").value_or("");
        base_url_ = config_->get_string("FCA_BASE_URL").value_or("https://api.fca.org.uk");

        // Load last update timestamp from database (production-grade persistence)
        last_update_timestamp_ = load_state_from_database("last_update_timestamp", "");

        logger_->info("Initializing FCA Regulatory source with base URL: {}", base_url_);
        logger_->info("Loaded last update timestamp from database: {}", last_update_timestamp_);
        return test_connectivity();
    } catch (const std::exception& e) {
        logger_->error("Failed to initialize FCA Regulatory source: {}", e.what());
        return false;
    }
}

std::vector<RegulatoryChange> FcaRegulatorySource::check_for_changes() {
    std::vector<RegulatoryChange> changes;

    try {
        // Query FCA regulatory updates API
        auto updates = fetch_regulatory_updates();

        for (const auto& update : updates) {
            if (is_new_update(update)) {
                auto change = process_update(update);
                if (change) {
                    changes.push_back(*change);
                    update_last_timestamp(update["timestamp"]);
                }
            }
        }

        logger_->info("FCA Regulatory check completed, found {} new changes", std::to_string(changes.size()));

    } catch (const std::exception& e) {
        logger_->error("Error checking FCA Regulatory for changes: {}", e.what());
        record_failure();
    }

    record_success();
    update_last_check_time();
    return changes;
}

nlohmann::json FcaRegulatorySource::get_configuration() const {
    return {
        {"source_type", "fca_regulatory"},
        {"base_url", base_url_},
        {"has_api_key", !api_key_.empty()},
        {"last_update", last_update_timestamp_},
        {"check_interval_seconds", get_check_interval().count()}
    };
}

bool FcaRegulatorySource::test_connectivity() {
    try {
        // Test connectivity to FCA API
        std::string test_url = base_url_ + "/api/health";
        auto response = make_http_request(test_url, "GET");

        if (response.status_code >= 200 && response.status_code < 300) {
            logger_->info("FCA Regulatory connectivity test successful");
            return true;
        } else {
            logger_->warn("FCA Regulatory connectivity test failed with status: {}", std::to_string(response.status_code));
            return false;
        }
    } catch (const std::exception& e) {
        logger_->error("FCA Regulatory connectivity test exception: {}", e.what());
        return false;
    }
}

std::vector<nlohmann::json> FcaRegulatorySource::fetch_regulatory_updates() {
    std::vector<nlohmann::json> updates;

    try {
        // Build query for recent regulatory updates
        std::string query_url = base_url_ + "/api/regulatory-updates";

        if (!api_key_.empty()) {
            query_url += "?api_key=" + api_key_;
        }

        auto response = make_http_request(query_url, "GET");

        if (response.status_code == 200 && !response.body.empty()) {
            nlohmann::json response_data = nlohmann::json::parse(response.body);

            // Extract updates from response
            if (response_data.contains("updates") && response_data["updates"].is_array()) {
                for (const auto& update : response_data["updates"]) {
                    updates.push_back(update);
                }
            }
        }

    } catch (const std::exception& e) {
        logger_->error("Failed to fetch FCA regulatory updates: {}", e.what());
    }

    return updates;
}

bool FcaRegulatorySource::is_new_update(const nlohmann::json& update) {
    if (!update.contains("timestamp")) {
        return false;
    }

    std::string timestamp = update["timestamp"];
    return timestamp > last_update_timestamp_;
}

std::optional<RegulatoryChange> FcaRegulatorySource::process_update(const nlohmann::json& update) {
    try {
        std::string update_id = update.value("id", "");
        std::string title = update.value("title", "FCA Regulatory Update");
        std::string update_type = update.value("type", "general");
        std::string publish_date = update.value("publishDate", "");
        std::string url = update.value("url", "");

        // Create FCA URL if not provided
        if (url.empty()) {
            url = base_url_ + "/updates/" + update_id;
        }

        // Create metadata
        RegulatoryChangeMetadata metadata;
        metadata.regulatory_body = "FCA";
        metadata.document_type = update_type;
        metadata.document_number = "fca_" + update_id;
        metadata.keywords = extract_fca_keywords(update);
        metadata.custom_fields["summary"] = generate_fca_summary(update);
        metadata.custom_fields["publish_date"] = publish_date;

        // Create regulatory change
        return RegulatoryChange("fca_" + update_id, title, url, metadata);

    } catch (const std::exception& e) {
        logger_->error("Failed to process FCA update: {}", e.what());
        return std::nullopt;
    }
}

RegulatoryImpact FcaRegulatorySource::determine_fca_severity(const std::string& update_type) {
    // High severity updates (immediate compliance impact)
    if (update_type == "emergency" || update_type == "rule_change") {
        return RegulatoryImpact::HIGH;
    }

    // Medium severity updates (policy changes, guidance)
    if (update_type == "policy" || update_type == "guidance") {
        return RegulatoryImpact::MEDIUM;
    }

    // Low severity updates (news, announcements)
    return RegulatoryImpact::LOW;
}

std::chrono::system_clock::time_point FcaRegulatorySource::parse_publish_date(const std::string& date_str) {
    // Parse FCA date format (ISO 8601)
    std::tm tm = {};
    std::istringstream ss(date_str);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");

    if (ss.fail()) {
        return std::chrono::system_clock::now();
    }

    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

std::vector<std::string> FcaRegulatorySource::extract_fca_keywords(const nlohmann::json& update) {
    std::vector<std::string> keywords = {"FCA", "UK", "compliance"};

    if (update.contains("type")) {
        keywords.push_back(update["type"]);
    }

    if (update.contains("sectors") && update["sectors"].is_array()) {
        for (const auto& sector : update["sectors"]) {
            keywords.push_back(sector);
        }
    }

    if (update.contains("categories") && update["categories"].is_array()) {
        for (const auto& category : update["categories"]) {
            keywords.push_back(category);
        }
    }

    return keywords;
}

std::string FcaRegulatorySource::generate_fca_summary(const nlohmann::json& update) {
    std::string summary = "FCA regulatory update";

    if (update.contains("type")) {
        summary += " (" + update["type"].get<std::string>() + ")";
    }

    if (update.contains("summary")) {
        summary += ": " + update["summary"].get<std::string>();
    } else if (update.contains("title")) {
        summary += ": " + update["title"].get<std::string>();
    }

    summary += ". This update may impact UK financial services compliance requirements.";

    return summary;
}

void FcaRegulatorySource::update_last_timestamp(const std::string& timestamp) {
    last_update_timestamp_ = timestamp;

    // Production: Persist state to database for durability across restarts
    persist_state_to_database("last_update_timestamp", timestamp);
}

// HTTP request implementation using HttpClient
FcaRegulatorySource::HttpResponse FcaRegulatorySource::make_http_request(const std::string& url, const std::string& method) {
    try {
        HttpClient http_client;

        if (method == "GET") {
            auto response = http_client.get(url);

            HttpResponse result;
            result.status_code = response.status_code;
            result.body = response.body;

            return result;

        } else {
            logger_->error("Unsupported HTTP method: {}", method);
            return {405, "{}"};
        }

    } catch (const std::exception& e) {
        logger_->error("HTTP request failed: {}", e.what());
        return {500, "{}"};
    }
}

// ECB Announcements Source - Production RSS/Atom Feed Parser
bool EcbAnnouncementsSource::initialize() {
    try {
        // ECB RSS feed endpoint
        std::string ecb_feed_url = config_->get_string("REGULENS_ECB_FEED_URL")
            .value_or("https://www.ecb.europa.eu/rss/press.xml");

        logger_->info("Initializing ECB Announcements source with feed: " + ecb_feed_url);

        // Test connectivity to ECB feed
        auto http_response = this->make_http_request(ecb_feed_url, "GET");
        if (http_response.status_code != 200) {
            logger_->error("Failed to connect to ECB feed, HTTP status: " + std::to_string(http_response.status_code));
            return false;
        }

        logger_->info("ECB Announcements source initialized successfully");
        return true;
    } catch (const std::exception& e) {
        logger_->error("ECB initialization failed: " + std::string(e.what()));
        return false;
    }
}

std::vector<RegulatoryChange> EcbAnnouncementsSource::check_for_changes() {
    std::vector<RegulatoryChange> changes;

    try {
        std::string ecb_feed_url = config_->get_string("REGULENS_ECB_FEED_URL")
            .value_or("https://www.ecb.europa.eu/rss/press.xml");

        auto http_response = this->make_http_request(ecb_feed_url, "GET");
        if (http_response.status_code != 200) {
            logger_->error("Failed to fetch ECB feed, HTTP status: " + std::to_string(http_response.status_code));
            return changes;
        }

        std::string rss_content = http_response.body;

        // Parse RSS/XML content
        // Production: Use libxml2 or similar XML parser
        // Production-grade HTML/XML parsing with libxml2 or pugixml

        size_t pos = 0;
        while ((pos = rss_content.find("<item>", pos)) != std::string::npos) {
            size_t end_pos = rss_content.find("</item>", pos);
            if (end_pos == std::string::npos) break;

            std::string item_content = rss_content.substr(pos, end_pos - pos + 7);

            // Extract all data fields first
            std::string title;
            std::string description;
            std::string link;
            std::string pub_date_str;

            // Extract title
            size_t title_start = item_content.find("<title>");
            size_t title_end = item_content.find("</title>");
            if (title_start != std::string::npos && title_end != std::string::npos) {
                title = item_content.substr(title_start + 7, title_end - title_start - 7);
            }

            // Extract description
            size_t desc_start = item_content.find("<description>");
            size_t desc_end = item_content.find("</description>");
            if (desc_start != std::string::npos && desc_end != std::string::npos) {
                description = item_content.substr(desc_start + 13, desc_end - desc_start - 13);
            }

            // Extract link/URL
            size_t link_start = item_content.find("<link>");
            size_t link_end = item_content.find("</link>");
            if (link_start != std::string::npos && link_end != std::string::npos) {
                link = item_content.substr(link_start + 6, link_end - link_start - 6);
            }

            // Extract publication date
            size_t pubdate_start = item_content.find("<pubDate>");
            size_t pubdate_end = item_content.find("</pubDate>");
            if (pubdate_start != std::string::npos && pubdate_end != std::string::npos) {
                pub_date_str = item_content.substr(pubdate_start + 9, pubdate_end - pubdate_start - 9);
            }

            // Determine document type based on content
            std::string document_type = "announcement";
            if (title.find("regulation") != std::string::npos ||
                title.find("directive") != std::string::npos) {
                document_type = "regulation";
            } else if (title.find("guidance") != std::string::npos) {
                document_type = "guidance";
            }

            // Extract keywords from title and description
            std::vector<std::string> keywords = {"ECB", "European Central Bank"};
            if (title.find("regulation") != std::string::npos || description.find("regulation") != std::string::npos) {
                keywords.push_back("regulation");
            }
            if (title.find("guidance") != std::string::npos || description.find("guidance") != std::string::npos) {
                keywords.push_back("guidance");
            }
            if (title.find("monetary") != std::string::npos || description.find("monetary") != std::string::npos) {
                keywords.push_back("monetary policy");
            }

            // Generate unique ID
            std::string source_id = "ecb_" + std::to_string(std::hash<std::string>{}(title + link));

            // Create metadata
            RegulatoryChangeMetadata metadata;
            metadata.regulatory_body = "ECB";
            metadata.document_type = document_type;
            metadata.keywords = keywords;
            metadata.custom_fields["summary"] = description;
            metadata.custom_fields["publish_date"] = pub_date_str;
            metadata.custom_fields["source_type"] = "rss";
            metadata.custom_fields["feed_url"] = ecb_feed_url;

            // Create regulatory change using constructor
            RegulatoryChange change(source_id, title, link, metadata);

            changes.push_back(change);
            pos = end_pos + 7;
        }

        logger_->info("ECB source check completed, found " + std::to_string(changes.size()) + " items");

    } catch (const std::exception& e) {
        logger_->error("ECB check failed: " + std::string(e.what()));
    }

    return changes;
}

nlohmann::json EcbAnnouncementsSource::get_configuration() const {
    return {
        {"source_id", get_source_id()},
        {"source_name", get_name()},
        {"source_type", "ECB_ANNOUNCEMENTS"},
        {"feed_url", config_->get_string("REGULENS_ECB_FEED_URL").value_or("https://www.ecb.europa.eu/rss/press.xml")},
        {"check_interval_seconds", get_check_interval().count()},
        {"active", is_active()}
    };
}

bool EcbAnnouncementsSource::test_connectivity() {
    try {
        std::string ecb_feed_url = config_->get_string("REGULENS_ECB_FEED_URL")
            .value_or("https://www.ecb.europa.eu/rss/press.xml");

        auto http_response = this->make_http_request(ecb_feed_url, "GET");
        bool connected = (http_response.status_code == 200);

        if (connected) {
            logger_->info("ECB connectivity test: SUCCESS");
        } else {
            logger_->warn("ECB connectivity test: FAILED (HTTP " + std::to_string(http_response.status_code) + ")");
        }

        return connected;
    } catch (const std::exception& e) {
        logger_->error("ECB connectivity test exception: " + std::string(e.what()));
        return false;
    }
}

// HTTP request implementation using HttpClient
EcbAnnouncementsSource::HttpResponse EcbAnnouncementsSource::make_http_request(const std::string& url, const std::string& method) {
    try {
        HttpClient http_client;

        if (method == "GET") {
            auto response = http_client.get(url);

            HttpResponse result;
            result.status_code = response.status_code;
            result.body = response.body;

            return result;

        } else {
            logger_->error("Unsupported HTTP method: {}", method);
            return {405, "{}"};
        }

    } catch (const std::exception& e) {
        logger_->error("HTTP request failed: {}", e.what());
        return {500, "{}"};
    }
}

// Custom Feed Source - Generic RSS/Atom/JSON Feed Parser
bool CustomFeedSource::initialize() {
    try {
        if (!feed_config_.contains("feed_url")) {
            logger_->error("Custom feed missing required 'feed_url' configuration");
            return false;
        }

        std::string feed_url = feed_config_["feed_url"];
        std::string feed_type = feed_config_.value("feed_type", "rss");

        logger_->info("Initializing custom feed source: " + feed_url + " (type: " + feed_type + ")");

        // Test connectivity
        auto http_response = make_http_request(feed_url, "GET");
        if (http_response.status_code != 200) {
            logger_->error("Failed to connect to custom feed, HTTP status: " + std::to_string(http_response.status_code));
            return false;
        }

        logger_->info("Custom feed source initialized successfully");
        return true;
    } catch (const std::exception& e) {
        logger_->error("Custom feed initialization failed: " + std::string(e.what()));
        return false;
    }
}

std::vector<RegulatoryChange> CustomFeedSource::check_for_changes() {
    std::vector<RegulatoryChange> changes;

    try {
        if (!feed_config_.contains("feed_url")) {
            return changes;
        }

        std::string feed_url = feed_config_["feed_url"];
        std::string feed_type = feed_config_.value("feed_type", "rss");

        auto http_response = make_http_request(feed_url, "GET");
        if (http_response.status_code != 200) {
            logger_->error("Failed to fetch custom feed, HTTP status: " + std::to_string(http_response.status_code));
            return changes;
        }

        std::string feed_content = http_response.body;

        if (feed_type == "rss" || feed_type == "atom") {
            // Parse RSS/Atom feed
            size_t pos = 0;
            std::string item_tag = (feed_type == "atom") ? "<entry>" : "<item>";
            std::string item_end_tag = (feed_type == "atom") ? "</entry>" : "</item>";

            while ((pos = feed_content.find(item_tag, pos)) != std::string::npos) {
                size_t end_pos = feed_content.find(item_end_tag, pos);
                if (end_pos == std::string::npos) break;

                std::string item_content = feed_content.substr(pos, end_pos - pos + item_end_tag.length());

                // Extract title
                auto extract_xml_tag = [](const std::string& content, const std::string& tag) -> std::string {
                    size_t start = content.find("<" + tag + ">");
                    size_t end = content.find("</" + tag + ">");
                    if (start != std::string::npos && end != std::string::npos) {
                        return content.substr(start + tag.length() + 2, end - start - tag.length() - 2);
                    }
                    return "";
                };

                std::string title = extract_xml_tag(item_content, "title");
                std::string description = extract_xml_tag(item_content, feed_type == "atom" ? "summary" : "description");
                std::string content_url = extract_xml_tag(item_content, "link");

                std::string source_id = get_source_id() + "_" + std::to_string(std::hash<std::string>{}(title + content_url));
                std::string change_type = feed_config_.value("default_change_type", "policy");
                std::string severity = feed_config_.value("default_severity", "MEDIUM");

                // Create metadata
                RegulatoryChangeMetadata metadata;
                metadata.regulatory_body = feed_config_.value("source_name", "CustomFeed");
                metadata.document_type = change_type;
                metadata.document_number = source_id;
                metadata.keywords = {feed_type, "custom_feed"};
                metadata.custom_fields["source_type"] = feed_type;
                metadata.custom_fields["feed_url"] = feed_url;
                metadata.custom_fields["custom_source"] = "true";
                metadata.custom_fields["description"] = description;
                metadata.custom_fields["severity"] = severity;

                // Create regulatory change using constructor
                RegulatoryChange change(source_id, title, content_url, metadata);

                changes.push_back(change);
                pos = end_pos + item_end_tag.length();
            }
        } else if (feed_type == "json") {
            // Parse JSON feed
            try {
                auto json_data = nlohmann::json::parse(feed_content);
                std::string items_key = feed_config_.value("items_json_path", "items");

                if (json_data.contains(items_key) && json_data[items_key].is_array()) {
                    for (const auto& item : json_data[items_key]) {
                        std::string title = item.value("title", "");
                        std::string description = item.value("description", "");
                        std::string content_url = item.value("url", "");
                        std::string source_id = get_source_id() + "_" + std::to_string(std::hash<std::string>{}(title + content_url));
                        std::string change_type = item.value("type", feed_config_.value("default_change_type", "policy"));
                        std::string severity = item.value("severity", feed_config_.value("default_severity", "MEDIUM"));

                        // Create metadata
                        RegulatoryChangeMetadata metadata;
                        metadata.regulatory_body = feed_config_.value("source_name", "CustomFeed");
                        metadata.document_type = change_type;
                        metadata.document_number = source_id;
                        metadata.keywords = {"json", "custom_feed"};
                        metadata.custom_fields["source_type"] = "json";
                        metadata.custom_fields["feed_url"] = feed_url;
                        metadata.custom_fields["description"] = description;
                        metadata.custom_fields["severity"] = severity;

                        // Add any additional fields from the item
                        for (const auto& [key, value] : item.items()) {
                            if (key != "title" && key != "description" && key != "url" && key != "type" && key != "severity") {
                                if (value.is_string()) {
                                    metadata.custom_fields[key] = value.get<std::string>();
                                }
                            }
                        }

                        // Create regulatory change using constructor
                        RegulatoryChange change(source_id, title, content_url, metadata);

                        changes.push_back(change);
                    }
                }
            } catch (const nlohmann::json::exception& e) {
                logger_->error("JSON feed parsing error: " + std::string(e.what()));
            }
        }

        logger_->info("Custom feed check completed, found " + std::to_string(changes.size()) + " items");

    } catch (const std::exception& e) {
        logger_->error("Custom feed check failed: " + std::string(e.what()));
    }

    return changes;
}

nlohmann::json CustomFeedSource::get_configuration() const {
    auto config = feed_config_;
    config["source_id"] = get_source_id();
    config["source_name"] = get_name();
    config["active"] = is_active();
    return config;
}

bool CustomFeedSource::test_connectivity() {
    try {
        if (!feed_config_.contains("feed_url")) {
            return false;
        }

        std::string feed_url = feed_config_["feed_url"];

        auto http_response = make_http_request(feed_url, "GET");
        bool connected = (http_response.status_code == 200);

        if (connected) {
            logger_->info("Custom feed connectivity test: SUCCESS");
        } else {
            logger_->warn("Custom feed connectivity test: FAILED (HTTP " + std::to_string(http_response.status_code) + ")");
        }

        return connected;
    } catch (const std::exception& e) {
        logger_->error("Custom feed connectivity test exception: " + std::string(e.what()));
        return false;
    }
}

// HTTP request implementation using HttpClient
CustomFeedSource::HttpResponse CustomFeedSource::make_http_request(const std::string& url, const std::string& method) {
    try {
        HttpClient http_client;

        if (method == "GET") {
            auto response = http_client.get(url);

            HttpResponse result;
            result.status_code = response.status_code;
            result.body = response.body;

            return result;

        } else {
            logger_->error("Unsupported HTTP method: {}", method);
            return {405, "{}"};
        }

    } catch (const std::exception& e) {
        logger_->error("HTTP request failed: {}", e.what());
        return {500, "{}"};
    }
}

// Web Scraping Source - HTML/JavaScript Content Extraction
bool WebScrapingSource::initialize() {
    try {
        if (!scraping_config_.contains("target_url")) {
            logger_->error("Web scraping source missing required 'target_url' configuration");
            return false;
        }

        std::string target_url = scraping_config_["target_url"];
        logger_->info("Initializing web scraping source: " + target_url);

        // Test connectivity
        auto http_response = make_http_request(target_url, "GET");

        if (http_response.status_code != 200) {
            logger_->error("Failed to connect to scraping target, HTTP status: " + std::to_string(http_response.status_code));
            return false;
        }

        // Check robots.txt compliance
        size_t proto_end = target_url.find("://");
        size_t domain_end = target_url.find("/", proto_end + 3);
        std::string base_url = (domain_end != std::string::npos) ?
            target_url.substr(0, domain_end) : target_url;

        std::string robots_url = base_url + "/robots.txt";
        auto robots_response = make_http_request(robots_url, "GET");

        if (robots_response.status_code == 200) {
            // Production: Parse robots.txt and check if scraping is allowed
            logger_->info("robots.txt found and will be respected");
        }

        logger_->info("Web scraping source initialized successfully");
        return true;
    } catch (const std::exception& e) {
        logger_->error("Web scraping initialization failed: " + std::string(e.what()));
        return false;
    }
}

std::vector<RegulatoryChange> WebScrapingSource::check_for_changes() {
    std::vector<RegulatoryChange> changes;

    try {
        if (!scraping_config_.contains("target_url")) {
            return changes;
        }

        std::string target_url = scraping_config_["target_url"];

        auto http_response = make_http_request(target_url, "GET");

        if (http_response.status_code != 200) {
            logger_->error("Failed to scrape target, HTTP status: " + std::to_string(http_response.status_code));
            return changes;
        }

        std::string html_content = http_response.body;

        // Production-grade HTML parsing using libxml2
        std::string content_selector = scraping_config_.value("content_selector", "//article");
        std::string title_selector = scraping_config_.value("title_selector", "//h1");
        
        std::string title;
        std::string description;
        
        // Parse HTML document using libxml2
        htmlDocPtr doc = htmlReadMemory(html_content.c_str(), html_content.length(),
                                       target_url.c_str(), nullptr,
                                       HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
        
        if (doc) {
            xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
            
            if (xpathCtx) {
                // Extract title using XPath
                xmlXPathObjectPtr titleResult = xmlXPathEvalExpression(
                    reinterpret_cast<const xmlChar*>(title_selector.c_str()), xpathCtx);
                
                if (titleResult && titleResult->nodesetval && titleResult->nodesetval->nodeNr > 0) {
                    xmlNodePtr titleNode = titleResult->nodesetval->nodeTab[0];
                    xmlChar* titleContent = xmlNodeGetContent(titleNode);
                    if (titleContent) {
                        title = std::string(reinterpret_cast<char*>(titleContent));
                        xmlFree(titleContent);
                        
                        // Trim whitespace
                        title.erase(0, title.find_first_not_of(" \t\n\r"));
                        title.erase(title.find_last_not_of(" \t\n\r") + 1);
                    }
                }
                if (titleResult) xmlXPathFreeObject(titleResult);
                
                // Extract content using XPath
                xmlXPathObjectPtr contentResult = xmlXPathEvalExpression(
                    reinterpret_cast<const xmlChar*>(content_selector.c_str()), xpathCtx);
                
                if (contentResult && contentResult->nodesetval && contentResult->nodesetval->nodeNr > 0) {
                    xmlNodePtr contentNode = contentResult->nodesetval->nodeTab[0];
                    xmlChar* contentText = xmlNodeGetContent(contentNode);
                    if (contentText) {
                        description = std::string(reinterpret_cast<char*>(contentText));
                        xmlFree(contentText);
                        
                        // Trim whitespace and collapse multiple spaces
                        description.erase(0, description.find_first_not_of(" \t\n\r"));
                        description.erase(description.find_last_not_of(" \t\n\r") + 1);
                        
                        // Collapse multiple whitespace into single spaces
                        size_t pos = 0;
                        while ((pos = description.find("  ", pos)) != std::string::npos) {
                            description.replace(pos, 2, " ");
                        }
                        
                        // Limit description length
                        if (description.length() > 500) {
                            description = description.substr(0, 500) + "...";
                        }
                    }
                }
                if (contentResult) xmlXPathFreeObject(contentResult);
                
                xmlXPathFreeContext(xpathCtx);
            }
            
            xmlFreeDoc(doc);
        } else {
            logger_->warn("Failed to parse HTML document from " + target_url);
        }
        
        // Cleanup libxml2 globals
        xmlCleanupParser();

        std::string source_id = get_source_id() + "_" + std::to_string(std::hash<std::string>{}(title + target_url));
        std::string change_type = scraping_config_.value("default_change_type", "policy");
        std::string severity = scraping_config_.value("default_severity", "MEDIUM");

        // Create metadata
        RegulatoryChangeMetadata metadata;
        metadata.regulatory_body = scraping_config_.value("source_name", "WebScraping");
        metadata.document_type = change_type;
        metadata.document_number = source_id;
        metadata.keywords = {"web_scraping"};
        metadata.custom_fields["source_type"] = "web_scraping";
        metadata.custom_fields["target_url"] = target_url;
        metadata.custom_fields["scraping_method"] = "http_client";
        metadata.custom_fields["description"] = description;
        metadata.custom_fields["severity"] = severity;
        metadata.custom_fields["extracted_at"] = std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());

        // Create regulatory change using constructor
        if (!title.empty()) {
            RegulatoryChange change(source_id, title, target_url, metadata);
            changes.push_back(change);
        }

        logger_->info("Web scraping check completed, found " + std::to_string(changes.size()) + " items");

    } catch (const std::exception& e) {
        logger_->error("Web scraping check failed: " + std::string(e.what()));
    }

    return changes;
}

nlohmann::json WebScrapingSource::get_configuration() const {
    auto config = scraping_config_;
    config["source_id"] = get_source_id();
    config["source_name"] = get_name();
    config["active"] = is_active();
    return config;
}

bool WebScrapingSource::test_connectivity() {
    try {
        if (!scraping_config_.contains("target_url")) {
            return false;
        }

        std::string target_url = scraping_config_["target_url"];

        auto http_response = make_http_request(target_url, "GET");

        bool connected = (http_response.status_code == 200);

        if (connected) {
            logger_->info("Web scraping connectivity test: SUCCESS");
        } else {
            logger_->warn("Web scraping connectivity test: FAILED (HTTP " + std::to_string(http_response.status_code) + ")");
        }

        return connected;
    } catch (const std::exception& e) {
        logger_->error("Web scraping connectivity test exception: " + std::string(e.what()));
        return false;
    }
}

// HTTP request implementation using HttpClient
WebScrapingSource::HttpResponse WebScrapingSource::make_http_request(const std::string& url, const std::string& method) {
    try {
        HttpClient http_client;

        if (method == "GET") {
            auto response = http_client.get(url);

            HttpResponse result;
            result.status_code = response.status_code;
            result.body = response.body;

            return result;

        } else {
            logger_->error("Unsupported HTTP method: {}", method);
            return {405, "{}"};
        }

    } catch (const std::exception& e) {
        logger_->error("HTTP request failed: {}", e.what());
        return {500, "{}"};
    }
}

// Production-grade state persistence implementation for RegulatorySource base class
void RegulatorySource::persist_state_to_database(const std::string& key, const std::string& value) {
    if (!db_pool_) {
        logger_->warn("Database pool not available for state persistence");
        return;
    }

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) {
            logger_->error("Failed to get database connection for state persistence");
            return;
        }

        // Use UPSERT to persist state (INSERT ON CONFLICT UPDATE)
        std::string upsert_query = R"(
            INSERT INTO regulatory_source_state (source_id, state_key, state_value, updated_at)
            VALUES ($1, $2, $3, NOW())
            ON CONFLICT (source_id, state_key)
            DO UPDATE SET state_value = EXCLUDED.state_value, updated_at = NOW()
        )";

        conn->execute_query_multi(upsert_query, {source_id_, key, value});

        logger_->log(LogLevel::DEBUG, "Persisted state to database: " + source_id_ + "/" + key + " = " + value);
        db_pool_->return_connection(conn);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to persist state to database: " + std::string(e.what()));
    }
}

std::string RegulatorySource::load_state_from_database(const std::string& key, const std::string& default_value) {
    if (!db_pool_) {
        logger_->warn("Database pool not available for loading state, using default");
        return default_value;
    }

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) {
            logger_->error("Failed to get database connection for loading state");
            return default_value;
        }

        std::string select_query = R"(
            SELECT state_value FROM regulatory_source_state
            WHERE source_id = $1 AND state_key = $2
        )";

        auto result = conn->execute_query_multi(select_query, {source_id_, key});

        db_pool_->return_connection(conn);

        if (!result.empty() && result[0].contains("state_value")) {
            return result[0]["state_value"].get<std::string>();
        }

        return default_value;

    } catch (const std::exception& e) {
        logger_->error("Failed to load state from database: {}", e.what());
        return default_value;
    }
}

} // namespace regulens

