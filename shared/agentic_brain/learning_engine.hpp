/**
 * Learning Engine - Agentic AI Learning and Adaptation
 *
 * Handles learning from historical data, human feedback, and continuous
 * improvement of agent decision-making capabilities.
 */

#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <random>
#include <atomic>
#include <chrono>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"
#include "llm_interface.hpp"
#include <nlohmann/json.hpp>

namespace regulens {

enum class LearningType {
    PATTERN_RECOGNITION,
    THRESHOLD_ADAPTATION,
    RULE_GENERATION,
    MODEL_UPDATE,
    KNOWLEDGE_EXPANSION
};

enum class FeedbackType {
    POSITIVE,
    NEGATIVE,
    NEUTRAL,
    CORRECTION
};

struct LearningPattern {
    std::string pattern_id;
    std::string pattern_type;
    nlohmann::json pattern_data;
    double confidence_score;
    int occurrence_count;
    std::chrono::system_clock::time_point first_observed;
    std::chrono::system_clock::time_point last_observed;
    std::string source_agent;
    bool active;
};

struct LearningFeedback {
    std::string feedback_id;
    std::string agent_id;
    std::string decision_id;
    FeedbackType feedback_type;
    double feedback_score; // -1.0 to 1.0
    std::string human_feedback;
    std::string feedback_provider;
    nlohmann::json feedback_context;
    std::chrono::system_clock::time_point feedback_timestamp;
    bool incorporated;
};

struct LearningModel {
    std::string model_id;
    std::string agent_type;
    LearningType learning_type;
    nlohmann::json model_parameters;
    nlohmann::json training_data;
    double accuracy_score;
    double precision_score;
    double recall_score;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_updated;
    bool active;
};

// Internal learning model structures
struct PatternRecognitionModel {
    std::string name;
    std::vector<std::string> feature_names;
    std::vector<double> weights;
    double bias = 0.0;
    double learning_rate = 0.001;
    double regularization = 0.1;
};

struct FeedbackModel {
    std::string name;
    std::vector<std::string> feedback_dimensions;
    std::vector<double> historical_feedback;
    double current_accuracy = 0.0;
    int feedback_count = 0;
};

struct Pattern {
    std::string id;
    double confidence_score = 0.0;
    std::unordered_map<std::string, std::string> characteristics;
    nlohmann::json metadata;
};

class LearningEngine {
public:
    LearningEngine(
        std::shared_ptr<ConnectionPool> db_pool,
        std::shared_ptr<LLMInterface> llm_interface,
        StructuredLogger* logger
    );

    ~LearningEngine();

    // Core learning operations
    bool initialize();
    void shutdown();

    // Pattern learning
    bool learn_pattern(const std::string& agent_id, const nlohmann::json& data);
    std::vector<LearningPattern> get_patterns(const std::string& agent_id, const std::string& pattern_type = "");
    bool update_pattern_confidence(const std::string& pattern_id, double new_confidence);

    // Feedback incorporation

    // Model training and adaptation
    bool train_model(const std::string& agent_id, LearningType learning_type, const std::vector<nlohmann::json>& training_data);
    bool update_model_parameters(const std::string& model_id, const nlohmann::json& new_parameters);
    LearningModel get_active_model(const std::string& agent_id, LearningType learning_type);

    // Historical data analysis
    nlohmann::json analyze_historical_performance(const std::string& agent_id, const std::chrono::system_clock::duration& time_window);
    std::vector<nlohmann::json> identify_improvement_opportunities(const std::string& agent_id);
    nlohmann::json generate_learning_insights(const std::string& agent_id);

    // Adaptive learning
    bool adapt_to_feedback_trends(const std::string& agent_id);
    bool retrain_underperforming_models(const std::string& agent_id);
    nlohmann::json suggest_parameter_adjustments(const std::string& agent_id);

    // Knowledge management
    bool store_learned_knowledge(const std::string& agent_id, const std::string& knowledge_type, const nlohmann::json& knowledge);
    nlohmann::json retrieve_knowledge(const std::string& agent_id, const std::string& knowledge_type);
    bool update_knowledge_confidence(const std::string& knowledge_id, double confidence_change);

    // Performance monitoring
    nlohmann::json get_learning_metrics(const std::string& agent_id);
    std::vector<std::string> identify_learning_gaps(const std::string& agent_id);

    // Database operations
    bool store_pattern(const LearningPattern& pattern);
    bool store_feedback(const LearningFeedback& feedback);
    bool store_model(const LearningModel& model);
    bool update_model_performance(const std::string& model_id, const nlohmann::json& metrics);

private:
    // Pattern discovery
    LearningPattern extract_pattern_from_data(const nlohmann::json& data);
    bool validate_pattern(const LearningPattern& pattern);
    bool merge_similar_patterns(const std::string& agent_id);

