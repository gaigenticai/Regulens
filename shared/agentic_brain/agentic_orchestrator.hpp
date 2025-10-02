/**
 * Agentic Brain - Orchestrator
 *
 * The central intelligence hub that coordinates all agentic AI operations.
 * This is the "brain" that learns from data, makes decisions, and adapts.
 */

#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"
#include "../network/http_client.hpp"
#include "../tool_integration/tool_interface.hpp"
#include "../tool_integration/tools/mcp_tool.hpp"
#include "../event_system/event_bus.hpp"
#include "llm_interface.hpp"
#include "learning_engine.hpp"
#include "decision_engine.hpp"
#include <nlohmann/json.hpp>

namespace regulens {

enum class AgentType {
    TRANSACTION_GUARDIAN,
    REGULATORY_ASSESSOR,
    AUDIT_INTELLIGENCE
};

enum class DecisionUrgency {
    LOW,
    MEDIUM,
    HIGH,
    CRITICAL
};

struct AgentDecision {
    std::string agent_id;
    AgentType agent_type;
    std::string decision_id;
    nlohmann::json input_context;
    nlohmann::json decision_output;
    DecisionUrgency urgency;
    double confidence_score;
    std::string reasoning;
    std::vector<std::string> recommended_actions;
    std::chrono::system_clock::time_point timestamp;
    bool requires_human_review;
};


class AgenticOrchestrator {
public:
    /**
     * @brief Constructor with full component injection
     * Provides maximum flexibility for testing and production use
     */
    AgenticOrchestrator(
        std::shared_ptr<ConnectionPool> db_pool,
        std::shared_ptr<LLMInterface> llm_interface,
        std::shared_ptr<LearningEngine> learning_engine,
        std::shared_ptr<DecisionEngine> decision_engine,
        std::shared_ptr<ToolRegistry> tool_registry,
        std::shared_ptr<EventBus> event_bus,
        StructuredLogger* logger
    );

    /**
     * @brief Simplified constructor with component auto-initialization
     * Creates default instances of complex components for easier usage
     */
    AgenticOrchestrator(
        std::shared_ptr<ConnectionPool> db_pool,
        StructuredLogger* logger
    );

    ~AgenticOrchestrator();

    // Core agentic operations
    bool initialize();
    void shutdown();

    // Decision making
    std::optional<AgentDecision> make_decision(
        AgentType agent_type,
        const nlohmann::json& input_context
    );

    // Learning and adaptation
    bool incorporate_feedback(const LearningFeedback& feedback);
    bool update_agent_knowledge(AgentType agent_type, const nlohmann::json& new_knowledge);
    nlohmann::json get_agent_insights(AgentType agent_type);

    // Proactive monitoring
    std::vector<AgentDecision> check_for_proactive_actions();
    std::vector<nlohmann::json> identify_risk_patterns();

    // Performance and health
    nlohmann::json get_system_health();
    nlohmann::json get_agent_performance_metrics();

    // Human-AI collaboration
    std::vector<nlohmann::json> get_pending_human_reviews();
    bool process_human_decision(
        const std::string& decision_id,
        bool approved,
        const std::string& human_reasoning = ""
    );

    // Tool integration - Autonomous tool usage
    std::vector<std::string> discover_available_tools(ToolCategory category = ToolCategory::INTEGRATION);
    std::vector<std::string> find_tools_by_capability(ToolCapability capability);
    std::unique_ptr<Tool> acquire_tool(const std::string& tool_id);
    ToolResult execute_tool_operation(
        const std::string& tool_id,
        const std::string& operation,
        const nlohmann::json& parameters
    );

    // Autonomous tool selection and execution
    std::vector<nlohmann::json> analyze_situation_and_recommend_tools(
        AgentType agent_type,
        const nlohmann::json& situation_context
    );

    bool execute_autonomous_tool_workflow(
        AgentType agent_type,
        const nlohmann::json& context,
        const std::vector<std::string>& required_tools
    );

    // Tool learning and adaptation
    bool learn_tool_effectiveness(
        const std::string& tool_id,
        const std::string& operation,
        bool success,
        std::chrono::milliseconds execution_time
    );

    std::vector<nlohmann::json> get_tool_usage_recommendations(AgentType agent_type);

