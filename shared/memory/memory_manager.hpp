/**
 * Advanced Memory Manager for Compliance Agents
 *
 * Intelligent memory lifecycle management with consolidation, forgetting,
 * and memory optimization for compliance AI systems.
 *
 * Features:
 * - Memory consolidation and compression
 * - Intelligent forgetting based on importance and age
 * - Memory optimization and deduplication
 * - Memory hierarchy management (episodic, semantic, procedural)
 * - Memory health monitoring and maintenance
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <optional>
#include <nlohmann/json.hpp>

#include "../config/configuration_manager.hpp"
#include "../logging/structured_logger.hpp"
#include "../error_handler.hpp"
#include "conversation_memory.hpp"
#include "learning_engine.hpp"

namespace regulens {

/**
 * @brief Memory consolidation strategy
 */
enum class ConsolidationStrategy {
    MERGE_SIMILAR,     // Merge similar memories into single consolidated memory
    EXTRACT_PATTERNS,  // Extract patterns and create semantic memories
    COMPRESS_DETAILS,  // Compress detailed episodic memories
    PROMOTE_IMPORTANT, // Promote important memories to higher tiers
    AGGREGATE_STATS    // Aggregate statistical information
};

/**
 * @brief Forgetting strategy for memory cleanup
 */
enum class ForgettingStrategy {
    TIME_BASED,        // Forget based on age
    IMPORTANCE_BASED,  // Forget based on importance scores
    USAGE_BASED,       // Forget based on access frequency
    DOMAIN_BASED,      // Forget domain-specific memories
    ADAPTIVE,          // Adaptive forgetting based on memory pressure
    PRESERVATION       // Never forget (for critical memories)
};

/**
 * @brief Memory tier hierarchy
 */
enum class MemoryTier {
    WORKING,           // Short-term working memory (seconds to minutes)
    EPISODIC,          // Specific events and conversations (hours to days)
    SEMANTIC,          // General knowledge and patterns (days to weeks)
    PROCEDURAL,        // Learned procedures and workflows (weeks to months)
    ARCHIVAL           // Long-term archival memory (months to years)
};

/**
 * @brief Memory health metrics
 */
struct MemoryHealthMetrics {
    size_t total_memories;
    size_t working_memories;
    size_t episodic_memories;
    size_t semantic_memories;
    size_t procedural_memories;
    size_t archival_memories;

    double average_importance;
    double memory_pressure;        // 0.0-1.0 (higher = more pressure)
    double consolidation_ratio;    // How much memory has been consolidated
    double forgetting_rate;        // Rate at which memories are being forgotten

    std::chrono::system_clock::time_point last_consolidation;
    std::chrono::system_clock::time_point last_forgetting;
    std::chrono::system_clock::time_point last_optimization;

    MemoryHealthMetrics()
        : total_memories(0), working_memories(0), episodic_memories(0),
          semantic_memories(0), procedural_memories(0), archival_memories(0),
          average_importance(0.5), memory_pressure(0.0), consolidation_ratio(0.0),
          forgetting_rate(0.0) {}
};

/**
 * @brief Memory consolidation result
 */
struct ConsolidationResult {
    bool success;
    size_t memories_processed;
    size_t memories_consolidated;
    size_t memories_promoted;
    size_t memories_compressed;
    double compression_ratio;
    std::chrono::milliseconds processing_time;
    std::vector<std::string> consolidation_steps;
    std::optional<std::string> error_message;

    ConsolidationResult()
        : success(false), memories_processed(0), memories_consolidated(0),
          memories_promoted(0), memories_compressed(0), compression_ratio(1.0),
          processing_time(std::chrono::milliseconds(0)) {}
};

/**
 * @brief Memory optimization plan
 */
struct MemoryOptimizationPlan {
    std::vector<ConsolidationStrategy> consolidation_strategies;
    ForgettingStrategy forgetting_strategy;
    std::unordered_map<MemoryTier, size_t> target_sizes; // Target size per tier
    std::chrono::hours optimization_interval;
    std::chrono::hours consolidation_interval; // Interval for memory consolidation
    double memory_pressure_threshold; // When to trigger optimization
    double importance_threshold; // Minimum importance score to retain memories
    size_t max_working_memory_size; // Maximum working memory entries
    size_t max_episodic_memory_size; // Maximum episodic memory entries
    std::string eviction_policy; // Policy for evicting memories (e.g., "LRU", "LFU", "ADAPTIVE")
    bool enable_automatic_optimization;

