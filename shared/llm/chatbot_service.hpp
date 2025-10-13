/**
 * GPT-4 Chatbot Service with RAG Integration
 * Production-grade conversational AI with knowledge base retrieval
 */

#ifndef CHATBOT_SERVICE_HPP
#define CHATBOT_SERVICE_HPP

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <chrono>
#include <nlohmann/json.hpp>
#include "../database/postgresql_connection.hpp"
#include "../knowledge_base/vector_knowledge_base.hpp"
#include "openai_client.hpp"

namespace regulens {

struct ChatbotMessage {
    std::string role; // "user", "assistant", "system"
    std::string content;
    int token_count = 0;
    std::optional<nlohmann::json> sources_used;
    double confidence_score = 0.0;
};

struct ChatbotConversation {
    std::string conversation_id;
    std::string user_id;
    std::string platform = "web";
    std::string title;
    std::vector<ChatbotMessage> messages;
    int total_tokens = 0;
    double total_cost = 0.0;
    bool is_active = true;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_message_at;
};

struct ChatbotRequest {
    std::string user_message;
    std::string conversation_id; // "new" for new conversations
    std::string user_id;
    std::string platform = "web";
    std::optional<std::string> model_override;
    bool enable_rag = true;
    int max_context_messages = 10; // Sliding window size
};

struct ChatbotResponse {
    std::string response_text;
    std::string conversation_id;
    double confidence_score = 0.0;
    int tokens_used = 0;
    double cost = 0.0;
    std::chrono::milliseconds processing_time;
    std::optional<nlohmann::json> sources_used;
    std::optional<std::string> error_message;
    bool success = true;
};

struct KnowledgeContext {
    std::vector<nlohmann::json> relevant_documents;
    std::vector<double> relevance_scores;
    std::string context_summary;
    int total_sources = 0;
};

class ChatbotService {
public:
    ChatbotService(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<VectorKnowledgeBase> knowledge_base,
        std::shared_ptr<OpenAIClient> openai_client
    );

    ~ChatbotService();

    // Core chatbot functionality
    ChatbotResponse process_message(const ChatbotRequest& request);

    // Conversation management
    std::optional<ChatbotConversation> get_conversation(const std::string& conversation_id);
    std::vector<ChatbotConversation> get_user_conversations(const std::string& user_id, int limit = 50);
    bool archive_conversation(const std::string& conversation_id);
    bool delete_conversation(const std::string& conversation_id);

    // RAG (Retrieval-Augmented Generation)
    KnowledgeContext retrieve_relevant_context(const std::string& query, int max_results = 5);

    // Rate limiting and cost management
    struct UsageLimits {
        int max_requests_per_hour = 100;
        int max_tokens_per_hour = 10000;
        double max_cost_per_day = 10.0;
    };

    bool check_rate_limits(const std::string& user_id, const UsageLimits& limits);
    void record_usage(const std::string& user_id, int tokens_used, double cost);

    // Configuration
    void set_default_model(const std::string& model);
    void set_knowledge_retrieval_enabled(bool enabled);
    void set_max_context_length(int max_messages);
    void set_usage_limits(const UsageLimits& limits);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<VectorKnowledgeBase> knowledge_base_;
    std::shared_ptr<OpenAIClient> openai_client_;

    // Configuration
    std::string default_model_ = "gpt-4-turbo-preview";
    bool rag_enabled_ = true;
    int max_context_messages_ = 10;
    UsageLimits usage_limits_;
    int knowledge_cache_ttl_hours_ = 24;

    // Internal methods
    std::string create_or_get_conversation(const ChatbotRequest& request);
    std::vector<ChatbotMessage> get_conversation_history(const std::string& conversation_id, int limit);
    std::string build_system_prompt(const KnowledgeContext& context);

    ChatbotResponse generate_gpt4_response(
        const std::vector<ChatbotMessage>& conversation_history,
        const KnowledgeContext& context,
        const ChatbotRequest& request
    );

    void store_message(
        const std::string& conversation_id,
        const std::string& role,
        const std::string& content,
        int token_count,
        double cost,
        const std::string& model_used,
        double confidence_score = 0.0,
        const std::optional<nlohmann::json>& sources_used = std::nullopt,
        std::chrono::milliseconds processing_time = std::chrono::milliseconds(0)
    );

    void update_conversation_stats(const std::string& conversation_id, int tokens_used, double cost);

    // Knowledge retrieval caching
    std::optional<KnowledgeContext> get_cached_context(const std::string& query_hash);
    void cache_context(const std::string& query_hash, const std::string& query, const KnowledgeContext& context);

    // Cost calculation
    double calculate_message_cost(const std::string& model, int input_tokens, int output_tokens);
    std::pair<int, int> estimate_token_count(const std::string& text);

    // Utility methods
    std::string generate_conversation_title(const std::string& first_message);
    std::string generate_uuid();
    std::string hash_string(const std::string& input);
    bool validate_conversation_access(const std::string& conversation_id, const std::string& user_id);

    // Fallback responses for API failures
    ChatbotResponse create_fallback_response(const std::string& error_message);
    std::string get_fallback_response(const std::string& user_message);
};

} // namespace regulens

#endif // CHATBOT_SERVICE_HPP
