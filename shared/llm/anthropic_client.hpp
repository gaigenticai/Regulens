#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <chrono>
#include <nlohmann/json.hpp>

#include "../network/http_client.hpp"
#include "../logging/structured_logger.hpp"
#include "../config/configuration_manager.hpp"
#include "../error_handler.hpp"

namespace regulens {

/**
 * @brief Anthropic Claude message structures
 */
struct ClaudeMessage {
    std::string role;    // "user", "assistant"
    std::string content;
    std::optional<std::vector<std::unordered_map<std::string, std::string>>> content_blocks; // For complex content

    nlohmann::json to_json() const {
        nlohmann::json msg = {
            {"role", role},
            {"content", content}
        };

        if (content_blocks) {
            nlohmann::json blocks = nlohmann::json::array();
            for (const auto& block : *content_blocks) {
                blocks.push_back(block);
            }
            msg["content"] = blocks;
        }

        return msg;
    }
};

struct ClaudeUsage {
    int input_tokens = 0;
    int output_tokens = 0;

    nlohmann::json to_json() const {
        return {
            {"input_tokens", input_tokens},
            {"output_tokens", output_tokens}
        };
    }
};

struct ClaudeResponse {
    std::string id;
    std::string type;  // "message"
    std::string role;  // "assistant"
    std::string model;
    std::vector<ClaudeMessage> content;
    std::string stop_reason;  // "end_turn", "max_tokens", "stop_sequence"
    std::optional<std::string> stop_sequence;
    ClaudeUsage usage;
    std::chrono::system_clock::time_point created_at;

    nlohmann::json to_json() const {
        nlohmann::json response = {
            {"id", id},
            {"type", type},
            {"role", role},
            {"model", model},
            {"content", nlohmann::json::array()},
            {"stop_reason", stop_reason},
            {"usage", usage.to_json()},
            {"created_at", std::chrono::duration_cast<std::chrono::milliseconds>(
                created_at.time_since_epoch()).count()}
        };

        for (const auto& msg : content) {
            response["content"].push_back(msg.to_json());
        }

        if (stop_sequence) {
            response["stop_sequence"] = *stop_sequence;
        }

        return response;
    }
};

/**
 * @brief Claude completion request parameters
 */
struct ClaudeCompletionRequest {
    std::string model = "claude-3-sonnet-20240229";
    int max_tokens = 4096;
    std::vector<ClaudeMessage> messages;
    std::optional<std::string> system;  // System prompt
    std::optional<std::vector<std::string>> stop_sequences;
    std::optional<bool> stream;  // Default false
    std::optional<double> temperature;  // 0.0 to 1.0
    std::optional<double> top_p;  // 0.0 to 1.0
    std::optional<double> top_k;  // 1 to 1000
    std::optional<std::unordered_map<std::string, double>> metadata;

    nlohmann::json to_json() const {
        nlohmann::json request = {
            {"model", model},
            {"max_tokens", max_tokens},
            {"messages", nlohmann::json::array()}
        };

        for (const auto& msg : messages) {
            request["messages"].push_back(msg.to_json());
        }

        if (system) request["system"] = *system;
        if (stop_sequences) request["stop_sequences"] = *stop_sequences;
        if (stream) request["stream"] = *stream;
        if (temperature) request["temperature"] = *temperature;
        if (top_p) request["top_p"] = *top_p;
        if (top_k) request["top_k"] = *top_k;
        if (metadata) {
            nlohmann::json meta;
            for (const auto& [key, value] : *metadata) {
                meta[key] = value;
            }
            request["metadata"] = meta;
        }

        return request;
    }
};

/**
 * @brief Anthropic Claude API client for advanced reasoning and analysis
 *
 * Provides production-grade integration with Anthropic's Claude API including:
 * - Advanced reasoning and analysis capabilities
 * - Constitutional AI safety features
 * - High-quality text generation and analysis
 * - Error handling and rate limiting
 * - Usage tracking and cost monitoring
 * - Fallback mechanisms
 */
class AnthropicClient {
public:
    AnthropicClient(std::shared_ptr<ConfigurationManager> config,
                   std::shared_ptr<StructuredLogger> logger,
                   std::shared_ptr<ErrorHandler> error_handler);

    ~AnthropicClient();

    /**
     * @brief Initialize the Anthropic client
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Shutdown the client and cleanup resources
     */
    void shutdown();

    /**
     * @brief Create a message completion with Claude
     * @param request Completion request parameters
     * @return Response from Claude API or error result
     */
    std::optional<ClaudeResponse> create_message(const ClaudeCompletionRequest& request);

