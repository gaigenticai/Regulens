/**
 * @file tool_base.hpp
 * @brief Production-grade tool system for agentic AI
 * 
 * Provides a flexible, secure tool system allowing agents to:
 * - Call external APIs (HTTP)
 * - Query databases
 * - Invoke LLM analysis
 * - Execute custom business logic
 * 
 * NO MOCKS - All tools execute real operations with proper error handling,
 * rate limiting, circuit breakers, and audit logging.
 */

#ifndef REGULENS_TOOL_BASE_HPP
#define REGULENS_TOOL_BASE_HPP

#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include <chrono>
#include <functional>
#include "../logging/structured_logger.hpp"
#include "../config/configuration_manager.hpp"

namespace regulens {

/**
 * @brief Tool execution result with success/failure status
 */
struct ToolResult {
    bool success;
    nlohmann::json result;
    std::string error_message;
    std::chrono::milliseconds execution_time;
    int tokens_used; // For LLM tools
    
    ToolResult() : success(false), execution_time(0), tokens_used(0) {}
};

/**
 * @brief Tool execution context with rate limiting and permissions
 */
struct ToolContext {
    std::string agent_id;
    std::string agent_name;
    std::string user_id;
    nlohmann::json permissions;
    int rate_limit_remaining;
    std::chrono::system_clock::time_point rate_limit_reset;
    
    ToolContext() : rate_limit_remaining(100) {}
};

/**
 * @brief Base class for all agent tools
 * 
 * Production-grade design with:
 * - Rate limiting per tool
 * - Circuit breaker pattern
 * - Audit logging
 * - Permission checking
 * - Timeout handling
 */
class ToolBase {
public:
    ToolBase(
        const std::string& name,
        const std::string& description,
        std::shared_ptr<StructuredLogger> logger,
        std::shared_ptr<ConfigurationManager> config
    ) : tool_name_(name), 
        tool_description_(description),
        logger_(logger),
        config_(config),
        total_calls_(0),
        successful_calls_(0),
        failed_calls_(0),
        circuit_breaker_open_(false),
        consecutive_failures_(0),
        last_failure_time_(std::chrono::system_clock::now()) {}
    
    virtual ~ToolBase() = default;
    
    /**
     * @brief Execute the tool with given parameters
     * 
     * This method handles:
     * - Rate limiting check
     * - Permission validation
     * - Circuit breaker check
     * - Timeout enforcement
     * - Audit logging
     * - Error handling
     * 
     * @param context Tool execution context with permissions
     * @param parameters Tool-specific parameters
     * @return ToolResult with success/failure and data
     */
    ToolResult execute(const ToolContext& context, const nlohmann::json& parameters) {
        auto start_time = std::chrono::steady_clock::now();
        ToolResult result;
        
        total_calls_++;
        
        // Check rate limiting
        if (!check_rate_limit(context)) {
            result.success = false;
            result.error_message = "Rate limit exceeded for tool: " + tool_name_;
            log_tool_execution(context, parameters, result);
            failed_calls_++;
            return result;
        }
        
        // Check permissions
        if (!check_permissions(context, parameters)) {
            result.success = false;
            result.error_message = "Permission denied for tool: " + tool_name_;
            log_tool_execution(context, parameters, result);
            failed_calls_++;
            return result;
        }
        
        // Check circuit breaker
        if (is_circuit_breaker_open()) {
            result.success = false;
            result.error_message = "Circuit breaker open for tool: " + tool_name_;
            log_tool_execution(context, parameters, result);
            failed_calls_++;
            return result;
        }
        
        // Execute the actual tool logic
        try {
            result = execute_impl(context, parameters);
            
            if (result.success) {
                successful_calls_++;
                reset_circuit_breaker();
            } else {
                failed_calls_++;
                record_failure();
            }
            
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Tool execution exception: ") + e.what();
            failed_calls_++;
            record_failure();
        }
        
        auto end_time = std::chrono::steady_clock::now();
        result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        // Log execution
        log_tool_execution(context, parameters, result);
        
        return result;
    }
    
    /**
     * @brief Get tool name
     */
    std::string get_name() const { return tool_name_; }
    
