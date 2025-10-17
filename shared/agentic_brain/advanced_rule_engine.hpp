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

// JSON serialization for RuleResult
inline void to_json(nlohmann::json& j, const RuleResult& result) {
    j = nlohmann::json{
        {"evaluation_id", result.evaluation_id},
        {"rule_id", result.rule_id},
        {"entity_id", result.entity_id},
        {"score", result.score},
        {"triggered", result.triggered},
        {"action", result.action},
        {"matched_conditions", result.matched_conditions},
        {"condition_scores", result.condition_scores},
        {"processing_time_ms", result.processing_time.count()},
        {"evaluated_at", std::chrono::duration_cast<std::chrono::seconds>(
            result.evaluated_at.time_since_epoch()).count()}
    };
}

inline void from_json(const nlohmann::json& j, RuleResult& result) {
    result.evaluation_id = j.value("evaluation_id", "");
    result.rule_id = j.value("rule_id", "");
    result.entity_id = j.value("entity_id", "");
    result.score = j.value("score", 0.0);
    result.triggered = j.value("triggered", false);
    result.action = j.value("action", RuleAction::ALLOW);
    result.matched_conditions = j.value("matched_conditions", std::vector<std::string>{});
    result.condition_scores = j.value("condition_scores", std::unordered_map<std::string, double>{});

    auto proc_time_ms = j.value("processing_time_ms", 0);
    result.processing_time = std::chrono::milliseconds(proc_time_ms);

    auto eval_ts = j.value("evaluated_at", static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()));
    result.evaluated_at = std::chrono::system_clock::time_point(
        std::chrono::seconds(eval_ts));
}

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

// JSON serialization for RuleDefinition
inline void to_json(nlohmann::json& j, const RuleDefinition& rule) {
    j = nlohmann::json{
        {"rule_id", rule.rule_id},
        {"rule_name", rule.rule_name},
        {"description", rule.description},
        {"category", static_cast<int>(rule.category)},
        {"severity", static_cast<int>(rule.severity)},
        {"conditions", rule.conditions},
        {"action", rule.action},
        {"threshold_score", rule.threshold_score},
        {"tags", rule.tags},
        {"enabled", rule.enabled},
        {"created_at", std::chrono::duration_cast<std::chrono::seconds>(
            rule.created_at.time_since_epoch()).count()},
        {"updated_at", std::chrono::duration_cast<std::chrono::seconds>(
            rule.updated_at.time_since_epoch()).count()}
    };
}

inline void from_json(const nlohmann::json& j, RuleDefinition& rule) {
    rule.rule_id = j.value("rule_id", "");
    rule.rule_name = j.value("rule_name", "");
    rule.description = j.value("description", "");
    rule.category = static_cast<RuleCategory>(j.value("category", 0));
    rule.severity = static_cast<RuleSeverity>(j.value("severity", 0));
    rule.conditions = j.value("conditions", std::vector<RuleCondition>{});
    rule.action = j.value("action", RuleAction{});
    rule.threshold_score = j.value("threshold_score", 0.5);
    rule.tags = j.value("tags", std::vector<std::string>{});
    rule.enabled = j.value("enabled", true);

    auto created_ts = j.value("created_at", static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()));
    rule.created_at = std::chrono::system_clock::time_point(
        std::chrono::seconds(created_ts));

    auto updated_ts = j.value("updated_at", static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()));
    rule.updated_at = std::chrono::system_clock::time_point(
        std::chrono::seconds(updated_ts));
}

// JSON serialization for EvaluationContext
inline void to_json(nlohmann::json& j, const EvaluationContext& context) {
    j = nlohmann::json{
        {"entity_id", context.entity_id},
        {"entity_type", context.entity_type},
        {"data", context.data},
        {"source_system", context.source_system},
        {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
            context.timestamp.time_since_epoch()).count()},
        {"metadata", context.metadata}
    };
}

