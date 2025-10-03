#include "decision_tree_optimizer.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <sstream>
#include <iomanip>
#include <limits>
#include <queue>
#include <stack>

namespace regulens {

DecisionTreeOptimizer::DecisionTreeOptimizer(std::shared_ptr<ConfigurationManager> config,
                                           std::shared_ptr<StructuredLogger> logger,
                                           std::shared_ptr<ErrorHandler> error_handler,
                                           std::shared_ptr<OpenAIClient> openai_client,
                                           std::shared_ptr<AnthropicClient> anthropic_client,
                                           std::shared_ptr<RiskAssessmentEngine> risk_engine)
    : config_manager_(config), logger_(logger), error_handler_(error_handler),
      openai_client_(openai_client), anthropic_client_(anthropic_client),
      risk_engine_(risk_engine) {
}

DecisionTreeOptimizer::~DecisionTreeOptimizer() {
    shutdown();
}

bool DecisionTreeOptimizer::initialize() {
    try {
        load_configuration();

        logger_->info("Decision Tree Optimizer initialized with AI analysis: {}",
                     config_.enable_ai_analysis ? "enabled" : "disabled");
        return true;

    } catch (const std::exception& e) {
        logger_->error("Failed to initialize Decision Tree Optimizer: {}", e.what());
        return false;
    }
}

void DecisionTreeOptimizer::shutdown() {
    logger_->info("Decision Tree Optimizer shutdown");
}

DecisionAnalysisResult DecisionTreeOptimizer::analyze_decision_mcda(
    const std::string& decision_problem,
    const std::vector<DecisionAlternative>& alternatives,
    MCDAMethod method) {

    DecisionAnalysisResult result;
    result.analysis_id = generate_analysis_id();
    result.decision_problem = decision_problem;
    result.method_used = method;
    result.alternatives = alternatives;

    // Validate input
    if (!validate_decision_input(decision_problem, alternatives)) {
        result.recommended_alternative = "";
        return result;
    }

    // Normalize criteria scores
    auto normalized_alternatives = alternatives;
    normalize_criteria_scores(normalized_alternatives);

    // Apply selected MCDA method
    switch (method) {
        case MCDAMethod::WEIGHTED_SUM:
            result.alternative_scores = weighted_sum_model(normalized_alternatives);
            break;
        case MCDAMethod::WEIGHTED_PRODUCT:
            result.alternative_scores = weighted_product_model(normalized_alternatives);
            break;
        case MCDAMethod::TOPSIS:
            result.alternative_scores = topsis_method(normalized_alternatives);
            break;
        case MCDAMethod::ELECTRE:
            result.alternative_scores = electre_method(normalized_alternatives);
            break;
        case MCDAMethod::PROMETHEE:
            result.alternative_scores = promethee_method(normalized_alternatives);
            break;
        case MCDAMethod::AHP:
            result.alternative_scores = ahp_method(normalized_alternatives);
            break;
        case MCDAMethod::VIKOR:
            result.alternative_scores = vikor_method(normalized_alternatives);
            break;
        default:
            result.alternative_scores = weighted_sum_model(normalized_alternatives);
            break;
    }

    // Generate ranking
    result.ranking = rank_alternatives(result.alternative_scores);

    // Set recommended alternative (highest score)
    if (!result.ranking.empty()) {
        result.recommended_alternative = result.ranking[0];
    }

    // Perform sensitivity analysis if enabled
    if (config_.enable_sensitivity_analysis) {
        result.sensitivity_analysis = perform_sensitivity_analysis(result);
    }

    // Integrate risk assessment if available
    if (config_.enable_risk_integration && risk_engine_) {
        // Create a simple risk assessment for the decision
        nlohmann::json regulatory_context = {
            {"decision_problem", decision_problem},
            {"alternatives_count", alternatives.size()},
            {"method_used", mcda_method_to_string(method)}
        };

        try {
            RiskAssessment risk_assessment = risk_engine_->assess_regulatory_risk(
                "decision_" + result.analysis_id, regulatory_context);
            result.risk_assessment = risk_assessment.to_json();
        } catch (const std::exception& e) {
            logger_->warn("Risk assessment integration failed: {}", e.what());
        }
    }

    // Add AI analysis if enabled
    if (config_.enable_ai_analysis) {
        auto ai_analysis = perform_ai_decision_analysis(
            decision_problem, alternatives, "MCDA analysis using " + mcda_method_to_string(method));
        if (ai_analysis) {
            result.ai_analysis = *ai_analysis;
        }
    }

    // Store in history
    {
        std::lock_guard<std::mutex> lock(history_mutex_);
        analysis_history_.push_back(result);

        // Keep only last 50 analyses
        if (analysis_history_.size() > 50) {
            analysis_history_.erase(analysis_history_.begin());
        }
    }

    logger_->info("Completed MCDA analysis for '" + decision_problem + "': method=" +
                 mcda_method_to_string(method) + ", alternatives=" + std::to_string(alternatives.size()) +
                 ", recommended=" + result.recommended_alternative);

    return result;
}

DecisionAnalysisResult DecisionTreeOptimizer::analyze_decision_tree(
    std::shared_ptr<DecisionNode> root_node, const std::string& decision_problem) {

    DecisionAnalysisResult result;
    result.analysis_id = generate_analysis_id();
    result.decision_problem = decision_problem;
    result.method_used = MCDAMethod::WEIGHTED_SUM; // Default for trees
    result.decision_tree = root_node;

    // Validate tree structure
    if (!validate_decision_tree(root_node)) {
        logger_->error("Invalid decision tree structure");
        return result;
    }

    // Optimize tree structure
    result.decision_tree = optimize_decision_tree(root_node);

    // Calculate expected value
    result.expected_value = calculate_expected_value(result.decision_tree);

    // Extract alternatives from terminal nodes
    std::vector<DecisionAlternative> alternatives;
    std::function<void(std::shared_ptr<DecisionNode>)> extract_alternatives =
        [&](std::shared_ptr<DecisionNode> node) {
            if (!node) return;

            if (node->type == DecisionNodeType::TERMINAL && node->alternative) {
                alternatives.push_back(*node->alternative);
            }

            for (const auto& child : node->children) {
                extract_alternatives(child);
            }
        };

    extract_alternatives(result.decision_tree);
    result.alternatives = alternatives;

    // Score alternatives based on expected values
    for (const auto& alt : alternatives) {
        // Simplified scoring - in production would use more sophisticated methods
        double score = 0.0;
        for (const auto& [criterion, value] : alt.criteria_scores) {
            score += value * alt.criteria_weights.at(criterion);
        }
        result.alternative_scores[alt.id] = score;
    }

    // Generate ranking
    result.ranking = rank_alternatives(result.alternative_scores);
    if (!result.ranking.empty()) {
        result.recommended_alternative = result.ranking[0];
    }

    // Add AI analysis for tree-based decisions
    if (config_.enable_ai_analysis) {
        std::string tree_context = "Decision tree analysis with " +
                                 std::to_string(alternatives.size()) + " terminal outcomes";
        auto ai_analysis = perform_ai_decision_analysis(decision_problem, alternatives, tree_context);
        if (ai_analysis) {
            result.ai_analysis = *ai_analysis;
        }
    }

    // Store in history
    {
        std::lock_guard<std::mutex> lock(history_mutex_);
        analysis_history_.push_back(result);
    }

    return result;
}

std::shared_ptr<DecisionNode> DecisionTreeOptimizer::optimize_decision_tree(std::shared_ptr<DecisionNode> tree) {
    if (!tree) return nullptr;

    // Prune tree to optimal depth
    tree = prune_decision_tree(tree, config_.max_tree_depth);

    // Balance probabilities for chance nodes
    balance_probabilities(tree);

    return tree;
}

std::unordered_map<std::string, double> DecisionTreeOptimizer::perform_sensitivity_analysis(
    const DecisionAnalysisResult& analysis) {

    std::unordered_map<std::string, double> sensitivity;

    // Test sensitivity to criteria weights (simplified)
    for (size_t i = 0; i < analysis.alternatives.size() && i < 3; ++i) {
        const auto& alt = analysis.alternatives[i];
        std::string param = "weight_sensitivity_" + alt.id;

        // Calculate impact of 10% weight change
        double original_score = analysis.alternative_scores.at(alt.id);
        double sensitivity_score = original_score * 1.1; // Simplified calculation
        double impact = std::abs(sensitivity_score - original_score) / original_score;

        sensitivity[param] = impact;
    }

    return sensitivity;
}

DecisionAnalysisResult DecisionTreeOptimizer::generate_ai_decision_recommendation(
    const std::string& decision_problem,
    const std::vector<DecisionAlternative>& alternatives,
    const std::string& context) {

    DecisionAnalysisResult result;
    result.analysis_id = generate_analysis_id();
    result.decision_problem = decision_problem;
    result.method_used = MCDAMethod::WEIGHTED_SUM;

    // Generate alternatives using AI if we have few alternatives
    std::vector<DecisionAlternative> final_alternatives = alternatives;
    if (final_alternatives.size() < 3 && config_.enable_ai_analysis) {
        auto ai_alternatives = generate_ai_alternatives(decision_problem,
            std::min(5, static_cast<int>(config_.max_alternatives) - static_cast<int>(final_alternatives.size())));
        final_alternatives.insert(final_alternatives.end(),
                                ai_alternatives.begin(), ai_alternatives.end());
    }

    result.alternatives = final_alternatives;

    // Use AI to score alternatives
    if (config_.enable_ai_analysis) {
        result.alternative_scores = score_alternatives_ai(final_alternatives, context);
    } else {
        // Fallback to weighted sum
        normalize_criteria_scores(final_alternatives);
        result.alternative_scores = weighted_sum_model(final_alternatives);
    }

    // Generate ranking and recommendation
    result.ranking = rank_alternatives(result.alternative_scores);
    if (!result.ranking.empty()) {
        result.recommended_alternative = result.ranking[0];
    }

    // Get detailed AI analysis
    auto ai_analysis = perform_ai_decision_analysis(decision_problem, final_alternatives, context);
    if (ai_analysis) {
        result.ai_analysis = *ai_analysis;
    }

    // Store in history
    {
        std::lock_guard<std::mutex> lock(history_mutex_);
        analysis_history_.push_back(result);
    }

    return result;
}

DecisionAlternative DecisionTreeOptimizer::create_decision_alternative(
    const std::string& description, const std::string& /*context*/) {

    DecisionAlternative alt;
    alt.id = "alt_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    alt.name = "Generated Alternative";
    alt.description = description;

    // Set default criteria scores (would be enhanced with AI analysis in production)
    alt.criteria_scores = {
        {DecisionCriterion::FINANCIAL_IMPACT, 0.5},
        {DecisionCriterion::REGULATORY_COMPLIANCE, 0.7},
        {DecisionCriterion::RISK_LEVEL, 0.4},
        {DecisionCriterion::OPERATIONAL_IMPACT, 0.6},
        {DecisionCriterion::STRATEGIC_ALIGNMENT, 0.5},
        {DecisionCriterion::ETHICAL_CONSIDERATIONS, 0.8}
    };

    // Set equal weights
    for (const auto& [criterion, _] : alt.criteria_scores) {
        alt.criteria_weights[criterion] = 1.0 / alt.criteria_scores.size();
    }

    // Parse advantages/disadvantages from description (simplified)
    alt.advantages = {"AI-generated alternative"};
    alt.disadvantages = {"Requires further evaluation"};

    return alt;
}

double DecisionTreeOptimizer::calculate_expected_value(std::shared_ptr<DecisionNode> node) {
    if (!node) return 0.0;

    if (node->type == DecisionNodeType::TERMINAL) {
        // Calculate utility value from criteria
        double utility = 0.0;
        for (const auto& [criterion, value] : node->utility_values) {
            utility += value; // Simplified - would use proper utility function
        }
        return utility;
    }

    if (node->type == DecisionNodeType::CHANCE && !node->probabilities.empty()) {
        // Expected value for chance node
        double expected_value = 0.0;
        for (size_t i = 0; i < node->children.size() && i < node->probabilities.size(); ++i) {
            double prob = node->probabilities.begin()->second; // Simplified
            expected_value += prob * calculate_expected_value(node->children[i]);
        }
        return expected_value;
    }

    if (node->type == DecisionNodeType::DECISION) {
        // Max value for decision node (choose best alternative)
        double max_value = std::numeric_limits<double>::lowest();
        for (const auto& child : node->children) {
            max_value = std::max(max_value, calculate_expected_value(child));
        }
        return max_value;
    }

    return 0.0;
}

nlohmann::json DecisionTreeOptimizer::export_for_visualization(const DecisionAnalysisResult& analysis) {
    nlohmann::json visualization = {
        {"analysis_id", analysis.analysis_id},
        {"decision_problem", analysis.decision_problem},
        {"method", mcda_method_to_string(analysis.method_used)},
        {"recommended_alternative", analysis.recommended_alternative},
        {"ranking", analysis.ranking}
    };

    // Alternatives data for charts
    nlohmann::json alternatives_data;
    for (const auto& alt : analysis.alternatives) {
        nlohmann::json alt_data = {
            {"id", alt.id},
            {"name", alt.name},
            {"score", analysis.alternative_scores.at(alt.id)},
            {"criteria", nlohmann::json::object()}
        };

        for (const auto& [criterion, score] : alt.criteria_scores) {
            alt_data["criteria"][decision_criterion_to_string(criterion)] = score;
        }

        alternatives_data.push_back(alt_data);
    }
    visualization["alternatives"] = alternatives_data;

    // Decision tree data (if available)
    if (analysis.decision_tree) {
        visualization["decision_tree"] = analysis.decision_tree->to_json();
    }

    return visualization;
}

std::vector<DecisionAnalysisResult> DecisionTreeOptimizer::get_analysis_history(int limit) {
    std::lock_guard<std::mutex> lock(history_mutex_);

    int start_idx = std::max(0, static_cast<int>(analysis_history_.size()) - limit);
    return std::vector<DecisionAnalysisResult>(
        analysis_history_.begin() + start_idx, analysis_history_.end());
}

// MCDA Method Implementations

std::unordered_map<std::string, double> DecisionTreeOptimizer::weighted_sum_model(
    const std::vector<DecisionAlternative>& alternatives) {

    std::unordered_map<std::string, double> scores;

    for (const auto& alt : alternatives) {
        double score = 0.0;
        for (const auto& [criterion, criterion_score] : alt.criteria_scores) {
            double weight = alt.criteria_weights.at(criterion);
            score += criterion_score * weight;
        }
        scores[alt.id] = score;
    }

    return scores;
}

std::unordered_map<std::string, double> DecisionTreeOptimizer::weighted_product_model(
    const std::vector<DecisionAlternative>& alternatives) {

    std::unordered_map<std::string, double> scores;

    for (const auto& alt : alternatives) {
        double score = 1.0;
        for (const auto& [criterion, criterion_score] : alt.criteria_scores) {
            double weight = alt.criteria_weights.at(criterion);
            score *= std::pow(criterion_score, weight);
        }
        scores[alt.id] = score;
    }

    return scores;
}

std::unordered_map<std::string, double> DecisionTreeOptimizer::topsis_method(
    const std::vector<DecisionAlternative>& alternatives) {

    if (alternatives.empty()) return {};

    // Get all criteria
    std::unordered_set<DecisionCriterion> all_criteria;
    for (const auto& alt : alternatives) {
        for (const auto& [criterion, _] : alt.criteria_scores) {
            all_criteria.insert(criterion);
        }
    }

    size_t n = alternatives.size();
    size_t m = all_criteria.size();

    // Create decision matrix
    std::vector<std::vector<double>> matrix(n, std::vector<double>(m, 0.0));
    std::vector<DecisionCriterion> criteria_order(all_criteria.begin(), all_criteria.end());

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < m; ++j) {
            auto it = alternatives[i].criteria_scores.find(criteria_order[j]);
            matrix[i][j] = (it != alternatives[i].criteria_scores.end()) ? it->second : 0.0;
        }
    }

    // Normalize matrix (vector normalization)
    for (size_t j = 0; j < m; ++j) {
        double sum_squares = 0.0;
        for (size_t i = 0; i < n; ++i) {
            sum_squares += matrix[i][j] * matrix[i][j];
        }
        double norm = std::sqrt(sum_squares);
        if (norm > 0) {
            for (size_t i = 0; i < n; ++i) {
                matrix[i][j] /= norm;
            }
        }
    }

    // Apply weights
    for (size_t i = 0; i < n; ++i) {
        const auto& alt = alternatives[i];
        for (size_t j = 0; j < m; ++j) {
            auto it = alt.criteria_weights.find(criteria_order[j]);
            double weight = (it != alt.criteria_weights.end()) ? it->second : (1.0 / m);
            matrix[i][j] *= weight;
        }
    }

    // Find ideal and negative-ideal solutions
    std::vector<double> ideal_solution(m, 0.0);
    std::vector<double> negative_ideal_solution(m, std::numeric_limits<double>::max());

    for (size_t j = 0; j < m; ++j) {
        for (size_t i = 0; i < n; ++i) {
            ideal_solution[j] = std::max(ideal_solution[j], matrix[i][j]);
            negative_ideal_solution[j] = std::min(negative_ideal_solution[j], matrix[i][j]);
        }
    }

    // Calculate distances and similarity scores
    std::unordered_map<std::string, double> scores;
    for (size_t i = 0; i < n; ++i) {
        double dist_ideal = 0.0;
        double dist_negative_ideal = 0.0;

        for (size_t j = 0; j < m; ++j) {
            dist_ideal += std::pow(matrix[i][j] - ideal_solution[j], config_.mcda_params.topsis_distance_p);
            dist_negative_ideal += std::pow(matrix[i][j] - negative_ideal_solution[j], config_.mcda_params.topsis_distance_p);
        }

        dist_ideal = std::pow(dist_ideal, 1.0 / config_.mcda_params.topsis_distance_p);
        dist_negative_ideal = std::pow(dist_negative_ideal, 1.0 / config_.mcda_params.topsis_distance_p);

        double similarity = dist_negative_ideal / (dist_ideal + dist_negative_ideal);
        scores[alternatives[i].id] = similarity;
    }

    return scores;
}

