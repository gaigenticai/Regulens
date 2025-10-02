#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <nlohmann/json.hpp>

#include "models/feedback_system.hpp"
#include "logging/structured_logger.hpp"
#include "config/configuration_manager.hpp"
#include "pattern_recognition.hpp"

namespace regulens {

/**
 * @brief Feedback incorporation system for continuous learning
 *
 * Collects feedback from various sources, analyzes patterns, and applies
 * learning to improve agent decision-making and system performance.
 */
class FeedbackIncorporationSystem {
public:
    FeedbackIncorporationSystem(std::shared_ptr<ConfigurationManager> config,
                               std::shared_ptr<StructuredLogger> logger,
                               std::shared_ptr<PatternRecognitionEngine> pattern_engine = nullptr);

    ~FeedbackIncorporationSystem();

    /**
     * @brief Initialize the feedback incorporation system
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Shutdown the feedback incorporation system
     */
    void shutdown();

    /**
     * @brief Submit feedback for processing
     * @param feedback The feedback to submit
     * @return true if feedback accepted for processing
     */
    bool submit_feedback(const FeedbackData& feedback);

    /**
     * @brief Submit feedback from human interaction
     * @param human_feedback Human feedback from collaboration system
     * @return true if feedback processed successfully
     */
    bool submit_human_feedback(const HumanFeedback& human_feedback);

    /**
     * @brief Submit system validation feedback
     * @param decision_id Decision that was validated
     * @param outcome Actual outcome (true for correct, false for incorrect)
     * @param confidence Validation confidence (0.0 to 1.0)
     * @return true if feedback processed successfully
     */
    bool submit_system_validation(const std::string& decision_id, bool outcome, double confidence = 1.0);

    /**
     * @brief Apply accumulated feedback to learning models
     * @param entity_id Entity to update (optional, update all if empty)
     * @return Number of models updated
     */
    size_t apply_feedback_learning(const std::string& entity_id = "");

    /**
     * @brief Get learning model for an entity
     * @param entity_id Entity ID
     * @param model_type Model type
     * @return Learning model if found
     */
    std::optional<std::shared_ptr<LearningModel>> get_learning_model(const std::string& entity_id,
                                                                    const std::string& model_type);

    /**
     * @brief Create or update a learning model
     * @param model Learning model to create/update
     * @return true if operation successful
     */
    bool update_learning_model(std::shared_ptr<LearningModel> model);

    /**
     * @brief Analyze feedback patterns for an entity
     * @param entity_id Entity to analyze
     * @param days_back Number of days to analyze
     * @return Feedback analysis results
     */
    FeedbackAnalysis analyze_feedback_patterns(const std::string& entity_id, int days_back = 7);

    /**
     * @brief Get feedback statistics
     * @return JSON with system-wide feedback statistics
     */
    nlohmann::json get_feedback_stats();

    /**
     * @brief Export feedback data
     * @param entity_id Entity filter (optional)
     * @param format Export format ("json", "csv")
     * @return Exported data as string
     */
    std::string export_feedback_data(const std::string& entity_id = "",
                                   const std::string& format = "json");

    /**
     * @brief Force cleanup of old feedback data
     * @return Number of items cleaned up
     */
    size_t cleanup_old_feedback();

    // Configuration access
    const FeedbackConfig& get_config() const { return config_; }

private:
    std::shared_ptr<ConfigurationManager> config_manager_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<PatternRecognitionEngine> pattern_engine_;

    FeedbackConfig config_;

    // Thread-safe storage
    std::mutex feedback_mutex_;
    std::unordered_map<std::string, std::deque<FeedbackData>> entity_feedback_; // entity_id -> feedback queue
    std::unordered_map<std::string, std::shared_ptr<LearningModel>> learning_models_; // model_id -> model

    std::mutex learning_mutex_;
    std::atomic<size_t> total_feedback_processed_;
    std::atomic<size_t> total_models_updated_;

    // Background processing
    std::thread learning_thread_;
    std::atomic<bool> running_;
    std::mutex learning_cv_mutex_;
    std::condition_variable learning_cv_;

    // Learning algorithms
    bool update_decision_model(std::shared_ptr<LearningModel>& model, const std::vector<FeedbackData>& feedback);
    bool update_behavior_model(std::shared_ptr<LearningModel>& model, const std::vector<FeedbackData>& feedback);
    bool update_risk_model(std::shared_ptr<LearningModel>& model, const std::vector<FeedbackData>& feedback);

    // Feedback analysis
    FeedbackAnalysis analyze_entity_feedback(const std::string& entity_id,
                                           std::chrono::system_clock::time_point start_time,
                                           std::chrono::system_clock::time_point end_time);

    // Learning strategies
    double apply_reinforcement_learning(const std::vector<FeedbackData>& feedback,
                                      std::unordered_map<std::string, double>& parameters);
    double apply_supervised_learning(const std::vector<FeedbackData>& feedback,
                                   std::unordered_map<std::string, double>& parameters);
    double apply_batch_learning(const std::vector<FeedbackData>& feedback,
                              std::unordered_map<std::string, double>& parameters);

