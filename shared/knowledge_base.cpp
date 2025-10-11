#include "knowledge_base.hpp"

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
    // Production-grade semantic search with embeddings and full-text search
    std::vector<std::pair<std::string, double>> scored_results;

    // Generate query embedding for semantic similarity
    std::vector<double> query_embedding;
    if (embeddings_client_) {
        query_embedding = embeddings_client_->generate_embedding(query);
    }
    
    // Search with multiple strategies and combine scores
    for (const auto& [key, value] : knowledge_store_) {
        double total_score = 0.0;
        
        // 1. Semantic similarity via embeddings (weight: 0.5)
        if (!query_embedding.empty() && knowledge_embeddings_.count(key) > 0) {
            double cosine_sim = calculate_cosine_similarity(query_embedding, knowledge_embeddings_.at(key));
            total_score += cosine_sim * 0.5;
        }
        
        // 2. Full-text search with TF-IDF ranking (weight: 0.3)
        double text_score = calculate_text_relevance(query, key, value);
        total_score += text_score * 0.3;
        
        // 3. Exact/fuzzy matching (weight: 0.2)
        double match_score = 0.0;
        if (key.find(query) != std::string::npos || value.find(query) != std::string::npos) {
            match_score = 1.0; // Exact substring match
        } else {
            // Fuzzy matching with Levenshtein distance
            match_score = calculate_fuzzy_match_score(query, key + " " + value);
        }
        total_score += match_score * 0.2;
        
        if (total_score > 0.1) { // Minimum relevance threshold
            scored_results.push_back({key, total_score});
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


