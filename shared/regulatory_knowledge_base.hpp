#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "../config/configuration_manager.hpp"
#include "../logging/structured_logger.hpp"
#include "../models/regulatory_change.hpp"

namespace regulens {

/**
 * @brief Regulatory knowledge base for storing and retrieving regulatory intelligence
 */
class RegulatoryKnowledgeBase {
public:
    RegulatoryKnowledgeBase(std::shared_ptr<ConfigurationManager> config,
                           std::shared_ptr<StructuredLogger> logger);

    ~RegulatoryKnowledgeBase();

    /**
     * @brief Initialize the knowledge base
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Shutdown the knowledge base
     */
    void shutdown();

    /**
     * @brief Store a regulatory change
     * @param change The regulatory change to store
     * @return true if stored successfully
     */
    bool store_regulatory_change(const RegulatoryChange& change);

    /**
     * @brief Retrieve a regulatory change by ID
     * @param change_id Change identifier
     * @return Regulatory change or nullopt if not found
     */
    std::optional<RegulatoryChange> get_regulatory_change(const std::string& change_id) const;

    /**
     * @brief Search regulatory changes by criteria
     * @param query Search query
     * @param filters Additional filters
     * @param limit Maximum results to return
     * @return List of matching regulatory changes
     */
    std::vector<RegulatoryChange> search_changes(const std::string& query,
                                                const std::unordered_map<std::string, std::string>& filters = {},
                                                size_t limit = 50) const;

    /**
     * @brief Get regulatory changes by impact level
     * @param impact_level Impact level to filter by
     * @param limit Maximum results to return
     * @return List of regulatory changes
     */
    std::vector<RegulatoryChange> get_changes_by_impact(RegulatoryImpact impact_level,
                                                       size_t limit = 100) const;

    /**
     * @brief Get regulatory changes by business domain
     * @param domain Business domain
     * @param limit Maximum results to return
     * @return List of regulatory changes
     */
    std::vector<RegulatoryChange> get_changes_by_domain(BusinessDomain domain,
                                                       size_t limit = 100) const;

    /**
     * @brief Get regulatory changes by regulatory body
     * @param regulatory_body Regulatory body (SEC, FCA, etc.)
     * @param limit Maximum results to return
     * @return List of regulatory changes
     */
    std::vector<RegulatoryChange> get_changes_by_body(const std::string& regulatory_body,
                                                     size_t limit = 100) const;

    /**
     * @brief Get recent regulatory changes
     * @param days Number of days to look back
     * @param limit Maximum results to return
     * @return List of recent regulatory changes
     */
    std::vector<RegulatoryChange> get_recent_changes(int days = 30,
                                                   size_t limit = 100) const;

    /**
     * @brief Update regulatory change status
     * @param change_id Change identifier
     * @param new_status New status
     * @return true if updated successfully
     */
    bool update_change_status(const std::string& change_id, RegulatoryChangeStatus new_status);

    /**
     * @brief Get knowledge base statistics
     * @return JSON with statistics
     */
    nlohmann::json get_statistics() const;

    /**
     * @brief Export knowledge base to JSON
     * @return JSON representation of all stored changes
     */
    nlohmann::json export_to_json() const;

    /**
     * @brief Import knowledge base from JSON
     * @param json JSON representation to import
     * @return true if import successful
     */
    bool import_from_json(const nlohmann::json& json);

    /**
     * @brief Clear all stored regulatory changes
     */
    void clear();

private:
    /**
     * @brief Index a regulatory change for search
     * @param change The regulatory change to index
     */
    void index_change(const RegulatoryChange& change);

    /**
     * @brief Remove a regulatory change from search indexes
     * @param change The regulatory change to remove
     */
    void remove_from_index(const RegulatoryChange& change);

    /**
     * @brief Create search index entry
     * @param text Text to index
     * @param change_id Associated change ID
     */
    void create_search_index(const std::string& text, const std::string& change_id);

    /**
     * @brief Search text in indexed content
     * @param query Search query
     * @return Set of matching change IDs
     */
    std::unordered_set<std::string> search_index(const std::string& query) const;

    /**
     * @brief Apply filters to search results
     * @param change_ids Set of change IDs to filter
     * @param filters Filter criteria
     * @return Filtered set of change IDs
     */
    std::unordered_set<std::string> apply_filters(
        const std::unordered_set<std::string>& change_ids,
        const std::unordered_map<std::string, std::string>& filters) const;

    /**
     * @brief Persist knowledge base to storage
     * @return true if persistence successful
     */
    bool persist_to_storage();

    /**
     * @brief Load knowledge base from storage
     * @return true if loading successful
     */
    bool load_from_storage();

    // Configuration and dependencies
    std::shared_ptr<ConfigurationManager> config_;
    std::shared_ptr<StructuredLogger> logger_;

    // Storage
    mutable std::mutex storage_mutex_;
    std::unordered_map<std::string, RegulatoryChange> changes_store_;

    // Search indexes
    mutable std::mutex index_mutex_;
    std::unordered_map<std::string, std::unordered_set<std::string>> word_index_; // word -> change_ids
    std::unordered_map<RegulatoryImpact, std::unordered_set<std::string>> impact_index_; // impact -> change_ids
    std::unordered_map<BusinessDomain, std::unordered_set<std::string>> domain_index_; // domain -> change_ids
    std::unordered_map<std::string, std::unordered_set<std::string>> body_index_; // regulatory_body -> change_ids

    // Statistics
    mutable std::mutex stats_mutex_;
    std::atomic<size_t> total_changes_;
    std::atomic<size_t> high_impact_changes_;
    std::atomic<size_t> critical_changes_;
    std::chrono::system_clock::time_point last_update_time_;

    // Configuration
    std::string storage_path_;
    size_t max_changes_in_memory_;
    bool enable_persistence_;
};

} // namespace regulens

