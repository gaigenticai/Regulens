/**
 * Production Regulatory Monitor - Implementation
 *
 * Real regulatory monitoring with web scraping and database persistence.
 */

#include "production_regulatory_monitor.hpp"
#include <regex>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace regulens {

// RegulatoryChange implementation
nlohmann::json RegulatoryChange::to_json() const {
    return {
        {"id", id},
        {"source", source},
        {"title", title},
        {"description", description},
        {"content_url", content_url},
        {"change_type", change_type},
        {"severity", severity},
        {"metadata", metadata},
        {"detected_at", std::chrono::duration_cast<std::chrono::milliseconds>(
            detected_at.time_since_epoch()).count()},
        {"published_at", std::chrono::duration_cast<std::chrono::milliseconds>(
            published_at.time_since_epoch()).count()}
    };
}

RegulatoryChange RegulatoryChange::from_json(const nlohmann::json& j) {
    RegulatoryChange change;

    change.id = j.value("id", "");
    change.source = j.value("source", "");
    change.title = j.value("title", "");
    change.description = j.value("description", "");
    change.content_url = j.value("content_url", "");
    change.change_type = j.value("change_type", "");
    change.severity = j.value("severity", "MEDIUM");
    change.metadata = j.value("metadata", nlohmann::json::object());

    auto detected_ms = j.value("detected_at", 0LL);
    change.detected_at = std::chrono::system_clock::time_point(
        std::chrono::milliseconds(detected_ms));

    auto published_ms = j.value("published_at", 0LL);
    change.published_at = std::chrono::system_clock::time_point(
        std::chrono::milliseconds(published_ms));

    return change;
}

bool RegulatorySource::should_check() const {
    if (!active) return false;

    if (consecutive_failures >= 5) return false; // Circuit breaker

    auto now = std::chrono::system_clock::now();
    auto time_since_last_check = std::chrono::duration_cast<std::chrono::minutes>(
        now - last_check).count();

    return time_since_last_check >= check_interval_minutes;
}

// ProductionRegulatoryMonitor implementation
ProductionRegulatoryMonitor::ProductionRegulatoryMonitor(
    std::shared_ptr<ConnectionPool> db_pool,
    std::shared_ptr<HttpClient> http_client,
    StructuredLogger* logger
) : db_pool_(db_pool), http_client_(http_client), logger_(logger),
    running_(false), initialized_(false),
    total_checks_(0), successful_checks_(0), failed_checks_(0),
    changes_detected_(0), duplicates_avoided_(0) {
}

ProductionRegulatoryMonitor::~ProductionRegulatoryMonitor() {
    stop_monitoring();
}

bool ProductionRegulatoryMonitor::initialize() {
    if (initialized_) return true;

    // Initialize default regulatory sources
    RegulatorySource sec_source;
    sec_source.id = "sec_edgar";
    sec_source.name = "SEC EDGAR";
    sec_source.base_url = "https://www.sec.gov/rss/news/press.xml";
    sec_source.source_type = "rss";
    sec_source.check_interval_minutes = 15;
    sec_source.scraping_config = {
        {"feed_type", "rss"},
        {"item_selector", "item"},
        {"title_selector", "title"},
        {"link_selector", "link"},
        {"description_selector", "description"},
        {"date_selector", "pubDate"}
    };

    RegulatorySource fca_source;
    fca_source.id = "fca_regulatory";
    fca_source.name = "FCA Regulatory";
    fca_source.base_url = "https://www.fca.org.uk/news";
    fca_source.source_type = "html";
    fca_source.check_interval_minutes = 30;
    fca_source.scraping_config = {
        {"content_selector", ".news-listing"},
        {"title_selector", ".news-title"},
        {"link_selector", ".news-link"},
        {"date_selector", ".news-date"}
    };

    add_source(sec_source);
    add_source(fca_source);

    initialized_ = true;
    logger_->info("Production regulatory monitor initialized", "RegulatoryMonitor", __func__);
    return true;
}

void ProductionRegulatoryMonitor::start_monitoring() {
    if (running_ || !initialized_) return;

    running_ = true;
    monitoring_thread_ = std::thread(&ProductionRegulatoryMonitor::monitoring_loop, this);

    logger_->info("Regulatory monitoring started", "RegulatoryMonitor", __func__);
}

