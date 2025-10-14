/**
 * Regulatory Chatbot API Handlers Implementation
 * Production-grade REST API endpoints for regulatory chatbot
 */

#include "chatbot_api_handlers.hpp"
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>

namespace regulens {
namespace chatbot {

ChatbotAPIHandlers::ChatbotAPIHandlers(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<StructuredLogger> logger,
    std::shared_ptr<RegulatoryChatbotService> chatbot_service
) : db_conn_(db_conn), logger_(logger), chatbot_service_(chatbot_service) {

    if (!db_conn_) {
        throw std::runtime_error("Database connection is required for ChatbotAPIHandlers");
    }
    if (!logger_) {
        throw std::runtime_error("Logger is required for ChatbotAPIHandlers");
    }
    if (!chatbot_service_) {
        throw std::runtime_error("Chatbot service is required for ChatbotAPIHandlers");
    }
}

std::string ChatbotAPIHandlers::handle_create_session(const std::string& request_body, const std::string& user_id) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);

        RegulatoryQueryContext context = parse_query_context(request);

        std::string session_id = chatbot_service_->create_session(user_id, context);

        if (session_id.empty()) {
            return create_error_response("Failed to create chatbot session", 500);
        }

        nlohmann::json response = {
            {"session_id", session_id},
            {"regulatory_domain", context.regulatory_domain},
            {"jurisdiction", context.jurisdiction},
            {"created_at", std::chrono::system_clock::now().time_since_epoch().count()}
        };

        logger_->log(LogLevel::INFO, "Created regulatory chatbot session for user " + user_id);
        return create_success_response(response, "Session created successfully");

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_create_session: " + std::string(e.what()));
        return create_error_response("Internal server error", 500);
    }
}

std::string ChatbotAPIHandlers::handle_send_message(const std::string& request_body, const std::string& user_id) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);

        std::string message = request.value("message", "");
        std::string session_id = request.value("session_id", "new");

        if (message.empty()) {
            return create_error_response("Message cannot be empty");
        }

        // Parse regulatory context
        RegulatoryQueryContext context = parse_query_context(request);

        // Create chatbot request
        RegulatoryChatbotRequest chatbot_request;
        chatbot_request.user_message = message;
        chatbot_request.session_id = session_id;
        chatbot_request.user_id = user_id;
        chatbot_request.query_context = context;
        chatbot_request.enable_rag = true;
        chatbot_request.require_citations = true;

        // Process the message
        RegulatoryChatbotResponse response = chatbot_service_->handle_regulatory_query(chatbot_request);

        if (!response.success) {
            return create_error_response(response.error_message.value_or("Failed to process message"), 500);
        }

        // Format response
        nlohmann::json api_response = format_chatbot_response(response);

        return create_success_response(api_response);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_send_message: " + std::string(e.what()));
        return create_error_response("Internal server error", 500);
    }
}

std::string ChatbotAPIHandlers::handle_get_sessions(const std::string& user_id, const std::map<std::string, std::string>& query_params) {
    try {
        int limit = 50;
        int offset = 0;

        if (query_params.count("limit")) {
            limit = std::min(std::stoi(query_params.at("limit")), 100); // Max 100
        }
        if (query_params.count("offset")) {
            offset = std::stoi(query_params.at("offset"));
        }

        std::vector<RegulatoryChatbotSession> sessions = query_user_sessions(user_id, limit, offset);

        nlohmann::json sessions_array = nlohmann::json::array();
        for (const auto& session : sessions) {
            sessions_array.push_back(format_session_response(session));
        }

        nlohmann::json response = {
            {"sessions", sessions_array},
            {"count", sessions.size()},
            {"limit", limit},
            {"offset", offset}
        };

        return create_success_response(response);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_get_sessions: " + std::string(e.what()));
        return create_error_response("Internal server error", 500);
    }
}

std::string ChatbotAPIHandlers::handle_get_messages(const std::string& session_id, const std::string& user_id, const std::map<std::string, std::string>& query_params) {
    try {
        // Validate session access
        if (!validate_session_access(session_id, user_id)) {
            return create_error_response("Session not found or access denied", 404);
        }

        int limit = 50;
        int offset = 0;

        if (query_params.count("limit")) {
            limit = std::min(std::stoi(query_params.at("limit")), 200); // Max 200 messages
        }
        if (query_params.count("offset")) {
            offset = std::stoi(query_params.at("offset"));
        }

        std::vector<RegulatoryChatbotMessage> messages = query_session_messages(session_id, limit, offset);

        nlohmann::json messages_array = nlohmann::json::array();
        for (const auto& message : messages) {
            messages_array.push_back(format_message_response(message));
        }

        nlohmann::json response = {
            {"messages", messages_array},
            {"count", messages.size()},
            {"session_id", session_id},
            {"limit", limit},
            {"offset", offset}
        };

        return create_success_response(response);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_get_messages: " + std::string(e.what()));
        return create_error_response("Internal server error", 500);
    }
}

