#pragma once

#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <nlohmann/json.hpp>

#include "../../shared/config/configuration_manager.hpp"
#include "../../shared/logging/structured_logger.hpp"
#include "../../shared/database/postgresql_connection.hpp"
#include "../../shared/models/agent_decision.hpp"
#include "../../shared/models/compliance_event.hpp"
#include "../../shared/llm/anthropic_client.hpp"
#include "../../shared/risk_assessment.hpp"

namespace regulens {

/**
 * @brief Transaction Guardian Agent - Real-time transaction monitoring and compliance
 *
 * This agent monitors financial transactions in real-time, assesses compliance risks,
 * detects suspicious activities, and ensures regulatory compliance for all transactions.
 */
class TransactionGuardianAgent {
public:
    TransactionGuardianAgent(std::shared_ptr<ConfigurationManager> config,
                            std::shared_ptr<StructuredLogger> logger,
                            std::shared_ptr<PostgreSQLConnectionPool> db_pool,
                            std::shared_ptr<AnthropicClient> llm_client,
                            std::shared_ptr<RiskAssessmentEngine> risk_engine);

    ~TransactionGuardianAgent();

    /**
     * @brief Initialize the transaction guardian agent
     */
    bool initialize();

    /**
     * @brief Start real-time transaction monitoring
     */
    void start();

    /**
     * @brief Stop transaction monitoring
     */
    void stop();

    /**
     * @brief Process a transaction for compliance and risk assessment
     * @param transaction_data Transaction details in JSON format
     * @return Compliance decision for the transaction
     */
    AgentDecision process_transaction(const nlohmann::json& transaction_data);

    /**
     * @brief Perform real-time fraud detection on transaction
     * @param transaction_data Transaction to analyze
     * @return Fraud risk assessment
     */
    nlohmann::json detect_fraud(const nlohmann::json& transaction_data);

    /**
     * @brief Check transaction against regulatory compliance rules
     * @param transaction_data Transaction to check
     * @return Compliance check results
     */
    nlohmann::json check_compliance(const nlohmann::json& transaction_data);

    /**
     * @brief Monitor transaction velocity and patterns
     * @param customer_id Customer identifier
     * @param transaction_amount Transaction amount
     * @return Velocity risk assessment
     */
    nlohmann::json monitor_velocity(const std::string& customer_id, double transaction_amount);

    /**
     * @brief Generate transaction compliance report
     * @param start_time Report start time
     * @param end_time Report end time
     * @return Comprehensive transaction compliance report
     */
    nlohmann::json generate_compliance_report(
        const std::chrono::system_clock::time_point& start_time,
        const std::chrono::system_clock::time_point& end_time);

    /**
     * @brief Handle suspicious transaction escalation
     * @param transaction_data Suspicious transaction data
     * @param risk_score Risk assessment score
     */
    void escalate_suspicious_transaction(const nlohmann::json& transaction_data, double risk_score);

private:
    /**
     * @brief Main transaction processing loop
     */
    void transaction_processing_loop();

    /**
     * @brief Process queued transactions
     */
    void process_transaction_queue();

    /**
     * @brief Analyze transaction patterns using AI
     * @param transaction_history Recent transactions
     * @return Pattern analysis results
     */
    nlohmann::json analyze_transaction_patterns(const std::vector<nlohmann::json>& transaction_history);

    /**
     * @brief Check for AML/KYC compliance
     * @param transaction_data Transaction to check
     * @param customer_profile Customer profile data
     * @return AML compliance assessment
     */
    nlohmann::json check_aml_compliance(const nlohmann::json& transaction_data,
                                      const nlohmann::json& customer_profile);

    /**
     * @brief Validate transaction against business rules
     * @param transaction_data Transaction to validate
     * @return Validation results
     */
    bool validate_business_rules(const nlohmann::json& transaction_data);

    /**
     * @brief Calculate transaction risk score using multiple factors
     * @param transaction_data Transaction data
     * @param customer_history Customer transaction history
     * @return Comprehensive risk score
     */
    double calculate_transaction_risk_score(const nlohmann::json& transaction_data,
                                          const std::vector<nlohmann::json>& customer_history);