void ProductionRegulatoryMonitor::stop_monitoring() {
    if (!running_) return;

    running_ = false;
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }

    logger_->info("Regulatory monitoring stopped", "RegulatoryMonitor", __func__);
}

bool ProductionRegulatoryMonitor::is_running() const {
    return running_;
}

bool ProductionRegulatoryMonitor::add_source(const RegulatorySource& source) {
    std::lock_guard<std::mutex> lock(sources_mutex_);
    if (sources_.find(source.id) != sources_.end()) {
        return false; // Source already exists
    }

    sources_[source.id] = source;
    logger_->info("Added regulatory source: " + source.name, "RegulatoryMonitor", __func__,
                 {{"source_id", source.id}, {"source_type", source.source_type}});
    return true;
}

bool ProductionRegulatoryMonitor::update_source(const std::string& source_id,
                                              const RegulatorySource& source) {
    std::lock_guard<std::mutex> lock(sources_mutex_);
    if (sources_.find(source_id) == sources_.end()) {
        return false;
    }

    sources_[source_id] = source;
    sources_[source_id].id = source_id; // Ensure ID consistency
    return true;
}

bool ProductionRegulatoryMonitor::remove_source(const std::string& source_id) {
    std::lock_guard<std::mutex> lock(sources_mutex_);
    auto it = sources_.find(source_id);
    if (it == sources_.end()) {
        return false;
    }

    sources_.erase(it);
    logger_->info("Removed regulatory source: " + source_id, "RegulatoryMonitor", __func__);
    return true;
}

std::vector<RegulatorySource> ProductionRegulatoryMonitor::get_sources() const {
    std::lock_guard<std::mutex> lock(sources_mutex_);
    std::vector<RegulatorySource> result;
    for (const auto& pair : sources_) {
        result.push_back(pair.second);
    }
    return result;
}

bool ProductionRegulatoryMonitor::force_check_source(const std::string& source_id) {
    std::lock_guard<std::mutex> lock(sources_mutex_);
    auto it = sources_.find(source_id);
    if (it == sources_.end()) {
        return false;
    }

    // Reset last check time to force immediate checking
    it->second.last_check = std::chrono::system_clock::time_point::min();
    return true;
}

bool ProductionRegulatoryMonitor::store_change(const RegulatoryChange& change) {
    return store_regulatory_change(change);
}

std::vector<RegulatoryChange> ProductionRegulatoryMonitor::get_recent_changes(int limit) const {
    auto conn = db_pool_->get_connection();
    if (!conn) return {};

    std::string query = R"(
        SELECT id, source, title, description, content_url, change_type, severity, metadata,
               detected_at, published_at
        FROM regulatory_changes
        ORDER BY detected_at DESC
        LIMIT $1
    )";

    auto results = conn->execute_query_multi(query, {std::to_string(limit)});
    db_pool_->return_connection(conn);

    std::vector<RegulatoryChange> changes;
    for (const auto& row : results) {
        try {
            changes.push_back(RegulatoryChange::from_json(row));
        } catch (const std::exception& e) {
            logger_->error("Failed to parse regulatory change: " + std::string(e.what()),
                          "RegulatoryMonitor", __func__);
        }
    }

    return changes;
}

nlohmann::json ProductionRegulatoryMonitor::get_monitoring_stats() const {
    return {
        {"running", running_.load()},
        {"initialized", initialized_.load()},
        {"total_checks", total_checks_.load()},
        {"successful_checks", successful_checks_.load()},
        {"failed_checks", failed_checks_.load()},
        {"changes_detected", changes_detected_.load()},
        {"duplicates_avoided", duplicates_avoided_.load()},
        {"active_sources", sources_.size()},
        {"monitoring_interval_seconds", MONITORING_INTERVAL_SECONDS}
    };
}