std::unordered_map<std::string, double> DecisionTreeOptimizer::electre_method(
    const std::vector<DecisionAlternative>& alternatives) {

    // Simplified ELECTRE implementation
    // In production, this would include concordance/discordance matrices

    std::unordered_map<std::string, double> scores;

    for (const auto& alt : alternatives) {
        // Simple scoring based on threshold comparisons
        double score = 0.0;
        for (const auto& [criterion, criterion_score] : alt.criteria_scores) {
            if (criterion_score >= config_.mcda_params.electre_threshold) {
                score += alt.criteria_weights.at(criterion);
            }
        }
        scores[alt.id] = score;
    }

    return scores;
}

std::unordered_map<std::string, double> DecisionTreeOptimizer::promethee_method(
    const std::vector<DecisionAlternative>& alternatives) {

    // Simplified PROMETHEE implementation
    std::unordered_map<std::string, double> scores;

    for (const auto& alt : alternatives) {
        double positive_flow = 0.0;
        double negative_flow = 0.0;

        // Compare with each other alternative
        for (const auto& other : alternatives) {
            if (alt.id == other.id) continue;

            double preference = 0.0;
            for (const auto& [criterion, score] : alt.criteria_scores) {
                auto other_it = other.criteria_scores.find(criterion);
                if (other_it != other.criteria_scores.end()) {
                    double diff = score - other_it->second;
                    if (diff > config_.mcda_params.promethee_preference_threshold) {
                        preference += alt.criteria_weights.at(criterion);
                    }
                }
            }

            if (preference > 0) positive_flow += preference;
            else negative_flow += std::abs(preference);
        }

        // Net flow as score
        scores[alt.id] = positive_flow - negative_flow;
    }

    return scores;
}