    /**
     * @brief Update customer risk profile based on transaction
     * @param customer_id Customer identifier
     * @param transaction_risk Risk from this transaction
     */
    void update_customer_risk_profile(const std::string& customer_id, double transaction_risk);

    /**
     * @brief Fetch customer profile from database
     * @param customer_id Customer identifier
     * @return Customer profile data or empty JSON on error
     */
    nlohmann::json fetch_customer_profile(const std::string& customer_id);

    /**
     * @brief Fetch customer transaction history for velocity analysis
     * @param customer_id Customer identifier
     * @param analysis_window Time window for analysis
     * @return Vector of recent transactions
     */
    std::vector<nlohmann::json> fetch_customer_transaction_history(const std::string& customer_id,
                                                                 const std::chrono::minutes& analysis_window);

    /**
     * @brief Fetch risk distribution data for compliance reporting
     * @param start_time Report start time
     * @param end_time Report end time
     * @return Risk distribution statistics
     */
    nlohmann::json fetch_risk_distribution(const std::chrono::system_clock::time_point& start_time,
                                         const std::chrono::system_clock::time_point& end_time);

    /**
     * @brief Fetch top compliance violations for reporting
     * @param start_time Report start time
     * @param end_time Report end time
     * @return Top violations statistics
     */
    nlohmann::json fetch_top_violations(const std::chrono::system_clock::time_point& start_time,
                                      const std::chrono::system_clock::time_point& end_time);

    std::shared_ptr<ConfigurationManager> config_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<PostgreSQLConnectionPool> db_pool_;
    std::shared_ptr<AnthropicClient> llm_client_;
    std::shared_ptr<RiskAssessmentEngine> risk_engine_;

    std::thread processing_thread_;
    std::atomic<bool> running_;
    std::atomic<size_t> transactions_processed_;
    std::atomic<size_t> suspicious_transactions_detected_;

    // Transaction processing queue
    std::queue<nlohmann::json> transaction_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    // Risk thresholds and parameters
    double fraud_threshold_;
    double velocity_threshold_;
    double high_risk_threshold_;
    std::chrono::minutes analysis_window_;
    std::vector<std::string> sanctioned_countries_;

    // Risk calculation parameters
    double risk_amount_100k_;
    double risk_amount_50k_;
    double risk_amount_10k_;
    double risk_international_;
    double risk_crypto_;
    double velocity_critical_threshold_;
    double velocity_high_threshold_;
    double velocity_moderate_threshold_;
    double velocity_ratio_5x_;
    double velocity_ratio_3x_;
    double velocity_ratio_2x_;
    double ai_confidence_weight_;
    double customer_risk_update_weight_;
    double ai_failure_fallback_increase_;

    // Transaction risk calculation parameters
    double unusual_amount_multiplier_;
    double unusual_amount_risk_weight_;
    double off_hours_risk_weight_;
    double weekend_risk_weight_;
    double risk_update_current_weight_;
    double risk_update_transaction_weight_;
    double base_time_risk_weight_;

    // Customer risk profiles cache
    std::unordered_map<std::string, nlohmann::json> customer_risk_profiles_;
    mutable std::mutex profiles_mutex_;

    // Error handling and resilience
    std::atomic<size_t> consecutive_db_failures_;
    std::atomic<size_t> consecutive_llm_failures_;
    std::chrono::steady_clock::time_point last_db_failure_;
    std::chrono::steady_clock::time_point last_llm_failure_;
    static constexpr size_t MAX_CONSECUTIVE_FAILURES = 5;
    static constexpr std::chrono::minutes CIRCUIT_BREAKER_TIMEOUT = std::chrono::minutes(5);

    // Fallback mechanisms
    nlohmann::json get_fallback_customer_profile(const std::string& customer_id) const;
    nlohmann::json get_fallback_transaction_history() const;
    double get_fallback_risk_score(const nlohmann::json& transaction_data) const;
    bool is_circuit_breaker_open(std::chrono::steady_clock::time_point last_failure,
                                size_t consecutive_failures) const;
    void record_operation_failure(std::atomic<size_t>& failure_counter,
                                 std::chrono::steady_clock::time_point& last_failure);
    void record_operation_success(std::atomic<size_t>& failure_counter);
};

} // namespace regulens
