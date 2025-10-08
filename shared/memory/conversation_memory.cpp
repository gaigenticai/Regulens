/**
 * Advanced Conversation Memory System Implementation
 *
 * Production-grade persistent memory with semantic indexing, importance scoring,
 * and intelligent retrieval for compliance agent learning.
 */

#include "conversation_memory.hpp"
#include <algorithm>
#include <cmath>
#include <random>
#include <sstream>
#include <iomanip>
#include <pqxx/pqxx>

namespace regulens {

// MemoryEntry Implementation

std::string MemoryEntry::generate_memory_id() {
    static std::atomic<size_t> counter{0};
    auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();

    std::stringstream ss;
    ss << "mem_" << timestamp << "_" << counter++;
    return ss.str();
}

nlohmann::json MemoryEntry::to_json() const {
    nlohmann::json json_entry = {
        {"memory_id", memory_id},
        {"conversation_id", conversation_id},
        {"agent_id", agent_id},
        {"agent_type", agent_type},
        {"memory_type", static_cast<int>(memory_type)},
        {"importance_level", static_cast<int>(importance_level)},
        {"timestamp", std::chrono::system_clock::to_time_t(timestamp)},
        {"last_accessed", std::chrono::system_clock::to_time_t(last_accessed)},
        {"access_count", access_count},
        {"context", context},
        {"summary", summary},
        {"key_topics", key_topics},
        {"compliance_tags", compliance_tags},
        {"decay_factor", decay_factor},
        {"consolidated", consolidated},
        {"metadata", metadata}
    };

    if (decision_made) {
        json_entry["decision_made"] = *decision_made;
    }

    if (outcome) {
        json_entry["outcome"] = *outcome;
    }

    if (confidence_score) {
        json_entry["confidence_score"] = *confidence_score;
    }

    if (human_feedback) {
        json_entry["human_feedback"] = *human_feedback;
    }

    if (feedback_type) {
        json_entry["feedback_type"] = *feedback_type;
    }

    if (feedback_score) {
        json_entry["feedback_score"] = *feedback_score;
    }

    if (!semantic_embedding.empty()) {
        json_entry["semantic_embedding"] = semantic_embedding;
    }

    if (consolidation_date) {
        json_entry["consolidation_date"] = std::chrono::system_clock::to_time_t(*consolidation_date);
    }

    if (parent_memory_id) {
        json_entry["parent_memory_id"] = *parent_memory_id;
    }

    return json_entry;
}

MemoryEntry MemoryEntry::from_json(const nlohmann::json& json) {
    MemoryEntry entry;

    entry.memory_id = json.value("memory_id", "");
    entry.conversation_id = json.value("conversation_id", "");
    entry.agent_id = json.value("agent_id", "");
    entry.agent_type = json.value("agent_type", "");
    entry.memory_type = static_cast<MemoryType>(json.value("memory_type", 0));
    entry.importance_level = static_cast<ImportanceLevel>(json.value("importance_level", 5));
    entry.timestamp = std::chrono::system_clock::from_time_t(json.value("timestamp", 0));
    entry.last_accessed = std::chrono::system_clock::from_time_t(json.value("last_accessed", 0));
    entry.access_count = json.value("access_count", 0);
    entry.context = json.value("context", nlohmann::json::object());
    entry.summary = json.value("summary", "");
    entry.key_topics = json.value("key_topics", std::vector<std::string>{});
    entry.compliance_tags = json.value("compliance_tags", std::vector<std::string>{});
    entry.decay_factor = json.value("decay_factor", 1.0);
    entry.consolidated = json.value("consolidated", false);
    entry.metadata = json.value("metadata", std::unordered_map<std::string, std::string>{});

    if (json.contains("decision_made")) {
        entry.decision_made = json["decision_made"];
    }

    if (json.contains("outcome")) {
        entry.outcome = json["outcome"];
    }

    if (json.contains("confidence_score")) {
        entry.confidence_score = json["confidence_score"];
    }

    if (json.contains("human_feedback")) {
        entry.human_feedback = json["human_feedback"];
    }

    if (json.contains("feedback_type")) {
        entry.feedback_type = json["feedback_type"];
    }

    if (json.contains("feedback_score")) {
        entry.feedback_score = json["feedback_score"];
    }

    if (json.contains("semantic_embedding")) {
        entry.semantic_embedding = json["semantic_embedding"].get<std::vector<float>>();
    }

    if (json.contains("consolidation_date")) {
        entry.consolidation_date = std::chrono::system_clock::from_time_t(json["consolidation_date"]);
    }

    if (json.contains("parent_memory_id")) {
        entry.parent_memory_id = json["parent_memory_id"];
    }

    return entry;
}

double MemoryEntry::calculate_importance_score() const {
    double base_score = static_cast<int>(importance_level) / 10.0;

    // Access frequency bonus
    double access_bonus = std::min(0.3, access_count * 0.01);

    // Feedback bonus/penalty
    double feedback_modifier = 0.0;
    if (feedback_score) {
        feedback_modifier = *feedback_score * 0.2;
    }

    // Recency bonus (newer memories are more important)
    auto now = std::chrono::system_clock::now();
    auto age_hours = std::chrono::duration_cast<std::chrono::hours>(now - timestamp).count();
    double recency_bonus = std::max(0.0, 0.1 * (1.0 - age_hours / 168.0)); // Decay over 1 week

    // Decay factor
    double final_score = (base_score + access_bonus + feedback_modifier + recency_bonus) * decay_factor;

    return std::max(0.0, std::min(1.0, final_score));
}

void MemoryEntry::record_access() {
    last_accessed = std::chrono::system_clock::now();
    access_count++;
}

bool MemoryEntry::should_forget() const {
    auto now = std::chrono::system_clock::now();
    auto age_hours = std::chrono::duration_cast<std::chrono::hours>(now - timestamp).count();

    // Forget very old, low-importance memories
    if (age_hours > 720 && calculate_importance_score() < 0.3) { // 30 days
        return true;
    }

    // Forget memories with very low decay factor
    if (decay_factor < 0.1) {
        return true;
    }

    return false;
}

ImportanceLevel MemoryEntry::determine_importance_level() const {
    // Determine importance based on content analysis
    if (context.contains("escalation") || context.contains("violation") ||
        context.contains("critical") || context.contains("breach")) {
        return ImportanceLevel::CRITICAL;
    }

    if (context.contains("decision") || context.contains("approval") ||
        context.contains("denial") || context.contains("risk")) {
        return ImportanceLevel::HIGH;
    }

    if (context.contains("compliance") || context.contains("regulation") ||
        context.contains("policy")) {
        return ImportanceLevel::MEDIUM;
    }

    return ImportanceLevel::LOW;
}

// ConversationMemory Implementation

ConversationMemory::ConversationMemory(std::shared_ptr<ConfigurationManager> config,
                                     std::shared_ptr<EmbeddingsClient> embeddings_client,
                                     std::shared_ptr<PostgreSQLConnection> db_connection,
                                     StructuredLogger* logger,
                                     ErrorHandler* error_handler)
    : config_(config), embeddings_client_(embeddings_client), db_connection_(db_connection),
      logger_(logger), error_handler_(error_handler) {

    // Load configuration
    max_cache_size_ = config_->get_int("MEMORY_MAX_CACHE_SIZE").value_or(10000);
    enable_persistence_ = config_->get_bool("MEMORY_ENABLE_PERSISTENCE").value_or(true);
    enable_embeddings_ = config_->get_bool("MEMORY_ENABLE_EMBEDDINGS").value_or(true);
    forgetting_threshold_ = config_->get_double("MEMORY_FORGETTING_THRESHOLD").value_or(0.2);
    consolidation_interval_ = std::chrono::hours(config_->get_int("MEMORY_CONSOLIDATION_INTERVAL_HOURS").value_or(24));
}

ConversationMemory::~ConversationMemory() {
    // Ensure all pending operations complete
    std::unique_lock<std::mutex> lock(memory_mutex_);
    memory_cache_.clear();
}

bool ConversationMemory::initialize() {
    if (logger_) {
        logger_->info("Initializing ConversationMemory system", "ConversationMemory", "initialize");
    }

    try {
        // Create memory tables if they don't exist
        if (enable_persistence_ && db_connection_) {
            if (!db_connection_->begin_transaction()) {
                throw std::runtime_error("Failed to begin transaction");
            }

            // Create conversation_memories table
            std::string create_table_sql = R"(
                CREATE TABLE IF NOT EXISTS conversation_memories (
                    memory_id VARCHAR(255) PRIMARY KEY,
                    conversation_id VARCHAR(255) NOT NULL,
                    agent_id VARCHAR(255) NOT NULL,
                    agent_type VARCHAR(100) NOT NULL,
                    memory_type INTEGER NOT NULL,
                    importance_level INTEGER NOT NULL DEFAULT 5,
                    timestamp TIMESTAMP NOT NULL,
                    last_accessed TIMESTAMP NOT NULL,
                    access_count INTEGER DEFAULT 0,
                    context JSONB NOT NULL,
                    summary TEXT,
                    key_topics TEXT[],
                    compliance_tags TEXT[],
                    decision_made TEXT,
                    outcome TEXT,
                    confidence_score DOUBLE PRECISION,
                    human_feedback JSONB,
                    feedback_type VARCHAR(50),
                    feedback_score DOUBLE PRECISION,
                    semantic_embedding VECTOR(384),
                    topic_weights JSONB,
                    decay_factor DOUBLE PRECISION DEFAULT 1.0,
                    consolidated BOOLEAN DEFAULT FALSE,
                    consolidation_date TIMESTAMP,
                    metadata JSONB,
                    parent_memory_id VARCHAR(255),
                    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                )
            )";

            if (!db_connection_->execute_command(create_table_sql)) {
                db_connection_->rollback_transaction();
                throw std::runtime_error("Failed to create conversation_memories table");
            }

            // Create indexes for efficient querying
            std::vector<std::string> index_sqls = {
                "CREATE INDEX IF NOT EXISTS idx_memory_conversation ON conversation_memories(conversation_id)",
                "CREATE INDEX IF NOT EXISTS idx_memory_agent ON conversation_memories(agent_id, agent_type)",
                "CREATE INDEX IF NOT EXISTS idx_memory_timestamp ON conversation_memories(timestamp)",
                "CREATE INDEX IF NOT EXISTS idx_memory_importance ON conversation_memories(importance_level)",
                "CREATE INDEX IF NOT EXISTS idx_memory_type ON conversation_memories(memory_type)"
                // Note: GIN indexes and vector indexes require extensions - add them separately if available
            };

            for (const auto& index_sql : index_sqls) {
                db_connection_->execute_command(index_sql); // Soft fail on index creation
            }

            if (!db_connection_->commit_transaction()) {
                db_connection_->rollback_transaction();
                throw std::runtime_error("Failed to commit transaction");
            }

            if (logger_) {
                logger_->info("Created conversation memory database schema", "ConversationMemory", "initialize");
            }
        }

        if (logger_) {
            logger_->info("ConversationMemory system initialized successfully", "ConversationMemory", "initialize");
        }

        return true;

    } catch (const std::exception& e) {
        if (error_handler_) {
            error_handler_->report_error(ErrorInfo{
                ErrorCategory::DATABASE,
                ErrorSeverity::HIGH,
                "ConversationMemory",
                "initialize",
                "Failed to initialize memory system: " + std::string(e.what()),
                "Memory initialization failure"
            });
        }

        if (logger_) {
            logger_->error("Failed to initialize ConversationMemory: " + std::string(e.what()),
                          "ConversationMemory", "initialize");
        }

        return false;
    }
}

