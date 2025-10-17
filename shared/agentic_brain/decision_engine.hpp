/**
 * Decision Engine - Intelligent Decision Making
 *
 * Core decision-making engine that evaluates risks, makes proactive decisions,
 * and provides explainable reasoning for all agent actions.
 */

#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <queue>
#include <list>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"
#include "llm_interface.hpp"
#include "learning_engine.hpp"
#include <nlohmann/json.hpp>

namespace regulens {

enum class DecisionType {
    TRANSACTION_APPROVAL,
    RISK_FLAG,
    REGULATORY_IMPACT_ASSESSMENT,
    AUDIT_ANOMALY_DETECTION,
    COMPLIANCE_ALERT,
    PROACTIVE_MONITORING
};

enum class RiskLevel {
    LOW,
    MEDIUM,
    HIGH,
    CRITICAL,
    UNKNOWN
};

enum class DecisionConfidence {
    LOW,
    MEDIUM,
    HIGH,
    VERY_HIGH
};

struct DecisionModel {
    std::string model_id;
    std::string name;
    DecisionType type;
    nlohmann::json parameters;
    nlohmann::json metadata;
    double accuracy_score;
    int usage_count;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_updated;
    bool is_active;
};

struct RiskAssessment {
    RiskLevel level;
    double score; // 0.0 to 1.0
    std::vector<std::string> risk_factors;
    std::vector<std::string> mitigating_factors;
    nlohmann::json assessment_details;
    std::chrono::system_clock::time_point assessed_at;
};

struct DecisionContext {
    std::string context_id;
    DecisionType decision_type;
    nlohmann::json input_data;
    nlohmann::json environmental_context;
    std::vector<RiskAssessment> risk_assessments;
    std::chrono::system_clock::time_point context_timestamp;
};

struct DecisionResult {
    std::string decision_id;
    DecisionType decision_type;
    std::string decision_outcome;
    DecisionConfidence confidence;
    std::string reasoning;
    std::vector<std::string> recommended_actions;
    nlohmann::json decision_metadata;
    bool requires_human_review;
    std::string human_review_reason;
    std::chrono::system_clock::time_point decision_timestamp;
    long long processing_time_ms;
};

struct ProactiveAction {
    std::string action_id;
    std::string action_type;
    std::string description;
    RiskLevel priority;
    nlohmann::json action_parameters;
    std::chrono::system_clock::time_point suggested_at;
    std::chrono::system_clock::time_point deadline;
};

class DecisionEngine {
public:
    DecisionEngine(
        std::shared_ptr<ConnectionPool> db_pool,
        std::shared_ptr<LLMInterface> llm_interface,
        std::shared_ptr<LearningEngine> learning_engine,
        StructuredLogger* logger
    );

    ~DecisionEngine();

    // Core decision operations
    bool initialize();
    void shutdown();

    // Decision making
    DecisionResult make_decision(const DecisionContext& context);
    std::vector<DecisionResult> batch_decide(const std::vector<DecisionContext>& contexts);

    // Risk assessment
    RiskAssessment assess_risk(const nlohmann::json& data, DecisionType decision_type);
    std::vector<RiskAssessment> assess_multiple_risks(const std::vector<nlohmann::json>& data_items, DecisionType decision_type);

    // Proactive capabilities
    std::vector<ProactiveAction> identify_proactive_actions();
    std::vector<nlohmann::json> detect_anomalous_patterns();
    std::vector<nlohmann::json> predict_future_risks();

    // Decision explanation
    nlohmann::json explain_decision(const std::string& decision_id);
    std::string generate_decision_summary(const DecisionResult& decision);

    // Learning integration
    bool incorporate_decision_feedback(const std::string& decision_id, const nlohmann::json& feedback);
    nlohmann::json analyze_decision_patterns(const std::string& agent_type, const std::chrono::system_clock::duration& time_window);

    // Human-AI collaboration
    std::vector<DecisionResult> get_pending_human_reviews();
    bool process_human_decision_override(const std::string& decision_id, const std::string& human_decision, const std::string& reasoning);

    // Performance monitoring
    nlohmann::json get_decision_metrics(const std::string& agent_type);
    std::vector<std::string> identify_decision_improvement_areas(const std::string& agent_type);

private:
    // Transaction decision logic
    DecisionResult evaluate_transaction(const DecisionContext& context);
    RiskAssessment assess_transaction_risk(const nlohmann::json& transaction_data);

    // Regulatory decision logic
    DecisionResult evaluate_regulatory_change(const DecisionContext& context);
    RiskAssessment assess_regulatory_risk(const nlohmann::json& regulatory_data);

    // Audit decision logic
    DecisionResult evaluate_audit_event(const DecisionContext& context);
    RiskAssessment assess_audit_risk(const nlohmann::json& audit_data);

    // Risk calculation algorithms
    double calculate_transaction_risk_score(const nlohmann::json& transaction);
    double calculate_regulatory_risk_score(const nlohmann::json& regulatory_data);
    double calculate_audit_risk_score(const nlohmann::json& audit_data);

