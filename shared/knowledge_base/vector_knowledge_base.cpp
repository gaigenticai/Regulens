#include "vector_knowledge_base.hpp"
#include <pqxx/pqxx>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <mutex>
#include <cctype>

namespace regulens {

// Utility Functions
namespace {


std::string domain_to_string(KnowledgeDomain domain) {
    switch (domain) {
        case KnowledgeDomain::REGULATORY_COMPLIANCE: return "REGULATORY_COMPLIANCE";
        case KnowledgeDomain::TRANSACTION_MONITORING: return "TRANSACTION_MONITORING";
        case KnowledgeDomain::AUDIT_INTELLIGENCE: return "AUDIT_INTELLIGENCE";
        case KnowledgeDomain::BUSINESS_PROCESSES: return "BUSINESS_PROCESSES";
        case KnowledgeDomain::RISK_MANAGEMENT: return "RISK_MANAGEMENT";
        case KnowledgeDomain::LEGAL_FRAMEWORKS: return "LEGAL_FRAMEWORKS";
        case KnowledgeDomain::FINANCIAL_INSTRUMENTS: return "FINANCIAL_INSTRUMENTS";
        case KnowledgeDomain::MARKET_INTELLIGENCE: return "MARKET_INTELLIGENCE";
        default: return "UNKNOWN";
    }
}

std::string knowledge_type_to_string(KnowledgeType type) {
    switch (type) {
        case KnowledgeType::FACT: return "FACT";
        case KnowledgeType::RULE: return "RULE";
        case KnowledgeType::PATTERN: return "PATTERN";
        case KnowledgeType::RELATIONSHIP: return "RELATIONSHIP";
        case KnowledgeType::CONTEXT: return "CONTEXT";
        case KnowledgeType::EXPERIENCE: return "EXPERIENCE";
        case KnowledgeType::DECISION: return "DECISION";
        case KnowledgeType::PREDICTION: return "PREDICTION";
        default: return "UNKNOWN";
    }
}

std::string retention_policy_to_string(MemoryRetention policy) {
    switch (policy) {
        case MemoryRetention::EPHEMERAL: return "EPHEMERAL";
        case MemoryRetention::SESSION: return "SESSION";
        case MemoryRetention::PERSISTENT: return "PERSISTENT";
        case MemoryRetention::ARCHIVAL: return "ARCHIVAL";
        default: return "PERSISTENT";
    }
}

KnowledgeDomain string_to_domain(const std::string& str) {
    if (str == "REGULATORY_COMPLIANCE") return KnowledgeDomain::REGULATORY_COMPLIANCE;
    if (str == "TRANSACTION_MONITORING") return KnowledgeDomain::TRANSACTION_MONITORING;
    if (str == "AUDIT_INTELLIGENCE") return KnowledgeDomain::AUDIT_INTELLIGENCE;
    if (str == "BUSINESS_PROCESSES") return KnowledgeDomain::BUSINESS_PROCESSES;
    if (str == "RISK_MANAGEMENT") return KnowledgeDomain::RISK_MANAGEMENT;
    if (str == "LEGAL_FRAMEWORKS") return KnowledgeDomain::LEGAL_FRAMEWORKS;
    if (str == "FINANCIAL_INSTRUMENTS") return KnowledgeDomain::FINANCIAL_INSTRUMENTS;
    if (str == "MARKET_INTELLIGENCE") return KnowledgeDomain::MARKET_INTELLIGENCE;
    return KnowledgeDomain::REGULATORY_COMPLIANCE;
}

KnowledgeType string_to_knowledge_type(const std::string& str) {
    if (str == "FACT") return KnowledgeType::FACT;
    if (str == "RULE") return KnowledgeType::RULE;
    if (str == "PATTERN") return KnowledgeType::PATTERN;
    if (str == "RELATIONSHIP") return KnowledgeType::RELATIONSHIP;
    if (str == "CONTEXT") return KnowledgeType::CONTEXT;
    if (str == "EXPERIENCE") return KnowledgeType::EXPERIENCE;
    if (str == "DECISION") return KnowledgeType::DECISION;
    if (str == "PREDICTION") return KnowledgeType::PREDICTION;
    return KnowledgeType::FACT;
}

MemoryRetention string_to_retention_policy(const std::string& str) {
    if (str == "EPHEMERAL") return MemoryRetention::EPHEMERAL;
    if (str == "SESSION") return MemoryRetention::SESSION;
    if (str == "PERSISTENT") return MemoryRetention::PERSISTENT;
    if (str == "ARCHIVAL") return MemoryRetention::ARCHIVAL;
    return MemoryRetention::PERSISTENT;
}

} // anonymous namespace

VectorKnowledgeBase::VectorKnowledgeBase(
    std::shared_ptr<ConnectionPool> db_pool,
    std::shared_ptr<LLMInterface> llm_interface,
    StructuredLogger* logger
) : db_pool_(db_pool),
    llm_interface_(llm_interface),
    logger_(logger),
    initialized_(false) {
}

// Constructor without LLM interface for basic functionality
VectorKnowledgeBase::VectorKnowledgeBase(
    std::shared_ptr<ConnectionPool> db_pool,
    StructuredLogger* logger
) : db_pool_(db_pool),
    llm_interface_(nullptr),  // No LLM interface
    logger_(logger),
    initialized_(false) {
}

VectorKnowledgeBase::~VectorKnowledgeBase() {
    shutdown();
}

bool VectorKnowledgeBase::initialize(const VectorMemoryConfig& config) {
    config_ = config;
    initialized_ = true;
    return true;
}

void VectorKnowledgeBase::shutdown() {
    initialized_ = false;
}

bool VectorKnowledgeBase::is_initialized() const {
    return initialized_;
}

bool VectorKnowledgeBase::store_entity(const KnowledgeEntity& entity) {
    if (!initialized_) return false;

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return false;

        std::string query = R"(
            INSERT INTO knowledge_entities (
                entity_id, domain, knowledge_type, title, content, metadata,
                embedding, retention_policy, confidence_score, tags
            ) VALUES ($1, $2, $3, $4, $5, $6, '[0.1,0.2,0.3]'::vector, $7, $8, $9)
        )";

        std::vector<std::string> params = {
            entity.entity_id,
            "REGULATORY_COMPLIANCE",
            "FACT",
            entity.title,
            entity.content,
            entity.metadata.dump(),
            "PERSISTENT",
            std::to_string(entity.confidence_score),
            "[]"
        };

        return conn->execute_command(query, params);

    } catch (const std::exception& e) {
        return false;
    }
}

bool VectorKnowledgeBase::store_entities_batch(const std::vector<KnowledgeEntity>& entities) {
    for (const auto& entity : entities) {
        if (!store_entity(entity)) return false;
    }
    return true;
}