bool ConversationMemory::store_memory(const MemoryEntry& entry) {
    if (!validate_memory_entry(entry)) {
        return false;
    }

    try {
        std::unique_lock<std::mutex> lock(memory_mutex_);

        // Generate embedding if enabled
        MemoryEntry processed_entry = entry;
        if (enable_embeddings_ && embeddings_client_) {
            processed_entry.semantic_embedding = generate_embedding(entry);
        }

        // Extract topics and tags
        auto [topics, tags] = extract_topics_and_tags(entry);
        processed_entry.key_topics = topics;
        processed_entry.compliance_tags = tags;

        // Generate summary
        processed_entry.summary = generate_summary(entry);

        // Store in cache
        memory_cache_[processed_entry.memory_id] = processed_entry;

        // Cleanup cache if needed
        cleanup_cache();

        // Persist to database if enabled
        if (enable_persistence_) {
            persist_memory(processed_entry);
        }

        if (logger_) {
            logger_->info("Stored memory entry: " + processed_entry.memory_id,
                         "ConversationMemory", "store_memory");
        }

        return true;

    } catch (const std::exception& e) {
        if (error_handler_) {
            error_handler_->report_error(ErrorInfo{
                ErrorCategory::PROCESSING,
                ErrorSeverity::HIGH,
                "ConversationMemory",
                "store_memory",
                "Failed to store memory: " + std::string(e.what()),
                "Memory storage failure"
            });
        }

        if (logger_) {
            logger_->error("Failed to store memory: " + std::string(e.what()),
                          "ConversationMemory", "store_memory");
        }

        return false;
    }
}

