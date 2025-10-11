/**
 * Redis Session Manager - Header
 * 
 * Production-grade session management using Redis for scalability and high availability.
 * Supports distributed session storage, session expiration, and concurrent session limits.
 */

#ifndef REGULENS_REDIS_SESSION_MANAGER_HPP
#define REGULENS_REDIS_SESSION_MANAGER_HPP

#include <string>
#include <map>
#include <vector>
#include <chrono>
#include <memory>
#include <mutex>

namespace regulens {

// Session data structure
struct SessionData {
    std::string session_id;
    std::string user_id;
    std::string username;
    std::string ip_address;
    std::string user_agent;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_accessed;
    std::chrono::system_clock::time_point expires_at;
    std::map<std::string, std::string> attributes;
    bool is_valid;
};

/**
 * Redis Session Manager
 * 
 * Manages user sessions using Redis for distributed, scalable session storage.
 * Features:
 * - Distributed session storage across multiple servers
 * - Automatic session expiration
 * - Concurrent session limits per user
 * - Session activity tracking
 * - Session hijacking prevention
 * - Secure session IDs
 * - Session replication for high availability
 */
class RedisSessionManager {
public:
    /**
     * Constructor
     * 
     * @param redis_host Redis server host
     * @param redis_port Redis server port
     * @param redis_password Redis password (empty if no auth)
     * @param redis_db Redis database number
     */
    explicit RedisSessionManager(
        const std::string& redis_host = "localhost",
        int redis_port = 6379,
        const std::string& redis_password = "",
        int redis_db = 0
    );

    ~RedisSessionManager();

    /**
     * Initialize Redis connection
     * 
     * @return true if successful
     */
    bool initialize();

    /**
     * Create a new session
     * 
     * @param user_id User ID
     * @param username Username
     * @param ip_address Client IP address
     * @param user_agent Client user agent
     * @param ttl_seconds Session TTL in seconds (default: 1800 = 30 minutes)
     * @return Session ID if successful, empty string if failed
     */
    std::string create_session(
        const std::string& user_id,
        const std::string& username,
        const std::string& ip_address,
        const std::string& user_agent,
        int ttl_seconds = 1800
    );

    /**
     * Get session data
     * 
     * @param session_id Session ID
     * @return Session data if valid, empty struct if not found or invalid
     */
    SessionData get_session(const std::string& session_id);

    /**
     * Update session
     * Updates last accessed time and extends expiration
     * 
     * @param session_id Session ID
     * @param extend_ttl_seconds How much to extend the TTL (default: 1800)
     * @return true if successful
     */
    bool update_session(const std::string& session_id, int extend_ttl_seconds = 1800);

    /**
     * Set session attribute
     * 
     * @param session_id Session ID
     * @param key Attribute key
     * @param value Attribute value
     * @return true if successful
     */
    bool set_session_attribute(
        const std::string& session_id,
        const std::string& key,
        const std::string& value
    );

    /**
     * Get session attribute
     * 
     * @param session_id Session ID
     * @param key Attribute key
     * @return Attribute value, empty string if not found
     */
    std::string get_session_attribute(
        const std::string& session_id,
        const std::string& key
    );

    /**
     * Delete session
     * 
     * @param session_id Session ID
     * @return true if successful
     */
    bool delete_session(const std::string& session_id);

    /**
     * Delete all sessions for a user
     * 
     * @param user_id User ID
     * @return Number of sessions deleted
     */
    int delete_user_sessions(const std::string& user_id);

    /**
     * Get all sessions for a user
     * 
     * @param user_id User ID
     * @return Vector of session IDs
     */
    std::vector<std::string> get_user_sessions(const std::string& user_id);

    /**
     * Get active session count for a user
     * 
     * @param user_id User ID
     * @return Number of active sessions
     */
    int get_user_session_count(const std::string& user_id);

    /**
     * Enforce concurrent session limit
     * Deletes oldest sessions if limit exceeded
     * 
     * @param user_id User ID
     * @param max_sessions Maximum allowed concurrent sessions
     * @return Number of sessions deleted
     */
    int enforce_session_limit(const std::string& user_id, int max_sessions);

    /**
     * Validate session
     * Checks if session exists, is not expired, and matches IP/user agent
     * 
     * @param session_id Session ID
     * @param ip_address Current IP address
     * @param user_agent Current user agent
     * @param check_ip Whether to validate IP address
     * @param check_user_agent Whether to validate user agent
     * @return true if valid
     */
    bool validate_session(
        const std::string& session_id,
        const std::string& ip_address = "",
        const std::string& user_agent = "",
        bool check_ip = true,
        bool check_user_agent = true
    );

    /**
     * Invalidate all sessions
     * Used for emergency logout of all users
     * 
     * @return Number of sessions invalidated
     */
    int invalidate_all_sessions();

    /**
     * Get total active session count
     * 
     * @return Number of active sessions across all users
     */
    int get_total_session_count();

    /**
     * Clean up expired sessions
     * Redis handles this automatically, but this provides manual cleanup
     * 
     * @return Number of sessions cleaned up
     */
    int cleanup_expired_sessions();

    /**
     * Get session statistics
     * 
     * @return JSON string with session statistics
     */
    std::string get_session_statistics();

private:
    std::string redis_host_;
    int redis_port_;
    std::string redis_password_;
    int redis_db_;
    void* redis_context_;  // redisContext pointer
    bool initialized_;
    std::mutex redis_mutex_;

    /**
     * Generate secure session ID
     */
    std::string generate_session_id() const;

    /**
     * Build Redis key for session
     */
    std::string build_session_key(const std::string& session_id) const;

    /**
     * Build Redis key for user sessions index
     */
    std::string build_user_sessions_key(const std::string& user_id) const;

    /**
     * Connect to Redis
     */
    bool connect();

    /**
     * Disconnect from Redis
     */
    void disconnect();

    /**
     * Check if connected
     */
    bool is_connected() const;

    /**
     * Execute Redis command
     */
    bool redis_command(const std::string& command, std::string& response);

    /**
     * Set Redis key with expiration
     */
    bool redis_setex(
        const std::string& key,
        const std::string& value,
        int ttl_seconds
    );

    /**
     * Get Redis key value
     */
    bool redis_get(const std::string& key, std::string& value);

    /**
     * Delete Redis key
     */
    bool redis_del(const std::string& key);

    /**
     * Add to Redis set
     */
    bool redis_sadd(const std::string& key, const std::string& member);

    /**
     * Remove from Redis set
     */
    bool redis_srem(const std::string& key, const std::string& member);

    /**
     * Get all members of Redis set
     */
    std::vector<std::string> redis_smembers(const std::string& key);

    /**
     * Serialize session data to JSON
     */
    std::string serialize_session(const SessionData& session) const;

    /**
     * Deserialize session data from JSON
     */
    SessionData deserialize_session(const std::string& json_data) const;
};

} // namespace regulens

#endif // REGULENS_REDIS_SESSION_MANAGER_HPP

