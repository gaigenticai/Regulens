/**
 * WebSocket Server - Week 3 Phase 1
 * Real-time bidirectional communication infrastructure
 * Production-grade WebSocket server with connection pooling
 */

#pragma once

#include <string>
#include <map>
#include <memory>
#include <functional>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <nlohmann/json.hpp>

namespace regulens {
namespace websocket {

using json = nlohmann::json;

// Connection states
enum class ConnectionState {
  CONNECTING,
  CONNECTED,
  AUTHENTICATED,
  DISCONNECTING,
  DISCONNECTED
};

// Message types for real-time events
enum class MessageType {
  CONNECTION_ESTABLISHED,
  HEARTBEAT,
  SUBSCRIBE,
  UNSUBSCRIBE,
  BROADCAST,
  DIRECT_MESSAGE,
  SESSION_UPDATE,
  RULE_EVALUATION_RESULT,
  DECISION_ANALYSIS_RESULT,
  CONSENSUS_UPDATE,
  LEARNING_FEEDBACK,
  ALERT,
  ERROR
};

// Connection information
struct WebSocketConnection {
  std::string connection_id;
  std::string user_id;
  std::string session_id;
  ConnectionState state;
  std::chrono::system_clock::time_point connected_at;
  std::chrono::system_clock::time_point last_heartbeat;
  std::vector<std::string> subscriptions;
  int failed_pings = 0;
  uint64_t messages_sent = 0;
  uint64_t messages_received = 0;
};

// Real-time message structure
struct WebSocketMessage {
  std::string message_id;
  MessageType type;
  std::string sender_id;
  std::string recipient_id;  // Empty for broadcast
  json payload;
  std::chrono::system_clock::time_point timestamp;
  bool requires_acknowledgment = false;
  std::string acknowledgment_id;
};

// Connection pool entry
struct ConnectionPoolEntry {
  std::shared_ptr<WebSocketConnection> connection;
  std::queue<WebSocketMessage> message_queue;
  std::mutex queue_lock;
};

// Message handler callback
using MessageHandler = std::function<void(const WebSocketMessage&, const std::shared_ptr<WebSocketConnection>&)>;

// Event callbacks
using OnConnectHandler = std::function<void(const std::shared_ptr<WebSocketConnection>&)>;
using OnDisconnectHandler = std::function<void(const std::shared_ptr<WebSocketConnection>&)>;
using OnErrorHandler = std::function<void(const std::string&, const std::string&)>;

class WebSocketServer {
public:
  WebSocketServer(int port, int max_connections = 5000);
  ~WebSocketServer();

  // Lifecycle management
  bool initialize();
  void start();
  void stop();
  void shutdown();

  // Connection management
  std::shared_ptr<WebSocketConnection> create_connection(
    const std::string& user_id,
    const std::string& session_id
  );
  bool add_connection(std::shared_ptr<WebSocketConnection> connection);
  bool remove_connection(const std::string& connection_id);
  std::shared_ptr<WebSocketConnection> get_connection(const std::string& connection_id);
  std::vector<std::shared_ptr<WebSocketConnection>> get_user_connections(const std::string& user_id);
  int get_connection_count();
  int get_active_connection_count();

  // Message handling
  void register_message_handler(MessageType type, MessageHandler handler);
  void handle_message(const WebSocketMessage& message, const std::string& connection_id);
  
  // Broadcasting and messaging
  void broadcast_message(const WebSocketMessage& message);
  void send_to_connection(const std::string& connection_id, const WebSocketMessage& message);
  void send_to_user(const std::string& user_id, const WebSocketMessage& message);
  void send_to_subscriptions(const std::vector<std::string>& subscriptions, const WebSocketMessage& message);
  
  // Subscription management
  bool subscribe(const std::string& connection_id, const std::string& channel);
  bool unsubscribe(const std::string& connection_id, const std::string& channel);
  std::vector<std::shared_ptr<WebSocketConnection>> get_subscribers(const std::string& channel);
  
  // Heartbeat and keep-alive
  void start_heartbeat();
  void stop_heartbeat();
  void send_heartbeat();
  void handle_pong(const std::string& connection_id);
  
  // Connection state management
  bool authenticate_connection(const std::string& connection_id, const std::string& user_id);
  ConnectionState get_connection_state(const std::string& connection_id);
  bool is_connection_alive(const std::string& connection_id);
  
  // Statistics and monitoring
  struct ServerStats {
    int total_connections;
    int active_connections;
    int authenticated_connections;
    uint64_t total_messages_processed;
    uint64_t total_messages_sent;
    double average_latency_ms;
    std::chrono::system_clock::time_point uptime;
  };
  ServerStats get_stats();
  
  // Configuration
  void set_heartbeat_interval(int milliseconds);
  void set_message_queue_size(int size);
  void set_max_message_size(int bytes);
  void set_connection_timeout(int seconds);
  
  // Event handlers
  void on_connect(OnConnectHandler handler) { connect_handler = handler; }
  void on_disconnect(OnDisconnectHandler handler) { disconnect_handler = handler; }
  void on_error(OnErrorHandler handler) { error_handler = handler; }

private:
  int port_;
  int max_connections_;
  bool is_running_ = false;
  
  // Connection pool
  std::map<std::string, ConnectionPoolEntry> connection_pool_;
  std::mutex pool_lock_;
  
  // Message handlers
  std::map<MessageType, MessageHandler> message_handlers_;
  std::mutex handlers_lock_;
  
  // Event handlers
  OnConnectHandler connect_handler;
  OnDisconnectHandler disconnect_handler;
  OnErrorHandler error_handler;
  
  // Heartbeat management
  std::thread heartbeat_thread_;
  bool heartbeat_running_ = false;
  int heartbeat_interval_ms_ = 30000;
  std::mutex heartbeat_lock_;
  std::condition_variable heartbeat_cv_;
  
  // Message processing
  std::thread message_processor_thread_;
  bool processor_running_ = false;
  int message_queue_size_ = 1000;
  int max_message_size_ = 1048576;  // 1MB
  
  // Connection timeout
  int connection_timeout_seconds_ = 300;
  std::thread timeout_monitor_thread_;
  bool timeout_monitor_running_ = false;
  
  // Statistics
  uint64_t total_messages_processed_ = 0;
  uint64_t total_messages_sent_ = 0;
  std::mutex stats_lock_;
  
  // Internal methods
  void heartbeat_loop();
  void message_processing_loop();
  void timeout_monitoring_loop();
  void cleanup_dead_connections();
  std::string generate_connection_id();
  bool validate_message(const WebSocketMessage& message);
};

}  // namespace websocket
}  // namespace regulens

