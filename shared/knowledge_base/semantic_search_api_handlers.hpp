/**
 * Semantic Search API Handlers
 * Production-grade API endpoints for vector-based knowledge retrieval
 */

#ifndef SEMANTIC_SEARCH_API_HANDLERS_HPP
#define SEMANTIC_SEARCH_API_HANDLERS_HPP

#include <memory>
#include <string>
#include <nlohmann/json.hpp>
#include "vector_knowledge_base.hpp"
#include "../database/postgresql_connection.hpp"

namespace regulens {

/**
 * API handlers for semantic search operations
 */
class SemanticSearchAPIHandlers {
public:
    SemanticSearchAPIHandlers(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<VectorKnowledgeBase> knowledge_base
    );

    ~SemanticSearchAPIHandlers();

    // Semantic Search Endpoints
    std::string handle_semantic_search(const std::string& request_body, const std::string& user_id);
    std::string handle_hybrid_search(const std::string& request_body, const std::string& user_id);
    std::string handle_similar_entities(const std::string& entity_id, const std::string& user_id);
    std::string handle_search_by_domain(const std::string& domain, const std::string& request_body, const std::string& user_id);

    // Search Configuration Endpoints
    std::string handle_get_search_config();
    std::string handle_update_search_config(const std::string& request_body);

    // Search Analytics Endpoints
    std::string handle_get_search_stats(const std::string& time_range);
    std::string handle_get_popular_queries(const std::string& limit);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<VectorKnowledgeBase> knowledge_base_;

    // Helper methods
    SemanticQuery parse_search_request(const nlohmann::json& request);
    nlohmann::json format_search_results(const std::vector<QueryResult>& results);
    nlohmann::json format_search_result(const QueryResult& result);

    // Validation methods
    bool validate_search_request(const nlohmann::json& request, std::string& error_message);
    bool validate_user_access(const std::string& user_id, const std::string& operation);

    // Response formatting
    nlohmann::json create_success_response(const nlohmann::json& data = nullptr,
                                         const std::string& message = "");
    nlohmann::json create_error_response(const std::string& message,
                                       int status_code = 400);
    nlohmann::json create_paginated_response(const std::vector<nlohmann::json>& items,
                                           int total_count,
                                           int page,
                                           int page_size);
};

} // namespace regulens

#endif // SEMANTIC_SEARCH_API_HANDLERS_HPP