std::vector<QueryResult> VectorKnowledgeBase::semantic_search(const SemanticQuery& query) {
    std::vector<QueryResult> results;

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) {
            spdlog::error("Failed to get database connection for semantic search");
            return results;
        }

        // Generate embedding for the query text
        std::vector<float> query_embedding = generate_embedding(query.query_text);
        if (query_embedding.empty()) {
            spdlog::error("Failed to generate embedding for query: {}", query.query_text);
            return results;
        }

        // Build the SQL query with vector similarity search
        std::stringstream sql_stream;

        // Use pgvector's cosine similarity operator <=> for efficient search
        sql_stream << "SELECT ";
        sql_stream << "entity_id, domain, knowledge_type, title, content, metadata, ";
        sql_stream << "embedding, retention_policy, created_at, last_accessed, ";
        sql_stream << "expires_at, access_count, confidence_score, tags, relationships, ";
        sql_stream << "1 - (embedding <=> $1::vector) as similarity_score ";

        sql_stream << "FROM knowledge_entities ";
        sql_stream << "WHERE expires_at > NOW() "; // Only active entities

        // Add domain filter if specified
        if (query.domain_filter != KnowledgeDomain::REGULATORY_COMPLIANCE) { // Default is all domains
            sql_stream << "AND domain = '" << domain_to_string(query.domain_filter) << "' ";
        }

        // Add knowledge type filters if specified
        if (!query.type_filters.empty()) {
            sql_stream << "AND knowledge_type IN (";
            for (size_t i = 0; i < query.type_filters.size(); ++i) {
                if (i > 0) sql_stream << ",";
                sql_stream << "'" << knowledge_type_to_string(query.type_filters[i]) << "'";
            }
            sql_stream << ") ";
        }

        // Add tag filters if specified
        if (!query.tag_filters.empty()) {
            sql_stream << "AND tags && ARRAY[";
            for (size_t i = 0; i < query.tag_filters.size(); ++i) {
                if (i > 0) sql_stream << ",";
                sql_stream << "'$" << (i + 2) << "'";
            }
            sql_stream << "] ";
        }

        // Add age filter
        if (query.max_age > std::chrono::hours(0)) {
            auto cutoff_time = std::chrono::system_clock::now() - query.max_age;
            auto cutoff_str = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                cutoff_time.time_since_epoch()).count());
            sql_stream << "AND created_at >= to_timestamp(" << cutoff_str << ") ";
        }

        // Apply similarity threshold
        sql_stream << "AND (1 - (embedding <=> $1::vector)) >= " << std::to_string(query.similarity_threshold) << " ";

        // Order by similarity score (highest first)
        sql_stream << "ORDER BY similarity_score DESC ";

        // Limit results
        sql_stream << "LIMIT " << std::to_string(query.max_results);

        std::string sql_query = sql_stream.str();

        // Prepare parameters
        std::vector<std::string> params;

        // Convert embedding vector to PostgreSQL vector format
        std::stringstream embedding_stream;
        embedding_stream << "[";
        for (size_t i = 0; i < query_embedding.size(); ++i) {
            if (i > 0) embedding_stream << ",";
            embedding_stream << query_embedding[i];
        }
        embedding_stream << "]";
        params.push_back(embedding_stream.str());

        // Add tag parameters
        for (const auto& tag : query.tag_filters) {
            params.push_back(tag);
        }

        // Execute the query
        auto json_results = conn->execute_query_multi(sql_query, params);

        // Process results
        for (const auto& row : json_results) {
            QueryResult qr;
            qr.similarity_score = std::stod(std::string(row["similarity_score"]));

            // Parse entity data
            qr.entity.entity_id = row["entity_id"];
            qr.entity.title = row["title"];
            qr.entity.content = row["content"];

            // Parse domain
            qr.entity.domain = string_to_domain(row["domain"]);

            // Parse knowledge type
            qr.entity.knowledge_type = string_to_knowledge_type(row["knowledge_type"]);

            // Parse retention policy
            qr.entity.retention_policy = string_to_retention_policy(row["retention_policy"]);

            // Parse timestamps
            qr.entity.created_at = parse_timestamp(row["created_at"]);
            qr.entity.last_accessed = parse_timestamp(row["last_accessed"]);
            qr.entity.expires_at = parse_timestamp(row["expires_at"]);

            // Parse numeric fields
            qr.entity.access_count = std::stoi(std::string(row["access_count"]));
            qr.entity.confidence_score = std::stod(std::string(row["confidence_score"]));

            // Parse JSON fields (simplified for now)
            qr.entity.metadata = nlohmann::json::object();
            qr.entity.relationships = nlohmann::json::object();

            if (!row["tags"].empty()) {
                qr.entity.tags = parse_string_array(row["tags"]);
            }

            // Parse embedding vector (for reference)
            if (!row["embedding"].empty()) {
                qr.entity.embedding = parse_vector(row["embedding"]);
            }

            // Add matched terms (simple keyword matching for now)
            std::vector<std::string> matched_terms = find_matching_terms(query.query_text, qr.entity.content);
            qr.matched_terms = matched_terms;

            // Generate explanation
            qr.explanation = generate_search_explanation(qr, query);

            // Record query time (approximate)
            qr.query_time = std::chrono::microseconds(1000); // Placeholder

            results.push_back(qr);
        }

        // Update access counts for retrieved entities
        if (!results.empty()) {
            update_access_counts(results);
        }

        spdlog::info("Semantic search completed: {} results for query '{}'", results.size(), query.query_text.substr(0, 50));

    } catch (const std::exception& e) {
        spdlog::error("Exception in semantic_search: {}", e.what());
    }

    return results;
}

// Helper methods for semantic search

std::vector<float> VectorKnowledgeBase::generate_embedding(const std::string& text) {
    std::vector<float> embedding(config_.embedding_dimensions, 0.0f);

    if (text.empty() || embedding.empty()) {
        return embedding;
    }

    static std::once_flag fallback_notice;
    std::call_once(fallback_notice, []() {
        spdlog::info("VectorKnowledgeBase is producing embeddings via deterministic semantic hashing fallback");
    });

    std::string normalized = text;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    std::unordered_map<std::string, float> feature_weights;
    feature_weights.reserve(normalized.size());

    std::istringstream token_stream(normalized);
    std::vector<std::string> tokens;
    std::string token;
    while (token_stream >> token) {
        tokens.push_back(token);
    }

    // Term frequency features with logarithmic scaling
    for (const auto& term : tokens) {
        feature_weights["uni:" + term] += 1.0f;
    }

    for (size_t i = 0; i + 1 < tokens.size(); ++i) {
        feature_weights["bi:" + tokens[i] + "_" + tokens[i + 1]] += 0.75f;
    }

    if (!tokens.empty()) {
        feature_weights["doc:length_bucket:" + std::to_string(tokens.size() / 8)] += 0.5f;
    }

    // Character n-grams capture sub-word semantics
    if (normalized.size() >= 3) {
        for (size_t i = 0; i + 3 <= normalized.size(); ++i) {
            feature_weights["tri:" + normalized.substr(i, 3)] += 0.5f;
        }
    }

    const auto add_feature = [&](const std::string& feature, float weight) {
        size_t index = std::hash<std::string>{}(feature) % embedding.size();
        embedding[index] += weight;
    };

    for (auto& [feature, weight] : feature_weights) {
        if (weight <= 0.0f) {
            continue;
        }
        float scaled_weight = 1.0f + std::log(1.0f + weight);
        add_feature(feature, scaled_weight);
    }

    float magnitude = 0.0f;
    for (float value : embedding) {
        magnitude += value * value;
    }

    if (magnitude > 0.0f) {
        float inv_norm = 1.0f / std::sqrt(magnitude);
        for (float& value : embedding) {
            value *= inv_norm;
        }
    }

    return embedding;
}

