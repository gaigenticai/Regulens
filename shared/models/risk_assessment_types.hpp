#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <optional>
#include <nlohmann/json.hpp>

enum class RiskSeverity {
    LOW, MEDIUM, HIGH, CRITICAL
};

enum class RiskMitigationAction {
    APPROVE,
    APPROVE_WITH_MONITORING,
    HOLD_FOR_REVIEW,
    ESCALATE,
    DECLINE,
    REQUIRE_ADDITIONAL_INFO,
    ENHANCE_VERIFICATION,
    REDUCE_LIMITS,
    INCREASE_MONITORING,
    REPORT_TO_AUTHORITIES
};

enum class RiskFactor {
    AMOUNT_SIZE,
    FREQUENCY_PATTERN,
    GEOGRAPHIC_LOCATION,
    COUNTERPARTY_RISK,
    PAYMENT_METHOD,
    TIMING_PATTERN,
    ROUND_NUMBERS,
    CUSTOMER_HISTORY,
    ACCOUNT_AGE,
    VERIFICATION_STATUS,
    BUSINESS_TYPE,
    OWNERSHIP_STRUCTURE,
    DEVIATION_FROM_NORM,
    PEER_COMPARISON,
    VELOCITY_CHANGES,
    CHANNEL_MIX,
    REGULATORY_CHANGES,
    MARKET_CONDITIONS,
    ECONOMIC_INDICATORS,
    GEOPOLITICAL_EVENTS
};

enum class RiskCategory {
    FINANCIAL,
    REGULATORY,
    OPERATIONAL,
    REPUTATIONAL,
    STRATEGIC,
    COMPLIANCE,
    TRANSACTION,
    ENTITY,
    MARKET,
    CYBER,
    LEGAL,
    CONCENTRATION
};

struct RiskAssessment {
    // Core fields
    std::string assessment_id;
    std::string entity_id;
    std::string transaction_id;
    std::string assessed_by;
    std::chrono::system_clock::time_point assessment_time;

    // Risk scoring
    double risk_score = 0.0;
    std::string risk_level;
    RiskSeverity overall_severity = RiskSeverity::LOW;
    double overall_score = 0.0;

    // Factors and indicators
    std::vector<std::string> risk_factors;
    std::vector<std::string> risk_indicators;
    std::vector<RiskMitigationAction> recommended_actions;
    std::unordered_map<RiskFactor, double> factor_contributions;
    std::unordered_map<RiskCategory, double> category_scores;

    // Additional fields
    std::unordered_map<std::string, std::string> context_data;
    std::optional<std::string> ai_analysis;

    nlohmann::json to_json() const {
        // For enums as keys, convert to string for JSON
        nlohmann::json fc_json;
        for (const auto& [k, v] : factor_contributions) {
            fc_json[std::to_string(static_cast<int>(k))] = v;
        }
        nlohmann::json cc_json;
        for (const auto& [k, v] : category_scores) {
            cc_json[std::to_string(static_cast<int>(k))] = v;
        }
        nlohmann::json ra_json;
        for (const auto& a : recommended_actions) {
            ra_json.push_back(static_cast<int>(a));
        }
        return {
            {"assessment_id", assessment_id},
            {"entity_id", entity_id},
            {"transaction_id", transaction_id},
            {"assessed_by", assessed_by},
            {"assessment_time", std::chrono::duration_cast<std::chrono::milliseconds>(assessment_time.time_since_epoch()).count()},
            {"risk_score", risk_score},
            {"risk_level", risk_level},
            {"overall_severity", static_cast<int>(overall_severity)},
            {"overall_score", overall_score},
            {"risk_factors", risk_factors},
            {"risk_indicators", risk_indicators},
            {"recommended_actions", ra_json},
            {"factor_contributions", fc_json},
            {"category_scores", cc_json},
            {"context_data", context_data},
            {"ai_analysis", ai_analysis.value_or("")}
        };
    }

    static RiskSeverity score_to_severity(double score) {
        if (score >= 0.9) return RiskSeverity::CRITICAL;
        if (score >= 0.7) return RiskSeverity::HIGH;
        if (score >= 0.4) return RiskSeverity::MEDIUM;
        return RiskSeverity::LOW;
    }
};