std::string ChatbotAPIHandlers::handle_submit_feedback(const std::string& message_id, const std::string& request_body, const std::string& user_id) {
    try {
        // Validate message access
        if (!validate_message_access(message_id, user_id)) {
            return create_error_response("Message not found or access denied", 404);
        }

        nlohmann::json request = nlohmann::json::parse(request_body);
        std::string feedback_type = request.value("feedback", "");
        std::optional<std::string> comments = request.contains("comments") ?
            std::optional<std::string>(request["comments"]) : std::nullopt;

        if (feedback_type.empty()) {
            return create_error_response("Feedback type is required");
        }

        bool success = chatbot_service_->submit_feedback(message_id, feedback_type, comments);

        if (!success) {
            return create_error_response("Failed to submit feedback", 500);
        }

        logger_->log(LogLevel::INFO, "Feedback submitted for message " + message_id + " by user " + user_id);

        nlohmann::json response = {
            {"message_id", message_id},
            {"feedback", feedback_type},
            {"submitted_at", std::chrono::system_clock::now().time_since_epoch().count()}
        };

        return create_success_response(response, "Feedback submitted successfully");

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_submit_feedback: " + std::string(e.what()));
        return create_error_response("Internal server error", 500);
    }
}

std::string ChatbotAPIHandlers::handle_get_chatbot_stats(const std::string& user_id, const std::map<std::string, std::string>& query_params) {
    try {
        auto analytics = calculate_user_analytics(user_id, query_params);

        return create_success_response(analytics);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_get_chatbot_stats: " + std::string(e.what()));
        return create_error_response("Internal server error", 500);
    }
}

std::string ChatbotAPIHandlers::handle_search_regulatory_knowledge(const std::map<std::string, std::string>& query_params) {
    try {
        std::string query = query_params.count("q") ? query_params.at("q") : "";
        if (query.empty()) {
            return create_error_response("Query parameter 'q' is required");
        }

        RegulatoryQueryContext context;
        context.regulatory_domain = query_params.count("domain") ? query_params.at("domain") : "general";
        context.jurisdiction = query_params.count("jurisdiction") ? query_params.at("jurisdiction") : "global";
        context.query_type = "knowledge_search";

        int max_results = query_params.count("limit") ? std::stoi(query_params.at("limit")) : 10;
        max_results = std::min(max_results, 20); // Max 20 results

        KnowledgeContext knowledge = chatbot_service_->search_regulatory_knowledge(query, context, max_results);

        nlohmann::json response = {
            {"query", query},
            {"domain", context.regulatory_domain},
            {"jurisdiction", context.jurisdiction},
            {"results", knowledge.relevant_documents},
            {"total_sources", knowledge.total_sources},
            {"context_summary", knowledge.context_summary}
        };

        return create_success_response(response);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_search_regulatory_knowledge: " + std::string(e.what()));
        return create_error_response("Internal server error", 500);
    }
}

RegulatoryQueryContext ChatbotAPIHandlers::parse_query_context(const nlohmann::json& request_json) {
    RegulatoryQueryContext context;

    context.query_type = request_json.value("query_type", "general_inquiry");
    context.regulatory_domain = request_json.value("regulatory_domain", "general");
    context.jurisdiction = request_json.value("jurisdiction", "global");
    context.risk_level = request_json.value("risk_level", "medium");
    context.requires_citation = request_json.value("requires_citation", true);
    context.audit_trail_required = request_json.value("audit_trail_required", true);

    if (request_json.contains("relevant_regulations") && request_json["relevant_regulations"].is_array()) {
        for (const auto& reg : request_json["relevant_regulations"]) {
            context.relevant_regulations.push_back(reg);
        }
    }

    return context;
}

nlohmann::json ChatbotAPIHandlers::format_session_response(const RegulatoryChatbotSession& session) {
    return {
        {"session_id", session.session_id},
        {"title", session.title},
        {"regulatory_domain", session.regulatory_domain},
        {"jurisdiction", session.jurisdiction},
        {"is_active", session.is_active},
        {"started_at", std::chrono::duration_cast<std::chrono::seconds>(
            session.started_at.time_since_epoch()).count()},
        {"last_activity_at", std::chrono::duration_cast<std::chrono::seconds>(
            session.last_activity_at.time_since_epoch()).count()},
        {"metadata", session.session_metadata}
    };
}