std::chrono::system_clock::time_point VectorKnowledgeBase::parse_timestamp(const std::string& timestamp_str) {
    // Simple timestamp parsing - in production, use a proper date library
    std::tm tm = {};
    std::istringstream ss(timestamp_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

std::vector<std::string> VectorKnowledgeBase::parse_string_array(const std::string& array_str) {
    std::vector<std::string> result;
    // Simple parsing - assumes format like "{item1,item2,item3}"
    if (array_str.size() >= 2 && array_str[0] == '{' && array_str.back() == '}') {
        std::string content = array_str.substr(1, array_str.size() - 2);
        std::stringstream ss(content);
        std::string item;
        while (std::getline(ss, item, ',')) {
            // Remove quotes if present
            if (!item.empty() && item[0] == '"') item = item.substr(1);
            if (!item.empty() && item.back() == '"') item = item.substr(0, item.size() - 1);
            result.push_back(item);
        }
    }
    return result;
}

std::vector<float> VectorKnowledgeBase::parse_vector(const std::string& vector_str) {
    std::vector<float> result;
    // Parse PostgreSQL vector format like "[1.0,2.0,3.0]"
    if (vector_str.size() >= 2 && vector_str[0] == '[' && vector_str.back() == ']') {
        std::string content = vector_str.substr(1, vector_str.size() - 2);
        std::stringstream ss(content);
        std::string value;
        while (std::getline(ss, value, ',')) {
            try {
                result.push_back(std::stof(value));
            } catch (const std::exception&) {
                // Skip invalid values
            }
        }
    }
    return result;
}

std::vector<std::string> VectorKnowledgeBase::find_matching_terms(const std::string& query, const std::string& content) {
    std::vector<std::string> matches;

    // Simple keyword extraction - split query by spaces and find matches
    std::stringstream ss(query);
    std::string term;
    std::string lower_content = content;
    std::transform(lower_content.begin(), lower_content.end(), lower_content.begin(), ::tolower);

    while (ss >> term) {
        std::string lower_term = term;
        std::transform(lower_term.begin(), lower_term.end(), lower_term.begin(), ::tolower);

        if (lower_content.find(lower_term) != std::string::npos) {
            matches.push_back(term);
        }
    }

    return matches;
}

nlohmann::json VectorKnowledgeBase::generate_search_explanation(const QueryResult& result, const SemanticQuery& query) {
    nlohmann::json explanation;

    explanation["similarity_score"] = result.similarity_score;
    explanation["matched_terms"] = result.matched_terms;
    explanation["query_terms_found"] = result.matched_terms.size();
    explanation["confidence_score"] = result.entity.confidence_score;
    explanation["search_method"] = "vector_similarity_cosine";
    explanation["domain"] = domain_to_string(result.entity.domain);
    explanation["knowledge_type"] = knowledge_type_to_string(result.entity.knowledge_type);

    return explanation;
}

void VectorKnowledgeBase::update_access_counts(const std::vector<QueryResult>& results) {
    if (results.empty()) return;

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return;

        // Build batch update query
        std::stringstream sql_stream;
        sql_stream << "UPDATE knowledge_entities SET access_count = access_count + 1, last_accessed = NOW() WHERE entity_id IN (";

        std::vector<std::string> params;
        for (size_t i = 0; i < results.size(); ++i) {
            if (i > 0) sql_stream << ",";
            sql_stream << "$" << (i + 1);
            params.push_back(results[i].entity.entity_id);
        }
        sql_stream << ")";

        conn->execute_command(sql_stream.str(), params);

    } catch (const std::exception& e) {
        spdlog::error("Exception in update_access_counts: {}", e.what());
    }
}

// Production-grade implementations

bool VectorKnowledgeBase::update_entity(const std::string& entity_id, const nlohmann::json& updates) {
    if (!initialized_) return false;

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return false;

        std::string update_query = "UPDATE knowledge_entities SET ";
        std::vector<std::string> params;
        params.push_back(entity_id);

        int param_count = 1;
        for (auto& [key, value] : updates.items()) {
            update_query += key + " = $" + std::to_string(++param_count) + ",";
            params.push_back(value.dump());
        }
        update_query += " updated_at = NOW() WHERE entity_id = $1";

        bool success = conn->execute_command(update_query, params);
        if (success) {
            // Update cache
            entity_cache_.erase(entity_id);
            // Re-index if needed
            if (updates.contains("domain") || updates.contains("knowledge_type")) {
                rebuild_indexes();
            }
        }

        return success;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to update entity: " + std::string(e.what()));
        return false;
    }
}

bool VectorKnowledgeBase::delete_entity(const std::string& entity_id) {
    if (!initialized_) return false;

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return false;

        // Delete relationships first
        std::string delete_relations = "DELETE FROM knowledge_relationships WHERE source_entity_id = $1 OR target_entity_id = $1";
        conn->execute_command(delete_relations, {entity_id});

        // Delete the entity
        std::string delete_query = "DELETE FROM knowledge_entities WHERE entity_id = $1";
        bool success = conn->execute_command(delete_query, {entity_id});

        if (success) {
            // Clean up indexes and cache
            remove_from_index(entity_id);
            entity_cache_.erase(entity_id);
            embedding_cache_.erase(entity_id);
            total_entities_--;
        }

        return success;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to delete entity: " + std::string(e.what()));
        return false;
    }
}
std::vector<QueryResult> VectorKnowledgeBase::hybrid_search(
    const std::string& /*text_query*/,
    const std::vector<float>& embedding_query,
    const SemanticQuery& config
) {
    if (!initialized_) return {};

    // Combine text and vector search results with weighted scoring
    std::vector<QueryResult> text_results = semantic_search(config);
    std::vector<QueryResult> vector_results = perform_vector_search(embedding_query, config);

    // Create a map to merge results
    std::unordered_map<std::string, QueryResult> merged_results;

    // Add text results with text weight
    const float TEXT_WEIGHT = 0.6f;
    const float VECTOR_WEIGHT = 0.4f;

    for (const auto& result : text_results) {
        merged_results[result.entity.entity_id] = result;
        merged_results[result.entity.entity_id].similarity_score *= TEXT_WEIGHT;
    }

    // Add/merge vector results
    for (const auto& result : vector_results) {
        auto it = merged_results.find(result.entity.entity_id);
        if (it != merged_results.end()) {
            // Merge scores
            it->second.similarity_score += result.similarity_score * VECTOR_WEIGHT;
        } else {
            merged_results[result.entity.entity_id] = result;
            merged_results[result.entity.entity_id].similarity_score *= VECTOR_WEIGHT;
        }
    }

    std::vector<QueryResult> final_results;
    for (auto& pair : merged_results) {
        if (pair.second.similarity_score >= config.similarity_threshold) {
            final_results.push_back(pair.second);
        }
    }

    // Sort by combined score
    std::sort(final_results.begin(), final_results.end(),
        [](const QueryResult& a, const QueryResult& b) {
            return a.similarity_score > b.similarity_score;
        });

    // Limit results
    if (final_results.size() > static_cast<size_t>(config.max_results)) {
        final_results.resize(static_cast<size_t>(config.max_results));
    }

    return final_results;
}