nlohmann::json ProductionRegulatoryMonitor::get_source_stats(const std::string& source_id) const {
    std::lock_guard<std::mutex> lock(sources_mutex_);
    auto it = sources_.find(source_id);
    if (it == sources_.end()) {
        return nullptr;
    }

    const auto& source = it->second;
    return {
        {"id", source.id},
        {"name", source.name},
        {"active", source.active},
        {"consecutive_failures", source.consecutive_failures},
        {"last_check_timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            source.last_check.time_since_epoch()).count()},
        {"check_interval_minutes", source.check_interval_minutes}
    };
}

void ProductionRegulatoryMonitor::monitoring_loop() {
    logger_->info("Regulatory monitoring loop started", "RegulatoryMonitor", __func__);

    while (running_) {
        try {
            check_all_sources();
        } catch (const std::exception& e) {
            logger_->error("Monitoring loop error: " + std::string(e.what()),
                          "RegulatoryMonitor", __func__);
        }

        // Sleep for monitoring interval
        for (int i = 0; i < MONITORING_INTERVAL_SECONDS && running_; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    logger_->info("Regulatory monitoring loop ended", "RegulatoryMonitor", __func__);
}

void ProductionRegulatoryMonitor::check_all_sources() {
    std::vector<std::string> source_ids;
    {
        std::lock_guard<std::mutex> lock(sources_mutex_);
        for (const auto& pair : sources_) {
            if (pair.second.should_check()) {
                source_ids.push_back(pair.first);
            }
        }
    }

    for (const auto& source_id : source_ids) {
        check_source(source_id);
    }
}

void ProductionRegulatoryMonitor::check_source(const std::string& source_id) {
    RegulatorySource source;
    {
        std::lock_guard<std::mutex> lock(sources_mutex_);
        auto it = sources_.find(source_id);
        if (it == sources_.end()) return;
        source = it->second;
    }

    total_checks_++;

    logger_->info("Checking regulatory source: " + source.name, "RegulatoryMonitor", __func__,
                 {{"source_id", source_id}, {"url", source.base_url}});

    try {
        // Make HTTP request
        HttpResponse response = http_client_->get(source.base_url);

        if (!response.success) {
            failed_checks_++;
            increment_source_failures(source_id);
            logger_->error("HTTP request failed for " + source.name + ": " + response.error_message,
                          "RegulatoryMonitor", __func__, {{"source_id", source_id}});
            return;
        }

        // Process the response data
        if (process_source_data(source_id, response.body, source)) {
            successful_checks_++;
            reset_source_failures(source_id);
            update_source_last_check(source_id);
            logger_->info("Successfully processed " + source.name, "RegulatoryMonitor", __func__,
                         {{"source_id", source_id}, {"data_size", std::to_string(response.body.size())}});
        } else {
            failed_checks_++;
            increment_source_failures(source_id);
            logger_->warn("Failed to process data from " + source.name, "RegulatoryMonitor", __func__,
                         {{"source_id", source_id}});
        }

    } catch (const std::exception& e) {
        failed_checks_++;
        increment_source_failures(source_id);
        logger_->error("Exception checking source " + source.name + ": " + e.what(),
                      "RegulatoryMonitor", __func__, {{"source_id", source_id}});
    }
}

bool ProductionRegulatoryMonitor::process_source_data(const std::string& source_id,
                                                    const std::string& raw_data,
                                                    const RegulatorySource& source) {
    std::vector<RegulatoryChange> changes;

    try {
        if (source.source_type == "rss") {
            if (source_id == "sec_edgar") {
                changes = extract_sec_edgar_changes(raw_data);
            }
        } else if (source.source_type == "html") {
            if (source_id == "fca_regulatory") {
                changes = extract_fca_changes(raw_data);
            }
        }

        // Store changes in database
        for (const auto& change : changes) {
            if (store_regulatory_change(change)) {
                changes_detected_++;
                logger_->info("Stored regulatory change: " + change.title, "RegulatoryMonitor", __func__,
                             {{"change_id", change.id}, {"source", change.source}, {"severity", change.severity}});
            }
        }

        return true;

    } catch (const std::exception& e) {
        logger_->error("Error processing source data: " + std::string(e.what()),
                      "RegulatoryMonitor", __func__, {{"source_id", source_id}});
        return false;
    }
}

std::vector<RegulatoryChange> ProductionRegulatoryMonitor::extract_sec_edgar_changes(
    const std::string& rss_data) {

    std::vector<RegulatoryChange> changes;

    // Parse RSS XML for SEC press releases
    std::regex item_regex(R"(<item>.*?<title>([^<]*)</title>.*?<link>([^<]*)</link>.*?<description>([^<]*)</description>.*?<pubDate>([^<]*)</pubDate>.*?</item>)");

    std::smatch matches;
    std::string::const_iterator search_start(rss_data.cbegin());

    while (std::regex_search(search_start, rss_data.cend(), matches, item_regex)) {
        std::string title = matches[1].str();
        std::string url = matches[2].str();
        std::string description = matches[3].str();
        std::string pub_date = matches[4].str();

        // Clean and filter content
        title.erase(title.begin(), std::find_if(title.begin(), title.end(),
            [](unsigned char ch) { return !std::isspace(ch); }));
        title.erase(std::find_if(title.rbegin(), title.rend(),
            [](unsigned char ch) { return !std::isspace(ch); }).base(), title.end());

        // Filter for regulatory content
        if (title.find("Rule") != std::string::npos ||
            title.find("Release") != std::string::npos ||
            title.find("Statement") != std::string::npos ||
            title.find("Commission") != std::string::npos) {

            RegulatoryChange change;
            change.id = generate_change_id("SEC", title);
            change.source = "SEC";
            change.title = title;
            change.description = description;
            change.content_url = url;
            change.change_type = "regulatory_action";
            change.severity = title.find("Emergency") != std::string::npos ? "CRITICAL" : "HIGH";
            change.detected_at = std::chrono::system_clock::now();

            // Production: Parse actual publication date from RSS feed
            change.published_at = parse_rfc822_date(pub_date);

            if (!is_duplicate_change(change)) {
                changes.push_back(change);
            } else {
                duplicates_avoided_++;
            }
        }

        search_start = matches.suffix().first;
    }

    return changes;
}

std::vector<RegulatoryChange> ProductionRegulatoryMonitor::extract_fca_changes(
    const std::string& html_data) {

    std::vector<RegulatoryChange> changes;

    // Extract FCA news items using regex patterns
    std::regex news_regex("<a[^>]*href=\"([^\"]*news[^\"]*)\"[^>]*>([^<]*(?:Policy|Guidance|Consultation|Statement|Rule)[^<]*)</a>");

    std::smatch matches;
    std::string::const_iterator search_start(html_data.cbegin());

    while (std::regex_search(search_start, html_data.cend(), matches, news_regex)) {
        std::string url = matches[1].str();
        std::string title = matches[2].str();

        // Clean title
        title.erase(title.begin(), std::find_if(title.begin(), title.end(),
            [](unsigned char ch) { return !std::isspace(ch); }));
        title.erase(std::find_if(title.rbegin(), title.rend(),
            [](unsigned char ch) { return !std::isspace(ch); }).base(), title.end());

        RegulatoryChange change;
        change.id = generate_change_id("FCA", title);
        change.source = "FCA";
        change.title = title;
        change.content_url = url.find("http") == 0 ? url : "https://www.fca.org.uk" + url;
        change.change_type = "regulatory_guidance";
        change.severity = "MEDIUM";
        change.detected_at = std::chrono::system_clock::now();
        change.published_at = std::chrono::system_clock::now();

        if (!is_duplicate_change(change)) {
            changes.push_back(change);
        } else {
            duplicates_avoided_++;
        }

        search_start = matches.suffix().first;
    }

    return changes;
}

std::vector<RegulatoryChange> ProductionRegulatoryMonitor::extract_ecb_changes(
    const std::string& /*xml_data*/) {
    // ECB extraction would be implemented similarly
    return {};
}

bool ProductionRegulatoryMonitor::store_regulatory_change(const RegulatoryChange& change) {
    auto conn = db_pool_->get_connection();
    if (!conn) return false;

    std::string query = R"(
        INSERT INTO regulatory_changes (
            id, source, title, description, content_url, change_type, severity, metadata,
            detected_at, published_at
        ) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10)
        ON CONFLICT (id) DO NOTHING
    )";

    std::vector<std::string> params = {
        change.id,
        change.source,
        change.title,
        change.description,
        change.content_url,
        change.change_type,
        change.severity,
        change.metadata.dump(),
        std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            change.detected_at.time_since_epoch()).count()),
        std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            change.published_at.time_since_epoch()).count())
    };

    bool success = conn->execute_command(query, params);
    db_pool_->return_connection(conn);
    return success;
}

