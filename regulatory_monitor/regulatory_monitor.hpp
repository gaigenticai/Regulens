#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include "../shared/config/configuration_manager.hpp"
#include "../shared/logging/structured_logger.hpp"
#include "../shared/models/compliance_event.hpp"
#include "../shared/models/regulatory_change.hpp"

namespace regulens {

// Forward declarations
class RegulatorySource;
class ChangeDetector;
class DocumentParser;
class RegulatoryKnowledgeBase;

/**
 * @brief Types of regulatory sources
 */
enum class RegulatorySourceType {
    SEC_EDGAR,
    FCA_REGULATORY,
    ECB_ANNOUNCEMENTS,
    CUSTOM_FEED,
    WEB_SCRAPING
};

/**
 * @brief Status of regulatory monitoring
 */
enum class MonitoringStatus {
    INITIALIZING,
    ACTIVE,
    PAUSED,
    ERROR,
    SHUTDOWN
};

/**
 * @brief Regulatory change event
 */
struct RegulatoryChangeEvent {
    std::string source_id;
    std::string document_title;
    std::string change_summary;
    std::chrono::system_clock::time_point detected_at;
    RegulatoryImpact impact_level;
    std::vector<std::string> affected_domains;
    std::string document_url;

    nlohmann::json to_json() const {
        return {
            {"source_id", source_id},
            {"document_title", document_title},
            {"change_summary", change_summary},
            {"detected_at", std::chrono::duration_cast<std::chrono::milliseconds>(
                detected_at.time_since_epoch()).count()},
            {"impact_level", static_cast<int>(impact_level)},
            {"affected_domains", affected_domains},
            {"document_url", document_url}
        };
    }
};

/**
 * @brief Core regulatory monitoring system
 *
 * Continuously monitors regulatory sources for changes and updates
 * the compliance system with new regulatory intelligence.
 */
class RegulatoryMonitor {
public:
    RegulatoryMonitor(std::shared_ptr<ConfigurationManager> config,
                     std::shared_ptr<StructuredLogger> logger,
                     std::shared_ptr<RegulatoryKnowledgeBase> knowledge_base);

    ~RegulatoryMonitor();

    /**
     * @brief Initialize the regulatory monitor
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Start monitoring regulatory sources
     * @return true if monitoring started successfully
     */
    bool start_monitoring();

    /**
     * @brief Stop monitoring
     */
    void stop_monitoring();

    /**
     * @brief Get current monitoring status
     * @return Current status
     */
    MonitoringStatus get_status() const;

    /**
     * @brief Get monitoring statistics
     * @return JSON with monitoring stats
     */
    nlohmann::json get_monitoring_stats() const;

    /**
     * @brief Force check all sources immediately
     * @return true if check completed successfully
     */
    bool force_check_all_sources();

    /**
     * @brief Add a custom regulatory source
     * @param source_config Configuration for the source
     * @return true if source added successfully
     */
    bool add_custom_source(const nlohmann::json& source_config);

    /**
     * @brief Remove a regulatory source
     * @param source_id ID of source to remove
     * @return true if source removed successfully
     */
    bool remove_source(const std::string& source_id);

    /**
     * @brief Get list of active sources
     * @return List of source IDs
     */
    std::vector<std::string> get_active_sources() const;

private:
    /**
     * @brief Main monitoring loop
     */
    void monitoring_loop();

    /**
     * @brief Check a specific regulatory source for changes
     * @param source The source to check
     */
    void check_source(std::shared_ptr<RegulatorySource> source);

    /**
     * @brief Process detected regulatory changes
     * @param changes List of detected changes
     */
    void process_regulatory_changes(const std::vector<RegulatoryChangeEvent>& changes);

    /**
     * @brief Notify agents about regulatory changes
     * @param change The regulatory change to notify about
     */
    void notify_agents(const RegulatoryChangeEvent& change);

    /**
     * @brief Update knowledge base with new regulatory information
     * @param change The regulatory change
     */
    void update_knowledge_base(const RegulatoryChangeEvent& change);

    /**
     * @brief Handle monitoring errors
     * @param source_id ID of source that had error
     * @param error_description Description of the error
     */
    void handle_monitoring_error(const std::string& source_id, const std::string& error_description);

    /**
     * @brief Convert RegulatoryChange objects to RegulatoryChangeEvent objects
     * @param changes List of regulatory changes
     * @return List of regulatory change events
     */
    std::vector<RegulatoryChangeEvent> convert_to_change_events(const std::vector<RegulatoryChange>& changes);

    // Configuration and dependencies
    std::shared_ptr<ConfigurationManager> config_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<RegulatoryKnowledgeBase> knowledge_base_;

    // Monitoring state
    std::atomic<MonitoringStatus> status_;
    std::thread monitoring_thread_;
    std::atomic<bool> should_stop_;

    // Source management
    mutable std::mutex sources_mutex_;
    std::unordered_map<std::string, std::shared_ptr<RegulatorySource>> active_sources_;

    // Change detection
    std::shared_ptr<ChangeDetector> change_detector_;
    std::shared_ptr<DocumentParser> document_parser_;

    // Monitoring statistics
    mutable std::mutex stats_mutex_;
    std::unordered_map<std::string, size_t> sources_checked_;
    std::unordered_map<std::string, size_t> changes_detected_;
    std::unordered_map<std::string, size_t> errors_encountered_;
    std::chrono::system_clock::time_point last_successful_check_;
    std::chrono::milliseconds check_interval_;

    // Callbacks for agent notifications
    std::function<void(const ComplianceEvent&)> event_callback_;
};

} // namespace regulens

