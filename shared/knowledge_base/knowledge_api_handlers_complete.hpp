/**
 * Knowledge Base API Handlers - Complete Implementation Header File
 * Production-grade knowledge management endpoint declarations
 * Implements RAG, semantic search, and knowledge CRUD operations
 */

#ifndef REGULENS_KNOWLEDGE_API_HANDLERS_COMPLETE_HPP
#define REGULENS_KNOWLEDGE_API_HANDLERS_COMPLETE_HPP

#include <string>
#include <map>
#include <memory>
#include <libpq-fe.h>

namespace regulens {
namespace knowledge {

// Knowledge Search and Discovery
std::string search_knowledge_base(PGconn* db_conn, const std::map<std::string, std::string>& query_params);
std::string get_knowledge_entries(PGconn* db_conn, const std::map<std::string, std::string>& query_params);
std::string get_knowledge_entry_by_id(PGconn* db_conn, const std::string& entry_id);
std::string create_knowledge_entry(PGconn* db_conn, const std::string& request_body, const std::string& user_id);
std::string update_knowledge_entry(PGconn* db_conn, const std::string& entry_id, const std::string& request_body);
std::string delete_knowledge_entry(PGconn* db_conn, const std::string& entry_id);

// Similarity and Related Content
std::string get_similar_entries(PGconn* db_conn, const std::string& entry_id, const std::map<std::string, std::string>& query_params);

// Case Management
std::string get_knowledge_cases(PGconn* db_conn, const std::map<std::string, std::string>& query_params);
std::string get_knowledge_case_by_id(PGconn* db_conn, const std::string& case_id);

// RAG Q&A System
std::string ask_knowledge_base(PGconn* db_conn, const std::string& request_body, const std::string& user_id);

// Embeddings and Vector Operations
std::string generate_embeddings(PGconn* db_conn, const std::string& request_body, const std::string& user_id);
std::string reindex_knowledge(PGconn* db_conn, const std::string& request_body, const std::string& user_id);

// Knowledge Statistics
std::string get_knowledge_stats(PGconn* db_conn, const std::map<std::string, std::string>& query_params);

// Helper functions
std::string vector_search(PGconn* db_conn, const std::string& query_text, int top_k = 10);
std::string hybrid_search(PGconn* db_conn, const std::string& query_text, const std::map<std::string, std::string>& filters);
double calculate_similarity(const std::vector<double>& vec1, const std::vector<double>& vec2);
std::vector<double> generate_embedding(const std::string& text);

} // namespace knowledge
} // namespace regulens

#endif // REGULENS_KNOWLEDGE_API_HANDLERS_COMPLETE_HPP