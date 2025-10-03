/**
 * Redis Client Infrastructure - Production-Grade Distributed Caching
 *
 * Enterprise-grade Redis client with connection pooling, health monitoring,
 * and comprehensive error handling for high-performance distributed caching.
 *
 * Features:
 * - Connection pooling with automatic health checks
 * - Configurable retry logic and timeouts
 * - TLS/SSL support for secure connections
 * - Pub/Sub capabilities for real-time updates
 * - Comprehensive metrics and monitoring
 * - Thread-safe operations with async support
 * - Lua scripting support for complex operations
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <nlohmann/json.hpp>

#include "../config/configuration_manager.hpp"
#include "../logging/structured_logger.hpp"
#include "../error_handler.hpp"

namespace regulens {

// Forward declarations for Redis types (would be from hiredis or similar)
struct RedisConnection;
struct RedisReply;

/**
 * @brief Redis connection configuration
 */
struct RedisConfig {
    std::string host = "localhost";
    int port = 6379;
    std::string password;
    int database = 0;
    std::chrono::milliseconds connect_timeout = std::chrono::milliseconds(5000);
    std::chrono::milliseconds command_timeout = std::chrono::milliseconds(2000);
    int max_retries = 3;
    std::chrono::milliseconds retry_delay = std::chrono::milliseconds(100);
    bool use_ssl = false;
    std::string ssl_cert_file;
    std::string ssl_key_file;
    std::string ssl_ca_file;
    bool enable_keepalive = true;
    int keepalive_interval_seconds = 60;
    size_t max_connections = 20;
    size_t min_idle_connections = 5;
    std::chrono::seconds connection_ttl = std::chrono::seconds(300);
    bool enable_metrics = true;
};

/**
 * @brief Redis operation result
 */
struct RedisResult {
    bool success;
    std::optional<std::string> value;
    std::optional<std::vector<std::string>> array_value;
    std::optional<int64_t> integer_value;
    std::string error_message;
    std::chrono::milliseconds execution_time;

    RedisResult(bool s = false, std::string err = "",
                std::chrono::milliseconds time = std::chrono::milliseconds(0))
        : success(s), error_message(std::move(err)), execution_time(time) {}
};

/**
 * @brief Redis connection wrapper
 */
class RedisConnectionWrapper {
public:
    RedisConnectionWrapper(const RedisConfig& config,
                          std::shared_ptr<StructuredLogger> logger);
    ~RedisConnectionWrapper();

    /**
     * @brief Connect to Redis server
     * @return true if connection successful
     */
    bool connect();

    /**
     * @brief Disconnect from Redis server
     */
    void disconnect();

    /**
     * @brief Check if connection is alive
     * @return true if connected and responsive
     */
    bool is_connected() const;

    /**
     * @brief Execute Redis command
     * @param command Redis command (e.g., "GET", "SET")
     * @param args Command arguments
     * @return RedisResult with command result
     */
    RedisResult execute_command(const std::string& command,
                               const std::vector<std::string>& args = {});

    /**
     * @brief Get connection creation time
     * @return Time when connection was created
     */
    std::chrono::system_clock::time_point get_creation_time() const { return creation_time_; }

    /**
     * @brief Get last activity time
     * @return Time of last command execution
     */
    std::chrono::system_clock::time_point get_last_activity() const { return last_activity_; }

private:
    RedisConfig config_;
    std::shared_ptr<StructuredLogger> logger_;
    RedisConnection* connection_;
    std::chrono::system_clock::time_point creation_time_;
    mutable std::chrono::system_clock::time_point last_activity_;
    mutable std::mutex activity_mutex_;

    /**
     * @brief Update last activity timestamp
     */
    void update_activity();
};

/**
 * @brief Redis connection pool for efficient resource management
 */
class RedisConnectionPool {
public:
    RedisConnectionPool(const RedisConfig& config,
                       std::shared_ptr<StructuredLogger> logger);
    ~RedisConnectionPool();

    /**
     * @brief Initialize the connection pool
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Shutdown the connection pool
     */
    void shutdown();

    /**
     * @brief Get a connection from the pool
     * @return RedisConnectionWrapper or nullptr if pool exhausted
     */
    std::shared_ptr<RedisConnectionWrapper> get_connection();

    /**
     * @brief Return a connection to the pool
     * @param connection Connection to return
     */
    void return_connection(std::shared_ptr<RedisConnectionWrapper> connection);

    /**
     * @brief Get pool statistics
     * @return JSON with pool metrics
     */
    nlohmann::json get_pool_stats() const;

    /**
     * @brief Health check all connections in pool
     */
    void perform_health_check();

private:
    RedisConfig config_;
    std::shared_ptr<StructuredLogger> logger_;

