/**
 * Advanced Rule Engine
 * Production-grade fraud detection and policy enforcement system
 *
 * Features:
 * - Real-time rule evaluation and execution
 * - Fraud detection algorithms
 * - Policy enforcement with escalation
 * - Performance monitoring and optimization
 * - Audit trail generation
 */

#ifndef ADVANCED_RULE_ENGINE_HPP
#define ADVANCED_RULE_ENGINE_HPP

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <nlohmann/json.hpp>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"
#include "../config/configuration_manager.hpp"

namespace regulens {

enum class RuleSeverity {
    LOW,
    MEDIUM,
    HIGH,
    CRITICAL
};

enum class RuleAction {
    ALLOW,
    DENY,
    ESCALATE,
    MONITOR,
    ALERT,
    QUARANTINE
};

enum class RuleCategory {
    FRAUD_DETECTION,
    COMPLIANCE_CHECK,
    RISK_ASSESSMENT,
    BUSINESS_LOGIC,
    SECURITY_POLICY,
    AUDIT_PROCEDURE
};

struct RuleCondition {
    std::string field_name;
    std::string operator_type;  // "equals", "contains", "greater_than", "less_than", "regex", etc.
    nlohmann::json value;
    double weight = 1.0;  // Importance weight for scoring
};

struct RuleDefinition {
    std::string rule_id;
    std::string rule_name;
    std::string description;
    RuleCategory category;
    RuleSeverity severity;
    std::vector<RuleCondition> conditions;
    RuleAction action;
    double threshold_score = 0.5;  // Minimum score to trigger action
    std::vector<std::string> tags;
    bool enabled = true;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
};

struct EvaluationContext {
    std::string entity_id;
    std::string entity_type;  // "transaction", "user", "account", etc.
    nlohmann::json data;
    std::string source_system;
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> metadata;
};

struct RuleResult {
    std::string evaluation_id;
    std::string rule_id;
    std::string entity_id;
    double score = 0.0;
    bool triggered = false;
    RuleAction action;
    std::vector<std::string> matched_conditions;
    std::unordered_map<std::string, double> condition_scores;
    std::chrono::milliseconds processing_time;
    std::chrono::system_clock::time_point evaluated_at;
};

struct EvaluationBatch {
    std::string batch_id;
    std::vector<EvaluationContext> contexts;
    std::vector<RuleResult> results;
    std::chrono::milliseconds total_processing_time;
    int rules_evaluated = 0;
    int rules_triggered = 0;
};

class AdvancedRuleEngine {
public:
    AdvancedRuleEngine(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<StructuredLogger> logger,
        std::shared_ptr<ConfigurationManager> config_manager
    );

    ~AdvancedRuleEngine();

    // Initialization and lifecycle
    bool initialize();
    void shutdown();
    bool is_initialized() const;

    // Rule Management
    bool create_rule(const RuleDefinition& rule);
    bool update_rule(const std::string& rule_id, const RuleDefinition& rule);
    bool delete_rule(const std::string& rule_id);
    bool enable_rule(const std::string& rule_id);
    bool disable_rule(const std::string& rule_id);

    // Rule Evaluation
    RuleResult evaluate_entity(const EvaluationContext& context);
    EvaluationBatch evaluate_batch(const std::vector<EvaluationContext>& contexts);

    // Rule Retrieval
    std::optional<RuleDefinition> get_rule(const std::string& rule_id);
    std::vector<RuleDefinition> get_rules_by_category(RuleCategory category);
    std::vector<RuleDefinition> get_active_rules();

    // Performance and Monitoring
    void set_execution_timeout(std::chrono::milliseconds timeout);
    void set_max_parallel_executions(int max_parallel);
    nlohmann::json get_performance_stats();
    nlohmann::json get_rule_execution_stats(const std::string& rule_id);

    // Configuration
    void set_cache_enabled(bool enabled);
    void set_cache_ttl_seconds(int ttl_seconds);
    void set_batch_processing_enabled(bool enabled);
    void set_max_batch_size(int size);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<ConfigurationManager> config_manager_;

    // Configuration
    std::chrono::milliseconds execution_timeout_ = std::chrono::milliseconds(5000);
    int max_parallel_executions_ = 10;
    bool cache_enabled_ = true;
    int cache_ttl_seconds_ = 300;
    bool batch_processing_enabled_ = true;
    int max_batch_size_ = 100;

    // Internal state
    bool initialized_ = false;
    std::mutex rules_mutex_;
    std::unordered_map<std::string, RuleDefinition> rules_cache_;
    std::chrono::system_clock::time_point cache_last_updated_;

    // Performance tracking
    std::mutex stats_mutex_;
    int64_t total_evaluations_ = 0;
    int64_t total_triggered_rules_ = 0;
    std::chrono::milliseconds total_processing_time_ = std::chrono::milliseconds(0);

    // Rule execution statistics
    std::unordered_map<std::string, int64_t> rule_execution_counts_;
    std::unordered_map<std::string, std::chrono::milliseconds> rule_execution_times_;
    std::unordered_map<std::string, int64_t> rule_trigger_counts_;

    // Internal methods
    bool load_rules_from_database();
    bool save_rule_to_database(const RuleDefinition& rule);
    bool update_rule_in_cache(const RuleDefinition& rule);
    void remove_rule_from_cache(const std::string& rule_id);

    double evaluate_conditions(const RuleDefinition& rule, const EvaluationContext& context,
                              std::vector<std::string>& matched_conditions,
                              std::unordered_map<std::string, double>& condition_scores);

    bool evaluate_condition(const RuleCondition& condition, const nlohmann::json& data);

    RuleResult create_rule_result(const RuleDefinition& rule, const EvaluationContext& context,
                                 double score, const std::vector<std::string>& matched_conditions,
                                 const std::unordered_map<std::string, double>& condition_scores,
                                 std::chrono::milliseconds processing_time);

    void update_performance_stats(const RuleResult& result);
    void log_rule_evaluation(const RuleResult& result, const EvaluationContext& context);

    // Batch processing
    EvaluationBatch process_batch_sequential(const std::vector<EvaluationContext>& contexts);
    EvaluationBatch process_batch_parallel(const std::vector<EvaluationContext>& contexts);

    // Utility methods
    std::string generate_evaluation_id();
    std::string generate_rule_id();
    bool validate_rule_definition(const RuleDefinition& rule);
    nlohmann::json serialize_rule_result(const RuleResult& result);
};

} // namespace regulens

#endif // ADVANCED_RULE_ENGINE_HPP

