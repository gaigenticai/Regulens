#pragma once

#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <regex>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include "../shared/network/http_client.hpp"
#include "../shared/logging/structured_logger.hpp"
#include "../shared/config/configuration_manager.hpp"
#include "../shared/models/compliance_event.hpp"
#include "../shared/models/agent_decision.hpp"
#include "../shared/resilience/circuit_breaker.hpp"
#include "../shared/cache/redis_client.hpp"

namespace regulens {

/**
 * @brief Real regulatory data fetcher that connects to actual websites
 *
 * This agent actually connects to SEC EDGAR and FCA websites to fetch
 * real regulatory bulletins, press releases, and compliance updates.
 */
class RealRegulatoryFetcher {
private:
    // HTML parsing helper structures
    struct HtmlLink {
        std::string url;
        std::string title;
    };

    struct HtmlContent {
        std::string title;
        std::string url;
        std::string date;
        std::string summary;
    };

public:
    RealRegulatoryFetcher(std::shared_ptr<HttpClient> http_client,
                         std::shared_ptr<EmailClient> email_client,
                         std::shared_ptr<StructuredLogger> logger);

    ~RealRegulatoryFetcher();

    /**
     * @brief Start fetching regulatory data
     */
    void start_fetching();

    /**
     * @brief Stop fetching
     */
    void stop_fetching();

    /**
     * @brief Check SEC EDGAR for new regulatory actions
     * @return Vector of new regulatory changes
     */
    std::vector<nlohmann::json> fetch_sec_updates();

    /**
     * @brief Check FCA website for new regulatory bulletins
     * @return Vector of new regulatory changes
     */
    std::vector<nlohmann::json> fetch_fca_updates();

    /**
     * @brief Check ECB website for new regulatory announcements
     * @return Vector of new regulatory changes
     */
    std::vector<nlohmann::json> fetch_ecb_updates();

    /**
     * @brief Send notification email about regulatory changes to configured recipients
     * @param changes Vector of regulatory changes
     */
    void send_notification_email(const std::vector<nlohmann::json>& changes);

    /**
     * @brief Get last fetch timestamp
     */
    std::chrono::system_clock::time_point get_last_fetch_time() const;

    /**
     * @brief Get total fetches performed
     */
    size_t get_total_fetches() const;

private:
    /**
     * @brief Main fetching loop
     */
    void fetching_loop();

    /**
     * @brief Parse SEC EDGAR HTML for regulatory actions
     * @param html HTML content from SEC website
     * @return Vector of parsed regulatory actions
     */
    std::vector<nlohmann::json> parse_sec_html(const std::string& html);

    /**
     * @brief Parse FCA HTML for regulatory bulletins
     * @param html HTML content from FCA website
     * @return Vector of parsed regulatory bulletins
     */
    std::vector<nlohmann::json> parse_fca_html(const std::string& html);

    /**
     * @brief Parse ECB HTML for regulatory announcements
     * @param html HTML content from ECB website
     * @return Vector of parsed regulatory announcements
     */
    std::vector<nlohmann::json> parse_ecb_html(const std::string& html);

    /**
     * @brief Check if content is new (not seen before)
     * @param content_hash Hash of content
     * @return true if new
     */
    bool is_new_content(const std::string& content_hash);

    /**
     * @brief Generate production-grade hash for content deduplication
     * @param content Content to hash
     * @return Cryptographically-enhanced hash string
     */
    std::string generate_content_hash(const std::string& content);

    // HTML parsing helper methods
    std::vector<HtmlLink> extract_structured_links(const std::string& html);
    std::vector<HtmlContent> extract_fca_content_blocks(const std::string& html);
    std::vector<HtmlLink> extract_ecb_press_releases(const std::string& html);

    bool is_sec_regulatory_content(const std::string& title, const std::string& url);
    bool is_fca_regulatory_content(const std::string& title);
    bool is_ecb_regulatory_content(const std::string& title);

    std::string normalize_sec_url(const std::string& url);
    std::string normalize_fca_url(const std::string& url);
    std::string normalize_ecb_url(const std::string& url);

    std::string sanitize_html_text(const std::string& text);
    nlohmann::json extract_content_metadata(const std::string& title);

    std::shared_ptr<HttpClient> http_client_;
    std::shared_ptr<EmailClient> email_client_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<ConfigurationManager> config_manager_;
    std::thread fetching_thread_;
    std::atomic<bool> running_;
    std::atomic<size_t> total_fetches_;
    std::chrono::system_clock::time_point last_fetch_time_;
    std::unordered_set<std::string> seen_content_hashes_;
    std::vector<std::string> notification_recipients_;

    // Circuit breakers for regulatory API resilience
    std::shared_ptr<CircuitBreaker> sec_circuit_breaker_;
    std::shared_ptr<CircuitBreaker> fca_circuit_breaker_;
    std::shared_ptr<CircuitBreaker> ecb_circuit_breaker_;

    // Redis client for regulatory data caching
    std::shared_ptr<RedisClient> redis_client_;

    mutable std::mutex content_mutex_;
};

/**
 * @brief Real compliance agent that performs actual analysis
 *
 * This agent connects to real systems, analyzes real regulatory data,
 * and makes actual decisions based on real compliance requirements.
 */
class RealComplianceAgent {
public:
    RealComplianceAgent(std::shared_ptr<HttpClient> http_client,
                       std::shared_ptr<EmailClient> email_client,
                       std::shared_ptr<StructuredLogger> logger);

