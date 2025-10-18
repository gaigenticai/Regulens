/**
 * WebSocket API Endpoints - Week 3 Phase 5
 * REST endpoints for WebSocket management and monitoring
 */

#include "websocket_endpoints.hpp"
#include "../logging/logger.hpp"
#include "../websocket/websocket_server.hpp"
#include <nlohmann/json.hpp>

namespace regulens {
namespace api_registry {

using json = nlohmann::json;

class WebSocketAPIHandlers {
public:
  WebSocketAPIHandlers(std::shared_ptr<websocket::WebSocketServer> ws_server)
      : ws_server_(ws_server) {}

  // WebSocket upgrade endpoint
  json handle_websocket_upgrade(const std::string& user_id, const std::string& session_id) {
    auto logger = logging::get_logger("websocket_endpoints");

    auto connection = ws_server_->create_connection(user_id, session_id);
    if (!ws_server_->add_connection(connection)) {
      logger->warn("Failed to add WebSocket connection for user: {}", user_id);
      return json{{"error", "Connection pool full"}};
    }

    logger->info("WebSocket connection established for user: {}", user_id);

    return json{
      {"success", true},
      {"connection_id", connection->connection_id},
      {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()},
    };
  }

  // Subscribe to channel
  json handle_subscribe(const std::string& connection_id, const std::string& channel) {
    auto logger = logging::get_logger("websocket_endpoints");

    if (!ws_server_->subscribe(connection_id, channel)) {
      logger->warn("Failed to subscribe connection {} to channel {}", connection_id, channel);
      return json{{"error", "Subscription failed"}};
    }

    logger->debug("Connection {} subscribed to channel {}", connection_id, channel);

    return json{
      {"success", true},
      {"channel", channel},
    };
  }

  // Unsubscribe from channel
  json handle_unsubscribe(const std::string& connection_id, const std::string& channel) {
    auto logger = logging::get_logger("websocket_endpoints");

    if (!ws_server_->unsubscribe(connection_id, channel)) {
      logger->warn("Failed to unsubscribe connection {} from channel {}", connection_id, channel);
      return json{{"error", "Unsubscription failed"}};
    }

    logger->debug("Connection {} unsubscribed from channel {}", connection_id, channel);

    return json{
      {"success", true},
      {"channel", channel},
    };
  }

  // Broadcast message
  json handle_broadcast(const websocket::WebSocketMessage& message) {
    auto logger = logging::get_logger("websocket_endpoints");

    ws_server_->broadcast_message(message);

    logger->debug("Message broadcasted from {}", message.sender_id);

    return json{
      {"success", true},
      {"message_id", message.message_id},
    };
  }

  // Send direct message
  json handle_direct_message(const std::string& connection_id,
                            const websocket::WebSocketMessage& message) {
    auto logger = logging::get_logger("websocket_endpoints");

    ws_server_->send_to_connection(connection_id, message);

    logger->debug("Direct message sent from {} to {}", message.sender_id, connection_id);

    return json{
      {"success", true},
      {"message_id", message.message_id},
      {"recipient_id", connection_id},
    };
  }

  // Get connection status
  json handle_get_connection_status(const std::string& connection_id) {
    auto connection = ws_server_->get_connection(connection_id);

    if (!connection) {
      return json{{"error", "Connection not found"}};
    }

    return json{
      {"connection_id", connection->connection_id},
      {"user_id", connection->user_id},
      {"state", static_cast<int>(connection->state)},
      {"connected_at", connection->connected_at.time_since_epoch().count()},
      {"messages_sent", connection->messages_sent},
      {"messages_received", connection->messages_received},
    };
  }

  // Get server stats
  json handle_get_server_stats() {
    auto stats = ws_server_->get_stats();

    return json{
      {"total_connections", stats.total_connections},
      {"active_connections", stats.active_connections},
      {"authenticated_connections", stats.authenticated_connections},
      {"total_messages_processed", stats.total_messages_processed},
      {"total_messages_sent", stats.total_messages_sent},
      {"average_latency_ms", stats.average_latency_ms},
      {"uptime", stats.uptime.time_since_epoch().count()},
    };
  }

  // Disconnect connection
  json handle_disconnect(const std::string& connection_id) {
    auto logger = logging::get_logger("websocket_endpoints");

    if (!ws_server_->remove_connection(connection_id)) {
      logger->warn("Failed to disconnect connection: {}", connection_id);
      return json{{"error", "Disconnection failed"}};
    }

    logger->info("Connection disconnected: {}", connection_id);

    return json{{"success", true}};
  }

private:
  std::shared_ptr<websocket::WebSocketServer> ws_server_;
};

}  // namespace api_registry
}  // namespace regulens
