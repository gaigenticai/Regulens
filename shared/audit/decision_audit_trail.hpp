/**
 * Decision Audit & Explanation System
 *
 * Complete transparency system that tracks every step of agent decision-making,
 * provides explainable AI capabilities, and enables human-AI collaboration.
 *
 * This is the most critical component for human trust - humans need to SEE
 * the reasoning process, not just the final decision.
 */

#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <chrono>
#include <atomic>
#include <fstream>
#include <nlohmann/json.hpp>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"
#include <nlohmann/json.hpp>

namespace regulens {

enum class AuditEventType {
    DECISION_STARTED,
    DATA_RETRIEVAL,
    PATTERN_ANALYSIS,
    RISK_ASSESSMENT,
    KNOWLEDGE_QUERY,
    LLM_INFERENCE,
    RULE_EVALUATION,
    CONFIDENCE_CALCULATION,
    DECISION_FINALIZED,
    HUMAN_REVIEW_REQUESTED,
    HUMAN_FEEDBACK_RECEIVED
};

enum class DecisionConfidence {
    VERY_LOW,      // < 0.3
    LOW,           // 0.3 - 0.5
    MEDIUM,        // 0.5 - 0.7
    HIGH,          // 0.7 - 0.9
    VERY_HIGH      // > 0.9
};

enum class ExplanationLevel {
    HIGH_LEVEL,    // Executive summary
    DETAILED,      // Step-by-step reasoning
    TECHNICAL,     // Full technical details
    DEBUG          // Internal debugging info
};

struct DecisionStep {
    std::string step_id;
    AuditEventType event_type;
    std::string description;
    nlohmann::json input_data;
    nlohmann::json output_data;
    nlohmann::json metadata;
    std::chrono::microseconds processing_time;
    double confidence_impact;  // How this step affected overall confidence
    std::chrono::system_clock::time_point timestamp;
    std::string agent_id;
    std::string decision_id;
};

struct DecisionAuditTrail {
    std::string trail_id;
    std::string decision_id;
    std::string agent_type;
    std::string agent_name;
    std::string trigger_event;
    nlohmann::json original_input;
    nlohmann::json final_decision;
    DecisionConfidence final_confidence;
    std::vector<DecisionStep> steps;
    nlohmann::json decision_tree;  // Hierarchical decision structure
    nlohmann::json risk_assessment;
    nlohmann::json alternative_options;  // What other decisions were considered
    std::chrono::system_clock::time_point started_at;
    std::chrono::system_clock::time_point completed_at;
    std::chrono::milliseconds total_processing_time;
    bool requires_human_review;
    std::string human_review_reason;
};

struct DecisionExplanation {
    std::string explanation_id;
    std::string decision_id;
    ExplanationLevel level;
    std::string natural_language_summary;
    std::vector<std::string> key_factors;
    std::vector<std::string> risk_indicators;
    std::vector<std::string> confidence_factors;
    nlohmann::json decision_flowchart;  // Visual representation
    nlohmann::json technical_details;
    std::string human_readable_reasoning;
    std::chrono::system_clock::time_point generated_at;
};

class DecisionAuditTrailManager {
public:
    DecisionAuditTrailManager(
        std::shared_ptr<ConnectionPool> db_pool,
        StructuredLogger* logger
    );

    ~DecisionAuditTrailManager();

    // Core audit functionality
    bool initialize();
    void shutdown();

    // Decision tracking
    std::string start_decision_audit(
        const std::string& agent_type,
        const std::string& agent_name,
        const std::string& trigger_event,
        const nlohmann::json& input_data
    );

    bool record_decision_step(
        const std::string& decision_id,
        AuditEventType event_type,
        const std::string& description,
        const nlohmann::json& input_data = nlohmann::json::object(),
        const nlohmann::json& output_data = nlohmann::json::object(),
        const nlohmann::json& metadata = nlohmann::json::object()
    );

    bool finalize_decision_audit(
        const std::string& decision_id,
        const nlohmann::json& final_decision,
        DecisionConfidence confidence,
        const nlohmann::json& decision_tree = nlohmann::json::object(),
        const nlohmann::json& risk_assessment = nlohmann::json::object(),
        const nlohmann::json& alternative_options = nlohmann::json::object()
    );

