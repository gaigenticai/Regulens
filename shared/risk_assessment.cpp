#include "risk_assessment.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <sstream>
#include <iomanip>
#include <regex>

namespace regulens {

RiskAssessmentEngine::RiskAssessmentEngine(std::shared_ptr<ConfigurationManager> config,
                                         std::shared_ptr<StructuredLogger> logger,
                                         std::shared_ptr<ErrorHandler> error_handler,
                                         std::shared_ptr<OpenAIClient> openai_client)
    : config_manager_(config), logger_(logger), error_handler_(error_handler),
      openai_client_(openai_client) {
}

RiskAssessmentEngine::~RiskAssessmentEngine() {
    shutdown();
}

bool RiskAssessmentEngine::initialize() {
    try {
        load_configuration();

        logger_->info("Risk Assessment Engine initialized with AI analysis: {}",
                     config_.enable_ai_analysis ? "enabled" : "disabled");
        return true;

    } catch (const std::exception& e) {
        logger_->error("Failed to initialize Risk Assessment Engine: {}", e.what());
        return false;
    }
}

void RiskAssessmentEngine::shutdown() {
    logger_->info("Risk Assessment Engine shutdown");
}

RiskAssessment RiskAssessmentEngine::assess_transaction_risk(const TransactionData& transaction,
                                                           const EntityProfile& entity) {
    RiskAssessment assessment;
    assessment.assessment_id = generate_assessment_id();
    assessment.entity_id = transaction.entity_id;
    assessment.transaction_id = transaction.transaction_id;
    assessment.assessed_by = config_.enable_ai_analysis ? "hybrid" : "automated";

    // Validate input data
    if (!validate_assessment_data(transaction, entity)) {
        assessment.overall_severity = RiskSeverity::CRITICAL;
        assessment.overall_score = 1.0;
        assessment.risk_indicators.push_back("INVALID_ASSESSMENT_DATA");
        assessment.recommended_actions.push_back(RiskMitigationAction::HOLD_FOR_REVIEW);
        return assessment;
    }

    // Calculate risk factors
    auto transaction_factors = calculate_transaction_factors(transaction, entity);
    auto entity_factors = calculate_entity_factors(entity, {transaction}); // Simplified - in production would get full history

    // Combine factor contributions
    std::unordered_map<RiskFactor, double> all_factors = transaction_factors;
    for (const auto& [factor, score] : entity_factors) {
        all_factors[factor] = std::max(all_factors[factor], score); // Take the higher risk score
    }

    assessment.factor_contributions = all_factors;

    // Aggregate into category scores
    assessment.category_scores = aggregate_category_scores(all_factors);

    // Calculate overall score
    assessment.overall_score = calculate_overall_score(assessment.category_scores);
    assessment.overall_severity = RiskAssessment::score_to_severity(assessment.overall_score);

    // Generate risk indicators and mitigation actions
    assessment.risk_indicators = generate_risk_indicators(assessment);
    assessment.recommended_actions = generate_mitigation_actions(assessment);

    // Store context data
    assessment.context_data = {
        {"transaction", transaction.to_json()},
        {"entity", entity.to_json()},
        {"assessment_time", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };

    // Perform AI analysis if enabled
    if (config_.enable_ai_analysis && openai_client_) {
        auto ai_analysis = perform_ai_risk_analysis(transaction, entity);
        if (ai_analysis) {
            assessment.ai_analysis = *ai_analysis;

            // Adjust score based on AI analysis if confidence is high
            if (ai_analysis->contains("confidence") &&
                ai_analysis->at("confidence").get<double>() >= config_.ai_confidence_threshold) {
                // Blend AI analysis with rule-based scoring
                double ai_score = ai_analysis->value("risk_score", assessment.overall_score);
                assessment.overall_score = (assessment.overall_score * 0.7) + (ai_score * 0.3);
                assessment.overall_severity = RiskAssessment::score_to_severity(assessment.overall_score);
            }
        }
    }

    // Store assessment in history
    {
        std::lock_guard<std::mutex> lock(history_mutex_);
        risk_history_[assessment.entity_id].push_back(assessment);

        // Keep only last 100 assessments per entity
        if (risk_history_[assessment.entity_id].size() > 100) {
            risk_history_[assessment.entity_id].erase(
                risk_history_[assessment.entity_id].begin());
        }
    }

    // Update statistical baselines
    update_baselines(transaction, entity);

    logger_->info("Completed transaction risk assessment for entity {}: score={:.3f}, severity={}",
                 entity.entity_id, assessment.overall_score,
                 risk_severity_to_string(assessment.overall_severity));

    return assessment;
}

RiskAssessment RiskAssessmentEngine::assess_entity_risk(const EntityProfile& entity,
                                                      const std::vector<TransactionData>& recent_transactions) {
    RiskAssessment assessment;
    assessment.assessment_id = generate_assessment_id();
    assessment.entity_id = entity.entity_id;
    assessment.assessed_by = config_.enable_ai_analysis ? "hybrid" : "automated";

    // Calculate entity-specific factors
    auto entity_factors = calculate_entity_factors(entity, recent_transactions);
    assessment.factor_contributions = entity_factors;

    // Focus on entity-related categories
    std::unordered_map<RiskCategory, double> category_scores;
    category_scores[RiskCategory::ENTITY] = 0.0;
    category_scores[RiskCategory::COMPLIANCE] = 0.0;
    category_scores[RiskCategory::REPUTATIONAL] = 0.0;

    // Aggregate entity factors
    for (const auto& [factor, score] : entity_factors) {
        double weight = config_.factor_weights.at(factor);

        if (factor == RiskFactor::CUSTOMER_HISTORY || factor == RiskFactor::ACCOUNT_AGE ||
            factor == RiskFactor::OWNERSHIP_STRUCTURE) {
            category_scores[RiskCategory::ENTITY] += score * weight;
        } else if (factor == RiskFactor::VERIFICATION_STATUS || factor == RiskFactor::BUSINESS_TYPE) {
            category_scores[RiskCategory::COMPLIANCE] += score * weight;
        }
    }

    assessment.category_scores = category_scores;
    assessment.overall_score = calculate_overall_score(category_scores);
    assessment.overall_severity = RiskAssessment::score_to_severity(assessment.overall_score);

    // Generate risk indicators and actions
    assessment.risk_indicators = generate_risk_indicators(assessment);
    assessment.recommended_actions = generate_mitigation_actions(assessment);

    // Store context
    assessment.context_data = {
        {"entity", entity.to_json()},
        {"recent_transactions_count", recent_transactions.size()},
        {"assessment_time", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };

    // Store in history
    {
        std::lock_guard<std::mutex> lock(history_mutex_);
        risk_history_[assessment.entity_id].push_back(assessment);
    }

    return assessment;
}

RiskAssessment RiskAssessmentEngine::assess_regulatory_risk(const std::string& entity_id,
                                                         const nlohmann::json& regulatory_context) {
    RiskAssessment assessment;
    assessment.assessment_id = generate_assessment_id();
    assessment.entity_id = entity_id;
    assessment.assessed_by = "automated";

    // Calculate regulatory factors
    auto regulatory_factors = calculate_regulatory_factors(entity_id, regulatory_context);
    assessment.factor_contributions = regulatory_factors;

    // Focus on regulatory categories
    std::unordered_map<RiskCategory, double> category_scores;
    category_scores[RiskCategory::REGULATORY] = 0.0;
    category_scores[RiskCategory::COMPLIANCE] = 0.0;
    category_scores[RiskCategory::LEGAL] = 0.0;

    // Aggregate regulatory factors
    for (const auto& [factor, score] : regulatory_factors) {
        double weight = config_.factor_weights.at(factor);
        category_scores[RiskCategory::REGULATORY] += score * weight;
    }

    assessment.category_scores = category_scores;
    assessment.overall_score = calculate_overall_score(category_scores);
    assessment.overall_severity = RiskAssessment::score_to_severity(assessment.overall_score);

    assessment.risk_indicators = generate_risk_indicators(assessment);
    assessment.recommended_actions = generate_mitigation_actions(assessment);

    assessment.context_data = {
        {"entity_id", entity_id},
        {"regulatory_context", regulatory_context},
        {"assessment_time", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };

    return assessment;
}

std::vector<RiskAssessment> RiskAssessmentEngine::get_risk_history(const std::string& entity_id, int limit) {
    std::lock_guard<std::mutex> lock(history_mutex_);

    auto it = risk_history_.find(entity_id);
    if (it == risk_history_.end()) {
        return {};
    }

    const auto& assessments = it->second;
    int start_idx = std::max(0, static_cast<int>(assessments.size()) - limit);

    return std::vector<RiskAssessment>(assessments.begin() + start_idx, assessments.end());
}

void RiskAssessmentEngine::update_risk_models(const RiskAssessment& assessment) {
    // Update statistical models based on assessment outcomes
    // In a production system, this would update machine learning models
    // For now, we update simple statistical baselines

    if (assessment.entity_id.empty()) return;

    // Update entity baseline risk score
    {
        std::lock_guard<std::mutex> lock(history_mutex_);
        auto history = get_risk_history(assessment.entity_id, 10); // Last 10 assessments

        if (!history.empty()) {
            double avg_score = 0.0;
            for (const auto& past_assessment : history) {
                avg_score += past_assessment.overall_score;
            }
            avg_score /= history.size();

            entity_baselines_[assessment.entity_id] = avg_score;
        }
    }

    logger_->debug("Updated risk models for entity: {}", assessment.entity_id);
}

nlohmann::json RiskAssessmentEngine::get_risk_analytics() {
    std::lock_guard<std::mutex> lock(history_mutex_);

    // Calculate overall statistics
    int total_assessments = 0;
    std::unordered_map<RiskSeverity, int> severity_counts;
    std::unordered_map<RiskCategory, double> avg_category_scores;
    std::unordered_map<RiskCategory, int> category_counts;

    for (const auto& [entity_id, assessments] : risk_history_) {
        for (const auto& assessment : assessments) {
            total_assessments++;
            severity_counts[assessment.overall_severity]++;

            for (const auto& [category, score] : assessment.category_scores) {
                avg_category_scores[category] += score;
                category_counts[category]++;
            }
        }
    }

    // Calculate averages
    for (auto& [category, total_score] : avg_category_scores) {
        if (category_counts[category] > 0) {
            total_score /= category_counts[category];
        }
    }

    return {
        {"total_assessments", total_assessments},
        {"severity_distribution", {
            {"low", severity_counts[RiskSeverity::LOW]},
            {"medium", severity_counts[RiskSeverity::MEDIUM]},
            {"high", severity_counts[RiskSeverity::HIGH]},
            {"critical", severity_counts[RiskSeverity::CRITICAL]}
        }},
        {"average_category_scores", [&]() {
            nlohmann::json scores;
            for (const auto& [category, score] : avg_category_scores) {
                scores[risk_category_to_string(category)] = score;
            }
            return scores;
        }()},
        {"entities_with_assessments", risk_history_.size()},
        {"configuration", config_.to_json()}
    };
}

nlohmann::json RiskAssessmentEngine::export_risk_data(
    const std::chrono::system_clock::time_point& start_date,
    const std::chrono::system_clock::time_point& end_date) {

    std::lock_guard<std::mutex> lock(history_mutex_);

    nlohmann::json export_data = nlohmann::json::array();

    for (const auto& [entity_id, assessments] : risk_history_) {
        for (const auto& assessment : assessments) {
            if (assessment.assessment_time >= start_date && assessment.assessment_time <= end_date) {
                export_data.push_back(assessment.to_json());
            }
        }
    }

    return export_data;
}

// Private implementation methods

std::unordered_map<RiskFactor, double> RiskAssessmentEngine::calculate_transaction_factors(
    const TransactionData& transaction, const EntityProfile& entity) {

    std::unordered_map<RiskFactor, double> factors;

    // Amount size risk
    factors[RiskFactor::AMOUNT_SIZE] = calculate_amount_risk(
        transaction.amount, transaction.currency,
        transaction_amount_history_[transaction.entity_id]);

    // Geographic location risk
    double geo_risk = 0.0;
    if (!transaction.source_location.empty()) {
        geo_risk = std::max(geo_risk, calculate_geographic_risk(transaction.source_location));
    }
    if (!transaction.destination_location.empty()) {
        geo_risk = std::max(geo_risk, calculate_geographic_risk(transaction.destination_location));
    }
    factors[RiskFactor::GEOGRAPHIC_LOCATION] = geo_risk;

    // Payment method risk
    if (transaction.payment_method == "cash" || transaction.payment_method == "cryptocurrency") {
        factors[RiskFactor::PAYMENT_METHOD] = 0.8;
    } else if (transaction.payment_method == "wire_transfer") {
        factors[RiskFactor::PAYMENT_METHOD] = 0.4;
    } else {
        factors[RiskFactor::PAYMENT_METHOD] = 0.1;
    }

    // Round numbers (potential structuring)
    if (std::fmod(transaction.amount, 1000.0) == 0.0 && transaction.amount >= 10000.0) {
        factors[RiskFactor::ROUND_NUMBERS] = 0.7;
    } else {
        factors[RiskFactor::ROUND_NUMBERS] = 0.0;
    }

    // Timing pattern (off-hours transactions)
    auto hour = std::chrono::duration_cast<std::chrono::hours>(
        transaction.transaction_time.time_since_epoch()).count() % 24;
    if (hour < 6 || hour > 22) { // Before 6 AM or after 10 PM
        factors[RiskFactor::TIMING_PATTERN] = 0.3;
    } else {
        factors[RiskFactor::TIMING_PATTERN] = 0.0;
    }

    // Counterparty risk
    if (!transaction.counterparty_type.empty()) {
        if (transaction.counterparty_type == "high_risk" ||
            transaction.counterparty_type == "sanctioned") {
            factors[RiskFactor::COUNTERPARTY_RISK] = 0.9;
        } else if (transaction.counterparty_type == "unknown") {
            factors[RiskFactor::COUNTERPARTY_RISK] = 0.5;
        } else {
            factors[RiskFactor::COUNTERPARTY_RISK] = 0.1;
        }
    }

    // Behavioral factors (would need transaction history in production)
    factors[RiskFactor::DEVIATION_FROM_NORM] = 0.2; // Placeholder
    factors[RiskFactor::VELOCITY_CHANGES] = 0.1;     // Placeholder
    factors[RiskFactor::PEER_COMPARISON] = 0.1;      // Placeholder

    return factors;
}

std::unordered_map<RiskFactor, double> RiskAssessmentEngine::calculate_entity_factors(
    const EntityProfile& entity, const std::vector<TransactionData>& recent_transactions) {

    std::unordered_map<RiskFactor, double> factors;

    // Account age
    auto now = std::chrono::system_clock::now();
    auto account_age_days = std::chrono::duration_cast<std::chrono::hours>(
        now - entity.account_creation_date).count() / 24.0;

    if (account_age_days < 30) {
        factors[RiskFactor::ACCOUNT_AGE] = 0.8; // Very new account
    } else if (account_age_days < 90) {
        factors[RiskFactor::ACCOUNT_AGE] = 0.4; // Relatively new
    } else {
        factors[RiskFactor::ACCOUNT_AGE] = 0.1; // Established account
    }

    // Verification status
    if (entity.verification_status == "unverified") {
        factors[RiskFactor::VERIFICATION_STATUS] = 0.9;
    } else if (entity.verification_status == "basic") {
        factors[RiskFactor::VERIFICATION_STATUS] = 0.5;
    } else if (entity.verification_status == "enhanced") {
        factors[RiskFactor::VERIFICATION_STATUS] = 0.2;
    } else {
        factors[RiskFactor::VERIFICATION_STATUS] = 0.0; // Premium/fully verified
    }

    // Business type risk
    factors[RiskFactor::BUSINESS_TYPE] = is_high_risk_industry(entity.business_type) ? 0.8 : 0.1;

    // Geographic jurisdiction
    factors[RiskFactor::GEOGRAPHIC_LOCATION] = is_high_risk_jurisdiction(entity.jurisdiction) ? 0.9 : 0.1;

    // Ownership structure complexity
    if (entity.entity_type == "business" || entity.entity_type == "organization") {
        factors[RiskFactor::OWNERSHIP_STRUCTURE] = 0.3; // Simplified - would analyze ownership complexity
    } else {
        factors[RiskFactor::OWNERSHIP_STRUCTURE] = 0.1;
    }

    // Customer history (simplified)
    if (!entity.historical_risk_scores.empty()) {
        double avg_historical_score = 0.0;
        for (const auto& [_, score] : entity.historical_risk_scores) {
            avg_historical_score += score;
        }
        avg_historical_score /= entity.historical_risk_scores.size();
        factors[RiskFactor::CUSTOMER_HISTORY] = avg_historical_score;
    } else {
        factors[RiskFactor::CUSTOMER_HISTORY] = 0.3; // No history available
    }

    // Risk flags
    if (!entity.risk_flags.empty()) {
        factors[RiskFactor::CUSTOMER_HISTORY] = std::min(1.0, factors[RiskFactor::CUSTOMER_HISTORY] + 0.2);
    }

    // Calculate velocity risk from recent transactions
    if (!recent_transactions.empty()) {
        factors[RiskFactor::VELOCITY_CHANGES] = calculate_velocity_risk(recent_transactions);
    }

    return factors;
}

std::unordered_map<RiskFactor, double> RiskAssessmentEngine::calculate_regulatory_factors(
    const std::string& entity_id, const nlohmann::json& regulatory_context) {

    std::unordered_map<RiskFactor, double> factors;

    // Regulatory changes impact
    if (regulatory_context.contains("recent_changes") &&
        regulatory_context["recent_changes"].is_array() &&
        !regulatory_context["recent_changes"].empty()) {
        factors[RiskFactor::REGULATORY_CHANGES] = 0.7;
    } else {
        factors[RiskFactor::REGULATORY_CHANGES] = 0.1;
    }

    // Market conditions
    if (regulatory_context.contains("market_volatility")) {
        double volatility = regulatory_context["market_volatility"];
        factors[RiskFactor::MARKET_CONDITIONS] = std::min(1.0, volatility / 100.0);
    } else {
        factors[RiskFactor::MARKET_CONDITIONS] = 0.2; // Default moderate volatility
    }

    // Economic indicators
    if (regulatory_context.contains("economic_stress")) {
        factors[RiskFactor::ECONOMIC_INDICATORS] = regulatory_context["economic_stress"];
    } else {
        factors[RiskFactor::ECONOMIC_INDICATORS] = 0.3;
    }

    // Geopolitical events
    if (regulatory_context.contains("geopolitical_risk")) {
        factors[RiskFactor::GEOPOLITICAL_EVENTS] = regulatory_context["geopolitical_risk"];
    } else {
        factors[RiskFactor::GEOPOLITICAL_EVENTS] = 0.1;
    }

    return factors;
}

std::unordered_map<RiskCategory, double> RiskAssessmentEngine::aggregate_category_scores(
    const std::unordered_map<RiskFactor, double>& factor_scores) {

    std::unordered_map<RiskCategory, double> category_scores;
    std::unordered_map<RiskCategory, double> category_totals;

    // Initialize categories
    for (const auto& [category, _] : config_.category_weights) {
        category_scores[category] = 0.0;
        category_totals[category] = 0.0;
    }

    // Map factors to categories and aggregate
    for (const auto& [factor, score] : factor_scores) {
        double weight = config_.factor_weights.at(factor);

        // Map factors to categories (simplified mapping)
        std::vector<RiskCategory> factor_categories;

        switch (factor) {
            case RiskFactor::AMOUNT_SIZE:
            case RiskFactor::PAYMENT_METHOD:
            case RiskFactor::ROUND_NUMBERS:
                factor_categories = {RiskCategory::FINANCIAL, RiskCategory::TRANSACTION};
                break;
            case RiskFactor::GEOGRAPHIC_LOCATION:
                factor_categories = {RiskCategory::COMPLIANCE, RiskCategory::ENTITY};
                break;
            case RiskFactor::COUNTERPARTY_RISK:
                factor_categories = {RiskCategory::FINANCIAL, RiskCategory::ENTITY};
                break;
            case RiskFactor::TIMING_PATTERN:
            case RiskFactor::FREQUENCY_PATTERN:
                factor_categories = {RiskCategory::OPERATIONAL, RiskCategory::TRANSACTION};
                break;
            case RiskFactor::CUSTOMER_HISTORY:
            case RiskFactor::ACCOUNT_AGE:
            case RiskFactor::OWNERSHIP_STRUCTURE:
                factor_categories = {RiskCategory::ENTITY, RiskCategory::REPUTATIONAL};
                break;
            case RiskFactor::VERIFICATION_STATUS:
            case RiskFactor::BUSINESS_TYPE:
                factor_categories = {RiskCategory::COMPLIANCE, RiskCategory::LEGAL};
                break;
            case RiskFactor::DEVIATION_FROM_NORM:
            case RiskFactor::PEER_COMPARISON:
            case RiskFactor::VELOCITY_CHANGES:
                factor_categories = {RiskCategory::OPERATIONAL, RiskCategory::TRANSACTION};
                break;
            case RiskFactor::REGULATORY_CHANGES:
                factor_categories = {RiskCategory::REGULATORY, RiskCategory::COMPLIANCE};
                break;
            case RiskFactor::MARKET_CONDITIONS:
                factor_categories = {RiskCategory::MARKET, RiskCategory::STRATEGIC};
                break;
            case RiskFactor::ECONOMIC_INDICATORS:
            case RiskFactor::GEOPOLITICAL_EVENTS:
                factor_categories = {RiskCategory::STRATEGIC, RiskCategory::MARKET};
                break;
            default:
                factor_categories = {RiskCategory::COMPLIANCE};
                break;
        }

        // Distribute factor score across relevant categories
        double score_per_category = score * weight / factor_categories.size();
        for (auto category : factor_categories) {
            category_scores[category] += score_per_category;
            category_totals[category] += weight / factor_categories.size();
        }
    }

    // Normalize category scores
    for (auto& [category, score] : category_scores) {
        if (category_totals[category] > 0.0) {
            score = std::min(1.0, score / category_totals[category]);
        }
    }

    return category_scores;
}

double RiskAssessmentEngine::calculate_overall_score(
    const std::unordered_map<RiskCategory, double>& category_scores) {

    double weighted_sum = 0.0;
    double total_weight = 0.0;

    for (const auto& [category, score] : category_scores) {
        double weight = config_.category_weights.at(category);
        weighted_sum += score * weight;
        total_weight += weight;
    }

    return total_weight > 0.0 ? std::min(1.0, weighted_sum / total_weight) : 0.0;
}

std::vector<std::string> RiskAssessmentEngine::generate_risk_indicators(const RiskAssessment& assessment) {
    std::vector<std::string> indicators;

    // High-level risk indicators based on score thresholds
    if (assessment.overall_score >= config_.critical_threshold) {
        indicators.push_back("CRITICAL_RISK_LEVEL");
    } else if (assessment.overall_score >= config_.high_threshold) {
        indicators.push_back("HIGH_RISK_LEVEL");
    } else if (assessment.overall_score >= config_.medium_threshold) {
        indicators.push_back("MEDIUM_RISK_LEVEL");
    }

    // Category-specific indicators
    for (const auto& [category, score] : assessment.category_scores) {
        if (score >= 0.8) {
            indicators.push_back("HIGH_" + risk_category_to_string(category) + "_RISK");
        }
    }

    // Factor-specific indicators
    for (const auto& [factor, score] : assessment.factor_contributions) {
        if (score >= 0.7) {
            indicators.push_back("HIGH_" + risk_factor_to_string(factor) + "_SCORE");
        }
    }

    // Specific business rule indicators
    if (assessment.factor_contributions.count(RiskFactor::GEOGRAPHIC_LOCATION) &&
        assessment.factor_contributions.at(RiskFactor::GEOGRAPHIC_LOCATION) >= 0.8) {
        indicators.push_back("HIGH_RISK_JURISDICTION");
    }

    if (assessment.factor_contributions.count(RiskFactor::ROUND_NUMBERS) &&
        assessment.factor_contributions.at(RiskFactor::ROUND_NUMBERS) >= 0.5) {
        indicators.push_back("POTENTIAL_STRUCTURING");
    }

    return indicators;
}

std::vector<RiskMitigationAction> RiskAssessmentEngine::generate_mitigation_actions(const RiskAssessment& assessment) {
    std::vector<RiskMitigationAction> actions;

    // Base actions on risk severity
    if (assessment.overall_severity == RiskSeverity::CRITICAL) {
        actions.push_back(RiskMitigationAction::DECLINE);
        actions.push_back(RiskMitigationAction::REPORT_TO_AUTHORITIES);
    } else if (assessment.overall_severity == RiskSeverity::HIGH) {
        actions.push_back(RiskMitigationAction::HOLD_FOR_REVIEW);
        actions.push_back(RiskMitigationAction::ENHANCE_VERIFICATION);
        actions.push_back(RiskMitigationAction::INCREASE_MONITORING);
    } else if (assessment.overall_severity == RiskSeverity::MEDIUM) {
        actions.push_back(RiskMitigationAction::APPROVE_WITH_MONITORING);
        actions.push_back(RiskMitigationAction::REQUIRE_ADDITIONAL_INFO);
    } else {
        actions.push_back(RiskMitigationAction::APPROVE);
    }

    // Additional actions based on specific factors
    if (assessment.factor_contributions.count(RiskFactor::VERIFICATION_STATUS) &&
        assessment.factor_contributions.at(RiskFactor::VERIFICATION_STATUS) >= 0.7) {
        actions.push_back(RiskMitigationAction::ENHANCE_VERIFICATION);
    }

    if (assessment.factor_contributions.count(RiskFactor::GEOGRAPHIC_LOCATION) &&
        assessment.factor_contributions.at(RiskFactor::GEOGRAPHIC_LOCATION) >= 0.6) {
        actions.push_back(RiskMitigationAction::REQUIRE_ADDITIONAL_INFO);
    }

    return actions;
}

std::optional<nlohmann::json> RiskAssessmentEngine::perform_ai_risk_analysis(
    const TransactionData& transaction, const EntityProfile& entity) {

    if (!openai_client_) return std::nullopt;

    try {
        std::string analysis_prompt = R"(
You are an expert financial crime prevention analyst. Analyze the following transaction and entity information for potential risks.

Transaction Details:
- Amount: $)" + std::to_string(transaction.amount) + R"(
- Currency: )" + transaction.currency + R"(
- Type: )" + transaction.transaction_type + R"(
- Payment Method: )" + transaction.payment_method + R"(
- Source Location: )" + transaction.source_location + R"(
- Destination Location: )" + transaction.destination_location + R"(
- Time: )" + std::to_string(std::chrono::duration_cast<std::chrono::hours>(
    transaction.transaction_time.time_since_epoch()).count() % 24) + R"(:00

Entity Details:
- Type: )" + entity.entity_type + R"(
- Business Type: )" + entity.business_type + R"(
- Jurisdiction: )" + entity.jurisdiction + R"(
- Verification Status: )" + entity.verification_status + R"(
- Account Age: )" + std::to_string(std::chrono::duration_cast<std::chrono::hours>(
    std::chrono::system_clock::now() - entity.account_creation_date).count() / 24) + R"( days