    MemoryOptimizationPlan()
        : forgetting_strategy(ForgettingStrategy::ADAPTIVE),
          optimization_interval(std::chrono::hours(24)),
          consolidation_interval(std::chrono::hours(24)),
          memory_pressure_threshold(0.8),
          importance_threshold(0.3),
          max_working_memory_size(1000),
          max_episodic_memory_size(10000),
          eviction_policy("ADAPTIVE"),
          enable_automatic_optimization(true) {}
};

/**
 * @brief Memory manager for comprehensive memory lifecycle management
 */
class MemoryManager {
public:
    MemoryManager(std::shared_ptr<ConfigurationManager> config,
                 std::shared_ptr<ConversationMemory> conversation_memory,
                 std::shared_ptr<LearningEngine> learning_engine,
                 StructuredLogger* logger,
                 ErrorHandler* error_handler);

    ~MemoryManager();

    /**
     * @brief Initialize the memory manager
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Perform memory consolidation
     * @param strategy Consolidation strategy to use
     * @param max_age Maximum age of memories to consolidate
     * @return Consolidation results
     */
    ConsolidationResult consolidate_memories(ConsolidationStrategy strategy = ConsolidationStrategy::MERGE_SIMILAR,
                                           std::chrono::hours max_age = std::chrono::hours(24));

    /**
     * @brief Perform intelligent forgetting
     * @param strategy Forgetting strategy to use
     * @param max_age Maximum age for time-based forgetting
     * @param min_importance Minimum importance threshold
     * @return Number of memories forgotten
     */
    size_t perform_forgetting(ForgettingStrategy strategy = ForgettingStrategy::ADAPTIVE,
                            std::chrono::hours max_age = std::chrono::hours(720),
                            double min_importance = 0.2);

    /**
     * @brief Optimize memory usage
     * @param plan Optimization plan to execute
     * @return true if optimization successful
     */
    bool optimize_memory(const MemoryOptimizationPlan& plan);

    /**
     * @brief Get current memory health metrics
     * @return Memory health metrics
     */
    MemoryHealthMetrics get_memory_health() const;

    /**
     * @brief Check if memory optimization is needed
     * @return true if optimization recommended
     */
    bool needs_optimization() const;

    /**
     * @brief Perform emergency memory cleanup
     * @param target_memory_pressure Target memory pressure (0.0-1.0)
     * @return Number of memories cleaned up
     */
    size_t emergency_cleanup(double target_memory_pressure = 0.7);

    /**
     * @brief Backup critical memories
     * @param backup_path Path to save backup
     * @return true if backup successful
     */
    bool backup_critical_memories(const std::string& backup_path);

    /**
     * @brief Restore memories from backup
     * @param backup_path Path to restore from
     * @return Number of memories restored
     */
    size_t restore_memories(const std::string& backup_path);

    /**
     * @brief Get memory management statistics
     * @return JSON with memory management stats
     */
    nlohmann::json get_management_statistics() const;

    /**
     * @brief Configure memory optimization plan
     * @param plan New optimization plan
     * @return true if configuration successful
     */
    bool configure_optimization_plan(const MemoryOptimizationPlan& plan);

    /**
     * @brief Get current optimization plan
     * @return Current optimization plan
     */
    MemoryOptimizationPlan get_optimization_plan() const;

    /**
     * @brief Manually trigger memory maintenance
     * @return Maintenance results
     */
    nlohmann::json perform_maintenance();

private:
    std::shared_ptr<ConfigurationManager> config_;
    std::shared_ptr<ConversationMemory> conversation_memory_;
    std::shared_ptr<LearningEngine> learning_engine_;
    StructuredLogger* logger_;
    ErrorHandler* error_handler_;

