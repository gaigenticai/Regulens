/**
 * WebSocket Message Handler - Week 3 Phase 1
 * Message parsing, validation, and routing
 */

#pragma once

#include "websocket_server.hpp"
#include <string>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>

namespace regulens {
namespace websocket {

using json = nlohmann::json;

class MessageHandler {
public:
  MessageHandler();
  ~MessageHandler();

  // Parse incoming message
  WebSocketMessage parse_message(const std::string& raw_message);
  
  // Serialize message to JSON
  std::string serialize_message(const WebSocketMessage& message);
  
  // Validate message structure
  bool validate_message(const WebSocketMessage& message);
  
  // Handle specific message types
  void handle_subscription(const WebSocketMessage& message, 
                          std::shared_ptr<WebSocketConnection> connection);
  void handle_broadcast(const WebSocketMessage& message,
                       std::shared_ptr<WebSocketServer> server);
  void handle_direct_message(const WebSocketMessage& message,
                            std::shared_ptr<WebSocketServer> server);
  void handle_heartbeat(const WebSocketMessage& message,
                       std::shared_ptr<WebSocketConnection> connection);
  
  // Create specific message types
  WebSocketMessage create_connection_established_message(
      const std::string& connection_id);
  WebSocketMessage create_heartbeat_message();
  WebSocketMessage create_error_message(const std::string& error_text);
  WebSocketMessage create_rule_evaluation_result_message(
      const std::string& rule_id,
      const json& result);
  WebSocketMessage create_decision_analysis_result_message(
      const std::string& analysis_id,
      const json& result);
  WebSocketMessage create_consensus_update_message(
      const std::string& session_id,
      const json& consensus_data);
  WebSocketMessage create_learning_feedback_message(
      const std::string& feedback_id,
      const json& feedback_data);
  WebSocketMessage create_alert_message(
      const std::string& alert_type,
      const json& alert_data);

private:
  json parse_json(const std::string& json_str);
  std::string generate_message_id();
};

}  // namespace websocket
}  // namespace regulens
