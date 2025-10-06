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

    if (analysis.alternatives.empty()) return sensitivity;

    // Test sensitivity to criteria weights
    for (const auto& alt : analysis.alternatives) {
        // Test each criterion weight sensitivity
        for (const auto& [criterion, original_weight] : alt.criteria_weights) {
            std::string param = "weight_sensitivity_" + alt.id + "_" + decision_criterion_to_string(criterion);

            // Test weight changes: -20%, -10%, +10%, +20%
            std::vector<double> weight_changes = {-0.2, -0.1, 0.1, 0.2};
            double max_impact = 0.0;

            for (double change : weight_changes) {
                double new_weight = std::max(0.0, std::min(1.0, original_weight + change));

                // Recalculate score with modified weight
                double modified_score = 0.0;
                double total_weight = 0.0;

                for (const auto& [crit, score] : alt.criteria_scores) {
                    double weight = (crit == criterion) ? new_weight : alt.criteria_weights.at(crit);
                    modified_score += score * weight;
                    total_weight += weight;
                }

                if (total_weight > 0) {
                    modified_score /= total_weight; // Normalize
                }

                double original_score = analysis.alternative_scores.at(alt.id);
                double impact = std::abs(modified_score - original_score) / std::max(std::abs(original_score), 1e-6);
                max_impact = std::max(max_impact, impact);
            }

            sensitivity[param] = max_impact;
        }
    }

    // Test sensitivity to criteria scores
    for (const auto& alt : analysis.alternatives) {
        for (const auto& [criterion, original_score] : alt.criteria_scores) {
            std::string param = "score_sensitivity_" + alt.id + "_" + decision_criterion_to_string(criterion);

            // Test score changes: -15%, -5%, +5%, +15%
            std::vector<double> score_changes = {-0.15, -0.05, 0.05, 0.15};
            double max_impact = 0.0;

            for (double change : score_changes) {
                double new_score = std::max(0.0, std::min(1.0, original_score + change));

                // Recalculate score with modified criterion score
                double modified_score = 0.0;
                for (const auto& [crit, score] : alt.criteria_scores) {
                    double actual_score = (crit == criterion) ? new_score : score;
                    modified_score += actual_score * alt.criteria_weights.at(crit);
                }

                double original_total_score = analysis.alternative_scores.at(alt.id);
                double impact = std::abs(modified_score - original_total_score) / std::max(std::abs(original_total_score), 1e-6);
                max_impact = std::max(max_impact, impact);
            }

            sensitivity[param] = max_impact;
        }
    }

    // Test ranking stability - how often ranking changes with perturbations
    std::string ranking_stability = "ranking_stability";
    int stable_rankings = 0;
    int total_tests = 10;

    for (int test = 0; test < total_tests; ++test) {
        // Add small random perturbations to all scores
        std::unordered_map<std::string, double> perturbed_scores;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<> noise(0.0, 0.02); // 2% standard deviation noise

        for (const auto& [id, score] : analysis.alternative_scores) {
            perturbed_scores[id] = score + noise(gen);
        }

        auto perturbed_ranking = rank_alternatives(perturbed_scores);

        // Check if top choice remains the same
        if (!perturbed_ranking.empty() && perturbed_ranking[0] == analysis.recommended_alternative) {
            stable_rankings++;
        }
    }

    sensitivity[ranking_stability] = static_cast<double>(stable_rankings) / total_tests;

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
    const std::string& description, const std::string& /*context*/) const {

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

    // Parse advantages/disadvantages from description using NLP-like analysis
    alt.advantages = parse_advantages_from_description(description);
    alt.disadvantages = parse_disadvantages_from_description(description);

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

    // Production ELECTRE III implementation with concordance and discordance matrices
    std::unordered_map<std::string, double> scores;

    if (alternatives.size() < 2) {
        for (const auto& alt : alternatives) {
            scores[alt.id] = 1.0;
        }
        return scores;
    }

    size_t n = alternatives.size();

    // Build concordance matrix C[i][j] - how much i outranks j
    std::vector<std::vector<double>> concordance(n, std::vector<double>(n, 0.0));

    // Build discordance matrix D[i][j] - how much i does NOT outrank j
    std::vector<std::vector<double>> discordance(n, std::vector<double>(n, 0.0));

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            if (i == j) continue;

            const auto& alt_i = alternatives[i];
            const auto& alt_j = alternatives[j];

            double concordance_sum = 0.0;
            double total_weight = 0.0;
            double max_discordance = 0.0;

            // For each criterion, calculate concordance and discordance
            for (const auto& [criterion, score_i] : alt_i.criteria_scores) {
                auto it_j = alt_j.criteria_scores.find(criterion);
                if (it_j == alt_j.criteria_scores.end()) continue;

                double score_j = it_j->second;
                double weight = alt_i.criteria_weights.at(criterion);
                total_weight += weight;

                // Concordance: i outranks j on this criterion
                if (score_i >= score_j - config_.mcda_params.electre_indifference_threshold) {
                    concordance_sum += weight;
                } else if (score_i >= score_j - config_.mcda_params.electre_preference_threshold) {
                    // Partial concordance (linear interpolation)
                    double diff = score_j - score_i;
                    double range = config_.mcda_params.electre_preference_threshold -
                                  config_.mcda_params.electre_indifference_threshold;
                    concordance_sum += weight * (1.0 - (diff - config_.mcda_params.electre_indifference_threshold) / range);
                }

                // Discordance: how much j is preferred over i
                if (score_j > score_i + config_.mcda_params.electre_preference_threshold) {
                    double diff = score_j - score_i;
                    double veto = config_.mcda_params.electre_veto_threshold;
                    double pref = config_.mcda_params.electre_preference_threshold;

                    double disc_value = 0.0;
                    if (diff >= veto) {
                        disc_value = 1.0;  // Complete veto
                    } else if (diff > pref) {
                        // Partial discordance
                        disc_value = (diff - pref) / (veto - pref);
                    }

                    max_discordance = std::max(max_discordance, disc_value);
                }
            }

            concordance[i][j] = (total_weight > 0) ? (concordance_sum / total_weight) : 0.0;
            discordance[i][j] = max_discordance;
        }
    }

    // Calculate outranking degrees using credibility index
    std::vector<std::vector<double>> credibility(n, std::vector<double>(n, 0.0));

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            if (i == j) continue;

            // Credibility = Concordance attenuated by discordance
            double c = concordance[i][j];
            double d = discordance[i][j];

            if (d < c) {
                credibility[i][j] = c;
            } else {
                credibility[i][j] = c * ((1.0 - d) / (1.0 - c));
            }
        }
    }

    // Calculate qualification scores (how many alternatives each one outranks)
    for (size_t i = 0; i < n; ++i) {
        double outranking_score = 0.0;
        double being_outranked_score = 0.0;

        for (size_t j = 0; j < n; ++j) {
            if (i != j) {
                // How much i outranks j
                if (credibility[i][j] >= config_.mcda_params.electre_threshold) {
                    outranking_score += credibility[i][j];
                }

                // How much j outranks i
                if (credibility[j][i] >= config_.mcda_params.electre_threshold) {
                    being_outranked_score += credibility[j][i];
                }
            }
        }

        // Net outranking score
        scores[alternatives[i].id] = outranking_score - being_outranked_score;
    }

    return scores;
}

