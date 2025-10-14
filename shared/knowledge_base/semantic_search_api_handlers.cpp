/**
 * Semantic Search API Handlers Implementation
 * Production-grade API endpoints for vector-based knowledge retrieval
 */

#include "semantic_search_api_handlers.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <cctype>

namespace regulens {

// Utility functions (duplicated from vector_knowledge_base.cpp for API handlers)
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

SemanticSearchAPIHandlers::SemanticSearchAPIHandlers(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<VectorKnowledgeBase> knowledge_base
) : db_conn_(db_conn), knowledge_base_(knowledge_base), access_control_(db_conn) {

    if (!db_conn_) {
        throw std::runtime_error("Database connection is required for SemanticSearchAPIHandlers");
    }
    if (!knowledge_base_) {
        throw std::runtime_error("Vector knowledge base is required for SemanticSearchAPIHandlers");
    }

    spdlog::info("SemanticSearchAPIHandlers initialized");
}

SemanticSearchAPIHandlers::~SemanticSearchAPIHandlers() {
    spdlog::info("SemanticSearchAPIHandlers shutting down");
}

std::string SemanticSearchAPIHandlers::handle_semantic_search(const std::string& request_body, const std::string& user_id) {
    try {
        // Parse and validate request
        nlohmann::json request = nlohmann::json::parse(request_body);
        std::string validation_error;
        if (!validate_search_request(request, validation_error)) {
            return create_error_response(validation_error).dump();
        }

        if (!validate_user_access(user_id, "semantic_search")) {
            return create_error_response("Access denied", 403).dump();
        }

        // Parse search configuration
        SemanticQuery query_config = parse_search_request(request);

        // Perform semantic search
        auto start_time = std::chrono::high_resolution_clock::now();
        std::vector<QueryResult> results = knowledge_base_->semantic_search(query_config);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto search_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        // Format response
        nlohmann::json response_data = format_search_results(results);
        response_data["search_time_ms"] = search_time.count();
        response_data["total_results"] = results.size();
        response_data["query"] = query_config.query_text;

        spdlog::info("Semantic search completed for user {}: {} results in {}ms",
                    user_id, results.size(), search_time.count());

        return create_success_response(response_data, "Search completed successfully").dump();

    } catch (const nlohmann::json::exception& e) {
        spdlog::error("JSON parsing error in handle_semantic_search: {}", e.what());
        return create_error_response("Invalid JSON format", 400).dump();
    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_semantic_search: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string SemanticSearchAPIHandlers::handle_hybrid_search(const std::string& request_body, const std::string& user_id) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);
        std::string validation_error;
        if (!validate_search_request(request, validation_error)) {
            return create_error_response(validation_error).dump();
        }

        if (!validate_user_access(user_id, "hybrid_search")) {
            return create_error_response("Access denied", 403).dump();
        }

        SemanticQuery query_config = parse_search_request(request);

        auto overall_start = std::chrono::high_resolution_clock::now();

        auto vector_start = std::chrono::high_resolution_clock::now();
        std::vector<QueryResult> vector_results = knowledge_base_->semantic_search(query_config);
        auto vector_duration = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - vector_start);

        std::vector<QueryResult> keyword_results = perform_keyword_search(query_config);

        struct HybridAggregation {
            QueryResult result;
            float vector_score = 0.0f;
            float keyword_score = 0.0f;
        };

        std::unordered_map<std::string, HybridAggregation> aggregated;
        aggregated.reserve(vector_results.size() + keyword_results.size());

        for (auto& vector_result : vector_results) {
            HybridAggregation aggregation;
            aggregation.result = vector_result;
            aggregation.vector_score = vector_result.similarity_score;
            if (!aggregation.result.explanation.is_object()) {
                aggregation.result.explanation = nlohmann::json::object();
            }
            aggregation.result.explanation["search_strategy"] = "hybrid";
            aggregation.result.explanation["vector_score"] = aggregation.vector_score;
            aggregated.emplace(vector_result.entity.entity_id, std::move(aggregation));
        }

