/**
 * Advanced Rule Engine
 * Production-grade fraud detection and policy enforcement system
 */

#ifndef ADVANCED_RULE_ENGINE_HPP
#define ADVANCED_RULE_ENGINE_HPP

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>
#include <chrono>
#include <mutex>
#include <nlohmann/json.hpp>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"
#include "../config/dynamic_config_manager.hpp"
#include "../agentic_brain/llm_interface.hpp"

namespace regulens {

enum class RuleExecutionMode {
    SYNCHRONOUS,   // Execute immediately and return result
    ASYNCHRONOUS,  // Queue for background execution
    BATCH,         // Execute as part of batch processing
    STREAMING      // Continuous rule evaluation
};

enum class RulePriority {
    LOW = 1,
    MEDIUM = 2,
    HIGH = 3,
    CRITICAL = 4
};

enum class RuleExecutionResult {
    PASS,           // Rule passed (no fraud detected)
    FAIL,           // Rule failed (fraud detected)
    ERROR,          // Rule execution error
    TIMEOUT,        // Rule execution timed out
    SKIPPED         // Rule was skipped
};

enum class FraudRiskLevel {
    LOW,
    MEDIUM,
    HIGH,
    CRITICAL
};

struct RuleExecutionContext {
    std::string transaction_id;
    std::string user_id;
    std::string session_id;
    nlohmann::json transaction_data;
    nlohmann::json user_profile;
    nlohmann::json historical_data;
    std::chrono::system_clock::time_point execution_time;
    std::string source_system;
    std::unordered_map<std::string, std::string> metadata;
};

struct RuleExecutionResultDetail {
    std::string rule_id;
    std::string rule_name;
    RuleExecutionResult result;
    double confidence_score = 0.0;
    FraudRiskLevel risk_level = FraudRiskLevel::LOW;
    nlohmann::json rule_output;
    std::string error_message;
    std::chrono::milliseconds execution_time;
    std::vector<std::string> triggered_conditions;
    std::unordered_map<std::string, double> risk_factors;
};

struct FraudDetectionResult {
    std::string transaction_id;
    bool is_fraudulent = false;
    FraudRiskLevel overall_risk = FraudRiskLevel::LOW;
    double fraud_score = 0.0;
    std::vector<RuleExecutionResultDetail> rule_results;
    nlohmann::json aggregated_findings;
    std::chrono::system_clock::time_point detection_time;
    std::string processing_duration;
    std::string recommendation; // "APPROVE", "REVIEW", "BLOCK"
};

struct RuleDefinition {
    std::string rule_id;
    std::string name;
    std::string description;
    RulePriority priority = RulePriority::MEDIUM;
    std::string rule_type; // "VALIDATION", "SCORING", "PATTERN", "MACHINE_LEARNING"
    nlohmann::json rule_logic;
    nlohmann::json parameters;
    std::vector<std::string> input_fields;
    std::vector<std::string> output_fields;
    bool is_active = true;
    std::optional<std::chrono::system_clock::time_point> valid_from;
    std::optional<std::chrono::system_clock::time_point> valid_until;
    std::string created_by;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
};

struct RulePerformanceMetrics {
    std::string rule_id;
    int total_executions = 0;
    int successful_executions = 0;
    int failed_executions = 0;
    int fraud_detections = 0;
    int false_positives = 0;
    double average_execution_time_ms = 0.0;
    double average_confidence_score = 0.0;
    std::chrono::system_clock::time_point last_execution;
    std::unordered_map<std::string, int> error_counts;
};

class AdvancedRuleEngine {
public:
    AdvancedRuleEngine(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<StructuredLogger> logger,
        std::shared_ptr<DynamicConfigManager> config_manager,
        std::shared_ptr<LLMInterface> llm_interface = nullptr
    );

    ~AdvancedRuleEngine();

    // Core rule execution methods
    RuleExecutionResultDetail execute_rule(
        const RuleDefinition& rule,
        const RuleExecutionContext& context,
        RuleExecutionMode mode = RuleExecutionMode::SYNCHRONOUS
    );

    FraudDetectionResult evaluate_transaction(
        const RuleExecutionContext& context,
        const std::vector<std::string>& rule_ids = {}
    );