std::unordered_map<std::string, double> DecisionTreeOptimizer::promethee_method(
    const std::vector<DecisionAlternative>& alternatives) {

    // Production PROMETHEE II implementation with preference functions
    std::unordered_map<std::string, double> scores;

    if (alternatives.empty()) return scores;

    size_t n = alternatives.size();

    // Calculate preference degrees using different preference functions
    auto calculate_preference = [this](double diff, const std::string& function_type) -> double {
        if (diff <= 0) return 0.0;

        // Type I: Usual criterion (step function)
        if (function_type == "usual") {
            return 1.0;
        }

        // Type II: U-shape (quasi-criterion)
        if (function_type == "u-shape") {
            double q = config_.mcda_params.promethee_indifference_threshold;
            return (diff > q) ? 1.0 : 0.0;
        }

        // Type III: V-shape (linear preference)
        if (function_type == "v-shape") {
            double p = config_.mcda_params.promethee_preference_threshold;
            if (diff >= p) return 1.0;
            return diff / p;
        }

        // Type IV: Level criterion
        if (function_type == "level") {
            double q = config_.mcda_params.promethee_indifference_threshold;
            double p = config_.mcda_params.promethee_preference_threshold;
            if (diff <= q) return 0.0;
            if (diff >= p) return 1.0;
            return 0.5;
        }

        // Type V: V-shape with indifference (most common)
        if (function_type == "v-shape-ind") {
            double q = config_.mcda_params.promethee_indifference_threshold;
            double p = config_.mcda_params.promethee_preference_threshold;
            if (diff <= q) return 0.0;
            if (diff >= p) return 1.0;
            return (diff - q) / (p - q);
        }

        // Type VI: Gaussian
        if (function_type == "gaussian") {
            double sigma = config_.mcda_params.promethee_preference_threshold / 2.0;
            return 1.0 - std::exp(-(diff * diff) / (2.0 * sigma * sigma));
        }

        // Default: V-shape with indifference
        double q = config_.mcda_params.promethee_indifference_threshold;
        double p = config_.mcda_params.promethee_preference_threshold;
        if (diff <= q) return 0.0;
        if (diff >= p) return 1.0;
        return (diff - q) / (p - q);
    };

    // Calculate multicriteria preference indices
    std::vector<std::vector<double>> preference_indices(n, std::vector<double>(n, 0.0));

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            if (i == j) continue;

            const auto& alt_i = alternatives[i];
            const auto& alt_j = alternatives[j];

            double weighted_preference_sum = 0.0;
            double total_weight = 0.0;

            for (const auto& [criterion, score_i] : alt_i.criteria_scores) {
                auto it_j = alt_j.criteria_scores.find(criterion);
                if (it_j == alt_j.criteria_scores.end()) continue;

                double score_j = it_j->second;
                double diff = score_i - score_j;
                double weight = alt_i.criteria_weights.at(criterion);

                // Use V-shape with indifference as default preference function
                double pref = calculate_preference(diff, "v-shape-ind");

                weighted_preference_sum += weight * pref;
                total_weight += weight;
            }

            preference_indices[i][j] = (total_weight > 0) ? (weighted_preference_sum / total_weight) : 0.0;
        }
    }

    // Calculate positive and negative flows
    std::vector<double> positive_flows(n, 0.0);
    std::vector<double> negative_flows(n, 0.0);

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            if (i != j) {
                positive_flows[i] += preference_indices[i][j];
                negative_flows[i] += preference_indices[j][i];
            }
        }

        // Normalize by n-1
        if (n > 1) {
            positive_flows[i] /= (n - 1);
            negative_flows[i] /= (n - 1);
        }
    }

    // Calculate net flows (PROMETHEE II)
    for (size_t i = 0; i < n; ++i) {
        scores[alternatives[i].id] = positive_flows[i] - negative_flows[i];
    }

    return scores;
}