    // Explanation generation
    std::optional<DecisionExplanation> generate_explanation(
        const std::string& decision_id,
        ExplanationLevel level = ExplanationLevel::DETAILED
    );

    // Audit retrieval
    std::optional<DecisionAuditTrail> get_decision_audit(
        const std::string& decision_id
    );

    std::vector<DecisionAuditTrail> get_agent_decisions(
        const std::string& agent_type,
        const std::string& agent_name,
        std::chrono::system_clock::time_point since = std::chrono::system_clock::now() - std::chrono::hours(24)
    );

    std::vector<DecisionAuditTrail> get_decisions_requiring_review();

    // Human-AI collaboration
    bool request_human_review(
        const std::string& decision_id,
        const std::string& reason
    );

    bool record_human_feedback(
        const std::string& decision_id,
        const std::string& human_feedback,
        bool approved,
        const std::string& reviewer_id
    );

    // Analytics and insights
    nlohmann::json get_agent_performance_analytics(
        const std::string& agent_type,
        std::chrono::system_clock::time_point since
    );

    nlohmann::json get_decision_pattern_analysis(
        const std::string& agent_type,
        std::chrono::system_clock::time_point since
    );

    // Compliance and audit
    std::vector<nlohmann::json> get_audit_trail_for_compliance(
        std::chrono::system_clock::time_point start_date,
        std::chrono::system_clock::time_point end_date
    );

    bool export_audit_data(
        const std::string& file_path,
        std::chrono::system_clock::time_point start_date,
        std::chrono::system_clock::time_point end_date
    );

private:
    // Private helper methods
    bool should_request_human_review(const DecisionAuditTrail& trail);
    std::string generate_human_review_reason(const DecisionAuditTrail& trail);
    std::string generate_detailed_reasoning(const DecisionAuditTrail& trail);
    std::string confidence_to_string(DecisionConfidence confidence);
    std::string event_type_to_string(AuditEventType type);
    // Database operations
    bool store_decision_step(const DecisionStep& step);
    bool update_decision_trail(const DecisionAuditTrail& trail);
    bool store_decision_explanation(const DecisionExplanation& explanation);

    std::optional<DecisionStep> load_decision_step(const std::string& step_id);
    std::optional<DecisionAuditTrail> load_decision_trail(const std::string& decision_id);
    std::optional<DecisionExplanation> load_decision_explanation(const std::string& explanation_id);

    // Explanation generation
    std::string generate_natural_language_summary(const DecisionAuditTrail& trail, ExplanationLevel level);
    std::vector<std::string> extract_key_factors(const DecisionAuditTrail& trail);
    std::vector<std::string> identify_risk_indicators(const DecisionAuditTrail& trail);
    std::vector<std::string> analyze_confidence_factors(const DecisionAuditTrail& trail);
    nlohmann::json build_decision_flowchart(const DecisionAuditTrail& trail);

    // Analytics helpers
    nlohmann::json analyze_decision_patterns(const std::vector<DecisionAuditTrail>& trails);
    nlohmann::json calculate_performance_metrics(const std::vector<DecisionAuditTrail>& trails);

    // Utility functions
    std::string generate_unique_id();
    DecisionConfidence calculate_overall_confidence(const std::vector<DecisionStep>& steps);
    std::chrono::milliseconds calculate_total_processing_time(const std::vector<DecisionStep>& steps);

    // Member variables
    std::shared_ptr<ConnectionPool> db_pool_;
    StructuredLogger* logger_;

    std::unordered_map<std::string, DecisionAuditTrail> active_trails_;
    std::mutex trails_mutex_;

    // Cache steps until trail is finalized
    std::unordered_map<std::string, std::vector<DecisionStep>> pending_steps_;
    std::mutex steps_mutex_;

    std::atomic<size_t> total_decisions_audited_{0};
    std::atomic<size_t> decisions_requiring_review_{0};
    std::atomic<size_t> human_reviews_completed_{0};
};

} // namespace regulens
