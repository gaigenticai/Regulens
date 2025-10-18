/**
 * ResilientEvaluatorWrapper - Production-Grade Fault Tolerance Layer
 *
 * Wraps AsyncRuleEvaluator and AsyncMCDADecisionService with:
 * - CircuitBreaker pattern for failure handling
 * - Fallback strategies for degraded operation
 * - Retry logic with exponential backoff
 * - Failure tracking and recovery
 * - Comprehensive health monitoring
 */

#pragma once

#include <string>
#include <memory>
#include <map>
#include <atomic>
#include <nlohmann/json.hpp>
#include "../rules/async_rule_evaluator.hpp"
#include "../decisions/async_mcda_decision_service.hpp"
#include "circuit_breaker.hpp"
#include "../logging/structured_logger.hpp"
#include "../error_handler.hpp"

using json = nlohmann::json;

namespace regulens {
namespace resilience {

/**
 * Failure recovery strategy enumeration
 */
enum class FailureRecoveryStrategy {
    RETRY_EXPONENTIAL_BACKOFF,
    FALLBACK_CACHED_RESULT,
    FALLBACK_DEFAULT_DECISION,
    DEGRADE_SERVICE_GRACEFULLY,
    REJECT_WITH_ERROR
};

/**
 * Service health status
 */
struct ServiceHealthStatus {
    std::string service_name;
    bool is_healthy;
    int failure_count;
    int success_count;
    double success_rate_percent;
    std::string circuit_breaker_state;  // CLOSED, OPEN, HALF_OPEN
    long last_failure_timestamp;
    long last_success_timestamp;
};

/**
 * ResilientEvaluatorWrapper provides fault tolerance for decision systems
 */
class ResilientEvaluatorWrapper {
public:
    ResilientEvaluatorWrapper(
        std::shared_ptr<rules::AsyncRuleEvaluator> rule_evaluator,
        std::shared_ptr<decisions::AsyncMCDADecisionService> mcda_service,
        std::shared_ptr<CircuitBreaker> rule_circuit_breaker,
        std::shared_ptr<CircuitBreaker> mcda_circuit_breaker,
        std::shared_ptr<StructuredLogger> logger,
        std::shared_ptr<ErrorHandler> error_handler
    );

    ~ResilientEvaluatorWrapper();

    /**
     * Initialize resilience wrapper
     */
    bool initialize();

    /**
     * Resilient rule evaluation with fallback
     */
    json evaluate_rule_resilient(
        const std::string& rule_id,
        const json& context,
        const std::string& execution_mode = "ASYNCHRONOUS",
        FailureRecoveryStrategy recovery_strategy = FailureRecoveryStrategy::RETRY_EXPONENTIAL_BACKOFF
    );

    /**
     * Resilient multi-rule evaluation with fallback
     */
    json evaluate_rules_resilient(
        const std::vector<std::string>& rule_ids,
        const json& context,
        const std::string& execution_mode = "ASYNCHRONOUS",
        FailureRecoveryStrategy recovery_strategy = FailureRecoveryStrategy::RETRY_EXPONENTIAL_BACKOFF
    );

    /**
     * Resilient MCDA analysis with fallback
     */
    json analyze_decision_resilient(
        const std::string& decision_problem,
        const std::vector<decisions::DecisionCriterion>& criteria,
        const std::vector<decisions::DecisionAlternative>& alternatives,
        decisions::MCDAAlgorithm algorithm,
        const std::string& execution_mode = "ASYNCHRONOUS",
        FailureRecoveryStrategy recovery_strategy = FailureRecoveryStrategy::RETRY_EXPONENTIAL_BACKOFF
    );

    /**
     * Resilient MCDA ensemble analysis
     */
    json analyze_decision_ensemble_resilient(
        const std::string& decision_problem,
        const std::vector<decisions::DecisionCriterion>& criteria,
        const std::vector<decisions::DecisionAlternative>& alternatives,
        const std::vector<decisions::MCDAAlgorithm>& algorithms,
        FailureRecoveryStrategy recovery_strategy = FailureRecoveryStrategy::RETRY_EXPONENTIAL_BACKOFF
    );

    /**
     * Get service health status
     */
    ServiceHealthStatus get_service_health(const std::string& service_name);

    /**
     * Get all services health status
     */
    json get_all_services_health();

    /**
     * Reset circuit breaker for service
     */
    bool reset_circuit_breaker(const std::string& service_name);

    /**
     * Disable resilience for a service (testing only)
     */
    bool disable_resilience(const std::string& service_name);

    /**
     * Enable resilience for a service
     */
    bool enable_resilience(const std::string& service_name);

    /**
     * Get resilience metrics
     */
    json get_resilience_metrics();

    /**
     * Get fallback statistics
     */
    json get_fallback_statistics();

private:
    std::shared_ptr<rules::AsyncRuleEvaluator> rule_evaluator_;
    std::shared_ptr<decisions::AsyncMCDADecisionService> mcda_service_;
    std::shared_ptr<CircuitBreaker> rule_circuit_breaker_;
    std::shared_ptr<CircuitBreaker> mcda_circuit_breaker_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<ErrorHandler> error_handler_;

    // Resilience state tracking
    std::map<std::string, bool> service_resilience_enabled_;
    std::mutex resilience_state_mutex_;

    // Failure tracking
    std::atomic<size_t> total_invocations_;
    std::atomic<size_t> total_failures_;
    std::atomic<size_t> total_fallbacks_;
    std::atomic<size_t> total_circuit_breaks_;

    // Result cache for fallback
    std::map<std::string, json> fallback_cache_;
    std::mutex cache_mutex_;

    // Last failure timestamps
    std::atomic<long> last_rule_eval_failure_;
    std::atomic<long> last_mcda_failure_;

    // Private helper methods
    json execute_with_circuit_breaker(
        const std::string& service_name,
        std::function<json()> operation,
        FailureRecoveryStrategy recovery_strategy
    );

    json execute_with_retry(
        std::function<json()> operation,
        int max_retries = 3,
        int initial_backoff_ms = 100
    );

    json execute_fallback_rule_evaluation(
        const std::string& rule_id,
        const json& context
    );

    json execute_fallback_mcda_analysis(
        const std::string& decision_problem,
        const std::vector<decisions::DecisionCriterion>& criteria,
        const std::vector<decisions::DecisionAlternative>& alternatives
    );

    std::string get_cache_key(const std::string& prefix, const json& data);
    
    void cache_successful_result(const std::string& key, const json& result);
    std::optional<json> get_cached_result(const std::string& key);

    void record_failure(const std::string& service_name);
    void record_success(const std::string& service_name);

    bool is_resilience_enabled(const std::string& service_name) const;
};

} // namespace resilience
} // namespace regulens

#endif // REGULENS_RESILIENT_EVALUATOR_WRAPPER_HPP