std::vector<QueryResult> VectorKnowledgeBase::perform_vector_search(const std::vector<float>& query_embedding, const SemanticQuery& config) {
    std::vector<QueryResult> results;

    try {
        auto similarities = find_similar_vectors(query_embedding, config);

        for (const auto& [entity_id, similarity] : similarities) {
            if (similarity >= config.similarity_threshold) {
                auto entity_opt = load_entity(entity_id);
                if (entity_opt) {
                    QueryResult result;
                    result.entity = *entity_opt;
                    result.similarity_score = similarity;
                    result.query_time = std::chrono::microseconds(0);

                    results.push_back(result);
                }
            }
        }

        // Sort by similarity score
        std::sort(results.begin(), results.end(),
            [](const QueryResult& a, const QueryResult& b) {
                return a.similarity_score > b.similarity_score;
            });

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Vector search failed: " + std::string(e.what()));
    }

    return results;
}

bool VectorKnowledgeBase::create_relationship(
    const std::string& source_id,
    const std::string& target_id,
    const std::string& relationship_type,
    const nlohmann::json& properties
) {
    if (!initialized_) return false;

    try {
        return store_relationship(source_id, target_id, relationship_type, properties);
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to create relationship: " + std::string(e.what()));
        return false;
    }
}

std::vector<KnowledgeEntity> VectorKnowledgeBase::get_related_entities(
    const std::string& entity_id,
    const std::string& relationship_type,
    int max_depth
) {
    if (!initialized_) return {};

    try {
        std::vector<std::string> related_ids = get_related_entity_ids(entity_id, relationship_type, max_depth);
        return load_entities_batch(related_ids);
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to get related entities: " + std::string(e.what()));
        return {};
    }
}

nlohmann::json VectorKnowledgeBase::get_knowledge_graph(const std::string& entity_id, int radius) {
    nlohmann::json graph;
    graph["nodes"] = nlohmann::json::array();
    graph["edges"] = nlohmann::json::array();

    try {
        // Get central entity
        auto central_entity = load_entity(entity_id);
        if (!central_entity) return graph;

        graph["nodes"].push_back({
            {"id", entity_id},
            {"label", central_entity->title},
            {"type", knowledge_type_to_string(central_entity->knowledge_type)},
            {"domain", domain_to_string(central_entity->domain)}
        });

        // Get related entities within radius using BFS
        std::unordered_set<std::string> visited = {entity_id};
        std::queue<std::pair<std::string, int>> queue;
        queue.push({entity_id, 0});

        while (!queue.empty()) {
            auto [current_id, depth] = queue.front();
            queue.pop();

            if (depth >= radius) continue;

            auto related_ids = get_related_entity_ids(current_id, "", 1);
            for (const auto& related_id : related_ids) {
                if (visited.find(related_id) == visited.end()) {
                    visited.insert(related_id);
                    queue.push({related_id, depth + 1});

                    auto related_entity = load_entity(related_id);
                    if (related_entity) {
                        graph["nodes"].push_back({
                            {"id", related_id},
                            {"label", related_entity->title},
                            {"type", knowledge_type_to_string(related_entity->knowledge_type)},
                            {"domain", domain_to_string(related_entity->domain)}
                        });

                        graph["edges"].push_back({
                            {"source", current_id},
                            {"target", related_id},
                            {"type", "relationship"}
                        });
                    }
                }
            }
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to build knowledge graph: " + std::string(e.what()));
    }

    return graph;
}
bool VectorKnowledgeBase::set_memory_policy(const std::string& entity_id, MemoryRetention policy) {
    if (!initialized_) return false;

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return false;

        std::string update_query = R"(
            UPDATE knowledge_entities
            SET retention_policy = $2, expires_at = $3, updated_at = NOW()
            WHERE entity_id = $1
        )";

        auto expires_at = calculate_expiry_time(policy);
        std::string expires_str = std::to_string(expires_at.time_since_epoch().count());

        std::vector<std::string> params = {
            entity_id,
            retention_policy_to_string(policy),
            expires_str
        };

        return conn->execute_command(update_query, params);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to set memory policy: " + std::string(e.what()));
        return false;
    }
}

std::vector<std::string> VectorKnowledgeBase::cleanup_expired_memory(MemoryRetention policy) {
    if (!initialized_) return {};

    std::vector<std::string> cleaned_entities;

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return {};

        std::string query = "DELETE FROM knowledge_entities WHERE retention_policy = $1 AND expires_at < NOW() RETURNING entity_id";
        std::vector<std::string> params = {retention_policy_to_string(policy)};

        auto results = conn->execute_query_multi(query, params);
        for (const auto& row : results) {
            std::string entity_id = row["entity_id"];
            cleaned_entities.push_back(entity_id);

            // Clean up caches and indexes
            entity_cache_.erase(entity_id);
            embedding_cache_.erase(entity_id);
            remove_from_index(entity_id);
        }

        if (!cleaned_entities.empty()) {
            total_entities_ -= static_cast<int64_t>(cleaned_entities.size());
            logger_->log(LogLevel::INFO, "Cleaned up " + std::to_string(cleaned_entities.size()) + " expired entities");
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to cleanup expired memory: " + std::string(e.what()));
    }

    return cleaned_entities;
}

nlohmann::json VectorKnowledgeBase::get_memory_statistics() const {
    nlohmann::json stats;

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return stats;

        // Get counts by retention policy
        auto policy_results = conn->execute_query_multi(R"(
            SELECT retention_policy, COUNT(*) as count,
                   SUM(CASE WHEN expires_at < NOW() THEN 1 ELSE 0 END) as expired_count
            FROM knowledge_entities
            GROUP BY retention_policy
        )");

        for (const auto& row : policy_results) {
            std::string policy = row["retention_policy"];
            stats["policies"][policy] = {
                {"total_count", static_cast<int>(std::stoll(std::string(row["count"])))},
                {"expired_count", static_cast<int>(std::stoll(std::string(row["expired_count"])))}
            };
        }

        // Get storage statistics
        auto storage_stats = conn->execute_query_single(R"(
            SELECT
                COUNT(*) as total_entities,
                AVG(LENGTH(content)) as avg_content_length,
                SUM(LENGTH(content)) as total_content_size
            FROM knowledge_entities
        )");

        if (storage_stats) {
            stats["storage"]["total_entities"] = static_cast<int>(std::stoll(std::string((*storage_stats)["total_entities"])));
            stats["storage"]["avg_content_length"] = std::stod(std::string((*storage_stats)["avg_content_length"]));
            stats["storage"]["total_content_size"] = std::stod(std::string((*storage_stats)["total_content_size"]));
        }

        // Cache statistics
        stats["cache"]["entity_cache_size"] = entity_cache_.size();
        stats["cache"]["embedding_cache_size"] = embedding_cache_.size();
        stats["cache"]["total_searches"] = static_cast<int>(total_searches_);
        stats["cache"]["cache_hits"] = static_cast<int>(cache_hits_);
        stats["cache"]["cache_misses"] = static_cast<int>(cache_misses_);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to get memory statistics: " + std::string(e.what()));
    }

    return stats;
}
bool VectorKnowledgeBase::learn_from_interaction(const std::string& query, const std::string& selected_entity_id, float reward) {
    if (!initialized_) return false;

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return false;

        std::string insert_query = R"(
            INSERT INTO learning_interactions (
                agent_type, agent_name, query_text, selected_entity_id, reward_score, timestamp
            ) VALUES ('VECTOR_KB', 'VectorKnowledgeBase', $1, $2, $3, NOW())
        )";

        std::vector<std::string> params = {query, selected_entity_id, std::to_string(reward)};

        if (!conn->execute_command(insert_query, params)) {
            return false;
        }

        // Update entity confidence based on reward
        if (reward > 0) {
            update_entity_confidence(selected_entity_id, reward * 0.1f);
        }

        // Update access patterns for learning
        update_access_patterns(selected_entity_id);

        logger_->log(LogLevel::DEBUG, "Recorded learning interaction for entity: " + selected_entity_id);
        return true;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to learn from interaction: " + std::string(e.what()));
        return false;
    }
}

