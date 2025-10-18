/**
 * AsyncMCDADecisionService - Production-Grade Multi-Criteria Decision Analysis
 *
 * Integrates 7 MCDA algorithms with AsyncJobManager for:
 * - AHP (Analytic Hierarchy Process)
 * - TOPSIS (Technique for Order Preference by Similarity to Ideal Solution)
 * - PROMETHEE (Preference Ranking Organization Method for Enrichment Evaluation)
 * - ELECTRE (Elimination and Choice Expressing Reality)
 * - Weighted Sum / Weighted Product models
 * - VIKOR (Vlsekritеrijumska Optimizacija I Kompromisno Resenje)
 *
 * Features:
 * - Async/batch decision analysis
 * - Sensitivity analysis for parameters
 * - Alternative ranking and recommendations
 * - Criterion weighting strategies
 * - Risk assessment integration
 * - Comprehensive audit trails
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <nlohmann/json.hpp>
#include "../async_jobs/async_job_manager.hpp"
#include "../cache/redis_cache_manager.hpp"
#include "../logging/structured_logger.hpp"
#include "../error_handler.hpp"

using json = nlohmann::json;

namespace regulens {
namespace decisions {

/**
 * Decision criterion for MCDA evaluation
 */
struct DecisionCriterion {
    std::string id;
    std::string name;
    std::string type;  // BENEFIT or COST
    double weight;
    double min_value;
    double max_value;
    std::string description;
    bool is_quantitative;
};

/**
 * Decision alternative for evaluation
 */
struct DecisionAlternative {
    std::string id;
    std::string name;
    std::map<std::string, double> scores;  // criterion_id -> score
    json metadata;
    std::string description;
};

/**
 * MCDA algorithm enumeration
 */
enum class MCDAAlgorithm {
    WEIGHTED_SUM,
    WEIGHTED_PRODUCT,
    TOPSIS,
    ELECTRE,
    PROMETHEE,
    AHP,
    VIKOR
};

/**
 * Decision analysis result
 */
struct MCDADecisionResult {
    std::string analysis_id;
    std::string decision_problem;
    MCDAAlgorithm algorithm_used;
    std::vector<DecisionAlternative> alternatives;
    std::vector<std::pair<std::string, double>> ranking;  // alternative_id, score
    std::string recommended_alternative;
    json sensitivity_analysis;
    json detailed_scores;
    double solution_quality_score;
    long execution_time_ms;
    std::chrono::system_clock::time_point analyzed_at;
};

/**
 * Sensitivity analysis configuration
 */
struct SensitivityAnalysisConfig {
    std::string parameter_id;
    std::string analysis_type;  // "weight_variation", "score_variation", "threshold"
    double min_value;
    double max_value;
    int steps;
    std::string criterion_id;  // For criterion weight variations
};

/**
 * AsyncMCDADecisionService orchestrates MCDA analysis
 */
class AsyncMCDADecisionService {
public:
    AsyncMCDADecisionService(
        std::shared_ptr<async_jobs::AsyncJobManager> job_manager,
        std::shared_ptr<cache::RedisCacheManager> cache_manager,
        std::shared_ptr<StructuredLogger> logger,
        std::shared_ptr<ErrorHandler> error_handler
    );

    ~AsyncMCDADecisionService();

    /**
     * Initialize the decision service
     */
    bool initialize();

    /**
     * Analyze decision problem with selected algorithm
     * @param decision_problem Problem description
     * @param criteria Decision criteria
     * @param alternatives Alternatives to evaluate
     * @param algorithm Algorithm to use
     * @param execution_mode SYNC/ASYNC/BATCH/STREAMING
     * @param use_cache Whether to use cache
     * @return Result for sync, job_id for async
     */
    json analyze_decision_async(
        const std::string& decision_problem,
        const std::vector<DecisionCriterion>& criteria,
        const std::vector<DecisionAlternative>& alternatives,
        MCDAAlgorithm algorithm,
        const std::string& execution_mode = "ASYNCHRONOUS",
        bool use_cache = true
    );

    /**
     * Run ensemble decision analysis (multiple algorithms)
     * @param decision_problem Problem description
     * @param criteria Decision criteria
     * @param alternatives Alternatives to evaluate
     * @param algorithms Algorithms to compare
     * @param execution_mode SYNC/ASYNC/BATCH/STREAMING
     * @return Comparative analysis results
     */
    json analyze_decision_ensemble(
        const std::string& decision_problem,
        const std::vector<DecisionCriterion>& criteria,
        const std::vector<DecisionAlternative>& alternatives,
        const std::vector<MCDAAlgorithm>& algorithms,
        const std::string& execution_mode = "ASYNCHRONOUS"
    );

    /**
     * Perform sensitivity analysis on decision
     * @param analysis_id Previous analysis ID
     * @param config Sensitivity analysis configuration
     * @return Sensitivity analysis results
     */
    json perform_sensitivity_analysis(
        const std::string& analysis_id,
        const SensitivityAnalysisConfig& config
    );

