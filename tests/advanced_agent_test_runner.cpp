#include "advanced_agent_tests.hpp"

#include <iostream>
#include <iomanip>
#include <chrono>
#include <string>

namespace regulens {

/**
 * @brief Main test runner for comprehensive Level 3 and Level 4 agent capability tests
 *
 * Provides command-line interface for running different test categories and
 * generating detailed test reports.
 */
class AdvancedAgentTestRunner {
public:
    AdvancedAgentTestRunner() = default;

    /**
     * @brief Run tests based on command line arguments
     * @param argc Argument count
     * @param argv Argument array
     * @return Exit code (0 for success, 1 for failure)
     */
    int run(int argc, char* argv[]) {
        print_banner();

        // Parse command line arguments
        TestConfig config = parse_arguments(argc, argv);

        // Initialize test suite
        AdvancedAgentTestSuite test_suite;
        if (!test_suite.initialize()) {
            std::cerr << "❌ Failed to initialize test suite" << std::endl;
            return 1;
        }

        // Run tests
        auto start_time = std::chrono::high_resolution_clock::now();

        nlohmann::json results;
        if (!config.category.empty()) {
            std::cout << "\n🎯 Running test category: " << config.category << std::endl;
            results = test_suite.run_test_category(config.category);
        } else {
            std::cout << "\n🚀 Running all Level 3 and Level 4 agent capability tests" << std::endl;
            results = test_suite.run_all_tests();
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        // Display results
        display_results(results, total_duration);

        // Generate report if requested
        if (config.generate_report) {
            generate_report(results, config.report_file);
        }

        // Return appropriate exit code
        int passed_tests = results["summary"]["passed_tests"];
        int total_tests = results["summary"]["total_tests"];

        std::cout << "\n" << std::string(60, '=') << std::endl;
        if (passed_tests == total_tests) {
            std::cout << "🎉 ALL TESTS PASSED! (" << passed_tests << "/" << total_tests << ")" << std::endl;
            return 0;
        } else {
            std::cout << "❌ SOME TESTS FAILED (" << passed_tests << "/" << total_tests << ")" << std::endl;
            return 1;
        }
    }

private:
    struct TestConfig {
        std::string category;
        bool generate_report = false;
        std::string report_file = "test_report.json";
        bool verbose = false;
    };

    void print_banner() {
        std::cout << R"(
╔══════════════════════════════════════════════════════════════════════════════╗
║                     Regulens Advanced Agent Test Suite                      ║
║                 Level 3 & Level 4 Agent Capability Tests                    ║
╠══════════════════════════════════════════════════════════════════════════════╣
║ Tests: Pattern Recognition • Feedback Learning • Human-AI Collaboration     ║
║        Error Handling • Activity Feeds • Decision Trees                     ║
║        Regulatory Monitoring • MCP Tools • Autonomous Decisions             ║
║        Multi-Agent Orchestration • Continuous Learning                      ║
╚══════════════════════════════════════════════════════════════════════════════╝
)" << std::endl;
    }