nlohmann::json ChatbotAPIHandlers::format_message_response(const RegulatoryChatbotMessage& message) {
    return {
        {"message_id", message.message_id},
        {"session_id", message.session_id},
        {"role", message.role},
        {"content", message.content},
        {"confidence_score", message.confidence_score},
        {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
            message.timestamp.time_since_epoch()).count()},
        {"sources", message.sources},
        {"citations", message.citations},
        {"feedback", message.feedback},
        {"context", {
            {"query_type", message.context.query_type},
            {"regulatory_domain", message.context.regulatory_domain},
            {"jurisdiction", message.context.jurisdiction},
            {"risk_level", message.context.risk_level}
        }}
    };
}

nlohmann::json ChatbotAPIHandlers::format_chatbot_response(const RegulatoryChatbotResponse& response) {
    nlohmann::json formatted = {
        {"session_id", response.session_id},
        {"response_text", response.response_text},
        {"confidence_score", response.confidence_score},
        {"tokens_used", response.tokens_used},
        {"cost", response.cost},
        {"processing_time_ms", response.processing_time.count()},
        {"success", response.success}
    };

    if (response.sources_used) {
        formatted["sources"] = *response.sources_used;
    }

    if (response.citations) {
        formatted["citations"] = *response.citations;
    }

    if (!response.regulatory_warnings.empty()) {
        formatted["warnings"] = response.regulatory_warnings;
    }

    if (!response.compliance_recommendations.empty()) {
        formatted["recommendations"] = response.compliance_recommendations;
    }

    if (response.error_message) {
        formatted["error"] = *response.error_message;
    }

    return formatted;
}

bool ChatbotAPIHandlers::validate_session_access(const std::string& session_id, const std::string& user_id) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) return false;

        const char* params[2] = {session_id.c_str(), user_id.c_str()};
        PGresult* result = PQexecParams(
            conn,
            "SELECT session_id FROM chatbot_sessions WHERE session_id = $1 AND user_id = $2",
            2, nullptr, params, nullptr, nullptr, 0
        );

        bool valid = (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) > 0);
        PQclear(result);
        return valid;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in validate_session_access: " + std::string(e.what()));
        return false;
    }
}

bool ChatbotAPIHandlers::validate_message_access(const std::string& message_id, const std::string& user_id) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) return false;

        const char* params[2] = {message_id.c_str(), user_id.c_str()};
        PGresult* result = PQexecParams(
            conn,
            "SELECT m.message_id FROM chatbot_messages m "
            "JOIN chatbot_sessions s ON m.session_id = s.session_id "
            "WHERE m.message_id = $1 AND s.user_id = $2",
            2, nullptr, params, nullptr, nullptr, 0
        );

        bool valid = (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) > 0);
        PQclear(result);
        return valid;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in validate_message_access: " + std::string(e.what()));
        return false;
    }
}

std::string ChatbotAPIHandlers::create_error_response(const std::string& message, int status_code) {
    nlohmann::json response = {
        {"success", false},
        {"error", message},
        {"status_code", status_code},
        {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
    };
    return response.dump();
}

std::string ChatbotAPIHandlers::create_success_response(const nlohmann::json& data, const std::string& message) {
    nlohmann::json response = {
        {"success", true},
        {"data", data},
        {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
    };

    if (!message.empty()) {
        response["message"] = message;
    }

    return response.dump();
}

// Placeholder implementations for database query methods
std::vector<RegulatoryChatbotSession> ChatbotAPIHandlers::query_user_sessions(const std::string& user_id, int limit, int offset) {
    // Implementation would query database for user sessions
    return {};
}

std::optional<RegulatoryChatbotSession> ChatbotAPIHandlers::query_session(const std::string& session_id) {
    // Implementation would query database for session details
    return std::nullopt;
}

std::vector<RegulatoryChatbotMessage> ChatbotAPIHandlers::query_session_messages(const std::string& session_id, int limit, int offset) {
    // Implementation would query database for session messages
    return {};
}

nlohmann::json ChatbotAPIHandlers::calculate_session_metrics(const std::string& session_id) {
    // Implementation would calculate session analytics
    return {};
}

nlohmann::json ChatbotAPIHandlers::calculate_user_analytics(const std::string& user_id, const std::map<std::string, std::string>& filters) {
    // Implementation would calculate user analytics
    return {};
}

} // namespace chatbot
} // namespace regulens