bool ConversationMemory::store_conversation(const std::string& conversation_id,
                                          const std::string& agent_id,
                                          const std::string& agent_type,
                                          const nlohmann::json& context,
                                          const std::optional<std::string>& decision,
                                          const std::optional<std::string>& outcome) {

    MemoryEntry entry(conversation_id, agent_id, agent_type, MemoryType::EPISODIC, context);

    if (decision) {
        entry.decision_made = decision;
    }

    if (outcome) {
        entry.outcome = outcome;
    }

    // Determine confidence based on context (simplified)
    if (context.contains("confidence")) {
        entry.confidence_score = context["confidence"];
    }

    return store_memory(entry);
}

std::vector<SimilarityResult> ConversationMemory::retrieve_similar_memories(const MemoryQuery& query) {
    std::vector<SimilarityResult> results;

    try {
        std::unique_lock<std::mutex> lock(memory_mutex_);

        // Generate query embedding if we have query text
        std::vector<float> query_embedding;
        if (enable_embeddings_ && embeddings_client_ && !query.query_text.empty()) {
            try {
                // Call the embeddings service to generate embedding for query text
                auto embedding_result = embeddings_client_->generate_single_embedding(query.query_text);
                if (embedding_result) {
                    query_embedding = *embedding_result;
                    if (logger_) {
                        logger_->debug("Generated embedding for query: " + query.query_text.substr(0, 50) + "...",
                                      "ConversationMemory", "retrieve_similar_memories");
                    }
                } else {
                    if (logger_) {
                        logger_->warn("Failed to generate embedding for query, falling back to topic matching",
                                     "ConversationMemory", "retrieve_similar_memories");
                    }
                    query_embedding.clear(); // Will fall back to topic matching
                }
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->error("Exception generating query embedding: " + std::string(e.what()),
                                  "ConversationMemory", "retrieve_similar_memories");
                }
                query_embedding.clear(); // Will fall back to topic matching
            }
        }

        // Search through memories
        for (const auto& [id, entry] : memory_cache_) {
            double similarity_score = 0.0;

            // Check time range
            if (entry.timestamp < query.start_time || entry.timestamp > query.end_time) {
                continue;
            }

            // Check agent filter
            if (query.agent_id && entry.agent_id != *query.agent_id) {
                continue;
            }

            // Check memory type filter
            if (query.memory_type && entry.memory_type != *query.memory_type) {
                continue;
            }

            // Check importance filter
            if (query.min_importance && static_cast<int>(entry.importance_level) < static_cast<int>(*query.min_importance)) {
                continue;
            }

            // Calculate semantic similarity
            if (!query_embedding.empty() && !entry.semantic_embedding.empty()) {
                similarity_score = calculate_similarity(query_embedding, entry.semantic_embedding);
            } else {
                // Fallback to topic matching
                similarity_score = calculate_topic_similarity(query, entry);
            }

            // Check similarity threshold
            if (similarity_score >= query.min_similarity) {
                // Find matching topics
                std::vector<std::string> matching_topics;
                for (const auto& topic : query.required_topics) {
                    if (std::find(entry.key_topics.begin(), entry.key_topics.end(), topic) != entry.key_topics.end()) {
                        matching_topics.push_back(topic);
                    }
                }

                results.emplace_back(entry.memory_id, similarity_score, matching_topics, entry.timestamp);

                // Update access statistics
                const_cast<MemoryEntry&>(entry).record_access();
            }
        }

        // Sort by similarity (highest first)
        std::sort(results.begin(), results.end(),
                 [](const SimilarityResult& a, const SimilarityResult& b) {
                     return a.similarity_score > b.similarity_score;
                 });

        // Limit results
        if (results.size() > static_cast<size_t>(query.max_results)) {
            results.erase(results.begin() + query.max_results, results.end());
        }

        if (logger_) {
            logger_->info("Retrieved " + std::to_string(results.size()) + " similar memories",
                         "ConversationMemory", "retrieve_similar_memories");
        }

    } catch (const std::exception& e) {
        if (error_handler_) {
            error_handler_->report_error(ErrorInfo{
                ErrorCategory::PROCESSING,
                ErrorSeverity::HIGH,
                "ConversationMemory",
                "retrieve_similar_memories",
                "Failed to retrieve similar memories: " + std::string(e.what()),
                "Memory retrieval failure"
            });
        }

        if (logger_) {
            logger_->error("Failed to retrieve similar memories: " + std::string(e.what()),
                          "ConversationMemory", "retrieve_similar_memories");
        }
    }

    return results;
}

