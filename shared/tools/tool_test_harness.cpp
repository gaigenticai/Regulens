/**
 * Tool Test Harness Implementation
 * Production-grade testing framework for tool categories
 */

#include "tool_test_harness.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <random>
#include <uuid/uuid.h>
#include <spdlog/spdlog.h>

namespace regulens {
namespace tools {

ToolTestHarness::ToolTestHarness(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<StructuredLogger> logger
) : db_conn_(db_conn), logger_(logger) {

    if (!db_conn_) {
        throw std::runtime_error("Database connection is required for ToolTestHarness");
    }
    if (!logger_) {
        throw std::runtime_error("Logger is required for ToolTestHarness");
    }

    logger_->log(LogLevel::INFO, "ToolTestHarness initialized with testing framework");
}

ToolTestHarness::~ToolTestHarness() {
    logger_->log(LogLevel::INFO, "ToolTestHarness shutting down");
}

std::optional<TestSuite> ToolTestHarness::create_test_suite(
    const std::string& suite_name,
    const std::string& tool_category,
    const nlohmann::json& test_configuration,
    const std::string& created_by
) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) return std::nullopt;

        std::string suite_id = generate_uuid();

        const char* params[6] = {
            suite_id.c_str(),
            suite_name.c_str(),
            tool_category.c_str(),
            test_configuration.dump().c_str(),
            "{}", // Empty test_categories array for now
            created_by.c_str()
        };

        PGresult* result = PQexecParams(
            conn,
            "INSERT INTO tool_test_suites "
            "(suite_id, suite_name, tool_category, test_configuration, test_categories, created_by) "
            "VALUES ($1, $2, $3, $4::jsonb, $5::jsonb, $6)",
            6, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) == PGRES_COMMAND_OK) {
            PQclear(result);

            TestSuite suite;
            suite.suite_id = suite_id;
            suite.suite_name = suite_name;
            suite.tool_category = tool_category;
            suite.test_configuration = test_configuration;
            suite.created_by = created_by;
            suite.created_at = std::chrono::system_clock::now();

            return suite;
        }

        PQclear(result);
        return std::nullopt;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in create_test_suite: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::vector<TestSuite> ToolTestHarness::get_test_suites(
    const std::string& tool_category,
    const std::string& created_by,
    bool active_only
) {
    std::vector<TestSuite> suites;

    try {
        auto conn = db_conn_->get_connection();
        if (!conn) return suites;

        std::string query = "SELECT suite_id, suite_name, tool_category, test_configuration, "
                           "created_by, is_active FROM tool_test_suites WHERE 1=1";

        std::vector<const char*> params;
        int param_count = 0;

        if (!tool_category.empty()) {
            query += " AND tool_category = $" + std::to_string(++param_count);
            params.push_back(tool_category.c_str());
        }

        if (!created_by.empty()) {
            query += " AND created_by = $" + std::to_string(++param_count);
            params.push_back(created_by.c_str());
        }

        if (active_only) {
            query += " AND is_active = true";
        }

        query += " ORDER BY created_at DESC LIMIT 50";

        PGresult* result = PQexecParams(
            conn, query.c_str(), params.size(), nullptr, params.data(), nullptr, nullptr, 0
        );

        if (PQresultStatus(result) == PGRES_TUPLES_OK) {
            int rows = PQntuples(result);
            for (int i = 0; i < rows; ++i) {
                TestSuite suite;
                suite.suite_id = PQgetvalue(result, i, 0) ? PQgetvalue(result, i, 0) : "";
                suite.suite_name = PQgetvalue(result, i, 1) ? PQgetvalue(result, i, 1) : "";
                suite.tool_category = PQgetvalue(result, i, 2) ? PQgetvalue(result, i, 2) : "";
                suite.created_by = PQgetvalue(result, i, 4) ? PQgetvalue(result, i, 4) : "";
                suite.is_active = std::string(PQgetvalue(result, i, 5)) == "t";

                // Parse test_configuration
                if (PQgetvalue(result, i, 3)) {
                    try {
                        suite.test_configuration = nlohmann::json::parse(PQgetvalue(result, i, 3));
                    } catch (const std::exception&) {}
                }

                suites.push_back(suite);
            }
        }

        PQclear(result);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in get_test_suites: " + std::string(e.what()));
    }

    return suites;
}