std::unordered_map<std::string, double> DecisionTreeOptimizer::ahp_method(
    const std::vector<DecisionAlternative>& alternatives) {

    // Simplified AHP implementation - would need full pairwise comparisons in production
    // For now, use weighted sum as approximation
    return weighted_sum_model(alternatives);
}

std::unordered_map<std::string, double> DecisionTreeOptimizer::vikor_method(
    const std::vector<DecisionAlternative>& alternatives) {

    // Simplified VIKOR implementation
    std::unordered_map<std::string, double> scores;

    if (alternatives.empty()) return scores;

    // Find best and worst values for each criterion
    std::unordered_map<DecisionCriterion, double> best_values, worst_values;

    for (const auto& alt : alternatives) {
        for (const auto& [criterion, score] : alt.criteria_scores) {
            if (best_values.find(criterion) == best_values.end()) {
                best_values[criterion] = score;
                worst_values[criterion] = score;
            } else {
                best_values[criterion] = std::max(best_values[criterion], score);
                worst_values[criterion] = std::min(worst_values[criterion], score);
            }
        }
    }

    // Calculate VIKOR scores
    for (const auto& alt : alternatives) {
        double s = 0.0; // Group utility
        double r = 0.0; // Individual regret

        for (const auto& [criterion, score] : alt.criteria_scores) {
            double weight = alt.criteria_weights.at(criterion);
            double best = best_values[criterion];
            double worst = worst_values[criterion];

            if (best != worst) {
                double normalized = (best - score) / (best - worst);
                s += weight * normalized;
                r = std::max(r, weight * normalized);
            }
        }

        // VIKOR combines S and R (simplified - typically uses v parameter)
        scores[alt.id] = 0.5 * s + 0.5 * r;
    }

    return scores;
}

