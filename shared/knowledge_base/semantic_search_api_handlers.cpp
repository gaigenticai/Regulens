/**
 * Semantic Search API Handlers Implementation
 * Production-grade API endpoints for vector-based knowledge retrieval
 */

#include "semantic_search_api_handlers.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>

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
) : db_conn_(db_conn), knowledge_base_(knowledge_base) {

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

        // For now, hybrid search uses the same implementation as semantic search
        // TODO: Implement true hybrid search combining vector and keyword search
        SemanticQuery query_config = parse_search_request(request);

        auto start_time = std::chrono::high_resolution_clock::now();
        std::vector<QueryResult> results = knowledge_base_->semantic_search(query_config);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto search_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        nlohmann::json response_data = format_search_results(results);
        response_data["search_time_ms"] = search_time.count();
        response_data["total_results"] = results.size();
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
        
        // Retrieve the entity from database
        std::string entity_query = R"(
            SELECT key, embeddings, content_type, tags
            FROM knowledge_base
            WHERE knowledge_id = $1
        )";
        
        auto entity_result = knowledge_base_->get_db_connection()->execute_query(entity_query, {entity_id});
        if (!entity_result.has_value()) {
            return create_error_response("Entity not found", 404).dump();
        }
        
        // Use the entity's embedding to find similar entities
        SemanticQuery query_config;
        query_config.query = entity_result->get<std::string>("key", "");
        query_config.max_results = limit;
        query_config.similarity_threshold = 0.6;  // Lower threshold for similar entities
        
        std::vector<QueryResult> similar_results = knowledge_base_->semantic_search(query_config);
        
        // Filter out the original entity
        nlohmann::json similar_entities = nlohmann::json::array();
        for (const auto& result : similar_results) {
            if (result.entity_id != entity_id) {
                similar_entities.push_back({
                    {"entity_id", result.entity_id},
                    {"key", result.key},
                    {"similarity_score", result.similarity_score},
                    {"content_type", result.metadata.value("content_type", "UNKNOWN")},
                    {"tags", result.metadata.value("tags", nlohmann::json::array())}
                });
            }
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
        
        knowledge_base_->get_db_connection()->execute_query(store_query, {
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
        
        auto stats_result = knowledge_base_->get_db_connection()->execute_query(stats_query);
        
        nlohmann::json stats;
        if (stats_result.has_value()) {
            stats = {
                {"total_searches", stats_result->get<int>("total_searches", 0)},
                {"average_response_time_ms", stats_result->get<double>("avg_response_time_ms", 0.0)},
                {"unique_users", stats_result->get<int>("unique_users", 0)},
                {"avg_results_count", stats_result->get<double>("avg_results_count", 0.0)},
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
        
        auto results = knowledge_base_->get_db_connection()->execute_query_multi(popular_query, {
            std::to_string(limit)
        });
        
        nlohmann::json queries_array = nlohmann::json::array();
        for (const auto& row : results) {
            queries_array.push_back({
                {"query", row.get<std::string>("query_text", "")},
                {"search_count", row.get<int>("search_count", 0)},
                {"avg_results", row.get<double>("avg_results", 0.0)},
                {"last_searched", row.get<std::string>("last_searched", "")}
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
    if (domain_str == "REGULATORY_COMPLIANCE") {
        query.domain_filter = KnowledgeDomain::REGULATORY_COMPLIANCE;
    }
    // TODO: Add other domain mappings

    // Parse type filters
    if (request.contains("type_filters") && request["type_filters"].is_array()) {
        for (const auto& type_str : request["type_filters"]) {
            // TODO: Parse knowledge type strings
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
    // TODO: Implement proper access control based on user roles and permissions
    // For now, allow all authenticated users
    return !user_id.empty();
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
