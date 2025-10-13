#include "knowledge_base.hpp"
#include <algorithm>
#include <cmath>

namespace regulens {

KnowledgeBase::KnowledgeBase(std::shared_ptr<ConfigurationManager> config,
                           std::shared_ptr<StructuredLogger> logger)
    : config_(config), logger_(logger) {}

KnowledgeBase::~KnowledgeBase() {
    shutdown();
}

bool KnowledgeBase::initialize() {
    logger_->info("Knowledge base initialized");
    return true;
}

void KnowledgeBase::shutdown() {
    knowledge_store_.clear();
    logger_->info("Knowledge base shutdown");
}

bool KnowledgeBase::store_information(const std::string& key, const std::string& value) {
    knowledge_store_[key] = value;
    return true;
}

std::optional<std::string> KnowledgeBase::retrieve_information(const std::string& key) const {
    auto it = knowledge_store_.find(key);
    if (it != knowledge_store_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<std::string> KnowledgeBase::search_similar(const std::string& query, size_t limit) const {
    // Production-grade search with substring matching and relevance scoring
    std::vector<std::pair<std::string, double>> scored_results;
    
    // Lowercase query for case-insensitive matching
    std::string query_lower = query;
    std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);
    
    // Search with relevance scoring
    for (const auto& [key, value] : knowledge_store_) {
        std::string key_lower = key;
        std::string value_lower = value;
        std::transform(key_lower.begin(), key_lower.end(), key_lower.begin(), ::tolower);
        std::transform(value_lower.begin(), value_lower.end(), value_lower.begin(), ::tolower);
        
        double score = 0.0;
        
        // Exact match in key (highest priority)
        if (key_lower == query_lower) {
            score = 10.0;
        } else if (key_lower.find(query_lower) != std::string::npos) {
            // Substring match in key
            score = 5.0 / (1.0 + std::abs(static_cast<int>(key_lower.length()) - static_cast<int>(query_lower.length())));
        } else if (value_lower.find(query_lower) != std::string::npos) {
            // Substring match in value
            score = 3.0 / (1.0 + std::abs(static_cast<int>(value_lower.length()) - static_cast<int>(query_lower.length())));
        }
        
        if (score > 0.0) {
            scored_results.push_back({key, score});
        }
    }
    
    // Sort by score (descending) and return top results
    std::sort(scored_results.begin(), scored_results.end(),
             [](const auto& a, const auto& b) { return a.second > b.second; });
    
    std::vector<std::string> results;
    for (size_t i = 0; i < std::min(limit, scored_results.size()); ++i) {
        results.push_back(scored_results[i].first);
    }

    return results;
}

} // namespace regulens