// Decision Tree Methods

std::shared_ptr<DecisionNode> DecisionTreeOptimizer::prune_decision_tree(
    std::shared_ptr<DecisionNode> node, int max_depth) {

    if (!node || max_depth <= 0) return nullptr;

    auto pruned_node = std::make_shared<DecisionNode>(node->id, node->label, node->type);
    pruned_node->description = node->description;
    pruned_node->alternative = node->alternative;
    pruned_node->probabilities = node->probabilities;
    pruned_node->utility_values = node->utility_values;
    pruned_node->metadata = node->metadata;

    for (const auto& child : node->children) {
        auto pruned_child = prune_decision_tree(child, max_depth - 1);
        if (pruned_child) {
            pruned_node->children.push_back(pruned_child);
        }
    }

    return pruned_node;
}

void DecisionTreeOptimizer::balance_probabilities(std::shared_ptr<DecisionNode> node) {
    if (!node || node->type != DecisionNodeType::CHANCE) return;

    // Ensure probabilities sum to 1.0
    double total_prob = 0.0;
    for (const auto& [_, prob] : node->probabilities) {
        total_prob += prob;
    }

    if (total_prob > 0) {
        for (auto& [key, prob] : node->probabilities) {
            prob /= total_prob;
        }
    }
}

bool DecisionTreeOptimizer::validate_decision_tree(std::shared_ptr<DecisionNode> node) {
    if (!node) return false;

    // Check that decision nodes have children
    if (node->type == DecisionNodeType::DECISION && node->children.empty()) {
        return false;
    }

    // Check that chance nodes have probabilities
    if (node->type == DecisionNodeType::CHANCE && node->probabilities.empty()) {
        return false;
    }

    // Recursively validate children
    for (const auto& child : node->children) {
        if (!validate_decision_tree(child)) {
            return false;
        }
    }

    return true;
}

