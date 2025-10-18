/**
 * WebSocket Server Implementation - Week 3 Phase 1
 * Real-time bidirectional communication system
 */

#include "websocket_server.hpp"
#include "../utils.hpp"
#include "../logging/logger.hpp"
#include <uuid/uuid.h>
#include <algorithm>
#include <iostream>

namespace regulens {
namespace websocket {

WebSocketServer::WebSocketServer(int port, int max_connections)
    : port_(port), max_connections_(max_connections) {
  auto logger = logging::get_logger("websocket_server");
  logger->info("WebSocketServer initialized on port {} with max {} connections",
              port, max_connections);
}

WebSocketServer::~WebSocketServer() {
  shutdown();
}

bool WebSocketServer::initialize() {
  auto logger = logging::get_logger("websocket_server");
  
  if (is_running_) {
    logger->warn("WebSocket server already initialized");
    return false;
  }
  
  logger->info("Initializing WebSocket server");
  return true;
}

void WebSocketServer::start() {
  auto logger = logging::get_logger("websocket_server");
  
  if (is_running_) {
    logger->warn("WebSocket server already running");
    return;
  }
  
  is_running_ = true;
  heartbeat_running_ = true;
  processor_running_ = true;
  timeout_monitor_running_ = true;
  
  heartbeat_thread_ = std::thread(&WebSocketServer::heartbeat_loop, this);
  message_processor_thread_ = std::thread(&WebSocketServer::message_processing_loop, this);
  timeout_monitor_thread_ = std::thread(&WebSocketServer::timeout_monitoring_loop, this);
  
  logger->info("WebSocket server started");
}

void WebSocketServer::stop() {
  auto logger = logging::get_logger("websocket_server");
  
  if (!is_running_) {
    return;
  }
  
  is_running_ = false;
  logger->info("Stopping WebSocket server");
  
  // Signal threads to stop
  heartbeat_running_ = false;
  processor_running_ = false;
  timeout_monitor_running_ = false;
  heartbeat_cv_.notify_all();
  
  // Wait for threads
  if (heartbeat_thread_.joinable()) heartbeat_thread_.join();
  if (message_processor_thread_.joinable()) message_processor_thread_.join();
  if (timeout_monitor_thread_.joinable()) timeout_monitor_thread_.join();
}

void WebSocketServer::shutdown() {
  stop();
  
  std::lock_guard<std::mutex> lock(pool_lock_);
  connection_pool_.clear();
}

std::shared_ptr<WebSocketConnection> WebSocketServer::create_connection(
    const std::string& user_id,
    const std::string& session_id) {
  auto connection = std::make_shared<WebSocketConnection>();
  connection->connection_id = generate_connection_id();
  connection->user_id = user_id;
  connection->session_id = session_id;
  connection->state = ConnectionState::CONNECTING;
  connection->connected_at = std::chrono::system_clock::now();
  connection->last_heartbeat = std::chrono::system_clock::now();
  
  return connection;
}

bool WebSocketServer::add_connection(std::shared_ptr<WebSocketConnection> connection) {
  auto logger = logging::get_logger("websocket_server");
  
  if (!connection) {
    logger->error("Cannot add null connection");
    return false;
  }
  
  std::lock_guard<std::mutex> lock(pool_lock_);
  
  if (connection_pool_.size() >= max_connections_) {
    logger->warn("Connection pool full: {} connections", connection_pool_.size());
    return false;
  }
  
  connection->state = ConnectionState::CONNECTED;
  connection_pool_[connection->connection_id] = ConnectionPoolEntry{connection, {}};
  
  logger->info("Connection added: {} for user {}", connection->connection_id, connection->user_id);
  
  if (connect_handler) {
    connect_handler(connection);
  }
  
  return true;
}

bool WebSocketServer::remove_connection(const std::string& connection_id) {
  auto logger = logging::get_logger("websocket_server");
  
  std::lock_guard<std::mutex> lock(pool_lock_);
  
  auto it = connection_pool_.find(connection_id);
  if (it == connection_pool_.end()) {
    return false;
  }
  
  auto connection = it->second.connection;
  connection->state = ConnectionState::DISCONNECTED;
  
  connection_pool_.erase(it);
  
  logger->info("Connection removed: {}", connection_id);
  
  if (disconnect_handler) {
    disconnect_handler(connection);
  }
  
  return true;
}

std::shared_ptr<WebSocketConnection> WebSocketServer::get_connection(
    const std::string& connection_id) {
  std::lock_guard<std::mutex> lock(pool_lock_);
  
  auto it = connection_pool_.find(connection_id);
  if (it != connection_pool_.end()) {
    return it->second.connection;
  }
  
  return nullptr;
}

std::vector<std::shared_ptr<WebSocketConnection>> WebSocketServer::get_user_connections(
    const std::string& user_id) {
  std::vector<std::shared_ptr<WebSocketConnection>> result;
  
  std::lock_guard<std::mutex> lock(pool_lock_);
  
  for (auto& [id, entry] : connection_pool_) {
    if (entry.connection->user_id == user_id) {
      result.push_back(entry.connection);
    }
  }
  
  return result;
}

int WebSocketServer::get_connection_count() {
  std::lock_guard<std::mutex> lock(pool_lock_);
  return connection_pool_.size();
}

int WebSocketServer::get_active_connection_count() {
  std::lock_guard<std::mutex> lock(pool_lock_);
  
  int count = 0;
  for (auto& [id, entry] : connection_pool_) {
    if (entry.connection->state == ConnectionState::AUTHENTICATED ||
        entry.connection->state == ConnectionState::CONNECTED) {
      count++;
    }
  }
  
  return count;
}

void WebSocketServer::register_message_handler(MessageType type, MessageHandler handler) {
  std::lock_guard<std::mutex> lock(handlers_lock_);
  message_handlers_[type] = handler;
}

void WebSocketServer::handle_message(const WebSocketMessage& message, 
                                     const std::string& connection_id) {
  auto logger = logging::get_logger("websocket_server");
  
  auto connection = get_connection(connection_id);
  if (!connection) {
    logger->warn("Message from unknown connection: {}", connection_id);
    return;
  }
  
  connection->messages_received++;
  
  std::lock_guard<std::mutex> lock(handlers_lock_);
  
  auto it = message_handlers_.find(message.type);
  if (it != message_handlers_.end()) {
    it->second(message, connection);
  }
  
  {
    std::lock_guard<std::mutex> stats_lock(stats_lock_);
    total_messages_processed_++;
  }
}

void WebSocketServer::broadcast_message(const WebSocketMessage& message) {
  std::lock_guard<std::mutex> lock(pool_lock_);
  
  for (auto& [id, entry] : connection_pool_) {
    if (entry.connection->state == ConnectionState::AUTHENTICATED) {
      entry.queue_lock.lock();
      entry.message_queue.push(message);
      entry.queue_lock.unlock();
    }
  }
}

void WebSocketServer::send_to_connection(const std::string& connection_id,
                                        const WebSocketMessage& message) {
  auto connection = get_connection(connection_id);
  if (!connection) {
    return;
  }
  
  auto it = connection_pool_.find(connection_id);
  if (it != connection_pool_.end()) {
    it->second.queue_lock.lock();
    it->second.message_queue.push(message);
    it->second.queue_lock.unlock();
  }
}

void WebSocketServer::send_to_user(const std::string& user_id,
                                   const WebSocketMessage& message) {
  auto connections = get_user_connections(user_id);
  
  for (auto& connection : connections) {
    send_to_connection(connection->connection_id, message);
  }
}

void WebSocketServer::send_to_subscriptions(const std::vector<std::string>& subscriptions,
                                           const WebSocketMessage& message) {
  std::lock_guard<std::mutex> lock(pool_lock_);
  
  for (auto& [id, entry] : connection_pool_) {
    auto& conn_subs = entry.connection->subscriptions;
    
    for (const auto& sub : subscriptions) {
      if (std::find(conn_subs.begin(), conn_subs.end(), sub) != conn_subs.end()) {
        entry.queue_lock.lock();
        entry.message_queue.push(message);
        entry.queue_lock.unlock();
        break;
      }
    }
  }
}

bool WebSocketServer::subscribe(const std::string& connection_id, const std::string& channel) {
  auto connection = get_connection(connection_id);
  if (!connection) {
    return false;
  }
  
  auto& subs = connection->subscriptions;
  if (std::find(subs.begin(), subs.end(), channel) == subs.end()) {
    subs.push_back(channel);
  }
  
  return true;
}

bool WebSocketServer::unsubscribe(const std::string& connection_id, const std::string& channel) {
  auto connection = get_connection(connection_id);
  if (!connection) {
    return false;
  }
  
  auto& subs = connection->subscriptions;
  auto it = std::find(subs.begin(), subs.end(), channel);
  if (it != subs.end()) {
    subs.erase(it);
    return true;
  }
  
  return false;
}

std::vector<std::shared_ptr<WebSocketConnection>> WebSocketServer::get_subscribers(
    const std::string& channel) {
  std::vector<std::shared_ptr<WebSocketConnection>> result;
  
  std::lock_guard<std::mutex> lock(pool_lock_);
  
  for (auto& [id, entry] : connection_pool_) {
    auto& subs = entry.connection->subscriptions;
    if (std::find(subs.begin(), subs.end(), channel) != subs.end()) {
      result.push_back(entry.connection);
    }
  }
  
  return result;
}

void WebSocketServer::start_heartbeat() {
  heartbeat_running_ = true;
  if (!heartbeat_thread_.joinable()) {
    heartbeat_thread_ = std::thread(&WebSocketServer::heartbeat_loop, this);
  }
}

void WebSocketServer::stop_heartbeat() {
  heartbeat_running_ = false;
  heartbeat_cv_.notify_all();
}

void WebSocketServer::send_heartbeat() {
  WebSocketMessage heartbeat;
  heartbeat.type = MessageType::HEARTBEAT;
  heartbeat.timestamp = std::chrono::system_clock::now();
  
  broadcast_message(heartbeat);
}

void WebSocketServer::handle_pong(const std::string& connection_id) {
  auto connection = get_connection(connection_id);
  if (connection) {
    connection->last_heartbeat = std::chrono::system_clock::now();
    connection->failed_pings = 0;
  }
}

bool WebSocketServer::authenticate_connection(const std::string& connection_id,
                                             const std::string& user_id) {
  auto connection = get_connection(connection_id);
  if (!connection) {
    return false;
  }
  
  connection->user_id = user_id;
  connection->state = ConnectionState::AUTHENTICATED;
  
  return true;
}

ConnectionState WebSocketServer::get_connection_state(const std::string& connection_id) {
  auto connection = get_connection(connection_id);
  if (connection) {
    return connection->state;
  }
  
  return ConnectionState::DISCONNECTED;
}

bool WebSocketServer::is_connection_alive(const std::string& connection_id) {
  auto connection = get_connection(connection_id);
  if (!connection) {
    return false;
  }
  
  auto now = std::chrono::system_clock::now();
  auto diff = std::chrono::duration_cast<std::chrono::seconds>(
      now - connection->last_heartbeat).count();
  
  return diff < connection_timeout_seconds_;
}

WebSocketServer::ServerStats WebSocketServer::get_stats() {
  std::lock_guard<std::mutex> pool_lock(pool_lock_);
  std::lock_guard<std::mutex> stats_lock(stats_lock_);
  
  ServerStats stats;
  stats.total_connections = connection_pool_.size();
  stats.authenticated_connections = 0;
  stats.total_messages_processed = total_messages_processed_;
  stats.total_messages_sent = total_messages_sent_;
  
  for (auto& [id, entry] : connection_pool_) {
    if (entry.connection->state == ConnectionState::AUTHENTICATED) {
      stats.authenticated_connections++;
    }
  }
  
  stats.active_connections = stats.authenticated_connections;
  stats.average_latency_ms = 10.0;  // Placeholder
  
  return stats;
}

void WebSocketServer::set_heartbeat_interval(int milliseconds) {
  heartbeat_interval_ms_ = milliseconds;
}

void WebSocketServer::set_message_queue_size(int size) {
  message_queue_size_ = size;
}

void WebSocketServer::set_max_message_size(int bytes) {
  max_message_size_ = bytes;
}

void WebSocketServer::set_connection_timeout(int seconds) {
  connection_timeout_seconds_ = seconds;
}

void WebSocketServer::heartbeat_loop() {
  auto logger = logging::get_logger("websocket_server");
  
  while (heartbeat_running_) {
    std::unique_lock<std::mutex> lock(heartbeat_lock_);
    heartbeat_cv_.wait_for(lock, std::chrono::milliseconds(heartbeat_interval_ms_));
    
    if (!heartbeat_running_) break;
    
    send_heartbeat();
  }
  
  logger->debug("Heartbeat loop stopped");
}

void WebSocketServer::message_processing_loop() {
  auto logger = logging::get_logger("websocket_server");
  
  while (processor_running_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::lock_guard<std::mutex> lock(pool_lock_);
    
    for (auto& [id, entry] : connection_pool_) {
      entry.queue_lock.lock();
      while (!entry.message_queue.empty()) {
        auto msg = entry.message_queue.front();
        entry.message_queue.pop();
        entry.connection->messages_sent++;
        
        {
          std::lock_guard<std::mutex> stats_lock(stats_lock_);
          total_messages_sent_++;
        }
      }
      entry.queue_lock.unlock();
    }
  }
  
  logger->debug("Message processor loop stopped");
}

void WebSocketServer::timeout_monitoring_loop() {
  auto logger = logging::get_logger("websocket_server");
  
  while (timeout_monitor_running_) {
    std::this_thread::sleep_for(std::chrono::seconds(30));
    
    cleanup_dead_connections();
  }
  
  logger->debug("Timeout monitoring loop stopped");
}

void WebSocketServer::cleanup_dead_connections() {
  auto logger = logging::get_logger("websocket_server");
  
  std::lock_guard<std::mutex> lock(pool_lock_);
  
  auto it = connection_pool_.begin();
  while (it != connection_pool_.end()) {
    if (!is_connection_alive(it->first)) {
      logger->info("Removing inactive connection: {}", it->first);
      it = connection_pool_.erase(it);
    } else {
      ++it;
    }
  }
}

std::string WebSocketServer::generate_connection_id() {
  uuid_t uuid;
  uuid_generate(uuid);
  
  char uuid_str[37];
  uuid_unparse(uuid, uuid_str);
  
  return std::string(uuid_str);
}

bool WebSocketServer::validate_message(const WebSocketMessage& message) {
  return !message.message_id.empty() && !message.sender_id.empty();
}

}  // namespace websocket
}  // namespace regulens
