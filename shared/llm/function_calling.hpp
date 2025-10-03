/**
 * Function Calling Framework - OpenAI Function Calling Integration
 *
 * Enterprise-grade function calling support for dynamic tool selection
 * and execution in compliance scenarios with comprehensive security controls.
 *
 * Features:
 * - JSON schema validation for function parameters
 * - Secure execution with timeouts and resource limits
 * - Audit logging for all function calls
 * - Permission-based access control
 * - Integration with existing tool framework
 * - Compliance-specific function libraries
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <chrono>
#include <nlohmann/json.hpp>

#include "../logging/structured_logger.hpp"
#include "../error_handler.hpp"
#include "../config/configuration_manager.hpp"

namespace regulens {

// Forward declarations
class FunctionRegistry;

/**
 * @brief Function execution result
 */
struct FunctionResult {
    bool success;
    nlohmann::json result;
    std::string error_message;
    std::chrono::milliseconds execution_time;
    std::string correlation_id;

    FunctionResult(bool s = false, const nlohmann::json& r = nullptr,
                   const std::string& err = "", std::chrono::milliseconds time = std::chrono::milliseconds(0),
                   const std::string& corr_id = "")
        : success(s), result(r), error_message(err), execution_time(time), correlation_id(corr_id) {}
};

// ValidationResult is defined in configuration_manager.hpp

/**
 * @brief Function execution context for security and auditing
 */
struct FunctionContext {
    std::string agent_id;
    std::string agent_type;
    std::vector<std::string> permissions;
    std::string correlation_id;
    std::chrono::system_clock::time_point request_time;
    nlohmann::json metadata;

    FunctionContext(const std::string& agent = "", const std::string& type = "",
                   const std::vector<std::string>& perms = {},
                   const std::string& corr = "", const nlohmann::json& meta = nullptr)
        : agent_id(agent), agent_type(type), permissions(perms),
          correlation_id(corr), request_time(std::chrono::system_clock::now()), metadata(meta) {}
};

/**
 * @brief Function definition with schema and execution logic
 */
struct FunctionDefinition {
    std::string name;
    std::string description;
    nlohmann::json parameters_schema;
    std::function<FunctionResult(const nlohmann::json&, const FunctionContext&)> executor;
    std::chrono::milliseconds timeout = std::chrono::seconds(30);
    std::vector<std::string> required_permissions;
    bool requires_audit = true;
    std::string category = "general";

    // Schema validation
    ValidationResult validate_parameters(const nlohmann::json& parameters) const;
};

/**
 * @brief Function call request structure
 */
struct FunctionCall {
    std::string name;
    nlohmann::json arguments;
    std::string call_id;
    std::string tool_call_id;  // For OpenAI tool calls

    FunctionCall(const std::string& n = "", const nlohmann::json& args = nullptr,
                const std::string& id = "", const std::string& tool_id = "")
        : name(n), arguments(args), call_id(id), tool_call_id(tool_id) {}

    // Parse from OpenAI function call format
    static FunctionCall from_openai_function_call(const nlohmann::json& function_call);

    // Parse from OpenAI tool call format
    static FunctionCall from_openai_tool_call(const nlohmann::json& tool_call);

    // Convert to JSON for API requests
    nlohmann::json to_json() const;
};

/**
 * @brief Function call response structure
 */
struct FunctionCallResponse {
    std::string call_id;
    std::string tool_call_id;
    FunctionResult result;

    FunctionCallResponse(const std::string& id = "", const std::string& tool_id = "",
                        const FunctionResult& res = FunctionResult())
        : call_id(id), tool_call_id(tool_id), result(res) {}

    nlohmann::json to_json() const;
};

/**
 * @brief Function registry for managing available functions
 */
class FunctionRegistry {
public:
    FunctionRegistry(std::shared_ptr<ConfigurationManager> config,
                    StructuredLogger* logger,
                    ErrorHandler* error_handler);

    /**
     * @brief Register a function definition
     * @param function Function definition to register
     * @return true if registration successful
     */
    bool register_function(const FunctionDefinition& function);

    /**
     * @brief Unregister a function
     * @param function_name Name of function to unregister
     * @return true if unregistration successful
     */
    bool unregister_function(const std::string& function_name);

    /**
     * @brief Check if function is registered
     * @param function_name Function name to check
     * @return true if function exists
     */
    bool has_function(const std::string& function_name) const;

    /**
     * @brief Get function definition
     * @param function_name Function name
     * @return Function definition or nullptr if not found
     */
    const FunctionDefinition* get_function(const std::string& function_name) const;

    /**
     * @brief Execute a function call
     * @param call Function call request
     * @param context Execution context
     * @return Function result
     */
    FunctionResult execute_function(const FunctionCall& call, const FunctionContext& context);

    /**
     * @brief Get all registered function names
     * @return Vector of function names
     */
    std::vector<std::string> get_registered_functions() const;

    /**
     * @brief Get function definitions for API requests (filtered by permissions)
     * @param permissions Available permissions
     * @return JSON array of function definitions
     */
    nlohmann::json get_function_definitions_for_api(const std::vector<std::string>& permissions) const;

    /**
     * @brief Validate function call permissions
     * @param call Function call
     * @param context Execution context
     * @return true if call is permitted
     */
    bool validate_permissions(const FunctionCall& call, const FunctionContext& context) const;

private:
    std::shared_ptr<ConfigurationManager> config_;
    StructuredLogger* logger_;
    ErrorHandler* error_handler_;  // For future error reporting enhancements

    std::unordered_map<std::string, FunctionDefinition> functions_;
    mutable std::mutex functions_mutex_;

    /**
     * @brief Log function execution for audit purposes
     * @param call Function call
     * @param context Execution context
     * @param result Execution result
     */
    void log_function_execution(const FunctionCall& call, const FunctionContext& context,
                               const FunctionResult& result);

    /**
     * @brief Check if execution timeout has been exceeded
     * @param start_time Execution start time
     * @param timeout Maximum allowed time
     * @return true if timeout exceeded
     */
    bool check_timeout(const std::chrono::steady_clock::time_point& start_time,
                      std::chrono::milliseconds timeout) const;
};

/**
 * @brief Function execution dispatcher with security controls
 */
class FunctionDispatcher {
public:
    FunctionDispatcher(std::shared_ptr<FunctionRegistry> registry,
                      StructuredLogger* logger,
                      ErrorHandler* error_handler);

    /**
     * @brief Execute multiple function calls
     * @param calls Vector of function calls
     * @param context Execution context
     * @return Vector of function responses
     */
    std::vector<FunctionCallResponse> execute_function_calls(
        const std::vector<FunctionCall>& calls,
        const FunctionContext& context);

    /**
     * @brief Execute single function call with comprehensive error handling
     * @param call Function call
     * @param context Execution context
     * @return Function response
     */
    FunctionCallResponse execute_single_function_call(
        const FunctionCall& call,
        const FunctionContext& context);

private:
    std::shared_ptr<FunctionRegistry> registry_;
    StructuredLogger* logger_;
    ErrorHandler* error_handler_;  // For future error reporting enhancements

    /**
     * @brief Validate function call before execution
     * @param call Function call
     * @param context Execution context
     * @return Validation result
     */
    ValidationResult pre_execution_validation(const FunctionCall& call, const FunctionContext& context);

    /**
     * @brief Apply security controls and resource limits
     * @param call Function call
     * @param context Execution context
     * @return true if execution should proceed
     */
    bool apply_security_controls(const FunctionCall& call, const FunctionContext& context);
};

} // namespace regulens