    // Utility functions
    std::string generate_model_id(const std::string& entity_id, const std::string& model_type);
    bool is_feedback_significant(const FeedbackData& feedback) const;
    double calculate_feedback_weight(const FeedbackData& feedback) const;
    std::vector<FeedbackData> get_recent_feedback(const std::string& entity_id, size_t count = 100);
    std::vector<FeedbackData> get_feedback_in_range(const std::string& entity_id,
                                                   std::chrono::system_clock::time_point start,
                                                   std::chrono::system_clock::time_point end);

    // Model validation and evaluation
    double evaluate_model_accuracy(const std::shared_ptr<LearningModel>& model,
                                 const std::vector<FeedbackData>& test_feedback);
    bool validate_model_parameters(const std::unordered_map<std::string, double>& parameters);

    // Background learning worker
    void learning_worker();

    // Integration with pattern recognition
    void submit_feedback_to_pattern_engine(const FeedbackData& feedback);

    // Persistence (when enabled)
    bool persist_feedback(const FeedbackData& feedback);
    bool persist_learning_model(const std::shared_ptr<LearningModel>& model);
    std::vector<FeedbackData> load_feedback(const std::string& entity_id);
    std::vector<std::shared_ptr<LearningModel>> load_learning_models(const std::string& entity_id);
};

// Convenience functions for creating feedback from different sources

/**
 * @brief Create feedback from human interaction
 */
inline FeedbackData create_feedback_from_human(const HumanFeedback& human_fb,
                                             const std::string& decision_id) {
    FeedbackData fb(decision_id, FeedbackType::HUMAN_EXPLICIT,
                   "human:" + human_fb.session_id, human_fb.agent_id);

    fb.decision_id = human_fb.decision_id;
    fb.context = "human_ai_collaboration";
    fb.feedback_text = human_fb.feedback_text;

    // Convert feedback type to score
    switch (human_fb.feedback_type) {
        case FeedbackType::AGREEMENT:
            fb.feedback_score = 1.0;
            break;
        case FeedbackType::DISAGREEMENT:
            fb.feedback_score = -1.0;
            break;
        case FeedbackType::PARTIAL_AGREEMENT:
            fb.feedback_score = 0.5;
            break;
        case FeedbackType::UNCERTAIN:
            fb.feedback_score = 0.0;
            break;
        case FeedbackType::REQUEST_CLARIFICATION:
            fb.feedback_score = -0.3;
            break;
        case FeedbackType::SUGGEST_ALTERNATIVE:
            fb.feedback_score = -0.7;
            break;
        default:
            fb.feedback_score = 0.0;
    }

    fb.priority = (std::abs(fb.feedback_score) > 0.7) ? FeedbackPriority::HIGH : FeedbackPriority::MEDIUM;

    return fb;
}

/**
 * @brief Create feedback from system validation
 */
inline FeedbackData create_feedback_from_validation(const std::string& decision_id,
                                                   const std::string& agent_id,
                                                   bool correct_outcome,
                                                   double confidence = 1.0) {
    FeedbackData fb("system_validation_" + decision_id, FeedbackType::SYSTEM_VALIDATION,
                   "system", agent_id);

    fb.decision_id = decision_id;
    fb.context = "system_validation";
    fb.feedback_score = correct_outcome ? confidence : -confidence;
    fb.feedback_text = correct_outcome ? "Decision validated as correct" : "Decision identified as incorrect";
    fb.priority = confidence > 0.8 ? FeedbackPriority::HIGH : FeedbackPriority::MEDIUM;

    fb.metadata["validation_confidence"] = std::to_string(confidence);
    fb.metadata["outcome"] = correct_outcome ? "correct" : "incorrect";

    return fb;
}

/**
 * @brief Create feedback from performance metrics
 */
inline FeedbackData create_feedback_from_performance(const std::string& agent_id,
                                                   const std::string& metric_name,
                                                   double actual_value,
                                                   double expected_value,
                                                   double tolerance = 0.1) {
    FeedbackData fb("performance_" + agent_id + "_" + metric_name,
                   FeedbackType::PERFORMANCE_METRIC, "system", agent_id);

    fb.context = "performance_monitoring";
    double deviation = std::abs(actual_value - expected_value) / std::abs(expected_value);
    fb.feedback_score = (deviation <= tolerance) ? 0.5 : -deviation;
    fb.feedback_text = "Performance metric: " + metric_name + " = " + std::to_string(actual_value) +
                      " (expected: " + std::to_string(expected_value) + ")";
    fb.priority = (deviation > tolerance * 2) ? FeedbackPriority::HIGH : FeedbackPriority::MEDIUM;

    fb.metadata["metric_name"] = metric_name;
    fb.metadata["actual_value"] = std::to_string(actual_value);
    fb.metadata["expected_value"] = std::to_string(expected_value);
    fb.metadata["deviation"] = std::to_string(deviation);

    return fb;
}

} // namespace regulens