    /**
     * @brief Process a regulatory change with real analysis
     * @param regulatory_data JSON data about regulatory change
     * @return Agent decision based on real analysis
     */
    AgentDecision process_regulatory_change(const nlohmann::json& regulatory_data);

    /**
     * @brief Perform real risk assessment
     * @param regulatory_data Regulatory data to assess
     * @return Risk assessment with real analysis
     */
    nlohmann::json perform_risk_assessment(const nlohmann::json& regulatory_data);

    /**
     * @brief Generate compliance recommendations
     * @param assessment Risk assessment data
     * @return Vector of compliance recommendations
     */
    std::vector<std::string> generate_compliance_recommendations(const nlohmann::json& assessment);

    /**
     * @brief Send compliance alert to stakeholders
     * @param regulatory_data Regulatory change data
     * @param recommendations Compliance recommendations
     */
    void send_compliance_alert(const nlohmann::json& regulatory_data,
                             const std::vector<std::string>& recommendations);

private:
    /**
     * @brief Analyze regulatory impact using real business logic
     * @param regulatory_data Regulatory change data
     * @return Impact assessment
     */
    std::string analyze_regulatory_impact(const nlohmann::json& regulatory_data);

    /**
     * @brief Calculate compliance deadline
     * @param regulatory_data Regulatory data with dates
     * @return Days until compliance required
     */
    int calculate_compliance_deadline(const nlohmann::json& regulatory_data);

    /**
     * @brief Determine affected business units
     * @param regulatory_data Regulatory change data
     * @return Vector of affected business units
     */
    std::vector<std::string> determine_affected_units(const nlohmann::json& regulatory_data);

    /**
     * @brief Calculate deterministic risk score based on regulatory content
     * @param title Regulatory change title
     * @param source Regulatory source (SEC, FCA, ECB)
     * @param content_type Type of regulatory content
     * @return Risk score between 0.1 and 0.95
     */
    double calculate_deterministic_risk_score(const std::string& title,
                                             const std::string& source,
                                             const std::string& content_type);

    /**
     * @brief Determine risk level from risk score
     * @param risk_score Numerical risk score
     * @return Risk level string (Critical, High, Medium, Low)
     */
    std::string determine_risk_level(double risk_score);

    /**
     * @brief Analyze contributing factors based on regulatory content
     * @param title Regulatory change title
     * @param source Regulatory source
     * @param content_type Type of regulatory content
     * @return Vector of contributing factors
     */
    std::vector<std::string> analyze_contributing_factors(const std::string& title,
                                                         const std::string& source,
                                                         const std::string& content_type);

    /**
     * @brief Determine mitigation strategy based on risk level and factors
     * @param risk_level Risk level (Critical, High, Medium, Low)
     * @param factors Contributing factors
     * @return Mitigation strategy description
     */
    std::string determine_mitigation_strategy(const std::string& risk_level,
                                             const std::vector<std::string>& factors);

    std::shared_ptr<HttpClient> http_client_;
    std::shared_ptr<EmailClient> email_client_;
    std::shared_ptr<StructuredLogger> logger_;
    std::vector<std::string> notification_recipients_;
};

/**
 * @brief Matrix-style real-time activity logger
 *
 * Displays agent activities in a Matrix-themed terminal interface
 * showing real connections, data transfers, and decisions.
 */
class MatrixActivityLogger {
public:
    MatrixActivityLogger();
    ~MatrixActivityLogger();

    /**
     * @brief Log agent connecting to external system
     * @param agent_name Agent name
     * @param target_system Target system (e.g., "SEC EDGAR", "FCA Website")
     */
    void log_connection(const std::string& agent_name, const std::string& target_system);

    /**
     * @brief Log data fetch activity
     * @param agent_name Agent name
     * @param data_type Type of data fetched
     * @param bytes_received Bytes received
     */
    void log_data_fetch(const std::string& agent_name,
                       const std::string& data_type,
                       size_t bytes_received);

    /**
     * @brief Log parsing activity
     * @param agent_name Agent name
     * @param content_type Content being parsed
     * @param items_found Number of items found
     */
    void log_parsing(const std::string& agent_name,
                    const std::string& content_type,
                    size_t items_found);

    /**
     * @brief Log decision-making activity
     * @param agent_name Agent name
     * @param decision_type Type of decision
     * @param confidence Confidence score
     */
    void log_decision(const std::string& agent_name,
                     const std::string& decision_type,
                     double confidence);

    /**
     * @brief Log email sending activity
     * @param recipient Email recipient
     * @param subject Email subject
     * @param success Whether sending succeeded
     */
    void log_email_send(const std::string& recipient,
                       const std::string& subject,
                       bool success);

    /**
     * @brief Log risk assessment activity
     * @param risk_level Calculated risk level
     * @param score Risk score
     */
    void log_risk_assessment(const std::string& risk_level, double score);

    /**
     * @brief Display current activity summary
     */
    void display_activity_summary();

private:
    /**
     * @brief Generate Matrix-style visual effects
     * @param message Message to display
     * @param color_code ANSI color code
     */
    void display_matrix_style(const std::string& message, const std::string& color_code = "32");

    std::atomic<size_t> total_connections_;
    std::atomic<size_t> total_data_fetched_;
    std::atomic<size_t> total_emails_sent_;
    std::atomic<size_t> total_decisions_made_;
    std::chrono::system_clock::time_point start_time_;
};

} // namespace regulens

