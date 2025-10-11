/**
 * Advanced Conversation Memory System
 *
 * Production-grade persistent memory for compliance agents with semantic indexing,
 * importance scoring, and intelligent retrieval capabilities.
 *
 * Features:
 * - Episodic memory: Specific compliance events and conversations
 * - Semantic indexing: Vector-based similarity search
 * - Importance scoring: Memory prioritization and consolidation
 * - Feedback integration: Learning from human corrections
 * - Memory lifecycle: Creation, consolidation, and forgetting
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <optional>
#include <nlohmann/json.hpp>

#include "../config/configuration_manager.hpp"
#include "../logging/structured_logger.hpp"
#include "../error_handler.hpp"
#include "../llm/embeddings_client.hpp"
#include "../database/postgresql_connection.hpp"

namespace regulens {

/**
 * @brief Memory types for different kinds of stored information
 */
enum class MemoryType {
    EPISODIC,       // Specific events and conversations
    SEMANTIC,       // General knowledge and patterns
    PROCEDURAL,     // Learned processes and workflows
    WORKING         // Temporary working memory
};

/**
 * @brief Importance levels for memory prioritization
 */
enum class ImportanceLevel {
    LOW = 1,        // Routine conversations
    MEDIUM = 5,     // Standard compliance decisions
    HIGH = 8,       // Critical decisions or escalations
    CRITICAL = 10   // Regulatory violations or major events
};

/**
 * @brief Comprehensive memory entry structure
 */
struct MemoryEntry {
    std::string memory_id;
    std::string conversation_id;
    std::string agent_id;
    std::string agent_type;
    MemoryType memory_type;
    ImportanceLevel importance_level;

    std::chrono::system_clock::time_point timestamp;
    std::chrono::system_clock::time_point last_accessed;
    int access_count = 0;

    // Content
    nlohmann::json context;           // Full conversation context
    std::string summary;              // Human-readable summary
    std::vector<std::string> key_topics; // Extracted topics
    std::vector<std::string> compliance_tags; // Regulatory tags

    // Decision and outcome
    std::optional<std::string> decision_made;
    std::optional<std::string> outcome;
    std::optional<double> confidence_score;

    // Learning and feedback
    std::optional<nlohmann::json> human_feedback;
    std::optional<std::string> feedback_type; // "correction", "approval", "escalation"
    std::optional<double> feedback_score; // -1.0 to 1.0

    // Semantic indexing
    std::vector<float> semantic_embedding;
    std::unordered_map<std::string, double> topic_weights;

    // Memory management
    double decay_factor = 1.0;        // For forgetting mechanisms
    bool consolidated = false;        // Whether memory has been consolidated
    std::optional<std::chrono::system_clock::time_point> consolidation_date;

    // Metadata
    std::unordered_map<std::string, std::string> metadata;
    std::optional<std::string> parent_memory_id; // For memory chains

    MemoryEntry() = default;

    MemoryEntry(std::string cid, std::string aid, std::string atype,
                MemoryType mtype, nlohmann::json ctx)
        : conversation_id(std::move(cid)), agent_id(std::move(aid)),
          agent_type(std::move(atype)), memory_type(mtype),
          timestamp(std::chrono::system_clock::now()),
          last_accessed(std::chrono::system_clock::now()), context(std::move(ctx)) {

        memory_id = generate_memory_id();
        importance_level = determine_importance_level();
    }

    // Generate unique memory ID
    static std::string generate_memory_id();

    // Convert to/from JSON for storage
    nlohmann::json to_json() const;
    static MemoryEntry from_json(const nlohmann::json& json);

    // Calculate dynamic importance score
    double calculate_importance_score() const;

    // Update access statistics
    void record_access();

    // Check if memory should be forgotten
    bool should_forget() const;

private:
    ImportanceLevel determine_importance_level() const;
};

/**
 * @brief Semantic similarity result
 */
struct SimilarityResult {
    std::string memory_id;
    double similarity_score;  // 0.0 to 1.0
    std::vector<std::string> matching_topics;
    std::chrono::system_clock::time_point memory_timestamp;

    SimilarityResult(std::string id, double score, std::vector<std::string> topics,
                    std::chrono::system_clock::time_point ts)
        : memory_id(std::move(id)), similarity_score(score),
          matching_topics(std::move(topics)), memory_timestamp(ts) {}
};

