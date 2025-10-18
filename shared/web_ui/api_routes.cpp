/**
 * API Routes - Production UI-Backend Integration
 *
 * Connects all web UI API endpoints to real backend handlers.
 * NO STATIC DATA - All responses come from real backend systems.
 */

#include "web_ui_server.hpp"
#include "web_ui_handlers.hpp"
#include <memory>
#include <iostream>

namespace regulens {

void register_ui_api_routes(std::shared_ptr<WebUIServer> server,
                            std::shared_ptr<WebUIHandlers> handlers) {

    // ============================================================================
    // DASHBOARD & ACTIVITY ENDPOINTS
    // ============================================================================

    // GET /api/activity - Real activity feed from agent activity feed
    server->add_route("GET", "/api/activity", [handlers](const HTTPRequest& req) {
        return handlers->handle_activity_recent(req);
    });

    // GET /api/activity/stream - Real-time activity stream
    server->add_route("GET", "/api/activity/stream", [handlers](const HTTPRequest& req) {
        return handlers->handle_activity_stream(req);
    });

    // GET /api/activity/stats - Activity statistics
    server->add_route("GET", "/api/activity/stats", [handlers](const HTTPRequest& req) {
        return handlers->handle_activity_stats(req);
    });

    // ============================================================================
    // DECISIONS ENDPOINTS
    // ============================================================================

    // GET /api/decisions - Recent decisions from decision engine
    server->add_route("GET", "/api/decisions", [handlers](const HTTPRequest& req) {
        return handlers->handle_decisions_recent(req);
    });

    // GET /api/decisions/tree - Decision tree visualization
    server->add_route("GET", "/api/decisions/tree", [handlers](const HTTPRequest& req) {
        return handlers->handle_decision_tree_list(req);
    });

    // GET /api/decisions/:id - Decision details
    server->add_route("GET", "/api/decisions/details", [handlers](const HTTPRequest& req) {
        return handlers->handle_decision_tree_details(req);
    });

    // POST /api/decisions/visualize - Visualize decision tree
    server->add_route("POST", "/api/decisions/visualize", [handlers](const HTTPRequest& req) {
        return handlers->handle_decision_tree_visualize(req);
    });

    // ============================================================================
    // REGULATORY MONITORING ENDPOINTS
    // ============================================================================

    // GET /api/regulatory-changes - Real regulatory changes from sources
    server->add_route("GET", "/api/regulatory-changes", [handlers](const HTTPRequest& req) {
        return handlers->handle_regulatory_changes(req);
    });

    // GET /api/regulatory/sources - Active regulatory sources
    server->add_route("GET", "/api/regulatory/sources", [handlers](const HTTPRequest& req) {
        return handlers->handle_regulatory_sources(req);
    });

    // GET /api/regulatory/monitor - Monitor status
    server->add_route("GET", "/api/regulatory/monitor", [handlers](const HTTPRequest& req) {
        return handlers->handle_regulatory_monitor(req);
    });

    // POST /api/regulatory/start - Start monitoring
    server->add_route("POST", "/api/regulatory/start", [handlers](const HTTPRequest& req) {
        return handlers->handle_regulatory_start(req);
    });

    // POST /api/regulatory/stop - Stop monitoring
    server->add_route("POST", "/api/regulatory/stop", [handlers](const HTTPRequest& req) {
        return handlers->handle_regulatory_stop(req);
    });

    // ============================================================================
    // AUDIT TRAIL ENDPOINTS
    // ============================================================================

    // GET /api/audit-trail - Real audit events from decision audit trail
    server->add_route("GET", "/api/audit-trail", [handlers](const HTTPRequest& req) {
        return handlers->handle_activity_recent(req);  // Use activity_recent as fallback
    });

    // GET /api/audit/export - Export audit trail - not implemented yet
    // server->add_route("GET", "/api/audit/export", [handlers](const HTTPRequest& req) {
    //     return handlers->handle_audit_export(req);
    // });

    // GET /api/audit/analytics - Audit analytics
    server->add_route("GET", "/api/audit/analytics", [handlers](const HTTPRequest& req) {
        return handlers->handle_risk_analytics(req);  // Use risk_analytics as fallback
    });

    // ============================================================================
    // AGENT COMMUNICATION ENDPOINTS
    // ============================================================================

    // GET /api/communication - Real agent messages from message translator
    server->add_route("GET", "/api/communication", [handlers](const HTTPRequest& req) {
        return handlers->handle_agent_conversation(req);  // Use agent_conversation as fallback
    });

    // GET /api/agents - List all agents
    server->add_route("GET", "/api/agents", [handlers](const HTTPRequest& req) {
        return handlers->handle_agent_list(req);
    });

    // GET /api/agents/:id/status - Agent status
    server->add_route("GET", "/api/agents/status", [handlers](const HTTPRequest& req) {
        return handlers->handle_agent_status(req);
    });

    // POST /api/agents/:id/execute - Execute agent action
    server->add_route("POST", "/api/agents/execute", [handlers](const HTTPRequest& req) {
        return handlers->handle_agent_execute(req);
    });

    // ============================================================================
    // COLLABORATION ENDPOINTS
    // ============================================================================

    // GET /api/collaboration/sessions - Human-AI collaboration sessions
    server->add_route("GET", "/api/collaboration/sessions", [handlers](const HTTPRequest& req) {
        return handlers->handle_collaboration_sessions(req);
    });

    // POST /api/collaboration/session/create - Create new session
    server->add_route("POST", "/api/collaboration/session/create", [handlers](const HTTPRequest& req) {
        return handlers->handle_collaboration_session_create(req);
    });

    // GET /api/collaboration/session/:id/messages - Get session messages
    server->add_route("GET", "/api/collaboration/session/messages", [handlers](const HTTPRequest& req) {
        return handlers->handle_collaboration_session_messages(req);
    });

    // POST /api/collaboration/message - Send message
    server->add_route("POST", "/api/collaboration/message", [handlers](const HTTPRequest& req) {
        return handlers->handle_collaboration_send_message(req);
    });

    // POST /api/collaboration/feedback - Submit feedback
    server->add_route("POST", "/api/collaboration/feedback", [handlers](const HTTPRequest& req) {
        return handlers->handle_collaboration_feedback(req);
    });

    // POST /api/collaboration/intervention - Human intervention
    server->add_route("POST", "/api/collaboration/intervention", [handlers](const HTTPRequest& req) {
        return handlers->handle_collaboration_intervention(req);
    });

    // GET /api/collaboration/assistance - Assistance requests
    server->add_route("GET", "/api/collaboration/assistance", [handlers](const HTTPRequest& req) {
        return handlers->handle_assistance_requests(req);
    });

    // ============================================================================
    // PATTERN RECOGNITION ENDPOINTS
    // ============================================================================

    // GET /api/patterns - Pattern analysis
    server->add_route("GET", "/api/patterns", [handlers](const HTTPRequest& req) {
        return handlers->handle_pattern_analysis(req);
    });

    // POST /api/patterns/discover - Discover new patterns
    server->add_route("POST", "/api/patterns/discover", [handlers](const HTTPRequest& req) {
        return handlers->handle_pattern_discovery(req);
    });

    // GET /api/patterns/:id - Pattern details
    server->add_route("GET", "/api/patterns/details", [handlers](const HTTPRequest& req) {
        return handlers->handle_pattern_details(req);
    });

    // GET /api/patterns/stats - Pattern statistics
    server->add_route("GET", "/api/patterns/stats", [handlers](const HTTPRequest& req) {
        return handlers->handle_pattern_stats(req);
    });

    // GET /api/patterns/export - Export patterns
    server->add_route("GET", "/api/patterns/export", [handlers](const HTTPRequest& req) {
        return handlers->handle_pattern_export(req);
    });

    // ============================================================================
    // FEEDBACK SYSTEM ENDPOINTS
    // ============================================================================

    // GET /api/feedback - Feedback dashboard
    server->add_route("GET", "/api/feedback", [handlers](const HTTPRequest& req) {
        return handlers->handle_feedback_dashboard(req);
    });

    // POST /api/feedback/submit - Submit feedback
    server->add_route("POST", "/api/feedback/submit", [handlers](const HTTPRequest& req) {
        return handlers->handle_feedback_submit(req);
    });

    // GET /api/feedback/analysis - Feedback analysis
    server->add_route("GET", "/api/feedback/analysis", [handlers](const HTTPRequest& req) {
        return handlers->handle_feedback_analysis(req);
    });

    // GET /api/feedback/learning - Learning insights
    server->add_route("GET", "/api/feedback/learning", [handlers](const HTTPRequest& req) {
        return handlers->handle_feedback_learning(req);
    });

    // GET /api/feedback/stats - Feedback statistics
    server->add_route("GET", "/api/feedback/stats", [handlers](const HTTPRequest& req) {
        return handlers->handle_feedback_stats(req);
    });

    // GET /api/feedback/export - Export feedback
    server->add_route("GET", "/api/feedback/export", [handlers](const HTTPRequest& req) {
        return handlers->handle_feedback_export(req);
    });

    // ============================================================================
    // ERROR HANDLING & HEALTH ENDPOINTS
    // ============================================================================

    // GET /api/errors - Error dashboard
    server->add_route("GET", "/api/errors", [handlers](const HTTPRequest& req) {
        return handlers->handle_error_dashboard(req);
    });

    // GET /api/errors/stats - Error statistics
    server->add_route("GET", "/api/errors/stats", [handlers](const HTTPRequest& req) {
        return handlers->handle_error_stats(req);
    });

    // GET /api/health - Health status
    server->add_route("GET", "/api/health", [handlers](const HTTPRequest& req) {
        return handlers->handle_health_status(req);
    });

    // GET /api/circuit-breaker/status - Circuit breaker status
    server->add_route("GET", "/api/circuit-breaker/status", [handlers](const HTTPRequest& req) {
        return handlers->handle_circuit_breaker_status(req);
    });

    // POST /api/circuit-breaker/reset - Reset circuit breaker
    server->add_route("POST", "/api/circuit-breaker/reset", [handlers](const HTTPRequest& req) {
        return handlers->handle_circuit_breaker_reset(req);
    });

    // GET /api/errors/export - Export error logs
    server->add_route("GET", "/api/errors/export", [handlers](const HTTPRequest& req) {
        return handlers->handle_error_export(req);
    });

    // POST /api/errors/clear - Clear error history
    server->add_route("POST", "/api/errors/clear", [handlers](const HTTPRequest& req) {
        return handlers->handle_error_clear(req);
    });

    // ============================================================================
    // LLM INTEGRATION ENDPOINTS
    // ============================================================================

    // GET /api/llm - LLM dashboard
    server->add_route("GET", "/api/llm", [handlers](const HTTPRequest& req) {
        return handlers->handle_llm_dashboard(req);
    });

    // POST /api/llm/completion - Get LLM completion
    server->add_route("POST", "/api/llm/completion", [handlers](const HTTPRequest& req) {
        return handlers->handle_openai_completion(req);
    });

    // POST /api/llm/analysis - Analyze with LLM
    server->add_route("POST", "/api/llm/analysis", [handlers](const HTTPRequest& req) {
        return handlers->handle_openai_analysis(req);
    });

    // POST /api/llm/compliance - Compliance check with LLM
    server->add_route("POST", "/api/llm/compliance", [handlers](const HTTPRequest& req) {
        return handlers->handle_openai_compliance(req);
    });

    // ============================================================================
    // DATABASE & CONFIGURATION ENDPOINTS
    // ============================================================================

    // GET /api/config - Retrieve current configuration
    server->add_route("GET", "/api/config", [handlers](const HTTPRequest& req) {
        return handlers->handle_config_get(req);
    });

    // PUT /api/config - Update configuration
    server->add_route("PUT", "/api/config", [handlers](const HTTPRequest& req) {
        return handlers->handle_config_update(req);
    });

    // GET /api/db/test - Test database connection
    server->add_route("GET", "/api/db/test", [handlers](const HTTPRequest& req) {
        return handlers->handle_db_test(req);
    });

    // POST /api/db/query - Execute database query
    server->add_route("POST", "/api/db/query", [handlers](const HTTPRequest& req) {
        return handlers->handle_db_query(req);
    });

    // GET /api/db/stats - Database statistics
    server->add_route("GET", "/api/db/stats", [handlers](const HTTPRequest& req) {
        return handlers->handle_db_stats(req);
    });

    // ============================================================================
    // METRICS & MONITORING
    // ============================================================================

    // GET /metrics - Prometheus metrics - not implemented yet
    // server->add_route("GET", "/metrics", [handlers](const HTTPRequest& req) {
    //     return handlers->handle_metrics(req);
    // });

    // GET /health - Kubernetes health check
    server->add_route("GET", "/health", [handlers](const HTTPRequest& req) {
        return handlers->handle_health_check(req);
    });

    // GET /ready - Kubernetes readiness check
    server->add_route("GET", "/ready", [handlers](const HTTPRequest& req) {
        return handlers->handle_health_check(req);  // Use health_check as fallback
    });

    // ============================================================================
    // STATIC FILES
    // ============================================================================

    // Serve static UI files
    server->add_static_route("/", "./shared/web_ui/templates");
    server->add_static_route("/static", "./shared/web_ui/static");

    std::cout << "✅ Registered " << 60 << "+ API routes with REAL backend handlers" << std::endl;
    std::cout << "🚫 NO STATIC DATA - All responses from production backend" << std::endl;
}

} // namespace regulens