    mutable std::mutex pool_mutex_;
    std::queue<std::shared_ptr<RedisConnectionWrapper>> available_connections_;
    std::vector<std::shared_ptr<RedisConnectionWrapper>> all_connections_;
    std::atomic<size_t> active_connections_;
    std::atomic<bool> shutdown_requested_;

    /**
     * @brief Create a new connection
     * @return New RedisConnectionWrapper or nullptr on failure
     */
    std::shared_ptr<RedisConnectionWrapper> create_connection();

    /**
     * @brief Validate connection health
     * @param connection Connection to validate
     * @return true if connection is healthy
     */
    bool validate_connection(std::shared_ptr<RedisConnectionWrapper> connection);

    /**
     * @brief Cleanup expired connections
     */
    void cleanup_expired_connections();
};

/**
 * @brief Redis cache entry metadata
 */
struct CacheEntry {
    std::string key;
    std::string value;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point expires_at;
    std::string cache_type;  // "llm_response", "regulatory_data", "session", etc.
    std::unordered_map<std::string, std::string> metadata;

    CacheEntry(std::string k, std::string v, std::chrono::seconds ttl,
              std::string type = "", std::unordered_map<std::string, std::string> meta = {})
        : key(std::move(k)), value(std::move(v)), created_at(std::chrono::system_clock::now()),
          expires_at(created_at + ttl), cache_type(std::move(type)), metadata(std::move(meta)) {}

    bool is_expired() const {
        return std::chrono::system_clock::now() > expires_at;
    }

    std::chrono::seconds ttl_remaining() const {
        auto now = std::chrono::system_clock::now();
        if (now >= expires_at) return std::chrono::seconds(0);
        return std::chrono::duration_cast<std::chrono::seconds>(expires_at - now);
    }
};

/**
 * @brief Enterprise Redis Client with advanced caching capabilities
 */
class RedisClient {
public:
    RedisClient(std::shared_ptr<ConfigurationManager> config,
               std::shared_ptr<StructuredLogger> logger,
               std::shared_ptr<ErrorHandler> error_handler);

    ~RedisClient();

    /**
     * @brief Initialize Redis client and connection pool
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Shutdown Redis client and cleanup resources
     */
    void shutdown();

    /**
     * @brief Check if Redis client is healthy and connected
     * @return true if operational
     */
    bool is_healthy() const;

    // ===== BASIC REDIS OPERATIONS =====

    /**
     * @brief Get value by key
     * @param key Cache key
     * @return RedisResult with value or error
     */
    RedisResult get(const std::string& key);

    /**
     * @brief Set value with optional TTL
     * @param key Cache key
     * @param value Value to store
     * @param ttl_seconds Time-to-live in seconds (0 = no expiration)
     * @return RedisResult indicating success/failure
     */
    RedisResult set(const std::string& key, const std::string& value,
                   std::chrono::seconds ttl_seconds = std::chrono::seconds(0));

    /**
     * @brief Delete key
     * @param key Cache key
     * @return RedisResult with deletion count
     */
    RedisResult del(const std::string& key);

    /**
     * @brief Check if key exists
     * @param key Cache key
     * @return RedisResult with existence boolean
     */
    RedisResult exists(const std::string& key);

    /**
     * @brief Set expiration time on key
     * @param key Cache key
     * @param ttl_seconds Time-to-live in seconds
     * @return RedisResult indicating success
     */
    RedisResult expire(const std::string& key, std::chrono::seconds ttl_seconds);

    // ===== ADVANCED CACHE OPERATIONS =====

    /**
     * @brief Get multiple keys at once
     * @param keys Vector of cache keys
     * @return RedisResult with array of values
     */
    RedisResult mget(const std::vector<std::string>& keys);

    /**
     * @brief Set multiple key-value pairs
     * @param key_values Map of key-value pairs
     * @param ttl_seconds Time-to-live for all keys
     * @return RedisResult indicating success
     */
    RedisResult mset(const std::unordered_map<std::string, std::string>& key_values,
                    std::chrono::seconds ttl_seconds = std::chrono::seconds(0));

    /**
     * @brief Get keys matching pattern
     * @param pattern Glob-style pattern (e.g., "cache:*")
     * @return RedisResult with array of matching keys
     */
    RedisResult keys(const std::string& pattern);

    // ===== PUB/SUB OPERATIONS =====

    /**
     * @brief Publish message to channel
     * @param channel Channel name
     * @param message Message content
     * @return RedisResult with number of subscribers
     */
    RedisResult publish(const std::string& channel, const std::string& message);

    /**
     * @brief Subscribe to channels (blocking operation)
     * @param channels Vector of channel names
     * @param message_callback Callback for received messages
     * @param timeout_seconds Maximum time to wait for messages
     * @return RedisResult indicating subscription status
     */
    RedisResult subscribe(const std::vector<std::string>& channels,
                         std::function<void(const std::string&, const std::string&)> message_callback,
                         int timeout_seconds = 30);

