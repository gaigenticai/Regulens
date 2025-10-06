#pragma once

#include <memory>

namespace regulens {

class WebUIServer;
class WebUIHandlers;

/**
 * Register all API routes for production UI-backend integration
 *
 * This function connects the web UI's API endpoints to real backend handlers.
 * NO STATIC/DEMO DATA - all responses come from actual backend systems.
 *
 * Routes registered:
 * - /api/activity - Activity feed
 * - /api/decisions - Decision history
 * - /api/regulatory-changes - Regulatory updates
 * - /api/audit-trail - Audit events
 * - /api/communication - Agent messages
 * - /api/agents - Agent management
 * - /api/collaboration - Human-AI collaboration
 * - /api/patterns - Pattern recognition
 * - /api/feedback - Feedback system
 * - /api/errors - Error dashboard
 * - /api/health - Health checks
 * - /api/llm - LLM integration
 * - /api/config - Configuration
 * - /api/db - Database operations
 * - /metrics - Prometheus metrics
 *
 * @param server The web UI server instance
 * @param handlers The web UI handlers instance with real backend logic
 */
void register_ui_api_routes(std::shared_ptr<WebUIServer> server,
                            std::shared_ptr<WebUIHandlers> handlers);

} // namespace regulens
