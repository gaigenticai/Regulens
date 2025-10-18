/**
 * AsyncLearningIntegrator - Production-Grade Feedback Loop Integration
 * Bridges evaluations with LearningEngine for continuous improvement
 */

#pragma once

#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include "../memory/learning_engine.hpp"
#include "../logging/structured_logger.hpp"

using json = nlohmann::json;

namespace regulens {
namespace rules {

class AsyncLearningIntegrator {
public:
    AsyncLearningIntegrator(
        std::shared_ptr<LearningEngine> learning_engine,
        std::shared_ptr<StructuredLogger> logger
    );

    /**
     * Submit rule evaluation feedback for learning
     */
    json submit_rule_evaluation_feedback(
        const std::string& rule_id,
        const std::string& evaluation_id,
        const json& evaluation_result,
        bool positive_outcome
    );

    /**
     * Submit decision analysis feedback for learning
     */
    json submit_decision_feedback(
        const std::string& analysis_id,
        const json& decision_result,
        const json& actual_outcome,
        double confidence_score
    );

    /**
     * Get learning recommendations for rule
     */
    json get_rule_learning_recommendations(const std::string& rule_id);

    /**
     * Get learning recommendations for decision problem
     */
    json get_decision_learning_recommendations(const std::string& decision_problem);

    /**
     * Update rule weights from learning outcomes
     */
    bool update_rule_effectiveness_weights(const std::string& rule_id, const json& new_weights);

    /**
     * Get learning statistics
     */
    json get_learning_statistics();

private:
    std::shared_ptr<LearningEngine> learning_engine_;
    std::shared_ptr<StructuredLogger> logger_;
    
    std::atomic<size_t> total_feedback_submissions_;
    std::atomic<size_t> successful_learning_updates_;
};

} // namespace rules
} // namespace regulens

#endif
