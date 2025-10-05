#pragma once

#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <optional>
#include <nlohmann/json.hpp>

#include "../../shared/config/configuration_manager.hpp"
#include "../../shared/logging/structured_logger.hpp"
#include "../../shared/database/postgresql_connection.hpp"
#include "../../shared/models/agent_decision.hpp"
#include "../../shared/models/compliance_event.hpp"
#include "../../shared/llm/anthropic_client.hpp"
#include "../../shared/knowledge_base.hpp"

namespace regulens {

/**
 * @brief Regulatory Assessor Agent - Advanced regulatory change impact analysis
 *
 * This agent analyzes regulatory changes, assesses their impact on business operations,
 * and provides intelligent recommendations for compliance adaptation.
 */
class RegulatoryAssessorAgent {
public:
    RegulatoryAssessorAgent(std::shared_ptr<ConfigurationManager> config,
                           std::shared_ptr<StructuredLogger> logger,
                           std::shared_ptr<PostgreSQLConnectionPool> db_pool,
                           std::shared_ptr<AnthropicClient> llm_client,
                           std::shared_ptr<KnowledgeBase> knowledge_base);

    ~RegulatoryAssessorAgent();

    /**
     * @brief Initialize the regulatory assessor agent
     */
    bool initialize();

    /**
     * @brief Start the regulatory assessment processing
     */
    void start();

    /**
     * @brief Stop the regulatory assessment processing
     */
    void stop();

    /**
     * @brief Assess impact of regulatory change on business operations
     * @param regulatory_change Regulatory change data
     * @return Comprehensive impact assessment
     */
    nlohmann::json assess_regulatory_impact(const nlohmann::json& regulatory_change);

    /**
     * @brief Generate compliance adaptation recommendations
     * @param impact_assessment Impact assessment results
     * @return Vector of actionable recommendations
     */
    std::vector<nlohmann::json> generate_adaptation_recommendations(const nlohmann::json& impact_assessment);

    /**
     * @brief Analyze regulatory change using AI-powered analysis
     * @param regulatory_data Regulatory change data
     * @return Agent decision with regulatory assessment
     */
    AgentDecision analyze_regulatory_change(const nlohmann::json& regulatory_data);

    /**
     * @brief Predict future regulatory trends based on current changes
     * @param recent_changes Recent regulatory changes
     * @return Trend prediction analysis
     */
    nlohmann::json predict_regulatory_trends(const std::vector<nlohmann::json>& recent_changes);

    /**
     * @brief Assess compliance gap between current state and new requirements
     * @param regulatory_change New regulatory requirements
     * @param current_compliance Current compliance status
     * @return Gap analysis report
     */
     nlohmann::json assess_compliance_gap(const nlohmann::json& regulatory_change,
                                        const nlohmann::json& current_compliance);

     /**
      * @brief Get total number of assessments processed
      * @return Total assessment count
      */
     size_t get_total_assessments_processed() const { return total_assessments_processed_.load(); }

private:
    /**
     * @brief Main regulatory assessment processing loop
     */
    void assessment_processing_loop();

    /**
     * @brief Analyze affected business processes
     * @param regulatory_change Regulatory change data
     * @return List of affected processes
     */
    std::vector<std::string> analyze_affected_processes(const nlohmann::json& regulatory_change);

    /**
     * @brief Calculate implementation complexity
     * @param regulatory_change Regulatory change data
     * @return Complexity score (0.0 to 1.0)
     */
    double calculate_implementation_complexity(const nlohmann::json& regulatory_change);

    /**
     * @brief Estimate compliance timeline
     * @param regulatory_change Regulatory change data
     * @return Timeline estimation in days
     */
    int estimate_compliance_timeline(const nlohmann::json& regulatory_change);

    /**
     * @brief Generate risk mitigation strategies
     * @param impact_assessment Impact assessment data
     * @return Risk mitigation recommendations
     */
    std::vector<nlohmann::json> generate_risk_mitigation_strategies(const nlohmann::json& impact_assessment);

    /**
     * @brief Use AI to analyze regulatory implications
     * @param regulatory_data Regulatory change data
     * @return AI-powered analysis results
     */
    nlohmann::json perform_ai_regulatory_analysis(const nlohmann::json& regulatory_data);

    /**
     * @brief Extract confidence score from LLM response for regulatory analysis
     * @param llm_response LLM response text
     * @return Confidence score between 0.0 and 1.0
     */
    double extract_confidence_from_llm_response(const std::string& llm_response);

    /**
     * @brief Parse ISO 8601 timestamp string into system_clock time_point
     * @param timestamp_str ISO 8601 timestamp string
     * @return Optional time_point, empty if parsing fails
     */
    std::optional<std::chrono::system_clock::time_point> parse_iso8601_timestamp(const std::string& timestamp_str);

    std::shared_ptr<ConfigurationManager> config_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<PostgreSQLConnectionPool> db_pool_;
    std::shared_ptr<AnthropicClient> llm_client_;
    std::shared_ptr<KnowledgeBase> knowledge_base_;

    std::thread assessment_thread_;
    std::atomic<bool> running_;
    std::atomic<size_t> total_assessments_processed_;

    // Assessment parameters
    double high_impact_threshold_;
    std::chrono::hours assessment_interval_;

    mutable std::mutex assessment_mutex_;
};

} // namespace regulens