/**
 * @brief Memory retrieval query
 */
struct MemoryQuery {
    std::string query_text;
    std::optional<std::string> agent_id;
    std::optional<MemoryType> memory_type;
    std::optional<ImportanceLevel> min_importance;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    std::vector<std::string> required_topics;
    std::vector<std::string> compliance_tags;
    int max_results = 10;
    double min_similarity = 0.3;

    MemoryQuery(std::string query = "")
        : query_text(std::move(query)),
          start_time(std::chrono::system_clock::now() - std::chrono::hours(24)),
          end_time(std::chrono::system_clock::now()) {}
};

/**
 * @brief Conversation memory system with semantic search
 */
class ConversationMemory {
public:
    ConversationMemory(std::shared_ptr<ConfigurationManager> config,
                      std::shared_ptr<EmbeddingsClient> embeddings_client,
                      std::shared_ptr<PostgreSQLConnection> db_connection,
                      StructuredLogger* logger,
                      ErrorHandler* error_handler);

    ~ConversationMemory();

    /**
     * @brief Initialize the memory system
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Store a new memory entry
     * @param entry Memory entry to store
     * @return true if stored successfully
     */
    bool store_memory(const MemoryEntry& entry);

    /**
     * @brief Store conversation with automatic processing
     * @param conversation_id Unique conversation identifier
     * @param agent_id Agent that handled the conversation
     * @param agent_type Type of agent
     * @param context Full conversation context
     * @param decision Decision made (if any)
     * @param outcome Conversation outcome
     * @return true if stored successfully
     */
    bool store_conversation(const std::string& conversation_id,
                           const std::string& agent_id,
                           const std::string& agent_type,
                           const nlohmann::json& context,
                           const std::optional<std::string>& decision = std::nullopt,
                           const std::optional<std::string>& outcome = std::nullopt);

    /**
     * @brief Retrieve memories similar to query
     * @param query Memory retrieval query
     * @return Vector of similarity results
     */
    std::vector<SimilarityResult> retrieve_similar_memories(const MemoryQuery& query);

    /**
     * @brief Retrieve specific memory by ID
     * @param memory_id Memory identifier
     * @return Memory entry if found
     */
    std::optional<MemoryEntry> retrieve_memory(const std::string& memory_id);

    /**
     * @brief Update memory with human feedback
     * @param memory_id Memory to update
     * @param feedback Human feedback content
     * @param feedback_type Type of feedback
     * @param feedback_score Numerical score (-1.0 to 1.0)
     * @return true if updated successfully
     */
    bool update_with_feedback(const std::string& memory_id,
                             const nlohmann::json& feedback,
                             const std::string& feedback_type,
                             double feedback_score);

    /**
     * @brief Search memories by natural language query
     * @param query_text Natural language search query
     * @param max_results Maximum number of results
     * @return Vector of matching memories
     */
    std::vector<MemoryEntry> search_memories(const std::string& query_text, int max_results = 10);

    /**
     * @brief Get memories by agent and time range
     * @param agent_id Agent identifier
     * @param start_time Start of time range
     * @param end_time End of time range
     * @return Vector of memories
     */
    std::vector<MemoryEntry> get_memories_by_agent(
        const std::string& agent_id,
        std::chrono::system_clock::time_point start_time,
        std::chrono::system_clock::time_point end_time);

    /**
     * @brief Consolidate memories - merge similar entries and update importance
     * @param max_age Maximum age for consolidation (older memories are consolidated more aggressively)
     */
    void consolidate_memories(std::chrono::hours max_age = std::chrono::hours(24));

    /**
     * @brief Forget outdated or low-importance memories
     * @param max_age Maximum age to keep memories
     * @param min_importance Minimum importance score to keep
     * @return Number of memories forgotten
     */
    size_t forget_memories(std::chrono::hours max_age = std::chrono::hours(720), // 30 days
                        double min_importance = 0.2);

    /**
     * @brief Get memory statistics
     * @return JSON with memory statistics
     */
    nlohmann::json get_memory_statistics() const;

