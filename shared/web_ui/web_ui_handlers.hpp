/**
 * Web UI Handlers - Feature Testing Interfaces
 *
 * Production-grade request handlers for testing all Regulens features
 * via professional web UI as required by Rule 6.
 *
 * Provides REST APIs and HTML dashboards for:
 * - Configuration management testing
 * - Database connectivity testing
 * - Agent orchestration testing
 * - Regulatory monitoring testing
 * - Metrics and monitoring testing
 * - Data ingestion testing
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include "web_ui_server.hpp"
#include "../config/configuration_manager.hpp"
#include "../logging/structured_logger.hpp"
#include "../metrics/metrics_collector.hpp"
#include "../database/postgresql_connection.hpp"
#include "../visualization/decision_tree_visualizer.hpp"
#include "../agent_activity_feed.hpp"
#include "../human_ai_collaboration.hpp"
#include "../pattern_recognition.hpp"
#include "../feedback_incorporation.hpp"
#include "../error_handler.hpp"
#include "../llm/openai_client.hpp"
#include "../llm/anthropic_client.hpp"
#include "../risk_assessment.hpp"
#include "../decision_tree_optimizer.hpp"

namespace regulens {

/**
 * @brief Web UI handlers for feature testing
 *
 * Implements all the request handlers needed for comprehensive
 * testing of Regulens features via web interface.
 */
class WebUIHandlers {
public:
    WebUIHandlers(std::shared_ptr<ConfigurationManager> config,
                  std::shared_ptr<StructuredLogger> logger,
                  std::shared_ptr<MetricsCollector> metrics);

    // Configuration testing handlers
    HTTPResponse handle_config_get(const HTTPRequest& request);
    HTTPResponse handle_config_update(const HTTPRequest& request);

    // Database testing handlers
    HTTPResponse handle_db_test(const HTTPRequest& request);
    HTTPResponse handle_db_query(const HTTPRequest& request);
    HTTPResponse handle_db_stats(const HTTPRequest& request);

    // Agent testing handlers
    HTTPResponse handle_agent_status(const HTTPRequest& request);
    HTTPResponse handle_agent_execute(const HTTPRequest& request);
    HTTPResponse handle_agent_list(const HTTPRequest& request);

    // Regulatory monitoring handlers
    HTTPResponse handle_regulatory_sources(const HTTPRequest& request);
    HTTPResponse handle_regulatory_changes(const HTTPRequest& request);
    HTTPResponse handle_regulatory_monitor(const HTTPRequest& request);

    // Decision tree visualization handlers
    HTTPResponse handle_decision_tree_visualize(const HTTPRequest& request);
    HTTPResponse handle_decision_tree_list(const HTTPRequest& request);
    HTTPResponse handle_decision_tree_details(const HTTPRequest& request);

    // Agent activity feed handlers
    HTTPResponse handle_activity_feed(const HTTPRequest& request);
    HTTPResponse handle_activity_stream(const HTTPRequest& request);
    HTTPResponse handle_activity_query(const HTTPRequest& request);
    HTTPResponse handle_activity_stats(const HTTPRequest& request);

    // Human-AI collaboration handlers
    HTTPResponse handle_collaboration_sessions(const HTTPRequest& request);
    HTTPResponse handle_collaboration_session_create(const HTTPRequest& request);
    HTTPResponse handle_collaboration_session_messages(const HTTPRequest& request);
    HTTPResponse handle_collaboration_send_message(const HTTPRequest& request);
    HTTPResponse handle_collaboration_feedback(const HTTPRequest& request);
    HTTPResponse handle_collaboration_intervention(const HTTPRequest& request);
    HTTPResponse handle_assistance_requests(const HTTPRequest& request);

    // Pattern recognition handlers
    HTTPResponse handle_pattern_analysis(const HTTPRequest& request);
    HTTPResponse handle_pattern_discovery(const HTTPRequest& request);
    HTTPResponse handle_pattern_details(const HTTPRequest& request);
    HTTPResponse handle_pattern_stats(const HTTPRequest& request);
    HTTPResponse handle_pattern_export(const HTTPRequest& request);

    // Feedback incorporation handlers
    HTTPResponse handle_feedback_dashboard(const HTTPRequest& request);
    HTTPResponse handle_feedback_submit(const HTTPRequest& request);
    HTTPResponse handle_feedback_analysis(const HTTPRequest& request);
    HTTPResponse handle_feedback_learning(const HTTPRequest& request);
    HTTPResponse handle_feedback_stats(const HTTPRequest& request);
    HTTPResponse handle_feedback_export(const HTTPRequest& request);

    // Error handling and monitoring handlers
    HTTPResponse handle_error_dashboard(const HTTPRequest& request);
    HTTPResponse handle_error_stats(const HTTPRequest& request);
    HTTPResponse handle_health_status(const HTTPRequest& request);
    HTTPResponse handle_circuit_breaker_status(const HTTPRequest& request);
    HTTPResponse handle_circuit_breaker_reset(const HTTPRequest& request);
    HTTPResponse handle_error_export(const HTTPRequest& request);

