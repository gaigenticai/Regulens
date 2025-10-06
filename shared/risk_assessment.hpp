#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <chrono>
#include <nlohmann/json.hpp>

#include "logging/structured_logger.hpp"
#include "config/configuration_manager.hpp"
#include "error_handler.hpp"
#include "llm/openai_client.hpp"
#include "models/compliance_event.hpp"
#include "models/risk_assessment_types.hpp"

namespace regulens {

/**
 * @brief Risk assessment result
 */

/**
 * @brief Risk assessment configuration
 */
struct RiskAssessmentConfig {
    // Scoring thresholds
    double critical_threshold = 0.8;
    double high_threshold = 0.6;
    double medium_threshold = 0.4;

    // Weight factors for different risk categories
    std::unordered_map<RiskCategory, double> category_weights = {
        {RiskCategory::FINANCIAL, 0.25},
        {RiskCategory::REGULATORY, 0.25},
        {RiskCategory::OPERATIONAL, 0.15},
        {RiskCategory::REPUTATIONAL, 0.15},
        {RiskCategory::STRATEGIC, 0.10},
        {RiskCategory::COMPLIANCE, 0.20},
        {RiskCategory::TRANSACTION, 0.30},
        {RiskCategory::ENTITY, 0.25},
        {RiskCategory::MARKET, 0.15},
        {RiskCategory::CYBER, 0.20},
        {RiskCategory::LEGAL, 0.20},
        {RiskCategory::CONCENTRATION, 0.15}
    };

    // Risk factor scoring rules
    std::unordered_map<RiskFactor, double> factor_weights;

    // High-risk indicators
    std::unordered_set<std::string> high_risk_jurisdictions = {
        "North Korea", "Iran", "Syria", "Cuba", "Venezuela"
    };

    std::unordered_set<std::string> high_risk_industries = {
        "Cryptocurrency", "Precious Metals", "Weapons", "Gambling", "Adult Entertainment"
    };

    // AI integration settings
    bool enable_ai_analysis = true;
    double ai_confidence_threshold = 0.7;
    std::string ai_model = "compliance_risk";

    RiskAssessmentConfig() {
        // Initialize factor weights with defaults
        factor_weights = {
            {RiskFactor::AMOUNT_SIZE, 0.2},
            {RiskFactor::FREQUENCY_PATTERN, 0.15},
            {RiskFactor::GEOGRAPHIC_LOCATION, 0.25},
            {RiskFactor::COUNTERPARTY_RISK, 0.2},
            {RiskFactor::PAYMENT_METHOD, 0.15},
            {RiskFactor::TIMING_PATTERN, 0.1},
            {RiskFactor::ROUND_NUMBERS, 0.1},
            {RiskFactor::CUSTOMER_HISTORY, 0.15},
            {RiskFactor::ACCOUNT_AGE, 0.1},
            {RiskFactor::VERIFICATION_STATUS, 0.2},
            {RiskFactor::BUSINESS_TYPE, 0.2},
            {RiskFactor::OWNERSHIP_STRUCTURE, 0.15},
            {RiskFactor::DEVIATION_FROM_NORM, 0.25},
            {RiskFactor::PEER_COMPARISON, 0.15},
            {RiskFactor::VELOCITY_CHANGES, 0.2},
            {RiskFactor::CHANNEL_MIX, 0.1},
            {RiskFactor::REGULATORY_CHANGES, 0.3},
            {RiskFactor::MARKET_CONDITIONS, 0.15},
            {RiskFactor::ECONOMIC_INDICATORS, 0.1},
            {RiskFactor::GEOPOLITICAL_EVENTS, 0.2}
        };
    }

    nlohmann::json to_json() const {
        nlohmann::json config = {
            {"critical_threshold", critical_threshold},
            {"high_threshold", high_threshold},
            {"medium_threshold", medium_threshold},
            {"enable_ai_analysis", enable_ai_analysis},
            {"ai_confidence_threshold", ai_confidence_threshold},
            {"ai_model", ai_model}
        };

        // Category weights
        nlohmann::json cat_weights;
        for (const auto& [cat, weight] : category_weights) {
            cat_weights[std::to_string(static_cast<int>(cat))] = weight;
        }
        config["category_weights"] = cat_weights;

        // Factor weights
        nlohmann::json fact_weights;
        for (const auto& [factor, weight] : factor_weights) {
            fact_weights[std::to_string(static_cast<int>(factor))] = weight;
        }
        config["factor_weights"] = fact_weights;

        // High-risk lists
        config["high_risk_jurisdictions"] = high_risk_jurisdictions;
        config["high_risk_industries"] = high_risk_industries;

        return config;
    }
};

/**
 * @brief Transaction data for risk assessment
 */
struct TransactionData {
    std::string transaction_id;
    std::string entity_id;
    double amount;
    std::string currency;
    std::string transaction_type;
    std::string payment_method;
    std::chrono::system_clock::time_point transaction_time;
    std::string source_location;
    std::string destination_location;
    std::string counterparty_id;
    std::string counterparty_type;
    std::unordered_map<std::string, std::string> metadata;

