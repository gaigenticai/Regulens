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
#include "streaming_handler.hpp"
#include "function_calling.hpp"

namespace regulens {

/**
 * @brief OpenAI API response structures
 */
struct OpenAIMessage {
    std::string role;    // "system", "user", "assistant", "function", "tool"
    std::string content;
    std::optional<std::string> name;  // Optional name for the message author
    std::optional<nlohmann::json> function_call;  // For function calling
    std::optional<nlohmann::json> tool_calls;     // For tool calling
    std::optional<std::string> tool_call_id;      // For tool responses

    // Default constructor
    OpenAIMessage() = default;

    // Constructor for basic messages
    OpenAIMessage(std::string r, std::string c,
                  std::optional<std::string> n = std::nullopt,
                  std::optional<nlohmann::json> fc = std::nullopt,
                  std::optional<nlohmann::json> tc = std::nullopt,
                  std::optional<std::string> tci = std::nullopt)
        : role(std::move(r)), content(std::move(c)), name(std::move(n)),
          function_call(std::move(fc)), tool_calls(std::move(tc)), tool_call_id(std::move(tci)) {}

    nlohmann::json to_json() const {
        nlohmann::json msg = {
            {"role", role}
        };

        if (!content.empty()) {
            msg["content"] = content;
        }

        if (name) {
            msg["name"] = *name;
        }

        if (function_call) {
            msg["function_call"] = *function_call;
        }

        if (tool_calls) {
            msg["tool_calls"] = *tool_calls;
        }

        if (tool_call_id) {
            msg["tool_call_id"] = *tool_call_id;
        }

        return msg;
    }
};

struct OpenAIUsage {
    int prompt_tokens = 0;
    int completion_tokens = 0;
    int total_tokens = 0;

    nlohmann::json to_json() const {
        return {
            {"prompt_tokens", prompt_tokens},
            {"completion_tokens", completion_tokens},
            {"total_tokens", total_tokens}
        };
    }
};

struct OpenAIChoice {
    int index = 0;
    OpenAIMessage message;
    std::string finish_reason;  // "stop", "length", "content_filter"
    std::optional<nlohmann::json> logprobs;  // Optional log probabilities

    nlohmann::json to_json() const {
        nlohmann::json choice = {
            {"index", index},
            {"message", message.to_json()},
            {"finish_reason", finish_reason}
        };
        if (logprobs) {
            choice["logprobs"] = *logprobs;
        }
        return choice;
    }
};

struct OpenAIResponse {
    std::string id;
    std::string object;  // "chat.completion"
    std::chrono::system_clock::time_point created;
    std::string model;
    std::vector<OpenAIChoice> choices;
    OpenAIUsage usage;
    std::optional<std::string> system_fingerprint;  // Optional system identifier

    nlohmann::json to_json() const {
        nlohmann::json response = {
            {"id", id},
            {"object", object},
            {"created", std::chrono::duration_cast<std::chrono::seconds>(
                created.time_since_epoch()).count()},
            {"model", model},
            {"choices", nlohmann::json::array()},
            {"usage", usage.to_json()}
        };

        for (const auto& choice : choices) {
            response["choices"].push_back(choice.to_json());
        }

        if (system_fingerprint) {
            response["system_fingerprint"] = *system_fingerprint;
        }

        return response;
    }
};

/**
 * @brief OpenAI API completion request parameters
 */
struct OpenAICompletionRequest {
    std::string model = "gpt-4-turbo-preview";
    std::vector<OpenAIMessage> messages;
    std::optional<double> temperature;       // 0.0 to 2.0, default 1.0
    std::optional<double> top_p;            // 0.0 to 1.0, nucleus sampling
    std::optional<int> max_tokens;          // Maximum tokens to generate
    std::optional<double> presence_penalty; // -2.0 to 2.0, default 0.0
    std::optional<double> frequency_penalty; // -2.0 to 2.0, default 0.0
    std::optional<std::unordered_map<std::string, int>> logit_bias; // Token logit bias
    std::optional<std::string> user;        // Unique identifier for user
    std::optional<int> n;                   // Number of completions, default 1
    std::optional<bool> stream;             // Stream response, default false
    std::optional<std::string> stop;        // Stop sequence(s)
    std::optional<std::vector<std::string>> stop_sequences; // Multiple stop sequences

    // Function calling support
    std::optional<nlohmann::json> functions;  // Array of function definitions (legacy)
    std::optional<nlohmann::json> tools;      // Array of tool definitions (new format)
    std::optional<std::string> tool_choice;   // "none", "auto", or specific function name