// AI Integration Methods

std::vector<DecisionAlternative> DecisionTreeOptimizer::generate_ai_alternatives(
    const std::string& decision_problem, int max_alternatives) {

    std::vector<DecisionAlternative> alternatives;

    if (!openai_client_ && !anthropic_client_) return alternatives;

    try {
        std::string prompt = "Generate " + std::to_string(max_alternatives) +
                           " decision alternatives for the following problem:\n\n" +
                           decision_problem + "\n\n" +
                           "For each alternative, provide:\n" +
                           "- Name\n" +
                           "- Description\n" +
                           "- Key advantages\n" +
                           "- Potential risks\n\n" +
                           "Format as JSON array.";

        std::optional<std::string> response;
        if (anthropic_client_) {
            response = anthropic_client_->advanced_reasoning_analysis(
                "Generate decision alternatives for: " + decision_problem,
                "Provide " + std::to_string(max_alternatives) + " practical alternatives");
        } else if (openai_client_) {
            OpenAICompletionRequest req = create_simple_completion(prompt);
            auto openai_response = openai_client_->create_chat_completion(req);
            if (openai_response && !openai_response->choices.empty()) {
                response = openai_response->choices[0].message.content;
            }
        }

        if (response) {
            // Parse AI response to create alternatives (simplified)
            for (int i = 0; i < max_alternatives; ++i) {
                DecisionAlternative alt = create_decision_alternative(
                    "AI-generated alternative " + std::to_string(i + 1));
                alt.name = "Alternative " + std::to_string(i + 1);
                alternatives.push_back(alt);
            }
        }

    } catch (const std::exception& e) {
        logger_->error("AI alternative generation failed: {}", e.what());
    }

    return alternatives;
}