Risk Factors to Consider:
1. Money laundering patterns
2. Sanctions evasion
3. Fraud indicators
4. Regulatory compliance issues
5. Unusual transaction patterns

Provide a risk assessment score (0.0 to 1.0) and detailed reasoning.
Format your response as JSON with fields: risk_score, confidence, reasoning, key_risks)";

        OpenAICompletionRequest request{
            .model = config_.ai_model,
            .messages = {
                OpenAIMessage{"system", "You are a financial risk assessment expert. Provide analysis in valid JSON format only."},
                OpenAIMessage{"user", analysis_prompt}
            },
            .temperature = 0.1,  // Low temperature for consistent analysis
            .max_tokens = 1000
        };

        auto response = openai_client_->create_chat_completion(request);
        if (!response || response->choices.empty()) {
            return std::nullopt;
        }

        // Parse AI response
        std::string ai_response = response->choices[0].message.content;

        // Extract JSON from response (AI might wrap it in markdown)
        std::regex json_regex(R"~~~(```json\s*([\s\S]*?)\s*```)~~~");
        std::smatch match;
        if (std::regex_search(ai_response, match, json_regex)) {
            ai_response = match[1].str();
        }

        return nlohmann::json::parse(ai_response);

    } catch (const std::exception& e) {
        logger_->error("AI risk analysis failed: {}", e.what());
        return std::nullopt;
    }
}

