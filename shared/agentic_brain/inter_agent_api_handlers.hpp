/**
 * Inter-Agent Communication API Handlers
 * REST API endpoints for agent communication
 */

#ifndef INTER_AGENT_API_HANDLERS_HPP
#define INTER_AGENT_API_HANDLERS_HPP

#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include "inter_agent_communicator.hpp"

class PostgreSQLConnection; // Forward declaration

namespace regulens {

class InterAgentAPIHandlers {
public:
    InterAgentAPIHandlers(std::shared_ptr<PostgreSQLConnection> db_conn,
                         std::shared_ptr<InterAgentCommunicator> communicator);

    // Message endpoints
    std::string handle_send_message(const std::string& request_body,
                                   const std::string& user_id);

    std::string handle_receive_messages(const std::string& agent_id,
                                       const std::map<std::string, std::string>& query_params);

    std::string handle_acknowledge_message(const std::string& message_id,
                                          const std::string& agent_id);

    std::string handle_broadcast_message(const std::string& request_body,
                                        const std::string& user_id);

    std::string handle_get_message_status(const std::string& message_id);

    // Conversation endpoints
    std::string handle_start_conversation(const std::string& request_body,
                                         const std::string& user_id);

    std::string handle_get_conversation_messages(const std::string& conversation_id,
                                                const std::map<std::string, std::string>& query_params);

    std::string handle_add_to_conversation(const std::string& message_id,
                                          const std::string& conversation_id);

    // Template endpoints
    std::string handle_save_template(const std::string& request_body,
                                    const std::string& user_id);

    std::string handle_get_template(const std::string& template_name);

    std::string handle_list_templates();

    // Statistics endpoints
    std::string handle_get_communication_stats(const std::map<std::string, std::string>& query_params);

    std::string handle_get_agent_stats(const std::string& agent_id,
                                      const std::map<std::string, std::string>& query_params);

    // Message type endpoints
    std::string handle_get_message_types();

    std::string handle_validate_message(const std::string& message_type,
                                       const std::string& request_body);

    // Maintenance endpoints
    std::string handle_cleanup_expired();

    std::string handle_retry_failed_messages();

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<InterAgentCommunicator> communicator_;

    // Helper methods
    nlohmann::json create_success_response(const nlohmann::json& data = nullptr,
                                          const std::string& message = "");

    nlohmann::json create_error_response(const std::string& error,
                                        int status_code = 400,
                                        const std::string& details = "");

    bool validate_request_body(const std::string& body, nlohmann::json& parsed);

    std::optional<std::string> extract_agent_id_from_token(const std::string& auth_header);

    bool authorize_agent_access(const std::string& agent_id, const std::string& user_id);

    // Message serialization
    nlohmann::json serialize_message(const AgentMessage& message);

    nlohmann::json serialize_conversation_stats(const std::string& conversation_id);

    // Input validation
    bool validate_send_message_request(const nlohmann::json& request);

    bool validate_conversation_request(const nlohmann::json& request);

    bool validate_template_request(const nlohmann::json& request);
};

} // namespace regulens

#endif // INTER_AGENT_API_HANDLERS_HPP
