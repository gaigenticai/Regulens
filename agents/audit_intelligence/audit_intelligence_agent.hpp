#pragma once

#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <nlohmann/json.hpp>

#include "../../shared/config/configuration_manager.hpp"
#include "../../shared/logging/structured_logger.hpp"
#include "../../shared/database/postgresql_connection.hpp"
#include "../../shared/models/agent_decision.hpp"
#include "../../shared/models/compliance_event.hpp"
#include "../../shared/llm/anthropic_client.hpp"
#include "../../shared/audit/decision_audit_trail.hpp"

namespace regulens {

/**
 * @brief Audit Intelligence Agent - Advanced compliance auditing and anomaly detection
 *
 * This agent performs sophisticated audit trail analysis, anomaly detection,
 * and intelligent compliance monitoring using machine learning and pattern recognition.
 */
class AuditIntelligenceAgent {
public:
    AuditIntelligenceAgent(std::shared_ptr<ConfigurationManager> config,
                          std::shared_ptr<StructuredLogger> logger,
                          std::shared_ptr<ConnectionPool> db_pool,
                          std::shared_ptr<AnthropicClient> llm_client,
                          std::shared_ptr<DecisionAuditTrailManager> audit_trail);

    ~AuditIntelligenceAgent();

    /**
     * @brief Initialize the audit intelligence agent
     */
    bool initialize();

    /**
     * @brief Load agent-specific configuration from database
     * @param agent_id UUID of the agent configuration in database
     * @return true if configuration loaded successfully
     */
    bool load_configuration_from_database(const std::string& agent_id);

    /**
     * @brief Start the audit intelligence processing
     */
    void start();

    /**
     * @brief Stop the audit intelligence processing
     */
    void stop();

    /**
     * @brief Analyze audit trails for anomalies and compliance issues
     * @param time_window Time window to analyze (in hours)
     * @return Vector of detected anomalies
     */
    std::vector<ComplianceEvent> analyze_audit_trails(int time_window_hours = 24);

    /**
     * @brief Perform real-time compliance monitoring
     * @param event Compliance event to analyze
     * @return Agent decision with audit intelligence
     */
    AgentDecision perform_compliance_monitoring(const ComplianceEvent& event);

    /**
     * @brief Generate audit intelligence report
     * @param start_time Start time for the report
     * @param end_time End time for the report
     * @return JSON report with audit insights
     */
    nlohmann::json generate_audit_report(
        const std::chrono::system_clock::time_point& start_time,
        const std::chrono::system_clock::time_point& end_time);

    /**
     * @brief Detect potential fraud patterns using AI analysis
     * @param transaction_data Transaction data to analyze
     * @return Fraud risk assessment
     */
    nlohmann::json detect_fraud_patterns(const nlohmann::json& transaction_data);

    /**
     * @brief Analyze decision patterns for bias and compliance
     * @param decisions Vector of agent decisions to analyze
     * @return Pattern analysis results
     */
    nlohmann::json analyze_decision_patterns(const std::vector<AgentDecision>& decisions);

private:
    /**
     * @brief Main audit processing loop
     */
    void audit_processing_loop();

    /**
     * @brief Analyze decision audit trail for anomalies
     */
    std::vector<ComplianceEvent> analyze_decision_anomalies();

    /**
     * @brief Perform pattern recognition on audit data
     */
    std::vector<nlohmann::json> perform_pattern_recognition();

    /**
     * @brief Calculate risk scores using machine learning
     */
    double calculate_risk_score(const nlohmann::json& audit_data);

    /**
     * @brief Generate compliance insights using LLM
     */
    std::string generate_compliance_insights(const std::vector<nlohmann::json>& audit_data);

    // Advanced ML-based analysis methods
    /**
     * @brief Calculate advanced risk score using multi-factor analysis
     */
    double calculate_advanced_risk_score(const nlohmann::json& analysis_data);

    /**
     * @brief Calculate basic risk score as fallback
     */
    double calculate_basic_risk_score(const nlohmann::json& audit_data);

    /**
     * @brief Analyze historical patterns for risk assessment
     */
    double analyze_historical_patterns(const nlohmann::json& analysis_data);

    /**
     * @brief Assess contextual risk using LLM
     */
    double assess_contextual_risk_with_llm(const nlohmann::json& analysis_data);