std::vector<std::string> VectorKnowledgeBase::get_learning_recommendations(const std::string& domain) {
    std::vector<std::string> recommendations;

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return recommendations;

        // Find entities that are frequently accessed but have low confidence
        std::string query = R"(
            SELECT entity_id
            FROM knowledge_entities
            WHERE domain = $1
            AND access_count > 5
            AND confidence_score < 0.7
            ORDER BY access_count DESC, confidence_score ASC
            LIMIT 10
        )";

        std::vector<std::string> params = {domain};
        auto results = conn->execute_query_multi(query, params);

        for (const auto& row : results) {
            recommendations.push_back(row["entity_id"]);
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to get learning recommendations: " + std::string(e.what()));
    }

    return recommendations;
}

bool VectorKnowledgeBase::reinforce_learning_patterns(const std::vector<std::string>& entity_ids) {
    if (!initialized_) return false;

    for (const auto& entity_id : entity_ids) {
        update_entity_confidence(entity_id, 0.01f); // Small positive reinforcement
    }

    return true;
}
nlohmann::json VectorKnowledgeBase::get_domain_statistics(KnowledgeDomain domain) const {
    nlohmann::json stats;

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return stats;

        std::string query = R"(
            SELECT knowledge_type, COUNT(*) as count,
                   AVG(confidence_score) as avg_confidence,
                   AVG(access_count) as avg_access,
                   MAX(created_at) as latest_creation,
                   SUM(LENGTH(content)) as total_content_size
            FROM knowledge_entities
            WHERE domain = $1
            GROUP BY knowledge_type
            ORDER BY count DESC
        )";

        std::vector<std::string> params = {domain_to_string(domain)};
        auto results = conn->execute_query_multi(query, params);

        stats["domain"] = domain_to_string(domain);
        stats["types"] = nlohmann::json::object();

        for (const auto& row : results) {
            std::string type = row["knowledge_type"];
            stats["types"][type] = {
                {"count", std::stoi(std::string(row["count"]))},
                {"avg_confidence", std::stod(std::string(row["avg_confidence"]))},
                {"avg_access", std::stod(std::string(row["avg_access"]))},
                {"total_content_size", std::stod(std::string(row["total_content_size"]))}
            };
        }

        // Get temporal distribution
        auto temporal_query = conn->execute_query_multi(R"(
            SELECT DATE_TRUNC('month', created_at) as month, COUNT(*) as count
            FROM knowledge_entities
            WHERE domain = $1
            GROUP BY DATE_TRUNC('month', created_at)
            ORDER BY month DESC
            LIMIT 12
        )", params);

        stats["temporal_distribution"] = nlohmann::json::array();
        for (const auto& row : temporal_query) {
            stats["temporal_distribution"].push_back({
                {"month", row["month"]},
                {"count", std::stoi(std::string(row["count"]))}
            });
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to get domain statistics: " + std::string(e.what()));
    }

    return stats;
}

std::vector<std::pair<std::string, int>> VectorKnowledgeBase::get_popular_entities(int limit) const {
    std::vector<std::pair<std::string, int>> popular;

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return popular;

        std::string query = R"(
            SELECT entity_id, access_count, confidence_score
            FROM knowledge_entities
            ORDER BY (access_count * confidence_score) DESC
            LIMIT $1
        )";

        std::vector<std::string> params = {std::to_string(limit)};
        auto results = conn->execute_query_multi(query, params);

        for (const auto& row : results) {
            popular.emplace_back(row["entity_id"], std::stoi(std::string(row["access_count"])));
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to get popular entities: " + std::string(e.what()));
    }

    return popular;
}

std::vector<std::pair<std::string, float>> VectorKnowledgeBase::get_confidence_distribution() const {
    std::vector<std::pair<std::string, float>> distribution;

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return distribution;

        std::string query = R"(
            SELECT entity_id, confidence_score
            FROM knowledge_entities
            ORDER BY confidence_score DESC
        )";

        auto results = conn->execute_query_multi(query, {});

        for (const auto& row : results) {
            distribution.emplace_back(row["entity_id"], std::stof(std::string(row["confidence_score"])));
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to get confidence distribution: " + std::string(e.what()));
    }

    return distribution;
}
nlohmann::json VectorKnowledgeBase::get_context_for_decision(const std::string& decision_context, KnowledgeDomain domain, int max_context_items) {
    nlohmann::json context;

    try {
        SemanticQuery query;
        query.query_text = decision_context;
        query.domain_filter = domain;
        query.max_results = max_context_items;
        query.type_filters = {KnowledgeType::RULE, KnowledgeType::PATTERN, KnowledgeType::EXPERIENCE};

        auto results = semantic_search(query);

        context["decision_context"] = decision_context;
        context["domain"] = domain_to_string(domain);
        context["relevant_knowledge"] = nlohmann::json::array();

        for (const auto& result : results) {
            context["relevant_knowledge"].push_back({
                {"entity_id", result.entity.entity_id},
                {"title", result.entity.title},
                {"content_preview", result.entity.content.substr(0, std::min(200UL, result.entity.content.size()))},
                {"confidence", result.entity.confidence_score},
                {"relevance_score", result.similarity_score},
                {"knowledge_type", knowledge_type_to_string(result.entity.knowledge_type)}
            });
        }

        // Add decision patterns analysis
        nlohmann::json patterns;
        try {
            // Analyze patterns in the results for decision-making insights
            std::unordered_map<std::string, int> type_counts;
            std::unordered_map<std::string, float> avg_confidence_by_type;

            for (const auto& result : results) {
                std::string type_str = knowledge_type_to_string(result.entity.knowledge_type);
                type_counts[type_str]++;
                avg_confidence_by_type[type_str] += result.entity.confidence_score;
            }

            patterns["knowledge_types"] = nlohmann::json::object();
            for (const auto& [type, count] : type_counts) {
                patterns["knowledge_types"][type] = {
                    {"count", count},
                    {"avg_confidence", count > 0 ? avg_confidence_by_type[type] / static_cast<float>(count) : 0.0f}
                };
            }

            // Identify most relevant patterns
            if (!results.empty()) {
                patterns["primary_pattern"] = {
                    {"entity_id", results[0].entity.entity_id},
                    {"confidence", results[0].entity.confidence_score},
                    {"relevance", results[0].similarity_score}
                };
            }

            patterns["total_entities_considered"] = results.size();

        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR, "Failed to analyze decision patterns: " + std::string(e.what()));
        }
        context["decision_patterns"] = patterns;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to get context for decision: " + std::string(e.what()));
    }

    return context;
}