std::optional<MemoryEntry> ConversationMemory::retrieve_memory(const std::string& memory_id) {
    std::unique_lock<std::mutex> lock(memory_mutex_);

    // Check cache first
    auto it = memory_cache_.find(memory_id);
    if (it != memory_cache_.end()) {
        MemoryEntry entry = it->second;
        entry.record_access();
        return entry;
    }

    // Load from database if not in cache
    if (enable_persistence_) {
        return load_memory(memory_id);
    }

    return std::nullopt;
}

bool ConversationMemory::update_with_feedback(const std::string& memory_id,
                                            const nlohmann::json& feedback,
                                            const std::string& feedback_type,
                                            double feedback_score) {

    try {
        std::unique_lock<std::mutex> lock(memory_mutex_);

        auto it = memory_cache_.find(memory_id);
        if (it == memory_cache_.end()) {
            // Try to load from database
            auto loaded = load_memory(memory_id);
            if (!loaded) {
                return false;
            }
            memory_cache_[memory_id] = *loaded;
            it = memory_cache_.find(memory_id);
        }

        MemoryEntry& entry = it->second;
        entry.human_feedback = feedback;
        entry.feedback_type = feedback_type;
        entry.feedback_score = feedback_score;

        // Update importance based on feedback
        update_memory_importance(entry);

        // Persist changes
        if (enable_persistence_) {
            persist_memory(entry);
        }

        if (logger_) {
            logger_->info("Updated memory with feedback: " + memory_id,
                         "ConversationMemory", "update_with_feedback");
        }

        return true;

    } catch (const std::exception& e) {
        if (error_handler_) {
            error_handler_->report_error(ErrorInfo{
                ErrorCategory::PROCESSING,
                ErrorSeverity::HIGH,
                "ConversationMemory",
                "update_with_feedback",
                "Failed to update memory with feedback: " + std::string(e.what()),
                "Memory feedback update failure"
            });
        }

        if (logger_) {
            logger_->error("Failed to update memory with feedback: " + std::string(e.what()),
                          "ConversationMemory", "update_with_feedback");
        }

        return false;
    }
}

std::vector<MemoryEntry> ConversationMemory::search_memories(const std::string& query_text, int max_results) {
    MemoryQuery query(query_text);
    query.max_results = max_results;

    auto similarity_results = retrieve_similar_memories(query);
    std::vector<MemoryEntry> memories;

    for (const auto& result : similarity_results) {
        auto memory = retrieve_memory(result.memory_id);
        if (memory) {
            memories.push_back(*memory);
        }
    }

    return memories;
}

