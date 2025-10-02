#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <memory>
#include <functional>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <nlohmann/json.hpp>

#include "logging/structured_logger.hpp"
#include "config/configuration_manager.hpp"
#include "error_handler.hpp"
#include "llm/openai_client.hpp"
#include "llm/anthropic_client.hpp"
#include "risk_assessment.hpp"

namespace regulens {

/**
 * @brief Decision tree node types
 */
enum class DecisionNodeType {
    DECISION,       // Decision point with branches
    CHANCE,         // Chance/probability node
    TERMINAL,       // End node with outcome
    UTILITY         // Utility assessment node
};

/**
 * @brief Decision criteria for multi-criteria analysis
 */
enum class DecisionCriterion {
    FINANCIAL_IMPACT,       // Monetary costs/benefits
    REGULATORY_COMPLIANCE,  // Compliance with regulations
    RISK_LEVEL,            // Risk assessment score
    OPERATIONAL_IMPACT,    // Operational complexity/effort
    STRATEGIC_ALIGNMENT,   // Alignment with business strategy
    ETHICAL_CONSIDERATIONS,// Ethical implications
    LEGAL_RISK,           // Legal liability exposure
    REPUTATIONAL_IMPACT,  // Brand/reputation effects
    TIME_TO_IMPLEMENT,    // Implementation timeline
    RESOURCE_REQUIREMENTS, // Required resources/staff
    STAKEHOLDER_IMPACT,   // Impact on stakeholders
    MARKET_POSITION       // Competitive positioning
};

/**
 * @brief Decision alternative/outcome
 */
struct DecisionAlternative {
    std::string id;
    std::string name;
    std::string description;
    std::unordered_map<DecisionCriterion, double> criteria_scores;  // 0.0 to 1.0
    std::unordered_map<DecisionCriterion, double> criteria_weights; // Relative importance
    std::vector<std::string> advantages;
    std::vector<std::string> disadvantages;
    std::vector<std::string> risks;
    nlohmann::json metadata;

    nlohmann::json to_json() const {
        nlohmann::json alt = {
            {"id", id},
            {"name", name},
            {"description", description},
            {"advantages", advantages},
            {"disadvantages", disadvantages},
            {"risks", risks},
            {"metadata", metadata}
        };

        nlohmann::json scores;
        for (const auto& [criterion, score] : criteria_scores) {
            scores[std::to_string(static_cast<int>(criterion))] = score;
        }
        alt["criteria_scores"] = scores;

        nlohmann::json weights;
        for (const auto& [criterion, weight] : criteria_weights) {
            weights[std::to_string(static_cast<int>(criterion))] = weight;
        }
        alt["criteria_weights"] = weights;

        return alt;
    }
};

/**
 * @brief Decision tree node
 */
struct DecisionNode {
    std::string id;
    std::string label;
    DecisionNodeType type;
    std::string description;
    std::optional<DecisionAlternative> alternative;  // For terminal nodes
    std::vector<std::shared_ptr<DecisionNode>> children;
    std::unordered_map<std::string, double> probabilities;  // For chance nodes
    std::unordered_map<DecisionCriterion, double> utility_values;  // For utility nodes
    nlohmann::json metadata;

    DecisionNode(const std::string& node_id, const std::string& node_label,
                DecisionNodeType node_type = DecisionNodeType::DECISION)
        : id(node_id), label(node_label), type(node_type) {}

    nlohmann::json to_json() const {
        nlohmann::json node = {
            {"id", id},
            {"label", label},
            {"type", static_cast<int>(type)},
            {"description", description},
            {"metadata", metadata}
        };

        if (alternative) {
            node["alternative"] = alternative->to_json();
        }

        nlohmann::json child_nodes;
        for (const auto& child : children) {
            if (child) {
                child_nodes.push_back(child->to_json());
            }
        }
        node["children"] = child_nodes;

        nlohmann::json probs;
        for (const auto& [key, prob] : probabilities) {
            probs[key] = prob;
        }
        node["probabilities"] = probs;

        nlohmann::json utilities;
        for (const auto& [criterion, value] : utility_values) {
            utilities[std::to_string(static_cast<int>(criterion))] = value;
        }
        node["utility_values"] = utilities;

        return node;
    }
};

/**
 * @brief Multi-criteria decision analysis (MCDA) methods
 */
enum class MCDAMethod {
    WEIGHTED_SUM,          // Simple weighted sum
    WEIGHTED_PRODUCT,      // Weighted product method
    TOPSIS,               // Technique for Order Preference by Similarity to Ideal Solution
    ELECTRE,              // Elimination and Choice Expressing Reality
    PROMETHEE,            // Preference Ranking Organization Method for Enrichment Evaluation
    AHP,                  // Analytic Hierarchy Process
    VIKOR                // VIseKriterijumska Optimizacija I Kompromisno Resenje
};

/**
 * @brief Decision tree analysis result
 */
struct DecisionAnalysisResult {
    std::string analysis_id;
    std::string decision_problem;
    std::chrono::system_clock::time_point analysis_time;
    MCDAMethod method_used;

