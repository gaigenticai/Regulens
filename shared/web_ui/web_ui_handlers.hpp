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
#include <deque>
#include <mutex>
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
#include "health_handlers.hpp"
#include "../llm/openai_client.hpp"
#include "../llm/anthropic_client.hpp"
#include "../llm/embeddings_client.hpp"
#include "../memory/conversation_memory.hpp"
#include "../memory/learning_engine.hpp"
#include "../memory/case_based_reasoning.hpp"
#include "../memory/memory_manager.hpp"
#include "../risk_assessment.hpp"
#include "../decision_tree_optimizer.hpp"
#include "../../core/agent/agent_communication.hpp"
#include "../../core/agent/message_translator.hpp"
#include "../../core/agent/consensus_engine.hpp"
#include "../../agents/real_agent.hpp"
#include "../knowledge_base.hpp"

namespace regulens {

// Forward declarations
class AgentOrchestrator;

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
    HTTPResponse handle_regulatory_start(const HTTPRequest& request);
    HTTPResponse handle_regulatory_stop(const HTTPRequest& request);

    // Decision tree visualization handlers
    HTTPResponse handle_decision_tree_visualize(const HTTPRequest& request);
    HTTPResponse handle_decision_tree_list(const HTTPRequest& request);
    HTTPResponse handle_decision_tree_details(const HTTPRequest& request);

    // Agent activity feed handlers
    HTTPResponse handle_activity_feed(const HTTPRequest& request);
    HTTPResponse handle_activity_stream(const HTTPRequest& request);
    HTTPResponse handle_activity_query(const HTTPRequest& request);
    HTTPResponse handle_activity_stats(const HTTPRequest& request);
    HTTPResponse handle_activity_recent(const HTTPRequest& request);
    HTTPResponse handle_decisions_recent(const HTTPRequest& request);

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

    // Function calling handlers
    HTTPResponse handle_function_calling_dashboard(const HTTPRequest& request);
    HTTPResponse handle_function_execute(const HTTPRequest& request);
    HTTPResponse handle_function_list(const HTTPRequest& request);
    HTTPResponse handle_function_audit(const HTTPRequest& request);
    HTTPResponse handle_function_metrics(const HTTPRequest& request);
    HTTPResponse handle_function_openai_integration(const HTTPRequest& request);

    // Embeddings handlers
    HTTPResponse handle_embeddings_dashboard(const HTTPRequest& request);
    HTTPResponse handle_embeddings_generate(const HTTPRequest& request);
    HTTPResponse handle_embeddings_search(const HTTPRequest& request);
    HTTPResponse handle_embeddings_index(const HTTPRequest& request);
    HTTPResponse handle_embeddings_models(const HTTPRequest& request);
    HTTPResponse handle_embeddings_stats(const HTTPRequest& request);

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

    // Multi-Agent Communication handlers
    HTTPResponse handle_multi_agent_dashboard(const HTTPRequest& request);
    HTTPResponse handle_agent_message_send(const HTTPRequest& request);
    HTTPResponse handle_agent_message_receive(const HTTPRequest& request);
    HTTPResponse handle_agent_message_broadcast(const HTTPRequest& request);
    HTTPResponse handle_consensus_start(const HTTPRequest& request);
    HTTPResponse handle_consensus_contribute(const HTTPRequest& request);
    HTTPResponse handle_consensus_result(const HTTPRequest& request);
    HTTPResponse handle_message_translate(const HTTPRequest& request);
    HTTPResponse handle_agent_conversation(const HTTPRequest& request);
    HTTPResponse handle_conflict_resolution(const HTTPRequest& request);
    HTTPResponse handle_communication_stats(const HTTPRequest& request);

    // Metrics and monitoring handlers
    HTTPResponse handle_metrics_dashboard(const HTTPRequest& request);
    HTTPResponse handle_metrics_data(const HTTPRequest& request);
    HTTPResponse handle_health_check(const HTTPRequest& request);
    HTTPResponse handle_detailed_health_report(const HTTPRequest& request);

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

    // Regulatory monitoring
    std::shared_ptr<RealRegulatoryFetcher> regulatory_fetcher_;

    // LLM and OpenAI integration
    std::shared_ptr<OpenAIClient> openai_client_;

    // Anthropic Claude integration
    std::shared_ptr<AnthropicClient> anthropic_client_;

    // Function Calling integration
    std::shared_ptr<FunctionRegistry> function_registry_;
    std::shared_ptr<FunctionDispatcher> function_dispatcher_;

    // Recent function calls tracking
    struct RecentFunctionCall {
        std::string function_name;
        std::chrono::system_clock::time_point timestamp;
        bool success;
        double response_time_ms;
        std::string user_agent;
        std::string correlation_id;
    };

    std::deque<RecentFunctionCall> recent_calls_;
    mutable std::mutex recent_calls_mutex_;
    static constexpr size_t MAX_RECENT_CALLS = 1000;

