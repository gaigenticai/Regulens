#include "test_framework.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <chrono>

#include "../../shared/event_processor.hpp"
#include "../../shared/knowledge_base.hpp"
#include "../../shared/models/compliance_event.hpp"
#include "../../shared/models/agent_decision.hpp"
#include "../../shared/models/regulatory_change.hpp"
#include "../../core/agent/agent_orchestrator.hpp"
#include "../../core/agent/compliance_agent.hpp"
#include "../../regulatory_monitor/regulatory_monitor.hpp"
#include "../../regulatory_monitor/regulatory_source.hpp"
#include "../../shared/tool_integration/tools/mcp_tool.hpp"
#include "../../shared/network/http_client.hpp"

namespace regulens {

// Base test fixture implementation
void RegulensTest::SetUp() {
    // Initialize test environment
    TestEnvironment::get_instance().initialize();
    TestEnvironment::get_instance().set_test_mode(true);

    // Create test logger
    test_logger_ = std::make_shared<StructuredLogger>();

    // Create test configuration
    test_config_ = std::make_shared<ConfigurationManager>();
    test_config_->initialize(0, nullptr);

    // Clear any existing environment variable overrides
    reset_test_environment();
}

void RegulensTest::TearDown() {
    // Restore original environment
    reset_test_environment();

    // Cleanup test environment
    TestEnvironment::get_instance().cleanup();
}

void RegulensTest::set_test_environment_variable(const std::string& key, const std::string& value) {
    TestEnvironment::get_instance().set_environment_variable(key, value);
}

void RegulensTest::clear_test_environment_variable(const std::string& key) {
    TestEnvironment::get_instance().restore_environment_variables();
}

void RegulensTest::reset_test_environment() {
    TestEnvironment::get_instance().restore_environment_variables();
    TestEnvironment::get_instance().clear_config_overrides();
    TestEnvironment::get_instance().clear_test_data();
}

std::shared_ptr<StructuredLogger> RegulensTest::get_test_logger() {
    return test_logger_;
}

std::shared_ptr<ConfigurationManager> RegulensTest::get_test_config() {
    return test_config_;
}

nlohmann::json RegulensTest::create_mock_regulatory_change() {
    RegulatoryChangeMetadata metadata;
    metadata.regulatory_body = "SEC";
    metadata.document_type = "Rule";
    metadata.keywords = {"compliance", "regulation", "test"};
    metadata.severity = RegulatorySeverity::HIGH;
    metadata.effective_date = std::chrono::system_clock::now() + std::chrono::hours(24);

    RegulatoryChange change(
        "test_source_" + test_utils::generate_random_string(5),
        "Test Regulatory Change " + test_utils::generate_random_string(10),
        "https://example.com/change/" + test_utils::generate_random_string(8),
        metadata
    );

    return change.to_json();
}

nlohmann::json RegulensTest::create_mock_compliance_event() {
    ComplianceEvent event(
        EventType::REGULATORY_CHANGE_DETECTED,
        EventSeverity::HIGH,
        "Mock compliance event for testing",
        {"test", "mock", "compliance"}
    );

    return event.to_json();
}

nlohmann::json RegulensTest::create_mock_agent_decision() {
    AgentDecision decision(
        "test_decision_" + test_utils::generate_random_string(5),
        DecisionType::COMPLIANCE_CHECK,
        "Test agent decision",
        0.85f,
        std::chrono::system_clock::now(),
        {{"test_reasoning", "Mock decision for testing"}}
    );

    return decision.to_json();
}

// Database test fixture implementation
void DatabaseTest::SetUp() {
    RegulensTest::SetUp();
    reset_mock_database();
}

void DatabaseTest::TearDown() {
    reset_mock_database();
    RegulensTest::TearDown();
}

bool DatabaseTest::mock_database_query(const std::string& query, nlohmann::json& result) {
    // Simple mock implementation - in real tests, this would simulate database responses
    if (query.find("SELECT") != std::string::npos) {
        result = nlohmann::json::array();
        // Add mock data based on query
        if (query.find("regulatory_changes") != std::string::npos) {
            result.push_back(create_mock_regulatory_change());
        }
        return true;
    }
    return false;
}

bool DatabaseTest::mock_database_insert(const std::string& table, const nlohmann::json& data) {
    // Store in mock state
    mock_db_state_[table].push_back(data);
    return true;
}

bool DatabaseTest::mock_database_update(const std::string& table, const nlohmann::json& data,
                                       const std::string& where_clause) {
    // Simple mock update - find and replace first matching record
    if (mock_db_state_.contains(table)) {
        auto& records = mock_db_state_[table];
        for (auto& record : records) {
            // Basic mock where clause matching
            if (where_clause.find("id") != std::string::npos) {
                record.update(data);
                return true;
            }
        }
    }
    return false;
}

void DatabaseTest::reset_mock_database() {
    mock_db_state_.clear();
}

// API test fixture implementation
void ApiTest::SetUp() {
    RegulensTest::SetUp();
    clear_mock_responses();
}

void ApiTest::TearDown() {
    clear_mock_responses();
    RegulensTest::TearDown();
}

void ApiTest::mock_api_response(const std::string& url, const std::string& method,
                               int status_code, const nlohmann::json& response) {
    std::string key = method + ":" + url;
    mock_responses_[key] = {
        {"status_code", status_code},
        {"response", response},
        {"is_error", false}
    };
}

void ApiTest::mock_api_error(const std::string& url, const std::string& method,
                            int status_code, const std::string& error_message) {
    std::string key = method + ":" + url;
    mock_responses_[key] = {
        {"status_code", status_code},
        {"error_message", error_message},
        {"is_error", true}
    };
}

void ApiTest::clear_mock_responses() {
    mock_responses_.clear();
}

// Agent orchestration test fixture implementation
void AgentOrchestrationTest::SetUp() {
    RegulensTest::SetUp();
    test_orchestrator_ = create_test_orchestrator();
}

void AgentOrchestrationTest::TearDown() {
    test_orchestrator_.reset();
    mock_agents_.clear();
    RegulensTest::TearDown();
}

std::unique_ptr<AgentOrchestrator> AgentOrchestrationTest::create_test_orchestrator() {
    return AgentOrchestrator::create_for_testing();
}

std::shared_ptr<ComplianceAgent> AgentOrchestrationTest::create_mock_agent(const std::string& agent_type) {
    // Create a basic mock agent for testing
    class MockComplianceAgent : public ComplianceAgent {
    public:
        MockComplianceAgent(const std::string& type, const std::string& name)
            : ComplianceAgent(type, name, AgentCapabilities{}) {}

        AgentDecision process_event(const ComplianceEvent& event) override {
            return AgentDecision(
                "mock_decision_" + test_utils::generate_random_string(5),
                DecisionType::COMPLIANCE_CHECK,
                "Mock agent decision",
                0.8f,
                std::chrono::system_clock::now()
            );
        }

        bool can_handle_event(EventType type) const override {
            return type == EventType::COMPLIANCE_VIOLATION_DETECTED;
        }

        AgentStatus get_status() const override {
            return AgentStatus{
                agent_type_,
                AgentHealth::HEALTHY,
                "Mock agent running",
                std::chrono::system_clock::now(),
                0, 0, 0.0f
            };
        }
    };

    auto agent = std::make_shared<MockComplianceAgent>(agent_type,
        "Mock " + agent_type + " Agent");
    mock_agents_.push_back(agent);
    return agent;
}

AgentTask AgentOrchestrationTest::create_test_task(const std::string& agent_type) {
    ComplianceEvent event(EventType::COMPLIANCE_VIOLATION_DETECTED,
                         EventSeverity::HIGH, "Test violation", {"test"});

    return AgentTask(test_utils::generate_random_string(10), agent_type, event);
}

void AgentOrchestrationTest::verify_task_execution(const AgentTask& task, const TaskResult& result) {
    ASSERT_FALSE(task.task_id.empty());
    ASSERT_FALSE(task.agent_type.empty());
    ASSERT_TRUE(result.success || !result.error_message.empty());
}

// Regulatory monitoring test fixture implementation
void RegulatoryMonitoringTest::SetUp() {
    RegulensTest::SetUp();
    mock_changes_.clear();
}

void RegulatoryMonitoringTest::TearDown() {
    mock_changes_.clear();
    RegulensTest::TearDown();
}

std::shared_ptr<RegulatorySource> RegulatoryMonitoringTest::create_mock_regulatory_source(
    RegulatorySourceType type, const std::string& source_id) {

    // Create a mock regulatory source
    class MockRegulatorySource : public RegulatorySource {
    public:
        MockRegulatorySource(const std::string& id, RegulatorySourceType type,
                           std::shared_ptr<ConfigurationManager> config,
                           std::shared_ptr<StructuredLogger> logger)
            : RegulatorySource(id, "Mock " + id, type, config, logger) {}

        bool initialize() override { return true; }

        std::vector<RegulatoryChange> check_for_changes() override {
            std::vector<RegulatoryChange> changes;
            // Return mock changes if any are set
            return changes;
        }

        nlohmann::json get_configuration() const override {
            return {{"type", "mock"}, {"source_id", get_source_id()}};
        }

        bool test_connectivity() override { return true; }
    };

    return std::make_shared<MockRegulatorySource>(source_id, type,
        get_test_config(), get_test_logger());
}

void RegulatoryMonitoringTest::mock_regulatory_change(const std::string& source_id,
                                                     const RegulatoryChange& change) {
    mock_changes_[source_id].push_back(change);
}

std::unique_ptr<RegulatoryMonitor> RegulatoryMonitoringTest::create_test_monitor() {
    auto monitor = std::make_unique<RegulatoryMonitor>(get_test_config(), get_test_logger());
    monitor->initialize();
    return monitor;
}

void RegulatoryMonitoringTest::verify_monitor_state(const RegulatoryMonitor& monitor) {
    // Verify monitor is in a valid state
    auto status = monitor.get_status();
    ASSERT_TRUE(status.contains("healthy"));
}

// MCP tool test fixture implementation
void McpToolTest::SetUp() {
    ApiTest::SetUp();
    mcp_responses_.clear();
}

void McpToolTest::TearDown() {
    mcp_responses_.clear();
    ApiTest::TearDown();
}

std::unique_ptr<MCPToolIntegration> McpToolTest::create_test_mcp_tool() {
    ToolConfig config;
    config.tool_id = "test_mcp_tool";
    config.tool_name = "Test MCP Tool";
    config.category = ToolCategory::MCP_TOOLS;
    config.timeout = std::chrono::seconds(30);
    config.metadata["mcp_server_url"] = "http://localhost:3000";

    return std::make_unique<MCPToolIntegration>(config, get_test_logger().get());
}

void McpToolTest::mock_mcp_server_response(const std::string& method,
                                          const nlohmann::json& request,
                                          const nlohmann::json& response) {
    std::string key = method + ":" + request.dump();
    mcp_responses_[key] = response;
}

void McpToolTest::verify_mcp_protocol(const std::string& operation, const ToolResult& result) {
    ASSERT_FALSE(operation.empty());
    // Verify MCP protocol compliance
    if (result.success) {
        ASSERT_TRUE(result.data.is_object() || result.data.is_array());
    } else {
        ASSERT_FALSE(result.error_message.empty());
    }
}

// Knowledge base test fixture implementation
void KnowledgeBaseTest::SetUp() {
    DatabaseTest::SetUp();
    mock_documents_.clear();
}

void KnowledgeBaseTest::TearDown() {
    mock_documents_.clear();
    DatabaseTest::TearDown();
}

std::unique_ptr<KnowledgeBase> KnowledgeBaseTest::create_test_knowledge_base() {
    auto kb = std::make_unique<KnowledgeBase>(get_test_config(), get_test_logger());
    kb->initialize();
    return kb;
}

void KnowledgeBaseTest::populate_mock_knowledge_base(const std::vector<nlohmann::json>& documents) {
    mock_documents_ = documents;
    // In a real implementation, this would populate the test database
}

void KnowledgeBaseTest::verify_search_results(const std::vector<std::string>& queries,
                                             const std::vector<nlohmann::json>& expected_results) {
    ASSERT_EQ(queries.size(), expected_results.size());

    for (size_t i = 0; i < queries.size(); ++i) {
        const auto& query = queries[i];
        const auto& result = expected_results[i];

        // Basic validation
        ASSERT_FALSE(query.empty()) << "Query at index " << i << " is empty";
        ASSERT_TRUE(result.is_object() || result.is_array()) << "Result at index " << i << " is not a valid JSON object or array";

        // Complex verification logic for search results
        if (result.is_array()) {
            // Verify array results contain expected structure
            for (const auto& item : result) {
                ASSERT_TRUE(item.is_object()) << "Array result item is not an object";

                // Check for required fields in search results
                ASSERT_TRUE(item.contains("id") || item.contains("document_id")) << "Search result missing ID field";
                ASSERT_TRUE(item.contains("content") || item.contains("text")) << "Search result missing content field";
                ASSERT_TRUE(item.contains("score") || item.contains("relevance")) << "Search result missing score/relevance field";

                // Verify score is within valid range
                if (item.contains("score")) {
                    double score = item["score"];
                    ASSERT_GE(score, 0.0) << "Search score is negative";
                    ASSERT_LE(score, 1.0) << "Search score exceeds maximum value";
                }

                // Verify content relevance to query
                std::string content;
                if (item.contains("content")) content = item["content"];
                else if (item.contains("text")) content = item["text"];

                // Check if query keywords appear in content (case-insensitive)
                std::string lower_content = content;
                std::string lower_query = query;
                std::transform(lower_content.begin(), lower_content.end(), lower_content.begin(), ::tolower);
                std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);

                // Split query into keywords and check relevance
                std::istringstream iss(lower_query);
                std::string keyword;
                int matched_keywords = 0;
                int total_keywords = 0;

                while (iss >> keyword) {
                    total_keywords++;
                    if (lower_content.find(keyword) != std::string::npos) {
                        matched_keywords++;
                    }
                }

                // At least 50% of keywords should match for relevance
                if (total_keywords > 0) {
                    double relevance_ratio = static_cast<double>(matched_keywords) / total_keywords;
                    ASSERT_GE(relevance_ratio, 0.5) << "Search result relevance too low for query: " << query;
                }

                // Verify metadata if present
                if (item.contains("metadata")) {
                    const auto& metadata = item["metadata"];
                    ASSERT_TRUE(metadata.is_object()) << "Metadata is not an object";

                    // Check for common metadata fields
                    if (metadata.contains("timestamp")) {
                        ASSERT_TRUE(metadata["timestamp"].is_string() || metadata["timestamp"].is_number())
                            << "Timestamp format invalid";
                    }
                    if (metadata.contains("source")) {
                        ASSERT_TRUE(metadata["source"].is_string()) << "Source is not a string";
                    }
                }
            }

            // Verify results are ordered by relevance (highest score first)
            if (result.size() > 1) {
                for (size_t j = 0; j < result.size() - 1; ++j) {
                    double current_score = result[j].contains("score") ? result[j]["score"].get<double>() : 0.0;
                    double next_score = result[j + 1].contains("score") ? result[j + 1]["score"].get<double>() : 0.0;
                    ASSERT_GE(current_score, next_score) << "Search results not properly ordered by score";
                }
            }

        } else if (result.is_object()) {
            // Verify single result object
            ASSERT_TRUE(result.contains("total_results") || result.contains("count")) << "Result object missing count field";
            ASSERT_TRUE(result.contains("results")) << "Result object missing results array";

            const auto& results_array = result["results"];
            ASSERT_TRUE(results_array.is_array()) << "Results field is not an array";

            // Recursively verify the results array
            verify_search_results({query}, {results_array});
        }

        // Verify query was properly processed (no SQL injection, proper sanitization)
        ASSERT_EQ(query.find("--"), std::string::npos) << "Query contains potential SQL comment injection";
        ASSERT_EQ(query.find(";"), std::string::npos) << "Query contains potential SQL injection";
    }
}