inline void from_json(const nlohmann::json& j, EvaluationContext& context) {
    context.entity_id = j.value("entity_id", "");
    context.entity_type = j.value("entity_type", "");
    context.data = j.value("data", nlohmann::json{});
    context.source_system = j.value("source_system", "");

    auto ts = j.value("timestamp", static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()));
    context.timestamp = std::chrono::system_clock::time_point(
        std::chrono::seconds(ts));

    context.metadata = j.value("metadata", std::unordered_map<std::string, std::string>{});
}

// JSON serialization for enums
inline void to_json(nlohmann::json& j, const RuleAction& action) {
    switch (action) {
        case RuleAction::ALLOW: j = "ALLOW"; break;
        case RuleAction::DENY: j = "DENY"; break;
        case RuleAction::ESCALATE: j = "ESCALATE"; break;
        case RuleAction::MONITOR: j = "MONITOR"; break;
        case RuleAction::ALERT: j = "ALERT"; break;
        case RuleAction::QUARANTINE: j = "QUARANTINE"; break;
        default: j = "ALLOW"; break;
    }
}

inline void from_json(const nlohmann::json& j, RuleAction& action) {
    std::string str = j.get<std::string>();
    if (str == "ALLOW") action = RuleAction::ALLOW;
    else if (str == "DENY") action = RuleAction::DENY;
    else if (str == "ESCALATE") action = RuleAction::ESCALATE;
    else if (str == "MONITOR") action = RuleAction::MONITOR;
    else if (str == "ALERT") action = RuleAction::ALERT;
    else if (str == "QUARANTINE") action = RuleAction::QUARANTINE;
    else action = RuleAction::ALLOW;
}

inline void to_json(nlohmann::json& j, const RuleSeverity& severity) {
    switch (severity) {
        case RuleSeverity::LOW: j = "LOW"; break;
        case RuleSeverity::MEDIUM: j = "MEDIUM"; break;
        case RuleSeverity::HIGH: j = "HIGH"; break;
        case RuleSeverity::CRITICAL: j = "CRITICAL"; break;
        default: j = "MEDIUM"; break;
    }
}

inline void from_json(const nlohmann::json& j, RuleSeverity& severity) {
    std::string str = j.get<std::string>();
    if (str == "LOW") severity = RuleSeverity::LOW;
    else if (str == "MEDIUM") severity = RuleSeverity::MEDIUM;
    else if (str == "HIGH") severity = RuleSeverity::HIGH;
    else if (str == "CRITICAL") severity = RuleSeverity::CRITICAL;
    else severity = RuleSeverity::MEDIUM;
}

inline void to_json(nlohmann::json& j, const RuleCategory& category) {
    switch (category) {
        case RuleCategory::FRAUD_DETECTION: j = "FRAUD_DETECTION"; break;
        case RuleCategory::COMPLIANCE_CHECK: j = "COMPLIANCE_CHECK"; break;
        case RuleCategory::RISK_ASSESSMENT: j = "RISK_ASSESSMENT"; break;
        case RuleCategory::SECURITY_MONITORING: j = "SECURITY_MONITORING"; break;
        case RuleCategory::TRANSACTION_MONITORING: j = "TRANSACTION_MONITORING"; break;
        default: j = "COMPLIANCE_CHECK"; break;
    }
}

inline void from_json(const nlohmann::json& j, RuleCategory& category) {
    std::string str = j.get<std::string>();
    if (str == "FRAUD_DETECTION") category = RuleCategory::FRAUD_DETECTION;
    else if (str == "COMPLIANCE_CHECK") category = RuleCategory::COMPLIANCE_CHECK;
    else if (str == "RISK_ASSESSMENT") category = RuleCategory::RISK_ASSESSMENT;
    else if (str == "SECURITY_MONITORING") category = RuleCategory::SECURITY_MONITORING;
    else if (str == "TRANSACTION_MONITORING") category = RuleCategory::TRANSACTION_MONITORING;
    else category = RuleCategory::COMPLIANCE_CHECK;
}

} // namespace regulens

#endif // ADVANCED_RULE_ENGINE_HPP