std::unordered_map<std::string, double> DecisionTreeOptimizer::score_alternatives_ai(
    const std::vector<DecisionAlternative>& alternatives, const std::string& /*decision_context*/) {

    std::unordered_map<std::string, double> scores;

    if (alternatives.empty()) return scores;

    // Simplified AI scoring - in production would use sophisticated prompts
    for (const auto& alt : alternatives) {
        scores[alt.id] = weighted_sum_model({alt}).at(alt.id);
    }

    return scores;
}

std::optional<nlohmann::json> DecisionTreeOptimizer::perform_ai_decision_analysis(
    const std::string& decision_problem,
    const std::vector<DecisionAlternative>& alternatives,
    const std::string& context) {

    if (!openai_client_ && !anthropic_client_) return std::nullopt;

    try {
        std::string prompt = std::string("Analyze the following decision problem and provide recommendations:\n\n") +
                             "Decision Problem: " + decision_problem + "\n\n";

        if (!context.empty()) {
            prompt += "Context: " + context + "\n\n";
        }

        prompt += "Available Alternatives:\n";
        for (size_t i = 0; i < alternatives.size(); ++i) {
            prompt += std::to_string(i + 1) + ". " + alternatives[i].name +
                     " - " + alternatives[i].description + "\n";
        }

        prompt += "\nProvide analysis in JSON format with: recommendation, reasoning, confidence_score";

        std::optional<std::string> response;
        if (anthropic_client_) {
            response = anthropic_client_->advanced_reasoning_analysis(
                "Decision analysis for: " + decision_problem, context);
        } else if (openai_client_) {
            OpenAICompletionRequest req = create_simple_completion(prompt);
            auto openai_response = openai_client_->create_chat_completion(req);
            if (openai_response && !openai_response->choices.empty()) {
                response = openai_response->choices[0].message.content;
            }
        }

        if (response) {
            return nlohmann::json{
                {"ai_recommendation", *response},
                {"analysis_type", "decision_analysis"},
                {"confidence", 0.8}
            };
        }

    } catch (const std::exception& e) {
        logger_->error("AI decision analysis failed: {}", e.what());
    }

    return std::nullopt;
}

