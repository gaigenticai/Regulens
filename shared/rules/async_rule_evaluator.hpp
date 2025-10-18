/**
 * AsyncRuleEvaluator - Production-Grade Async Rule Evaluation Engine
 *
 * Integrates AsyncJobManager with AdvancedRuleEngine for:
 * - Async/batch/streaming rule evaluation
 * - Priority-based rule scheduling
 * - Result caching with feature-specific TTLs
 * - Performance tracking and monitoring
 * - Learning engine feedback loops
 * - Comprehensive audit trails
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <nlohmann/json.hpp>
#include "../async_jobs/async_job_manager.hpp"
#include "../cache/redis_cache_manager.hpp"
#include "advanced_rule_engine.hpp"
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"
#include "../error_handler.hpp"
#include "../memory/learning_engine.hpp"

using json = nlohmann::json;

namespace regulens {
namespace rules {

/**
 * Evaluation metadata for tracking and analytics
 */
struct RuleEvaluationMetadata {
    std::string evaluation_id;
    std::string job_id;
    std::string rule_id;
    std::vector<std::string> rule_ids;
    std::string execution_mode;
    std::string priority;
    int batch_size;
    int total_items;
    bool use_cache;
    bool enable_learning;
    std::chrono::system_clock::time_point submitted_at;
    std::chrono::system_clock::time_point started_at;
    std::chrono::system_clock::time_point completed_at;
    int total_duration_ms;
};

/**
 * Rule evaluation result with metadata
 */
struct AsyncRuleResult {
    std::string evaluation_id;
    std::string job_id;
    bool success;
    json evaluation_results;
    json performance_metrics;
    RuleEvaluationMetadata metadata;
    std::string error_message;
};

/**
 * Batch evaluation context
 */
struct BatchEvaluationContext {
    std::vector<json> contexts;
    std::vector<std::string> rule_ids;
    size_t batch_size;
    int priority;
    bool use_cache;
    bool enable_learning;
};

/**
 * AsyncRuleEvaluator - Orchestrates async rule evaluation
 */
class AsyncRuleEvaluator {
public:
    AsyncRuleEvaluator(
        std::shared_ptr<async_jobs::AsyncJobManager> job_manager,
        std::shared_ptr<cache::RedisCacheManager> cache_manager,
        std::shared_ptr<AdvancedRuleEngine> rule_engine,
        std::shared_ptr<StructuredLogger> logger,
        std::shared_ptr<ErrorHandler> error_handler,
        std::shared_ptr<LearningEngine> learning_engine = nullptr
    );

    ~AsyncRuleEvaluator();

    /**
     * Initialize the evaluator
     */
    bool initialize();

    /**
     * Evaluate a single rule asynchronously
     * @param rule_id Rule to evaluate
     * @param context Evaluation context
     * @param execution_mode SYNC/ASYNC/BATCH/STREAMING
     * @param priority Job priority (LOW/MEDIUM/HIGH/CRITICAL)
     * @param use_cache Whether to use cache
     * @return Job ID for async mode, result for sync mode
     */
    json evaluate_rule_async(
        const std::string& rule_id,
        const json& context,
        const std::string& execution_mode = "ASYNCHRONOUS",
        const std::string& priority = "MEDIUM",
        bool use_cache = true
    );

    /**
     * Evaluate multiple rules asynchronously
     * @param rule_ids Rules to evaluate
     * @param context Evaluation context
     * @param execution_mode SYNC/ASYNC/BATCH/STREAMING
     * @param priority Job priority
     * @param use_cache Whether to use cache
     * @return Job ID or batch results
     */
    json evaluate_rules_async(
        const std::vector<std::string>& rule_ids,
        const json& context,
        const std::string& execution_mode = "ASYNCHRONOUS",
        const std::string& priority = "MEDIUM",
        bool use_cache = true
    );

    /**
     * Evaluate batch contexts
     * @param batch_context Batch evaluation context
     * @return Job ID for async/batch mode
     */
    json evaluate_batch(const BatchEvaluationContext& batch_context);

    /**
     * Get evaluation result by job ID
     * @param job_id Job identifier
     * @return Evaluation result or cached result
     */
    std::optional<AsyncRuleResult> get_evaluation_result(const std::string& job_id);

    /**
     * Poll evaluation status
     * @param job_id Job identifier
     * @return Current status with progress percentage
     */
    json get_evaluation_status(const std::string& job_id);

    /**
     * Cancel ongoing evaluation
     * @param job_id Job identifier
     * @return true if cancelled
     */
    bool cancel_evaluation(const std::string& job_id);

    /**
     * Get evaluation history
     * @param rule_id Optional rule filter
     * @param limit Result limit
     * @return Historical evaluations
     */
    std::vector<RuleEvaluationMetadata> get_evaluation_history(
        const std::string& rule_id = "",
        int limit = 100
    );

    /**
     * Get rule performance metrics
     * @param rule_id Rule identifier
     * @return Performance metrics (success rate, avg duration, etc.)
     */
    json get_rule_performance_metrics(const std::string& rule_id);

    /**
     * Submit feedback for evaluation
     * @param evaluation_id Evaluation identifier
     * @param feedback Feedback data
     * @param outcome true for positive, false for negative
     * @return Learning result
     */
    json submit_evaluation_feedback(
        const std::string& evaluation_id,
        const json& feedback,
        bool outcome
    );

    /**
     * Get analytics dashboard data
     * @return Summary statistics and trends
     */
    json get_analytics_dashboard();

    /**
     * Health check for evaluator
     * @return Health status
     */
    json get_health_status();

    /**
     * System metrics
     * @return System metrics
     */
    json get_system_metrics();

private:
    std::shared_ptr<async_jobs::AsyncJobManager> job_manager_;
    std::shared_ptr<cache::RedisCacheManager> cache_manager_;
    std::shared_ptr<AdvancedRuleEngine> rule_engine_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<ErrorHandler> error_handler_;
    std::shared_ptr<LearningEngine> learning_engine_;

    std::map<std::string, RuleEvaluationMetadata> evaluation_history_;
    std::mutex history_mutex_;

    // Statistics
    std::atomic<size_t> total_evaluations_;
    std::atomic<size_t> successful_evaluations_;
    std::atomic<size_t> failed_evaluations_;
    std::atomic<size_t> cache_hits_;
    std::atomic<size_t> cache_misses_;

    // Private helpers
    std::string generate_evaluation_id();
    
    async_jobs::ExecutionMode map_to_job_execution_mode(const std::string& mode);
    std::string map_from_job_execution_mode(async_jobs::ExecutionMode mode);
    
    async_jobs::JobPriority map_to_job_priority(const std::string& priority);
    std::string map_from_job_priority(async_jobs::JobPriority priority);

    json execute_sync_evaluation(const std::string& rule_id, const json& context);
    std::string execute_async_evaluation(const std::string& rule_id, const json& context, const std::string& priority);
    std::string execute_batch_evaluation(const std::vector<std::string>& rule_ids, const std::vector<json>& contexts, const std::string& priority);

    std::optional<json> get_cached_result(const std::string& cache_key);
    bool cache_result(const std::string& cache_key, const json& result, const std::string& rule_id);

    void record_evaluation_metadata(const RuleEvaluationMetadata& metadata);
    void submit_learning_feedback_async(const std::string& evaluation_id, const json& feedback, bool outcome);

    RuleEvaluationMetadata load_evaluation_metadata(const std::string& evaluation_id);
};

} // namespace rules
} // namespace regulens

#endif // REGULENS_ASYNC_RULE_EVALUATOR_HPP