    nlohmann::json to_json() const {
        return {
            {"transaction_id", transaction_id},
            {"entity_id", entity_id},
            {"amount", amount},
            {"currency", currency},
            {"transaction_type", transaction_type},
            {"payment_method", payment_method},
            {"transaction_time", std::chrono::duration_cast<std::chrono::milliseconds>(
                transaction_time.time_since_epoch()).count()},
            {"source_location", source_location},
            {"destination_location", destination_location},
            {"counterparty_id", counterparty_id},
            {"counterparty_type", counterparty_type},
            {"metadata", metadata}
        };
    }
};

/**
 * @brief Entity profile for risk assessment
 */
struct EntityProfile {
    std::string entity_id;
    std::string entity_type;  // "individual", "business", "organization"
    std::string business_type;
    std::string jurisdiction;
    std::chrono::system_clock::time_point account_creation_date;
    std::string verification_status;  // "unverified", "basic", "enhanced", "premium"
    std::vector<std::string> risk_flags;
    std::unordered_map<std::string, double> historical_risk_scores;
    std::unordered_map<std::string, std::string> metadata;

    nlohmann::json to_json() const {
        nlohmann::json profile = {
            {"entity_id", entity_id},
            {"entity_type", entity_type},
            {"business_type", business_type},
            {"jurisdiction", jurisdiction},
            {"account_creation_date", std::chrono::duration_cast<std::chrono::milliseconds>(
                account_creation_date.time_since_epoch()).count()},
            {"verification_status", verification_status},
            {"risk_flags", risk_flags},
            {"metadata", metadata}
        };

        nlohmann::json hist_scores;
        for (const auto& [date, score] : historical_risk_scores) {
            hist_scores[date] = score;
        }
        profile["historical_risk_scores"] = hist_scores;

        return profile;
    }
};

/**
 * @brief Comprehensive risk assessment engine
 *
 * Provides multi-factor risk analysis for transactions, entities, and regulatory compliance
 * using both rule-based algorithms and AI-powered analysis.
 */
class RiskAssessmentEngine {
public:
    RiskAssessmentEngine(std::shared_ptr<ConfigurationManager> config,
                        std::shared_ptr<StructuredLogger> logger,
                        std::shared_ptr<ErrorHandler> error_handler,
                        std::shared_ptr<OpenAIClient> openai_client = nullptr);

    ~RiskAssessmentEngine();

    /**
     * @brief Initialize the risk assessment engine
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Shutdown the engine and cleanup resources
     */
    void shutdown();

    /**
     * @brief Assess risk for a transaction
     * @param transaction Transaction data
     * @param entity_profile Entity profile information
     * @return Risk assessment result
     */
    RiskAssessment assess_transaction_risk(const TransactionData& transaction,
                                          const EntityProfile& entity_profile);

    /**
     * @brief Assess risk for an entity
     * @param entity_profile Entity profile
     * @param recent_transactions Recent transaction history
     * @return Risk assessment result
     */
    RiskAssessment assess_entity_risk(const EntityProfile& entity_profile,
                                     const std::vector<TransactionData>& recent_transactions);

    /**
     * @brief Assess regulatory compliance risk
     * @param entity_id Entity to assess
     * @param regulatory_context Current regulatory environment
     * @return Risk assessment result
     */
    RiskAssessment assess_regulatory_risk(const std::string& entity_id,
                                         const nlohmann::json& regulatory_context);

    /**
     * @brief Get risk assessment history
     * @param entity_id Entity identifier
     * @param limit Maximum number of assessments to return
     * @return Vector of historical risk assessments
     */
    std::vector<RiskAssessment> get_risk_history(const std::string& entity_id, int limit = 10);

    /**
     * @brief Update risk models with new data
     * @param assessment Completed risk assessment with outcome
     */
    void update_risk_models(const RiskAssessment& assessment);

    /**
     * @brief Get risk statistics and analytics
     * @return JSON with risk analytics data
     */
    nlohmann::json get_risk_analytics();

    /**
     * @brief Export risk assessment data for analysis
     * @param start_date Start date for export
     * @param end_date End date for export
     * @return JSON array of risk assessments
     */
    nlohmann::json export_risk_data(const std::chrono::system_clock::time_point& start_date,
                                  const std::chrono::system_clock::time_point& end_date);