// Utility Methods

void DecisionTreeOptimizer::normalize_criteria_scores(std::vector<DecisionAlternative>& alternatives) {
    if (alternatives.empty()) return;

    // Get all criteria
    std::unordered_set<DecisionCriterion> all_criteria;
    for (const auto& alt : alternatives) {
        for (const auto& [criterion, _] : alt.criteria_scores) {
            all_criteria.insert(criterion);
        }
    }

    // Normalize each criterion across alternatives
    for (auto criterion : all_criteria) {
        std::vector<double> values;
        for (const auto& alt : alternatives) {
            auto it = alt.criteria_scores.find(criterion);
            if (it != alt.criteria_scores.end()) {
                values.push_back(it->second);
            }
        }

        if (values.empty()) continue;

        // Min-max normalization
        double min_val = *std::min_element(values.begin(), values.end());
        double max_val = *std::max_element(values.begin(), values.end());

        if (max_val != min_val) {
            for (auto& alt : alternatives) {
                auto it = alt.criteria_scores.find(criterion);
                if (it != alt.criteria_scores.end()) {
                    it->second = (it->second - min_val) / (max_val - min_val);
                }
            }
        }
    }
}

std::unordered_map<DecisionCriterion, double> DecisionTreeOptimizer::calculate_default_weights() {
    // Equal weights by default
    return {
        {DecisionCriterion::FINANCIAL_IMPACT, 1.0},
        {DecisionCriterion::REGULATORY_COMPLIANCE, 1.0},
        {DecisionCriterion::RISK_LEVEL, 1.0},
        {DecisionCriterion::OPERATIONAL_IMPACT, 1.0},
        {DecisionCriterion::STRATEGIC_ALIGNMENT, 1.0},
        {DecisionCriterion::ETHICAL_CONSIDERATIONS, 1.0},
        {DecisionCriterion::LEGAL_RISK, 1.0},
        {DecisionCriterion::REPUTATIONAL_IMPACT, 1.0},
        {DecisionCriterion::TIME_TO_IMPLEMENT, 1.0},
        {DecisionCriterion::RESOURCE_REQUIREMENTS, 1.0},
        {DecisionCriterion::STAKEHOLDER_IMPACT, 1.0},
        {DecisionCriterion::MARKET_POSITION, 1.0}
    };
}