        const float keyword_weight = 0.35f;
        const float vector_weight = 1.0f - keyword_weight;

        for (size_t index = 0; index < keyword_results.size(); ++index) {
            QueryResult keyword_result = keyword_results[index];
            float rank_factor = 1.0f - static_cast<float>(index) / std::max<size_t>(1, keyword_results.size());
            float keyword_score = std::min(1.0f, 0.5f + 0.5f * rank_factor);

            auto it = aggregated.find(keyword_result.entity.entity_id);
            if (it == aggregated.end()) {
                HybridAggregation aggregation;
                aggregation.result = std::move(keyword_result);
                aggregation.keyword_score = keyword_score;
                aggregation.result.similarity_score = keyword_score;
                if (!aggregation.result.explanation.is_object()) {
                    aggregation.result.explanation = nlohmann::json::object();
                }
                aggregation.result.explanation["search_strategy"] = "hybrid";
                aggregation.result.explanation["vector_score"] = aggregation.vector_score;
                aggregation.result.explanation["keyword_score"] = aggregation.keyword_score;
                aggregated.emplace(aggregation.result.entity.entity_id, std::move(aggregation));
                continue;
            }

            it->second.keyword_score = std::max(it->second.keyword_score, keyword_score);

            std::unordered_set<std::string> merged_terms(
                it->second.result.matched_terms.begin(),
                it->second.result.matched_terms.end());
            merged_terms.insert(keyword_result.matched_terms.begin(), keyword_result.matched_terms.end());
            it->second.result.matched_terms.assign(merged_terms.begin(), merged_terms.end());

            if (!keyword_result.entity.tags.empty()) {
                std::unordered_set<std::string> merged_tags(
                    it->second.result.entity.tags.begin(),
                    it->second.result.entity.tags.end());
                merged_tags.insert(keyword_result.entity.tags.begin(), keyword_result.entity.tags.end());
                it->second.result.entity.tags.assign(merged_tags.begin(), merged_tags.end());
            }

            it->second.result.similarity_score = std::min(
                1.0f,
                it->second.vector_score * vector_weight + it->second.keyword_score * keyword_weight
            );

            if (!it->second.result.explanation.is_object()) {
                it->second.result.explanation = nlohmann::json::object();
            }
            it->second.result.explanation["search_strategy"] = "hybrid";
            it->second.result.explanation["vector_score"] = it->second.vector_score;
            it->second.result.explanation["keyword_score"] = it->second.keyword_score;
            if (!keyword_result.explanation.is_null()) {
                it->second.result.explanation["keyword_details"] = keyword_result.explanation;
            }
        }

