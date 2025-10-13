/**
 * Inter-Agent Communication API Handlers Implementation
 */

#include "inter_agent_api_handlers.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace regulens {

InterAgentAPIHandlers::InterAgentAPIHandlers(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<InterAgentCommunicator> communicator
) : db_conn_(db_conn), communicator_(communicator) {
    if (!db_conn_) {
        throw std::runtime_error("Database connection is required");
    }
    if (!communicator_) {
        throw std::runtime_error("InterAgentCommunicator is required");
    }
}

std::string InterAgentAPIHandlers::handle_send_message(
    const std::string& request_body,
    const std::string& user_id
) {
    try {
        nlohmann::json request;
        if (!validate_request_body(request_body, request)) {
            return create_error_response("Invalid JSON in request body").dump();
        }

        if (!validate_send_message_request(request)) {
            return create_error_response("Invalid message request format").dump();
        }

        // Extract message parameters
        std::string from_agent = request["from_agent"];
        std::string to_agent = request["to_agent"];
        std::string message_type = request["message_type"];
        nlohmann::json content = request["content"];

        int priority = request.value("priority", 3);
        std::optional<std::string> correlation_id;
        if (request.contains("correlation_id") && !request["correlation_id"].is_null()) {
            correlation_id = request["correlation_id"];
        }

        std::optional<std::string> conversation_id;
        if (request.contains("conversation_id") && !request["conversation_id"].is_null()) {
            conversation_id = request["conversation_id"];
        }

        std::optional<std::chrono::hours> expiry_hours;
        if (request.contains("expiry_hours") && request["expiry_hours"].is_number()) {
            expiry_hours = std::chrono::hours(request["expiry_hours"]);
        }

        // Authorize that user can send messages for this agent
        if (!authorize_agent_access(from_agent, user_id)) {
            return create_error_response("Unauthorized to send messages for this agent", 403).dump();
        }

        // Send the message
        auto message_id = communicator_->send_message(
            from_agent, to_agent, message_type, content,
            priority, correlation_id, conversation_id, expiry_hours
        );

        if (message_id) {
            nlohmann::json response = {
                {"success", true},
                {"message_id", *message_id},
                {"status", "sent"}
            };
            return response.dump();
        } else {
            return create_error_response("Failed to send message", 500).dump();
        }

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_send_message: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string InterAgentAPIHandlers::handle_receive_messages(
    const std::string& agent_id,
    const std::map<std::string, std::string>& query_params
) {
    try {
        // Parse query parameters
        int limit = 10;
        if (query_params.count("limit")) {
            try {
                limit = std::stoi(query_params.at("limit"));
                limit = std::max(1, std::min(limit, 100)); // Clamp between 1-100
            } catch (...) {
                limit = 10;
            }
        }

        std::optional<std::string> message_type;
        if (query_params.count("message_type")) {
            message_type = query_params.at("message_type");
        }

        // Get messages for the agent
        auto messages = communicator_->receive_messages(agent_id, limit, message_type);

        // Serialize messages
        nlohmann::json messages_json = nlohmann::json::array();
        for (const auto& message : messages) {
            messages_json.push_back(serialize_message(message));
        }

        return create_success_response({
            {"messages", messages_json},
            {"count", messages.size()},
            {"agent_id", agent_id}
        }).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_receive_messages: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string InterAgentAPIHandlers::handle_acknowledge_message(
    const std::string& message_id,
    const std::string& agent_id
) {
    try {
        if (message_id.empty() || agent_id.empty()) {
            return create_error_response("Message ID and agent ID are required").dump();
        }

        bool success = communicator_->acknowledge_message(message_id, agent_id);

        if (success) {
            return create_success_response({
                {"message_id", message_id},
                {"agent_id", agent_id},
                {"status", "acknowledged"}
            }).dump();
        } else {
            return create_error_response("Failed to acknowledge message - message not found or not authorized", 404).dump();
        }

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_acknowledge_message: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string InterAgentAPIHandlers::handle_broadcast_message(
    const std::string& request_body,
    const std::string& user_id
) {
    try {
        nlohmann::json request;
        if (!validate_request_body(request_body, request)) {
            return create_error_response("Invalid JSON in request body").dump();
        }

        // Extract broadcast parameters
        std::string from_agent = request["from_agent"];
        std::string message_type = request["message_type"];
        nlohmann::json content = request["content"];

        int priority = request.value("priority", 3);
        std::vector<std::string> excluded_agents;
        if (request.contains("excluded_agents") && request["excluded_agents"].is_array()) {
            for (const auto& agent : request["excluded_agents"]) {
                excluded_agents.push_back(agent);
            }
        }

        std::optional<std::string> correlation_id;
        if (request.contains("correlation_id") && !request["correlation_id"].is_null()) {
            correlation_id = request["correlation_id"];
        }

        std::optional<std::chrono::hours> expiry_hours;
        if (request.contains("expiry_hours") && request["expiry_hours"].is_number()) {
            expiry_hours = std::chrono::hours(request["expiry_hours"]);
        }

        // Authorize broadcast access
        if (!authorize_agent_access(from_agent, user_id)) {
            return create_error_response("Unauthorized to broadcast messages for this agent", 403).dump();
        }

        // Send broadcast
        bool success = communicator_->broadcast_message(
            from_agent, message_type, content, priority,
            excluded_agents, correlation_id, expiry_hours
        );

        if (success) {
            return create_success_response({
                {"status", "broadcast_sent"},
                {"from_agent", from_agent},
                {"message_type", message_type}
            }).dump();
        } else {
            return create_error_response("Failed to send broadcast message", 500).dump();
        }

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_broadcast_message: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string InterAgentAPIHandlers::handle_get_message_status(const std::string& message_id) {
    try {
        if (message_id.empty()) {
            return create_error_response("Message ID is required").dump();
        }

        // Query message status from database
        std::string query =
            "SELECT status, retry_count, error_message, created_at, delivered_at, acknowledged_at "
            "FROM agent_messages WHERE message_id = $1";

        std::vector<std::string> params = {message_id};
        auto result = db_conn_->execute_query(query, params);

        if (result.rows.empty()) {
            return create_error_response("Message not found", 404).dump();
        }

        const auto& row = result.rows[0];
        nlohmann::json status = {
            {"message_id", message_id},
            {"status", row.at("status")},
            {"retry_count", std::stoi(row.at("retry_count"))},
            {"created_at", row.at("created_at")}
        };

        auto error_msg_it = row.find("error_message");
        if (error_msg_it != row.end() && !error_msg_it->second.empty()) {
            status["error_message"] = error_msg_it->second;
        }

        auto delivered_it = row.find("delivered_at");
        if (delivered_it != row.end() && !delivered_it->second.empty()) {
            status["delivered_at"] = delivered_it->second;
        }

        auto acknowledged_it = row.find("acknowledged_at");
        if (acknowledged_it != row.end() && !acknowledged_it->second.empty()) {
            status["acknowledged_at"] = acknowledged_it->second;
        }

        return create_success_response(status).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_message_status: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string InterAgentAPIHandlers::handle_start_conversation(
    const std::string& request_body,
    const std::string& user_id
) {
    try {
        nlohmann::json request;
        if (!validate_request_body(request_body, request)) {
            return create_error_response("Invalid JSON in request body").dump();
        }

        if (!validate_conversation_request(request)) {
            return create_error_response("Invalid conversation request format").dump();
        }

        std::string topic = request["topic"];
        std::vector<std::string> participant_agents;
        for (const auto& agent : request["participant_agents"]) {
            participant_agents.push_back(agent);
        }

        std::string priority = request.value("priority", "normal");
        std::optional<nlohmann::json> metadata;
        if (request.contains("metadata") && !request["metadata"].is_null()) {
            metadata = request["metadata"];
        }

        std::optional<std::chrono::hours> expiry_hours;
        if (request.contains("expiry_hours") && request["expiry_hours"].is_number()) {
            expiry_hours = std::chrono::hours(request["expiry_hours"]);
        }

        auto conversation_id = communicator_->start_conversation(
            topic, participant_agents, priority, metadata, expiry_hours
        );

        if (conversation_id) {
            return create_success_response({
                {"conversation_id", *conversation_id},
                {"topic", topic},
                {"participant_count", participant_agents.size()},
                {"status", "active"}
            }).dump();
        } else {
            return create_error_response("Failed to start conversation", 500).dump();
        }

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_start_conversation: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string InterAgentAPIHandlers::handle_get_conversation_messages(
    const std::string& conversation_id,
    const std::map<std::string, std::string>& query_params
) {
    try {
        if (conversation_id.empty()) {
            return create_error_response("Conversation ID is required").dump();
        }

        int limit = 50;
        if (query_params.count("limit")) {
            try {
                limit = std::stoi(query_params.at("limit"));
                limit = std::max(1, std::min(limit, 500));
            } catch (...) {
                limit = 50;
            }
        }

        auto messages = communicator_->get_conversation_messages(conversation_id, limit);

        nlohmann::json messages_json = nlohmann::json::array();
        for (const auto& message : messages) {
            messages_json.push_back(serialize_message(message));
        }

        return create_success_response({
            {"conversation_id", conversation_id},
            {"messages", messages_json},
            {"count", messages.size()}
        }).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_conversation_messages: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string InterAgentAPIHandlers::handle_save_template(
    const std::string& request_body,
    const std::string& user_id
) {
    try {
        nlohmann::json request;
        if (!validate_request_body(request_body, request)) {
            return create_error_response("Invalid JSON in request body").dump();
        }

        if (!validate_template_request(request)) {
            return create_error_response("Invalid template request format").dump();
        }

        std::string template_name = request["template_name"];
        std::string message_type = request["message_type"];
        nlohmann::json template_content = request["template_content"];
        std::string description = request.value("description", "");

        bool success = communicator_->save_message_template(
            template_name, message_type, template_content, description, user_id
        );

        if (success) {
            return create_success_response({
                {"template_name", template_name},
                {"message_type", message_type},
                {"status", "saved"}
            }).dump();
        } else {
            return create_error_response("Failed to save message template", 500).dump();
        }

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_save_template: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string InterAgentAPIHandlers::handle_get_template(const std::string& template_name) {
    try {
        if (template_name.empty()) {
            return create_error_response("Template name is required").dump();
        }

        auto template_content = communicator_->get_message_template(template_name);

        if (template_content) {
            return create_success_response({
                {"template_name", template_name},
                {"template_content", *template_content}
            }).dump();
        } else {
            return create_error_response("Template not found", 404).dump();
        }

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_template: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string InterAgentAPIHandlers::handle_list_templates() {
    try {
        auto templates = communicator_->list_message_templates();

        return create_success_response({
            {"templates", templates},
            {"count", templates.size()}
        }).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_list_templates: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string InterAgentAPIHandlers::handle_get_communication_stats(
    const std::map<std::string, std::string>& query_params
) {
    try {
        std::optional<std::string> agent_id;
        if (query_params.count("agent_id")) {
            agent_id = query_params.at("agent_id");
        }

        std::optional<std::chrono::hours> hours_back;
        if (query_params.count("hours")) {
            try {
                int hours = std::stoi(query_params.at("hours"));
                hours_back = std::chrono::hours(hours);
            } catch (...) {
                // Use default
            }
        }

        auto stats = communicator_->get_communication_stats(agent_id, hours_back);

        return create_success_response({
            {"total_messages_sent", stats.total_messages_sent},
            {"total_messages_delivered", stats.total_messages_delivered},
            {"total_messages_failed", stats.total_messages_failed},
            {"pending_messages", stats.pending_messages},
            {"active_conversations", stats.active_conversations},
            {"delivery_success_rate", stats.delivery_success_rate}
        }).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_communication_stats: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string InterAgentAPIHandlers::handle_get_message_types() {
    try {
        auto message_types = communicator_->get_supported_message_types();

        nlohmann::json types_json = nlohmann::json::array();
        for (const auto& type : message_types) {
            auto schema = communicator_->get_message_type_schema(type);
            nlohmann::json type_info = {
                {"message_type", type}
            };
            if (schema) {
                type_info["schema"] = *schema;
            }
            types_json.push_back(type_info);
        }

        return create_success_response({
            {"message_types", types_json},
            {"count", message_types.size()}
        }).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_message_types: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string InterAgentAPIHandlers::handle_cleanup_expired() {
    try {
        bool success = communicator_->cleanup_expired_messages();

        if (success) {
            return create_success_response({
                {"status", "cleanup_completed"},
                {"message", "Expired messages have been cleaned up"}
            }).dump();
        } else {
            return create_error_response("Failed to cleanup expired messages", 500).dump();
        }

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_cleanup_expired: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

// Helper method implementations
nlohmann::json InterAgentAPIHandlers::create_success_response(
    const nlohmann::json& data,
    const std::string& message
) {
    nlohmann::json response = {
        {"success", true},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };

    if (!message.empty()) {
        response["message"] = message;
    }

    if (!data.is_null()) {
        response["data"] = data;
    }

    return response;
}

nlohmann::json InterAgentAPIHandlers::create_error_response(
    const std::string& error,
    int status_code,
    const std::string& details
) {
    nlohmann::json response = {
        {"success", false},
        {"error", error},
        {"status_code", status_code},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };

    if (!details.empty()) {
        response["details"] = details;
    }

    return response;
}

bool InterAgentAPIHandlers::validate_request_body(const std::string& body, nlohmann::json& parsed) {
    try {
        parsed = nlohmann::json::parse(body);
        return parsed.is_object();
    } catch (const std::exception&) {
        return false;
    }
}

nlohmann::json InterAgentAPIHandlers::serialize_message(const AgentMessage& message) {
    nlohmann::json json_msg = {
        {"message_id", message.message_id},
        {"from_agent_id", message.from_agent_id},
        {"message_type", message.message_type},
        {"content", message.content},
        {"priority", message.priority},
        {"status", message.status},
        {"retry_count", message.retry_count},
        {"created_at", std::chrono::duration_cast<std::chrono::milliseconds>(
            message.created_at.time_since_epoch()).count()}
    };

    if (message.to_agent_id) {
        json_msg["to_agent_id"] = *message.to_agent_id;
    }

    if (message.correlation_id) {
        json_msg["correlation_id"] = *message.correlation_id;
    }

    if (message.conversation_id) {
        json_msg["conversation_id"] = *message.conversation_id;
    }

    if (message.error_message) {
        json_msg["error_message"] = *message.error_message;
    }

    return json_msg;
}

bool InterAgentAPIHandlers::validate_send_message_request(const nlohmann::json& request) {
    return request.contains("from_agent") && request["from_agent"].is_string() &&
           request.contains("to_agent") && request["to_agent"].is_string() &&
           request.contains("message_type") && request["message_type"].is_string() &&
           request.contains("content") && request["content"].is_object();
}

bool InterAgentAPIHandlers::validate_conversation_request(const nlohmann::json& request) {
    return request.contains("topic") && request["topic"].is_string() &&
           request.contains("participant_agents") && request["participant_agents"].is_array() &&
           !request["participant_agents"].empty();
}

bool InterAgentAPIHandlers::validate_template_request(const nlohmann::json& request) {
    return request.contains("template_name") && request["template_name"].is_string() &&
           request.contains("message_type") && request["message_type"].is_string() &&
           request.contains("template_content") && request["template_content"].is_object();
}

bool InterAgentAPIHandlers::authorize_agent_access(const std::string& agent_id, const std::string& user_id) {
    // Basic authorization - in production, this would check user permissions
    // For now, allow access if user_id is not empty
    return !user_id.empty();
}

} // namespace regulens
