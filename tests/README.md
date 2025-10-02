# Regulens Test Infrastructure

This directory contains a comprehensive test infrastructure for the Regulens Agentic AI Compliance System that enables isolated, reliable testing without external dependencies.

## Overview

The test infrastructure provides:

- **Isolated Test Execution**: Tests run in isolated environments with mock services
- **Dependency Injection**: Easy mocking of external services (APIs, databases, etc.)
- **Resource Management**: Automatic cleanup of temporary files and test data
- **Comprehensive Fixtures**: Ready-to-use test fixtures for different system components
- **Performance Testing**: Built-in timing and performance verification
- **Integration Testing**: Full system integration tests with proper isolation

## Architecture

### Core Components

#### TestEnvironment
Singleton managing global test state and resources:
- Temporary file/directory creation and cleanup
- Environment variable isolation
- Mock service registration
- Configuration overrides
- Test data management

#### Test Fixtures

##### RegulensTest (Base)
All tests should inherit from this base fixture:
```cpp
class MyTest : public RegulensTest {
protected:
    void SetUp() override { /* test setup */ }
    void TearDown() override { /* test cleanup */ }
};
```

##### DatabaseTest
For tests requiring database operations:
```cpp
TEST_F(DatabaseTest, MyDatabaseTest) {
    mock_database_insert("table", test_data);
    nlohmann::json result;
    mock_database_query("SELECT * FROM table", result);
}
```

##### ApiTest
For tests involving external API calls:
```cpp
TEST_F(ApiTest, MyApiTest) {
    mock_api_response("https://api.example.com", "GET", 200, expected_response);
    // HTTP client will use mock response instead of making real call
}
```

##### AgentOrchestrationTest
For testing agent orchestration logic:
```cpp
TEST_F(AgentOrchestrationTest, MyAgentTest) {
    auto orchestrator = create_test_orchestrator();
    auto agent = create_mock_agent("compliance_agent");
    auto task = create_test_task("compliance_agent");
}
```

##### RegulatoryMonitoringTest
For testing regulatory monitoring:
```cpp
TEST_F(RegulatoryMonitoringTest, MyMonitorTest) {
    auto monitor = create_test_monitor();
    auto source = create_mock_regulatory_source(SEC_EDGAR, "test_source");
}
```

##### McpToolTest
For testing MCP tool integration:
```cpp
TEST_F(McpToolTest, MyMcpTest) {
    auto mcp_tool = create_test_mcp_tool();
    mock_mcp_server_response("tools/list", request, response);
}
```

##### KnowledgeBaseTest
For testing knowledge base operations:
```cpp
TEST_F(KnowledgeBaseTest, MyKbTest) {
    auto kb = create_test_knowledge_base();
    populate_mock_knowledge_base(test_documents);
}
```

## Usage Examples

### Basic Test Setup

```cpp
#include "infrastructure/test_framework.hpp"

TEST_F(RegulensTest, BasicFunctionality) {
    // Test environment is automatically set up
    EXPECT_TRUE(TestEnvironment::get_instance().is_test_mode());

    // Access test utilities
    auto logger = get_test_logger();
    auto config = get_test_config();

    // Generate mock data
    auto mock_change = create_mock_regulatory_change();
    EXPECT_JSON_CONTAINS(mock_change, "source_id");
}
```

### Environment Variable Testing

```cpp
TEST_F(RegulensTest, EnvironmentVariables) {
    // Set test environment variable
    set_test_environment_variable("TEST_VAR", "test_value");

    // Verify it's set
    EXPECT_EQ(get_environment_variable("TEST_VAR"), "test_value");

    // Variable is automatically cleaned up after test
}
```

### Mock API Testing

```cpp
TEST_F(ApiTest, ApiIntegration) {
    // Set up mock response
    nlohmann::json expected_response = {{"status", "success"}};
    mock_api_response("https://api.example.com/test", "GET", 200, expected_response);

    // In real implementation, HTTP client would use this mock response
    // instead of making actual network call
}
```

### Database Testing

