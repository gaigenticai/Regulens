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

} // namespace regulens