    std::vector<DecisionAlternative> alternatives;
    std::unordered_map<std::string, double> alternative_scores;  // Alternative ID -> score
    std::vector<std::string> ranking;  // Ordered by preference (best first)
    std::string recommended_alternative;

    // Decision tree analysis
    std::shared_ptr<DecisionNode> decision_tree;
    double expected_value;  // For decision trees with probabilities
    std::unordered_map<std::string, double> sensitivity_analysis;

    // AI-powered analysis
    nlohmann::json ai_analysis;
    nlohmann::json risk_assessment;

    DecisionAnalysisResult()
        : analysis_time(std::chrono::system_clock::now()),
          method_used(MCDAMethod::WEIGHTED_SUM),
          expected_value(0.0) {}

    nlohmann::json to_json() const {
        nlohmann::json result = {
            {"analysis_id", analysis_id},
            {"decision_problem", decision_problem},
            {"analysis_time", std::chrono::duration_cast<std::chrono::milliseconds>(
                analysis_time.time_since_epoch()).count()},
            {"method_used", static_cast<int>(method_used)},
            {"recommended_alternative", recommended_alternative},
            {"expected_value", expected_value},
            {"ai_analysis", ai_analysis},
            {"risk_assessment", risk_assessment}
        };

        nlohmann::json alts;
        for (const auto& alt : alternatives) {
            alts.push_back(alt.to_json());
        }
        result["alternatives"] = alts;

        nlohmann::json scores;
        for (const auto& [alt_id, score] : alternative_scores) {
            scores[alt_id] = score;
        }
        result["alternative_scores"] = scores;

        result["ranking"] = ranking;

        if (decision_tree) {
            result["decision_tree"] = decision_tree->to_json();
        }

        nlohmann::json sensitivity;
        for (const auto& [param, impact] : sensitivity_analysis) {
            sensitivity[param] = impact;
        }
        result["sensitivity_analysis"] = sensitivity;

        return result;
    }
};

/**
 * @brief Decision tree optimization configuration
 */
struct DecisionTreeConfig {
    MCDAMethod default_method = MCDAMethod::WEIGHTED_SUM;
    bool enable_ai_analysis = true;
    bool enable_risk_integration = true;
    double risk_weight = 0.3;  // Weight for risk assessment in decision making
    double ai_confidence_threshold = 0.7;
    int max_tree_depth = 10;
    int max_alternatives = 20;
    bool enable_sensitivity_analysis = true;
    std::string ai_model = "decision_analysis";

    // MCDA method parameters
    struct {
        double topsis_distance_p = 2.0;  // p-norm for TOPSIS
        double electre_threshold = 0.7;  // Concordance threshold for ELECTRE
        double promethee_preference_threshold = 0.1;  // Preference threshold for PROMETHEE
    } mcda_params;

    nlohmann::json to_json() const {
        return {
            {"default_method", static_cast<int>(default_method)},
            {"enable_ai_analysis", enable_ai_analysis},
            {"enable_risk_integration", enable_risk_integration},
            {"risk_weight", risk_weight},
            {"ai_confidence_threshold", ai_confidence_threshold},
            {"max_tree_depth", max_tree_depth},
            {"max_alternatives", max_alternatives},
            {"enable_sensitivity_analysis", enable_sensitivity_analysis},
            {"ai_model", ai_model},
            {"mcda_params", {
                {"topsis_distance_p", mcda_params.topsis_distance_p},
                {"electre_threshold", mcda_params.electre_threshold},
                {"promethee_preference_threshold", mcda_params.promethee_preference_threshold}
            }}
        };
    }
};

/**
 * @brief Comprehensive decision tree optimizer and multi-criteria decision analysis engine
 *
 * Provides advanced decision-making capabilities for complex compliance and regulatory scenarios
 * using multiple MCDA methods, decision tree analysis, and AI-powered optimization.
 */
class DecisionTreeOptimizer {
public:
    DecisionTreeOptimizer(std::shared_ptr<ConfigurationManager> config,
                         std::shared_ptr<StructuredLogger> logger,
                         std::shared_ptr<ErrorHandler> error_handler,
                         std::shared_ptr<OpenAIClient> openai_client = nullptr,
                         std::shared_ptr<AnthropicClient> anthropic_client = nullptr,
                         std::shared_ptr<RiskAssessmentEngine> risk_engine = nullptr);

    ~DecisionTreeOptimizer();

    /**
     * @brief Initialize the decision tree optimizer
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Shutdown the optimizer and cleanup resources
     */
    void shutdown();