    // Level 3: Tool-Intelligent Capabilities
    nlohmann::json analyze_situation_with_llm(const nlohmann::json& context, AgentType agent_type);
    std::vector<nlohmann::json> generate_intelligent_tool_recommendations(
        const nlohmann::json& situation_analysis,
        const std::vector<std::string>& available_tools
    );
    std::vector<nlohmann::json> optimize_tool_workflow(
        const std::vector<nlohmann::json>& tool_recommendations,
        const nlohmann::json& context
    );

    // Level 4: Tool-Creative Capabilities
    nlohmann::json discover_unknown_tools(const nlohmann::json& requirements);
    nlohmann::json generate_custom_tool_config(
        const std::string& tool_type,
        const nlohmann::json& requirements,
        const nlohmann::json& context
    );
    std::vector<nlohmann::json> compose_tool_workflow(
        const nlohmann::json& complex_task,
        const std::vector<std::string>& available_tools
    );
    bool negotiate_tool_capabilities(
        const std::string& tool_id,
        const nlohmann::json& required_capabilities
    );

    // Environment Variable Configuration
    AgentCapabilityConfig get_capability_config() const { return capability_config_; }

private:
    // Advanced capability configuration
    AgentCapabilityConfig capability_config_;

    // Helper methods for advanced capabilities
    std::vector<nlohmann::json> generate_fallback_tool_recommendations(
        AgentType agent_type,
        const nlohmann::json& situation_context
    );

    // Tool operation determination helpers
    std::string determine_transaction_tool_operation(const nlohmann::json& context);
    nlohmann::json prepare_transaction_tool_parameters(const nlohmann::json& context);
    std::string determine_regulatory_tool_operation(const nlohmann::json& context);
    nlohmann::json prepare_regulatory_tool_parameters(const nlohmann::json& context);
    std::string determine_audit_tool_operation(const nlohmann::json& context);
    nlohmann::json prepare_audit_tool_parameters(const nlohmann::json& context);

    // Advanced capability helper methods
    bool validate_generated_config(const nlohmann::json& config);
    nlohmann::json generate_basic_tool_config(const std::string& tool_type, const nlohmann::json& requirements);
    bool validate_workflow_composition(const std::vector<nlohmann::json>& workflow, const std::vector<std::string>& available_tools);
    std::vector<nlohmann::json> create_simple_workflow(const nlohmann::json& complex_task, const std::vector<std::string>& available_tools);
    // Agent management
    bool initialize_agents();
    bool load_agent_configurations();
    bool train_agents_from_historical_data();

    // Decision processing
    AgentDecision process_transaction_decision(const nlohmann::json& context);
    AgentDecision process_regulatory_decision(const nlohmann::json& context);
    AgentDecision process_audit_decision(const nlohmann::json& context);

    // Learning pipeline
    bool update_learning_models(const LearningFeedback& feedback);
    bool retrain_agent_models(AgentType agent_type);
    nlohmann::json extract_patterns_from_historical_data(AgentType agent_type);


    // Risk assessment
    double assess_transaction_risk(const nlohmann::json& transaction_data);
    double assess_regulatory_risk(const nlohmann::json& regulatory_data);
    double assess_audit_risk(const nlohmann::json& audit_data);

    // Proactive capabilities
    std::vector<nlohmann::json> detect_anomalous_patterns();
    std::vector<nlohmann::json> predict_future_risks();
    std::vector<nlohmann::json> identify_adaptation_opportunities();

    // Internal state
    std::shared_ptr<ConnectionPool> db_pool_;
    std::shared_ptr<HttpClient> http_client_;
    StructuredLogger* logger_;

    std::shared_ptr<LLMInterface> llm_interface_;
    std::shared_ptr<LearningEngine> learning_engine_;
    std::shared_ptr<DecisionEngine> decision_engine_;

    std::unordered_map<AgentType, nlohmann::json> agent_configurations_;
    std::unordered_map<std::string, AgentDecision> pending_decisions_;

    // Tool integration
    std::shared_ptr<ToolRegistry> tool_registry_;
    std::unordered_map<std::string, std::unique_ptr<Tool>> active_tools_;
    std::mutex tool_mutex_;

    // Event system for agent communication
    std::shared_ptr<EventBus> event_bus_;

    bool initialized_;
    std::atomic<bool> running_;
};

} // namespace regulens
