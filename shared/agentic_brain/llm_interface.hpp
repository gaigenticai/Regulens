/**
 * LLM Interface - Production Agentic AI Integration
 *
 * Interfaces with various LLM providers (OpenAI, Anthropic, etc.) for
 * intelligent decision making, reasoning, and learning capabilities.
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "../network/http_client.hpp"
#include "../logging/structured_logger.hpp"
#include <nlohmann/json.hpp>

namespace regulens {

enum class LLMProvider {
    NONE,
    OPENAI,
    ANTHROPIC,
    LOCAL_LLM
};

enum class LLMModel {
    NONE,
    // OpenAI models
    GPT_4_TURBO,
    GPT_4,
    GPT_3_5_TURBO,

    // Anthropic models
    CLAUDE_3_OPUS,
    CLAUDE_3_SONNET,
    CLAUDE_3_HAIKU,
    CLAUDE_2,

    // Local models
    LLAMA_3_70B,
    MISTRAL_7B,
    LOCAL_MODEL
};

struct LLMMessage {
    std::string role; // "system", "user", "assistant"
    std::string content;
    std::unordered_map<std::string, std::string> metadata;

    LLMMessage(const std::string& r, const std::string& c)
        : role(r), content(c) {}
};

struct LLMRequest {
    std::string model;
    LLMModel model_preference = LLMModel::GPT_4;
    std::vector<LLMMessage> messages;
    double temperature = 0.7;
    int max_tokens = 2000;
    bool stream = false;
    std::unordered_map<std::string, std::string> parameters;
};

struct LLMResponse {
    bool success;
    std::string content;
    std::string reasoning;
    double confidence_score;
    int tokens_used = 0;
    LLMModel model_used = LLMModel::NONE;
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, double> token_usage;
    std::string error_message;
    nlohmann::json raw_response;

    LLMResponse() : success(false), confidence_score(0.0) {}
};

// Rate limiter for API calls
class RateLimiter {
public:
    RateLimiter(int requests_per_window, std::chrono::steady_clock::duration window_duration)
        : max_requests_(requests_per_window), window_duration_(window_duration) {}

    bool allow_request() {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(mutex_);

        // Reset window if expired
        if (now - window_start_ >= window_duration_) {
            request_count_ = 0;
            window_start_ = now;
        }

        if (request_count_ < max_requests_) {
            request_count_++;
            return true;
        }

        return false;
    }

private:
    int max_requests_;
    std::chrono::steady_clock::duration window_duration_;
    int request_count_ = 0;
    std::chrono::steady_clock::time_point window_start_ = std::chrono::steady_clock::now();
    std::mutex mutex_;
};

class LLMInterface {
public:
    LLMInterface(
        std::shared_ptr<HttpClient> http_client,
        StructuredLogger* logger
    );

    ~LLMInterface();

    // Configuration
    bool configure_provider(LLMProvider provider, const nlohmann::json& config);
    bool set_model(LLMModel model);
    void set_temperature(double temperature);
    void set_provider(LLMProvider provider) { current_provider_ = provider; }

    // Core LLM operations
    LLMResponse generate_completion(const LLMRequest& request);
    LLMResponse analyze_text(const std::string& text, const std::string& task);
    LLMResponse analyze_with_context(const std::string& prompt, const nlohmann::json& context);
    LLMResponse make_decision(const nlohmann::json& context, const std::string& decision_type);

    // Specialized agentic operations
    LLMResponse assess_risk(const nlohmann::json& data, const std::string& risk_type);
    LLMResponse extract_patterns(const std::vector<nlohmann::json>& historical_data);
    LLMResponse generate_insights(const nlohmann::json& data, const std::string& domain);
    LLMResponse explain_decision(const nlohmann::json& decision_context);

    // Learning and adaptation
    LLMResponse learn_from_feedback(const nlohmann::json& decision, const nlohmann::json& feedback);
    LLMResponse update_knowledge(const std::string& knowledge_type, const nlohmann::json& new_data);

    // Compliance-specific operations
    LLMResponse analyze_regulatory_text(const std::string& text);
    LLMResponse assess_compliance_impact(const nlohmann::json& change_data);
    LLMResponse detect_anomalies(const std::vector<nlohmann::json>& data_points);

    // Performance monitoring
    nlohmann::json get_usage_stats();
    bool is_healthy();

private:
    // Provider-specific implementations
    LLMResponse call_openai(const LLMRequest& request);
    LLMResponse call_anthropic(const LLMRequest& request);
    LLMResponse call_local_llm(const LLMRequest& request);
    // Request preparation
    nlohmann::json prepare_openai_request(const LLMRequest& request);
    nlohmann::json prepare_anthropic_request(const LLMRequest& request);

    // Response parsing
    LLMResponse parse_openai_response(const nlohmann::json& response);
    LLMResponse parse_anthropic_response(const nlohmann::json& response);

    // Prompt engineering
    std::string create_system_prompt(const std::string& task_type);
    std::string create_risk_assessment_prompt(const nlohmann::json& data);
    std::string create_pattern_analysis_prompt(const std::vector<nlohmann::json>& data);
    std::string create_decision_explanation_prompt(const nlohmann::json& context);

    // Configuration validation
    bool validate_config(const nlohmann::json& config);
    void initialize_default_configs();
    bool validate_provider_config(LLMProvider provider, const nlohmann::json& config);
    bool test_provider_connection(LLMProvider provider);
    std::vector<LLMModel> get_available_models_for_provider(LLMProvider provider);
    std::string model_to_string(LLMModel model);
    bool test_openai_connection();
    bool test_anthropic_connection();
    bool test_local_connection();

    // Internal state
    std::shared_ptr<HttpClient> http_client_;
    StructuredLogger* logger_;

    LLMProvider current_provider_;
    LLMModel current_model_;
    std::unordered_map<LLMProvider, nlohmann::json> provider_configs_;

    // Rate limiting
    std::unordered_map<LLMProvider, RateLimiter> rate_limiters_;

    // Usage tracking
    struct UsageStats {
        int total_requests = 0;
        int total_tokens = 0;
        double total_cost = 0.0;
    } usage_stats_;

    // Circuit breaker for reliability
    int consecutive_failures_;
    bool circuit_breaker_open_;
    std::chrono::steady_clock::time_point last_failure_time_;

private:
    // Helper methods
    bool check_rate_limit(LLMProvider provider);
    void update_usage_stats(const LLMResponse& response);
    double calculate_cost(const LLMResponse& response);
    LLMResponse generate_openai_completion(const LLMRequest& request);
    LLMResponse generate_anthropic_completion(const LLMRequest& request);
    LLMResponse generate_local_completion(const LLMRequest& request);
};

} // namespace regulens