bool ProductionRegulatoryMonitor::update_source_last_check(const std::string& source_id) {
    auto conn = db_pool_->get_connection();
    if (!conn) return false;

    std::string query = R"(
        UPDATE regulatory_sources
        SET last_check = NOW(), consecutive_failures = 0
        WHERE id = $1
    )";

    bool success = conn->execute_command(query, {source_id});
    db_pool_->return_connection(conn);

    // Also update in-memory state
    std::lock_guard<std::mutex> lock(sources_mutex_);
    if (sources_.find(source_id) != sources_.end()) {
        sources_[source_id].last_check = std::chrono::system_clock::now();
        sources_[source_id].consecutive_failures = 0;
    }

    return success;
}

bool ProductionRegulatoryMonitor::increment_source_failures(const std::string& source_id) {
    auto conn = db_pool_->get_connection();
    if (!conn) return false;

    std::string query = R"(
        UPDATE regulatory_sources
        SET consecutive_failures = consecutive_failures + 1
        WHERE id = $1
    )";

    bool success = conn->execute_command(query, {source_id});
    db_pool_->return_connection(conn);

    // Update in-memory state
    std::lock_guard<std::mutex> lock(sources_mutex_);
    if (sources_.find(source_id) != sources_.end()) {
        sources_[source_id].consecutive_failures++;
    }

    return success;
}

