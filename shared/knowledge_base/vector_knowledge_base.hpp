/**
 * Vector Knowledge Base - Advanced Semantic Search and Memory System
 *
 * Production-grade vector database with embeddings for intelligent knowledge retrieval,
 * semantic search, and long-term memory. Integrates with LLM-powered agents for
 * context-aware reasoning and learning.
 *
 * Retrospective Enhancement: Elevates basic knowledge storage to AI-ready semantic memory
 * Forward-Thinking: Foundation for unlimited knowledge expansion and agent intelligence
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <chrono>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"
#include <thread>
#include "../agentic_brain/llm_interface.hpp"
#include <nlohmann/json.hpp>

namespace regulens {

enum class KnowledgeDomain {
    REGULATORY_COMPLIANCE,
    TRANSACTION_MONITORING,
    AUDIT_INTELLIGENCE,
    BUSINESS_PROCESSES,
    RISK_MANAGEMENT,
    LEGAL_FRAMEWORKS,
    FINANCIAL_INSTRUMENTS,
    MARKET_INTELLIGENCE
};

enum class VectorSimilarity {
    COSINE,
    EUCLIDEAN,
    DOT_PRODUCT,
    MANHATTAN
};

enum class KnowledgeType {
    FACT,
    RULE,
    PATTERN,
    RELATIONSHIP,
    CONTEXT,
    EXPERIENCE,
    DECISION,
    PREDICTION
};

enum class MemoryRetention {
    EPHEMERAL,      // Short-term, auto-cleanup
    SESSION,        // Current session only
    PERSISTENT,     // Long-term storage
    ARCHIVAL        // Historical, rarely accessed
};

struct KnowledgeEntity {
    std::string entity_id;
    KnowledgeDomain domain;
    KnowledgeType knowledge_type;
    std::string title;
    std::string content;
    nlohmann::json metadata;
    std::vector<float> embedding;
    MemoryRetention retention_policy;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_accessed;
    std::chrono::system_clock::time_point expires_at;
    int access_count = 0;
    float confidence_score = 1.0f;
    std::vector<std::string> tags;
    std::unordered_map<std::string, nlohmann::json> relationships;
};

struct SemanticQuery {
    std::string query_text;
    std::vector<float> query_embedding;
    KnowledgeDomain domain_filter;
    std::vector<KnowledgeType> type_filters;
    VectorSimilarity similarity_metric = VectorSimilarity::COSINE;
    float similarity_threshold = 0.7f;
    int max_results = 10;
    bool include_metadata = true;
    bool include_relationships = false;
    std::chrono::hours max_age = std::chrono::hours(24 * 365); // 1 year
    std::vector<std::string> tag_filters;
};

struct QueryResult {
    KnowledgeEntity entity;
    float similarity_score;
    std::vector<std::string> matched_terms;
    nlohmann::json explanation;
    std::chrono::microseconds query_time;
};

struct VectorMemoryConfig {
    int embedding_dimensions = 384;  // Default for sentence transformers
    VectorSimilarity default_similarity = VectorSimilarity::COSINE;
    int max_results_per_query = 50;
    std::chrono::hours memory_cleanup_interval = std::chrono::hours(1);
    std::chrono::days ephemeral_retention = std::chrono::days(1);
    std::chrono::days session_retention = std::chrono::days(30);
    std::chrono::days archival_retention = std::chrono::days(365 * 7); // 7 years
    int max_entities_per_domain = 100000;
    bool enable_auto_cleanup = true;
    bool enable_embedding_cache = true;
    std::chrono::seconds embedding_cache_ttl = std::chrono::seconds(3600);
    int batch_indexing_size = 100;
    bool enable_incremental_updates = true;
};

class VectorKnowledgeBase {
public:
    VectorKnowledgeBase(
        std::shared_ptr<ConnectionPool> db_pool,
        std::shared_ptr<LLMInterface> llm_interface,
        StructuredLogger* logger
    );

    // Constructor without LLM interface for basic functionality
    VectorKnowledgeBase(
        std::shared_ptr<ConnectionPool> db_pool,
        StructuredLogger* logger
    );

    ~VectorKnowledgeBase();

    // Lifecycle Management
    bool initialize(const VectorMemoryConfig& config = VectorMemoryConfig{});
    void shutdown();
    bool is_initialized() const;

    // Knowledge Storage
    bool store_entity(const KnowledgeEntity& entity);
    bool store_entities_batch(const std::vector<KnowledgeEntity>& entities);
    bool update_entity(const std::string& entity_id, const nlohmann::json& updates);
    bool delete_entity(const std::string& entity_id);
    bool update_entity_confidence(const std::string& entity_id, float confidence_delta);

    // Semantic Search and Retrieval
    std::vector<QueryResult> semantic_search(const SemanticQuery& query);

    // Helper methods for semantic search implementation
private:
    std::chrono::system_clock::time_point parse_timestamp(const std::string& timestamp_str);
    std::vector<std::string> parse_string_array(const std::string& array_str);
    std::vector<float> parse_vector(const std::string& vector_str);
    std::vector<std::string> find_matching_terms(const std::string& query, const std::string& content);
    nlohmann::json generate_search_explanation(const QueryResult& result, const SemanticQuery& query);
    void update_access_counts(const std::vector<QueryResult>& results);
    std::vector<QueryResult> hybrid_search(const std::string& text_query,
                                         const std::vector<float>& embedding_query,
                                         const SemanticQuery& config);

    // Knowledge Graph Operations
    bool create_relationship(const std::string& source_id,
                           const std::string& target_id,
                           const std::string& relationship_type,
                           const nlohmann::json& properties = nullptr);
    std::vector<KnowledgeEntity> get_related_entities(const std::string& entity_id,
                                                    const std::string& relationship_type = "",
                                                    int max_depth = 2);
    nlohmann::json get_knowledge_graph(const std::string& entity_id, int radius = 3);

    // Memory Management
    bool set_memory_policy(const std::string& entity_id, MemoryRetention policy);
    std::vector<std::string> cleanup_expired_memory(MemoryRetention policy = MemoryRetention::EPHEMERAL);
    nlohmann::json get_memory_statistics() const;

    // Learning and Adaptation
    bool learn_from_interaction(const std::string& query, const std::string& selected_entity_id, float reward = 1.0f);
    std::vector<std::string> get_learning_recommendations(const std::string& domain);
    bool reinforce_learning_patterns(const std::vector<std::string>& entity_ids);

    // Analytics and Insights
    nlohmann::json get_domain_statistics(KnowledgeDomain domain) const;
    std::vector<std::pair<std::string, int>> get_popular_entities(int limit = 20) const;
    std::vector<std::pair<std::string, float>> get_confidence_distribution() const;

    // Agent Integration APIs
    nlohmann::json get_context_for_decision(const std::string& decision_context,
                                          KnowledgeDomain domain,
                                          int max_context_items = 5);
    std::vector<KnowledgeEntity> get_relevant_knowledge(const std::string& agent_query,
                                                      const std::string& agent_type,
                                                      int limit = 10);
    bool update_knowledge_from_feedback(const std::string& entity_id,
                                      const nlohmann::json& feedback);

    // POC-Specific Knowledge APIs
    std::vector<QueryResult> search_transaction_patterns(const std::string& transaction_type,
                                                       const nlohmann::json& risk_indicators);
    std::vector<QueryResult> search_compliance_requirements(const std::string& business_domain,
                                                          const std::string& regulation_type);
    std::vector<QueryResult> search_audit_anomalies(const std::string& system_name,
                                                  const std::chrono::system_clock::time_point& start_time);

    // Administration
    bool rebuild_indexes();
    bool optimize_storage();
    nlohmann::json export_knowledge_base(const std::vector<KnowledgeDomain>& domains = {}) const;
    bool import_knowledge_base(const nlohmann::json& knowledge_data);

private:
    // Vector Operations
    std::vector<float> generate_embedding(const std::string& text);
    float calculate_similarity(const std::vector<float>& vec1,
                            const std::vector<float>& vec2,
                            VectorSimilarity metric) const;
    std::vector<std::pair<std::string, float>> find_similar_vectors(
        const std::vector<float>& query_vector,
        const SemanticQuery& config);

    // Database Operations
    bool create_tables_if_not_exist();
    bool insert_entity(const KnowledgeEntity& entity);
    bool update_entity_embedding(const std::string& entity_id, const std::vector<float>& embedding);
    std::optional<KnowledgeEntity> load_entity(const std::string& entity_id);
    std::vector<KnowledgeEntity> load_entities_batch(const std::vector<std::string>& entity_ids);

    // Indexing and Search
    bool index_entity(const KnowledgeEntity& entity);
    bool remove_from_index(const std::string& entity_id);
    std::vector<std::string> search_by_metadata(const nlohmann::json& filters) const;
    std::vector<std::string> search_by_tags(const std::vector<std::string>& tags) const;
    std::vector<QueryResult> perform_vector_search(const std::vector<float>& query_embedding, const SemanticQuery& config);

    // Memory Management
    void schedule_memory_cleanup();
    void perform_memory_cleanup();
    std::chrono::system_clock::time_point calculate_expiry_time(MemoryRetention policy);

    // Caching
    bool is_embedding_cached(const std::string& text_key) const;
    std::vector<float> get_cached_embedding(const std::string& text_key);
    void cache_embedding(const std::string& text_key, const std::vector<float>& embedding);

    // Relationship Management
    bool store_relationship(const std::string& source_id,
                          const std::string& target_id,
                          const std::string& relationship_type,
                          const nlohmann::json& properties);
    std::vector<std::string> get_related_entity_ids(const std::string& entity_id,
                                                  const std::string& relationship_type,
                                                  int max_depth);

    // Learning Algorithms
    void update_access_patterns(const std::string& entity_id);
    float calculate_relevance_score(const KnowledgeEntity& entity, const SemanticQuery& query) const;
    void update_confidence_scores();

    // Background Processing
    void start_background_tasks();
    void stop_background_tasks();
    void background_cleanup_worker();
    void background_learning_worker();

    // Internal State
    std::shared_ptr<ConnectionPool> db_pool_;
    std::shared_ptr<LLMInterface> llm_interface_;
    StructuredLogger* logger_;

    VectorMemoryConfig config_;
    bool initialized_;

    // In-memory caches and indexes
    mutable std::mutex entity_cache_mutex_;
    std::unordered_map<std::string, KnowledgeEntity> entity_cache_;
    std::unordered_map<std::string, std::vector<float>> embedding_cache_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> embedding_cache_timestamps_;

    // Domain-specific indexes
    std::unordered_map<KnowledgeDomain, std::unordered_set<std::string>> domain_index_;
    std::unordered_map<KnowledgeType, std::unordered_set<std::string>> type_index_;
    std::unordered_map<std::string, std::unordered_set<std::string>> tag_index_;

    // Background processing
    std::thread cleanup_thread_;
    std::thread learning_thread_;
    std::atomic<bool> background_running_;

    // Statistics
    std::atomic<int64_t> total_entities_;
    std::atomic<int64_t> total_searches_;
    std::atomic<int64_t> cache_hits_;
    std::atomic<int64_t> cache_misses_;

    // Utility Methods
    std::string vector_to_string(const std::vector<float>& vec) const;

    // Constants
    const std::string EMBEDDING_MODEL = "sentence-transformers/all-MiniLM-L6-v2";
    const int MAX_EMBEDDING_CACHE_SIZE = 10000;
    const std::chrono::seconds CLEANUP_INTERVAL = std::chrono::seconds(300);
    const std::chrono::seconds LEARNING_INTERVAL = std::chrono::seconds(600);
};

} // namespace regulens
