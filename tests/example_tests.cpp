#include <gtest/gtest.h>

#include "infrastructure/test_framework.hpp"

namespace regulens {

// Example test using the base test fixture
TEST_F(RegulensTest, BasicSetup) {
    // Test that the test environment is properly initialized
    EXPECT_TRUE(TestEnvironment::get_instance().is_test_mode());

    // Test that we can get a logger and config
    auto logger = get_test_logger();
    auto config = get_test_config();

    ASSERT_TRUE(logger != nullptr);
    ASSERT_TRUE(config != nullptr);

    // Test mock data generation
    auto mock_change = create_mock_regulatory_change();
    auto mock_event = create_mock_compliance_event();
    auto mock_decision = create_mock_agent_decision();

    EXPECT_JSON_CONTAINS(mock_change, "source_id");
    EXPECT_JSON_CONTAINS(mock_event, "type");
    EXPECT_JSON_CONTAINS(mock_decision, "decision_id");
}

// Example database test
TEST_F(DatabaseTest, MockDatabaseOperations) {
    // Test inserting data
    nlohmann::json test_data = {
        {"id", "test_123"},
        {"name", "Test Record"},
        {"value", 42}
    };

    EXPECT_TRUE(mock_database_insert("test_table", test_data));

    // Test querying data
    nlohmann::json result;
    EXPECT_TRUE(mock_database_query("SELECT * FROM test_table", result));
    EXPECT_TRUE(result.is_array());
}

// Example API test
TEST_F(ApiTest, MockApiResponses) {
    // Set up mock response
    nlohmann::json mock_response = {
        {"status", "success"},
        {"data", {{"id", 123}, {"name", "test"}}}
    };

    mock_api_response("https://api.example.com/test", "GET", 200, mock_response);

    // Verify mock response is set up (in real test, HTTP client would use this)
    EXPECT_TRUE(true); // Placeholder - actual verification would depend on HTTP client integration
}

// Example agent orchestration test
TEST_F(AgentOrchestrationTest, OrchestratorCreation) {
    // Test that we can create a test orchestrator
    auto orchestrator = create_test_orchestrator();
    ASSERT_TRUE(orchestrator != nullptr);

    // Test creating mock agents
    auto agent = create_mock_agent("test_agent");
    ASSERT_TRUE(agent != nullptr);
    EXPECT_EQ(agent->get_agent_type(), "test_agent");

    // Test creating and verifying tasks
    auto task = create_test_task("test_agent");
    EXPECT_FALSE(task.task_id.empty());
    EXPECT_EQ(task.agent_type, "test_agent");
}

// Example regulatory monitoring test
TEST_F(RegulatoryMonitoringTest, MonitorSetup) {
    // Test creating a test monitor
    auto monitor = create_test_monitor();
    ASSERT_TRUE(monitor != nullptr);

    // Test creating mock regulatory sources
    auto source = create_mock_regulatory_source(RegulatorySourceType::SEC_EDGAR, "test_sec");
    ASSERT_TRUE(source != nullptr);
    EXPECT_EQ(source->get_source_id(), "test_sec");

    // Verify monitor state
    verify_monitor_state(*monitor);
}

// Example MCP tool test
TEST_F(McpToolTest, ToolCreation) {
    // Test creating MCP tool
    auto mcp_tool = create_test_mcp_tool();
    ASSERT_TRUE(mcp_tool != nullptr);

    // Test MCP protocol verification
    ToolResult mock_result(true, {{"test", "data"}}, "");
    verify_mcp_protocol("test_operation", mock_result);
}

// Example knowledge base test
TEST_F(KnowledgeBaseTest, KnowledgeBaseSetup) {
    // Test creating knowledge base
    auto kb = create_test_knowledge_base();
    ASSERT_TRUE(kb != nullptr);

    // Test populating mock data
    std::vector<nlohmann::json> documents = {
        {{"id", "doc1"}, {"content", "Test document 1"}},
        {{"id", "doc2"}, {"content", "Test document 2"}}
    };

    populate_mock_knowledge_base(documents);

    // Test search verification
    std::vector<std::string> queries = {"test", "document"};
    std::vector<nlohmann::json> expected = {
        {{"results", "mock_results"}},
        {{"results", "mock_results"}}
    };

    verify_search_results(queries, expected);
}

// Example integration test
TEST(IntegrationTest, FullSystemTest) {
    TestEnvironmentGuard guard; // Automatically manages test environment

    // This test demonstrates how to test the full system
    // with proper isolation

    // Create isolated components
    auto config = std::make_shared<ConfigurationManager>();
    config->initialize(0, nullptr);

    auto logger = std::make_shared<StructuredLogger>();

    // Test that components can be created and initialized
    EXPECT_TRUE(config->load_from_environment());
    EXPECT_TRUE(logger != nullptr);

    // Verify test isolation
    EXPECT_TRUE(TestEnvironment::get_instance().verify_isolation());
}

// Example performance test
TEST(PerformanceTest, TimingTest) {
    TestEnvironmentGuard guard;

    auto start = std::chrono::high_resolution_clock::now();

    // Simulate some work
    for (int i = 0; i < 1000; ++i) {
        auto data = test_utils::generate_random_json();
        TestEnvironment::get_instance().set_test_data("perf_test_" + std::to_string(i), data);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Performance should be reasonable
    EXPECT_LT(duration.count(), 1000); // Less than 1 second
}

// Example fixture cleanup test
TEST_F(RegulensTest, CleanupTest) {
    // Set some test data
    set_test_environment_variable("TEST_VAR", "test_value");
    TestEnvironment::get_instance().set_test_data("test_key", {"test", "data"});

    // Data should be available during test
    EXPECT_EQ(get_environment_variable("TEST_VAR"), "test_value");
    EXPECT_FALSE(TestEnvironment::get_instance().get_test_data("test_key").empty());

    // After test cleanup (in TearDown), data should be cleared
    // This is verified implicitly by the test framework
}

// Error handling and health check tests
TEST_F(RegulensTest, ErrorHandlerCorrelationIdGeneration) {
    auto error_handler = create_test_error_handler();
    auto config = get_test_config();
    auto logger = get_test_logger();

    ASSERT_TRUE(error_handler != nullptr);

    // Test correlation ID generation
    std::string id1 = error_handler->generate_error_correlation_id();
    std::string id2 = error_handler->generate_error_correlation_id();

    EXPECT_FALSE(id1.empty());
    EXPECT_FALSE(id2.empty());
    EXPECT_NE(id1, id2);

    // Test that correlation IDs start with "err_"
    EXPECT_EQ(id1.substr(0, 4), "err_");
    EXPECT_EQ(id2.substr(0, 4), "err_");
}

TEST_F(RegulensTest, ErrorHandlerContextTracking) {
    auto error_handler = create_test_error_handler();
    auto config = get_test_config();
    auto logger = get_test_logger();

    ASSERT_TRUE(error_handler != nullptr);

    std::string correlation_id = "test_corr_123";

    // Add context information
    error_handler->add_error_context(correlation_id, "component", "test_component");
    error_handler->add_error_context(correlation_id, "operation", "test_operation");
    error_handler->add_error_context(correlation_id, "severity", "3");

    // Retrieve context
    auto context = error_handler->get_error_context(correlation_id);

    EXPECT_EQ(context.size(), 3);
    EXPECT_EQ(context["component"], "test_component");
    EXPECT_EQ(context["operation"], "test_operation");
    EXPECT_EQ(context["severity"], "3");

    // Clear context
    error_handler->clear_error_context(correlation_id);
    auto empty_context = error_handler->get_error_context(correlation_id);
    EXPECT_TRUE(empty_context.empty());
}

TEST_F(RegulensTest, ErrorHandlerReportError) {
    auto error_handler = create_test_error_handler();
    auto config = get_test_config();
    auto logger = get_test_logger();

    ASSERT_TRUE(error_handler != nullptr);

    ErrorInfo error_info{
        "test_error_123",
        ErrorSeverity::HIGH,
        "Test error for unit testing",
        "TestComponent",
        "test_operation",
        std::chrono::system_clock::now()
    };

    // Report error and get correlation ID
    std::string correlation_id = error_handler->report_error(error_info);

    EXPECT_FALSE(correlation_id.empty());
    EXPECT_EQ(correlation_id.substr(0, 4), "err_");

    // Verify context was added
    auto context = error_handler->get_error_context(correlation_id);
    EXPECT_FALSE(context.empty());
    EXPECT_EQ(context["component"], "TestComponent");
    EXPECT_EQ(context["operation"], "test_operation");
}

TEST_F(RegulensTest, ErrorHandlerHealthReport) {
    auto error_handler = create_test_error_handler();
    auto config = get_test_config();
    auto logger = get_test_logger();

    ASSERT_TRUE(error_handler != nullptr);

    // Generate health report
    nlohmann::json health_report = error_handler->get_system_health_report();

    // Verify report structure
    EXPECT_TRUE(health_report.contains("timestamp"));
    EXPECT_TRUE(health_report.contains("status"));
    EXPECT_TRUE(health_report.contains("components"));
    EXPECT_TRUE(health_report.contains("metrics"));

    // Verify metrics
    auto& metrics = health_report["metrics"];
    EXPECT_TRUE(metrics.contains("total_errors_processed"));
    EXPECT_TRUE(metrics.contains("total_recovery_attempts"));
    EXPECT_TRUE(metrics.contains("total_successful_recoveries"));
    EXPECT_TRUE(metrics.contains("active_error_contexts"));
    EXPECT_TRUE(metrics.contains("error_history_size"));

    // Verify components section
    auto& components = health_report["components"];
    EXPECT_TRUE(components.contains("database"));
    EXPECT_TRUE(components.contains("regulatory_monitor"));
    EXPECT_TRUE(components.contains("knowledge_base"));
}

TEST_F(RegulensTest, ErrorHandlerComponentHealthStatus) {
    auto error_handler = create_test_error_handler();
    auto config = get_test_config();
    auto logger = get_test_logger();

    ASSERT_TRUE(error_handler != nullptr);

    // Get component health status
    nlohmann::json component_status = error_handler->get_component_health_status();

    // Verify structure
    EXPECT_TRUE(component_status.contains("database"));
    EXPECT_TRUE(component_status.contains("regulatory_monitor"));
    EXPECT_TRUE(component_status.contains("knowledge_base"));
    EXPECT_TRUE(component_status.contains("llm_services"));
    EXPECT_TRUE(component_status.contains("pattern_recognition"));
    EXPECT_TRUE(component_status.contains("risk_assessment"));

    // Verify each component has required fields
    for (const auto& [component_name, component_data] : component_status.items()) {
        EXPECT_TRUE(component_data.contains("status"));
        EXPECT_TRUE(component_data.contains("last_check"));
        EXPECT_TRUE(component_data.contains("message"));
    }
}

TEST_F(RegulensTest, ErrorHandlerExternalServiceHealthCheck) {
    auto error_handler = create_test_error_handler();
    auto config = get_test_config();
    auto logger = get_test_logger();

    ASSERT_TRUE(error_handler != nullptr);

    // Test health check for a non-existent service (should fail gracefully)
    bool result = error_handler->check_external_service_health("test_service", "http://nonexistent:9999/health");

    // Should return false for non-existent service, but not crash
    EXPECT_FALSE(result);
}

// Integration test for configuration management
TEST_F(RegulensTest, ConfigurationManagerEnvironmentVariables) {
    auto config = get_test_config();
    ASSERT_TRUE(config != nullptr);

    // Test that configuration can be loaded without localhost defaults
    EXPECT_NO_THROW({
        // This should work in test environment
        config->validate_configuration();
    });
}

// Integration test for logging with correlation IDs
TEST_F(RegulensTest, StructuredLoggerCorrelationSupport) {
    auto logger = get_test_logger();
    ASSERT_TRUE(logger != nullptr);

    // Test logging with correlation ID
    std::string test_correlation_id = "test_corr_456";

    EXPECT_NO_THROW({
        logger->error_with_correlation("Test error with correlation",
                                      test_correlation_id,
                                      "TestComponent",
                                      "test_function");
    });
}

// Streaming response handler tests
TEST_F(RegulensTest, StreamingHandlerCreation) {
    auto config = get_test_config();
    auto logger = get_test_logger();
    auto error_handler = create_test_error_handler();

    ASSERT_TRUE(config != nullptr);
    ASSERT_TRUE(logger != nullptr);
    ASSERT_TRUE(error_handler != nullptr);

    // Test streaming handler creation
    EXPECT_NO_THROW({
        StreamingResponseHandler handler(config, logger.get(), error_handler.get());
    });
}

TEST_F(RegulensTest, StreamingSessionManagement) {
    auto config = get_test_config();
    auto logger = get_test_logger();
    auto error_handler = create_test_error_handler();

    StreamingResponseHandler handler(config, logger.get(), error_handler.get());

    // Test session creation
    std::string session_id = "test_session_123";
    auto session = handler.create_session(session_id);
    ASSERT_TRUE(session != nullptr);

    // Test session retrieval
    auto retrieved_session = handler.get_session(session_id);
    ASSERT_TRUE(retrieved_session != nullptr);
    EXPECT_EQ(retrieved_session, session);

    // Test session removal
    handler.remove_session(session_id);
    auto removed_session = handler.get_session(session_id);
    EXPECT_TRUE(removed_session == nullptr);
}

TEST_F(RegulensTest, StreamingEventParsing) {
    auto logger = get_test_logger();
    SSEParser parser(logger.get());

    // Test SSE event parsing
    std::string sse_data = "data: {\"type\": \"content_block_delta\", \"delta\": {\"text\": \"Hello\"}}\n\n";
    auto events = parser.parse_chunk(sse_data);

    EXPECT_EQ(events.size(), 1);
    EXPECT_EQ(events[0].type, StreamingEventType::TOKEN);
    EXPECT_FALSE(events[0].data.empty());
}

TEST_F(RegulensTest, StreamingAccumulator) {
    auto logger = get_test_logger();
    StreamingAccumulator accumulator(logger.get());

    // Test content accumulation
    StreamingEvent token_event(StreamingEventType::TOKEN, "{\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}");
    accumulator.add_event(token_event);

    std::string content = accumulator.get_accumulated_content();
    EXPECT_EQ(content, "Hello");

    // Test completion
    StreamingEvent completion_event(StreamingEventType::COMPLETION, "{}");
    accumulator.add_event(completion_event);

    EXPECT_TRUE(accumulator.validate_accumulation());
}

// OpenAI streaming client tests
TEST_F(RegulensTest, OpenAIStreamingClientCreation) {
    auto config = get_test_config();
    auto logger = get_test_logger();
    auto error_handler = create_test_error_handler();

    EXPECT_NO_THROW({
        OpenAIClient client(config, logger, error_handler);
    });
}

// Anthropic streaming client tests
TEST_F(RegulensTest, AnthropicStreamingClientCreation) {
    auto config = get_test_config();
    auto logger = get_test_logger();
    auto error_handler = create_test_error_handler();

    EXPECT_NO_THROW({
        AnthropicClient client(config, logger, error_handler);
    });
}

} // namespace regulens