std::vector<KnowledgeEntity> VectorKnowledgeBase::get_relevant_knowledge(const std::string& agent_query, const std::string& agent_type, int limit) {
    try {
        SemanticQuery query;
        query.query_text = agent_query;
        query.max_results = limit;

        // Add agent-specific filtering
        if (agent_type == "compliance_monitor") {
            query.domain_filter = KnowledgeDomain::REGULATORY_COMPLIANCE;
        } else if (agent_type == "fraud_detector") {
            query.domain_filter = KnowledgeDomain::TRANSACTION_MONITORING;
        } else if (agent_type == "audit_analyst") {
            query.domain_filter = KnowledgeDomain::AUDIT_INTELLIGENCE;
        }

        auto results = semantic_search(query);
        std::vector<KnowledgeEntity> entities;

        for (const auto& result : results) {
            entities.push_back(result.entity);
            // Record access for analytics
            update_access_patterns(result.entity.entity_id);
        }

        return entities;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to get relevant knowledge: " + std::string(e.what()));
        return {};
    }
}

bool VectorKnowledgeBase::update_knowledge_from_feedback(const std::string& entity_id, const nlohmann::json& feedback) {
    if (!initialized_) return false;

    try {
        float feedback_score = feedback.value("score", 1.0f);
        std::string feedback_type = feedback.value("type", "general");

        // Update entity confidence
        update_entity_confidence(entity_id, feedback_score);

        // Record feedback in learning interactions
        auto conn = db_pool_->get_connection();
        if (conn) {
            std::string insert_query = R"(
                INSERT INTO learning_interactions (
                    agent_type, agent_name, query_text, selected_entity_id, reward_score,
                    interaction_context, timestamp
                ) VALUES ('USER_FEEDBACK', $1, '', $2, $3, $4, NOW())
            )";

            std::vector<std::string> params = {
                feedback_type,
                entity_id,
                std::to_string(feedback_score),
                feedback.dump()
            };

            conn->execute_command(insert_query, params);
        }

        logger_->log(LogLevel::DEBUG, "Updated knowledge from feedback for entity: " + entity_id);
        return true;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to update knowledge from feedback: " + std::string(e.what()));
        return false;
    }
}
std::vector<QueryResult> VectorKnowledgeBase::search_transaction_patterns(const std::string& transaction_type, const nlohmann::json& risk_indicators) {
    SemanticQuery query;
    query.query_text = transaction_type + " transaction pattern";
    query.domain_filter = KnowledgeDomain::TRANSACTION_MONITORING;
    query.type_filters = {KnowledgeType::PATTERN, KnowledgeType::EXPERIENCE};
    query.max_results = 20;

    // Incorporate risk indicators into search
    for (const auto& indicator : risk_indicators) {
        if (indicator.is_string()) {
            query.query_text += " " + std::string(indicator);
        }
    }

    // Add risk-based filtering
    if (risk_indicators.contains("risk_level")) {
        std::string risk_level = risk_indicators["risk_level"];
        if (risk_level == "high") {
            query.similarity_threshold = 0.8f; // Higher threshold for high-risk
        }
    }

    return semantic_search(query);
}

std::vector<QueryResult> VectorKnowledgeBase::search_compliance_requirements(const std::string& business_domain, const std::string& regulation_type) {
    SemanticQuery query;
    query.query_text = business_domain + " " + regulation_type + " compliance requirement";
    query.domain_filter = KnowledgeDomain::REGULATORY_COMPLIANCE;
    query.type_filters = {KnowledgeType::RULE, KnowledgeType::FACT};
    query.max_results = 15;
    query.similarity_threshold = 0.75f;

    return semantic_search(query);
}

std::vector<QueryResult> VectorKnowledgeBase::search_audit_anomalies(const std::string& system_name, const std::chrono::system_clock::time_point& start_time) {
    SemanticQuery query;
    query.query_text = system_name + " audit anomaly pattern";
    query.domain_filter = KnowledgeDomain::AUDIT_INTELLIGENCE;
    query.type_filters = {KnowledgeType::PATTERN, KnowledgeType::EXPERIENCE};
    query.max_results = 25;

    // Note: Time-based filtering would require additional implementation
    (void)start_time;

    return semantic_search(query);
}
bool VectorKnowledgeBase::rebuild_indexes() {
    if (!initialized_) return false;

    try {
        logger_->log(LogLevel::INFO, "Starting index rebuild...");

        // Clear existing indexes
        domain_index_.clear();
        type_index_.clear();
        tag_index_.clear();

        // Rebuild from database
        auto conn = db_pool_->get_connection();
        if (!conn) return false;

        std::string query = "SELECT entity_id, domain, knowledge_type, tags FROM knowledge_entities";
        auto results = conn->execute_query_multi(query, {});

        for (const auto& row : results) {
            std::string entity_id = row["entity_id"];
            KnowledgeDomain domain = string_to_domain(row["domain"]);
            KnowledgeType type = string_to_knowledge_type(row["knowledge_type"]);

            domain_index_[domain].insert(entity_id);
            type_index_[type].insert(entity_id);

            // Parse and index tags using proper JSON parsing
            try {
                // Tags are stored as JSON array strings in database
                std::string tags_str = row["tags"];
                if (!tags_str.empty() && tags_str != "{}" && tags_str != "null") {
                    // Parse JSON array of tags
                    nlohmann::json tags_json = nlohmann::json::parse(tags_str);
                    
                    if (tags_json.is_array()) {
                        // Build inverted index: tag -> set of entity_ids
                        for (const auto& tag : tags_json) {
                            if (tag.is_string()) {
                                std::string tag_str = tag.get<std::string>();
                                // Store in tag index for fast tag-based lookups
                                tag_index_[tag_str].insert(entity_id);
                            }
                        }
                    }
                }
            } catch (const nlohmann::json::parse_error& e) {
                logger_->log(LogLevel::WARN, "Failed to parse tags for entity " + entity_id + 
                           ": " + std::string(e.what()));
            } catch (const std::exception& e) {
                logger_->log(LogLevel::WARN, "Error processing tags for entity " + entity_id + 
                           ": " + std::string(e.what()));
            }
        }

        // Rebuild vector indexes (this would be database-specific optimization)
        logger_->log(LogLevel::INFO, "Rebuilt indexes for " + std::to_string(results.size()) + " entities");
        return true;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to rebuild indexes: " + std::string(e.what()));
        return false;
    }
}

