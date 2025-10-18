/**
 * Advanced Decision API Handlers - REST Endpoints for Week 2 Features
 */

#pragma once

#include <string>
#include <memory>
#include <map>
#include <nlohmann/json.hpp>
#include "async_mcda_decision_service.hpp"
#include "../resilience/resilient_evaluator_wrapper.hpp"
#include "../rules/async_learning_integrator.hpp"
#include "../logging/structured_logger.hpp"

using json = nlohmann::json;

namespace regulens {
namespace decisions {

struct HTTPRequest {
    std::string method;
    std::string path;
    std::string body;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> query_params;
};

struct HTTPResponse {
    int status_code;
    std::string status_message;
    std::string body;
    std::string content_type;
};

class AdvancedDecisionAPIHandlers {
public:
    AdvancedDecisionAPIHandlers(
        std::shared_ptr<AsyncMCDADecisionService> mcda_service,
        std::shared_ptr<resilience::ResilientEvaluatorWrapper> resilient_wrapper,
        std::shared_ptr<rules::AsyncLearningIntegrator> learning_integrator,
        std::shared_ptr<StructuredLogger> logger
    );

    /**
     * POST /api/decisions/analyze - Resilient MCDA analysis
     */
    HTTPResponse handle_analyze_decision(const HTTPRequest& req);

    /**
     * POST /api/decisions/analyze-ensemble - Compare multiple algorithms
     */
    HTTPResponse handle_analyze_ensemble(const HTTPRequest& req);

    /**
     * GET /api/decisions/algorithms - List available algorithms
     */
    HTTPResponse handle_get_algorithms(const HTTPRequest& req);

    /**
     * GET /api/decisions/{id}/status - Get analysis status
     */
    HTTPResponse handle_get_analysis_status(const HTTPRequest& req, const std::string& analysis_id);

    /**
     * POST /api/decisions/{id}/feedback - Submit feedback for learning
     */
    HTTPResponse handle_submit_feedback(const HTTPRequest& req, const std::string& analysis_id);

    /**
     * GET /api/services/health - System health check
     */
    HTTPResponse handle_health_check(const HTTPRequest& req);

    /**
     * GET /api/services/metrics - System metrics
     */
    HTTPResponse handle_system_metrics(const HTTPRequest& req);

private:
    std::shared_ptr<AsyncMCDADecisionService> mcda_service_;
    std::shared_ptr<resilience::ResilientEvaluatorWrapper> resilient_wrapper_;
    std::shared_ptr<rules::AsyncLearningIntegrator> learning_integrator_;
    std::shared_ptr<StructuredLogger> logger_;

    HTTPResponse create_response(int code, const json& data);
    HTTPResponse create_error_response(int code, const std::string& message);
};

} // namespace decisions
} // namespace regulens

#endif
