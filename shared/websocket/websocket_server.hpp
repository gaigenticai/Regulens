/**
 * WebSocket Server - Header
 * 
 * Production-grade WebSocket server for real-time regulatory updates.
 * Supports WebSocket protocol, real-time push notifications, and broadcast messaging.
 */

#ifndef REGULENS_WEBSOCKET_SERVER_HPP
#define REGULENS_WEBSOCKET_SERVER_HPP

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <mutex>
#include <thread>
#include <functional>
#include <chrono>

namespace regulens {

// WebSocket message types
enum class WSMessageType {
    TEXT,
    BINARY,
    PING,
    PONG,
    CLOSE
};

// WebSocket connection state
enum class WSConnectionState {
    CONNECTING,
    OPEN,
    CLOSING,
    CLOSED
};

// WebSocket message
struct WSMessage {
    WSMessageType type;
    std::string data;
    std::chrono::system_clock::time_point timestamp;
    std::string client_id;
};

// WebSocket client connection
struct WSClient {
    std::string client_id;
    int socket_fd;
    WSConnectionState state;
    std::string user_id;
    std::string session_id;
    std::set<std::string> subscribed_topics;
    std::chrono::system_clock::time_point connected_at;
    std::chrono::system_clock::time_point last_activity;
    std::string ip_address;
    std::string user_agent;
};

// Message handler callback
using MessageHandler = std::function<void(const WSClient&, const WSMessage&)>;

// Connection handler callback
using ConnectionHandler = std::function<void(const WSClient&)>;

/**
 * WebSocket Server
 * 
 * Production-grade WebSocket server for real-time communication.
 * Features:
 * - WebSocket protocol (RFC 6455) support
 * - Real-time push notifications
 * - Topic-based subscriptions
 * - Broadcast messaging
 * - Connection management
 * - Heartbeat/ping-pong
 * - Authentication integration
 * - Message queueing
 * - Compression support
 * - SSL/TLS support
 */
class WebSocketServer {
public:
    /**
     * Constructor
     * 
     * @param port Port to listen on
     * @param host Host to bind to (default: 0.0.0.0)
     */
    explicit WebSocketServer(int port = 8081, const std::string& host = "0.0.0.0");

    ~WebSocketServer();

    /**
     * Start the WebSocket server
     * 
     * @return true if started successfully
     */
    bool start();

    /**
     * Stop the WebSocket server
     */
    void stop();

    /**
     * Check if server is running
     * 
     * @return true if running
     */
    bool is_running() const;

    /**
     * Send message to a specific client
     * 
     * @param client_id Client ID
     * @param message Message to send
     * @return true if sent successfully
     */
    bool send_to_client(const std::string& client_id, const std::string& message);

    /**
     * Broadcast message to all connected clients
     * 
     * @param message Message to broadcast
     * @return Number of clients message was sent to
     */
    int broadcast(const std::string& message);

    /**
     * Broadcast message to clients subscribed to a topic
     * 
     * @param topic Topic name
     * @param message Message to send
     * @return Number of clients message was sent to
     */
    int broadcast_to_topic(const std::string& topic, const std::string& message);

    /**
     * Send regulatory update to all subscribed clients
     * 
     * @param update_data Regulatory update data (JSON)
     * @return Number of clients notified
     */
    int send_regulatory_update(const std::string& update_data);

    /**
     * Send transaction alert to specific user
     * 
     * @param user_id User ID
     * @param alert_data Alert data (JSON)
     * @return true if sent successfully
     */
    bool send_transaction_alert(const std::string& user_id, const std::string& alert_data);

    /**
     * Send system notification
     * 
     * @param notification_data Notification data (JSON)
     * @param target_users Optional list of specific users (empty = all users)
     * @return Number of clients notified
     */
    int send_system_notification(
        const std::string& notification_data,
        const std::vector<std::string>& target_users = {}
    );

    /**
     * Subscribe client to topic
     * 
     * @param client_id Client ID
     * @param topic Topic name
     * @return true if subscribed successfully
     */
    bool subscribe_client_to_topic(const std::string& client_id, const std::string& topic);

