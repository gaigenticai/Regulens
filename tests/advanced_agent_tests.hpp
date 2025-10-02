#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

#include "infrastructure/test_framework.hpp"
#include "shared/config/configuration_manager.hpp"
#include "shared/logging/structured_logger.hpp"
#include "shared/models/agent_decision.hpp"
#include "shared/models/compliance_event.hpp"
#include "shared/pattern_recognition.hpp"
#include "shared/feedback_incorporation.hpp"
#include "shared/error_handler.hpp"
#include "shared/human_ai_collaboration.hpp"
#include "shared/agent_activity_feed.hpp"
#include "shared/visualization/decision_tree_visualizer.hpp"

namespace regulens {

/**
 * @brief Comprehensive test suite for Level 3 and Level 4 agent capabilities
 *
 * Tests advanced agent features including pattern recognition, feedback learning,
 * human-AI collaboration, error handling, and autonomous decision-making.
 */
class AdvancedAgentTestSuite {
public:
    AdvancedAgentTestSuite();

    ~AdvancedAgentTestSuite();

    /**
     * @brief Initialize test suite with required components
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Run all Level 3 and Level 4 capability tests
     * @return Test results summary
     */
    nlohmann::json run_all_tests();

    /**
     * @brief Run specific test category
     * @param category Test category to run
     * @return Test results for category
     */
    nlohmann::json run_test_category(const std::string& category);

private:
    // Test infrastructure
    std::shared_ptr<ConfigurationManager> config_manager_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<PatternRecognitionEngine> pattern_engine_;
    std::shared_ptr<FeedbackIncorporationSystem> feedback_system_;
    std::shared_ptr<ErrorHandler> error_handler_;
    std::shared_ptr<HumanAICollaboration> collaboration_system_;
    std::shared_ptr<AgentActivityFeed> activity_feed_;
    std::shared_ptr<DecisionTreeVisualizer> decision_visualizer_;

    // Test data and state
    std::vector<PatternDataPoint> test_data_points_;
    std::vector<FeedbackData> test_feedback_;
    std::vector<AgentDecision> test_decisions_;
    std::vector<ComplianceEvent> test_events_;

    // Test results tracking
    struct TestResult {
        std::string test_name;
        bool passed;
        std::string error_message;
        std::chrono::milliseconds duration;
        nlohmann::json details;

        TestResult(std::string name) : test_name(std::move(name)), passed(false), duration(0) {}
    };

    std::vector<TestResult> test_results_;

    // Level 3 Advanced Agent Tests

    /**
     * @brief Pattern Recognition System Tests
     */
    void test_pattern_recognition_system();
    void test_decision_pattern_analysis();
    void test_behavior_pattern_detection();
    void test_anomaly_detection();
    void test_trend_analysis();
    void test_correlation_analysis();
    void test_sequence_pattern_mining();
    void test_pattern_application_to_data();

    /**
     * @brief Feedback Incorporation Tests
     */
    void test_feedback_collection_system();
    void test_human_feedback_processing();
    void test_system_validation_feedback();
    void test_learning_model_updates();
    void test_feedback_driven_improvement();
    void test_feedback_analytics();

    /**
     * @brief Human-AI Collaboration Tests
     */
    void test_collaboration_session_management();
    void test_real_time_messaging();
    void test_feedback_submission();
    void test_intervention_handling();
    void test_user_permission_system();
    void test_collaboration_analytics();

    /**
     * @brief Error Handling & Recovery Tests
     */
    void test_circuit_breaker_functionality();
    void test_retry_mechanisms();
    void test_fallback_systems();
    void test_health_monitoring();
    void test_error_recovery_workflows();
    void test_graceful_degradation();

    /**
     * @brief Real-time Activity Feed Tests
     */
    void test_activity_streaming();
    void test_activity_filtering();
    void test_activity_analytics();
    void test_performance_monitoring();

    /**
     * @brief Decision Tree Visualization Tests
     */
    void test_decision_tree_generation();
    void test_decision_tree_interactive_features();
    void test_decision_tree_export_formats();

    // Level 4 Autonomous Systems Tests

    /**
     * @brief Regulatory Monitoring Tests
     */
    void test_regulatory_change_detection();
    void test_regulatory_source_integration();
    void test_regulatory_impact_analysis();
    void test_regulatory_compliance_tracking();

    /**
     * @brief MCP Tool Integration Tests
     */
    void test_mcp_tool_discovery();
    void test_mcp_tool_execution();
    void test_mcp_protocol_compliance();
    void test_mcp_error_handling();

    /**
     * @brief Autonomous Decision Making Tests
     */
    void test_autonomous_decision_workflows();
    void test_decision_confidence_scoring();
    void test_decision_explainability();
    void test_decision_audit_trails();

    /**
     * @brief Multi-Agent Orchestration Tests
     */
    void test_agent_orchestration();
    void test_agent_task_distribution();
    void test_agent_coordination();
    void test_agent_performance_optimization();

    /**
     * @brief Continuous Learning System Tests
     */
    void test_continuous_learning_loops();
    void test_adaptive_behavior_modification();
    void test_performance_based_learning();
    void test_knowledge_accumulation();

    // Integration and End-to-End Tests

    /**
     * @brief Complete Agent Workflow Tests
     */
    void test_end_to_end_decision_process();
    void test_full_feedback_learning_cycle();
    void test_complete_error_recovery_scenario();
    void test_autonomous_operation_simulation();

    /**
     * @brief Performance and Scalability Tests
     */
    void test_concurrent_agent_operations();
    void test_large_scale_data_processing();
    void test_system_performance_under_load();
    void test_memory_and_resource_usage();

    /**
     * @brief Edge Case and Error Condition Tests
     */
    void test_extreme_input_handling();
    void test_network_failure_scenarios();
    void test_database_unavailability();
    void test_external_service_failures();

    // Helper methods

    /**
     * @brief Generate test data for various scenarios
     */
    void generate_test_data();
    void generate_decision_test_data();
    void generate_feedback_test_data();
    void generate_event_test_data();
    void generate_activity_test_data();

    /**
     * @brief Test execution helpers
     */
    TestResult run_individual_test(std::function<bool()> test_func, const std::string& test_name);
    void record_test_result(const TestResult& result);
    nlohmann::json generate_test_summary() const;

    /**
     * @brief Validation helpers
     */
    bool validate_pattern_accuracy(const std::vector<std::shared_ptr<Pattern>>& patterns,
                                 const std::string& expected_pattern_type);
    bool validate_feedback_processing(const FeedbackData& original, const FeedbackData& processed);
    bool validate_decision_quality(const AgentDecision& decision);
    bool validate_error_handling(const std::string& operation, bool should_succeed);

    /**
     * @brief Mock data generators for testing
     */
    PatternDataPoint create_mock_pattern_data_point(const std::string& entity_id,
                                                   const std::string& activity_type,
                                                   double value);
    FeedbackData create_mock_feedback(const std::string& entity_id,
                                    FeedbackType type, double score);
    AgentDecision create_mock_decision(const std::string& agent_id,
                                     DecisionType type, double confidence);
    ComplianceEvent create_mock_event(EventType type, EventSeverity severity);

    /**
     * @brief Performance measurement
     */
    std::chrono::milliseconds measure_operation_time(std::function<void()> operation);
    void validate_performance_threshold(const std::string& operation,
                                     std::chrono::milliseconds actual_time,
                                     std::chrono::milliseconds threshold);
};

} // namespace regulens