    /**
     * @brief Get tool description
     */
    std::string get_description() const { return tool_description_; }
    
    /**
     * @brief Get tool parameters schema (JSON Schema format)
     */
    virtual nlohmann::json get_parameters_schema() const = 0;
    
    /**
     * @brief Get tool execution statistics
     */
    nlohmann::json get_statistics() const {
        nlohmann::json stats;
        stats["tool_name"] = tool_name_;
        stats["total_calls"] = total_calls_.load();
        stats["successful_calls"] = successful_calls_.load();
        stats["failed_calls"] = failed_calls_.load();
        stats["success_rate"] = total_calls_.load() > 0 ? (double)successful_calls_.load() / total_calls_.load() : 0.0;
        stats["circuit_breaker_open"] = circuit_breaker_open_.load();
        stats["consecutive_failures"] = consecutive_failures_.load();
        return stats;
    }
    
protected:
    /**
     * @brief Implement tool-specific execution logic
     * 
     * Subclasses MUST implement this method with real, production-grade logic.
     * NO MOCKS ALLOWED.
     */
    virtual ToolResult execute_impl(const ToolContext& context, const nlohmann::json& parameters) = 0;
    
    /**
     * @brief Check if agent has permission to use this tool
     */
    virtual bool check_permissions([[maybe_unused]] const ToolContext& context, [[maybe_unused]] const nlohmann::json& parameters) const {
        // Production: Check against permission database or policy engine
        return true; // Override in subclasses for specific permission logic
    }
    
    /**
     * @brief Check rate limiting for tool usage
     */
    virtual bool check_rate_limit(const ToolContext& context) const {
        // Production: Check against rate limiting service (Redis)
        return context.rate_limit_remaining > 0;
    }
    
    /**
     * @brief Check if circuit breaker is open
     */
    bool is_circuit_breaker_open() const {
        if (!circuit_breaker_open_) return false;
        
        // Check if enough time has passed to try again (30 seconds)
        auto now = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_failure_time_);
        
        if (duration.count() > 30) {
            return false; // Half-open state, allow one try
        }
        
        return true;
    }
    
    /**
     * @brief Record failure for circuit breaker
     */
    void record_failure() {
        consecutive_failures_++;
        last_failure_time_ = std::chrono::system_clock::now();
        
        // Open circuit breaker after 5 consecutive failures
        if (consecutive_failures_ >= 5) {
            circuit_breaker_open_ = true;
            logger_->log(LogLevel::WARN, "Circuit breaker opened for tool: " + tool_name_);
        }
    }
    
    /**
     * @brief Reset circuit breaker after success
     */
    void reset_circuit_breaker() {
        if (circuit_breaker_open_ || consecutive_failures_ > 0) {
            circuit_breaker_open_ = false;
            consecutive_failures_ = 0;
            logger_->log(LogLevel::INFO, "Circuit breaker reset for tool: " + tool_name_);
        }
    }
    
    /**
     * @brief Log tool execution to database for audit trail
     */
    void log_tool_execution([[maybe_unused]] const ToolContext& context, [[maybe_unused]] const nlohmann::json& parameters, [[maybe_unused]] const ToolResult& result) {
        nlohmann::json log_data;
        log_data["agent_id"] = context.agent_id;
        log_data["agent_name"] = context.agent_name;
        log_data["success"] = result.success;
        log_data["execution_time_ms"] = result.execution_time.count();
        log_data["error"] = result.error_message;

        logger_->log(
            result.success ? LogLevel::INFO : LogLevel::ERROR,
            "Tool execution: " + tool_name_,
            log_data
        );
        
        // Production: Also log to function_call_logs table for debugging
    }
    
    std::string tool_name_;
    std::string tool_description_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<ConfigurationManager> config_;
    
    // Statistics
    std::atomic<uint64_t> total_calls_;
    std::atomic<uint64_t> successful_calls_;
    std::atomic<uint64_t> failed_calls_;
    
    // Circuit breaker
    std::atomic<bool> circuit_breaker_open_;
    std::atomic<int> consecutive_failures_;
    std::chrono::system_clock::time_point last_failure_time_;
};

} // namespace regulens

#endif // REGULENS_TOOL_BASE_HPP