    /**
     * Get analysis result
     * @param job_id Job identifier
     * @return MCDADecisionResult
     */
    std::optional<MCDADecisionResult> get_analysis_result(const std::string& job_id);

    /**
     * Get analysis status
     * @param job_id Job identifier
     * @return Current status with progress
     */
    json get_analysis_status(const std::string& job_id);

    /**
     * Cancel ongoing analysis
     * @param job_id Job identifier
     * @return true if cancelled
     */
    bool cancel_analysis(const std::string& job_id);

    /**
     * Get analysis history
     * @param algorithm Optional algorithm filter
     * @param limit Result limit
     * @return Historical analyses
     */
    std::vector<MCDADecisionResult> get_analysis_history(
        const std::string& algorithm = "",
        int limit = 50
    );

    /**
     * Get algorithm performance comparison
     * @param decision_problem Problem description
     * @return Comparison metrics for all algorithms
     */
    json get_algorithm_comparison(const std::string& decision_problem);

    /**
     * Update criterion weights
     * @param analysis_id Analysis identifier
     * @param new_weights New criterion weights
     * @return Updated analysis with new weights
     */
    json update_criterion_weights(
        const std::string& analysis_id,
        const std::map<std::string, double>& new_weights
    );

    /**
     * Get decision recommendations
     * @param analysis_id Analysis identifier
     * @return Ranked recommendations with confidence scores
     */
    json get_decision_recommendations(const std::string& analysis_id);

    /**
     * Validate decision input
     * @param criteria Criteria list
     * @param alternatives Alternatives list
     * @return Validation result with error details
     */
    json validate_decision_input(
        const std::vector<DecisionCriterion>& criteria,
        const std::vector<DecisionAlternative>& alternatives
    );

    /**
     * Get MCDA algorithms list
     * @return Available algorithms with descriptions
     */
    json get_available_algorithms();

    /**
     * Health check for service
     * @return Health status
     */
    json get_health_status();

    /**
     * System metrics
     * @return System metrics
     */
    json get_system_metrics();

private:
    std::shared_ptr<async_jobs::AsyncJobManager> job_manager_;
    std::shared_ptr<cache::RedisCacheManager> cache_manager_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<ErrorHandler> error_handler_;

    std::map<std::string, MCDADecisionResult> analysis_history_;
    std::mutex history_mutex_;

    // Statistics
    std::atomic<size_t> total_analyses_;
    std::atomic<size_t> successful_analyses_;
    std::atomic<size_t> failed_analyses_;

    // Private methods
    std::string generate_analysis_id();
    
    std::string algorithm_to_string(MCDAAlgorithm algo);
    MCDAAlgorithm string_to_algorithm(const std::string& algo);

    json execute_sync_analysis(
        const std::string& decision_problem,
        const std::vector<DecisionCriterion>& criteria,
        const std::vector<DecisionAlternative>& alternatives,
        MCDAAlgorithm algorithm
    );

    std::string execute_async_analysis(
        const std::string& decision_problem,
        const std::vector<DecisionCriterion>& criteria,
        const std::vector<DecisionAlternative>& alternatives,
        MCDAAlgorithm algorithm
    );

    json execute_algorithm(
        MCDAAlgorithm algorithm,
        const std::vector<DecisionCriterion>& criteria,
        const std::vector<DecisionAlternative>& alternatives
    );

    std::optional<json> get_cached_result(const std::string& cache_key);
    bool cache_result(const std::string& cache_key, const json& result);

    void record_analysis_metadata(const MCDADecisionResult& result);
    MCDADecisionResult load_analysis_metadata(const std::string& analysis_id);

    // Algorithm implementations
    json execute_ahp(const std::vector<DecisionCriterion>& criteria,
                     const std::vector<DecisionAlternative>& alternatives);
    json execute_topsis(const std::vector<DecisionCriterion>& criteria,
                       const std::vector<DecisionAlternative>& alternatives);
    json execute_promethee(const std::vector<DecisionCriterion>& criteria,
                          const std::vector<DecisionAlternative>& alternatives);
    json execute_electre(const std::vector<DecisionCriterion>& criteria,
                        const std::vector<DecisionAlternative>& alternatives);
    json execute_weighted_sum(const std::vector<DecisionCriterion>& criteria,
                             const std::vector<DecisionAlternative>& alternatives);
    json execute_weighted_product(const std::vector<DecisionCriterion>& criteria,
                                 const std::vector<DecisionAlternative>& alternatives);
    json execute_vikor(const std::vector<DecisionCriterion>& criteria,
                      const std::vector<DecisionAlternative>& alternatives);
};

} // namespace decisions
} // namespace regulens

#endif // REGULENS_ASYNC_MCDA_DECISION_SERVICE_HPP