// Helper method implementations

bool RiskAssessmentEngine::is_high_risk_jurisdiction(const std::string& location) const {
    return config_.high_risk_jurisdictions.find(location) != config_.high_risk_jurisdictions.end();
}

bool RiskAssessmentEngine::is_high_risk_industry(const std::string& business_type) const {
    return config_.high_risk_industries.find(business_type) != config_.high_risk_industries.end();
}

double RiskAssessmentEngine::calculate_amount_risk(double amount, const std::string& currency,
                                                 const std::vector<double>& historical_amounts) {
    // Simplified amount risk calculation
    // In production, this would use statistical analysis of historical amounts

    // Base risk on amount size
    double size_risk = 0.0;
    if (amount > 100000) {
        size_risk = 0.8;
    } else if (amount > 50000) {
        size_risk = 0.6;
    } else if (amount > 10000) {
        size_risk = 0.4;
    } else {
        size_risk = 0.1;
    }

    // Check deviation from historical amounts
    double deviation_risk = 0.0;
    if (!historical_amounts.empty()) {
        double avg_historical = 0.0;
        for (double hist_amount : historical_amounts) {
            avg_historical += hist_amount;
        }
        avg_historical /= historical_amounts.size();

        if (avg_historical > 0) {
            double deviation = std::abs(amount - avg_historical) / avg_historical;
            deviation_risk = std::min(0.5, deviation);
        }
    }

    return std::min(1.0, size_risk + deviation_risk);
}