std::vector<MemoryEntry> ConversationMemory::get_memories_by_agent(
    const std::string& agent_id,
    std::chrono::system_clock::time_point start_time,
    std::chrono::system_clock::time_point end_time) {

    std::vector<MemoryEntry> results;

    try {
        std::unique_lock<std::mutex> lock(memory_mutex_);

        // Search cache first
        for (const auto& [id, entry] : memory_cache_) {
            if (entry.agent_id == agent_id &&
                entry.timestamp >= start_time &&
                entry.timestamp <= end_time) {
                results.push_back(entry);
            }
        }

        // If persistence is enabled and we need more results, query database
        if (enable_persistence_ && db_connection_) {
            auto db_results = load_memories_by_query(
                "SELECT * FROM conversation_memories WHERE agent_id = $1 AND timestamp >= $2 AND timestamp <= $3 ORDER BY timestamp DESC",
                {agent_id,
                 std::to_string(std::chrono::system_clock::to_time_t(start_time)),
                 std::to_string(std::chrono::system_clock::to_time_t(end_time))}
            );

            // Merge with cache results (avoid duplicates)
            for (const auto& db_entry : db_results) {
                bool exists = false;
                for (const auto& cache_entry : results) {
                    if (cache_entry.memory_id == db_entry.memory_id) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    results.push_back(db_entry);
                }
            }
        }

        // Sort by timestamp (most recent first)
        std::sort(results.begin(), results.end(),
                 [](const MemoryEntry& a, const MemoryEntry& b) {
                     return a.timestamp > b.timestamp;
                 });

    } catch (const std::exception& e) {
        if (error_handler_) {
            error_handler_->report_error(ErrorInfo{
                ErrorCategory::DATABASE,
                ErrorSeverity::HIGH,
                "ConversationMemory",
                "get_memories_by_agent",
                "Failed to get memories by agent: " + std::string(e.what()),
                "Memory query failure"
            });
        }

        if (logger_) {
            logger_->error("Failed to get memories by agent: " + std::string(e.what()),
                          "ConversationMemory", "get_memories_by_agent");
        }
    }

    return results;
}

void ConversationMemory::consolidate_memories(std::chrono::hours max_age) {
    try {
        std::unique_lock<std::mutex> lock(memory_mutex_);

        auto now = std::chrono::system_clock::now();
        int consolidated_count = 0;

        // Consolidate memories based on similarity
        std::vector<std::string> memory_ids;
        for (const auto& [id, entry] : memory_cache_) {
            memory_ids.push_back(id);
        }

        for (const auto& id : memory_ids) {
            auto it = memory_cache_.find(id);
            if (it == memory_cache_.end()) continue;

            MemoryEntry& entry = it->second;

            // Skip recently accessed memories
            auto age = std::chrono::duration_cast<std::chrono::hours>(now - entry.last_accessed);
            if (age < max_age) continue;

            // Mark as consolidated
            entry.consolidated = true;
            entry.consolidation_date = now;

            // Reduce decay factor for consolidated memories
            entry.decay_factor *= 0.9;

            consolidated_count++;
        }

        if (logger_) {
            logger_->info("Consolidated " + std::to_string(consolidated_count) + " memories",
                         "ConversationMemory", "consolidate_memories");
        }

    } catch (const std::exception& e) {
        if (error_handler_) {
            error_handler_->report_error(ErrorInfo{
                ErrorCategory::PROCESSING,
                ErrorSeverity::HIGH,
                "ConversationMemory",
                "consolidate_memories",
                "Failed to consolidate memories: " + std::string(e.what()),
                "Memory consolidation failure"
            });
        }

        if (logger_) {
            logger_->error("Failed to consolidate memories: " + std::string(e.what()),
                          "ConversationMemory", "consolidate_memories");
        }
    }
}

void ConversationMemory::forget_memories(std::chrono::hours max_age, double min_importance) {
    try {
        std::unique_lock<std::mutex> lock(memory_mutex_);

        auto now = std::chrono::system_clock::now();
        std::vector<std::string> to_forget;

        for (const auto& [id, entry] : memory_cache_) {
            auto age = std::chrono::duration_cast<std::chrono::hours>(now - entry.timestamp);

            if (age > max_age && entry.calculate_importance_score() < min_importance) {
                to_forget.push_back(id);
            } else if (entry.should_forget()) {
                to_forget.push_back(id);
            }
        }

        // Remove from cache
        for (const auto& id : to_forget) {
            memory_cache_.erase(id);
        }

        // Remove from database if persistence is enabled
        if (enable_persistence_ && db_connection_ && !to_forget.empty()) {
            if (db_connection_->begin_transaction()) {
                for (const auto& id : to_forget) {
                    db_connection_->execute_command("DELETE FROM conversation_memories WHERE memory_id = $1", {id});
                }
                db_connection_->commit_transaction();
            }
        }

        if (logger_) {
            logger_->info("Forgot " + std::to_string(to_forget.size()) + " memories",
                         "ConversationMemory", "forget_memories");
        }

    } catch (const std::exception& e) {
        if (error_handler_) {
            error_handler_->report_error(ErrorInfo{
                ErrorCategory::DATABASE,
                ErrorSeverity::HIGH,
                "ConversationMemory",
                "forget_memories",
                "Failed to forget memories: " + std::string(e.what()),
                "Memory forgetting failure"
            });
        }

        if (logger_) {
            logger_->error("Failed to forget memories: " + std::string(e.what()),
                          "ConversationMemory", "forget_memories");
        }
    }
}

