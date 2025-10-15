/**
 * Regulatory Chatbot Service
 * Specialized chatbot for regulatory compliance Q&A with audit trail
 * Extends base ChatbotService with regulatory-specific functionality
 */

#ifndef REGULENS_REGULATORY_CHATBOT_SERVICE_HPP
#define REGULENS_REGULATORY_CHATBOT_SERVICE_HPP

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <chrono>
#include <nlohmann/json.hpp>
#include "../database/postgresql_connection.hpp"
#include "../knowledge_base/vector_knowledge_base.hpp"
#include "../llm/openai_client.hpp"
#include "../llm/chatbot_service.hpp"
#include "../logging/structured_logger.hpp"

namespace regulens {
namespace chatbot {

struct RegulatoryQueryContext {
    std::string query_type; // 'compliance_question', 'regulatory_interpretation', 'policy_clarification', 'audit_preparation'
    std::string regulatory_domain; // 'aml', 'kyc', 'fraud', 'data_privacy', 'financial_reporting'
    std::string jurisdiction; // 'us', 'eu', 'uk', 'global'
    std::string risk_level; // 'low', 'medium', 'high', 'critical'
    std::vector<std::string> relevant_regulations; // List of regulation codes
    bool requires_citation = true;
    bool audit_trail_required = true;
};

struct RegulatoryChatbotMessage {
    std::string message_id;
    std::string session_id;
    std::string role; // 'user', 'assistant', 'system'
    std::string content;
    std::chrono::system_clock::time_point timestamp;
    double confidence_score = 0.0;
    std::optional<nlohmann::json> sources;
    std::optional<nlohmann::json> citations;
    std::string feedback; // 'helpful', 'not_helpful', 'partially_helpful'
    RegulatoryQueryContext context;
};

struct RegulatoryChatbotSession {
    std::string session_id;
    std::string user_id;
    std::string title;
    std::string regulatory_domain;
    std::string jurisdiction;
    bool audit_mode = true;
    std::vector<std::string> accessed_regulations;
    std::chrono::system_clock::time_point started_at;
    std::chrono::system_clock::time_point last_activity_at;
    bool is_active = true;
    nlohmann::json session_metadata;
};

struct RegulatoryChatbotRequest {
    std::string user_message;
    std::string session_id; // "new" for new sessions
    std::string user_id;
    RegulatoryQueryContext query_context;
    std::optional<std::string> model_override;
    bool enable_rag = true;
    bool require_citations = true;
    int max_context_messages = 15;
};

struct RegulatoryChatbotResponse {
    std::string response_text;
    std::string session_id;
    double confidence_score = 0.0;
    int tokens_used = 0;
    double cost = 0.0;
    std::chrono::milliseconds processing_time;
    std::optional<nlohmann::json> sources_used;
    std::optional<nlohmann::json> citations;
    std::vector<std::string> regulatory_warnings;
    std::vector<std::string> compliance_recommendations;
    std::optional<std::string> error_message;
    bool success = true;
};

class RegulatoryChatbotService {
public:
    RegulatoryChatbotService(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<VectorKnowledgeBase> knowledge_base,
        std::shared_ptr<OpenAIClient> openai_client,
        std::shared_ptr<StructuredLogger> logger
    );

    ~RegulatoryChatbotService();

    // Core regulatory chatbot functionality
    RegulatoryChatbotResponse handle_regulatory_query(const RegulatoryChatbotRequest& request);

    // Session management
    std::optional<RegulatoryChatbotSession> get_session(const std::string& session_id);
    std::vector<RegulatoryChatbotSession> get_user_sessions(const std::string& user_id, int limit = 50);
    std::string create_session(const std::string& user_id, const RegulatoryQueryContext& context);
    bool archive_session(const std::string& session_id);
    bool update_session_activity(const std::string& session_id);

    // Regulatory knowledge retrieval
    KnowledgeContext search_regulatory_knowledge(
        const std::string& query,
        const RegulatoryQueryContext& context,
        int max_results = 10
    );

    // Citation and audit trail
    std::vector<nlohmann::json> cite_sources(
        const std::string& message_id,
        const std::vector<nlohmann::json>& sources
    );