    // Embeddings integration
    std::shared_ptr<EmbeddingsClient> embeddings_client_;
    std::shared_ptr<DocumentProcessor> document_processor_;
    std::shared_ptr<SemanticSearchEngine> semantic_search_engine_;

    // Risk Assessment
    std::shared_ptr<RiskAssessmentEngine> risk_assessment_;

    // Decision Tree Optimization
    std::shared_ptr<DecisionTreeOptimizer> decision_optimizer_;

    // Multi-Agent Communication System
    std::shared_ptr<AgentRegistry> agent_registry_;
    std::shared_ptr<InterAgentCommunicator> inter_agent_communicator_;
    std::shared_ptr<IntelligentMessageTranslator> message_translator_;
    std::shared_ptr<ConsensusEngine> consensus_engine_;
    std::shared_ptr<CommunicationMediator> communication_mediator_;

    // Memory System components
    std::shared_ptr<ConversationMemory> conversation_memory_;
    std::shared_ptr<LearningEngine> learning_engine_;
    std::shared_ptr<CaseBasedReasoner> case_based_reasoning_;
    std::shared_ptr<MemoryManager> memory_manager_;

    // Knowledge base
    std::shared_ptr<KnowledgeBase> knowledge_base_;

    // Database connection for testing
    std::shared_ptr<PostgreSQLConnection> db_connection_;
    std::shared_ptr<ConnectionPool> db_pool_;

    // Health check handler for Kubernetes probes
    std::shared_ptr<HealthCheckHandler> health_check_handler_;

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
    std::string generate_function_calling_html() const;
    std::string generate_embeddings_html() const;
    std::string generate_decision_dashboard_html() const;
    std::string generate_risk_dashboard_html() const;
       std::string generate_multi_agent_html() const;

       // Memory System UI handlers
       HTTPResponse handle_memory_dashboard(const HTTPRequest& request);
       HTTPResponse handle_memory_conversation_store(const HTTPRequest& request);
       HTTPResponse handle_memory_conversation_retrieve(const HTTPRequest& request);
       HTTPResponse handle_memory_conversation_search(const HTTPRequest& request);
       HTTPResponse handle_memory_conversation_delete(const HTTPRequest& request);
       HTTPResponse handle_memory_case_store(const HTTPRequest& request);
       HTTPResponse handle_memory_case_retrieve(const HTTPRequest& request);
       HTTPResponse handle_memory_case_search(const HTTPRequest& request);
       HTTPResponse handle_memory_case_delete(const HTTPRequest& request);
       HTTPResponse handle_memory_feedback_store(const HTTPRequest& request);
       HTTPResponse handle_memory_feedback_retrieve(const HTTPRequest& request);
       HTTPResponse handle_memory_feedback_search(const HTTPRequest& request);
       HTTPResponse handle_memory_learning_models(const HTTPRequest& request);
       HTTPResponse handle_memory_consolidation_status(const HTTPRequest& request);
       HTTPResponse handle_memory_consolidation_run(const HTTPRequest& request);
       HTTPResponse handle_memory_access_patterns(const HTTPRequest& request);
       HTTPResponse handle_memory_statistics(const HTTPRequest& request);

       std::string generate_memory_html() const;
    std::string generate_ingestion_html() const;
    std::string generate_api_docs_html() const;

    // JSON response helpers
    std::string generate_config_json() const;
    std::string generate_metrics_json() const;
    std::string generate_health_json() const;

    // Audit data collection
    nlohmann::json collect_audit_data() const;
    nlohmann::json collect_recent_function_calls() const;

    // Function call tracking
    void record_function_call(const std::string& function_name, bool success,
                            double response_time_ms, const std::string& user_agent = "",
                            const std::string& correlation_id = "");

    // Performance metrics and AI insights
    nlohmann::json collect_performance_metrics() const;
    double calculate_error_rate() const;
    double calculate_timeout_rate() const;
    nlohmann::json generate_ai_insights(const nlohmann::json& metrics) const;
    nlohmann::json generate_performance_recommendations(const nlohmann::json& metrics) const;
    nlohmann::json detect_performance_anomalies(const nlohmann::json& metrics) const;
    double calculate_system_health_score(const nlohmann::json& metrics) const;
    std::string analyze_performance_trend(const nlohmann::json& metrics) const;

    // Utility methods
    HTTPResponse create_json_response(const std::string& json_data);
    HTTPResponse create_json_response(int status_code, const nlohmann::json& json_data);
    HTTPResponse create_html_response(const std::string& html_content);
    HTTPResponse create_error_response(int code, const std::string& message);
    bool validate_request(const HTTPRequest& request);
    std::unordered_map<std::string, std::string> parse_form_data(const std::string& body);
    std::string url_decode(const std::string& input) const;
    std::string escape_html(const std::string& input) const;
};

} // namespace regulens