    // Pattern-based decision making
    nlohmann::json apply_learned_patterns(const DecisionContext& context);
    bool matches_known_risk_pattern(const nlohmann::json& data, const std::vector<LearningPattern>& patterns);

    // Confidence calculation
    DecisionConfidence calculate_decision_confidence(const DecisionContext& context, const DecisionResult& result);
    double calculate_context_confidence(const DecisionContext& context);

    // Proactive analysis
    std::vector<nlohmann::json> analyze_trends_for_proactive_actions();
    std::vector<nlohmann::json> identify_emerging_risks();
    std::vector<nlohmann::json> suggest_preventive_measures();

    // Decision validation
    bool validate_decision_logic(const DecisionResult& result, const DecisionContext& context);
    bool check_decision_consistency(const DecisionResult& result, const std::vector<DecisionResult>& historical_decisions);

    // Explanation generation
    std::string generate_transaction_explanation(const DecisionResult& decision, const DecisionContext& context);
    std::string generate_regulatory_explanation(const DecisionResult& decision, const DecisionContext& context);
    std::string generate_audit_explanation(const DecisionResult& decision, const DecisionContext& context);

    // Threshold management
    nlohmann::json get_dynamic_thresholds(DecisionType decision_type);
    bool update_thresholds_based_on_feedback(DecisionType decision_type, const nlohmann::json& feedback);

    // Decision storage and retrieval
    bool store_decision(const DecisionResult& decision, const DecisionContext& context);
    DecisionResult retrieve_decision(const std::string& decision_id);
    std::vector<DecisionResult> get_recent_decisions(const std::string& agent_type, int limit = 100);

    // Performance tracking
    void update_decision_metrics(const DecisionResult& decision, const DecisionContext& context);
    nlohmann::json calculate_decision_accuracy(const std::string& agent_type, const std::chrono::system_clock::duration& time_window);

    // Internal state
    std::shared_ptr<ConnectionPool> db_pool_;
    std::shared_ptr<LLMInterface> llm_interface_;
    std::shared_ptr<LearningEngine> learning_engine_;
    StructuredLogger* logger_;

    std::unordered_map<DecisionType, nlohmann::json> decision_thresholds_;
    std::unordered_map<std::string, DecisionResult> decision_cache_;
    std::list<std::string> cache_access_order_;  // LRU tracking: front = oldest, back = newest
    std::queue<DecisionContext> pending_decisions_;

    // Performance metrics
    std::unordered_map<std::string, int> decision_counts_;
    std::unordered_map<std::string, double> accuracy_scores_;
    std::unordered_map<std::string, std::chrono::milliseconds> avg_decision_times_;

    bool initialized_;
    std::atomic<bool> processing_active_;

    // Active decision models
    std::unordered_map<std::string, DecisionModel> active_models_;

    // Random number generation
    std::mt19937 random_engine_;

    // Initialization and state management
    void initialize_default_thresholds();
    void initialize_database_schema(PostgreSQLConnection& conn);
    void load_persisted_state();
    void save_current_state();

    // Utility functions
    std::string generate_decision_id();
    std::string risk_level_to_string(RiskLevel level);
    DecisionType string_to_decision_type(const std::string& str);
    std::string decision_type_to_string(DecisionType type);
    DecisionConfidence string_to_confidence(const std::string& str);
    bool matches_learned_pattern(const nlohmann::json& data, const LearningPattern& pattern);
    bool matches_learned_pattern(const nlohmann::json& data, const std::vector<LearningPattern>& patterns);
    std::string timestamp_to_string(std::chrono::system_clock::time_point tp);
    std::chrono::system_clock::time_point string_to_timestamp(const std::string& str);
    nlohmann::json generate_detailed_explanation(const DecisionResult& decision);

    // Specialized decision evaluators
    DecisionResult evaluate_compliance_alert(const DecisionContext& context);
    DecisionResult evaluate_proactive_monitoring(const DecisionContext& context);

    // Human review logic
    bool should_require_human_review(const DecisionResult& decision, const RiskAssessment& risk);

    // Action generation
    std::vector<std::string> generate_recommended_actions(const DecisionResult& result, const DecisionContext& context);
    std::vector<std::string> generate_risk_factors(const nlohmann::json& data, double risk_score);
    std::vector<std::string> generate_mitigating_factors(const nlohmann::json& data, double risk_score);
    double calculate_assessment_confidence(const nlohmann::json& data);
    nlohmann::json generate_risk_recommendations(const RiskAssessment& assessment);
    std::string confidence_to_string(DecisionConfidence confidence);
    RiskLevel score_to_risk_level(double score);
    std::string get_impact_level(double impact_score);
    double calculate_compliance_risk_score(const nlohmann::json& compliance_data);
    std::string get_severity_level(double score);
};

} // namespace regulens