    // Feedback processing
    bool process_feedback_batch(const std::vector<LearningFeedback>& feedback_batch);
    nlohmann::json analyze_feedback_impact(const LearningFeedback& feedback);
    bool update_model_from_feedback(const std::string& model_id, const LearningFeedback& feedback);

    // Model training
    bool train_pattern_recognition_model(const std::string& agent_id, const std::vector<nlohmann::json>& data);
    bool train_threshold_adaptation_model(const std::string& agent_id, const std::vector<nlohmann::json>& data);
    bool train_rule_generation_model(const std::string& agent_id, const std::vector<nlohmann::json>& data);

    // Historical analysis
    std::vector<nlohmann::json> extract_decision_sequences(const std::string& agent_id, int sequence_length = 10);
    nlohmann::json calculate_performance_trends(const std::string& agent_id, const std::chrono::system_clock::duration& time_window);
    std::vector<nlohmann::json> identify_decision_patterns(const std::string& agent_id);

    // Adaptive algorithms
    nlohmann::json calculate_optimal_parameters(const std::string& agent_id, LearningType learning_type);
    bool implement_parameter_adjustments(const std::string& model_id, const nlohmann::json& adjustments);
    double evaluate_parameter_effectiveness(const nlohmann::json& before_metrics, const nlohmann::json& after_metrics);

    // Knowledge synthesis
    nlohmann::json synthesize_knowledge_from_patterns(const std::vector<LearningPattern>& patterns);
    bool validate_knowledge_consistency(const nlohmann::json& knowledge);
    bool propagate_knowledge_updates(const std::string& agent_id);

    // Internal state management
    std::string generate_pattern_id(const std::string& agent_id);
    std::string generate_feedback_id();
    std::string generate_model_id(const std::string& agent_id, LearningType learning_type);

    // Internal state
    std::shared_ptr<ConnectionPool> db_pool_;
    std::shared_ptr<LLMInterface> llm_interface_;
    StructuredLogger* logger_;

    std::unordered_map<std::string, std::vector<LearningPattern>> agent_patterns_;
    std::unordered_map<std::string, LearningModel> active_models_;
    std::vector<LearningFeedback> pending_feedback_;

    bool initialized_;
    std::atomic<bool> learning_active_;

    // Learning model structures
    std::unordered_map<std::string, PatternRecognitionModel> pattern_models_;
    std::unordered_map<std::string, FeedbackModel> feedback_models_;
    std::vector<LearningFeedback> feedback_history_;
    std::mt19937 random_engine_;

    // Performance tracking
    struct PerformanceMetrics {
        int total_feedback_processed = 0;
        double average_feedback_score = 0.0;
        int positive_feedback_count = 0;
        std::unordered_map<FeedbackType, int> feedback_by_type;
    } performance_metrics_;

    static const size_t MAX_FEEDBACK_HISTORY = 1000;

private:
    // Helper methods
    void initialize_learning_models();
    void initialize_database_schema(PostgreSQLConnection& conn);
    bool validate_feedback(const LearningFeedback& feedback);
    void update_models_from_feedback(const LearningFeedback& feedback);
    double predict_with_model(const PatternRecognitionModel& model, const std::vector<double>& features);
    double calculate_recent_accuracy(const FeedbackModel& model);
    void store_feedback_to_database(const LearningFeedback& feedback);
    void update_performance_metrics(const LearningFeedback& feedback);
    double calculate_learning_effectiveness();
    double calculate_pattern_quality();
    nlohmann::json analyze_feedback_trends();
    nlohmann::json generate_learning_recommendations();
    void save_learning_state();
    std::string feedback_type_to_string(FeedbackType type);
    std::string timestamp_to_string(std::chrono::system_clock::time_point tp);

    // Pattern analysis methods (for future implementation)
    std::vector<std::vector<double>> extract_features_from_data(const std::vector<nlohmann::json>& data);
    std::vector<std::vector<size_t>> perform_clustering(const std::vector<std::vector<double>>& features, size_t max_clusters);
    double euclidean_distance(const std::vector<double>& a, const std::vector<double>& b);
    Pattern create_pattern_from_cluster(const std::vector<size_t>& cluster_indices, const std::vector<nlohmann::json>& data);
    double calculate_pattern_confidence(const Pattern& pattern, const std::vector<size_t>& cluster_indices);
};

} // namespace regulens