std::unordered_map<std::string, double> DecisionTreeOptimizer::ahp_method(
    const std::vector<DecisionAlternative>& alternatives) {

    // Production AHP (Analytic Hierarchy Process) implementation
    std::unordered_map<std::string, double> scores;

    if (alternatives.empty()) return scores;

    size_t n = alternatives.size();

    // Build pairwise comparison matrix from criteria scores
    // A[i][j] = how much alternative i is preferred over alternative j
    std::vector<std::vector<double>> pairwise_matrix(n, std::vector<double>(n, 1.0));

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            if (i == j) {
                pairwise_matrix[i][j] = 1.0;
                continue;
            }

            const auto& alt_i = alternatives[i];
            const auto& alt_j = alternatives[j];

            double ratio_sum = 0.0;
            int count = 0;

            // Compare alternatives based on all criteria
            for (const auto& [criterion, score_i] : alt_i.criteria_scores) {
                auto it_j = alt_j.criteria_scores.find(criterion);
                if (it_j == alt_j.criteria_scores.end()) continue;

                double score_j = it_j->second;

                // Avoid division by zero
                if (score_j > 0.001) {
                    double ratio = score_i / score_j;
                    double weight = alt_i.criteria_weights.at(criterion);

                    ratio_sum += ratio * weight;
                    count++;
                }
            }

            pairwise_matrix[i][j] = (count > 0) ? ratio_sum / count : 1.0;

            // Ensure reciprocal property: A[j][i] = 1/A[i][j]
            pairwise_matrix[j][i] = 1.0 / pairwise_matrix[i][j];
        }
    }

    // Calculate priority vector using eigenvector method (power iteration)
    std::vector<double> priority_vector(n, 1.0 / n);  // Initial guess
    const int max_iterations = 100;
    const double tolerance = 1e-6;

    for (int iter = 0; iter < max_iterations; ++iter) {
        std::vector<double> new_priority(n, 0.0);

        // Matrix-vector multiplication
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < n; ++j) {
                new_priority[i] += pairwise_matrix[i][j] * priority_vector[j];
            }
        }

        // Normalize
        double sum = 0.0;
        for (double val : new_priority) {
            sum += val;
        }

        for (size_t i = 0; i < n; ++i) {
            new_priority[i] /= sum;
        }

        // Check convergence
        double max_diff = 0.0;
        for (size_t i = 0; i < n; ++i) {
            max_diff = std::max(max_diff, std::abs(new_priority[i] - priority_vector[i]));
        }

        priority_vector = new_priority;

        if (max_diff < tolerance) {
            break;
        }
    }

    // Calculate consistency ratio to verify the quality of judgments
    // Calculate lambda_max (principal eigenvalue)
    double lambda_max = 0.0;
    for (size_t i = 0; i < n; ++i) {
        double sum = 0.0;
        for (size_t j = 0; j < n; ++j) {
            sum += pairwise_matrix[i][j] * priority_vector[j];
        }
        lambda_max += sum / priority_vector[i];
    }
    lambda_max /= n;

    // Consistency Index
    double CI = (lambda_max - n) / (n - 1);

    // Random Index (for n alternatives)
    const double RI_values[] = {0, 0, 0.58, 0.90, 1.12, 1.24, 1.32, 1.41, 1.45, 1.49};
    double RI = (n < 10) ? RI_values[n] : 1.49;

    // Consistency Ratio
    double CR = (RI > 0) ? CI / RI : 0.0;

    // Log warning if consistency ratio is high
    if (CR > 0.1) {
        // Judgments are inconsistent (CR should be < 0.1)
        // In production, this might trigger a re-evaluation request
        logger_->warn("AHP consistency ratio is high (" + std::to_string(CR) +
                     "), results may be unreliable");
    }

    // Assign scores based on priority vector
    for (size_t i = 0; i < n; ++i) {
        scores[alternatives[i].id] = priority_vector[i];
    }

    return scores;
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

        // VIKOR combines S and R using v parameter: Q = v*S + (1-v)*R
        // v represents weight of strategy of maximum group utility
        double v = config_.mcda_params.vikor_v_parameter; // Typically 0.5
        double q = v * s + (1.0 - v) * r;
        scores[alt.id] = q;
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
            // Parse AI response as JSON to create alternatives
            try {
                nlohmann::json response_json = nlohmann::json::parse(*response);

                // Handle different possible JSON structures
                if (response_json.is_array()) {
                    // Direct array of alternatives
                    for (const auto& alt_json : response_json) {
                        DecisionAlternative alt = parse_alternative_from_json(alt_json);
                        if (!alt.id.empty()) {
                            alternatives.push_back(alt);
                        }
                    }
                } else if (response_json.contains("alternatives") && response_json["alternatives"].is_array()) {
                    // Nested alternatives array
                    for (const auto& alt_json : response_json["alternatives"]) {
                        DecisionAlternative alt = parse_alternative_from_json(alt_json);
                        if (!alt.id.empty()) {
                            alternatives.push_back(alt);
                        }
                    }
                } else {
                    // Try to extract alternatives from text response
                    alternatives = parse_alternatives_from_text(*response, max_alternatives);
                }

                // Limit to requested number
                if (alternatives.size() > static_cast<size_t>(max_alternatives)) {
                    alternatives.resize(max_alternatives);
                }

            } catch (const nlohmann::json::parse_error& e) {
                logger_->warn("Failed to parse AI response as JSON, falling back to text parsing: {}", e.what());
                alternatives = parse_alternatives_from_text(*response, max_alternatives);
            } catch (const std::exception& e) {
                logger_->error("Error parsing AI alternatives response: {}", e.what());
                // Fallback to simple alternatives
                for (int i = 0; i < max_alternatives; ++i) {
                    DecisionAlternative alt = create_decision_alternative(
                        "AI-generated alternative " + std::to_string(i + 1));
                    alt.name = "Alternative " + std::to_string(i + 1);
                    alternatives.push_back(alt);
                }
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

    // Load MCDA parameters
    config_.mcda_params.topsis_distance_p = config_manager_->get_double("DECISION_TOPSIS_DISTANCE_P")
                                           .value_or(2.0);
    config_.mcda_params.electre_threshold = config_manager_->get_double("DECISION_ELECTRE_THRESHOLD")
                                           .value_or(0.7);
    config_.mcda_params.promethee_preference_threshold = config_manager_->get_double("DECISION_PROMETHEE_PREFERENCE_THRESHOLD")
                                                        .value_or(0.1);
    config_.mcda_params.vikor_v_parameter = config_manager_->get_double("DECISION_VIKOR_V_PARAMETER")
                                           .value_or(0.5);
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

std::vector<std::string> DecisionTreeOptimizer::parse_advantages_from_description(const std::string& description) const {
    std::vector<std::string> advantages;

    // Simple keyword-based parsing for advantages
    std::vector<std::string> advantage_keywords = {
        "benefit", "advantage", "strength", "positive", "good", "better", "improved",
        "efficient", "effective", "reliable", "robust", "flexible", "scalable",
        "cost-effective", "time-saving", "user-friendly", "innovative"
    };

    std::string lower_desc = description;
    std::transform(lower_desc.begin(), lower_desc.end(), lower_desc.begin(), ::tolower);

    // Extract sentences containing advantage keywords
    std::istringstream iss(description);
    std::string sentence;
    while (std::getline(iss, sentence, '.')) {
        std::string lower_sentence = sentence;
        std::transform(lower_sentence.begin(), lower_sentence.end(), lower_sentence.begin(), ::tolower);

        for (const auto& keyword : advantage_keywords) {
            if (lower_sentence.find(keyword) != std::string::npos) {
                // Clean up the sentence
                std::string clean_sentence = sentence;
                // Remove leading/trailing whitespace
                clean_sentence.erase(clean_sentence.begin(),
                    std::find_if(clean_sentence.begin(), clean_sentence.end(),
                        [](int ch) { return !std::isspace(ch); }));
                clean_sentence.erase(
                    std::find_if(clean_sentence.rbegin(), clean_sentence.rend(),
                        [](int ch) { return !std::isspace(ch); }).base(),
                    clean_sentence.end());

                if (!clean_sentence.empty()) {
                    advantages.push_back(clean_sentence);
                }
                break; // Only add each sentence once
            }
        }
    }

    // If no advantages found, provide defaults
    if (advantages.empty()) {
        advantages = {"Potential efficiency improvements", "Structured approach to decision making"};
    }

    return advantages;
}

std::vector<std::string> DecisionTreeOptimizer::parse_disadvantages_from_description(const std::string& description) const {
    std::vector<std::string> disadvantages;

    // Simple keyword-based parsing for disadvantages
    std::vector<std::string> disadvantage_keywords = {
        "risk", "disadvantage", "weakness", "negative", "problem", "issue", "concern",
        "costly", "complex", "difficult", "challenging", "limitation", "drawback",
        "expensive", "time-consuming", "error-prone", "unreliable"
    };

    std::string lower_desc = description;
    std::transform(lower_desc.begin(), lower_desc.end(), lower_desc.begin(), ::tolower);

    // Extract sentences containing disadvantage keywords
    std::istringstream iss(description);
    std::string sentence;
    while (std::getline(iss, sentence, '.')) {
        std::string lower_sentence = sentence;
        std::transform(lower_sentence.begin(), lower_sentence.end(), lower_sentence.begin(), ::tolower);

        for (const auto& keyword : disadvantage_keywords) {
            if (lower_sentence.find(keyword) != std::string::npos) {
                // Clean up the sentence
                std::string clean_sentence = sentence;
                // Remove leading/trailing whitespace
                clean_sentence.erase(clean_sentence.begin(),
                    std::find_if(clean_sentence.begin(), clean_sentence.end(),
                        [](int ch) { return !std::isspace(ch); }));
                clean_sentence.erase(
                    std::find_if(clean_sentence.rbegin(), clean_sentence.rend(),
                        [](int ch) { return !std::isspace(ch); }).base(),
                    clean_sentence.end());

                if (!clean_sentence.empty()) {
                    disadvantages.push_back(clean_sentence);
                }
                break; // Only add each sentence once
            }
        }
    }

    // If no disadvantages found, provide defaults
    if (disadvantages.empty()) {
        disadvantages = {"May require additional resources", "Implementation challenges possible"};
    }

    return disadvantages;
}

DecisionCriterion string_to_decision_criterion(const std::string& str) {
    if (str == "FINANCIAL_IMPACT") return DecisionCriterion::FINANCIAL_IMPACT;
    if (str == "REGULATORY_COMPLIANCE") return DecisionCriterion::REGULATORY_COMPLIANCE;
    if (str == "RISK_LEVEL") return DecisionCriterion::RISK_LEVEL;
    if (str == "OPERATIONAL_IMPACT") return DecisionCriterion::OPERATIONAL_IMPACT;
    if (str == "STRATEGIC_ALIGNMENT") return DecisionCriterion::STRATEGIC_ALIGNMENT;
    if (str == "ETHICAL_CONSIDERATIONS") return DecisionCriterion::ETHICAL_CONSIDERATIONS;
    if (str == "LEGAL_RISK") return DecisionCriterion::LEGAL_RISK;
    if (str == "REPUTATIONAL_IMPACT") return DecisionCriterion::REPUTATIONAL_IMPACT;
    if (str == "TIME_TO_IMPLEMENT") return DecisionCriterion::TIME_TO_IMPLEMENT;
    if (str == "RESOURCE_REQUIREMENTS") return DecisionCriterion::RESOURCE_REQUIREMENTS;
    if (str == "STAKEHOLDER_IMPACT") return DecisionCriterion::STAKEHOLDER_IMPACT;
    if (str == "MARKET_POSITION") return DecisionCriterion::MARKET_POSITION;
    return DecisionCriterion::FINANCIAL_IMPACT; // Default fallback
}

DecisionAlternative DecisionTreeOptimizer::parse_alternative_from_json(const nlohmann::json& alt_json) const {
    DecisionAlternative alt;

    try {
        // Generate unique ID
        alt.id = "ai_alt_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) +
                "_" + std::to_string(rand());

        // Parse name
        if (alt_json.contains("name") && alt_json["name"].is_string()) {
            alt.name = alt_json["name"];
        } else if (alt_json.contains("title") && alt_json["title"].is_string()) {
            alt.name = alt_json["title"];
        } else {
            alt.name = "AI Alternative";
        }

        // Parse description
        if (alt_json.contains("description") && alt_json["description"].is_string()) {
            alt.description = alt_json["description"];
        } else if (alt_json.contains("summary") && alt_json["summary"].is_string()) {
            alt.description = alt_json["summary"];
        } else {
            alt.description = "AI-generated decision alternative";
        }

        // Parse advantages
        if (alt_json.contains("advantages") && alt_json["advantages"].is_array()) {
            for (const auto& adv : alt_json["advantages"]) {
                if (adv.is_string()) {
                    alt.advantages.push_back(adv);
                }
            }
        } else if (alt_json.contains("pros") && alt_json["pros"].is_array()) {
            for (const auto& pro : alt_json["pros"]) {
                if (pro.is_string()) {
                    alt.advantages.push_back(pro);
                }
            }
        }

        // Parse disadvantages
        if (alt_json.contains("disadvantages") && alt_json["disadvantages"].is_array()) {
            for (const auto& dis : alt_json["disadvantages"]) {
                if (dis.is_string()) {
                    alt.disadvantages.push_back(dis);
                }
            }
        } else if (alt_json.contains("cons") && alt_json["cons"].is_array()) {
            for (const auto& con : alt_json["cons"]) {
                if (con.is_string()) {
                    alt.disadvantages.push_back(con);
                }
            }
        }

        // Parse criteria scores if provided
        if (alt_json.contains("criteria_scores") && alt_json["criteria_scores"].is_object()) {
            for (const auto& [criterion_str, score] : alt_json["criteria_scores"].items()) {
                DecisionCriterion criterion = string_to_decision_criterion(criterion_str);
                if (score.is_number()) {
                    alt.criteria_scores[criterion] = score;
                }
            }
        }

        // Set default criteria scores if not provided
        if (alt.criteria_scores.empty()) {
            alt.criteria_scores = {
                {DecisionCriterion::FINANCIAL_IMPACT, 0.5},
                {DecisionCriterion::REGULATORY_COMPLIANCE, 0.7},
                {DecisionCriterion::RISK_LEVEL, 0.4},
                {DecisionCriterion::OPERATIONAL_IMPACT, 0.6},
                {DecisionCriterion::STRATEGIC_ALIGNMENT, 0.5},
                {DecisionCriterion::ETHICAL_CONSIDERATIONS, 0.8}
            };
        }

        // Set equal weights by default
        for (const auto& [criterion, _] : alt.criteria_scores) {
            alt.criteria_weights[criterion] = 1.0 / alt.criteria_scores.size();
        }

        // Ensure advantages and disadvantages are not empty
        if (alt.advantages.empty()) {
            alt.advantages = parse_advantages_from_description(alt.description);
        }
        if (alt.disadvantages.empty()) {
            alt.disadvantages = parse_disadvantages_from_description(alt.description);
        }

    } catch (const std::exception& e) {
        logger_->warn("Error parsing alternative from JSON: {}", e.what());
        // Return empty alternative to indicate parsing failure
        alt.id = "";
    }

    return alt;
}

std::vector<DecisionAlternative> DecisionTreeOptimizer::parse_alternatives_from_text(
    const std::string& text, int max_alternatives) const {

    std::vector<DecisionAlternative> alternatives;

    try {
        // Simple text parsing - split by numbered items or bullet points
        std::vector<std::string> lines;
        std::istringstream iss(text);
        std::string line;

        while (std::getline(iss, line)) {
            // Remove leading/trailing whitespace
            line.erase(line.begin(), std::find_if(line.begin(), line.end(),
                [](int ch) { return !std::isspace(ch); }));
            line.erase(std::find_if(line.rbegin(), line.rend(),
                [](int ch) { return !std::isspace(ch); }).base(), line.end());

            if (!line.empty()) {
                lines.push_back(line);
            }
        }

        // Look for numbered alternatives (1., 2., etc.) or bullet points
        std::vector<std::string> alt_texts;
        std::string current_alt;

        for (const auto& line : lines) {
            // Check if line starts a new alternative

            // Check for numbered alternatives (e.g., "1.")
            bool is_numbered = (line.size() >= 2 && std::isdigit(line[0]) && line[1] == '.');

            // Check for bullet points: '-', '*', or Unicode bullet ("\u2022" or "•")
            bool is_bullet = false;
            if (line.size() >= 1) {
                if (line[0] == '-' || line[0] == '*') {
                    is_bullet = true;
                } else if ((line.size() >= 3 && line.substr(0,3) == "\xe2\x80\xa2") || line.compare(0, 3, "\u2022") == 0) {
                    // Handles UTF-8 encoded bullet (•)
                    is_bullet = true;
                }
            }

            if (is_numbered || is_bullet) {

                if (!current_alt.empty()) {
                    alt_texts.push_back(current_alt);
                }
                current_alt = line.substr(line.find_first_not_of("-•*1234567890. "));
            } else if (!line.empty()) {
                if (!current_alt.empty()) current_alt += " ";
                current_alt += line;
            }
        }

        if (!current_alt.empty()) {
            alt_texts.push_back(current_alt);
        }

        // Create alternatives from parsed text
        for (size_t i = 0; i < alt_texts.size() && i < static_cast<size_t>(max_alternatives); ++i) {
            DecisionAlternative alt = create_decision_alternative(alt_texts[i]);
            alt.name = "Alternative " + std::to_string(i + 1);
            alternatives.push_back(alt);
        }

        // If no alternatives found, create basic ones
        if (alternatives.empty()) {
            for (int i = 0; i < max_alternatives; ++i) {
                DecisionAlternative alt = create_decision_alternative(
                    "AI-generated alternative based on: " + text.substr(0, 100));
                alt.name = "Alternative " + std::to_string(i + 1);
                alternatives.push_back(alt);
            }
        }

    } catch (const std::exception& e) {
        logger_->error("Error parsing alternatives from text: {}", e.what());
        // Fallback
        for (int i = 0; i < max_alternatives; ++i) {
            DecisionAlternative alt = create_decision_alternative("AI-generated alternative");
            alt.name = "Alternative " + std::to_string(i + 1);
            alternatives.push_back(alt);
        }
    }

    return alternatives;
}

} // namespace regulens