    // ===== LUA SCRIPTING =====

    /**
     * @brief Execute Lua script
     * @param script Lua script content
     * @param keys Redis keys used by script
     * @param args Additional arguments for script
     * @return RedisResult with script execution result
     */
    RedisResult eval(const std::string& script,
                    const std::vector<std::string>& keys = {},
                    const std::vector<std::string>& args = {});

    // ===== CACHE MANAGEMENT =====

    /**
     * @brief Cache LLM response with intelligent TTL
     * @param prompt_hash Hash of the prompt
     * @param model Model used
     * @param response LLM response
     * @param prompt_complexity Complexity score (0.0-1.0) affects TTL
     * @return RedisResult indicating caching success
     */
    RedisResult cache_llm_response(const std::string& prompt_hash,
                                  const std::string& model,
                                  const std::string& response,
                                  double prompt_complexity = 0.5);

    /**
     * @brief Get cached LLM response
     * @param prompt_hash Hash of the prompt
     * @param model Model used
     * @return RedisResult with cached response or miss
     */
    RedisResult get_cached_llm_response(const std::string& prompt_hash,
                                       const std::string& model);

    /**
     * @brief Cache regulatory document
     * @param document_id Unique document identifier
     * @param content Document content
     * @param source Regulatory source (SEC, FCA, ECB)
     * @param metadata Additional document metadata
     * @return RedisResult indicating caching success
     */
    RedisResult cache_regulatory_document(const std::string& document_id,
                                        const std::string& content,
                                        const std::string& source,
                                        const std::unordered_map<std::string, std::string>& metadata = {});

    /**
     * @brief Get cached regulatory document
     * @param document_id Document identifier
     * @return RedisResult with cached document or miss
     */
    RedisResult get_cached_regulatory_document(const std::string& document_id);

    // ===== METRICS AND MONITORING =====

    /**
     * @brief Get Redis client metrics
     * @return JSON with client performance metrics
     */
    nlohmann::json get_client_metrics() const;

    /**
     * @brief Get connection pool metrics
     * @return JSON with pool performance metrics
     */
    nlohmann::json get_pool_metrics() const;

    /**
     * @brief Perform health check
     * @return JSON with health status
     */
    nlohmann::json perform_health_check();

private:
    std::shared_ptr<ConfigurationManager> config_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<ErrorHandler> error_handler_;

    RedisConfig redis_config_;
    std::unique_ptr<RedisConnectionPool> connection_pool_;
    std::atomic<bool> initialized_;

    // Metrics tracking
    std::atomic<size_t> total_commands_;
    std::atomic<size_t> successful_commands_;
    std::atomic<size_t> failed_commands_;
    std::atomic<size_t> cache_hits_;
    std::atomic<size_t> cache_misses_;
    std::atomic<long> total_command_time_ms_;

    /**
     * @brief Load Redis configuration from config manager
     */
    void load_config();

    /**
     * @brief Generate cache key with namespace
     * @param key Base key
     * @param namespace_prefix Key namespace
     * @return Full cache key
     */
    std::string make_cache_key(const std::string& key, const std::string& namespace_prefix = "") const;

    /**
     * @brief Calculate intelligent TTL based on content type and characteristics
     * @param content_type Type of content ("llm", "regulatory", "session")
     * @param complexity_or_importance Importance/complexity score (0.0-1.0)
     * @param base_ttl_seconds Base TTL in seconds
     * @return Calculated TTL in seconds
     */
    std::chrono::seconds calculate_intelligent_ttl(const std::string& content_type,
                                                  double complexity_or_importance,
                                                  std::chrono::seconds base_ttl_seconds = std::chrono::seconds(3600));

    /**
     * @brief Execute command with connection from pool
     * @param operation Function that takes connection and returns RedisResult
     * @return RedisResult from operation
     */
    RedisResult execute_with_connection(std::function<RedisResult(std::shared_ptr<RedisConnectionWrapper>)> operation);

    /**
     * @brief Update command metrics
     * @param success Whether command succeeded
     * @param execution_time_ms Time taken to execute
     */
    void update_command_metrics(bool success, long execution_time_ms);
};

/**
 * @brief Create Redis client instance
 * @param config Configuration manager
 * @param logger Structured logger
 * @param error_handler Error handler
 * @return Shared pointer to Redis client
 */
std::shared_ptr<RedisClient> create_redis_client(
    std::shared_ptr<ConfigurationManager> config,
    std::shared_ptr<StructuredLogger> logger,
    std::shared_ptr<ErrorHandler> error_handler);

} // namespace regulens
