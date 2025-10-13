/**
 * Knowledge Base API Handlers - Header File
 * Production-grade knowledge management endpoint declarations
 * Uses KnowledgeBase and VectorKnowledgeBase for RAG and semantic search
 */

#ifndef REGULENS_KNOWLEDGE_API_HANDLERS_HPP
#define REGULENS_KNOWLEDGE_API_HANDLERS_HPP

#include <string>
#include <map>
#include <memory>
#include <libpq-fe.h>
#include "../knowledge_base.hpp"
#include "vector_knowledge_base.hpp"
#include "../llm/embeddings_client.hpp"

namespace regulens {
namespace knowledge {

// Initialize knowledge base engines (should be called at startup)
bool initialize_knowledge_engines(
    std::shared_ptr<KnowledgeBase> kb,
    std::shared_ptr<VectorKnowledgeBase> vector_kb,
    std::shared_ptr<EmbeddingsClient> embeddings
);

// Get shared engine instances
std::shared_ptr<KnowledgeBase> get_knowledge_base();
std::shared_ptr<VectorKnowledgeBase> get_vector_knowledge_base();
std::shared_ptr<EmbeddingsClient> get_embeddings_client();

// Knowledge Entry CRUD
std::string create_knowledge_entry(
    PGconn* db_conn,
    const std::string& request_body,
    const std::string& user_id
);

std::string update_knowledge_entry(
    PGconn* db_conn,
    const std::string& entry_id,
    const std::string& request_body
);

std::string delete_knowledge_entry(
    PGconn* db_conn,
    const std::string& entry_id
);

// Similarity Search
std::string get_similar_entries(
    PGconn* db_conn,
    const std::string& entry_id,
    const std::map<std::string, std::string>& query_params
);

// Case Management
std::string get_knowledge_cases(
    PGconn* db_conn,
    const std::map<std::string, std::string>& query_params
);

std::string get_knowledge_case(
    PGconn* db_conn,
    const std::string& case_id
);

// RAG Q&A
std::string ask_knowledge_base(
    PGconn* db_conn,
    const std::string& request_body,
    const std::string& user_id
);

// Embeddings Generation
std::string generate_embeddings(
    PGconn* db_conn,
    const std::string& request_body,
    const std::string& user_id
);

} // namespace knowledge
} // namespace regulens

#endif // REGULENS_KNOWLEDGE_API_HANDLERS_HPP