    // Rule management methods
    bool register_rule(const RuleDefinition& rule);
    bool update_rule(const std::string& rule_id, const nlohmann::json& updates);
    bool deactivate_rule(const std::string& rule_id);
    bool delete_rule(const std::string& rule_id);
    std::optional<RuleDefinition> get_rule(const std::string& rule_id);
    std::vector<RuleDefinition> get_active_rules();
    std::vector<RuleDefinition> get_rules_by_type(const std::string& rule_type);

    // Batch processing methods
    std::string submit_batch_evaluation(
        const std::vector<RuleExecutionContext>& contexts,
        const std::vector<std::string>& rule_ids = {}
    );
    std::unordered_map<std::string, FraudDetectionResult> get_batch_results(const std::string& batch_id);
    double get_batch_progress(const std::string& batch_id);

    // Rule validation and testing
    nlohmann::json validate_rule_logic(const nlohmann::json& rule_logic);
    nlohmann::json test_rule_execution(
        const std::string& rule_id,
        const RuleExecutionContext& test_context
    );

    // Performance monitoring
    RulePerformanceMetrics get_rule_metrics(const std::string& rule_id);
    std::vector<RulePerformanceMetrics> get_all_rule_metrics();
    void reset_rule_metrics(const std::string& rule_id = "");

    // Configuration and optimization
    void reload_rules(); // Force reload from database
    void optimize_rule_execution(); // Optimize rule execution order
    void set_execution_timeout(std::chrono::milliseconds timeout);
    void set_max_parallel_executions(int max_parallel);

    // Risk scoring and aggregation
    double calculate_aggregated_risk_score(const std::vector<RuleExecutionResultDetail>& results);
    FraudRiskLevel determine_overall_risk_level(const std::vector<RuleExecutionResultDetail>& results);
    std::string generate_fraud_recommendation(const FraudDetectionResult& result);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<DynamicConfigManager> config_manager_;
    std::shared_ptr<LLMInterface> llm_interface_;

    // In-memory rule cache
    std::unordered_map<std::string, RuleDefinition> rule_cache_;
    std::unordered_map<std::string, RulePerformanceMetrics> metrics_cache_;
    std::mutex cache_mutex_;

    // Execution configuration
    std::chrono::milliseconds execution_timeout_ = std::chrono::milliseconds(5000);
    int max_parallel_executions_ = 10;
    bool enable_performance_monitoring_ = true;

    // Rule execution methods
    RuleExecutionResultDetail execute_validation_rule(
        const RuleDefinition& rule,
        const RuleExecutionContext& context
    );

    RuleExecutionResultDetail execute_scoring_rule(
        const RuleDefinition& rule,
        const RuleExecutionContext& context
    );

    RuleExecutionResultDetail execute_pattern_rule(
        const RuleDefinition& rule,
        const RuleExecutionContext& context
    );

    RuleExecutionResultDetail execute_ml_rule(
        const RuleDefinition& rule,
        const RuleExecutionContext& context
    );

    // Utility methods
    std::string generate_rule_id();
    std::string generate_transaction_id();
    bool validate_rule_definition(const RuleDefinition& rule);
    nlohmann::json extract_field_value(const nlohmann::json& data, const std::string& field_path);
    bool evaluate_condition(const nlohmann::json& condition, const nlohmann::json& data);
    double calculate_rule_confidence(const RuleDefinition& rule, const RuleExecutionResultDetail& result);

    // Risk calculation helpers
    double normalize_risk_score(double raw_score);
    FraudRiskLevel score_to_risk_level(double score);
    std::string risk_level_to_recommendation(FraudRiskLevel level);

    // Database operations
    bool store_rule(const RuleDefinition& rule);
    bool update_rule_in_db(const std::string& rule_id, const nlohmann::json& updates);
    std::optional<RuleDefinition> load_rule(const std::string& rule_id);
    std::vector<RuleDefinition> load_active_rules();
    bool store_execution_result(const RuleExecutionResultDetail& result, const std::string& transaction_id);
    bool store_fraud_detection_result(const FraudDetectionResult& result);

    // Performance tracking
    void update_rule_metrics(const std::string& rule_id, const RuleExecutionResultDetail& result);
    void record_execution_time(const std::string& rule_id, std::chrono::milliseconds duration);

    // Cache management
    void cache_rule(const RuleDefinition& rule);
    void invalidate_rule_cache(const std::string& rule_id = "");
    void cleanup_expired_cache_entries();
    void load_configuration();
};

} // namespace regulens

#endif // ADVANCED_RULE_ENGINE_HPP