    // Configuration access
    const RiskAssessmentConfig& get_config() const { return config_; }
    void update_config(const RiskAssessmentConfig& new_config) { config_ = new_config; }

    // --- Add missing declarations for risk scoring helpers ---
    double calculate_transaction_frequency_risk(size_t transaction_count) const;
    double calculate_amount_clustering_risk(double transaction_amount, const std::vector<double>& history) const;

private:
    std::shared_ptr<ConfigurationManager> config_manager_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<ErrorHandler> error_handler_;
    std::shared_ptr<OpenAIClient> openai_client_;

    RiskAssessmentConfig config_;

    // Risk assessment storage (in production, this would be in a database)
    std::unordered_map<std::string, std::vector<RiskAssessment>> risk_history_;
    std::mutex history_mutex_;

    // Statistical models and baselines
    std::unordered_map<std::string, double> entity_baselines_;
    std::unordered_map<std::string, std::vector<double>> transaction_amount_history_;

    // Risk scoring algorithms

    /**
     * @brief Calculate transaction-specific risk factors
     */
    std::unordered_map<RiskFactor, double> calculate_transaction_factors(
        const TransactionData& transaction, const EntityProfile& entity);

    /**
     * @brief Calculate entity-specific risk factors
     */
    std::unordered_map<RiskFactor, double> calculate_entity_factors(
        const EntityProfile& entity, const std::vector<TransactionData>& recent_transactions);

    /**
     * @brief Calculate regulatory risk factors
     */
    std::unordered_map<RiskFactor, double> calculate_regulatory_factors(
        const std::string& entity_id, const nlohmann::json& regulatory_context);

    /**
     * @brief Aggregate factor scores into category scores
     */
    std::unordered_map<RiskCategory, double> aggregate_category_scores(
        const std::unordered_map<RiskFactor, double>& factor_scores);

    /**
     * @brief Calculate overall risk score
     */
    double calculate_overall_score(const std::unordered_map<RiskCategory, double>& category_scores);

    /**
     * @brief Generate risk indicators based on assessment
     */
    std::vector<std::string> generate_risk_indicators(
    const RiskAssessment& assessment);

    /**
     * @brief Calculate deviation from customer's normal transaction patterns
     */
    double calculate_deviation_from_norm(double transaction_amount,
                                       const std::vector<double>& history) const;

    /**
     * @brief Calculate velocity changes in transaction patterns
     */
    double calculate_velocity_changes(const TransactionData& transaction,
                                    const std::vector<double>& history) const;

    /**
     * @brief Calculate peer comparison risk (simplified implementation)
     */
    double calculate_peer_comparison(double transaction_amount,
                                   const std::vector<double>& history) const;

    /**
     * @brief Generate recommended mitigation actions
     */
    std::vector<RiskMitigationAction> generate_mitigation_actions(
    const RiskAssessment& assessment);

    /**
     * @brief Perform AI-powered risk analysis
     */
    std::optional<nlohmann::json> perform_ai_risk_analysis(
        const TransactionData& transaction, const EntityProfile& entity);

    // Helper methods

    /**
     * @brief Check if location is high-risk jurisdiction
     */
    bool is_high_risk_jurisdiction(const std::string& location) const;

    /**
     * @brief Check if business type is high-risk industry
     */
    bool is_high_risk_industry(const std::string& business_type) const;

    /**
     * @brief Calculate amount risk score (0.0 to 1.0)
     */
    double calculate_amount_risk(double amount, const std::string& currency,
                               const std::vector<double>& historical_amounts);

    /**
     * @brief Calculate geographic risk score
     */
    double calculate_geographic_risk(const std::string& location);

    /**
     * @brief Calculate behavioral risk score
     */
    double calculate_behavioral_risk(const TransactionData& transaction,
                                   const std::vector<TransactionData>& history);

    /**
     * @brief Calculate velocity risk (transaction frequency changes)
     */
    double calculate_velocity_risk(const std::vector<TransactionData>& recent_transactions,
                                 const std::chrono::hours& time_window = std::chrono::hours(24));

    /**
     * @brief Update statistical baselines
     */
    void update_baselines(const TransactionData& transaction, const EntityProfile& entity);

    /**
     * @brief Load risk assessment configuration from environment
     */
    void load_configuration();

    /**
     * @brief Generate unique assessment ID
     */
    std::string generate_assessment_id() const;