    MemoryOptimizationPlan optimization_plan_;
    MemoryHealthMetrics health_metrics_;
    mutable std::mutex manager_mutex_;

    // Management statistics
    std::atomic<size_t> consolidations_performed_{0};
    std::atomic<size_t> forgettings_performed_{0};
    std::atomic<size_t> optimizations_performed_{0};
    std::atomic<size_t> emergency_cleanups_{0};

    // Scheduling for automatic optimization
    std::shared_ptr<std::thread> optimization_timer_thread_;
    std::atomic<bool> cancel_scheduled_optimization_{false};
    std::chrono::system_clock::time_point next_scheduled_optimization_;

    /**
     * @brief Update memory health metrics
     */
    void update_health_metrics();

    /**
     * @brief Calculate current memory pressure
     * @return Memory pressure (0.0-1.0)
     */
    double calculate_memory_pressure() const;

    /**
     * @brief Get memories older than specified cutoff time
     * @param cutoff_time Time threshold for memory age
     * @return Vector of memory entries older than cutoff
     */
    std::vector<MemoryEntry> get_memories_older_than(std::chrono::system_clock::time_point cutoff_time);

    /**
     * @brief Merge similar memories
     * @param memories Memories to potentially merge
     * @return Consolidation result
     */
    ConsolidationResult merge_similar_memories(const std::vector<MemoryEntry>& memories);

    /**
     * @brief Extract patterns from episodic memories
     * @param memories Episodic memories to analyze
     * @return Patterns extracted
     */
    std::vector<nlohmann::json> extract_patterns_from_memories(const std::vector<MemoryEntry>& memories);

    /**
     * @brief Compress memory details
     * @param memory Memory to compress
     * @return Compressed memory
     */
    MemoryEntry compress_memory_details(const MemoryEntry& memory);

    /**
     * @brief Promote memory to higher tier
     * @param memory Memory to promote
     * @param target_tier Target memory tier
     * @return Promoted memory
     */
    MemoryEntry promote_memory_tier(const MemoryEntry& memory, MemoryTier target_tier);

    /**
     * @brief Identify critical memories that should never be forgotten
     * @return List of critical memory IDs
     */
    std::vector<std::string> identify_critical_memories() const;

    /**
     * @brief Calculate optimal forgetting threshold
     * @param current_pressure Current memory pressure
     * @return Optimal forgetting threshold
     */
    double calculate_optimal_forgetting_threshold(double current_pressure) const;

    /**
     * @brief Perform domain-based forgetting
     * @param domain Domain to forget memories from
     * @param max_age Maximum age to keep
     * @return Number of memories forgotten
     */
    size_t forget_domain_memories(const std::string& domain, std::chrono::hours max_age);

    /**
     * @brief Validate memory optimization plan
     * @param plan Plan to validate
     * @return true if plan is valid
     */
    bool validate_optimization_plan(const MemoryOptimizationPlan& plan) const;

    /**
     * @brief Schedule next optimization
     * @param plan Optimization plan
     */
    void schedule_next_optimization(const MemoryOptimizationPlan& plan);

    /**
     * @brief Log memory management operation
     * @param operation Operation performed
     * @param details Operation details
     */
    void log_management_operation(const std::string& operation, const nlohmann::json& details);
};

/**
 * @brief Memory tier manager for hierarchical memory organization
 */
class MemoryTierManager {
public:
    MemoryTierManager(std::shared_ptr<ConfigurationManager> config,
                     StructuredLogger* logger);

    /**
     * @brief Assign memory to appropriate tier
     * @param memory Memory to assign
     * @return Assigned memory tier
     */
    MemoryTier assign_memory_tier(const MemoryEntry& memory);

    /**
     * @brief Get tier-specific retention policies
     * @param tier Memory tier
     * @return Retention policy (max age, min importance)
     */
    std::pair<std::chrono::hours, double> get_tier_retention_policy(MemoryTier tier) const;

    /**
     * @brief Check if memory should be promoted to higher tier
     * @param memory Memory to check
     * @param current_tier Current tier
     * @return Target tier if promotion recommended, empty otherwise
     */
    std::optional<MemoryTier> should_promote_memory(const MemoryEntry& memory, MemoryTier current_tier) const;