        auto overall_duration = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - overall_start);

        std::vector<QueryResult> combined_results;
        combined_results.reserve(aggregated.size());
        for (auto& [entity_id, aggregation] : aggregated) {
            aggregation.result.query_time = overall_duration;
            combined_results.push_back(std::move(aggregation.result));
        }

        std::sort(combined_results.begin(), combined_results.end(), [](const QueryResult& lhs, const QueryResult& rhs) {
            return lhs.similarity_score > rhs.similarity_score;
        });

        if (combined_results.size() > static_cast<size_t>(query_config.max_results)) {
            combined_results.resize(query_config.max_results);
        }

        nlohmann::json response_data = format_search_results(combined_results);
        response_data["vector_query_time_us"] = vector_duration.count();
        response_data["overall_query_time_us"] = overall_duration.count();
        response_data["vector_result_count"] = vector_results.size();
        response_data["keyword_result_count"] = keyword_results.size();
        response_data["total_results"] = combined_results.size();
        response_data["search_type"] = "hybrid";

        return create_success_response(response_data, "Hybrid search completed successfully").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_hybrid_search: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string SemanticSearchAPIHandlers::handle_similar_entities(const std::string& entity_id, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "similar_entities")) {
            return create_error_response("Access denied", 403).dump();
        }

        // Production-grade similar entities search using vector similarity
        if (!knowledge_base_) {
            return create_error_response("Knowledge base not available", 503).dump();
        }
        
        std::string entity_query = R"(
            SELECT key, content_type, COALESCE(array_to_json(tags), '[]')::text AS tags_json
            FROM knowledge_base
            WHERE knowledge_id = $1
        )";

        auto entity_row = db_conn_->execute_query_single(entity_query, {entity_id});
        if (!entity_row.has_value()) {
            return create_error_response("Entity not found", 404).dump();
        }

        SemanticQuery query_config;
        query_config.query_text = entity_row->value("key", "");
        query_config.domain_filter = domain_from_string(entity_row->value("content_type", ""));
        query_config.max_results = 20;
        query_config.similarity_threshold = 0.6f;

        std::vector<QueryResult> similar_results = knowledge_base_->semantic_search(query_config);

        nlohmann::json similar_entities = nlohmann::json::array();
        for (const auto& result : similar_results) {
            if (result.entity.entity_id == entity_id) {
                continue;
            }
            similar_entities.push_back(format_search_result(result));
        }
        
        nlohmann::json response_data = {
            {"entity_id", entity_id},
            {"similar_entities", similar_entities},
            {"count", similar_entities.size()}
        };

        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_similar_entities: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string SemanticSearchAPIHandlers::handle_search_by_domain(const std::string& domain, const std::string& request_body, const std::string& user_id) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);
        request["domain_filter"] = domain;  // Override domain filter

        std::string validation_error;
        if (!validate_search_request(request, validation_error)) {
            return create_error_response(validation_error).dump();
        }

        if (!validate_user_access(user_id, "domain_search")) {
            return create_error_response("Access denied", 403).dump();
        }

        SemanticQuery query_config = parse_search_request(request);
        std::vector<QueryResult> results = knowledge_base_->semantic_search(query_config);

        nlohmann::json response_data = format_search_results(results);
        response_data["domain"] = domain;
        response_data["filtered_by_domain"] = true;

        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_search_by_domain: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string SemanticSearchAPIHandlers::handle_get_search_config() {
    try {
        nlohmann::json config = {
            {"embedding_dimensions", 384},
            {"default_similarity", "cosine"},
            {"max_results_per_query", 50},
            {"default_similarity_threshold", 0.7},
            {"supported_domains", {
                "REGULATORY_COMPLIANCE",
                "TRANSACTION_MONITORING",
                "AUDIT_INTELLIGENCE",
                "BUSINESS_PROCESSES",
                "RISK_MANAGEMENT",
                "LEGAL_FRAMEWORKS",
                "FINANCIAL_INSTRUMENTS",
                "MARKET_INTELLIGENCE"
            }},
            {"supported_knowledge_types", {
                "FACT",
                "RULE",
                "PATTERN",
                "RELATIONSHIP",
                "CONTEXT",
                "EXPERIENCE",
                "DECISION",
                "PREDICTION"
            }}
        };

        return create_success_response(config).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_search_config: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string SemanticSearchAPIHandlers::handle_update_search_config(const std::string& request_body) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);
        
        if (!request.contains("config_key") || !request.contains("value")) {
            return create_error_response("Missing required fields: config_key, value", 400).dump();
        }
        
        std::string config_key = request["config_key"];
        nlohmann::json config_value = request["value"];
        
        // Store configuration in database for persistence
        std::string store_query = R"(
            INSERT INTO semantic_search_config (config_key, config_value, updated_at)
            VALUES ($1, $2, NOW())
            ON CONFLICT (config_key) DO UPDATE
            SET config_value = $2, updated_at = NOW()
        )";
        
        db_conn_->execute_command(store_query, {
            config_key,
            config_value.dump()
        });
        
        nlohmann::json response = {
            {"success", true},
            {"message", "Search configuration updated successfully"},
            {"config_key", config_key},
            {"new_value", config_value}
        };
        
        return create_success_response(response).dump();
        
    } catch (const nlohmann::json::parse_error& e) {
        return create_error_response("Invalid JSON: " + std::string(e.what()), 400).dump();
    } catch (const std::exception& e) {
        return create_error_response("Configuration update failed: " + std::string(e.what()), 500).dump();
    }
}