nlohmann::json ConversationMemory::get_memory_statistics() const {
    std::unique_lock<std::mutex> lock(memory_mutex_);

    nlohmann::json stats = {
        {"cache_size", memory_cache_.size()},
        {"max_cache_size", max_cache_size_},
        {"persistence_enabled", enable_persistence_},
        {"embeddings_enabled", enable_embeddings_},
        {"memory_types", {
            {"episodic", 0},
            {"semantic", 0},
            {"procedural", 0},
            {"working", 0}
        }},
        {"importance_levels", {
            {"low", 0},
            {"medium", 0},
            {"high", 0},
            {"critical", 0}
        }}
    };

    for (const auto& [id, entry] : memory_cache_) {
        // Count by memory type
        std::string type_key;
        switch (entry.memory_type) {
            case MemoryType::EPISODIC: type_key = "episodic"; break;
            case MemoryType::SEMANTIC: type_key = "semantic"; break;
            case MemoryType::PROCEDURAL: type_key = "procedural"; break;
            case MemoryType::WORKING: type_key = "working"; break;
        }
        stats["memory_types"][type_key] = stats["memory_types"][type_key].get<int>() + 1;

        // Count by importance level
        std::string importance_key;
        switch (entry.importance_level) {
            case ImportanceLevel::LOW: importance_key = "low"; break;
            case ImportanceLevel::MEDIUM: importance_key = "medium"; break;
            case ImportanceLevel::HIGH: importance_key = "high"; break;
            case ImportanceLevel::CRITICAL: importance_key = "critical"; break;
        }
        stats["importance_levels"][importance_key] = stats["importance_levels"][importance_key].get<int>() + 1;
    }

    return stats;
}

nlohmann::json ConversationMemory::export_memories(const std::optional<std::string>& agent_id,
                                                  const std::optional<std::chrono::system_clock::time_point>& start_time,
                                                  const std::optional<std::chrono::system_clock::time_point>& end_time) {

    nlohmann::json export_data = nlohmann::json::array();

    try {
        std::unique_lock<std::mutex> lock(memory_mutex_);

        for (const auto& [id, entry] : memory_cache_) {
            // Apply filters
            if (agent_id && entry.agent_id != *agent_id) continue;
            if (start_time && entry.timestamp < *start_time) continue;
            if (end_time && entry.timestamp > *end_time) continue;

            export_data.push_back(entry.to_json());
        }

        if (logger_) {
            logger_->info("Exported " + std::to_string(export_data.size()) + " memories",
                         "ConversationMemory", "export_memories");
        }

    } catch (const std::exception& e) {
        if (error_handler_) {
            error_handler_->report_error(ErrorInfo{
                ErrorCategory::PROCESSING,
                ErrorSeverity::HIGH,
                "ConversationMemory",
                "export_memories",
                "Failed to export memories: " + std::string(e.what()),
                "Memory export failure"
            });
        }

        if (logger_) {
            logger_->error("Failed to export memories: " + std::string(e.what()),
                          "ConversationMemory", "export_memories");
        }
    }

    return export_data;
}

// Private helper methods

std::vector<float> ConversationMemory::generate_embedding(const MemoryEntry& entry) {
    if (!embeddings_client_ || !enable_embeddings_) {
        // Return zero vector if embeddings are disabled or client unavailable
        return std::vector<float>(384, 0.0f);
    }

    try {
        // Create comprehensive text representation of the memory entry for embedding
        std::string memory_text = entry.summary;
        
        // Add context as string
        if (!entry.context.empty()) {
            memory_text += " Context: " + entry.context.dump();
        }

        // Add compliance tags for better semantic understanding
        if (!entry.compliance_tags.empty()) {
            memory_text += " Tags: ";
            for (const auto& tag : entry.compliance_tags) {
                memory_text += tag + " ";
            }
        }

        // Add agent_type (direct field)
        memory_text += " Agent type: " + entry.agent_type;

        // Add importance level
        memory_text += " Importance: " + std::to_string(static_cast<int>(entry.importance_level));

        // Add memory type
        memory_text += " Memory type: " + std::to_string(static_cast<int>(entry.memory_type));

        // Add conversation_id (direct field)
        memory_text += " Conversation: " + entry.conversation_id;

        // Generate embedding using the embeddings client
        auto embedding_result = embeddings_client_->generate_single_embedding(memory_text);
        if (embedding_result) {
            return *embedding_result;
        } else {
            // Fallback to zero vector if embedding generation fails
            if (logger_) {
                logger_->warn("Failed to generate embedding for memory entry: " + entry.memory_id,
                             "ConversationMemory", "generate_embedding");
            }
            return std::vector<float>(384, 0.0f);
        }
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in generate_embedding: " + std::string(e.what()),
                          "ConversationMemory", "generate_embedding");
        }
        return std::vector<float>(384, 0.0f);
    }
}

double ConversationMemory::calculate_similarity(const std::vector<float>& embedding1,
                                              const std::vector<float>& embedding2) const {
    if (embedding1.size() != embedding2.size()) {
        return 0.0;
    }

    // Cosine similarity calculation
    double dot_product = 0.0;
    double norm1 = 0.0;
    double norm2 = 0.0;

    for (size_t i = 0; i < embedding1.size(); ++i) {
        dot_product += embedding1[i] * embedding2[i];
        norm1 += embedding1[i] * embedding1[i];
        norm2 += embedding2[i] * embedding2[i];
    }

    norm1 = std::sqrt(norm1);
    norm2 = std::sqrt(norm2);

    if (norm1 == 0.0 || norm2 == 0.0) {
        return 0.0;
    }

    return dot_product / (norm1 * norm2);
}

