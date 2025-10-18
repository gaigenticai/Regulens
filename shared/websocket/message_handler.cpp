/**
 * WebSocket Message Handler Implementation - Week 3 Phase 1
 * Production-grade message processing and creation
 */

#include "message_handler.hpp"
#include "../logging/logger.hpp"
#include <uuid/uuid.h>
#include <chrono>
#include <iostream>

namespace regulens {
namespace websocket {

MessageHandler::MessageHandler() = default;

MessageHandler::~MessageHandler() = default;

WebSocketMessage MessageHandler::parse_message(const std::string& raw_message) {
  auto logger = logging::get_logger("message_handler");
  
  WebSocketMessage msg;
  msg.message_id = generate_message_id();
  msg.timestamp = std::chrono::system_clock::now();
  
  try {
    auto json_msg = parse_json(raw_message);
    
    if (json_msg.contains("type")) {
      std::string type_str = json_msg["type"];
      if (type_str == "CONNECTION_ESTABLISHED") msg.type = MessageType::CONNECTION_ESTABLISHED;
      else if (type_str == "HEARTBEAT") msg.type = MessageType::HEARTBEAT;
      else if (type_str == "SUBSCRIBE") msg.type = MessageType::SUBSCRIBE;
      else if (type_str == "UNSUBSCRIBE") msg.type = MessageType::UNSUBSCRIBE;
      else if (type_str == "BROADCAST") msg.type = MessageType::BROADCAST;
      else if (type_str == "DIRECT_MESSAGE") msg.type = MessageType::DIRECT_MESSAGE;
      else if (type_str == "SESSION_UPDATE") msg.type = MessageType::SESSION_UPDATE;
      else if (type_str == "RULE_EVALUATION_RESULT") msg.type = MessageType::RULE_EVALUATION_RESULT;
      else if (type_str == "DECISION_ANALYSIS_RESULT") msg.type = MessageType::DECISION_ANALYSIS_RESULT;
      else if (type_str == "CONSENSUS_UPDATE") msg.type = MessageType::CONSENSUS_UPDATE;
      else if (type_str == "LEARNING_FEEDBACK") msg.type = MessageType::LEARNING_FEEDBACK;
      else if (type_str == "ALERT") msg.type = MessageType::ALERT;
      else msg.type = MessageType::ERROR;
    }
    
    if (json_msg.contains("sender_id")) msg.sender_id = json_msg["sender_id"];
    if (json_msg.contains("recipient_id")) msg.recipient_id = json_msg["recipient_id"];
    if (json_msg.contains("payload")) msg.payload = json_msg["payload"];
    if (json_msg.contains("requires_acknowledgment")) {
      msg.requires_acknowledgment = json_msg["requires_acknowledgment"];
    }
    
  } catch (const std::exception& e) {
    logger->error("Failed to parse message: {}", e.what());
    msg.type = MessageType::ERROR;
  }
  
  return msg;
}

std::string MessageHandler::serialize_message(const WebSocketMessage& message) {
  json j;
  
  j["message_id"] = message.message_id;
  j["sender_id"] = message.sender_id;
  j["recipient_id"] = message.recipient_id;
  j["payload"] = message.payload;
  j["requires_acknowledgment"] = message.requires_acknowledgment;
  
  switch (message.type) {
    case MessageType::CONNECTION_ESTABLISHED:
      j["type"] = "CONNECTION_ESTABLISHED";
      break;
    case MessageType::HEARTBEAT:
      j["type"] = "HEARTBEAT";
      break;
    case MessageType::SUBSCRIBE:
      j["type"] = "SUBSCRIBE";
      break;
    case MessageType::UNSUBSCRIBE:
      j["type"] = "UNSUBSCRIBE";
      break;
    case MessageType::BROADCAST:
      j["type"] = "BROADCAST";
      break;
    case MessageType::DIRECT_MESSAGE:
      j["type"] = "DIRECT_MESSAGE";
      break;
    case MessageType::SESSION_UPDATE:
      j["type"] = "SESSION_UPDATE";
      break;
    case MessageType::RULE_EVALUATION_RESULT:
      j["type"] = "RULE_EVALUATION_RESULT";
      break;
    case MessageType::DECISION_ANALYSIS_RESULT:
      j["type"] = "DECISION_ANALYSIS_RESULT";
      break;
    case MessageType::CONSENSUS_UPDATE:
      j["type"] = "CONSENSUS_UPDATE";
      break;
    case MessageType::LEARNING_FEEDBACK:
      j["type"] = "LEARNING_FEEDBACK";
      break;
    case MessageType::ALERT:
      j["type"] = "ALERT";
      break;
    case MessageType::ERROR:
      j["type"] = "ERROR";
      break;
  }
  
  return j.dump();
}

bool MessageHandler::validate_message(const WebSocketMessage& message) {
  if (message.message_id.empty()) return false;
  if (message.sender_id.empty()) return false;
  
  return true;
}

void MessageHandler::handle_subscription(const WebSocketMessage& message,
                                        std::shared_ptr<WebSocketConnection> connection) {
  auto logger = logging::get_logger("message_handler");
  
  if (!message.payload.contains("channel")) {
    logger->warn("Subscription message missing channel");
    return;
  }
  
  std::string channel = message.payload["channel"];
  logger->debug("Subscription request for channel: {}", channel);
}

void MessageHandler::handle_broadcast(const WebSocketMessage& message,
                                     std::shared_ptr<WebSocketServer> server) {
  auto logger = logging::get_logger("message_handler");
  logger->debug("Broadcast message from {}", message.sender_id);
}

void MessageHandler::handle_direct_message(const WebSocketMessage& message,
                                          std::shared_ptr<WebSocketServer> server) {
  auto logger = logging::get_logger("message_handler");
  logger->debug("Direct message to {}", message.recipient_id);
}

void MessageHandler::handle_heartbeat(const WebSocketMessage& message,
                                     std::shared_ptr<WebSocketConnection> connection) {
  if (connection) {
    connection->last_heartbeat = std::chrono::system_clock::now();
  }
}

WebSocketMessage MessageHandler::create_connection_established_message(
    const std::string& connection_id) {
  WebSocketMessage msg;
  msg.message_id = generate_message_id();
  msg.type = MessageType::CONNECTION_ESTABLISHED;
  msg.sender_id = "system";
  msg.timestamp = std::chrono::system_clock::now();
  msg.payload["connection_id"] = connection_id;
  msg.payload["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
  
  return msg;
}

WebSocketMessage MessageHandler::create_heartbeat_message() {
  WebSocketMessage msg;
  msg.message_id = generate_message_id();
  msg.type = MessageType::HEARTBEAT;
  msg.sender_id = "system";
  msg.timestamp = std::chrono::system_clock::now();
  
  return msg;
}

WebSocketMessage MessageHandler::create_error_message(const std::string& error_text) {
  WebSocketMessage msg;
  msg.message_id = generate_message_id();
  msg.type = MessageType::ERROR;
  msg.sender_id = "system";
  msg.timestamp = std::chrono::system_clock::now();
  msg.payload["error"] = error_text;
  
  return msg;
}

WebSocketMessage MessageHandler::create_rule_evaluation_result_message(
    const std::string& rule_id,
    const json& result) {
  WebSocketMessage msg;
  msg.message_id = generate_message_id();
  msg.type = MessageType::RULE_EVALUATION_RESULT;
  msg.sender_id = "system";
  msg.timestamp = std::chrono::system_clock::now();
  msg.payload["rule_id"] = rule_id;
  msg.payload["result"] = result;
  msg.requires_acknowledgment = true;
  
  return msg;
}

WebSocketMessage MessageHandler::create_decision_analysis_result_message(
    const std::string& analysis_id,
    const json& result) {
  WebSocketMessage msg;
  msg.message_id = generate_message_id();
  msg.type = MessageType::DECISION_ANALYSIS_RESULT;
  msg.sender_id = "system";
  msg.timestamp = std::chrono::system_clock::now();
  msg.payload["analysis_id"] = analysis_id;
  msg.payload["result"] = result;
  msg.requires_acknowledgment = true;
  
  return msg;
}

WebSocketMessage MessageHandler::create_consensus_update_message(
    const std::string& session_id,
    const json& consensus_data) {
  WebSocketMessage msg;
  msg.message_id = generate_message_id();
  msg.type = MessageType::CONSENSUS_UPDATE;
  msg.sender_id = "system";
  msg.timestamp = std::chrono::system_clock::now();
  msg.payload["session_id"] = session_id;
  msg.payload["consensus"] = consensus_data;
  msg.requires_acknowledgment = true;
  
  return msg;
}

WebSocketMessage MessageHandler::create_learning_feedback_message(
    const std::string& feedback_id,
    const json& feedback_data) {
  WebSocketMessage msg;
  msg.message_id = generate_message_id();
  msg.type = MessageType::LEARNING_FEEDBACK;
  msg.sender_id = "system";
  msg.timestamp = std::chrono::system_clock::now();
  msg.payload["feedback_id"] = feedback_id;
  msg.payload["feedback"] = feedback_data;
  
  return msg;
}

WebSocketMessage MessageHandler::create_alert_message(
    const std::string& alert_type,
    const json& alert_data) {
  WebSocketMessage msg;
  msg.message_id = generate_message_id();
  msg.type = MessageType::ALERT;
  msg.sender_id = "system";
  msg.timestamp = std::chrono::system_clock::now();
  msg.payload["alert_type"] = alert_type;
  msg.payload["alert"] = alert_data;
  msg.requires_acknowledgment = true;
  
  return msg;
}

json MessageHandler::parse_json(const std::string& json_str) {
  return json::parse(json_str);
}

std::string MessageHandler::generate_message_id() {
  uuid_t uuid;
  uuid_generate(uuid);
  
  char uuid_str[37];
  uuid_unparse(uuid, uuid_str);
  
  return std::string(uuid_str);
}

}  // namespace websocket
}  // namespace regulens