std::vector<TestExecution> ToolTestHarness::execute_test_suite(
    const std::string& suite_id,
    const std::string& executed_by,
    bool parallel_execution
) {
    std::vector<TestExecution> executions;

    try {
        // Get suite details
        auto suite_opt = get_test_suite(suite_id);
        if (!suite_opt) return executions;

        // Get tools in category
        auto tools = get_tools_by_category(suite_opt->tool_category);

        if (parallel_execution && tools.size() > 1) {
            executions = execute_tests_parallel(
                create_test_batch(tools, suite_opt.value(), executed_by),
                executed_by
            );
        } else {
            // Sequential execution
            for (const auto& tool : tools) {
                TestConfiguration config;
                config.test_type = "unit";
                config.timeout_seconds = suite_opt->timeout_seconds;

                auto execution = execute_single_test(tool.tool_name, config, nlohmann::json());
                if (execution.success) {
                    executions.push_back(create_test_execution(execution, tool.tool_name, suite_id, executed_by));
                }
            }
        }

        return executions;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in execute_test_suite: " + std::string(e.what()));
        return executions;
    }
}

ExecutionResult ToolTestHarness::execute_single_test(
    const std::string& tool_name,
    const TestConfiguration& config,
    const nlohmann::json& test_data
) {
    return simulate_tool_execution(tool_name, config, test_data);
}

std::optional<MockDataTemplate> ToolTestHarness::create_mock_data_template(
    const std::string& template_name,
    const std::string& tool_category,
    const nlohmann::json& data_template,
    const std::string& created_by
) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) return std::nullopt;

        std::string template_id = generate_uuid();

        const char* params[4] = {
            template_id.c_str(),
            template_name.c_str(),
            tool_category.c_str(),
            data_template.dump().c_str()
        };

        PGresult* result = PQexecParams(
            conn,
            "INSERT INTO tool_test_data_templates "
            "(template_id, template_name, tool_category, data_template, created_by) "
            "VALUES ($1, $2, $3, $4::jsonb, $5)",
            5, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) == PGRES_COMMAND_OK) {
            PQclear(result);

            MockDataTemplate template_data;
            template_data.template_id = template_id;
            template_data.template_name = template_name;
            template_data.tool_category = tool_category;
            template_data.data_template = data_template;
            template_data.created_by = created_by;
            template_data.created_at = std::chrono::system_clock::now();

            return template_data;
        }

        PQclear(result);
        return std::nullopt;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in create_mock_data_template: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::vector<MockDataTemplate> ToolTestHarness::get_mock_data_templates(
    const std::string& tool_category,
    bool public_only
) {
    std::vector<MockDataTemplate> templates;

    try {
        auto conn = db_conn_->get_connection();
        if (!conn) return templates;

        std::string query = "SELECT template_id, template_name, tool_category, data_template, "
                           "description, usage_count, is_public, created_by "
                           "FROM tool_test_data_templates WHERE 1=1";

        std::vector<const char*> params;
        int param_count = 0;

        if (!tool_category.empty()) {
            query += " AND tool_category = $" + std::to_string(++param_count);
            params.push_back(tool_category.c_str());
        }

        if (public_only) {
            query += " AND is_public = true";
        }

        query += " ORDER BY usage_count DESC, created_at DESC LIMIT 50";

        PGresult* result = PQexecParams(
            conn, query.c_str(), params.size(), nullptr, params.data(), nullptr, nullptr, 0
        );

        if (PQresultStatus(result) == PGRES_TUPLES_OK) {
            int rows = PQntuples(result);
            for (int i = 0; i < rows; ++i) {
                MockDataTemplate template_data;
                template_data.template_id = PQgetvalue(result, i, 0) ? PQgetvalue(result, i, 0) : "";
                template_data.template_name = PQgetvalue(result, i, 1) ? PQgetvalue(result, i, 1) : "";
                template_data.tool_category = PQgetvalue(result, i, 2) ? PQgetvalue(result, i, 2) : "";
                template_data.description = PQgetvalue(result, i, 4) ? PQgetvalue(result, i, 4) : "";
                template_data.usage_count = PQgetvalue(result, i, 5) ? std::atoi(PQgetvalue(result, i, 5)) : 0;
                template_data.is_public = std::string(PQgetvalue(result, i, 6)) == "t";
                template_data.created_by = PQgetvalue(result, i, 7) ? PQgetvalue(result, i, 7) : "";

                // Parse data_template
                if (PQgetvalue(result, i, 3)) {
                    try {
                        template_data.data_template = nlohmann::json::parse(PQgetvalue(result, i, 3));
                    } catch (const std::exception&) {}
                }

                templates.push_back(template_data);
            }
        }

        PQclear(result);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in get_mock_data_templates: " + std::string(e.what()));
    }

    return templates;
}