    bool store_audit_trail(
        const std::string& session_id,
        const std::string& message_id,
        const RegulatoryChatbotRequest& request,
        const RegulatoryChatbotResponse& response
    );

    // Message management
    std::vector<RegulatoryChatbotMessage> get_session_messages(
        const std::string& session_id,
        int limit = 100,
        int offset = 0
    );

    bool submit_feedback(
        const std::string& message_id,
        const std::string& feedback_type,
        const std::optional<std::string>& comments = std::nullopt
    );

    // Regulatory compliance validation
    std::vector<std::string> validate_response_compliance(
        const std::string& response_text,
        const RegulatoryQueryContext& context
    );

    std::vector<std::string> generate_compliance_recommendations(
        const RegulatoryQueryContext& context,
        const std::vector<nlohmann::json>& relevant_sources
    );

    // Configuration
    void set_regulatory_focus_domains(const std::vector<std::string>& domains);
    void set_audit_trail_enabled(bool enabled);
    void set_minimum_confidence_threshold(double threshold);
    void set_citation_required(bool required);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<VectorKnowledgeBase> knowledge_base_;
    std::shared_ptr<OpenAIClient> openai_client_;
    std::shared_ptr<StructuredLogger> logger_;

    // Configuration
    std::vector<std::string> regulatory_focus_domains_ = {"aml", "kyc", "fraud", "compliance"};
    bool audit_trail_enabled_ = true;
    double min_confidence_threshold_ = 0.75;
    bool citations_required_ = true;
    int max_session_messages_ = 15;
    std::string default_model_ = "gpt-4-turbo-preview";

    // Internal methods
    std::string build_regulatory_system_prompt(
        const RegulatoryQueryContext& context,
        const KnowledgeContext& knowledge_context
    );

    RegulatoryChatbotResponse generate_regulatory_response(
        const std::vector<ChatbotMessage>& conversation_history,
        const KnowledgeContext& knowledge_context,
        const RegulatoryChatbotRequest& request
    );

    std::vector<std::string> extract_regulatory_entities(const std::string& text);
    std::vector<std::string> identify_risk_indicators(const std::string& query);

    std::string store_regulatory_message(
        const std::string& session_id,
        const std::string& role,
        const std::string& content,
        const RegulatoryQueryContext& context,
        int token_count,
        double cost,
        double confidence_score,
        const std::optional<nlohmann::json>& sources,
        const std::optional<nlohmann::json>& citations,
        std::chrono::milliseconds processing_time
    );

    std::string generate_session_title(const std::string& first_message, const RegulatoryQueryContext& context);
    std::string generate_uuid();

    // Regulatory knowledge filtering
    KnowledgeContext filter_regulatory_context(
        const KnowledgeContext& context,
        const RegulatoryQueryContext& query_context
    );

    // Compliance validation
    bool contains_disclaimer_language(const std::string& response);
    std::vector<std::string> check_regulatory_warnings(const std::string& response, const RegulatoryQueryContext& context);

    // Audit trail methods
    bool log_access_to_regulation(const std::string& session_id, const std::string& regulation_code);
    bool log_citation_usage(const std::string& message_id, const std::vector<nlohmann::json>& citations);

    // Utility methods
    RegulatoryChatbotResponse create_error_response(const std::string& error_message);
    std::string format_citations_for_display(const std::vector<nlohmann::json>& citations);
    double calculate_regulatory_confidence(
        const std::vector<nlohmann::json>& sources,
        const RegulatoryQueryContext& context
    );
    double calculate_message_cost(const std::string& model, int input_tokens, int output_tokens);
    std::vector<nlohmann::json> build_citation_previews(const std::vector<nlohmann::json>& sources) const;
    std::vector<nlohmann::json> fetch_message_citations(const std::string& message_id);
    std::chrono::system_clock::time_point parse_timestamp(const std::string& timestamp) const;
};

} // namespace chatbot
} // namespace regulens

#endif // REGULENS_REGULATORY_CHATBOT_SERVICE_HPP
