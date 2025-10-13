/**
 * LLM Integration API Handlers - Header File
 * Production-grade LLM endpoint declarations
 * Uses OpenAIClient, AnthropicClient, and EmbeddingsClient
 */

#ifndef REGULENS_LLM_API_HANDLERS_HPP
#define REGULENS_LLM_API_HANDLERS_HPP

#include <string>
#include <map>
#include <memory>
#include <libpq-fe.h>
#include "openai_client.hpp"
#include "anthropic_client.hpp"
#include "embeddings_client.hpp"

namespace regulens {
namespace llm {

// Initialize LLM clients (should be called at startup)
bool initialize_llm_clients(
    std::shared_ptr<OpenAIClient> openai,
    std::shared_ptr<AnthropicClient> anthropic,
    std::shared_ptr<EmbeddingsClient> embeddings
);

// Get shared client instances
std::shared_ptr<OpenAIClient> get_openai_client();
std::shared_ptr<AnthropicClient> get_anthropic_client();
std::shared_ptr<EmbeddingsClient> get_embeddings_client();

// Model Management
std::string get_llm_model_by_id(
    PGconn* db_conn,
    const std::string& model_id
);

// Text Analysis
std::string analyze_text_with_llm(
    PGconn* db_conn,
    const std::string& request_body,
    const std::string& user_id
);

// Conversation Management
std::string get_llm_conversations(
    PGconn* db_conn,
    const std::map<std::string, std::string>& query_params,
    const std::string& user_id
);

std::string get_llm_conversation_by_id(
    PGconn* db_conn,
    const std::string& conversation_id
);

std::string create_llm_conversation(
    PGconn* db_conn,
    const std::string& request_body,
    const std::string& user_id
);

std::string add_message_to_conversation(
    PGconn* db_conn,
    const std::string& conversation_id,
    const std::string& request_body,
    const std::string& user_id
);

std::string delete_llm_conversation(
    PGconn* db_conn,
    const std::string& conversation_id
);

// Usage Statistics
std::string get_llm_usage_statistics(
    PGconn* db_conn,
    const std::map<std::string, std::string>& query_params,
    const std::string& user_id
);

// Batch Processing
std::string create_llm_batch_job(
    PGconn* db_conn,
    const std::string& request_body,
    const std::string& user_id
);

std::string get_llm_batch_job_status(
    PGconn* db_conn,
    const std::string& job_id
);

// Fine-tuning
std::string create_fine_tune_job(
    PGconn* db_conn,
    const std::string& request_body,
    const std::string& user_id
);

std::string get_fine_tune_job_status(
    PGconn* db_conn,
    const std::string& job_id
);

// Cost Estimation
std::string estimate_llm_cost(
    PGconn* db_conn,
    const std::string& request_body
);

// Benchmarks
std::string get_llm_model_benchmarks(
    PGconn* db_conn,
    const std::map<std::string, std::string>& query_params
);

} // namespace llm
} // namespace regulens

#endif // REGULENS_LLM_API_HANDLERS_HPP