double RiskAssessmentEngine::calculate_geographic_risk(const std::string& location) {
    if (is_high_risk_jurisdiction(location)) {
        return 0.9;
    }

    // Simplified geographic risk - in production would use comprehensive lists
    static const std::unordered_set<std::string> medium_risk_locations = {
        "Russia", "China", "India", "Brazil", "Mexico"
    };

    if (medium_risk_locations.find(location) != medium_risk_locations.end()) {
        return 0.5;
    }

    return 0.1; // Low risk for most locations
}

double RiskAssessmentEngine::calculate_velocity_risk(const std::vector<TransactionData>& recent_transactions,
                                                   const std::chrono::hours& time_window) {
    if (recent_transactions.size() < 3) return 0.0;

    // Count transactions in the time window
    auto cutoff_time = std::chrono::system_clock::now() - time_window;
    int transactions_in_window = 0;
    double total_amount = 0.0;

    for (const auto& transaction : recent_transactions) {
        if (transaction.transaction_time >= cutoff_time) {
            transactions_in_window++;
            total_amount += transaction.amount;
        }
    }

    // Calculate velocity risk based on transaction frequency and amount
    double frequency_risk = 0.0;
    if (transactions_in_window >= 10) {
        frequency_risk = 0.8;
    } else if (transactions_in_window >= 5) {
        frequency_risk = 0.4;
    }

    // Calculate amount velocity risk
    double avg_amount = total_amount / transactions_in_window;
    double amount_risk = 0.0;
    if (avg_amount > 50000) {
        amount_risk = 0.6;
    } else if (avg_amount > 10000) {
        amount_risk = 0.3;
    }

    return std::min(1.0, frequency_risk + amount_risk);
}