    // LLM and OpenAI handlers
    HTTPResponse handle_llm_dashboard(const HTTPRequest& request);
    HTTPResponse handle_openai_completion(const HTTPRequest& request);
    HTTPResponse handle_openai_analysis(const HTTPRequest& request);
    HTTPResponse handle_openai_compliance(const HTTPRequest& request);
    HTTPResponse handle_openai_extraction(const HTTPRequest& request);
    HTTPResponse handle_openai_decision(const HTTPRequest& request);
    HTTPResponse handle_openai_stats(const HTTPRequest& request);

    // Anthropic Claude handlers
    HTTPResponse handle_claude_dashboard(const HTTPRequest& request);
    HTTPResponse handle_claude_message(const HTTPRequest& request);
    HTTPResponse handle_claude_reasoning(const HTTPRequest& request);
    HTTPResponse handle_claude_constitutional(const HTTPRequest& request);
    HTTPResponse handle_claude_ethical_decision(const HTTPRequest& request);
    HTTPResponse handle_claude_complex_reasoning(const HTTPRequest& request);
    HTTPResponse handle_claude_regulatory(const HTTPRequest& request);
    HTTPResponse handle_claude_stats(const HTTPRequest& request);

    // Decision Tree Optimizer handlers
    HTTPResponse handle_decision_dashboard(const HTTPRequest& request);
    HTTPResponse handle_decision_mcda_analysis(const HTTPRequest& request);
    HTTPResponse handle_decision_tree_analysis(const HTTPRequest& request);
    HTTPResponse handle_decision_ai_recommendation(const HTTPRequest& request);
    HTTPResponse handle_decision_history(const HTTPRequest& request);
    HTTPResponse handle_decision_visualization(const HTTPRequest& request);

    // Risk Assessment handlers
    HTTPResponse handle_risk_dashboard(const HTTPRequest& request);
    HTTPResponse handle_risk_assess_transaction(const HTTPRequest& request);
    HTTPResponse handle_risk_assess_entity(const HTTPRequest& request);
    HTTPResponse handle_risk_assess_regulatory(const HTTPRequest& request);
    HTTPResponse handle_risk_history(const HTTPRequest& request);
    HTTPResponse handle_risk_analytics(const HTTPRequest& request);
    HTTPResponse handle_risk_export(const HTTPRequest& request);

    // Metrics and monitoring handlers
    HTTPResponse handle_metrics_dashboard(const HTTPRequest& request);
    HTTPResponse handle_metrics_data(const HTTPRequest& request);
    HTTPResponse handle_health_check(const HTTPRequest& request);

    // Data ingestion handlers
    HTTPResponse handle_ingestion_status(const HTTPRequest& request);
    HTTPResponse handle_ingestion_test(const HTTPRequest& request);
    HTTPResponse handle_ingestion_stats(const HTTPRequest& request);

    // Main dashboard
    HTTPResponse handle_dashboard(const HTTPRequest& request);
    HTTPResponse handle_api_docs(const HTTPRequest& request);

private:
    std::shared_ptr<ConfigurationManager> config_manager_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<MetricsCollector> metrics_collector_;

    // Decision tree visualization
    std::shared_ptr<DecisionTreeVisualizer> decision_tree_visualizer_;

    // Agent activity feed
    std::shared_ptr<AgentActivityFeed> activity_feed_;

    // Human-AI collaboration
    std::shared_ptr<HumanAICollaboration> collaboration_;

    // Pattern recognition
    std::shared_ptr<PatternRecognitionEngine> pattern_recognition_;

    // Feedback incorporation
    std::shared_ptr<FeedbackIncorporationSystem> feedback_system_;

    // Error handling and recovery
    std::shared_ptr<ErrorHandler> error_handler_;

    // LLM and OpenAI integration
    std::shared_ptr<OpenAIClient> openai_client_;

    // Anthropic Claude integration
    std::shared_ptr<AnthropicClient> anthropic_client_;

    // Risk Assessment
    std::shared_ptr<RiskAssessmentEngine> risk_assessment_;

    // Decision Tree Optimization
    std::shared_ptr<DecisionTreeOptimizer> decision_optimizer_;

    // Database connection for testing
    std::shared_ptr<PostgreSQLConnection> db_connection_;
    std::shared_ptr<ConnectionPool> db_pool_;

    // HTML templates
    std::string generate_dashboard_html() const;
    std::string generate_config_html() const;
    std::string generate_database_html() const;
    std::string generate_agents_html() const;
    std::string generate_monitoring_html() const;
    std::string generate_decision_trees_html() const;
    std::string generate_activity_feed_html() const;
    std::string generate_collaboration_html() const;
    std::string generate_pattern_analysis_html() const;
    std::string generate_feedback_dashboard_html() const;
    std::string generate_error_dashboard_html() const;
    std::string generate_llm_dashboard_html() const;
    std::string generate_claude_dashboard_html() const;
    std::string generate_decision_dashboard_html() const;
    std::string generate_risk_dashboard_html() const;
    std::string generate_ingestion_html() const;
    std::string generate_api_docs_html() const;

    // JSON response helpers
    std::string generate_config_json() const;
    std::string generate_metrics_json() const;
    std::string generate_health_json() const;

    // Utility methods
    HTTPResponse create_json_response(const std::string& json_data);
    HTTPResponse create_html_response(const std::string& html_content);
    HTTPResponse create_error_response(int code, const std::string& message);
    bool validate_request(const HTTPRequest& request);
    std::unordered_map<std::string, std::string> parse_form_data(const std::string& body);
    std::string escape_html(const std::string& input) const;
};

} // namespace regulens