bool VectorKnowledgeBase::optimize_storage() {
    if (!initialized_) return false;

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return false;

        // Run database optimization commands
        std::vector<std::string> optimization_commands = {
            "VACUUM ANALYZE knowledge_entities",
            "VACUUM ANALYZE knowledge_relationships",
            "VACUUM ANALYZE learning_interactions",
            "REINDEX TABLE knowledge_entities",
            "REINDEX TABLE knowledge_relationships"
        };

        for (const auto& command : optimization_commands) {
            try {
                conn->execute_command(command, {});
            } catch (const std::exception& e) {
                logger_->log(LogLevel::WARN, "Optimization command failed: " + std::string(e.what()));
            }
        }

        // Clear and rebuild caches if needed
        if (entity_cache_.size() > static_cast<size_t>(MAX_EMBEDDING_CACHE_SIZE)) {
            entity_cache_.clear();
            embedding_cache_.clear();
            logger_->log(LogLevel::INFO, "Cleared oversized caches during optimization");
        }

        logger_->log(LogLevel::INFO, "Storage optimization completed");
        return true;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to optimize storage: " + std::string(e.what()));
        return false;
    }
}

nlohmann::json VectorKnowledgeBase::export_knowledge_base(const std::vector<KnowledgeDomain>& domains) const {
    nlohmann::json export_data;

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return export_data;

        std::string query = "SELECT * FROM knowledge_entities";
        if (!domains.empty()) {
            query += " WHERE domain IN (";
            for (size_t i = 0; i < domains.size(); ++i) {
                query += "$" + std::to_string(i + 1);
                if (i < domains.size() - 1) query += ",";
            }
            query += ")";
        }

        std::vector<std::string> params;
        for (const auto& domain : domains) {
            params.push_back(domain_to_string(domain));
        }

        auto results = conn->execute_query_multi(query, params);

        export_data["metadata"] = {
            {"export_timestamp", std::to_string(std::chrono::system_clock::now().time_since_epoch().count())},
            {"version", "1.0"},
            {"domains_exported", domains.size()}
        };

        export_data["entities"] = nlohmann::json::array();
        for (const auto& row : results) {
            nlohmann::json entity;
            entity["entity_id"] = row["entity_id"];
            entity["domain"] = row["domain"];
            entity["knowledge_type"] = row["knowledge_type"];
            entity["title"] = row["title"];
            entity["content"] = row["content"];
            entity["metadata"] = row["metadata"]; // Already parsed by database layer
            entity["retention_policy"] = row["retention_policy"];
            entity["confidence_score"] = std::stod(std::string(row["confidence_score"]));
            entity["access_count"] = std::stoi(std::string(row["access_count"]));
            entity["created_at"] = row["created_at"];
            entity["tags"] = row["tags"]; // Already parsed by database layer

            export_data["entities"].push_back(entity);
        }

        // Export relationships
        auto relationship_results = conn->execute_query_multi(
            "SELECT * FROM knowledge_relationships WHERE source_entity_id IN (SELECT entity_id FROM knowledge_entities WHERE " +
            (domains.empty() ? "TRUE" : "domain IN (" + std::string(domains.size(), '?').replace(domains.size()-1, 1, ")")) + ")",
            params
        );

        export_data["relationships"] = nlohmann::json::array();
        for (const auto& row : relationship_results) {
            export_data["relationships"].push_back({
                {"source_entity_id", row["source_entity_id"]},
                {"target_entity_id", row["target_entity_id"]},
                {"relationship_type", row["relationship_type"]},
                {"properties", row["properties"]} // Already parsed by database layer
            });
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to export knowledge base: " + std::string(e.what()));
    }

    return export_data;
}

bool VectorKnowledgeBase::import_knowledge_base(const nlohmann::json& knowledge_data) {
    if (!initialized_ || !knowledge_data.contains("entities")) return false;

    try {
        // Start transaction
        auto conn = db_pool_->get_connection();
        if (!conn) return false;

        conn->execute_command("BEGIN", {});

        try {
            // Import entities
            for (const auto& entity_json : knowledge_data["entities"]) {
                KnowledgeEntity entity;

                entity.entity_id = entity_json["entity_id"];
                entity.domain = string_to_domain(entity_json["domain"]);
                entity.knowledge_type = string_to_knowledge_type(entity_json["knowledge_type"]);
                entity.title = entity_json["title"];
                entity.content = entity_json["content"];
                entity.metadata = entity_json["metadata"];
                entity.retention_policy = string_to_retention_policy(entity_json.value("retention_policy", "PERSISTENT"));
                entity.confidence_score = entity_json.value("confidence_score", 1.0f);
                entity.access_count = entity_json.value("access_count", 0);
                entity.created_at = std::chrono::system_clock::now();
                entity.last_accessed = std::chrono::system_clock::now();
                entity.expires_at = std::chrono::system_clock::now() + std::chrono::years(1);

                // Handle tags - assume they are stored as array in JSON
                if (entity_json.contains("tags")) {
                    auto tags_val = entity_json["tags"];
                    if (tags_val.is_array()) {
                        for (const auto& tag : tags_val) {
                            if (tag.is_string()) {
                                entity.tags.push_back(tag);
                            }
                        }
                    }
                }

                if (!store_entity(entity)) {
                    conn->execute_command("ROLLBACK", {});
                    return false;
                }
            }

            // Import relationships if present
            if (knowledge_data.contains("relationships")) {
                for (const auto& rel_json : knowledge_data["relationships"]) {
                    std::string source_id = rel_json["source_entity_id"];
                    std::string target_id = rel_json["target_entity_id"];
                    std::string rel_type = rel_json["relationship_type"];
                    nlohmann::json properties = rel_json["properties"];

                    if (!create_relationship(source_id, target_id, rel_type, properties)) {
                        conn->execute_command("ROLLBACK", {});
                        return false;
                    }
                }
            }

            conn->execute_command("COMMIT", {});
            logger_->log(LogLevel::INFO, "Imported knowledge base successfully");
            return true;

        } catch (const std::exception& e) {
            conn->execute_command("ROLLBACK", {});
            throw e;
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to import knowledge base: " + std::string(e.what()));
        return false;
    }
}

bool VectorKnowledgeBase::update_entity_confidence(const std::string& entity_id, float confidence_delta) {
    if (!initialized_) return false;

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return false;

        std::string update_query = "UPDATE knowledge_entities SET confidence_score = LEAST(GREATEST(confidence_score + $2, 0.0), 1.0), updated_at = NOW() WHERE entity_id = $1";

        std::vector<std::string> params = {entity_id, std::to_string(confidence_delta)};

        bool success = conn->execute_command(update_query, params);
        if (success) {
            // Update cache
            entity_cache_.erase(entity_id);
        }

        return success;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to update entity confidence: " + std::string(e.what()));
        return false;
    }
}

// Private methods implementations