    /**
     * @brief Perform multi-criteria decision analysis
     * @param decision_problem Description of the decision problem
     * @param alternatives Available decision alternatives
     * @param method MCDA method to use
     * @return Decision analysis result
     */
    DecisionAnalysisResult analyze_decision_mcda(
        const std::string& decision_problem,
        const std::vector<DecisionAlternative>& alternatives,
        MCDAMethod method = MCDAMethod::WEIGHTED_SUM);

    /**
     * @brief Build and analyze a decision tree
     * @param root_node Root node of the decision tree
     * @param decision_problem Description of the decision problem
     * @return Decision analysis result with tree analysis
     */
    DecisionAnalysisResult analyze_decision_tree(
        std::shared_ptr<DecisionNode> root_node,
        const std::string& decision_problem);

    /**
     * @brief Optimize decision tree structure
     * @param tree Decision tree to optimize
     * @return Optimized decision tree
     */
    std::shared_ptr<DecisionNode> optimize_decision_tree(std::shared_ptr<DecisionNode> tree);

    /**
     * @brief Perform sensitivity analysis on decision parameters
     * @param analysis Completed decision analysis
     * @return Sensitivity analysis results
     */
    std::unordered_map<std::string, double> perform_sensitivity_analysis(
        const DecisionAnalysisResult& analysis);

    /**
     * @brief Generate decision recommendations with AI assistance
     * @param decision_problem Decision problem description
     * @param alternatives Available alternatives
     * @param context Additional context for analysis
     * @return AI-enhanced decision analysis
     */
    DecisionAnalysisResult generate_ai_decision_recommendation(
        const std::string& decision_problem,
        const std::vector<DecisionAlternative>& alternatives,
        const std::string& context = "");

    /**
     * @brief Create decision alternative from description
     * @param description Alternative description
     * @param context Decision context
     * @return Structured decision alternative
     */
    DecisionAlternative create_decision_alternative(
        const std::string& description,
        const std::string& context = "");

    /**
     * @brief Evaluate decision tree expected value
     * @param node Decision tree node
     * @return Expected value of the subtree
     */
    double calculate_expected_value(std::shared_ptr<DecisionNode> node);

    /**
     * @brief Export decision analysis for visualization
     * @param analysis Decision analysis result
     * @return JSON suitable for visualization tools
     */
    nlohmann::json export_for_visualization(const DecisionAnalysisResult& analysis);

    /**
     * @brief Get decision analysis history
     * @param limit Maximum number of analyses to return
     * @return Vector of historical decision analyses
     */
    std::vector<DecisionAnalysisResult> get_analysis_history(int limit = 10);

    // Configuration access
    const DecisionTreeConfig& get_config() const { return config_; }
    void update_config(const DecisionTreeConfig& new_config) { config_ = new_config; }

private:
    std::shared_ptr<ConfigurationManager> config_manager_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<ErrorHandler> error_handler_;
    std::shared_ptr<OpenAIClient> openai_client_;
    std::shared_ptr<AnthropicClient> anthropic_client_;
    std::shared_ptr<RiskAssessmentEngine> risk_engine_;

    DecisionTreeConfig config_;

    // Analysis history storage (in production, this would be in a database)
    std::vector<DecisionAnalysisResult> analysis_history_;
    std::mutex history_mutex_;

    // MCDA algorithm implementations

    /**
     * @brief Weighted Sum Model (WSM)
     */
    std::unordered_map<std::string, double> weighted_sum_model(
        const std::vector<DecisionAlternative>& alternatives);

    /**
     * @brief Weighted Product Model (WPM)
     */
    std::unordered_map<std::string, double> weighted_product_model(
        const std::vector<DecisionAlternative>& alternatives);

    /**
     * @brief TOPSIS method
     */
    std::unordered_map<std::string, double> topsis_method(
        const std::vector<DecisionAlternative>& alternatives);

    /**
     * @brief ELECTRE method (simplified implementation)
     */
    std::unordered_map<std::string, double> electre_method(
        const std::vector<DecisionAlternative>& alternatives);

    /**
     * @brief PROMETHEE method (simplified implementation)
     */
    std::unordered_map<std::string, double> promethee_method(
        const std::vector<DecisionAlternative>& alternatives);

    /**
     * @brief Analytic Hierarchy Process (simplified)
     */
    std::unordered_map<std::string, double> ahp_method(
        const std::vector<DecisionAlternative>& alternatives);

    /**
     * @brief VIKOR method
     */
    std::unordered_map<std::string, double> vikor_method(
        const std::vector<DecisionAlternative>& alternatives);

    // Decision tree algorithms

    /**
     * @brief Prune decision tree to optimal depth
     */
    std::shared_ptr<DecisionNode> prune_decision_tree(std::shared_ptr<DecisionNode> node, int max_depth);