void RiskAssessmentEngine::update_baselines(const TransactionData& transaction, const EntityProfile& entity) {
    // Update statistical baselines for future risk assessments
    transaction_amount_history_[transaction.entity_id].push_back(transaction.amount);

    // Keep only last 50 transactions for statistical analysis
    if (transaction_amount_history_[transaction.entity_id].size() > 50) {
        transaction_amount_history_[transaction.entity_id].erase(
            transaction_amount_history_[transaction.entity_id].begin());
    }
}

void RiskAssessmentEngine::load_configuration() {
    // Load configuration from environment variables
    config_.enable_ai_analysis = config_manager_->get_bool("RISK_ENABLE_AI_ANALYSIS")
                                .value_or(true);
    config_.ai_confidence_threshold = config_manager_->get_double("RISK_AI_CONFIDENCE_THRESHOLD")
                                     .value_or(0.7);
    config_.ai_model = config_manager_->get_string("RISK_AI_MODEL")
                      .value_or("compliance_risk");

    // Load risk thresholds
    config_.critical_threshold = config_manager_->get_double("RISK_CRITICAL_THRESHOLD")
                                .value_or(0.8);
    config_.high_threshold = config_manager_->get_double("RISK_HIGH_THRESHOLD")
                            .value_or(0.6);
    config_.medium_threshold = config_manager_->get_double("RISK_MEDIUM_THRESHOLD")
                              .value_or(0.4);

    // Load high-risk lists from configuration if available
    // For now, using hardcoded defaults - in production would load from config files
}

std::string RiskAssessmentEngine::generate_assessment_id() const {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);

    return "risk_" + std::to_string(ms) + "_" + std::to_string(dis(gen));
}

bool RiskAssessmentEngine::validate_assessment_data(const TransactionData& transaction,
                                                  const EntityProfile& entity) const {
    // Basic validation of input data
    if (transaction.entity_id.empty() || transaction.transaction_id.empty()) {
        return false;
    }

    if (entity.entity_id != transaction.entity_id) {
        return false;
    }

    if (transaction.amount <= 0.0) {
        return false;
    }

    return true;
}

} // namespace regulens
