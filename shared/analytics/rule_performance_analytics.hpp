/**
 * Rule Performance Analytics Engine - Phase 7A
 * Tracks rule effectiveness, false positives, redundancy
 */

#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>
#include <chrono>

namespace regulens {
namespace analytics {

using json = nlohmann::json;

// Rule confusion matrix
struct RuleConfusionMatrix {
  int true_positives = 0;
  int false_positives = 0;
  int true_negatives = 0;
  int false_negatives = 0;
  
  double precision() const {
    int positive_total = true_positives + false_positives;
    return positive_total == 0 ? 0.0 : (double)true_positives / positive_total;
  }
  
  double recall() const {
    int actual_positive = true_positives + false_negatives;
    return actual_positive == 0 ? 0.0 : (double)true_positives / actual_positive;
  }
  
  double f1_score() const {
    double p = precision();
    double r = recall();
    if (p + r == 0) return 0.0;
    return 2.0 * (p * r) / (p + r);
  }
  
  double specificity() const {
    int actual_negative = true_negatives + false_positives;
    return actual_negative == 0 ? 0.0 : (double)true_negatives / actual_negative;
  }
};

// Rule execution metrics
struct RuleExecutionMetrics {
  std::string rule_id;
  int total_executions = 0;
  int successful_executions = 0;
  int failed_executions = 0;
  double avg_execution_time_ms = 0.0;
  double min_execution_time_ms = 0.0;
  double max_execution_time_ms = 0.0;
  double p95_execution_time_ms = 0.0;
  std::chrono::system_clock::time_point last_executed;
};

// Rule interaction (redundancy detection)
struct RuleInteraction {
  std::string rule_id_1;
  std::string rule_id_2;
  int overlapping_triggers = 0;  // Both rules triggered on same data
  int conflicting_outcomes = 0;  // Rules produced different outcomes
  double similarity_score = 0.0;  // 0-1, how similar the rules are
};

// Rule effectiveness record
struct RuleEffectivenessRecord {
  std::string rule_id;
  std::string rule_name;
  std::string rule_type;  // VALIDATION, SCORING, PATTERN, ML
  RuleConfusionMatrix confusion_matrix;
  RuleExecutionMetrics execution_metrics;
  int total_events_processed = 0;
  double business_impact_score = 0.0;  // 0-100, subjective value
  std::chrono::system_clock::time_point created_at;
  std::chrono::system_clock::time_point last_updated;
};

class RulePerformanceAnalyticsEngine {
public:
  RulePerformanceAnalyticsEngine();
  ~RulePerformanceAnalyticsEngine();

  // Record rule execution
  bool record_rule_execution(
      const std::string& rule_id,
      bool was_successful,
      double execution_time_ms);

  // Record rule outcome (feedback)
  bool record_rule_outcome(
      const std::string& rule_id,
      bool predicted_positive,
      bool actual_positive);

  // Record rule interaction (for redundancy detection)
  bool record_rule_interaction(const RuleInteraction& interaction);

  // Query analytics
  RuleEffectivenessRecord get_rule_metrics(const std::string& rule_id);
  
  std::vector<RuleEffectivenessRecord> get_all_rule_metrics();

  // Redundancy analysis
  json get_redundant_rules(double similarity_threshold = 0.7);
  
  json get_rule_interactions(const std::string& rule_id);

  // Performance analysis
  json get_rule_performance_comparison(
      const std::vector<std::string>& rule_ids,
      const std::string& metric = "f1_score");

  json get_rules_by_false_positive_rate(
      int limit = 10,
      double min_fp_rate = 0.0);

  json get_execution_time_analysis(
      const std::string& rule_id,
      int days = 30);

  // Statistics
  struct RuleStats {
    int total_rules;
    int rules_with_feedback;
    double avg_precision;
    double avg_recall;
    double avg_f1_score;
    int redundant_rule_pairs;
    std::vector<std::string> problematic_rules;  // High FP rate
    std::chrono::system_clock::time_point calculated_at;
  };

  RuleStats get_rule_stats(int days = 30);

  // Database operations
  bool initialize_database();
  bool save_to_database();
  bool load_from_database();

private:
  std::map<std::string, RuleEffectivenessRecord> rule_records_;
  std::vector<RuleInteraction> rule_interactions_;
  
  std::mutex data_lock_;

  // Internal helpers
  void update_execution_metrics(
      RuleExecutionMetrics& metrics,
      double execution_time_ms);
  double calculate_similarity(
      const RuleConfusionMatrix& m1,
      const RuleConfusionMatrix& m2);
  void detect_redundancy();
};

}  // namespace analytics
}  // namespace regulens