    TestConfig parse_arguments(int argc, char* argv[]) {
        TestConfig config;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--category" || arg == "-c") {
                if (i + 1 < argc) {
                    config.category = argv[++i];
                }
            } else if (arg == "--report" || arg == "-r") {
                config.generate_report = true;
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    config.report_file = argv[++i];
                }
            } else if (arg == "--verbose" || arg == "-v") {
                config.verbose = true;
            } else if (arg == "--help" || arg == "-h") {
                print_help();
                exit(0);
            } else if (arg.substr(0, 2) == "--") {
                std::cerr << "Unknown option: " << arg << std::endl;
                print_help();
                exit(1);
            }
        }

        return config;
    }

    void print_help() {
        std::cout << "Usage: advanced_agent_test_runner [options]\n\n"
                  << "Options:\n"
                  << "  -c, --category <name>    Run specific test category\n"
                  << "  -r, --report [file]      Generate JSON report (default: test_report.json)\n"
                  << "  -v, --verbose           Enable verbose output\n"
                  << "  -h, --help              Show this help message\n\n"
                  << "Available test categories:\n"
                  << "  pattern_recognition     Pattern recognition and analysis\n"
                  << "  feedback               Feedback collection and learning\n"
                  << "  collaboration          Human-AI collaboration\n"
                  << "  error_handling         Error handling and recovery\n"
                  << "  activity_feed          Real-time activity feeds\n"
                  << "  decision_trees         Decision tree visualization\n"
                  << "  regulatory             Regulatory monitoring\n"
                  << "  mcp_tools             MCP tool integration\n"
                  << "  autonomous            Autonomous decision making\n"
                  << "  orchestration          Multi-agent orchestration\n"
                  << "  learning              Continuous learning systems\n"
                  << "  integration           End-to-end integration tests\n"
                  << "  performance           Performance and scalability\n"
                  << "  edge_cases            Edge cases and error conditions\n\n"
                  << "Examples:\n"
                  << "  advanced_agent_test_runner                          # Run all tests\n"
                  << "  advanced_agent_test_runner -c pattern_recognition  # Run pattern tests\n"
                  << "  advanced_agent_test_runner --report results.json   # Generate report\n"
                  << std::endl;
    }

    void display_results(const nlohmann::json& results,
                        std::chrono::milliseconds total_duration) {
        const auto& summary = results["summary"];

        int total_tests = summary["total_tests"];
        int passed_tests = summary["passed_tests"];
        int failed_tests = summary["failed_tests"];
        double success_rate = summary["success_rate_percent"];
        double avg_duration = summary["average_duration_ms"];

        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "📊 TEST RESULTS SUMMARY" << std::endl;
        std::cout << std::string(60, '=') << std::endl;

        std::cout << std::left << std::setw(25) << "Total Tests:" << total_tests << std::endl;
        std::cout << std::left << std::setw(25) << "Passed:" << passed_tests << " ✓" << std::endl;
        std::cout << std::left << std::setw(25) << "Failed:" << failed_tests << " ✗" << std::endl;
        std::cout << std::left << std::setw(25) << "Success Rate:" << std::fixed << std::setprecision(1) << success_rate << "%" << std::endl;
        std::cout << std::left << std::setw(25) << "Average Duration:" << std::fixed << std::setprecision(2) << avg_duration << "ms" << std::endl;
        std::cout << std::left << std::setw(25) << "Total Duration:" << total_duration.count() << "ms" << std::endl;

        // Display performance rating
        std::string performance_rating;
        if (success_rate >= 95.0) {
            performance_rating = "🟢 EXCELLENT";
        } else if (success_rate >= 85.0) {
            performance_rating = "🟡 GOOD";
        } else if (success_rate >= 70.0) {
            performance_rating = "🟠 FAIR";
        } else {
            performance_rating = "🔴 NEEDS IMPROVEMENT";
        }

        std::cout << std::left << std::setw(25) << "Performance Rating:" << performance_rating << std::endl;

        // Display failed tests if any
        const auto& failed_tests_array = results["failed_tests"];
        if (!failed_tests_array.empty()) {
            std::cout << "\n❌ FAILED TESTS:" << std::endl;
            std::cout << std::string(60, '-') << std::endl;

            for (const auto& failed_test : failed_tests_array) {
                std::string test_name = failed_test["test_name"];
                std::string error_msg = failed_test["error_message"];
                int duration = failed_test["duration_ms"];

                std::cout << "✗ " << test_name << " (" << duration << "ms)" << std::endl;
                if (!error_msg.empty()) {
                    std::cout << "  Error: " << error_msg << std::endl;
                }
                std::cout << std::endl;
            }
        }

        // Display test categories summary
        display_category_breakdown(results);
    }

    void display_category_breakdown(const nlohmann::json& results) {
        // Group tests by category for better reporting
        std::map<std::string, std::vector<std::string>> category_tests;
        std::map<std::string, int> category_passed;
        std::map<std::string, int> category_total;

        // This is a simplified categorization - in practice, you'd track categories during test execution
        std::vector<std::pair<std::string, std::vector<std::string>>> categories = {
            {"Pattern Recognition", {"Decision Pattern Analysis", "Behavior Pattern Detection", "Anomaly Detection", "Trend Analysis", "Correlation Analysis", "Sequence Pattern Mining", "Pattern Application to Data"}},
            {"Feedback Systems", {"Feedback Collection", "Human Feedback Processing", "System Validation Feedback", "Learning Model Updates", "Feedback-Driven Improvement", "Feedback Analytics"}},
            {"Collaboration", {"Collaboration Session Creation", "Real-time Messaging", "Feedback Submission", "Intervention Handling", "User Permission System", "Collaboration Analytics"}},
            {"Error Handling", {"Circuit Breaker Creation", "Retry Mechanisms", "Fallback Systems", "Health Monitoring", "Error Recovery Workflows", "Graceful Degradation"}},
            {"Activity Feeds", {"Activity Recording", "Activity Filtering", "Activity Analytics", "Performance Monitoring"}},
            {"Decision Trees", {"Decision Tree Generation", "Interactive Features", "Export Formats"}},
            {"Integration", {"End-to-End Decision Process", "Concurrent Agent Operations", "Extreme Input Handling"}}
        };

        std::cout << "\n📈 CATEGORY BREAKDOWN" << std::endl;
        std::cout << std::string(60, '-') << std::endl;

        for (const auto& [category_name, test_names] : categories) {
            int passed = 0;
            int total = test_names.size();

            for (const auto& test_name : test_names) {
                // Check if this test passed
                const auto& failed_tests = results["failed_tests"];
                bool test_failed = false;
                for (const auto& failed : failed_tests) {
                    if (failed["test_name"] == test_name) {
                        test_failed = true;
                        break;
                    }
                }
                if (!test_failed) passed++;
            }

            double category_success = total > 0 ? (static_cast<double>(passed) / total) * 100.0 : 0.0;

            std::cout << std::left << std::setw(25) << category_name << ": "
                      << std::right << std::setw(3) << passed << "/" << total
                      << " (" << std::fixed << std::setprecision(1) << category_success << "%)";

            if (category_success >= 80.0) {
                std::cout << " ✓";
            } else {
                std::cout << " ⚠";
            }
            std::cout << std::endl;
        }
    }

    void generate_report(const nlohmann::json& results, const std::string& filename) {
        try {
            // Add metadata to the report
            nlohmann::json report = results;
            report["metadata"] = {
                {"test_suite", "Advanced Agent Capability Tests"},
                {"version", "1.0.0"},
                {"level", "3 and 4"},
                {"generated_at", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()},
                {"hostname", "test_environment"},
                {"test_categories", {
                    "pattern_recognition", "feedback", "collaboration", "error_handling",
                    "activity_feed", "decision_trees", "regulatory", "mcp_tools",
                    "autonomous", "orchestration", "learning", "integration", "performance", "edge_cases"
                }}
            };

            // Write to file
            std::ofstream file(filename);
            if (file.is_open()) {
                file << report.dump(2);
                file.close();
                std::cout << "\n📄 Report generated: " << filename << std::endl;
            } else {
                std::cerr << "\n❌ Failed to write report file: " << filename << std::endl;
            }

        } catch (const std::exception& e) {
            std::cerr << "\n❌ Failed to generate report: " << e.what() << std::endl;
        }
    }
};

} // namespace regulens

int main(int argc, char* argv[]) {
    regulens::AdvancedAgentTestRunner runner;
    return runner.run(argc, argv);
}
