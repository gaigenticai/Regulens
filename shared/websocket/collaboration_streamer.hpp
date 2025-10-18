/**
 * Collaboration Streamer - Week 3 Phase 2
 * Real-time collaboration session streaming and updates
 */

#pragma once

#include "websocket_server.hpp"
#include "message_handler.hpp"
#include <memory>
#include <nlohmann/json.hpp>

namespace regulens {
namespace websocket {

using json = nlohmann::json;

class CollaborationStreamer {
public:
  CollaborationStreamer(std::shared_ptr<WebSocketServer> ws_server,
                       std::shared_ptr<MessageHandler> msg_handler);
  ~CollaborationStreamer();

  // Session streaming
  void stream_session_state(const std::string& session_id, const json& session_data);
  void stream_participant_joined(const std::string& session_id, const json& participant);
  void stream_participant_left(const std::string& session_id, const std::string& participant_id);
  void stream_participant_status(const std::string& session_id, const json& status_update);
  
  // Real-time activity
  void stream_activity_message(const std::string& session_id, const json& message);
  void stream_decision_update(const std::string& session_id, const json& decision_data);
  void stream_rule_evaluation(const std::string& session_id, const json& eval_data);
  
  // Consensus and voting
  void stream_consensus_initiated(const std::string& session_id, const json& consensus_data);
  void stream_vote_cast(const std::string& session_id, const std::string& voter_id, const json& vote);
  void stream_consensus_update(const std::string& session_id, const json& consensus_state);
  void stream_consensus_result(const std::string& session_id, const json& result);
  
  // Learning and feedback
  void stream_learning_feedback(const std::string& session_id, const json& feedback);
  void stream_learning_update(const std::string& session_id, const json& update);
  
  // Alerts and notifications
  void stream_alert(const std::string& session_id, const json& alert_data);
  void stream_notification(const std::string& user_id, const json& notification);
  
  // Broadcast to all session participants
  void broadcast_to_session(const std::string& session_id, const WebSocketMessage& message);
  
  // Send to specific participant
  void send_to_participant(const std::string& session_id, const std::string& participant_id,
                          const WebSocketMessage& message);

private:
  std::shared_ptr<WebSocketServer> ws_server_;
  std::shared_ptr<MessageHandler> msg_handler_;
  
  std::vector<std::string> get_session_participants(const std::string& session_id);
};

}  // namespace websocket
}  // namespace regulens