    /**
     * @brief Perform advanced reasoning analysis
     * @param prompt Analysis prompt
     * @param context Additional context for analysis
     * @param analysis_type Type of analysis (reasoning, compliance, risk, etc.)
     * @return Analysis result or error
     */
    std::optional<std::string> advanced_reasoning_analysis(
        const std::string& prompt,
        const std::string& context = "",
        const std::string& analysis_type = "general");

    /**
     * @brief Constitutional AI analysis for compliance and ethics
     * @param content Content to analyze
     * @param compliance_requirements Specific compliance requirements
     * @return Constitutional analysis result or error
     */
    std::optional<std::string> constitutional_ai_analysis(
        const std::string& content,
        const std::vector<std::string>& compliance_requirements = {});

    /**
     * @brief Generate comprehensive decision analysis
     * @param scenario Decision scenario
     * @param options Available options
     * @param constraints Business and regulatory constraints
     * @param ethical_considerations Ethical factors to consider
     * @return Decision analysis result or error
     */
    std::optional<std::string> ethical_decision_analysis(
        const std::string& scenario,
        const std::vector<std::string>& options,
        const std::vector<std::string>& constraints,
        const std::vector<std::string>& ethical_considerations = {});

    /**
     * @brief Perform complex reasoning tasks
     * @param task_description Description of the reasoning task
     * @param data Input data for reasoning
     * @param reasoning_steps Number of reasoning steps to perform
     * @return Reasoning result or error
     */
    std::optional<std::string> complex_reasoning_task(
        const std::string& task_description,
        const nlohmann::json& data,
        int reasoning_steps = 5);

    /**
     * @brief Analyze regulatory compliance with advanced reasoning
     * @param regulation_text Regulatory text to analyze
     * @param business_context Business context for analysis
     * @param risk_factors Risk factors to consider
     * @return Compliance analysis result or error
     */
    std::optional<std::string> regulatory_compliance_reasoning(
        const std::string& regulation_text,
        const std::string& business_context,
        const std::vector<std::string>& risk_factors = {});

    /**
     * @brief Get usage statistics
     * @return JSON with usage metrics
     */
    nlohmann::json get_usage_statistics();

    /**
     * @brief Get client health status
     * @return Health status information
     */
    nlohmann::json get_health_status();

    /**
     * @brief Reset usage counters (for testing/admin)
     */
    void reset_usage_counters();

    // Configuration access
    const std::string& get_model() const { return default_model_; }
    int get_max_tokens() const { return max_tokens_; }
    double get_temperature() const { return temperature_; }

private:
    std::shared_ptr<ConfigurationManager> config_manager_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<ErrorHandler> error_handler_;
    std::shared_ptr<HttpClient> http_client_;

    // Configuration
    std::string api_key_;
    std::string base_url_;
    std::string default_model_;
    int max_tokens_;
    double temperature_;
    int request_timeout_seconds_;
    int max_retries_;
    std::chrono::milliseconds rate_limit_window_;

    // Usage tracking
    std::atomic<size_t> total_requests_;
    std::atomic<size_t> successful_requests_;
    std::atomic<size_t> failed_requests_;
    std::atomic<size_t> total_input_tokens_;
    std::atomic<size_t> total_output_tokens_;
    std::atomic<double> estimated_cost_usd_;
    std::chrono::system_clock::time_point last_request_time_;

    // Rate limiting
    std::mutex rate_limit_mutex_;
    std::deque<std::chrono::system_clock::time_point> request_timestamps_;
    int max_requests_per_minute_;

    // Circuit breaker service name
    static constexpr const char* CIRCUIT_BREAKER_SERVICE = "anthropic_api";

    /**
     * @brief Make HTTP request to Anthropic API
     * @param payload JSON payload
     * @return HTTP response or error
     */
    std::optional<HttpResponse> make_api_request(const nlohmann::json& payload);

    /**
     * @brief Parse Claude API response
     * @param response HTTP response
     * @return Parsed Claude response or error
     */
    std::optional<ClaudeResponse> parse_api_response(const HttpResponse& response);

    /**
     * @brief Handle API errors with appropriate logging and error reporting
     * @param error_type Type of error
     * @param message Error message
     * @param context Additional context
     */
    void handle_api_error(const std::string& error_type,
                         const std::string& message,
                         const std::unordered_map<std::string, std::string>& context = {});

    /**
     * @brief Check and enforce rate limits
     * @return true if request allowed, false if rate limited
     */
    bool check_rate_limit();

    /**
     * @brief Update usage statistics
     * @param response API response
     */
    void update_usage_stats(const ClaudeResponse& response);

    /**
     * @brief Calculate estimated cost for API usage
     * @param model Model used
     * @param input_tokens Input tokens used
     * @param output_tokens Output tokens used
     * @return Estimated cost in USD
     */
    double calculate_cost(const std::string& model, int input_tokens, int output_tokens);