    nlohmann::json to_json() const {
        nlohmann::json request = {
            {"model", model},
            {"messages", nlohmann::json::array()}
        };

        for (const auto& msg : messages) {
            request["messages"].push_back(msg.to_json());
        }

        if (temperature) request["temperature"] = *temperature;
        if (top_p) request["top_p"] = *top_p;
        if (max_tokens) request["max_tokens"] = *max_tokens;
        if (presence_penalty) request["presence_penalty"] = *presence_penalty;
        if (frequency_penalty) request["frequency_penalty"] = *frequency_penalty;
        if (logit_bias) {
            nlohmann::json bias;
            for (const auto& [token, bias_val] : *logit_bias) {
                bias[token] = bias_val;
            }
            request["logit_bias"] = bias;
        }
        if (user) request["user"] = *user;
        if (n) request["n"] = *n;
        if (stream) request["stream"] = *stream;
        if (stop) request["stop"] = *stop;
        if (stop_sequences) {
            request["stop"] = *stop_sequences;
        }

        // Function calling parameters
        if (functions) request["functions"] = *functions;
        if (tools) request["tools"] = *tools;
        if (tool_choice) request["tool_choice"] = *tool_choice;

        return request;
    }
};

/**
 * @brief OpenAI API client for LLM interactions
 *
 * Provides production-grade integration with OpenAI's API including:
 * - Chat completions
 * - Text analysis and reasoning
 * - Error handling and rate limiting
 * - Usage tracking and cost monitoring
 * - Fallback mechanisms
 */
class OpenAIClient {
public:
    OpenAIClient(std::shared_ptr<ConfigurationManager> config,
                std::shared_ptr<StructuredLogger> logger,
                std::shared_ptr<ErrorHandler> error_handler);

    ~OpenAIClient();

    /**
     * @brief Initialize the OpenAI client
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Shutdown the client and cleanup resources
     */
    void shutdown();

    /**
     * @brief Create a chat completion
     * @param request Completion request parameters
     * @return Response from OpenAI API or error result
     */
    std::optional<OpenAIResponse> create_chat_completion(const OpenAICompletionRequest& request);

    /**
     * @brief Create streaming chat completion with real-time token processing
     * @param request Completion request with stream=true
     * @param streaming_callback Callback for real-time token processing
     * @param completion_callback Callback for final completion
     * @return Streaming session or nullopt on error
     */
    std::optional<std::shared_ptr<StreamingSession>> create_streaming_completion(
        const OpenAICompletionRequest& request,
        StreamingCallback streaming_callback,
        CompletionCallback completion_callback);

    /**
     * @brief Analyze text with advanced reasoning
     * @param text Text to analyze
     * @param analysis_type Type of analysis (compliance, risk, sentiment, etc.)
     * @param context Additional context for analysis
     * @return Analysis result or error
     */
    std::optional<std::string> analyze_text(const std::string& text,
                                          const std::string& analysis_type,
                                          const std::string& context = "");

    /**
     * @brief Generate compliance reasoning for a decision
     * @param decision_context Context of the decision
     * @param regulatory_requirements Applicable regulations
     * @param risk_factors Identified risk factors
     * @return Reasoning analysis or error
     */
    std::optional<std::string> generate_compliance_reasoning(
        const std::string& decision_context,
        const std::vector<std::string>& regulatory_requirements,
        const std::vector<std::string>& risk_factors);

    /**
     * @brief Extract structured data from unstructured text
     * @param text Source text
     * @param schema Expected output schema
     * @return Structured JSON data or error
     */
    std::optional<nlohmann::json> extract_structured_data(
        const std::string& text,
        const nlohmann::json& schema);

    /**
     * @brief Generate decision recommendations
     * @param scenario Decision scenario description
     * @param options Available decision options
     * @param constraints Business and regulatory constraints
     * @return Recommendation analysis or error
     */
    std::optional<std::string> generate_decision_recommendation(
        const std::string& scenario,
        const std::vector<std::string>& options,
        const std::vector<std::string>& constraints);

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
     * @brief Check if client is healthy
     * @return true if healthy, false otherwise
     */
    bool is_healthy() const;

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
    std::shared_ptr<StreamingResponseHandler> streaming_handler_;

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
    std::atomic<size_t> total_tokens_used_;
    std::atomic<double> estimated_cost_usd_;
    std::chrono::system_clock::time_point last_request_time_;

    // Rate limiting
    std::mutex rate_limit_mutex_;
    std::deque<std::chrono::system_clock::time_point> request_timestamps_;
    int max_requests_per_minute_;

    // Circuit breaker service name
    static constexpr const char* CIRCUIT_BREAKER_SERVICE = "openai_api";

