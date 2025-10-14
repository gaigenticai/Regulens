/**
 * Regulatory Chatbot API Handlers
 * REST API endpoints for regulatory chatbot functionality
 */

#ifndef REGULENS_CHATBOT_API_HANDLERS_HPP
#define REGULENS_CHATBOT_API_HANDLERS_HPP

#include <string>
#include <map>
#include <memory>
#include <libpq-fe.h>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"
#include "regulatory_chatbot_service.hpp"

namespace regulens {
namespace chatbot {

class ChatbotAPIHandlers {
public:
    ChatbotAPIHandlers(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<StructuredLogger> logger,
        std::shared_ptr<RegulatoryChatbotService> chatbot_service
    );

    // Session management
    std::string handle_create_session(const std::string& request_body, const std::string& user_id);
    std::string handle_get_sessions(const std::string& user_id, const std::map<std::string, std::string>& query_params);
    std::string handle_get_session(const std::string& session_id, const std::string& user_id);
    std::string handle_archive_session(const std::string& session_id, const std::string& user_id);

    // Message handling
    std::string handle_send_message(const std::string& request_body, const std::string& user_id);
    std::string handle_get_messages(const std::string& session_id, const std::string& user_id, const std::map<std::string, std::string>& query_params);

    // Feedback and ratings
    std::string handle_submit_feedback(const std::string& message_id, const std::string& request_body, const std::string& user_id);

    // Analytics and reporting
    std::string handle_get_chatbot_stats(const std::string& user_id, const std::map<std::string, std::string>& query_params);
    std::string handle_get_session_analytics(const std::string& session_id, const std::string& user_id);

    // Knowledge base queries (read-only)
    std::string handle_search_regulatory_knowledge(const std::map<std::string, std::string>& query_params);

    // Citation management
    std::string handle_get_citations(const std::string& message_id, const std::string& user_id);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<RegulatoryChatbotService> chatbot_service_;

    // Helper methods
    RegulatoryQueryContext parse_query_context(const nlohmann::json& request_json);
    nlohmann::json format_session_response(const RegulatoryChatbotSession& session);
    nlohmann::json format_message_response(const RegulatoryChatbotMessage& message);
    nlohmann::json format_chatbot_response(const RegulatoryChatbotResponse& response);

    bool validate_session_access(const std::string& session_id, const std::string& user_id);
    bool validate_message_access(const std::string& message_id, const std::string& user_id);

    std::string create_error_response(const std::string& message, int status_code = 400);
    std::string create_success_response(const nlohmann::json& data, const std::string& message = "");

    // Database query helpers
    std::vector<RegulatoryChatbotSession> query_user_sessions(const std::string& user_id, int limit, int offset);
    std::optional<RegulatoryChatbotSession> query_session(const std::string& session_id);
    std::vector<RegulatoryChatbotMessage> query_session_messages(const std::string& session_id, int limit, int offset);

    // Analytics helpers
    nlohmann::json calculate_session_metrics(const std::string& session_id);
    nlohmann::json calculate_user_analytics(const std::string& user_id, const std::map<std::string, std::string>& filters);
};

} // namespace chatbot
} // namespace regulens

#endif // REGULENS_CHATBOT_API_HANDLERS_HPP