double ConversationMemory::calculate_topic_similarity(const MemoryQuery& query, const MemoryEntry& entry) const {
    if (query.required_topics.empty()) {
        return 0.5; // Neutral similarity if no specific topics required
    }

    int matching_topics = 0;
    for (const auto& required_topic : query.required_topics) {
        if (std::find(entry.key_topics.begin(), entry.key_topics.end(), required_topic) != entry.key_topics.end()) {
            matching_topics++;
        }
    }

    return static_cast<double>(matching_topics) / query.required_topics.size();
}

std::pair<std::vector<std::string>, std::vector<std::string>> ConversationMemory::extract_topics_and_tags(const MemoryEntry& entry) {
    std::vector<std::string> topics;
    std::vector<std::string> tags;

    // Simple topic extraction based on keywords
    std::string content_str = entry.context.dump();

    // Compliance topics
    if (content_str.find("KYC") != std::string::npos) topics.push_back("KYC");
    if (content_str.find("AML") != std::string::npos) topics.push_back("AML");
    if (content_str.find("compliance") != std::string::npos) topics.push_back("compliance");
    if (content_str.find("regulation") != std::string::npos) topics.push_back("regulation");
    if (content_str.find("risk") != std::string::npos) topics.push_back("risk");

    // Compliance tags
    if (content_str.find("violation") != std::string::npos) tags.push_back("violation");
    if (content_str.find("breach") != std::string::npos) tags.push_back("breach");
    if (content_str.find("escalation") != std::string::npos) tags.push_back("escalation");
    if (content_str.find("approval") != std::string::npos) tags.push_back("approval");
    if (content_str.find("denial") != std::string::npos) tags.push_back("denial");

    return {topics, tags};
}

std::string ConversationMemory::generate_summary(const MemoryEntry& entry) {
    // Simple summary generation
    std::string summary = "Conversation with " + entry.agent_type + " agent";

    if (entry.decision_made) {
        summary += " - Decision: " + *entry.decision_made;
    }

    if (entry.outcome) {
        summary += " - Outcome: " + *entry.outcome;
    }

    return summary;
}

bool ConversationMemory::persist_memory(const MemoryEntry& entry) {
    if (!db_connection_) return false;

    try {
        // Convert JSON fields
        std::string context_json = entry.context.dump();
        std::string metadata_json = nlohmann::json(entry.metadata).dump();
        std::string topics_json = nlohmann::json(entry.key_topics).dump();
        std::string tags_json = nlohmann::json(entry.compliance_tags).dump();
        std::string human_feedback_json = entry.human_feedback ? entry.human_feedback->dump() : "null";

        std::vector<std::string> params = {
            entry.memory_id, entry.conversation_id, entry.agent_id, entry.agent_type,
            std::to_string(static_cast<int>(entry.memory_type)),
            std::to_string(static_cast<int>(entry.importance_level)),
            std::to_string(std::chrono::system_clock::to_time_t(entry.timestamp)),
            std::to_string(std::chrono::system_clock::to_time_t(entry.last_accessed)),
            std::to_string(entry.access_count), context_json, entry.summary,
            topics_json, tags_json,
            entry.decision_made.value_or(""), entry.outcome.value_or(""),
            std::to_string(entry.confidence_score.value_or(0.0)),
            human_feedback_json,
            entry.feedback_type.value_or(""),
            std::to_string(entry.feedback_score.value_or(0.0)),
            std::to_string(entry.decay_factor),
            entry.consolidated ? "true" : "false",
            metadata_json
        };

        std::string insert_sql = R"(
            INSERT INTO conversation_memories (
                memory_id, conversation_id, agent_id, agent_type, memory_type,
                importance_level, timestamp, last_accessed, access_count,
                context, summary, key_topics, compliance_tags,
                decision_made, outcome, confidence_score,
                human_feedback, feedback_type, feedback_score,
                decay_factor, consolidated, metadata
            ) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17, $18, $19, $20, $21, $22)
            ON CONFLICT (memory_id) DO UPDATE SET
                last_accessed = EXCLUDED.last_accessed,
                access_count = EXCLUDED.access_count,
                human_feedback = EXCLUDED.human_feedback,
                feedback_type = EXCLUDED.feedback_type,
                feedback_score = EXCLUDED.feedback_score,
                decay_factor = EXCLUDED.decay_factor,
                consolidated = EXCLUDED.consolidated,
                metadata = EXCLUDED.metadata,
                updated_at = CURRENT_TIMESTAMP
        )";

        return db_connection_->execute_command(insert_sql, params);

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Failed to persist memory: " + std::string(e.what()),
                          "ConversationMemory", "persist_memory");
        }
        return false;
    }
}

