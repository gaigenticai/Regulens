/**
 * RedisCacheManager - Production-Grade High-Level Caching Layer
 * 
 * Enterprise-grade caching wrapper built on top of RedisClient providing:
 * - Automatic TTL management per feature
 * - Cache invalidation with pattern matching
 * - Serialization/deserialization of complex objects
 * - Cache statistics and monitoring
 * - Multi-level caching strategy
 * - Async cache warming
 * - Cache coherence across distributed systems
 * - Cloud-deployable configuration
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <chrono>
#include <atomic>
#include <mutex>
#include <nlohmann/json.hpp>
#include "redis_client.hpp"
#include "../config/configuration_manager.hpp"
#include "../logging/structured_logger.hpp"
#include "../error_handler.hpp"

using json = nlohmann::json;

namespace regulens {
namespace cache {

/**
 * Cache entry metadata
 */
struct CacheEntry {
    std::string key;
    json value;
    std::string value_type;
    size_t size_bytes;
    int ttl_seconds;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point expires_at;
    int hit_count;
    int miss_count;
    std::chrono::system_clock::time_point last_hit_at;
};

/**
 * Cache invalidation rule for automatic invalidation
 */
struct CacheInvalidationRule {
    std::string rule_id;
    std::string cache_key_pattern;
    std::string trigger_event;
    std::string trigger_table;
    std::string trigger_column;
    int ttl_seconds;
    bool is_active;
};

/**
 * Cache statistics for monitoring
 */
struct CacheStats {
    size_t total_entries;
    size_t total_size_bytes;
    size_t hit_count;
    size_t miss_count;
    double hit_rate;
    std::chrono::milliseconds avg_access_time_ms;
    json breakdown_by_type;
};

/**
 * High-level caching layer with automatic TTL management
 */
class RedisCacheManager {
public:
    RedisCacheManager(
        std::shared_ptr<RedisClient> redis_client,
        std::shared_ptr<StructuredLogger> logger,
        std::shared_ptr<ConfigurationManager> config,
        std::shared_ptr<ErrorHandler> error_handler
    );

    ~RedisCacheManager();

    /**
     * Initialize the cache manager
     */
    bool initialize();

    /**
     * Shutdown cache manager
     */
    void shutdown();

    /**
     * Set a cache value with automatic TTL
     * @param key Cache key
     * @param value Value to cache (auto-serialized if JSON)
     * @param ttl_seconds Time to live in seconds
     * @param value_type Optional type tag for statistics
     * @return true if successful
     */
    bool set(const std::string& key,
             const json& value,
             int ttl_seconds = 3600,
             const std::string& value_type = "generic");

    /**
     * Get a cached value
     * @param key Cache key
     * @return Cached value or empty json if not found
     */
    std::optional<json> get(const std::string& key);

    /**
     * Check if key exists in cache
     */
    bool exists(const std::string& key);

    /**
     * Delete a cache key
     */
    bool delete_key(const std::string& key);

    /**
     * Delete multiple keys matching pattern
     */
    size_t delete_pattern(const std::string& pattern);

    /**
     * Get all keys matching pattern
     */
    std::vector<std::string> get_keys(const std::string& pattern);

    /**
     * Clear entire cache
     */
    bool clear();

    /**
     * Set feature-specific TTL
     * @param feature_name Feature identifier
     * @param ttl_seconds Time to live
     */
    void set_feature_ttl(const std::string& feature_name, int ttl_seconds);

    /**
     * Get feature-specific TTL
     */
    int get_feature_ttl(const std::string& feature_name) const;

    /**
     * Warm up cache with precomputed values
     */
    bool warm_cache(const std::string& feature, const json& data);

    /**
     * Register a cache invalidation rule
     */
    bool register_invalidation_rule(const CacheInvalidationRule& rule);

    /**
     * Trigger cache invalidation for matching patterns
     */
    size_t invalidate_by_event(const std::string& event, const std::string& table = "", const std::string& column = "");

    /**
     * Get cache statistics
     */
    CacheStats get_statistics();

    /**
     * Get entry-level statistics
     */
    std::optional<CacheEntry> get_entry_stats(const std::string& key);

    /**
     * Monitor cache health
     */
    json get_health_status();

    /**
     * Batch operations for performance
     */
    bool batch_set(const std::vector<std::pair<std::string, json>>& entries, int ttl_seconds = 3600);

    /**
     * Batch get operations
     */
    std::map<std::string, json> batch_get(const std::vector<std::string>& keys);

    /**
     * Get all cache entries (for debugging/export)
     */
    std::vector<CacheEntry> get_all_entries();

    /**
     * Export cache to JSON for backup
     */
    json export_cache();

    /**
     * Import cache from JSON
     */
    bool import_cache(const json& cache_data);

    /**
     * Async cache operation
     */
    std::string async_warm_cache(const std::string& feature, const json& data);

    /**
     * Compact/optimize cache
     */
    json compact_cache();

    /**
     * Get cache configuration
     */
    json get_configuration();

private:
    std::shared_ptr<RedisClient> redis_client_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<ConfigurationManager> config_;
    std::shared_ptr<ErrorHandler> error_handler_;

    std::atomic<bool> running_;
    std::map<std::string, int> feature_ttls_;
    std::mutex ttl_mutex_;

    std::vector<CacheInvalidationRule> invalidation_rules_;
    std::mutex rules_mutex_;

    // Statistics tracking
    std::atomic<size_t> total_hits_;
    std::atomic<size_t> total_misses_;
    std::atomic<size_t> total_sets_;

    // Configuration
    int default_ttl_seconds_;
    size_t max_cache_size_bytes_;
    bool compression_enabled_;
    bool persistence_enabled_;

    // Private methods
    std::string serialize_value(const json& value);
    json deserialize_value(const std::string& data);
    std::string generate_cache_key(const std::string& prefix, const std::string& key);
    size_t estimate_size_bytes(const json& value);
    bool should_compress(size_t size_bytes);
    std::string compress_data(const std::string& data);
    std::string decompress_data(const std::string& data);
    bool check_cache_limits();
    void evict_expired_entries();
    void record_hit(const std::string& key);
    void record_miss(const std::string& key);
};

} // namespace cache
} // namespace regulens

#endif // REGULENS_REDIS_CACHE_MANAGER_HPP