    /**
     * @brief Get tier statistics
     * @return JSON with tier statistics
     */
    nlohmann::json get_tier_statistics() const;

private:
    std::shared_ptr<ConfigurationManager> config_;
    StructuredLogger* logger_;

    /**
     * @brief Calculate memory tier based on characteristics
     * @param memory Memory to analyze
     * @return Appropriate memory tier
     */
    MemoryTier calculate_memory_tier(const MemoryEntry& memory) const;

    /**
     * @brief Check if memory meets tier promotion criteria
     * @param memory Memory to check
     * @param target_tier Target tier
     * @return true if promotion criteria met
     */
    bool meets_promotion_criteria(const MemoryEntry& memory, MemoryTier target_tier) const;
};

/**
 * @brief Memory health monitor
 */
class MemoryHealthMonitor {
public:
    MemoryHealthMonitor(std::shared_ptr<ConfigurationManager> config,
                       StructuredLogger* logger);

    /**
     * @brief Monitor memory health and generate alerts
     * @param health_metrics Current health metrics
     * @return Health alerts and recommendations
     */
    nlohmann::json monitor_memory_health(const MemoryHealthMetrics& health_metrics);

    /**
     * @brief Predict memory pressure trends
     * @param historical_metrics Historical health metrics
     * @return Pressure trend prediction
     */
    nlohmann::json predict_memory_pressure(const std::vector<MemoryHealthMetrics>& historical_metrics);

    /**
     * @brief Generate memory health report
     * @param health_metrics Current health metrics
     * @return Comprehensive health report
     */
    nlohmann::json generate_health_report(const MemoryHealthMetrics& health_metrics);

    /**
     * @brief Check for memory anomalies
     * @param health_metrics Current health metrics
     * @param baseline_metrics Baseline metrics
     * @return Detected anomalies
     */
    std::vector<std::string> detect_memory_anomalies(const MemoryHealthMetrics& health_metrics,
                                                    const MemoryHealthMetrics& baseline_metrics);

private:
    std::shared_ptr<ConfigurationManager> config_;
    StructuredLogger* logger_;

    /**
     * @brief Calculate memory health score
     * @param metrics Health metrics
     * @return Health score (0.0-1.0, higher = healthier)
     */
    double calculate_memory_health_score(const MemoryHealthMetrics& metrics) const;

    /**
     * @brief Identify potential memory issues
     * @param metrics Health metrics
     * @return List of identified issues
     */
    std::vector<std::string> identify_memory_issues(const MemoryHealthMetrics& metrics) const;

    /**
     * @brief Generate health recommendations
     * @param issues Identified issues
     * @return Health recommendations
     */
    std::vector<std::string> generate_health_recommendations(const std::vector<std::string>& issues) const;
};

/**
 * @brief Create memory manager instance
 * @param config Configuration manager
 * @param conversation_memory Conversation memory system
 * @param learning_engine Learning engine
 * @param logger Structured logger
 * @param error_handler Error handler
 * @return Shared pointer to memory manager
 */
std::shared_ptr<MemoryManager> create_memory_manager(
    std::shared_ptr<ConfigurationManager> config,
    std::shared_ptr<ConversationMemory> conversation_memory,
    std::shared_ptr<LearningEngine> learning_engine,
    StructuredLogger* logger,
    ErrorHandler* error_handler);

/**
 * @brief Create memory tier manager instance
 * @param config Configuration manager
 * @param logger Structured logger
 * @return Shared pointer to memory tier manager
 */
std::shared_ptr<MemoryTierManager> create_memory_tier_manager(
    std::shared_ptr<ConfigurationManager> config,
    StructuredLogger* logger);

/**
 * @brief Create memory health monitor instance
 * @param config Configuration manager
 * @param logger Structured logger
 * @return Shared pointer to memory health monitor
 */
std::shared_ptr<MemoryHealthMonitor> create_memory_health_monitor(
    std::shared_ptr<ConfigurationManager> config,
    StructuredLogger* logger);

} // namespace regulens