    /**
     * @brief Validate assessment data
     */
    bool validate_assessment_data(const TransactionData& transaction,
                                const EntityProfile& entity) const;
};

// Utility functions

/**
 * @brief Convert risk severity to string
 */
inline std::string risk_severity_to_string(RiskSeverity severity) {
    switch (severity) {
        case RiskSeverity::LOW: return "LOW";
        case RiskSeverity::MEDIUM: return "MEDIUM";
        case RiskSeverity::HIGH: return "HIGH";
        case RiskSeverity::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Convert risk category to string
 */
inline std::string risk_category_to_string(RiskCategory category) {
    switch (category) {
        case RiskCategory::FINANCIAL: return "FINANCIAL";
        case RiskCategory::REGULATORY: return "REGULATORY";
        case RiskCategory::OPERATIONAL: return "OPERATIONAL";
        case RiskCategory::REPUTATIONAL: return "REPUTATIONAL";
        case RiskCategory::STRATEGIC: return "STRATEGIC";
        case RiskCategory::COMPLIANCE: return "COMPLIANCE";
        case RiskCategory::TRANSACTION: return "TRANSACTION";
        case RiskCategory::ENTITY: return "ENTITY";
        case RiskCategory::MARKET: return "MARKET";
        case RiskCategory::CYBER: return "CYBER";
        case RiskCategory::LEGAL: return "LEGAL";
        case RiskCategory::CONCENTRATION: return "CONCENTRATION";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Convert risk factor to string
 */
inline std::string risk_factor_to_string(RiskFactor factor) {
    switch (factor) {
        case RiskFactor::AMOUNT_SIZE: return "AMOUNT_SIZE";
        case RiskFactor::FREQUENCY_PATTERN: return "FREQUENCY_PATTERN";
        case RiskFactor::GEOGRAPHIC_LOCATION: return "GEOGRAPHIC_LOCATION";
        case RiskFactor::COUNTERPARTY_RISK: return "COUNTERPARTY_RISK";
        case RiskFactor::PAYMENT_METHOD: return "PAYMENT_METHOD";
        case RiskFactor::TIMING_PATTERN: return "TIMING_PATTERN";
        case RiskFactor::ROUND_NUMBERS: return "ROUND_NUMBERS";
        case RiskFactor::CUSTOMER_HISTORY: return "CUSTOMER_HISTORY";
        case RiskFactor::ACCOUNT_AGE: return "ACCOUNT_AGE";
        case RiskFactor::VERIFICATION_STATUS: return "VERIFICATION_STATUS";
        case RiskFactor::BUSINESS_TYPE: return "BUSINESS_TYPE";
        case RiskFactor::OWNERSHIP_STRUCTURE: return "OWNERSHIP_STRUCTURE";
        case RiskFactor::DEVIATION_FROM_NORM: return "DEVIATION_FROM_NORM";
        case RiskFactor::PEER_COMPARISON: return "PEER_COMPARISON";
        case RiskFactor::VELOCITY_CHANGES: return "VELOCITY_CHANGES";
        case RiskFactor::CHANNEL_MIX: return "CHANNEL_MIX";
        case RiskFactor::REGULATORY_CHANGES: return "REGULATORY_CHANGES";
        case RiskFactor::MARKET_CONDITIONS: return "MARKET_CONDITIONS";
        case RiskFactor::ECONOMIC_INDICATORS: return "ECONOMIC_INDICATORS";
        case RiskFactor::GEOPOLITICAL_EVENTS: return "GEOPOLITICAL_EVENTS";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Convert mitigation action to string
 */
inline std::string mitigation_action_to_string(RiskMitigationAction action) {
    switch (action) {
        case RiskMitigationAction::APPROVE: return "APPROVE";
        case RiskMitigationAction::APPROVE_WITH_MONITORING: return "APPROVE_WITH_MONITORING";
        case RiskMitigationAction::HOLD_FOR_REVIEW: return "HOLD_FOR_REVIEW";
        case RiskMitigationAction::ESCALATE: return "ESCALATE";
        case RiskMitigationAction::DECLINE: return "DECLINE";
        case RiskMitigationAction::REQUIRE_ADDITIONAL_INFO: return "REQUIRE_ADDITIONAL_INFO";
        case RiskMitigationAction::ENHANCE_VERIFICATION: return "ENHANCE_VERIFICATION";
        case RiskMitigationAction::REDUCE_LIMITS: return "REDUCE_LIMITS";
        case RiskMitigationAction::INCREASE_MONITORING: return "INCREASE_MONITORING";
        case RiskMitigationAction::REPORT_TO_AUTHORITIES: return "REPORT_TO_AUTHORITIES";
        default: return "UNKNOWN";
    }
}

} // namespace regulens