bool ProductionRegulatoryMonitor::reset_source_failures(const std::string& source_id) {
    auto conn = db_pool_->get_connection();
    if (!conn) return false;

    std::string query = R"(
        UPDATE regulatory_sources
        SET consecutive_failures = 0
        WHERE id = $1
    )";

    bool success = conn->execute_command(query, {source_id});
    db_pool_->return_connection(conn);

    // Update in-memory state
    std::lock_guard<std::mutex> lock(sources_mutex_);
    if (sources_.find(source_id) != sources_.end()) {
        sources_[source_id].consecutive_failures = 0;
    }

    return success;
}

std::string ProductionRegulatoryMonitor::generate_change_id(const std::string& source,
                                                          const std::string& title) {
    std::hash<std::string> hasher;
    std::string combined = source + ":" + title + ":" +
        std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    return source + "_" + std::to_string(hasher(combined));
}

bool ProductionRegulatoryMonitor::is_duplicate_change(const RegulatoryChange& change) {
    auto conn = db_pool_->get_connection();
    if (!conn) return false;

    std::string query = "SELECT COUNT(*) as count FROM regulatory_changes WHERE id = $1";
    auto result = conn->execute_query_single(query, {change.id});
    db_pool_->return_connection(conn);

    if (result && result->contains("count")) {
        return std::stoi(result->at("count").get<std::string>()) > 0;
    }

    return false;
}

// Production-grade RFC 822 date parser for RSS feeds
std::chrono::system_clock::time_point ProductionRegulatoryMonitor::parse_rfc822_date(const std::string& date_str) {
    // RFC 822 format: "Wed, 02 Oct 2002 13:00:00 GMT"
    // Also handles other variants like "Wed, 02 Oct 2002 13:00:00 +0000"

    if (date_str.empty()) {
        return std::chrono::system_clock::now();
    }

    std::tm tm = {};
    std::istringstream ss(date_str);

    // Skip day of week (e.g., "Wed,")
    std::string day_of_week;
    std::getline(ss, day_of_week, ',');

    // Parse the rest
    ss >> std::get_time(&tm, " %d %b %Y %H:%M:%S");

    if (ss.fail()) {
        // Fallback: Try alternative format
        ss.clear();
        ss.str(date_str);
        ss >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S");

        if (ss.fail()) {
            logger_->warn("Failed to parse RFC 822 date: " + date_str, "RegulatoryMonitor", __func__);
            return std::chrono::system_clock::now();
        }
    }

    // Convert to time_point
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

} // namespace regulens