    /**
     * @brief Balance decision tree probabilities
     */
    void balance_probabilities(std::shared_ptr<DecisionNode> node);

    /**
     * @brief Validate decision tree structure
     */
    bool validate_decision_tree(std::shared_ptr<DecisionNode> node);

    // AI integration methods

    /**
     * @brief Generate decision alternatives using AI
     */
    std::vector<DecisionAlternative> generate_ai_alternatives(
        const std::string& decision_problem, int max_alternatives = 5);

    /**
     * @brief Score alternatives using AI analysis
     */
    std::unordered_map<std::string, double> score_alternatives_ai(
        const std::vector<DecisionAlternative>& alternatives,
        const std::string& decision_context);

    /**
     * @brief Perform AI-powered decision reasoning
     */
    std::optional<nlohmann::json> perform_ai_decision_analysis(
        const std::string& decision_problem,
        const std::vector<DecisionAlternative>& alternatives,
        const std::string& context);

    // Utility methods

    /**
     * @brief Normalize criteria scores
     */
    void normalize_criteria_scores(std::vector<DecisionAlternative>& alternatives);

    /**
     * @brief Calculate default weights for criteria
     */
    std::unordered_map<DecisionCriterion, double> calculate_default_weights();

    /**
     * @brief Rank alternatives by score
     */
    std::vector<std::string> rank_alternatives(
        const std::unordered_map<std::string, double>& scores);

    /**
     * @brief Generate unique analysis ID
     */
    std::string generate_analysis_id() const;

    /**
     * @brief Load decision tree configuration from environment
     */
    void load_configuration();

    /**
     * @brief Validate decision analysis input
     */
    bool validate_decision_input(const std::string& decision_problem,
                                const std::vector<DecisionAlternative>& alternatives) const;
};

// Utility functions

/**
 * @brief Convert MCDA method to string
 */
inline std::string mcda_method_to_string(MCDAMethod method) {
    switch (method) {
        case MCDAMethod::WEIGHTED_SUM: return "WEIGHTED_SUM";
        case MCDAMethod::WEIGHTED_PRODUCT: return "WEIGHTED_PRODUCT";
        case MCDAMethod::TOPSIS: return "TOPSIS";
        case MCDAMethod::ELECTRE: return "ELECTRE";
        case MCDAMethod::PROMETHEE: return "PROMETHEE";
        case MCDAMethod::AHP: return "AHP";
        case MCDAMethod::VIKOR: return "VIKOR";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Convert decision criterion to string
 */
inline std::string decision_criterion_to_string(DecisionCriterion criterion) {
    switch (criterion) {
        case DecisionCriterion::FINANCIAL_IMPACT: return "FINANCIAL_IMPACT";
        case DecisionCriterion::REGULATORY_COMPLIANCE: return "REGULATORY_COMPLIANCE";
        case DecisionCriterion::RISK_LEVEL: return "RISK_LEVEL";
        case DecisionCriterion::OPERATIONAL_IMPACT: return "OPERATIONAL_IMPACT";
        case DecisionCriterion::STRATEGIC_ALIGNMENT: return "STRATEGIC_ALIGNMENT";
        case DecisionCriterion::ETHICAL_CONSIDERATIONS: return "ETHICAL_CONSIDERATIONS";
        case DecisionCriterion::LEGAL_RISK: return "LEGAL_RISK";
        case DecisionCriterion::REPUTATIONAL_IMPACT: return "REPUTATIONAL_IMPACT";
        case DecisionCriterion::TIME_TO_IMPLEMENT: return "TIME_TO_IMPLEMENT";
        case DecisionCriterion::RESOURCE_REQUIREMENTS: return "RESOURCE_REQUIREMENTS";
        case DecisionCriterion::STAKEHOLDER_IMPACT: return "STAKEHOLDER_IMPACT";
        case DecisionCriterion::MARKET_POSITION: return "MARKET_POSITION";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Convert decision node type to string
 */
inline std::string decision_node_type_to_string(DecisionNodeType type) {
    switch (type) {
        case DecisionNodeType::DECISION: return "DECISION";
        case DecisionNodeType::CHANCE: return "CHANCE";
        case DecisionNodeType::TERMINAL: return "TERMINAL";
        case DecisionNodeType::UTILITY: return "UTILITY";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Create a simple decision alternative
 */
inline DecisionAlternative create_simple_alternative(
    const std::string& id, const std::string& name, const std::string& description,
    const std::unordered_map<DecisionCriterion, double>& scores) {

    DecisionAlternative alt;
    alt.id = id;
    alt.name = name;
    alt.description = description;
    alt.criteria_scores = scores;

    // Set equal weights by default
    for (const auto& [criterion, _] : scores) {
        alt.criteria_weights[criterion] = 1.0 / scores.size();
    }

    return alt;
}

} // namespace regulens
