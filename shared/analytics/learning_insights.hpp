/**
 * Learning Engine Insights - Phase 7A
 * Analytics for feedback effectiveness and learning progress
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>
#include <chrono>

namespace regulens {
namespace analytics {

using json = nlohmann::json;

// Feedback effectiveness tracking
struct FeedbackEffectiveness {
  std::string feedback_id;
  std::string feedback_type;  // rule_correction, decision_validation, etc.
  std::string entity_id;  // rule_id or decision_id
  int improvement_score = 0;  // -100 to 100
  int follow_up_count = 0;  // How many times the same feedback was needed
  bool led_to_model_update = false;
  double model_accuracy_before = 0.0;
  double model_accuracy_after = 0.0;
  std::chrono::system_clock::time_point submitted_at;
  std::chrono::system_clock::time_point processed_at;
};

// Reinforcement learning reward tracking
struct RewardEvent {
  std::string decision_id;
  std::string rule_id;
  double reward_value = 0.0;  // Positive, negative, or zero
  std::string reward_reason;  // "good_outcome", "bad_outcome", "correction", etc.
  int cumulative_reward = 0;  // Sum of all rewards for this entity
  std::chrono::system_clock::time_point occurred_at;
};

// Feature importance in learning
struct FeatureImportance {
  std::string feature_name;
  double importance_score = 0.0;  // 0-1, how important for decisions
  int usage_count = 0;
  double avg_correlation_with_outcomes = 0.0;
  std::vector<std::string> correlated_features;
};

// Convergence metrics
struct ConvergenceMetrics {
  double loss_value = 0.0;  // Current loss (lower is better)
  double loss_trend = 0.0;  // Rate of improvement (-1 to 1)
  int iterations_to_convergence = 0;
  double accuracy_improvement_rate = 0.0;  // Per iteration
  bool has_converged = false;
  std::chrono::system_clock::time_point calculated_at;
};

// Learning recommendation
struct LearningRecommendation {
  std::string recommendation_type;  // "more_feedback_needed", "model_update_ready", etc.
  std::string target_entity;  // rule_id, decision_id, or "system"
  int priority = 0;  // 0-10, higher is more urgent
  std::string description;
  json details;
};

class LearningInsightsEngine {
public:
  LearningInsightsEngine();
  ~LearningInsightsEngine();

  // Record feedback
  bool record_feedback(const FeedbackEffectiveness& feedback);

  // Record reward
  bool record_reward(const RewardEvent& reward);

  // Record feature importance
  bool record_feature_importance(const FeatureImportance& feature);

  // Query analytics
  json get_feedback_effectiveness_summary(int days = 30);
  
  std::vector<FeedbackEffectiveness> get_most_effective_feedback(
      int limit = 10,
      const std::string& feedback_type_filter = "");

  json get_feedback_convergence_rate(
      const std::string& entity_id,
      int days = 30);

  json get_reward_analysis(
      const std::string& entity_id = "",
      int days = 30);

  json get_cumulative_reward_distribution();

  json get_feature_importance_ranking(int limit = 20);

  json get_feature_correlation_analysis(const std::string& feature_name);

  json get_convergence_status();

  std::vector<LearningRecommendation> get_learning_recommendations();

  // Statistics
  struct LearningStats {
    int total_feedback_items;
    double avg_feedback_effectiveness;
    int feedback_leading_to_updates;
    double total_cumulative_reward;
    int top_features_count;
    bool learning_converged;
    double current_system_accuracy;
    std::chrono::system_clock::time_point calculated_at;
  };

  LearningStats get_learning_stats(int days = 30);

  // Database operations
  bool initialize_database();
  bool save_to_database();
  bool load_from_database();

private:
  std::vector<FeedbackEffectiveness> feedback_records_;
  std::vector<RewardEvent> reward_events_;
  std::map<std::string, FeatureImportance> feature_importance_;
  ConvergenceMetrics convergence_metrics_;
  
  std::mutex data_lock_;

  // Internal helpers
  void update_convergence_metrics();
  void generate_recommendations();
  double calculate_feature_correlation(
      const std::string& feature_1,
      const std::string& feature_2);
};

}  // namespace analytics
}  // namespace regulens