    /**
     * Unsubscribe client from topic
     * 
     * @param client_id Client ID
     * @param topic Topic name
     * @return true if unsubscribed successfully
     */
    bool unsubscribe_client_from_topic(const std::string& client_id, const std::string& topic);

    /**
     * Get all connected clients
     * 
     * @return Vector of client IDs
     */
    std::vector<std::string> get_connected_clients() const;

    /**
     * Get client count
     * 
     * @return Number of connected clients
     */
    int get_client_count() const;

    /**
     * Get clients subscribed to a topic
     * 
     * @param topic Topic name
     * @return Vector of client IDs
     */
    std::vector<std::string> get_topic_subscribers(const std::string& topic) const;

    /**
     * Disconnect a client
     * 
     * @param client_id Client ID
     * @param reason Disconnect reason
     * @return true if disconnected
     */
    bool disconnect_client(const std::string& client_id, const std::string& reason = "");

    /**
     * Set message handler
     * 
     * @param handler Message handler function
     */
    void set_message_handler(MessageHandler handler);

    /**
     * Set connection handler
     * 
     * @param handler Connection handler function
     */
    void set_connection_handler(ConnectionHandler handler);

    /**
     * Set disconnection handler
     * 
     * @param handler Disconnection handler function
     */
    void set_disconnection_handler(ConnectionHandler handler);

    /**
     * Enable compression
     * 
     * @param enabled Whether to enable compression
     */
    void set_compression_enabled(bool enabled);

    /**
     * Set heartbeat interval
     * 
     * @param interval_seconds Interval in seconds (0 to disable)
     */
    void set_heartbeat_interval(int interval_seconds);

    /**
     * Get server statistics
     * 
     * @return JSON string with statistics
     */
    std::string get_statistics() const;

private:
    int port_;
    std::string host_;
    int server_socket_;
    bool running_;
    bool compression_enabled_;
    int heartbeat_interval_seconds_;

    std::map<std::string, WSClient> clients_;
    std::map<std::string, std::set<std::string>> topic_subscribers_;  // topic -> client_ids
    
    std::mutex clients_mutex_;
    std::mutex topics_mutex_;

    MessageHandler message_handler_;
    ConnectionHandler connection_handler_;
    ConnectionHandler disconnection_handler_;

    std::thread server_thread_;
    std::thread heartbeat_thread_;

    /**
     * Server loop
     */
    void server_loop();

    /**
     * Heartbeat loop
     */
    void heartbeat_loop();

    /**
     * Handle new connection
     */
    void handle_connection(int client_socket, const std::string& ip_address);

    /**
     * Handle client messages
     */
    void handle_client(const std::string& client_id);

    /**
     * Perform WebSocket handshake
     */
    bool perform_websocket_handshake(int socket_fd, const std::string& request);

    /**
     * Send WebSocket frame
     */
    bool send_websocket_frame(
        int socket_fd,
        const std::string& data,
        WSMessageType type = WSMessageType::TEXT
    );

    /**
     * Receive WebSocket frame
     */
    WSMessage receive_websocket_frame(int socket_fd);

    /**
     * Generate client ID
     */
    std::string generate_client_id() const;

    /**
     * Parse WebSocket upgrade request
     */
    std::map<std::string, std::string> parse_http_headers(const std::string& request);

    /**
     * Generate WebSocket accept key
     */
    std::string generate_accept_key(const std::string& client_key) const;

    /**
     * Close client connection
     */
    void close_client_connection(const std::string& client_id);

    /**
     * Clean up inactive clients
     */
    void cleanup_inactive_clients();

    /**
     * Encode WebSocket frame
     */
    std::vector<uint8_t> encode_websocket_frame(
        const std::string& payload,
        uint8_t opcode,
        bool mask = false
    );

    /**
     * Decode WebSocket frame
     */
    bool decode_websocket_frame(
        const std::vector<uint8_t>& frame,
        std::string& payload,
        uint8_t& opcode
    );
};

} // namespace regulens

#endif // REGULENS_WEBSOCKET_SERVER_HPP

