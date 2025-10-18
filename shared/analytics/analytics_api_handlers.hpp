/**
 * Analytics API Handlers - Phase 7A
 * REST endpoints for decision, rule, and learning analytics
 */

#pragma once

#include <string>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include "decision_analytics.hpp"
#include "rule_performance_analytics.hpp"
#include "learning_insights.hpp"

namespace regulens {
namespace analytics {

using json = nlohmann::json;

struct HTTPRequest {
  std::string method;
  std::string path;
  json body;
  std::map<std::string, std::string> headers;
  std::map<std::string, std::string> query_params;
};

struct HTTPResponse {
  int status_code = 200;
  json body;
  std::map<std::string, std::string> headers;
};

class AnalyticsAPIHandlers {
public:
  AnalyticsAPIHandlers(
      std::shared_ptr<DecisionAnalyticsEngine> decision_engine,
      std::shared_ptr<RulePerformanceAnalyticsEngine> rule_engine,
      std::shared_ptr<LearningInsightsEngine> learning_engine);

  ~AnalyticsAPIHandlers();

  // Decision Analytics Endpoints
  HTTPResponse handle_get_algorithm_comparison(const HTTPRequest& req);
  HTTPResponse handle_get_decision_accuracy_timeline(const HTTPRequest& req);
  HTTPResponse handle_get_ensemble_comparison(const HTTPRequest& req);
  HTTPResponse handle_record_decision(const HTTPRequest& req);
  HTTPResponse handle_record_decision_outcome(const HTTPRequest& req);
  HTTPResponse handle_get_decision_stats(const HTTPRequest& req);

  // Rule Performance Endpoints
  HTTPResponse handle_get_rule_metrics(const HTTPRequest& req);
  HTTPResponse handle_get_redundant_rules(const HTTPRequest& req);
  HTTPResponse handle_get_rule_performance_comparison(const HTTPRequest& req);
  HTTPResponse handle_get_high_false_positive_rules(const HTTPRequest& req);
  HTTPResponse handle_record_rule_execution(const HTTPRequest& req);
  HTTPResponse handle_record_rule_outcome(const HTTPRequest& req);
  HTTPResponse handle_get_rule_stats(const HTTPRequest& req);

  // Learning Insights Endpoints
  HTTPResponse handle_get_feedback_effectiveness(const HTTPRequest& req);
  HTTPResponse handle_get_reward_analysis(const HTTPRequest& req);
  HTTPResponse handle_get_feature_importance(const HTTPRequest& req);
  HTTPResponse handle_get_convergence_status(const HTTPRequest& req);
  HTTPResponse handle_get_learning_recommendations(const HTTPRequest& req);
  HTTPResponse handle_record_feedback(const HTTPRequest& req);
  HTTPResponse handle_record_reward(const HTTPRequest& req);
  HTTPResponse handle_get_learning_stats(const HTTPRequest& req);

  // Aggregated Endpoints
  HTTPResponse handle_get_system_analytics_dashboard(const HTTPRequest& req);
  HTTPResponse handle_get_health_check(const HTTPRequest& req);

private:
  std::shared_ptr<DecisionAnalyticsEngine> decision_engine_;
  std::shared_ptr<RulePerformanceAnalyticsEngine> rule_engine_;
  std::shared_ptr<LearningInsightsEngine> learning_engine_;

  // Helper methods
  HTTPResponse create_error_response(int status_code, const std::string& message);
  HTTPResponse create_success_response(const json& data);
  std::string get_query_param(const HTTPRequest& req, const std::string& key, const std::string& default_value = "");
  int get_query_param_int(const HTTPRequest& req, const std::string& key, int default_value = 0);
};

}  // namespace analytics
}  // namespace regulens