```cpp
TEST_F(DatabaseTest, DatabaseOperations) {
    // Insert mock data
    nlohmann::json test_record = {{"id", 1}, {"name", "test"}};
    EXPECT_TRUE(mock_database_insert("test_table", test_record));

    // Query mock data
    nlohmann::json result;
    EXPECT_TRUE(mock_database_query("SELECT * FROM test_table", result));
    EXPECT_TRUE(result.is_array());
    EXPECT_EQ(result.size(), 1);
}
```

## Test Utilities

### Random Data Generation
```cpp
std::string random_str = test_utils::generate_random_string(10);
int random_num = test_utils::generate_random_int(1, 100);
nlohmann::json random_json = test_utils::generate_random_json();
```

### File System Operations
```cpp
std::string temp_file = test_utils::create_temp_file("test content");
std::string temp_dir;
test_utils::create_temp_directory("test_prefix", temp_dir);
// Files are automatically cleaned up
```

### Time Control
```cpp
// Freeze time for deterministic testing
test_utils::advance_test_time(std::chrono::hours(1));
// Time automatically resets after test
```

### Assertions
```cpp
ASSERT_JSON_EQ(actual, expected);
ASSERT_JSON_CONTAINS(json, "key");
EXPECT_JSON_EQ(actual, expected);
EXPECT_JSON_CONTAINS(json, "key");
```

## Running Tests

### Build Tests
```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make regulens_tests
```

### Run All Tests
```bash
./regulens_tests
```

### Run Specific Tests
```bash
./regulens_tests --gtest_filter="*DatabaseTest*"
```

### Run with GTest Options
```bash
./regulens_tests --gtest_output=xml:test_results.xml
```

## Best Practices

### Test Organization
1. **Use appropriate fixtures**: Choose the most specific fixture for your test needs
2. **Keep tests isolated**: Each test should be independent and not rely on others
3. **Use descriptive names**: Test names should clearly indicate what they're testing
4. **Mock external dependencies**: Never make real network calls or database connections in tests

### Test Structure
```cpp
TEST_F(MyTestFixture, TestName) {
    // Arrange - Set up test data and mocks
    auto test_data = create_mock_test_data();

    // Act - Execute the code under test
    auto result = my_function_under_test(test_data);

    // Assert - Verify expected behavior
    EXPECT_EQ(result.status, "success");
    EXPECT_JSON_CONTAINS(result.data, "expected_field");
}
```

### Resource Management
- **Automatic cleanup**: TestEnvironment automatically cleans up resources
- **RAII pattern**: Use TestEnvironmentGuard for complex test scenarios
- **Isolation verification**: Use `TestEnvironment::verify_isolation()` to ensure proper isolation

### Performance Testing
```cpp
TEST_F(RegulensTest, PerformanceTest) {
    auto start = std::chrono::high_resolution_clock::now();

    // Execute code under test
    perform_operation();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Verify performance requirements
    EXPECT_LT(duration.count(), 100); // Less than 100ms
}
```

## Integration with CI/CD

The test infrastructure is designed to work seamlessly with CI/CD pipelines:

- **No external dependencies**: Tests run without requiring real databases, APIs, or services
- **Deterministic results**: Isolated environment ensures consistent test outcomes
- **Fast execution**: Mock implementations provide fast test execution
- **Comprehensive coverage**: Fixtures cover all major system components

## Extending the Test Infrastructure

### Adding New Mock Services
```cpp
// In your test
TestEnvironment::get_instance().register_mock_service("my_service", mock_instance);

// In your code under test
auto service = TestEnvironment::get_instance().get_mock_service("my_service");
if (service && TestEnvironment::get_instance().is_test_mode()) {
    // Use mock service
} else {
    // Use real service
}
```

### Creating Custom Fixtures
```cpp
class MyCustomTest : public RegulensTest {
protected:
    void SetUp() override {
        RegulensTest::SetUp();
        // Custom setup logic
    }

    void TearDown() override {
        // Custom cleanup logic
        RegulensTest::TearDown();
    }
};
```

This test infrastructure provides a solid foundation for maintaining high-quality, reliable tests throughout the development lifecycle of the Regulens system.
