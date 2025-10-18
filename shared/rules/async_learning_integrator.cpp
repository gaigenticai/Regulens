/**
 * AsyncLearningIntegrator Implementation
 */

#include "async_learning_integrator.hpp"

namespace regulens {
namespace rules {

AsyncLearningIntegrator::AsyncLearningIntegrator(
    std::shared_ptr<LearningEngine> learning_engine,
    std::shared_ptr<StructuredLogger> logger)
    : learning_engine_(learning_engine),
      logger_(logger),
      total_feedback_submissions_(0),
      successful_learning_updates_(0) {
}

json AsyncLearningIntegrator::submit_rule_evaluation_feedback(
    const std::string& rule_id,
    const std::string& evaluation_id,
    const json& evaluation_result,
    bool positive_outcome) {

    total_feedback_submissions_++;
    
    logger_->info("Submitting feedback for rule {} (evaluation: {})", rule_id, evaluation_id);
    
    if (!learning_engine_) {
        return json::object({{"warning", "Learning engine not available"}});
    }

    // Process feedback through learning engine
    auto feedback_result = learning_engine_->process_feedback(
        rule_id,
        evaluation_id,
        json::object({
            {"result", evaluation_result},
            {"outcome", positive_outcome ? "positive" : "negative"}
        }),
        positive_outcome ? LearningFeedbackType::POSITIVE : LearningFeedbackType::NEGATIVE
    );

    if (feedback_result.value("success", false)) {
        successful_learning_updates_++;
    }

    return feedback_result;
}

json AsyncLearningIntegrator::submit_decision_feedback(
    const std::string& analysis_id,
    const json& decision_result,
    const json& actual_outcome,
    double confidence_score) {

    total_feedback_submissions_++;

    logger_->info("Submitting decision feedback for analysis: {} (confidence: {})", analysis_id, confidence_score);

    if (!learning_engine_) {
        return json::object({{"warning", "Learning engine not available"}});
    }

    // Determine outcome quality
    bool positive = confidence_score > 0.7;
    
    auto feedback_result = learning_engine_->process_feedback(
        "mcda_decision",
        analysis_id,
        json::object({
            {"decision", decision_result},
            {"actual_outcome", actual_outcome},
            {"confidence", confidence_score}
        }),
        positive ? LearningFeedbackType::POSITIVE : LearningFeedbackType::NEGATIVE
    );

    if (feedback_result.value("success", false)) {
        successful_learning_updates_++;
    }

    return feedback_result;
}

json AsyncLearningIntegrator::get_rule_learning_recommendations(const std::string& rule_id) {
    if (!learning_engine_) {
        return json::object({{"error", "Learning engine not available"}});
    }

    return learning_engine_->get_learning_recommendations(rule_id, json::object());
}

json AsyncLearningIntegrator::get_decision_learning_recommendations(const std::string& decision_problem) {
    if (!learning_engine_) {
        return json::object({{"error", "Learning engine not available"}});
    }

    return learning_engine_->get_learning_recommendations(
        "mcda_decision",
        json::object({{"problem", decision_problem}})
    );
}

bool AsyncLearningIntegrator::update_rule_effectiveness_weights(
    const std::string& rule_id,
    const json& new_weights) {

    logger_->info("Updating rule effectiveness weights for: {}", rule_id);
    return true;
}

json AsyncLearningIntegrator::get_learning_statistics() {
    return json::object({
        {"total_feedback_submissions", total_feedback_submissions_.load()},
        {"successful_learning_updates", successful_learning_updates_.load()},
        {"learning_efficiency_percent", total_feedback_submissions_ > 0 ?
            (static_cast<double>(successful_learning_updates_.load()) / total_feedback_submissions_.load()) * 100.0 : 0.0}
    });
}

} // namespace rules
} // namespace regulens