    /**
     * @brief Make HTTP request to OpenAI API
     * @param endpoint API endpoint
     * @param payload JSON payload
     * @return HTTP response or error
     */
    std::optional<HttpResponse> make_api_request(const std::string& endpoint,
                                               const nlohmann::json& payload);

    /**
     * @brief Parse OpenAI API response
     * @param response HTTP response
     * @return Parsed OpenAI response or error
     */
    std::optional<OpenAIResponse> parse_api_response(const HttpResponse& response);

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
    void update_usage_stats(const OpenAIResponse& response);

    /**
     * @brief Calculate estimated cost for API usage
     * @param model Model used
     * @param tokens Tokens used
     * @return Estimated cost in USD
     */
    double calculate_cost(const std::string& model, int tokens);

    /**
     * @brief Create standardized system prompt for compliance analysis
     * @param task_type Type of analysis task
     * @return System prompt
     */
    std::string create_system_prompt(const std::string& task_type);

    /**
     * @brief Validate API response structure
     * @param response Parsed response
     * @return true if valid
     */
    bool validate_response(const OpenAIResponse& response);

    /**
     * @brief Create completion request with function calling support
     * @param messages Conversation messages
     * @param functions Function definitions for calling
     * @param model Model to use (optional)
     * @return Completion request with function support
     */
    OpenAICompletionRequest create_function_completion_request(
        const std::vector<OpenAIMessage>& messages,
        const nlohmann::json& functions,
        const std::string& model = "gpt-4-turbo-preview");

    /**
     * @brief Create completion request with tool calling support (new format)
     * @param messages Conversation messages
     * @param tools Tool definitions for calling
     * @param tool_choice Tool choice strategy ("auto", "none", or function name)
     * @param model Model to use (optional)
     * @return Completion request with tool support
     */
    OpenAICompletionRequest create_tool_completion_request(
        const std::vector<OpenAIMessage>& messages,
        const nlohmann::json& tools,
        const std::string& tool_choice = "auto",
        const std::string& model = "gpt-4-turbo-preview");

    /**
     * @brief Create function call message from function response
     * @param function_name Name of the function called
     * @param function_response Function execution result
     * @param tool_call_id Tool call ID for tool format
     * @return Message with function/tool response
     */
    OpenAIMessage create_function_response_message(
        const std::string& function_name,
        const nlohmann::json& function_response,
        const std::string& tool_call_id = "");

    /**
     * @brief Parse function calls from API response
     * @param response API response
     * @return Vector of parsed function calls
     */
    std::vector<FunctionCall> parse_function_calls_from_response(const OpenAIResponse& response);

    /**
     * @brief Check if response contains function calls
     * @param response API response
     * @return true if function calls present
     */
    bool response_contains_function_calls(const OpenAIResponse& response);

    // Fallback responses removed - Rule 7 compliance: No makeshift workarounds
};

// Convenience functions for common LLM operations

/**
 * @brief Create a simple text completion request
 */
inline OpenAICompletionRequest create_simple_completion(
    const std::string& prompt,
    const std::string& model = "gpt-4-turbo-preview",
    double temperature = 0.7) {

    return OpenAICompletionRequest{
        .model = model,
        .messages = {OpenAIMessage{"user", prompt}},
        .temperature = temperature,
        .max_tokens = 1000
    };
}

/**
 * @brief Create a chat completion with system prompt
 */
inline OpenAICompletionRequest create_chat_completion(
    const std::string& system_prompt,
    const std::string& user_message,
    const std::string& model = "gpt-4-turbo-preview") {

    return OpenAICompletionRequest{
        .model = model,
        .messages = {
            OpenAIMessage{"system", system_prompt, std::nullopt},
            OpenAIMessage{"user", user_message, std::nullopt}
        },
        .temperature = 0.7,
        .max_tokens = 2000
    };
}

/**
 * @brief Create an analysis request with specific instructions
 */
inline OpenAICompletionRequest create_analysis_request(
    const std::string& text_to_analyze,
    const std::string& analysis_instructions,
    const std::string& model = "gpt-4-turbo-preview") {

    std::string system_prompt = "You are an expert compliance and risk analysis AI. " + analysis_instructions;
    std::string user_prompt = "Please analyze the following text:\n\n" + text_to_analyze;

    return OpenAICompletionRequest{
        .model = model,
        .messages = {
            OpenAIMessage{"system", system_prompt},
            OpenAIMessage{"user", user_prompt}
        },
        .temperature = 0.1,  // Lower temperature for more consistent analysis
        .max_tokens = 3000
    };
}

} // namespace regulens