std::string SemanticSearchAPIHandlers::handle_get_search_stats(const std::string& time_range) {
    try {
        // Production-grade search analytics from database
        std::string stats_query = R"(
            SELECT 
                COUNT(*) as total_searches,
                AVG(response_time_ms) as avg_response_time_ms,
                COUNT(DISTINCT user_id) as unique_users,
                AVG(results_count) as avg_results_count
            FROM semantic_search_logs
            WHERE created_at > NOW() - INTERVAL '1 day'
        )";
        
        // Adjust time range
        if (time_range == "week") {
            stats_query = R"(
                SELECT 
                    COUNT(*) as total_searches,
                    AVG(response_time_ms) as avg_response_time_ms,
                    COUNT(DISTINCT user_id) as unique_users,
                    AVG(results_count) as avg_results_count
                FROM semantic_search_logs
                WHERE created_at > NOW() - INTERVAL '7 days'
            )";
        } else if (time_range == "month") {
            stats_query = R"(
                SELECT 
                    COUNT(*) as total_searches,
                    AVG(response_time_ms) as avg_response_time_ms,
                    COUNT(DISTINCT user_id) as unique_users,
                    AVG(results_count) as avg_results_count
                FROM semantic_search_logs
                WHERE created_at > NOW() - INTERVAL '30 days'
            )";
        }
        
        auto stats_result = db_conn_->execute_query_single(stats_query);
        
        nlohmann::json stats;
        if (stats_result.has_value()) {
            stats = {
                {"total_searches", stats_result->value("total_searches", 0)},
                {"average_response_time_ms", stats_result->value("avg_response_time_ms", 0.0)},
                {"unique_users", stats_result->value("unique_users", 0)},
                {"avg_results_count", stats_result->value("avg_results_count", 0.0)},
                {"time_range", time_range}
            };
        } else {
            stats = {
                {"total_searches", 0},
                {"average_response_time_ms", 0.0},
                {"unique_users", 0},
                {"time_range", time_range},
                {"note", "No data available"}
            };
        }

        return create_success_response(stats).dump();
        
    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_search_stats: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string SemanticSearchAPIHandlers::handle_get_popular_queries(const std::string& limit_str) {
    int limit = 10;
    try {
        limit = std::stoi(limit_str);
        limit = std::max(1, std::min(limit, 100)); // Clamp between 1 and 100
    } catch (const std::exception&) {
        // Use default limit
    }

    try {
        // Production-grade popular queries tracking
        std::string popular_query = R"(
            SELECT 
                query_text,
                COUNT(*) as search_count,
                AVG(results_count) as avg_results,
                MAX(created_at) as last_searched
            FROM semantic_search_logs
            WHERE created_at > NOW() - INTERVAL '7 days'
            GROUP BY query_text
            ORDER BY search_count DESC
            LIMIT $1
        )";
        
        auto results = db_conn_->execute_query_multi(popular_query, {
            std::to_string(limit)
        });
        
        nlohmann::json queries_array = nlohmann::json::array();
        for (const auto& row : results) {
            queries_array.push_back({
                {"query", row.value("query_text", "")},
                {"search_count", row.value("search_count", 0)},
                {"avg_results", row.value("avg_results", 0.0)},
                {"last_searched", row.value("last_searched", "")}
            });
        }
        
        nlohmann::json response = {
            {"limit", limit},
            {"queries", queries_array},
            {"count", queries_array.size()}
        };

        return create_success_response(response).dump();
        
    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_popular_queries: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

// Helper method implementations

SemanticQuery SemanticSearchAPIHandlers::parse_search_request(const nlohmann::json& request) {
    SemanticQuery query;

    query.query_text = request.value("query", "");
    query.max_results = request.value("max_results", 10);
    query.similarity_threshold = request.value("similarity_threshold", 0.7f);
    query.include_metadata = request.value("include_metadata", true);
    query.include_relationships = request.value("include_relationships", false);

    // Parse domain filter
    std::string domain_str = request.value("domain_filter", "REGULATORY_COMPLIANCE");
    query.domain_filter = domain_from_string(domain_str);

    // Parse type filters
    if (request.contains("type_filters") && request["type_filters"].is_array()) {
        for (const auto& type_str : request["type_filters"]) {
            if (type_str.is_string()) {
                query.type_filters.push_back(knowledge_type_from_string(type_str.get<std::string>()));
            }
        }
    }

    // Parse tag filters
    if (request.contains("tag_filters") && request["tag_filters"].is_array()) {
        for (const auto& tag : request["tag_filters"]) {
            query.tag_filters.push_back(tag);
        }
    }

    // Parse age filter
    if (request.contains("max_age_hours")) {
        int max_age_hours = request["max_age_hours"];
        query.max_age = std::chrono::hours(max_age_hours);
    }

    return query;
}

KnowledgeDomain SemanticSearchAPIHandlers::domain_from_string(const std::string& domain_str) const {
    std::string normalized;
    normalized.reserve(domain_str.size());
    for (char ch : domain_str) {
        normalized.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
    }

    if (normalized == "TRANSACTION_MONITORING") return KnowledgeDomain::TRANSACTION_MONITORING;
    if (normalized == "AUDIT_INTELLIGENCE") return KnowledgeDomain::AUDIT_INTELLIGENCE;
    if (normalized == "BUSINESS_PROCESSES") return KnowledgeDomain::BUSINESS_PROCESSES;
    if (normalized == "RISK_MANAGEMENT") return KnowledgeDomain::RISK_MANAGEMENT;
    if (normalized == "LEGAL_FRAMEWORKS") return KnowledgeDomain::LEGAL_FRAMEWORKS;
    if (normalized == "FINANCIAL_INSTRUMENTS") return KnowledgeDomain::FINANCIAL_INSTRUMENTS;
    if (normalized == "MARKET_INTELLIGENCE") return KnowledgeDomain::MARKET_INTELLIGENCE;
    return KnowledgeDomain::REGULATORY_COMPLIANCE;
}

KnowledgeType SemanticSearchAPIHandlers::knowledge_type_from_string(const std::string& type_str) const {
    std::string normalized;
    normalized.reserve(type_str.size());
    for (char ch : type_str) {
        normalized.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
    }

    if (normalized == "FACT") return KnowledgeType::FACT;
    if (normalized == "RULE") return KnowledgeType::RULE;
    if (normalized == "PATTERN") return KnowledgeType::PATTERN;
    if (normalized == "RELATIONSHIP") return KnowledgeType::RELATIONSHIP;
    if (normalized == "CONTEXT") return KnowledgeType::CONTEXT;
    if (normalized == "EXPERIENCE") return KnowledgeType::EXPERIENCE;
    if (normalized == "DECISION") return KnowledgeType::DECISION;
    if (normalized == "PREDICTION") return KnowledgeType::PREDICTION;
    return KnowledgeType::CONTEXT;
}

std::vector<std::string> SemanticSearchAPIHandlers::parse_tag_array(const std::string& tags_str) const {
    std::vector<std::string> tags;
    if (tags_str.empty()) {
        return tags;
    }

    try {
        auto parsed = nlohmann::json::parse(tags_str);
        if (parsed.is_array()) {
            for (const auto& tag : parsed) {
                if (tag.is_string()) {
                    tags.push_back(tag.get<std::string>());
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::warn("Failed to parse tags array '{}': {}", tags_str, e.what());
    }

    return tags;
}

std::vector<std::string> SemanticSearchAPIHandlers::extract_keyword_matches(const std::string& query, const std::string& content) const {
    std::vector<std::string> matches;
    if (query.empty() || content.empty()) {
        return matches;
    }

    std::string lower_content = content;
    std::transform(lower_content.begin(), lower_content.end(), lower_content.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    std::unordered_set<std::string> unique_terms;
    std::string token;
    std::stringstream ss(query);
    while (ss >> token) {
        std::string normalized;
        normalized.reserve(token.size());
        for (char ch : token) {
            if (std::isalnum(static_cast<unsigned char>(ch))) {
                normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            }
        }
        if (normalized.size() < 3) {
            continue;
        }
        if (lower_content.find(normalized) != std::string::npos) {
            unique_terms.insert(normalized);
        }
    }

    matches.assign(unique_terms.begin(), unique_terms.end());
    return matches;
}

std::vector<QueryResult> SemanticSearchAPIHandlers::perform_keyword_search(const SemanticQuery& query_config) {
    std::vector<QueryResult> keyword_results;

    if (!db_conn_ || query_config.query_text.empty()) {
        return keyword_results;
    }

    try {
        auto keyword_start = std::chrono::high_resolution_clock::now();

        std::string sql = R"(
            SELECT 
                knowledge_id,
                key,
                content,
                content_type,
                COALESCE(array_to_json(tags), '[]')::text AS tags_json,
                COALESCE(confidence_score, 0.0) AS confidence_score
            FROM knowledge_base
            WHERE LOWER(key) LIKE LOWER($1) OR LOWER(content) LIKE LOWER($1)
            ORDER BY confidence_score DESC, last_updated DESC
            LIMIT $2
        )";

        std::string pattern = "%" + query_config.query_text + "%";
        auto raw_results = db_conn_->execute_query(sql, {pattern, std::to_string(query_config.max_results)});

        auto keyword_duration = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - keyword_start);

        if (raw_results.rows.empty()) {
            return keyword_results;
        }

        keyword_results.reserve(raw_results.rows.size());
        size_t rank = 0;
        for (const auto& row : raw_results.rows) {
            QueryResult result;
            auto id_it = row.find("knowledge_id");
            auto key_it = row.find("key");
            auto content_it = row.find("content");
            auto type_it = row.find("content_type");
            auto tags_it = row.find("tags_json");
            auto confidence_it = row.find("confidence_score");

            if (id_it == row.end() || key_it == row.end() || content_it == row.end()) {
                continue;
            }

            result.entity.entity_id = id_it->second;
            result.entity.title = key_it->second;
            result.entity.content = content_it->second;
            std::string content_type = (type_it != row.end()) ? type_it->second : "";
            result.entity.domain = domain_from_string(content_type);
            result.entity.knowledge_type = KnowledgeType::CONTEXT;
            result.entity.metadata = nlohmann::json::object();
            if (!content_type.empty()) {
                result.entity.metadata["content_type"] = content_type;
            }
            if (tags_it != row.end()) {
                result.entity.tags = parse_tag_array(tags_it->second);
                result.entity.metadata["tags"] = result.entity.tags;
            }

            if (confidence_it != row.end()) {
                try {
                    result.entity.confidence_score = std::stof(confidence_it->second);
                } catch (...) {
                    result.entity.confidence_score = 0.0f;
                }
            } else {
                result.entity.confidence_score = 0.0f;
            }

            result.matched_terms = extract_keyword_matches(query_config.query_text, result.entity.content);
            result.similarity_score = 0.0f; // will be adjusted during hybrid aggregation
            result.explanation = {
                {"match_type", "keyword"},
                {"rank", rank + 1},
                {"matched_terms", result.matched_terms}
            };
            result.query_time = keyword_duration;

            keyword_results.push_back(std::move(result));
            ++rank;
        }

    } catch (const std::exception& e) {
        spdlog::error("Keyword search failed: {}", e.what());
    }

    return keyword_results;
}

nlohmann::json SemanticSearchAPIHandlers::format_search_results(const std::vector<QueryResult>& results) {
    nlohmann::json response = nlohmann::json::array();

    for (const auto& result : results) {
        response.push_back(format_search_result(result));
    }

    return response;
}

nlohmann::json SemanticSearchAPIHandlers::format_search_result(const QueryResult& result) {
    nlohmann::json item;

    item["entity_id"] = result.entity.entity_id;
    item["title"] = result.entity.title;
    item["content"] = result.entity.content;
    item["similarity_score"] = result.similarity_score;
    item["confidence_score"] = result.entity.confidence_score;
    item["domain"] = domain_to_string(result.entity.domain);
    item["knowledge_type"] = knowledge_type_to_string(result.entity.knowledge_type);
    item["matched_terms"] = result.matched_terms;
    item["explanation"] = result.explanation;
    item["query_time_us"] = result.query_time.count();

    if (result.entity.metadata.is_object()) {
        item["metadata"] = result.entity.metadata;
    }

    if (!result.entity.tags.empty()) {
        item["tags"] = result.entity.tags;
    }

    return item;
}

bool SemanticSearchAPIHandlers::validate_search_request(const nlohmann::json& request, std::string& error_message) {
    if (!request.contains("query") || !request["query"].is_string()) {
        error_message = "Missing or invalid 'query' field";
        return false;
    }

    std::string query = request["query"];
    if (query.empty()) {
        error_message = "Query cannot be empty";
        return false;
    }

    if (query.length() > 1000) {
        error_message = "Query too long (maximum 1000 characters)";
        return false;
    }

    if (request.contains("max_results")) {
        int max_results = request.value("max_results", 10);
        if (max_results < 1 || max_results > 100) {
            error_message = "max_results must be between 1 and 100";
            return false;
        }
    }

    if (request.contains("similarity_threshold")) {
        double threshold = request.value("similarity_threshold", 0.7);
        if (threshold < 0.0 || threshold > 1.0) {
            error_message = "similarity_threshold must be between 0.0 and 1.0";
            return false;
        }
    }

    return true;
}

bool SemanticSearchAPIHandlers::validate_user_access(const std::string& user_id, const std::string& operation) {
    if (user_id.empty() || operation.empty()) {
        return false;
    }

    if (access_control_.is_admin(user_id)) {
        return true;
    }

    std::vector<AccessControlService::PermissionQuery> queries = {
        {operation, "semantic_search", "", 0},
        {operation, "knowledge_base", "", 0},
        {"use_semantic_search", "", "", 0},
        {operation, "", "", 0}
    };

    if (operation.find("stats") != std::string::npos) {
        queries.push_back({"view_semantic_search_stats", "", "", 0});
    }
    if (operation.find("config") != std::string::npos) {
        queries.push_back({"manage_semantic_search_config", "", "", 0});
    }

    return access_control_.has_any_permission(user_id, queries);
}

nlohmann::json SemanticSearchAPIHandlers::create_success_response(const nlohmann::json& data, const std::string& message) {
    nlohmann::json response = {
        {"success", true},
        {"status_code", 200}
    };

    if (!message.empty()) {
        response["message"] = message;
    }

    if (data.is_object() || data.is_array()) {
        response["data"] = data;
    }

    return response;
}

nlohmann::json SemanticSearchAPIHandlers::create_error_response(const std::string& message, int status_code) {
    return {
        {"success", false},
        {"status_code", status_code},
        {"error", message}
    };
}

nlohmann::json SemanticSearchAPIHandlers::create_paginated_response(const std::vector<nlohmann::json>& items,
                                                                  int total_count,
                                                                  int page,
                                                                  int page_size) {
    int total_pages = (total_count + page_size - 1) / page_size; // Ceiling division

    return {
        {"success", true},
        {"status_code", 200},
        {"data", {
            {"items", items},
            {"pagination", {
                {"page", page},
                {"page_size", page_size},
                {"total_count", total_count},
                {"total_pages", total_pages},
                {"has_next", page < total_pages},
                {"has_prev", page > 1}
            }}
        }}
    };
}

} // namespace regulens
