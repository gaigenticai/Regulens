/**
 * Decision Analytics Engine - Phase 7A
 * Production-grade analytics for MCDA decisions
 * Tracks accuracy, algorithm comparison, sensitivity analysis
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

// Decision outcome tracking
enum class DecisionOutcome {
  PENDING,
  APPROVED,
  REJECTED,
  MODIFIED,
  EXECUTED,
  SUCCESSFUL,
  FAILED
};

// Algorithm performance metrics
struct AlgorithmMetrics {
  std::string algorithm_name;
  int total_decisions = 0;
  int accurate_decisions = 0;
  int inaccurate_decisions = 0;
  double accuracy_rate = 0.0;
  double avg_execution_time_ms = 0.0;
  double avg_confidence_score = 0.0;
  std::chrono::system_clock::time_point last_updated;
};

// Decision record for analytics
struct DecisionRecord {
  std::string decision_id;
  std::string algorithm;
  std::vector<std::string> alternative_names;
  std::string selected_alternative;
  double decision_score;
  double confidence;
  json criteria_weights;
  json alternative_scores;
  std::string actual_outcome;  // What actually happened
  bool was_correct = false;
  int feedback_count = 0;
  std::chrono::system_clock::time_point created_at;
  std::chrono::system_clock::time_point resolved_at;
};

// Sensitivity analysis result
struct SensitivityResult {
  std::string decision_id;
  std::string parameter_name;
  double min_value;
  double max_value;
  double step;
  std::vector<std::string> impacted_alternatives;
  json sensitivity_curve;  // Parameter value -> alternative ranking changes
};

// Ensemble comparison metrics
struct EnsembleMetrics {
  std::string decision_id;
  std::vector<std::string> algorithms_used;
  std::vector<std::string> algorithm_results;  // What each algorithm recommended
  std::string ensemble_result;  // Final aggregated decision
  std::string actual_outcome;
  bool ensemble_was_correct = false;
  bool best_individual_correct = false;
  double ensemble_confidence;
};

class DecisionAnalyticsEngine {
public:
  DecisionAnalyticsEngine();
  ~DecisionAnalyticsEngine();

  // Record decision execution
  bool record_decision(const DecisionRecord& record);

  // Record actual outcome (feedback)
  bool record_decision_outcome(const std::string& decision_id, 
                               const std::string& actual_outcome,
                               bool was_correct);

  // Record sensitivity analysis
  bool record_sensitivity_analysis(const SensitivityResult& result);

  // Record ensemble comparison
  bool record_ensemble_comparison(const EnsembleMetrics& metrics);

  // Query analytics
  AlgorithmMetrics get_algorithm_metrics(const std::string& algorithm_name);
  
  std::vector<AlgorithmMetrics> get_all_algorithm_metrics();

  json get_algorithm_comparison(
      const std::vector<std::string>& algorithms,
      int days = 30);

  json get_ensemble_vs_individual_analysis(int days = 30);

  json get_decision_accuracy_timeline(
      const std::string& algorithm,
      int days = 30,
      int bucket_hours = 24);

  json get_sensitivity_analysis_summary(const std::string& decision_id);

  std::vector<DecisionRecord> get_recent_decisions(
      int limit = 100,
      const std::string& algorithm_filter = "");

  // Statistics
  struct DecisionStats {
    int total_decisions;
    int decisions_with_feedback;
    double overall_accuracy;
    double avg_confidence;
    std::string best_algorithm;
    std::string worst_algorithm;
    std::chrono::system_clock::time_point calculated_at;
  };

  DecisionStats get_decision_stats(int days = 30);

  // Database operations
  bool initialize_database();
  bool save_to_database();
  bool load_from_database();

private:
  std::map<std::string, AlgorithmMetrics> algorithm_metrics_;
  std::map<std::string, DecisionRecord> decision_records_;
  std::map<std::string, SensitivityResult> sensitivity_results_;
  std::vector<EnsembleMetrics> ensemble_comparisons_;
  
  std::mutex data_lock_;

  // Internal helpers
  void update_algorithm_metrics(const DecisionRecord& record);
  double calculate_accuracy_rate(const std::string& algorithm);
  json generate_comparison_json(const std::vector<AlgorithmMetrics>& metrics);
};

}  // namespace analytics
}  // namespace regulens
