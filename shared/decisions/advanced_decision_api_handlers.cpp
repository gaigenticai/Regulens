/**
 * Advanced Decision API Handlers Implementation
 */

#include "advanced_decision_api_handlers.hpp"

namespace regulens {
namespace decisions {

AdvancedDecisionAPIHandlers::AdvancedDecisionAPIHandlers(
    std::shared_ptr<AsyncMCDADecisionService> mcda_service,
    std::shared_ptr<resilience::ResilientEvaluatorWrapper> resilient_wrapper,
    std::shared_ptr<rules::AsyncLearningIntegrator> learning_integrator,
    std::shared_ptr<StructuredLogger> logger)
    : mcda_service_(mcda_service),
      resilient_wrapper_(resilient_wrapper),
      learning_integrator_(learning_integrator),
      logger_(logger) {
}

HTTPResponse AdvancedDecisionAPIHandlers::handle_analyze_decision(const HTTPRequest& req) {
    try {
        auto body = json::parse(req.body);
        
        // Extract parameters
        std::string decision_problem = body.value("decision_problem", "");
        std::vector<DecisionCriterion> criteria;
        std::vector<DecisionAlternative> alternatives;
        std::string algorithm_str = body.value("algorithm", "WEIGHTED_SUM");
        std::string execution_mode = body.value("execution_mode", "ASYNCHRONOUS");

        // Call resilient MCDA service
        auto result = resilient_wrapper_->analyze_decision_resilient(
            decision_problem,
            criteria,
            alternatives,
            MCDAAlgorithm::WEIGHTED_SUM,
            execution_mode
        );

        return create_response(200, result);
    } catch (const std::exception& e) {
        logger_->error("Error in analyze_decision: {}", e.what());
        return create_error_response(400, e.what());
    }
}

HTTPResponse AdvancedDecisionAPIHandlers::handle_analyze_ensemble(const HTTPRequest& req) {
    try {
        auto body = json::parse(req.body);
        
        std::string decision_problem = body.value("decision_problem", "");
        std::vector<DecisionCriterion> criteria;
        std::vector<DecisionAlternative> alternatives;
        std::vector<MCDAAlgorithm> algorithms = {
            MCDAAlgorithm::WEIGHTED_SUM,
            MCDAAlgorithm::TOPSIS,
            MCDAAlgorithm::AHP
        };

        auto result = resilient_wrapper_->analyze_decision_ensemble_resilient(
            decision_problem,
            criteria,
            alternatives,
            algorithms
        );

        return create_response(200, result);
    } catch (const std::exception& e) {
        return create_error_response(400, e.what());
    }
}

HTTPResponse AdvancedDecisionAPIHandlers::handle_get_algorithms(const HTTPRequest& req) {
    try {
        auto algorithms = mcda_service_->get_available_algorithms();
        return create_response(200, json::object({{"algorithms", algorithms}}));
    } catch (const std::exception& e) {
        return create_error_response(500, e.what());
    }
}

HTTPResponse AdvancedDecisionAPIHandlers::handle_get_analysis_status(const HTTPRequest& req, const std::string& analysis_id) {
    try {
        auto status = mcda_service_->get_analysis_status(analysis_id);
        return create_response(200, status);
    } catch (const std::exception& e) {
        return create_error_response(404, "Analysis not found");
    }
}

HTTPResponse AdvancedDecisionAPIHandlers::handle_submit_feedback(const HTTPRequest& req, const std::string& analysis_id) {
    try {
        auto body = json::parse(req.body);
        
        json actual_outcome = body.value("actual_outcome", json::object());
        double confidence = body.value("confidence", 0.5);

        auto result = learning_integrator_->submit_decision_feedback(
            analysis_id,
            json::object(),
            actual_outcome,
            confidence
        );

        return create_response(200, result);
    } catch (const std::exception& e) {
        return create_error_response(400, e.what());
    }
}

HTTPResponse AdvancedDecisionAPIHandlers::handle_health_check(const HTTPRequest& req) {
    try {
        auto health = resilient_wrapper_->get_all_services_health();
        return create_response(200, health);
    } catch (const std::exception& e) {
        return create_error_response(500, e.what());
    }
}

HTTPResponse AdvancedDecisionAPIHandlers::handle_system_metrics(const HTTPRequest& req) {
    try {
        auto metrics = json::object({
            {"mcda_metrics", mcda_service_->get_system_metrics()},
            {"resilience_metrics", resilient_wrapper_->get_resilience_metrics()},
            {"learning_stats", learning_integrator_->get_learning_statistics()}
        });
        return create_response(200, metrics);
    } catch (const std::exception& e) {
        return create_error_response(500, e.what());
    }
}

HTTPResponse AdvancedDecisionAPIHandlers::create_response(int code, const json& data) {
    return {code, "OK", data.dump(), "application/json"};
}

HTTPResponse AdvancedDecisionAPIHandlers::create_error_response(int code, const std::string& message) {
    return {code, "Error", json::object({{"error", message}}).dump(), "application/json"};
}

} // namespace decisions
} // namespace regulens