// Test utilities implementation
namespace test_utils {

std::string generate_random_string(size_t length) {
    static const std::string charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> distribution(0, charset.size() - 1);

    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += charset[distribution(generator)];
    }
    return result;
}

int generate_random_int(int min, int max) {
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> distribution(min, max);
    return distribution(generator);
}

nlohmann::json generate_random_json() {
    nlohmann::json json;
    json["id"] = generate_random_string(8);
    json["value"] = generate_random_int(1, 100);
    json["name"] = "test_" + generate_random_string(5);
    return json;
}

bool create_temp_directory(const std::string& prefix, std::string& temp_path) {
    try {
        std::filesystem::path temp_dir = std::filesystem::temp_directory_path() /
                                        (prefix + "_" + generate_random_string(8));
        if (std::filesystem::create_directories(temp_dir)) {
            temp_path = temp_dir.string();
            return true;
        }
    } catch (const std::exception&) {
        // Ignore errors
    }
    return false;
}

bool remove_temp_directory(const std::string& path) {
    try {
        std::filesystem::remove_all(path);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::string create_temp_file(const std::string& content) {
    std::string temp_path;
    if (create_temp_directory("regulens_test_file", temp_path)) {
        std::filesystem::path file_path = std::filesystem::path(temp_path) / "test_file.txt";
        std::ofstream file(file_path);
        if (file.is_open()) {
            file << content;
            file.close();
            return file_path.string();
        }
    }
    return "";
}

std::chrono::system_clock::time_point get_test_timestamp() {
    return std::chrono::system_clock::now();
}

void advance_test_time(std::chrono::seconds duration) {
    TestTimeController::get_instance().advance_time(duration);
}

} // namespace test_utils

} // namespace regulens