    // Anomaly detection methods
    /**
     * @brief Detect temporal anomalies in audit data
     */
    std::vector<nlohmann::json> detect_temporal_anomalies(const std::vector<nlohmann::json>& audit_data);

    /**
     * @brief Detect behavioral anomalies across agents
     */
    std::vector<nlohmann::json> detect_behavioral_anomalies(const std::vector<nlohmann::json>& audit_data);

    /**
     * @brief Detect risk correlation anomalies
     */
    std::vector<nlohmann::json> detect_risk_correlation_anomalies(const std::vector<nlohmann::json>& audit_data);

    /**
     * @brief Generate insights for detected anomalies using LLM
     */
    std::string generate_anomaly_insights(const nlohmann::json& anomaly);

    // Pattern analysis methods
    /**
     * @brief Detect bias patterns in decision making
     */
    nlohmann::json detect_bias_patterns(
        const std::unordered_map<std::string, int>& decision_counts,
        const std::unordered_map<std::string, int>& agent_counts);

    /**
     * @brief Detect performance anomalies in processing times
     */
    nlohmann::json detect_performance_anomalies(
        const std::unordered_map<std::string, std::vector<long long>>& processing_times);

    /**
     * @brief Calculate Pearson correlation coefficient
     */
    double calculate_correlation(const std::vector<std::pair<int, int>>& data_points);

    // Fraud analysis methods
    /**
     * @brief Extract risk score from LLM response
     */
    double extract_risk_score_from_llm_response(const std::string& llm_response);

    /**
     * @brief Generate fraud recommendations based on risk score
     */
    nlohmann::json generate_fraud_recommendations(double risk_score, const nlohmann::json& transaction_data);

    /**
     * @brief Adjust risk score based on transaction characteristics
     */
    double adjust_risk_for_transaction_characteristics(double base_risk, const nlohmann::json& transaction_data);

    /**
     * @brief Identify specific fraud indicators in transaction data
     */
    nlohmann::json identify_fraud_indicators(const nlohmann::json& transaction_data, const std::string& llm_response);

    /**
     * @brief Calculate baseline fraud risk without LLM analysis
     */
    double calculate_baseline_fraud_risk(const nlohmann::json& transaction_data);

    /**
     * @brief Generate basic fraud recommendations when AI analysis is unavailable
     */
    nlohmann::json generate_basic_fraud_recommendations(const nlohmann::json& transaction_data);

    // Helper methods for converting audit trail data
    std::vector<AgentDecision> convert_audit_trails_to_decisions(const std::vector<nlohmann::json>& audit_trails);
    DecisionType string_to_decision_type(const std::string& decision_str);
    ConfidenceLevel int_to_confidence_level(int confidence_int);
    double parse_structured_risk_response(const std::string& llm_response);
    std::optional<std::chrono::system_clock::time_point> parse_iso_timestamp(const std::string& timestamp_str);
    
    // Pattern analysis helpers
    nlohmann::json analyze_decision_patterns_from_audit_trails(const std::vector<nlohmann::json>& audit_trails);
    std::vector<nlohmann::json> perform_advanced_pattern_recognition(const std::vector<nlohmann::json>& audit_data);
    
    /**
     * @brief Analyze time-based risk patterns
     */
    double analyze_time_based_risk_patterns(const nlohmann::json& analysis_data);

    std::shared_ptr<ConfigurationManager> config_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<ConnectionPool> db_pool_;
    std::shared_ptr<AnthropicClient> llm_client_;
    std::shared_ptr<DecisionAuditTrailManager> audit_trail_;

    std::thread audit_thread_;
    std::atomic<bool> running_;
    std::atomic<size_t> total_audits_processed_;

    // Anomaly detection parameters
    double anomaly_threshold_;
    std::chrono::minutes analysis_interval_;

    // Database configuration (loaded from agent_configurations table)
    std::string agent_id_;
    std::string region_;
    std::string alert_email_;
    bool config_loaded_from_db_;

    // Configurable risk score parameters
    double critical_severity_risk_;
    double high_severity_risk_;
    double medium_severity_risk_;
    double low_severity_risk_;

    mutable std::mutex audit_mutex_;
};

} // namespace regulens
