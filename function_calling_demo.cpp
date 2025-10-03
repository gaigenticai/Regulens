/**
 * Function Calling Demo - OpenAI Function Calling Integration
 *
 * Demonstrates production-grade function calling capabilities for
 * dynamic tool selection in compliance scenarios.
 */

#include <iostream>
#include <memory>

#include "shared/llm/function_calling.hpp"
#include "shared/config/configuration_manager.hpp"
#include "shared/logging/structured_logger.hpp"
#include "shared/error_handler.hpp"

using namespace regulens;

/**
 * @brief Simple function implementation for demo
 */
FunctionResult simple_search_function(const nlohmann::json& args, const FunctionContext& context) {
    std::string query = args.value("query", "");
    return FunctionResult(true, {
        {"query", query},
        {"results", {"Result 1 for: " + query, "Result 2 for: " + query}},
        {"agent_id", context.agent_id}
    });
}

/**
 * @brief Demo basic function calling functionality
 */
void demonstrate_basic_function_calling() {
    std::cout << "🔧 Function Calling Demo - Basic Framework\n";
    std::cout << "===========================================\n\n";

    // Initialize core components
    auto config = std::make_shared<ConfigurationManager>();
    auto logger = std::make_shared<StructuredLogger>();
    auto error_handler = std::make_shared<ErrorHandler>(config, logger.get());

    // Create function registry
    auto function_registry = std::make_shared<FunctionRegistry>(config, logger.get(), error_handler.get());

    // Register a simple function
    FunctionDefinition search_function = {
        "search_regulations",
        "Search regulatory knowledge base",
        {
            {"type", "object"},
            {"properties", {
                {"query", {{"type", "string"}, {"description", "Search query"}}}
            }},
            {"required", {"query"}}
        },
        simple_search_function,
        std::chrono::seconds(10),
        {"read_regulations"},
        true,
        "regulatory_search"
    };

    if (function_registry->register_function(search_function)) {
        std::cout << "✅ Function registered successfully\n";
    } else {
        std::cout << "❌ Failed to register function\n";
        return;
    }

    // Create function dispatcher
    FunctionDispatcher dispatcher(function_registry, logger.get(), error_handler.get());

    // Execute function call
    FunctionCall call("search_regulations",
        {{"query", "money laundering prevention"}},
        "demo_call_001");

    FunctionContext context("demo_agent", "compliance_analyzer",
        {"read_regulations"}, "corr_123");

    auto result = dispatcher.execute_single_function_call(call, context);

    if (result.result.success) {
        std::cout << "✅ Function executed successfully\n";
        std::cout << "Result: " << result.result.result.dump(2) << "\n";
    } else {
        std::cout << "❌ Function execution failed: " << result.result.error_message << "\n";
    }

    // Test permission validation
    FunctionContext restricted_context("demo_agent", "restricted_user",
        {"read_basic"}, "corr_124");

    auto restricted_result = dispatcher.execute_single_function_call(call, restricted_context);

    if (!restricted_result.result.success) {
        std::cout << "✅ Permission control working - access denied\n";
    } else {
        std::cout << "❌ Permission control failed\n";
    }

    // Show registered functions
    auto functions = function_registry->get_registered_functions();
    std::cout << "\n📋 Registered functions: " << functions.size() << "\n";
    for (const auto& func : functions) {
        std::cout << "  • " << func << "\n";
    }

    std::cout << "\n🎯 Basic Function Calling Framework Working!\n";
    std::cout << "===========================================\n";
}

/**
 * @brief Main function
 */
int main(int argc, char* argv[]) {
    try {
        demonstrate_basic_function_calling();
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "❌ Demo failed with exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
