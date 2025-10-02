#pragma once

// Conditionally include GTest
#ifdef NO_GTEST
// Define minimal test macros for environments without GTest
#define TEST(test_case_name, test_name) void test_case_name##_##test_name()
#define TEST_F(test_fixture, test_name) void test_fixture##_##test_name()
#define EXPECT_TRUE(condition) if (!(condition)) { /* assertion failed */ }
#define EXPECT_FALSE(condition) if (condition) { /* assertion failed */ }
#define EXPECT_EQ(a, b) if ((a) != (b)) { /* assertion failed */ }
#define ASSERT_TRUE(condition) if (!(condition)) { return; }
#define ASSERT_FALSE(condition) if (condition) { return; }
#define ASSERT_EQ(a, b) if ((a) != (b)) { return; }
#else
#include <gtest/gtest.h>
#endif

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>

#include "../../shared/logging/structured_logger.hpp"
#include "../../shared/config/configuration_manager.hpp"
#include "../../core/agent/agent_orchestrator.hpp"
#include "../../core/agent/compliance_agent.hpp"

// Forward declarations for components that may not compile yet
class RegulatorySource;
class RegulatorySourceType;
class RegulatoryChange;
class RegulatoryMonitor;
class MCPToolIntegration;
struct ToolResult;

namespace regulens {

/**
 * @brief Base test fixture for all Regulens tests
 *
 * Provides common test setup, teardown, and utilities for isolated testing
 * without external dependencies.
 */
#ifdef NO_GTEST
class RegulensTest {
public:
    virtual ~RegulensTest() = default;
protected:
    virtual void SetUp();
    virtual void TearDown();
#else
class RegulensTest : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;
#endif

    // Test utilities
    void set_test_environment_variable(const std::string& key, const std::string& value);
    void clear_test_environment_variable(const std::string& key);
    void reset_test_environment();

    // Logger access for tests
    std::shared_ptr<StructuredLogger> get_test_logger();

    // Configuration for tests
    std::shared_ptr<ConfigurationManager> get_test_config();

    // Mock data generation
    nlohmann::json create_mock_regulatory_change();
    nlohmann::json create_mock_compliance_event();
    nlohmann::json create_mock_agent_decision();

private:
    std::unordered_map<std::string, std::string> original_env_vars_;
    std::shared_ptr<StructuredLogger> test_logger_;
    std::shared_ptr<ConfigurationManager> test_config_;
};

/**
 * @brief Test fixture for database-dependent tests
 *
 * Provides mock database functionality for testing without real database connections.
 */
class DatabaseTest : public RegulensTest {
protected:
#ifdef NO_GTEST
    void SetUp();
    void TearDown();
#else
    void SetUp() override;
    void TearDown() override;
#endif

    // Mock database operations
    bool mock_database_query(const std::string& query, nlohmann::json& result);
    bool mock_database_insert(const std::string& table, const nlohmann::json& data);
    bool mock_database_update(const std::string& table, const nlohmann::json& data,
                             const std::string& where_clause);

    // Reset mock database state
    void reset_mock_database();

private:
    nlohmann::json mock_db_state_;
};

/**
 * @brief Test fixture for API-dependent tests
 *
 * Provides mock API responses for testing without real network calls.
 */
class ApiTest : public RegulensTest {
protected:
#ifdef NO_GTEST
    void SetUp();
    void TearDown();
#else
    void SetUp() override;
    void TearDown() override;
#endif

    // Mock API responses
    void mock_api_response(const std::string& url, const std::string& method,
                          int status_code, const nlohmann::json& response);
    void mock_api_error(const std::string& url, const std::string& method,
                       int status_code, const std::string& error_message);

    // Clear all mock responses
    void clear_mock_responses();

private:
    std::unordered_map<std::string, nlohmann::json> mock_responses_;
};

/**
 * @brief Test fixture for agent orchestration tests
 *
 * Provides isolated agent testing environment with mock dependencies.
 */
class AgentOrchestrationTest : public RegulensTest {
protected:
#ifdef NO_GTEST
    void SetUp();
    void TearDown();
#else
    void SetUp() override;
    void TearDown() override;
#endif

