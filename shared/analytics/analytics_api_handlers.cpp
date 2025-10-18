/**
 * Analytics API Handlers Implementation - Phase 7A
 * Production-grade REST endpoints for analytics
 */

#include "analytics_api_handlers.hpp"
#include "../logging/logger.hpp"

namespace regulens {
namespace analytics {

AnalyticsAPIHandlers::AnalyticsAPIHandlers(
    std::shared_ptr<DecisionAnalyticsEngine> decision_engine,
    std::shared_ptr<RulePerformanceAnalyticsEngine> rule_engine,
    std::shared_ptr<LearningInsightsEngine> learning_engine)
    : decision_engine_(decision_engine),
      rule_engine_(rule_engine),
      learning_engine_(learning_engine) {
  auto logger = logging::get_logger("analytics_api");
  logger->info("AnalyticsAPIHandlers initialized");
}

AnalyticsAPIHandlers::~AnalyticsAPIHandlers() = default;

// Decision Analytics Endpoints
HTTPResponse AnalyticsAPIHandlers::handle_get_algorithm_comparison(const HTTPRequest& req) {
  auto logger = logging::get_logger("analytics_api");
  
  try {
    std::vector<std::string> algorithms;
    if (req.body.contains("algorithms")) {
      for (const auto& algo : req.body["algorithms"]) {
        algorithms.push_back(algo.get<std::string>());
      }
    }
    
    int days = get_query_param_int(req, "days", 30);
    
    json comparison = decision_engine_->get_algorithm_comparison(algorithms, days);
    return create_success_response(comparison);
  } catch (const std::exception& e) {
    logger->error("Error in algorithm comparison: {}", e.what());
    return create_error_response(500, e.what());
  }
}

HTTPResponse AnalyticsAPIHandlers::handle_get_decision_accuracy_timeline(const HTTPRequest& req) {
  auto logger = logging::get_logger("analytics_api");
  
  try {
    std::string algorithm = get_query_param(req, "algorithm", "");
    int days = get_query_param_int(req, "days", 30);
    
    if (algorithm.empty()) {
      return create_error_response(400, "Algorithm parameter required");
    }
    
    json timeline = decision_engine_->get_decision_accuracy_timeline(algorithm, days, 24);
    return create_success_response(timeline);
  } catch (const std::exception& e) {
    logger->error("Error in accuracy timeline: {}", e.what());
    return create_error_response(500, e.what());
  }
}

HTTPResponse AnalyticsAPIHandlers::handle_get_ensemble_comparison(const HTTPRequest& req) {
  auto logger = logging::get_logger("analytics_api");
  
  try {
    int days = get_query_param_int(req, "days", 30);
    
    json comparison = decision_engine_->get_ensemble_vs_individual_analysis(days);
    return create_success_response(comparison);
  } catch (const std::exception& e) {
    logger->error("Error in ensemble comparison: {}", e.what());
    return create_error_response(500, e.what());
  }
}

HTTPResponse AnalyticsAPIHandlers::handle_record_decision(const HTTPRequest& req) {
  auto logger = logging::get_logger("analytics_api");
  
  try {
    if (!req.body.contains("decision_id") || !req.body.contains("algorithm")) {
      return create_error_response(400, "Missing required fields");
    }
    
    DecisionRecord record;
    record.decision_id = req.body["decision_id"].get<std::string>();
    record.algorithm = req.body["algorithm"].get<std::string>();
    record.decision_score = req.body.value("decision_score", 0.0);
    record.confidence = req.body.value("confidence", 0.5);
    record.created_at = std::chrono::system_clock::now();
    
    bool success = decision_engine_->record_decision(record);
    return create_success_response({{"recorded", success}});
  } catch (const std::exception& e) {
    logger->error("Error recording decision: {}", e.what());
    return create_error_response(500, e.what());
  }
}

HTTPResponse AnalyticsAPIHandlers::handle_record_decision_outcome(const HTTPRequest& req) {
  auto logger = logging::get_logger("analytics_api");
  
  try {
    if (!req.body.contains("decision_id")) {
      return create_error_response(400, "decision_id required");
    }
    
    std::string decision_id = req.body["decision_id"].get<std::string>();
    std::string actual_outcome = req.body.value("actual_outcome", "");
    bool was_correct = req.body.value("was_correct", false);
    
    bool success = decision_engine_->record_decision_outcome(decision_id, actual_outcome, was_correct);
    return create_success_response({{"recorded", success}});
  } catch (const std::exception& e) {
    logger->error("Error recording decision outcome: {}", e.what());
    return create_error_response(500, e.what());
  }
}

HTTPResponse AnalyticsAPIHandlers::handle_get_decision_stats(const HTTPRequest& req) {
  auto logger = logging::get_logger("analytics_api");
  
  try {
    int days = get_query_param_int(req, "days", 30);
    
    auto stats = decision_engine_->get_decision_stats(days);
    return create_success_response({
      {"total_decisions", stats.total_decisions},
      {"decisions_with_feedback", stats.decisions_with_feedback},
      {"overall_accuracy", stats.overall_accuracy},
      {"avg_confidence", stats.avg_confidence},
      {"best_algorithm", stats.best_algorithm},
      {"worst_algorithm", stats.worst_algorithm},
    });
  } catch (const std::exception& e) {
    logger->error("Error getting decision stats: {}", e.what());
    return create_error_response(500, e.what());
  }
}

// Rule Performance Endpoints
HTTPResponse AnalyticsAPIHandlers::handle_get_rule_metrics(const HTTPRequest& req) {
  auto logger = logging::get_logger("analytics_api");
  
  try {
    std::string rule_id = get_query_param(req, "rule_id", "");
    
    if (rule_id.empty()) {
      return create_error_response(400, "rule_id parameter required");
    }
    
    auto metrics = rule_engine_->get_rule_metrics(rule_id);
    return create_success_response({
      {"rule_id", metrics.rule_id},
      {"rule_name", metrics.rule_name},
      {"precision", metrics.confusion_matrix.precision()},
      {"recall", metrics.confusion_matrix.recall()},
      {"f1_score", metrics.confusion_matrix.f1_score()},
      {"executions", metrics.execution_metrics.total_executions},
      {"avg_execution_time_ms", metrics.execution_metrics.avg_execution_time_ms},
    });
  } catch (const std::exception& e) {
    logger->error("Error getting rule metrics: {}", e.what());
    return create_error_response(500, e.what());
  }
}

HTTPResponse AnalyticsAPIHandlers::handle_get_redundant_rules(const HTTPRequest& req) {
  auto logger = logging::get_logger("analytics_api");
  
  try {
    double threshold = 0.7;
    if (req.body.contains("similarity_threshold")) {
      threshold = req.body["similarity_threshold"].get<double>();
    }
    
    json redundant = rule_engine_->get_redundant_rules(threshold);
    return create_success_response({{"redundant_rules", redundant}});
  } catch (const std::exception& e) {
    logger->error("Error getting redundant rules: {}", e.what());
    return create_error_response(500, e.what());
  }
}

HTTPResponse AnalyticsAPIHandlers::handle_get_high_false_positive_rules(const HTTPRequest& req) {
  auto logger = logging::get_logger("analytics_api");
  
  try {
    int limit = get_query_param_int(req, "limit", 10);
    
    json rules = rule_engine_->get_rules_by_false_positive_rate(limit, 0.0);
    return create_success_response({{"high_fp_rules", rules}});
  } catch (const std::exception& e) {
    logger->error("Error getting high FP rules: {}", e.what());
    return create_error_response(500, e.what());
  }
}

HTTPResponse AnalyticsAPIHandlers::handle_record_rule_execution(const HTTPRequest& req) {
  auto logger = logging::get_logger("analytics_api");
  
  try {
    if (!req.body.contains("rule_id") || !req.body.contains("was_successful")) {
      return create_error_response(400, "Missing required fields");
    }
    
    std::string rule_id = req.body["rule_id"].get<std::string>();
    bool was_successful = req.body["was_successful"].get<bool>();
    double execution_time = req.body.value("execution_time_ms", 0.0);
    
    bool success = rule_engine_->record_rule_execution(rule_id, was_successful, execution_time);
    return create_success_response({{"recorded", success}});
  } catch (const std::exception& e) {
    logger->error("Error recording rule execution: {}", e.what());
    return create_error_response(500, e.what());
  }
}

HTTPResponse AnalyticsAPIHandlers::handle_record_rule_outcome(const HTTPRequest& req) {
  auto logger = logging::get_logger("analytics_api");
  
  try {
    if (!req.body.contains("rule_id")) {
      return create_error_response(400, "rule_id required");
    }
    
    std::string rule_id = req.body["rule_id"].get<std::string>();
    bool predicted_positive = req.body.value("predicted_positive", false);
    bool actual_positive = req.body.value("actual_positive", false);
    
    bool success = rule_engine_->record_rule_outcome(rule_id, predicted_positive, actual_positive);
    return create_success_response({{"recorded", success}});
  } catch (const std::exception& e) {
    logger->error("Error recording rule outcome: {}", e.what());
    return create_error_response(500, e.what());
  }
}

HTTPResponse AnalyticsAPIHandlers::handle_get_rule_stats(const HTTPRequest& req) {
  auto logger = logging::get_logger("analytics_api");
  
  try {
    int days = get_query_param_int(req, "days", 30);
    
    auto stats = rule_engine_->get_rule_stats(days);
    return create_success_response({
      {"total_rules", stats.total_rules},
      {"rules_with_feedback", stats.rules_with_feedback},
      {"avg_precision", stats.avg_precision},
      {"avg_recall", stats.avg_recall},
      {"avg_f1_score", stats.avg_f1_score},
      {"redundant_rule_pairs", stats.redundant_rule_pairs},
    });
  } catch (const std::exception& e) {
    logger->error("Error getting rule stats: {}", e.what());
    return create_error_response(500, e.what());
  }
}

// Learning Insights Endpoints
HTTPResponse AnalyticsAPIHandlers::handle_get_feedback_effectiveness(const HTTPRequest& req) {
  auto logger = logging::get_logger("analytics_api");
  
  try {
    int days = get_query_param_int(req, "days", 30);
    
    json effectiveness = learning_engine_->get_feedback_effectiveness_summary(days);
    return create_success_response(effectiveness);
  } catch (const std::exception& e) {
    logger->error("Error getting feedback effectiveness: {}", e.what());
    return create_error_response(500, e.what());
  }
}

HTTPResponse AnalyticsAPIHandlers::handle_get_reward_analysis(const HTTPRequest& req) {
  auto logger = logging::get_logger("analytics_api");
  
  try {
    std::string entity_id = get_query_param(req, "entity_id", "");
    int days = get_query_param_int(req, "days", 30);
    
    json analysis = learning_engine_->get_reward_analysis(entity_id, days);
    return create_success_response(analysis);
  } catch (const std::exception& e) {
    logger->error("Error getting reward analysis: {}", e.what());
    return create_error_response(500, e.what());
  }
}

HTTPResponse AnalyticsAPIHandlers::handle_get_feature_importance(const HTTPRequest& req) {
  auto logger = logging::get_logger("analytics_api");
  
  try {
    int limit = get_query_param_int(req, "limit", 20);
    
    json features = learning_engine_->get_feature_importance_ranking(limit);
    return create_success_response({{"features", features}});
  } catch (const std::exception& e) {
    logger->error("Error getting feature importance: {}", e.what());
    return create_error_response(500, e.what());
  }
}

HTTPResponse AnalyticsAPIHandlers::handle_get_convergence_status(const HTTPRequest& req) {
  auto logger = logging::get_logger("analytics_api");
  
  try {
    json status = learning_engine_->get_convergence_status();
    return create_success_response(status);
  } catch (const std::exception& e) {
    logger->error("Error getting convergence status: {}", e.what());
    return create_error_response(500, e.what());
  }
}

HTTPResponse AnalyticsAPIHandlers::handle_get_learning_recommendations(const HTTPRequest& req) {
  auto logger = logging::get_logger("analytics_api");
  
  try {
    auto recommendations = learning_engine_->get_learning_recommendations();
    json rec_json = json::array();
    for (const auto& rec : recommendations) {
      rec_json.push_back({
        {"recommendation_type", rec.recommendation_type},
        {"target_entity", rec.target_entity},
        {"priority", rec.priority},
        {"description", rec.description},
      });
    }
    return create_success_response({{"recommendations", rec_json}});
  } catch (const std::exception& e) {
    logger->error("Error getting recommendations: {}", e.what());
    return create_error_response(500, e.what());
  }
}

HTTPResponse AnalyticsAPIHandlers::handle_record_feedback(const HTTPRequest& req) {
  auto logger = logging::get_logger("analytics_api");
  
  try {
    if (!req.body.contains("feedback_type") || !req.body.contains("entity_id")) {
      return create_error_response(400, "Missing required fields");
    }
    
    FeedbackEffectiveness feedback;
    feedback.feedback_type = req.body["feedback_type"].get<std::string>();
    feedback.entity_id = req.body["entity_id"].get<std::string>();
    feedback.improvement_score = req.body.value("improvement_score", 0);
    feedback.led_to_model_update = req.body.value("led_to_model_update", false);
    feedback.submitted_at = std::chrono::system_clock::now();
    
    bool success = learning_engine_->record_feedback(feedback);
    return create_success_response({{"recorded", success}});
  } catch (const std::exception& e) {
    logger->error("Error recording feedback: {}", e.what());
    return create_error_response(500, e.what());
  }
}

HTTPResponse AnalyticsAPIHandlers::handle_get_learning_stats(const HTTPRequest& req) {
  auto logger = logging::get_logger("analytics_api");
  
  try {
    int days = get_query_param_int(req, "days", 30);
    
    auto stats = learning_engine_->get_learning_stats(days);
    return create_success_response({
      {"total_feedback_items", stats.total_feedback_items},
      {"avg_feedback_effectiveness", stats.avg_feedback_effectiveness},
      {"total_cumulative_reward", stats.total_cumulative_reward},
      {"learning_converged", stats.learning_converged},
    });
  } catch (const std::exception& e) {
    logger->error("Error getting learning stats: {}", e.what());
    return create_error_response(500, e.what());
  }
}

HTTPResponse AnalyticsAPIHandlers::handle_get_system_analytics_dashboard(const HTTPRequest& req) {
  auto logger = logging::get_logger("analytics_api");
  
  try {
    int days = get_query_param_int(req, "days", 30);
    
    json dashboard;
    dashboard["decisions"] = decision_engine_->get_decision_stats(days);
    dashboard["rules"] = rule_engine_->get_rule_stats(days);
    dashboard["learning"] = learning_engine_->get_learning_stats(days);
    
    return create_success_response(dashboard);
  } catch (const std::exception& e) {
    logger->error("Error getting dashboard: {}", e.what());
    return create_error_response(500, e.what());
  }
}

HTTPResponse AnalyticsAPIHandlers::handle_get_health_check(const HTTPRequest& req) {
  return create_success_response({
    {"status", "healthy"},
    {"engines", {
      {"decision_analytics", true},
      {"rule_performance", true},
      {"learning_insights", true},
    }},
  });
}

// Helper methods
HTTPResponse AnalyticsAPIHandlers::create_error_response(
    int status_code,
    const std::string& message) {
  HTTPResponse response;
  response.status_code = status_code;
  response.body = {{"error", message}};
  return response;
}

HTTPResponse AnalyticsAPIHandlers::create_success_response(const json& data) {
  HTTPResponse response;
  response.status_code = 200;
  response.body = {{"success", true}, {"data", data}};
  return response;
}

std::string AnalyticsAPIHandlers::get_query_param(
    const HTTPRequest& req,
    const std::string& key,
    const std::string& default_value) {
  auto it = req.query_params.find(key);
  if (it != req.query_params.end()) {
    return it->second;
  }
  return default_value;
}

int AnalyticsAPIHandlers::get_query_param_int(
    const HTTPRequest& req,
    const std::string& key,
    int default_value) {
  std::string str_value = get_query_param(req, key, "");
  if (str_value.empty()) {
    return default_value;
  }
  try {
    return std::stoi(str_value);
  } catch (...) {
    return default_value;
  }
}

}  // namespace analytics
}  // namespace regulens