    /**
     * @brief Create constitutional AI system prompt
     * @param task_type Type of analysis task
     * @return System prompt for Claude
     */
    std::string create_constitutional_system_prompt(const std::string& task_type);

    /**
     * @brief Create advanced reasoning system prompt
     * @param reasoning_type Type of reasoning task
     * @return System prompt for Claude
     */
    std::string create_reasoning_system_prompt(const std::string& reasoning_type);

    /**
     * @brief Validate API response structure
     * @param response Parsed response
     * @return true if valid
     */
    bool validate_response(const ClaudeResponse& response);

    /**
     * @brief Execute API call with retry logic
     * @param operation Function that makes the API call
     * @param operation_name Name for logging
     * @return API response
     */
    std::optional<ClaudeResponse> execute_with_retry(std::function<std::optional<ClaudeResponse>()> operation,
                                                    const std::string& operation_name);

    // Retry configuration
    int max_retries_;
    std::chrono::milliseconds base_retry_delay_;

    /**
     * @brief Load Anthropic client configuration from environment
     */
    void load_configuration();

    /**
     * @brief Generate unique request ID
     */
    std::string generate_request_id() const;
};

// Convenience functions for common Claude operations

/**
 * @brief Create a simple Claude message request
 */
inline ClaudeCompletionRequest create_simple_claude_message(
    const std::string& user_prompt,
    const std::string& model = "claude-3-sonnet-20240229",
    int max_tokens = 4096) {

    return ClaudeCompletionRequest{
        .model = model,
        .max_tokens = max_tokens,
        .messages = {ClaudeMessage{"user", user_prompt}},
        .temperature = 0.7
    };
}

/**
 * @brief Create a Claude analysis request with system prompt
 */
inline ClaudeCompletionRequest create_claude_analysis_request(
    const std::string& system_prompt,
    const std::string& user_content,
    const std::string& model = "claude-3-sonnet-20240229") {

    return ClaudeCompletionRequest{
        .model = model,
        .max_tokens = 4096,
        .messages = {ClaudeMessage{"user", user_content}},
        .system = system_prompt,
        .temperature = 0.1  // Lower temperature for consistent analysis
    };
}

/**
 * @brief Create a Claude reasoning request
 */
inline ClaudeCompletionRequest create_claude_reasoning_request(
    const std::string& task_description,
    const std::string& data,
    const std::string& model = "claude-3-sonnet-20240229") {

    std::string system_prompt = R"(
You are Claude, an AI assistant created by Anthropic. You are designed to be helpful, honest, and harmless.

For reasoning tasks, you should:
1. Break down complex problems into smaller, manageable parts
2. Consider multiple perspectives and potential outcomes
3. Provide clear, logical reasoning for your conclusions
4. Acknowledge uncertainties and limitations
5. Provide actionable insights when possible

Structure your response with clear reasoning steps and conclusions.)";

    std::string user_prompt = task_description + "\n\nData/Context:\n" + data;

    return ClaudeCompletionRequest{
        .model = model,
        .max_tokens = 4096,
        .messages = {ClaudeMessage{"user", user_prompt}},
        .system = system_prompt,
        .temperature = 0.3  // Moderate temperature for balanced reasoning
    };
}

/**
 * @brief Create a constitutional AI compliance request
 */
inline ClaudeCompletionRequest create_constitutional_compliance_request(
    const std::string& content_to_analyze,
    const std::vector<std::string>& requirements,
    const std::string& model = "claude-3-sonnet-20240229") {

    std::string system_prompt = R"(
You are Claude, an AI assistant created by Anthropic. You are designed to be helpful, honest, and harmless.

As a constitutional AI, you must ensure all analysis and recommendations comply with:
- Legal and regulatory requirements
- Ethical standards and principles
- Safety and security considerations
- Transparency and accountability
- Fairness and non-discrimination

Provide analysis that explicitly considers compliance implications and recommends safe, ethical actions.)";

    std::string user_prompt = "Please analyze the following content for compliance and ethical considerations:\n\n" + content_to_analyze;

    if (!requirements.empty()) {
        user_prompt += "\n\nSpecific Requirements to Consider:\n";
        for (size_t i = 0; i < requirements.size(); ++i) {
            user_prompt += std::to_string(i + 1) + ". " + requirements[i] + "\n";
        }
    }

    return ClaudeCompletionRequest{
        .model = model,
        .max_tokens = 4096,
        .messages = {ClaudeMessage{"user", user_prompt}},
        .system = system_prompt,
        .temperature = 0.1  // Low temperature for consistent compliance analysis
    };
}

} // namespace regulens