    // Agent testing utilities
    std::unique_ptr<AgentOrchestrator> create_test_orchestrator();
    std::shared_ptr<ComplianceAgent> create_mock_agent(const std::string& agent_type);

    // Task testing
    AgentTask create_test_task(const std::string& agent_type = "test_agent");
    void verify_task_execution(const AgentTask& task, const TaskResult& result);

private:
    std::unique_ptr<AgentOrchestrator> test_orchestrator_;
    std::vector<std::shared_ptr<ComplianceAgent>> mock_agents_;
};

/**
 * @brief Test fixture for regulatory monitoring tests
 *
 * Provides mock regulatory sources and monitoring environment.
 */
class RegulatoryMonitoringTest : public RegulensTest {
protected:
#ifdef NO_GTEST
    void SetUp();
    void TearDown();
#else
    void SetUp() override;
    void TearDown() override;
#endif

    // Regulatory source mocking
    std::shared_ptr<RegulatorySource> create_mock_regulatory_source(
        RegulatorySourceType type, const std::string& source_id);
    void mock_regulatory_change(const std::string& source_id,
                               const RegulatoryChange& change);

    // Monitor testing
    std::unique_ptr<RegulatoryMonitor> create_test_monitor();
    void verify_monitor_state(const RegulatoryMonitor& monitor);

private:
    std::unordered_map<std::string, std::vector<RegulatoryChange>> mock_changes_;
};

/**
 * @brief Test fixture for MCP tool integration tests
 *
 * Provides isolated MCP tool testing environment.
 */
class McpToolTest : public ApiTest {
protected:
#ifdef NO_GTEST
    void SetUp();
    void TearDown();
#else
    void SetUp() override;
    void TearDown() override;
#endif

    // MCP tool testing
    std::unique_ptr<MCPToolIntegration> create_test_mcp_tool();
    void mock_mcp_server_response(const std::string& method,
                                  const nlohmann::json& request,
                                  const nlohmann::json& response);

    // Protocol verification
    void verify_mcp_protocol(const std::string& operation, const ToolResult& result);

private:
    std::unordered_map<std::string, nlohmann::json> mcp_responses_;
};

/**
 * @brief Test fixture for knowledge base tests
 *
 * Provides isolated knowledge base testing with mock data.
 */
class KnowledgeBaseTest : public DatabaseTest {
protected:
#ifdef NO_GTEST
    void SetUp();
    void TearDown();
#else
    void SetUp() override;
    void TearDown() override;
#endif

    // Knowledge base testing
    std::unique_ptr<KnowledgeBase> create_test_knowledge_base();
    void populate_mock_knowledge_base(const std::vector<nlohmann::json>& documents);

    // Search testing
    void verify_search_results(const std::vector<std::string>& queries,
                              const std::vector<nlohmann::json>& expected_results);

private:
    std::vector<nlohmann::json> mock_documents_;
};

/**
 * @brief Test utilities namespace
 */
namespace test_utils {

/**
 * @brief Generate random test data
 */
std::string generate_random_string(size_t length = 10);
int generate_random_int(int min = 0, int max = 100);
nlohmann::json generate_random_json();

/**
 * @brief File system utilities for tests
 */
bool create_temp_directory(const std::string& prefix, std::string& temp_path);
bool remove_temp_directory(const std::string& path);
std::string create_temp_file(const std::string& content = "");

/**
 * @brief Time utilities for tests
 */
std::chrono::system_clock::time_point get_test_timestamp();
void advance_test_time(std::chrono::seconds duration);

/**
 * @brief Assertion helpers
 */
#define ASSERT_JSON_EQ(actual, expected) \
    ASSERT_EQ(actual.dump(), expected.dump()) << "JSON mismatch"

#define ASSERT_JSON_CONTAINS(json, key) \
    ASSERT_TRUE(json.contains(key)) << "JSON missing key: " << key

#define EXPECT_JSON_EQ(actual, expected) \
    EXPECT_EQ(actual.dump(), expected.dump()) << "JSON mismatch"

#define EXPECT_JSON_CONTAINS(json, key) \
    EXPECT_TRUE(json.contains(key)) << "JSON missing key: " << key

} // namespace test_utils

} // namespace regulens
