/**
 * Collaboration Streamer Implementation - Week 3 Phase 2
 * Real-time session streaming for collaborative features
 */

#include "collaboration_streamer.hpp"
#include "../logging/logger.hpp"

namespace regulens {
namespace websocket {

CollaborationStreamer::CollaborationStreamer(std::shared_ptr<WebSocketServer> ws_server,
                                             std::shared_ptr<MessageHandler> msg_handler)
    : ws_server_(ws_server), msg_handler_(msg_handler) {
  auto logger = logging::get_logger("collaboration_streamer");
  logger->info("CollaborationStreamer initialized");
}

CollaborationStreamer::~CollaborationStreamer() = default;

void CollaborationStreamer::stream_session_state(const std::string& session_id,
                                                 const json& session_data) {
  auto logger = logging::get_logger("collaboration_streamer");
  
  WebSocketMessage msg;
  msg.message_id = session_id;
  msg.type = MessageType::SESSION_UPDATE;
  msg.sender_id = "system";
  msg.payload["session_id"] = session_id;
  msg.payload["state"] = session_data;
  msg.requires_acknowledgment = true;
  
  broadcast_to_session(session_id, msg);
  logger->debug("Session state streamed: {}", session_id);
}

void CollaborationStreamer::stream_participant_joined(const std::string& session_id,
                                                      const json& participant) {
  auto logger = logging::get_logger("collaboration_streamer");
  
  WebSocketMessage msg;
  msg.type = MessageType::SESSION_UPDATE;
  msg.sender_id = "system";
  msg.payload["event"] = "participant_joined";
  msg.payload["session_id"] = session_id;
  msg.payload["participant"] = participant;
  
  broadcast_to_session(session_id, msg);
  logger->debug("Participant joined: {}", session_id);
}

void CollaborationStreamer::stream_participant_left(const std::string& session_id,
                                                    const std::string& participant_id) {
  auto logger = logging::get_logger("collaboration_streamer");
  
  WebSocketMessage msg;
  msg.type = MessageType::SESSION_UPDATE;
  msg.sender_id = "system";
  msg.payload["event"] = "participant_left";
  msg.payload["session_id"] = session_id;
  msg.payload["participant_id"] = participant_id;
  
  broadcast_to_session(session_id, msg);
  logger->debug("Participant left: {}", session_id);
}

void CollaborationStreamer::stream_participant_status(const std::string& session_id,
                                                      const json& status_update) {
  WebSocketMessage msg;
  msg.type = MessageType::SESSION_UPDATE;
  msg.sender_id = "system";
  msg.payload["event"] = "status_update";
  msg.payload["session_id"] = session_id;
  msg.payload["status"] = status_update;
  
  broadcast_to_session(session_id, msg);
}

void CollaborationStreamer::stream_activity_message(const std::string& session_id,
                                                    const json& message) {
  WebSocketMessage msg;
  msg.type = MessageType::SESSION_UPDATE;
  msg.sender_id = "system";
  msg.payload["event"] = "activity_message";
  msg.payload["session_id"] = session_id;
  msg.payload["activity"] = message;
  
  broadcast_to_session(session_id, msg);
}

void CollaborationStreamer::stream_decision_update(const std::string& session_id,
                                                   const json& decision_data) {
  auto logger = logging::get_logger("collaboration_streamer");
  
  WebSocketMessage msg;
  msg.type = MessageType::DECISION_ANALYSIS_RESULT;
  msg.sender_id = "system";
  msg.payload["session_id"] = session_id;
  msg.payload["decision"] = decision_data;
  msg.requires_acknowledgment = true;
  
  broadcast_to_session(session_id, msg);
  logger->debug("Decision update streamed: {}", session_id);
}

void CollaborationStreamer::stream_rule_evaluation(const std::string& session_id,
                                                   const json& eval_data) {
  auto logger = logging::get_logger("collaboration_streamer");
  
  WebSocketMessage msg;
  msg.type = MessageType::RULE_EVALUATION_RESULT;
  msg.sender_id = "system";
  msg.payload["session_id"] = session_id;
  msg.payload["evaluation"] = eval_data;
  msg.requires_acknowledgment = true;
  
  broadcast_to_session(session_id, msg);
  logger->debug("Rule evaluation streamed: {}", session_id);
}

void CollaborationStreamer::stream_consensus_initiated(const std::string& session_id,
                                                       const json& consensus_data) {
  auto logger = logging::get_logger("collaboration_streamer");
  
  WebSocketMessage msg;
  msg.type = MessageType::CONSENSUS_UPDATE;
  msg.sender_id = "system";
  msg.payload["session_id"] = session_id;
  msg.payload["event"] = "consensus_initiated";
  msg.payload["consensus"] = consensus_data;
  msg.requires_acknowledgment = true;
  
  broadcast_to_session(session_id, msg);
  logger->debug("Consensus initiated: {}", session_id);
}

void CollaborationStreamer::stream_vote_cast(const std::string& session_id,
                                             const std::string& voter_id,
                                             const json& vote) {
  auto logger = logging::get_logger("collaboration_streamer");
  
  WebSocketMessage msg;
  msg.type = MessageType::CONSENSUS_UPDATE;
  msg.sender_id = "system";
  msg.payload["session_id"] = session_id;
  msg.payload["event"] = "vote_cast";
  msg.payload["voter_id"] = voter_id;
  msg.payload["vote"] = vote;
  msg.requires_acknowledgment = false;
  
  broadcast_to_session(session_id, msg);
  logger->debug("Vote cast: {} in {}", voter_id, session_id);
}

void CollaborationStreamer::stream_consensus_update(const std::string& session_id,
                                                    const json& consensus_state) {
  auto logger = logging::get_logger("collaboration_streamer");
  
  WebSocketMessage msg;
  msg.type = MessageType::CONSENSUS_UPDATE;
  msg.sender_id = "system";
  msg.payload["session_id"] = session_id;
  msg.payload["event"] = "consensus_update";
  msg.payload["state"] = consensus_state;
  msg.requires_acknowledgment = true;
  
  broadcast_to_session(session_id, msg);
  logger->debug("Consensus update: {}", session_id);
}

void CollaborationStreamer::stream_consensus_result(const std::string& session_id,
                                                    const json& result) {
  auto logger = logging::get_logger("collaboration_streamer");
  
  WebSocketMessage msg;
  msg.type = MessageType::CONSENSUS_UPDATE;
  msg.sender_id = "system";
  msg.payload["session_id"] = session_id;
  msg.payload["event"] = "consensus_result";
  msg.payload["result"] = result;
  msg.requires_acknowledgment = true;
  
  broadcast_to_session(session_id, msg);
  logger->info("Consensus result finalized: {}", session_id);
}

void CollaborationStreamer::stream_learning_feedback(const std::string& session_id,
                                                     const json& feedback) {
  auto logger = logging::get_logger("collaboration_streamer");
  
  WebSocketMessage msg;
  msg.type = MessageType::LEARNING_FEEDBACK;
  msg.sender_id = "system";
  msg.payload["session_id"] = session_id;
  msg.payload["feedback"] = feedback;
  msg.requires_acknowledgment = false;
  
  broadcast_to_session(session_id, msg);
  logger->debug("Learning feedback streamed: {}", session_id);
}

void CollaborationStreamer::stream_learning_update(const std::string& session_id,
                                                   const json& update) {
  auto logger = logging::get_logger("collaboration_streamer");
  
  WebSocketMessage msg;
  msg.type = MessageType::SESSION_UPDATE;
  msg.sender_id = "system";
  msg.payload["session_id"] = session_id;
  msg.payload["event"] = "learning_update";
  msg.payload["update"] = update;
  msg.requires_acknowledgment = false;
  
  broadcast_to_session(session_id, msg);
  logger->debug("Learning update streamed: {}", session_id);
}

void CollaborationStreamer::stream_alert(const std::string& session_id,
                                         const json& alert_data) {
  auto logger = logging::get_logger("collaboration_streamer");
  
  WebSocketMessage msg;
  msg.type = MessageType::ALERT;
  msg.sender_id = "system";
  msg.payload["session_id"] = session_id;
  msg.payload["alert"] = alert_data;
  msg.requires_acknowledgment = true;
  
  broadcast_to_session(session_id, msg);
  logger->debug("Alert streamed: {}", session_id);
}

void CollaborationStreamer::stream_notification(const std::string& user_id,
                                                const json& notification) {
  auto logger = logging::get_logger("collaboration_streamer");
  
  WebSocketMessage msg;
  msg.type = MessageType::ALERT;
  msg.sender_id = "system";
  msg.payload["notification"] = notification;
  msg.requires_acknowledgment = false;
  
  ws_server_->send_to_user(user_id, msg);
  logger->debug("Notification sent to user: {}", user_id);
}

void CollaborationStreamer::broadcast_to_session(const std::string& session_id,
                                                 const WebSocketMessage& message) {
  std::vector<std::string> subscriptions = {session_id};
  ws_server_->send_to_subscriptions(subscriptions, message);
}

void CollaborationStreamer::send_to_participant(const std::string& session_id,
                                                const std::string& participant_id,
                                                const WebSocketMessage& message) {
  ws_server_->send_to_user(participant_id, message);
}

std::vector<std::string> CollaborationStreamer::get_session_participants(
    const std::string& session_id) {
  auto subscribers = ws_server_->get_subscribers(session_id);
  
  std::vector<std::string> participant_ids;
  for (const auto& connection : subscribers) {
    participant_ids.push_back(connection->user_id);
  }
  
  return participant_ids;
}

}  // namespace websocket
}  // namespace regulens