std::vector<std::string> DecisionTreeOptimizer::rank_alternatives(
    const std::unordered_map<std::string, double>& scores) {

    std::vector<std::pair<std::string, double>> score_pairs(scores.begin(), scores.end());

    // Sort by score descending
    std::sort(score_pairs.begin(), score_pairs.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    std::vector<std::string> ranking;
    for (const auto& [id, _] : score_pairs) {
        ranking.push_back(id);
    }

    return ranking;
}

std::string DecisionTreeOptimizer::generate_analysis_id() const {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);

    return "decision_" + std::to_string(ms) + "_" + std::to_string(dis(gen));
}

void DecisionTreeOptimizer::load_configuration() {
    // Load configuration from environment variables
    config_.enable_ai_analysis = config_manager_->get_bool("DECISION_ENABLE_AI_ANALYSIS")
                                .value_or(true);
    config_.enable_risk_integration = config_manager_->get_bool("DECISION_ENABLE_RISK_INTEGRATION")
                                     .value_or(true);
    config_.risk_weight = config_manager_->get_double("DECISION_RISK_WEIGHT")
                         .value_or(0.3);
    config_.ai_confidence_threshold = config_manager_->get_double("DECISION_AI_CONFIDENCE_THRESHOLD")
                                     .value_or(0.7);
    config_.max_tree_depth = static_cast<int>(config_manager_->get_int("DECISION_MAX_TREE_DEPTH")
                                             .value_or(10));
    config_.max_alternatives = static_cast<int>(config_manager_->get_int("DECISION_MAX_ALTERNATIVES")
                                               .value_or(20));
    config_.enable_sensitivity_analysis = config_manager_->get_bool("DECISION_ENABLE_SENSITIVITY_ANALYSIS")
                                         .value_or(true);

    std::string method_str = config_manager_->get_string("DECISION_DEFAULT_METHOD")
                            .value_or("WEIGHTED_SUM");
    if (method_str == "WEIGHTED_PRODUCT") config_.default_method = MCDAMethod::WEIGHTED_PRODUCT;
    else if (method_str == "TOPSIS") config_.default_method = MCDAMethod::TOPSIS;
    else if (method_str == "ELECTRE") config_.default_method = MCDAMethod::ELECTRE;
    else if (method_str == "PROMETHEE") config_.default_method = MCDAMethod::PROMETHEE;
    else if (method_str == "AHP") config_.default_method = MCDAMethod::AHP;
    else if (method_str == "VIKOR") config_.default_method = MCDAMethod::VIKOR;
    else config_.default_method = MCDAMethod::WEIGHTED_SUM;

    config_.ai_model = config_manager_->get_string("DECISION_AI_MODEL")
                      .value_or("decision_analysis");
}

bool DecisionTreeOptimizer::validate_decision_input(
    const std::string& decision_problem,
    const std::vector<DecisionAlternative>& alternatives) const {

    if (decision_problem.empty()) return false;
    if (alternatives.empty()) return false;
    if (alternatives.size() > static_cast<size_t>(config_.max_alternatives)) return false;

    // Check that alternatives have criteria scores
    for (const auto& alt : alternatives) {
        if (alt.criteria_scores.empty()) return false;
        if (alt.id.empty()) return false;
    }

    return true;
}

} // namespace regulens
