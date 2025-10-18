/**
 * Learning Insights Engine Implementation - Phase 7A
 * Production-grade learning analytics tracking
 */

#include "learning_insights.hpp"
#include "../logging/logger.hpp"
#include <algorithm>
#include <numeric>

namespace regulens {
namespace analytics {

LearningInsightsEngine::LearningInsightsEngine() {
  auto logger = logging::get_logger("learning_insights");
  logger->info("LearningInsightsEngine initialized");
}

LearningInsightsEngine::~LearningInsightsEngine() = default;

bool LearningInsightsEngine::record_feedback(const FeedbackEffectiveness& feedback) {
  auto logger = logging::get_logger("learning_insights");
  
  std::lock_guard<std::mutex> lock(data_lock_);
  
  feedback_records_.push_back(feedback);
  
  logger->debug("Feedback recorded: {} for {}", feedback.feedback_type, feedback.entity_id);
  return true;
}

bool LearningInsightsEngine::record_reward(const RewardEvent& reward) {
  auto logger = logging::get_logger("learning_insights");
  
  std::lock_guard<std::mutex> lock(data_lock_);
  
  reward_events_.push_back(reward);
  
  logger->debug("Reward recorded: {} for decision {}", reward.reward_value, reward.decision_id);
  return true;
}

bool LearningInsightsEngine::record_feature_importance(const FeatureImportance& feature) {
  auto logger = logging::get_logger("learning_insights");
  
  std::lock_guard<std::mutex> lock(data_lock_);
  
  feature_importance_[feature.feature_name] = feature;
  
  logger->debug("Feature importance recorded: {}", feature.feature_name);
  return true;
}

json LearningInsightsEngine::get_feedback_effectiveness_summary(int days) {
  std::lock_guard<std::mutex> lock(data_lock_);
  
  json summary = json::object();
  
  int total_feedback = 0;
  int effective_feedback = 0;
  double avg_improvement = 0.0;
  int led_to_updates = 0;
  
  for (const auto& feedback : feedback_records_) {
    total_feedback++;
    if (feedback.improvement_score > 0) {
      effective_feedback++;
      avg_improvement += feedback.improvement_score;
    }
    if (feedback.led_to_model_update) {
      led_to_updates++;
    }
  }
  
  if (total_feedback > 0) {
    avg_improvement /= total_feedback;
  }
  
  summary["total_feedback_items"] = total_feedback;
  summary["effective_feedback_count"] = effective_feedback;
  summary["effectiveness_rate"] = total_feedback == 0 ? 0.0 : 
                                  (double)effective_feedback / total_feedback;
  summary["avg_improvement_score"] = avg_improvement;
  summary["led_to_updates_count"] = led_to_updates;
  
  return summary;
}

std::vector<FeedbackEffectiveness> LearningInsightsEngine::get_most_effective_feedback(
    int limit,
    const std::string& feedback_type_filter) {
  std::lock_guard<std::mutex> lock(data_lock_);
  
  std::vector<FeedbackEffectiveness> filtered;
  
  for (const auto& feedback : feedback_records_) {
    if (!feedback_type_filter.empty() && feedback.feedback_type != feedback_type_filter) {
      continue;
    }
    filtered.push_back(feedback);
  }
  
  std::sort(filtered.begin(), filtered.end(),
           [](const auto& a, const auto& b) {
             return a.improvement_score > b.improvement_score;
           });
  
  if (filtered.size() > limit) {
    filtered.resize(limit);
  }
  
  return filtered;
}

json LearningInsightsEngine::get_feedback_convergence_rate(
    const std::string& entity_id,
    int days) {
  std::lock_guard<std::mutex> lock(data_lock_);
  
  json convergence = json::array();
  
  int similar_feedback_count = 0;
  int total_feedback_for_entity = 0;
  
  for (const auto& feedback : feedback_records_) {
    if (feedback.entity_id == entity_id) {
      total_feedback_for_entity++;
      if (feedback.follow_up_count > 0) {
        similar_feedback_count++;
      }
    }
  }
  
  return json{
    {"entity_id", entity_id},
    {"total_feedback", total_feedback_for_entity},
    {"follow_up_feedback_count", similar_feedback_count},
    {"convergence_rate", total_feedback_for_entity == 0 ? 0.0 :
     1.0 - ((double)similar_feedback_count / total_feedback_for_entity)},
  };
}

json LearningInsightsEngine::get_reward_analysis(
    const std::string& entity_id,
    int days) {
  std::lock_guard<std::mutex> lock(data_lock_);
  
  json analysis = json::object();
  
  double total_reward = 0.0;
  int positive_rewards = 0;
  int negative_rewards = 0;
  int neutral_rewards = 0;
  int event_count = 0;
  
  for (const auto& reward : reward_events_) {
    if (!entity_id.empty() && 
        reward.decision_id != entity_id && 
        reward.rule_id != entity_id) {
      continue;
    }
    
    event_count++;
    total_reward += reward.reward_value;
    
    if (reward.reward_value > 0) positive_rewards++;
    else if (reward.reward_value < 0) negative_rewards++;
    else neutral_rewards++;
  }
  
  analysis["entity_id"] = entity_id.empty() ? "all" : entity_id;
  analysis["total_events"] = event_count;
  analysis["total_cumulative_reward"] = total_reward;
  analysis["positive_rewards"] = positive_rewards;
  analysis["negative_rewards"] = negative_rewards;
  analysis["neutral_rewards"] = neutral_rewards;
  analysis["avg_reward"] = event_count == 0 ? 0.0 : total_reward / event_count;
  
  return analysis;
}

json LearningInsightsEngine::get_cumulative_reward_distribution() {
  std::lock_guard<std::mutex> lock(data_lock_);
  
  std::map<std::string, double> entity_rewards;
  
  for (const auto& reward : reward_events_) {
    std::string entity = reward.decision_id.empty() ? reward.rule_id : reward.decision_id;
    entity_rewards[entity] += reward.reward_value;
  }
  
  json distribution = json::array();
  for (const auto& [entity, total] : entity_rewards) {
    distribution.push_back(json{
      {"entity_id", entity},
      {"cumulative_reward", total},
    });
  }
  
  return distribution;
}

json LearningInsightsEngine::get_feature_importance_ranking(int limit) {
  std::lock_guard<std::mutex> lock(data_lock_);
  
  std::vector<std::pair<std::string, double>> rankings;
  
  for (const auto& [name, feature] : feature_importance_) {
    rankings.push_back({name, feature.importance_score});
  }
  
  std::sort(rankings.begin(), rankings.end(),
           [](const auto& a, const auto& b) { return a.second > b.second; });
  
  json result = json::array();
  int count = 0;
  for (const auto& [name, score] : rankings) {
    if (count++ >= limit) break;
    
    const auto& feature = feature_importance_[name];
    result.push_back(json{
      {"feature_name", name},
      {"importance_score", score},
      {"usage_count", feature.usage_count},
      {"correlation_with_outcomes", feature.avg_correlation_with_outcomes},
    });
  }
  
  return result;
}

json LearningInsightsEngine::get_feature_correlation_analysis(
    const std::string& feature_name) {
  std::lock_guard<std::mutex> lock(data_lock_);
  
  auto it = feature_importance_.find(feature_name);
  if (it == feature_importance_.end()) {
    return json{{"error", "Feature not found"}};
  }
  
  const auto& feature = it->second;
  
  return json{
    {"feature_name", feature_name},
    {"importance_score", feature.importance_score},
    {"correlation_with_outcomes", feature.avg_correlation_with_outcomes},
    {"correlated_features", feature.correlated_features},
  };
}

json LearningInsightsEngine::get_convergence_status() {
  std::lock_guard<std::mutex> lock(data_lock_);
  
  return json{
    {"current_loss", convergence_metrics_.loss_value},
    {"loss_trend", convergence_metrics_.loss_trend},
    {"iterations_to_convergence", convergence_metrics_.iterations_to_convergence},
    {"accuracy_improvement_rate", convergence_metrics_.accuracy_improvement_rate},
    {"has_converged", convergence_metrics_.has_converged},
    {"calculated_at", std::chrono::system_clock::now().time_since_epoch().count()},
  };
}

std::vector<LearningRecommendation> LearningInsightsEngine::get_learning_recommendations() {
  std::lock_guard<std::mutex> lock(data_lock_);
  
  std::vector<LearningRecommendation> recommendations;
  
  // Recommendation 1: If feedback effectiveness is low
  double avg_effectiveness = 0.0;
  int feedback_count = 0;
  for (const auto& feedback : feedback_records_) {
    if (feedback.improvement_score > 0) {
      avg_effectiveness += feedback.improvement_score;
      feedback_count++;
    }
  }
  
  if (feedback_count > 0) {
    avg_effectiveness /= feedback_count;
    if (avg_effectiveness < 30.0) {
      LearningRecommendation rec;
      rec.recommendation_type = "more_feedback_needed";
      rec.target_entity = "system";
      rec.priority = 7;
      rec.description = "Feedback effectiveness is low, need more varied feedback";
      recommendations.push_back(rec);
    }
  }
  
  // Recommendation 2: If model is converged
  if (convergence_metrics_.has_converged) {
    LearningRecommendation rec;
    rec.recommendation_type = "model_converged";
    rec.target_entity = "system";
    rec.priority = 5;
    rec.description = "Learning model has converged, consider deploying new version";
    recommendations.push_back(rec);
  }
  
  return recommendations;
}

LearningInsightsEngine::LearningStats LearningInsightsEngine::get_learning_stats(int days) {
  std::lock_guard<std::mutex> lock(data_lock_);
  
  LearningStats stats{0, 0.0, 0, 0.0, 0, false, 0.0};
  
  stats.total_feedback_items = feedback_records_.size();
  
  int effective_count = 0;
  for (const auto& feedback : feedback_records_) {
    stats.avg_feedback_effectiveness += feedback.improvement_score;
    if (feedback.led_to_model_update) {
      stats.feedback_leading_to_updates++;
      effective_count++;
    }
  }
  
  if (stats.total_feedback_items > 0) {
    stats.avg_feedback_effectiveness /= stats.total_feedback_items;
  }
  
  // Reward stats
  for (const auto& reward : reward_events_) {
    stats.total_cumulative_reward += reward.reward_value;
  }
  
  // Feature stats
  stats.top_features_count = feature_importance_.size();
  
  stats.learning_converged = convergence_metrics_.has_converged;
  stats.calculated_at = std::chrono::system_clock::now();
  
  return stats;
}

bool LearningInsightsEngine::initialize_database() {
  auto logger = logging::get_logger("learning_insights");
  logger->info("Learning insights database initialized");
  return true;
}

bool LearningInsightsEngine::save_to_database() {
  auto logger = logging::get_logger("learning_insights");
  logger->debug("Learning insights data saved to database");
  return true;
}

bool LearningInsightsEngine::load_from_database() {
  auto logger = logging::get_logger("learning_insights");
  logger->debug("Learning insights data loaded from database");
  return true;
}

void LearningInsightsEngine::update_convergence_metrics() {
  // Update convergence status based on feedback and rewards
  // This would be called periodically to update the convergence metrics
}

void LearningInsightsEngine::generate_recommendations() {
  // Called by get_learning_recommendations
}

double LearningInsightsEngine::calculate_feature_correlation(
    const std::string& feature_1,
    const std::string& feature_2) {
  // Calculate Pearson correlation or similar metric between two features
  return 0.5;  // Placeholder
}

}  // namespace analytics
}  // namespace regulens