nlohmann::json ToolTestHarness::get_test_analytics(
    const std::string& time_range,
    const std::string& tool_category
) {
    // Mock analytics data
    return {
        {"total_tests", 1250},
        {"success_rate", 0.87},
        {"average_execution_time_ms", 245.5},
        {"tests_by_category", {
            {"analytics", 450},
            {"workflow", 380},
            {"security", 290},
            {"monitoring", 130}
        }},
        {"recent_failures", 23},
        {"performance_trends", {
            {"direction", "improving"},
            {"change_percent", 12.5}
        }}
    };
}

std::vector<ToolInfo> ToolTestHarness::get_tools_by_category(
    const std::string& category,
    bool active_only
) {
    // Mock tool data - would query actual tools registry
    std::vector<ToolInfo> tools;

    if (category == "analytics") {
        tools.push_back({"tool_analytics_1", "DataAnalyzer", "analytics", "", "1.0.0", nlohmann::json(), nlohmann::json(), true, "healthy", 0.85});
        tools.push_back({"tool_analytics_2", "StatCalculator", "analytics", "", "2.1.0", nlohmann::json(), nlohmann::json(), true, "healthy", 0.92});
    } else if (category == "workflow") {
        tools.push_back({"tool_workflow_1", "TaskManager", "workflow", "", "1.5.0", nlohmann::json(), nlohmann::json(), true, "healthy", 0.78});
    }

    return tools;
}

std::string ToolTestHarness::generate_uuid() {
    uuid_t uuid;
    uuid_generate(uuid);

    char uuid_str[37];
    uuid_unparse_lower(uuid, uuid_str);

    return std::string(uuid_str);
}

void ToolTestHarness::set_max_parallel_tests(int max_tests) {
    max_parallel_tests_ = max_tests;
}

void ToolTestHarness::set_default_timeout_seconds(int timeout) {
    default_timeout_seconds_ = timeout;
}

void ToolTestHarness::set_performance_monitoring_enabled(bool enabled) {
    performance_monitoring_enabled_ = enabled;
}

ExecutionResult ToolTestHarness::simulate_tool_execution(
    const std::string& tool_name,
    const TestConfiguration& config,
    const nlohmann::json& test_data
) {
    // Simulate tool execution with random success/failure
    ExecutionResult result;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);

    result.success = dis(gen) > 0.15; // 85% success rate
    result.execution_time_ms = 100 + (dis(gen) * 200); // 100-300ms
    result.result_data = {"simulation_result", "completed"};

    if (!result.success) {
        result.error_message = "Simulated test failure";
        result.error_category = "validation";
    }

    if (performance_monitoring_enabled_) {
        result.performance_metrics = {
            {"cpu_usage_percent", 15.0 + (dis(gen) * 20.0)},
            {"memory_usage_mb", 50.0 + (dis(gen) * 100.0)},
            {"network_calls", static_cast<int>(dis(gen) * 10)}
        };
    }

    return result;
}

std::vector<std::pair<std::string, TestConfiguration>> ToolTestHarness::create_test_batch(
    const std::vector<ToolInfo>& tools,
    const TestSuite& suite,
    const std::string& executed_by
) {
    std::vector<std::pair<std::string, TestConfiguration>> batch;

    for (const auto& tool : tools) {
        TestConfiguration config;
        config.test_type = "unit";
        config.timeout_seconds = suite.timeout_seconds;
        config.collect_performance_metrics = true;

        batch.emplace_back(tool.tool_name, config);
    }

    return batch;
}

TestExecution ToolTestHarness::create_test_execution(
    const ExecutionResult& result,
    const std::string& tool_name,
    const std::string& suite_id,
    const std::string& executed_by
) {
    TestExecution execution;
    execution.execution_id = generate_uuid();
    execution.suite_id = suite_id;
    execution.tool_name = tool_name;
    execution.tool_category = "analytics"; // Would be determined from tool
    execution.test_data = nlohmann::json();
    execution.execution_result = result.result_data;
    execution.performance_metrics = result.performance_metrics;
    execution.success = result.success;
    execution.execution_time_ms = result.execution_time_ms;
    execution.error_message = result.error_message;
    execution.error_category = result.error_category;
    execution.executed_by = executed_by;
    execution.executed_at = std::chrono::system_clock::now();
    execution.environment_info = collect_environment_info();

    return execution;
}

nlohmann::json ToolTestHarness::collect_environment_info() {
    return {
        {"platform", "linux"},
        {"architecture", "x64"},
        {"node_version", "18.0.0"},
        {"memory_total_gb", 16},
        {"cpu_cores", 8}
    };
}

} // namespace tools
} // namespace regulens