std::optional<MemoryEntry> ConversationMemory::load_memory(const std::string& memory_id) {
    if (!db_connection_) return std::nullopt;

    try {
        std::string query = "SELECT * FROM conversation_memories WHERE memory_id = $1";
        auto result = db_connection_->execute_query(query, {memory_id});

        if (result.rows.empty()) {
            return std::nullopt;
        }

        const auto& row = result.rows[0];
        nlohmann::json context = nlohmann::json::parse(row.at("context"));
        nlohmann::json metadata = nlohmann::json::parse(row.at("metadata"));

        MemoryEntry entry("", "", "", MemoryType::EPISODIC, context);
        entry.memory_id = row.at("memory_id");
        entry.conversation_id = row.at("conversation_id");
        entry.agent_id = row.at("agent_id");
        entry.agent_type = row.at("agent_type");
        entry.memory_type = static_cast<MemoryType>(std::stoi(row.at("memory_type")));
        entry.importance_level = static_cast<ImportanceLevel>(std::stoi(row.at("importance_level")));
        entry.timestamp = std::chrono::system_clock::from_time_t(std::stoll(row.at("timestamp")));
        entry.last_accessed = std::chrono::system_clock::from_time_t(std::stoll(row.at("last_accessed")));
        entry.access_count = std::stoi(row.at("access_count"));
        entry.summary = row.at("summary");
        entry.key_topics = nlohmann::json::parse(row.at("key_topics"));
        entry.compliance_tags = nlohmann::json::parse(row.at("compliance_tags"));
        entry.decay_factor = std::stod(row.at("decay_factor"));
        entry.consolidated = row.at("consolidated") == "t" || row.at("consolidated") == "true";
        entry.metadata = metadata;

        if (!row.at("decision_made").empty()) {
            entry.decision_made = row.at("decision_made");
        }

        if (!row.at("outcome").empty()) {
            entry.outcome = row.at("outcome");
        }

        if (!row.at("confidence_score").empty()) {
            entry.confidence_score = std::stod(row.at("confidence_score"));
        }

        if (!row.at("human_feedback").empty() && row.at("human_feedback") != "null") {
            entry.human_feedback = nlohmann::json::parse(row.at("human_feedback"));
        }

        if (!row.at("feedback_type").empty()) {
            entry.feedback_type = row.at("feedback_type");
        }

        if (!row.at("feedback_score").empty()) {
            entry.feedback_score = std::stod(row.at("feedback_score"));
        }

        return entry;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Failed to load memory: " + std::string(e.what()),
                          "ConversationMemory", "load_memory");
        }
        return std::nullopt;
    }
}

std::vector<MemoryEntry> ConversationMemory::load_memories_by_query(const std::string& query,
                                                                   const std::vector<std::string>& params) {
    std::vector<MemoryEntry> memories;

    if (!db_connection_) return memories;

    try {
        auto result = db_connection_->execute_query(query, params);

        for (const auto& row : result.rows) {
            // Similar parsing as in load_memory
            nlohmann::json context = nlohmann::json::parse(row.at("context"));
            MemoryEntry entry("", "", "", MemoryType::EPISODIC, context);
            // Basic fields - similar to load_memory but simplified for performance
            entry.memory_id = row.at("memory_id");
            entry.conversation_id = row.at("conversation_id");
            entry.agent_id = row.at("agent_id");
            entry.agent_type = row.at("agent_type");
            memories.push_back(entry);
        }

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Failed to load memories by query: " + std::string(e.what()),
                          "ConversationMemory", "load_memories_by_query");
        }
    }

    return memories;
}

void ConversationMemory::update_memory_importance(MemoryEntry& entry) {
    // Update importance based on feedback
    if (entry.feedback_score) {
        double feedback = *entry.feedback_score;
        if (feedback > 0.5) {
            // Positive feedback increases importance
            if (entry.importance_level != ImportanceLevel::CRITICAL) {
                entry.importance_level = static_cast<ImportanceLevel>(
                    std::min(10, static_cast<int>(entry.importance_level) + 1));
            }
        } else if (feedback < -0.5) {
            // Negative feedback might decrease importance or mark for review
            entry.decay_factor *= 0.8;
        }
    }
}

void ConversationMemory::cleanup_cache() {
    if (memory_cache_.size() <= max_cache_size_) {
        return;
    }

    // Remove least recently accessed entries
    std::vector<std::pair<std::string, std::chrono::system_clock::time_point>> entries;

    for (const auto& [id, entry] : memory_cache_) {
        entries.emplace_back(id, entry.last_accessed);
    }

    std::sort(entries.begin(), entries.end(),
             [](const auto& a, const auto& b) { return a.second < b.second; });

    size_t to_remove = memory_cache_.size() - max_cache_size_;
    for (size_t i = 0; i < to_remove && i < entries.size(); ++i) {
        memory_cache_.erase(entries[i].first);
    }
}

bool ConversationMemory::validate_memory_entry(const MemoryEntry& entry) const {
    if (entry.memory_id.empty()) return false;
    if (entry.conversation_id.empty()) return false;
    if (entry.agent_id.empty()) return false;
    if (entry.context.is_null()) return false;

    return true;
}

// Factory function

std::shared_ptr<ConversationMemory> create_conversation_memory(
    std::shared_ptr<ConfigurationManager> config,
    std::shared_ptr<EmbeddingsClient> embeddings_client,
    std::shared_ptr<PostgreSQLConnection> db_connection,
    StructuredLogger* logger,
    ErrorHandler* error_handler) {

    auto memory = std::make_shared<ConversationMemory>(
        config, embeddings_client, db_connection, logger, error_handler);

    if (!memory->initialize()) {
        return nullptr;
    }

    return memory;
}

} // namespace regulens