    /**
     * @brief Export memories for backup or analysis
     * @param agent_id Optional agent filter
     * @param start_time Optional time filter
     * @param end_time Optional time filter
     * @return JSON array of memories
     */
    nlohmann::json export_memories(const std::optional<std::string>& agent_id = std::nullopt,
                                  const std::optional<std::chrono::system_clock::time_point>& start_time = std::nullopt,
                                  const std::optional<std::chrono::system_clock::time_point>& end_time = std::nullopt);

private:
    std::shared_ptr<ConfigurationManager> config_;
    std::shared_ptr<EmbeddingsClient> embeddings_client_;
    std::shared_ptr<PostgreSQLConnection> db_connection_;
    StructuredLogger* logger_;
    ErrorHandler* error_handler_;

    // Memory storage
    std::unordered_map<std::string, MemoryEntry> memory_cache_;
    mutable std::mutex memory_mutex_;

    // Configuration
    size_t max_cache_size_;
    bool enable_persistence_;
    bool enable_embeddings_;
    double forgetting_threshold_;
    std::chrono::hours consolidation_interval_;

    /**
     * @brief Generate semantic embedding for memory content
     * @param entry Memory entry
     * @return Vector embedding
     */
    std::vector<float> generate_embedding(const MemoryEntry& entry);

    /**
     * @brief Calculate similarity between two embeddings
     * @param embedding1 First embedding
     * @param embedding2 Second embedding
     * @return Similarity score (0.0-1.0)
     */
    double calculate_similarity(const std::vector<float>& embedding1,
                               const std::vector<float>& embedding2) const;

    /**
     * @brief Calculate topic-based similarity between query and memory entry
     * @param query Memory query with required topics
     * @param entry Memory entry to compare
     * @return Similarity score (0.0-1.0)
     */
    double calculate_topic_similarity(const MemoryQuery& query, const MemoryEntry& entry) const;

    /**
     * @brief Extract topics and tags from memory content
     * @param entry Memory entry
     * @return Pair of topics and compliance tags
     */
    std::pair<std::vector<std::string>, std::vector<std::string>> extract_topics_and_tags(const MemoryEntry& entry);

    /**
     * @brief Generate summary for memory entry
     * @param entry Memory entry
     * @return Human-readable summary
     */
    std::string generate_summary(const MemoryEntry& entry);

    /**
     * @brief Persist memory to database
     * @param entry Memory entry to persist
     * @return true if persisted successfully
     */
    bool persist_memory(const MemoryEntry& entry);

    /**
     * @brief Load memory from database
     * @param memory_id Memory identifier
     * @return Memory entry if found
     */
    std::optional<MemoryEntry> load_memory(const std::string& memory_id);

    /**
     * @brief Load memories from database by query
     * @param query SQL query
     * @param params Query parameters
     * @return Vector of memory entries
     */
    std::vector<MemoryEntry> load_memories_by_query(const std::string& query,
                                                   const std::vector<std::string>& params = {});

    /**
     * @brief Update memory importance based on access patterns and feedback
     * @param entry Memory entry to update
     */
    void update_memory_importance(MemoryEntry& entry);

    /**
     * @brief Clean up expired cache entries
     */
    void cleanup_cache();

    /**
     * @brief Validate memory entry before storage
     * @param entry Memory entry to validate
     * @return true if valid
     */
    bool validate_memory_entry(const MemoryEntry& entry) const;
    
    /**
     * @brief Calculate confidence score using multi-factor analysis
     * @param context Conversation context
     * @param decision Optional decision made
     * @param outcome Optional outcome
     * @return Confidence score (0.0-1.0)
     */
    double calculate_conversation_confidence(const nlohmann::json& context,
                                            const std::optional<std::string>& decision,
                                            const std::optional<std::string>& outcome) const;
};

/**
 * @brief Create conversation memory instance
 * @param config Configuration manager
 * @param embeddings_client Embeddings client for semantic search
 * @param db_connection Database connection
 * @param logger Structured logger
 * @param error_handler Error handler
 * @return Shared pointer to conversation memory
 */
std::shared_ptr<ConversationMemory> create_conversation_memory(
    std::shared_ptr<ConfigurationManager> config,
    std::shared_ptr<EmbeddingsClient> embeddings_client,
    std::shared_ptr<PostgreSQLConnection> db_connection,
    StructuredLogger* logger,
    ErrorHandler* error_handler);

} // namespace regulens