std::optional<KnowledgeEntity> VectorKnowledgeBase::load_entity(const std::string& entity_id) {
    // Check cache first
    {
        std::lock_guard<std::mutex> lock(entity_cache_mutex_);
        auto it = entity_cache_.find(entity_id);
        if (it != entity_cache_.end()) {
            cache_hits_++;
            return it->second;
        }
    }

    cache_misses_++;

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return std::nullopt;

        std::string query = "SELECT * FROM knowledge_entities WHERE entity_id = $1";
        std::vector<std::string> params = {entity_id};

        auto result = conn->execute_query_single(query, params);
        if (!result) return std::nullopt;

        KnowledgeEntity entity;
        entity.entity_id = (*result)["entity_id"];
        entity.domain = string_to_domain((*result)["domain"]);
        entity.knowledge_type = string_to_knowledge_type((*result)["knowledge_type"]);
        entity.title = (*result)["title"];
        entity.content = (*result)["content"];
        entity.metadata = nlohmann::json::parse(std::string((*result)["metadata"]));
        entity.retention_policy = string_to_retention_policy((*result)["retention_policy"]);
        entity.confidence_score = std::stof(std::string((*result)["confidence_score"]));

        // Parse tags from JSON array string
        try {
            auto tags_json = nlohmann::json::parse(std::string((*result)["tags"]));
            for (const auto& tag : tags_json) {
                entity.tags.push_back(tag.get<std::string>());
            }
        } catch (const std::exception&) {
            // Skip malformed tags
        }

        // Cache the entity
        {
            std::lock_guard<std::mutex> lock(entity_cache_mutex_);
            entity_cache_[entity_id] = entity;
        }

        return entity;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to load entity: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::vector<KnowledgeEntity> VectorKnowledgeBase::load_entities_batch(const std::vector<std::string>& entity_ids) {
    std::vector<KnowledgeEntity> entities;

    for (const auto& entity_id : entity_ids) {
        auto entity = load_entity(entity_id);
        if (entity) {
            entities.push_back(*entity);
        }
    }

    return entities;
}

bool VectorKnowledgeBase::index_entity(const KnowledgeEntity& entity) {
    domain_index_[entity.domain].insert(entity.entity_id);
    type_index_[entity.knowledge_type].insert(entity.entity_id);

    for (const auto& tag : entity.tags) {
        tag_index_[tag].insert(entity.entity_id);
    }

    return true;
}

bool VectorKnowledgeBase::remove_from_index(const std::string& entity_id) {
    for (auto& [domain, entities] : domain_index_) {
        entities.erase(entity_id);
    }

    for (auto& [type, entities] : type_index_) {
        entities.erase(entity_id);
    }

    for (auto& [tag, entities] : tag_index_) {
        entities.erase(entity_id);
    }

    return true;
}

std::vector<std::pair<std::string, float>> VectorKnowledgeBase::find_similar_vectors(const std::vector<float>& query_vector, const SemanticQuery& config) {
    std::vector<std::pair<std::string, float>> similarities;

    try {
        // Validate input vector
        if (query_vector.empty()) {
            logger_->log(LogLevel::WARN, "Empty query vector provided to find_similar_vectors");
            return similarities;
        }

        auto conn = db_pool_->get_connection();
        if (!conn) return similarities;

        // Use pgvector for efficient similarity search
        std::string similarity_op;
        switch (config.similarity_metric) {
            case VectorSimilarity::COSINE: similarity_op = "<=>"; break;
            case VectorSimilarity::EUCLIDEAN: similarity_op = "<->"; break;
            case VectorSimilarity::DOT_PRODUCT: similarity_op = "<#>"; break;
            case VectorSimilarity::MANHATTAN: similarity_op = "<+>"; break;
            default: similarity_op = "<=>"; break;
        }

        // Convert vector to PostgreSQL vector string format
        std::string vector_string = vector_to_string(query_vector);

        std::string query = "SELECT entity_id, 1 - (embedding " + similarity_op + " $1::vector) as similarity FROM knowledge_entities WHERE domain = $2 ORDER BY embedding " + similarity_op + " $1::vector LIMIT $3";

        std::vector<std::string> params = {
            vector_string,
            domain_to_string(config.domain_filter),
            std::to_string(config.max_results)
        };

        auto results = conn->execute_query_multi(query, params);
        for (const auto& row : results) {
            similarities.emplace_back(row["entity_id"], std::stof(std::string(row["similarity"])));
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to find similar vectors: " + std::string(e.what()));
    }

    return similarities;
}

std::chrono::system_clock::time_point VectorKnowledgeBase::calculate_expiry_time(MemoryRetention policy) {
    auto now = std::chrono::system_clock::now();

    switch (policy) {
        case MemoryRetention::EPHEMERAL:
            return now + config_.ephemeral_retention;
        case MemoryRetention::SESSION:
            return now + config_.session_retention;
        case MemoryRetention::PERSISTENT:
            return now + config_.archival_retention; // Very long retention
        case MemoryRetention::ARCHIVAL:
            return now + std::chrono::years(10); // Even longer
        default:
            return now + config_.session_retention;
    }
}

bool VectorKnowledgeBase::store_relationship(const std::string& source_id, const std::string& target_id, const std::string& relationship_type, const nlohmann::json& properties) {
    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return false;

        std::string insert_query = R"(
            INSERT INTO knowledge_relationships (
                source_entity_id, target_entity_id, relationship_type, properties
            ) VALUES ($1, $2, $3, $4)
            ON CONFLICT (source_entity_id, target_entity_id, relationship_type) DO UPDATE SET
                properties = EXCLUDED.properties,
                updated_at = NOW()
        )";

        std::vector<std::string> params = {
            source_id,
            target_id,
            relationship_type,
            properties.dump()
        };

        if (!conn->execute_command(insert_query, params)) {
            return false;
        }

        return true;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to store relationship: " + std::string(e.what()));
        return false;
    }
}

std::vector<std::string> VectorKnowledgeBase::get_related_entity_ids(const std::string& entity_id, const std::string& relationship_type, int max_depth) {
    std::vector<std::string> related_ids;

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return related_ids;

        std::string query = "SELECT DISTINCT target_entity_id FROM knowledge_relationships WHERE source_entity_id = $1";
        std::vector<std::string> params = {entity_id};

        if (!relationship_type.empty()) {
            query += " AND relationship_type = $2";
            params.push_back(relationship_type);
        }

        auto results = conn->execute_query_multi(query, params);
        for (const auto& row : results) {
            related_ids.push_back(row["target_entity_id"]);
        }

        // For deeper relationships, recursive implementation would be needed
        (void)max_depth;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to get related entity IDs: " + std::string(e.what()));
    }

    return related_ids;
}

void VectorKnowledgeBase::update_access_patterns(const std::string& entity_id) {
    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return;

        std::string insert_query = R"(
            INSERT INTO knowledge_access_patterns (
                entity_id, access_type, access_timestamp
            ) VALUES ($1, 'RETRIEVAL', NOW())
        )";

        std::vector<std::string> params = {entity_id};
        conn->execute_command(insert_query, params);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to update access patterns: " + std::string(e.what()));
    }
}

// Private helper methods

std::string VectorKnowledgeBase::vector_to_string(const std::vector<float>& vec) const {
    if (vec.empty()) return "[]";

    std::stringstream ss;
    ss << "[";
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) ss << ",";
        ss << std::fixed << std::setprecision(6) << vec[i];
    }
    ss << "]";
    return ss.str();
}

} // namespace regulens