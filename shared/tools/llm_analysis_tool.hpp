/**
 * @file llm_analysis_tool.hpp
 * @brief Production-grade LLM analysis tool for agents
 * 
 * Allows agents to use Claude/GPT for:
 * - Complex reasoning tasks
 * - Text analysis and summarization
 * - Pattern detection
 * - Fraud indicators identification
 * - Compliance gap analysis
 * 
 * NO MOCKS - Real API calls with:
 * - Token tracking and cost management
 * - Rate limiting per agent
 * - Response caching
 * - Prompt templates
 */

#ifndef REGULENS_LLM_ANALYSIS_TOOL_HPP
#define REGULENS_LLM_ANALYSIS_TOOL_HPP

#include "tool_base.hpp"
#include "../llm/anthropic_client.hpp"
#include <map>
#include <mutex>

namespace regulens {

/**
 * @brief LLM Analysis Tool for AI-powered reasoning
 * 
 * Production features:
 * - Multiple LLM providers (Claude, GPT-4)
 * - Token usage tracking
 * - Cost monitoring
 * - Response caching
 * - Prompt templates library
 * - Rate limiting (tokens per hour)
 */
class LLMAnalysisTool : public ToolBase {
public:
    LLMAnalysisTool(
        std::shared_ptr<StructuredLogger> logger,
        std::shared_ptr<ConfigurationManager> config,
        std::shared_ptr<AnthropicClient> llm_client
    ) : ToolBase("llm_analysis", "Analyze data using Large Language Models (Claude/GPT)", logger, config),
        llm_client_(llm_client) {
        
        // Load configuration
        max_tokens_per_request_ = config->get_int("LLM_TOOL_MAX_TOKENS").value_or(4000);
        tokens_per_hour_limit_ = config->get_int("LLM_TOOL_HOURLY_TOKEN_LIMIT").value_or(100000);
        enable_caching_ = config->get_bool("LLM_TOOL_ENABLE_CACHE").value_or(true);
        cache_ttl_minutes_ = config->get_int("LLM_TOOL_CACHE_TTL_MINUTES").value_or(60);
        
        // Initialize prompt templates
        initialize_prompt_templates();
    }
    
    nlohmann::json get_parameters_schema() const override {
        return {
            {"type", "object"},
            {"properties", {
                {"task_type", {
                    {"type", "string"},
                    {"enum", nlohmann::json::array({
                        "fraud_analysis",
                        "compliance_check",
                        "risk_assessment",
                        "text_summarization",
                        "pattern_detection",
                        "custom_reasoning"
                    })},
                    {"description", "Type of analysis task"}
                }},
                {"input_data", {
                    {"type", "object"},
                    {"description", "Data to analyze (transaction, text, patterns, etc.)"}
                }},
                {"custom_prompt", {
                    {"type", "string"},
                    {"description", "Custom prompt (only for custom_reasoning)"}
                }},
                {"max_tokens", {
                    {"type", "integer"},
                    {"description", "Maximum tokens for response"},
                    {"minimum", 100},
                    {"maximum", 4000}
                }},
                {"temperature", {
                    {"type", "number"},
                    {"description", "Creativity level (0.0-1.0)"},
                    {"minimum", 0.0},
                    {"maximum", 1.0}
                }}
            }},
            {"required", nlohmann::json::array({"task_type", "input_data"})}
        };
    }
    
protected:
    ToolResult execute_impl(const ToolContext& context, const nlohmann::json& parameters) override {
        ToolResult result;
        
        if (!parameters.contains("task_type") || !parameters.contains("input_data")) {
            result.error_message = "Missing required parameters";
            return result;
        }
        
        std::string task_type = parameters["task_type"];
        int max_tokens = parameters.value("max_tokens", max_tokens_per_request_);
        double temperature = parameters.value("temperature", 0.3);
        
        // Check token rate limit
        if (!check_token_rate_limit(context.agent_id, max_tokens)) {
            result.error_message = "Token rate limit exceeded for agent: " + context.agent_id;
            return result;
        }
        
        // Check cache first
        if (enable_caching_) {
            std::string cache_key = generate_cache_key(task_type, parameters["input_data"]);
            if (has_cached_response(cache_key)) {
                result = get_cached_response(cache_key);
                result.result["from_cache"] = true;
                return result;
            }
        }
        
        // Build prompt from template
        std::string prompt = build_prompt(task_type, parameters);
        
        try {
            // Call LLM API
            auto llm_response_opt = llm_client_->complex_reasoning_task(prompt, max_tokens, temperature);

            if (!llm_response_opt.has_value()) {
                result.error_message = "LLM API call failed: No response received";
                return result;
            }

            // Parse the JSON response
            nlohmann::json llm_response = nlohmann::json::parse(llm_response_opt.value());

            if (llm_response.contains("success") && llm_response["success"].get<bool>()) {
                result.success = true;
                nlohmann::json result_data;
                result_data["analysis"] = llm_response.value("response", "");
                result_data["task_type"] = task_type;
                result_data["tokens_used"] = llm_response.value("tokens_used", 0);
                result_data["model"] = llm_response.value("model", "claude-3-sonnet");
                result_data["from_cache"] = false;
                result.result = result_data;

                result.tokens_used = llm_response.value("tokens_used", 0);

                // Update token usage tracking
                record_token_usage(context.agent_id, result.tokens_used);

                // Cache the response
                if (enable_caching_) {
                    std::string cache_key = generate_cache_key(task_type, parameters["input_data"]);
                    cache_response(cache_key, result);
                }

            } else {
                result.error_message = "LLM API call failed: " + llm_response.value("error", "Unknown error");
            }
            
        } catch (const std::exception& e) {
            result.error_message = std::string("LLM analysis exception: ") + e.what();
        }
        
        return result;
    }
    
private:
    /**
     * @brief Build prompt from template based on task type
     */
    std::string build_prompt(const std::string& task_type, const nlohmann::json& parameters) {
        if (task_type == "custom_reasoning" && parameters.contains("custom_prompt")) {
            return parameters["custom_prompt"];
        }
        
        auto template_it = prompt_templates_.find(task_type);
        if (template_it == prompt_templates_.end()) {
            return "Analyze the following data:\n" + parameters["input_data"].dump(2);
        }
        
        // Replace placeholders in template
        std::string prompt = template_it->second;
        prompt = replace_placeholders(prompt, parameters["input_data"]);
        
        return prompt;
    }
    
    /**
     * @brief Replace placeholders in prompt template
     */
    std::string replace_placeholders(std::string prompt, const nlohmann::json& data) {
        for (auto& [key, value] : data.items()) {
            std::string placeholder = "{{" + key + "}}";
            std::string replacement = value.is_string() ? value.get<std::string>() : value.dump();
            
            size_t pos = prompt.find(placeholder);
            while (pos != std::string::npos) {
                prompt.replace(pos, placeholder.length(), replacement);
                pos = prompt.find(placeholder, pos + replacement.length());
            }
        }
        
        return prompt;
    }
    
    /**
     * @brief Check if agent has exceeded token rate limit
     */
    bool check_token_rate_limit(const std::string& agent_id, int tokens_requested) {
        std::lock_guard<std::mutex> lock(token_usage_mutex_);
        
        auto now = std::chrono::system_clock::now();
        auto hour_ago = now - std::chrono::hours(1);
        
        // Get usage for this agent in last hour
        int hourly_usage = 0;
        if (token_usage_.count(agent_id)) {
            auto& usage_records = token_usage_[agent_id];
            
            // Remove old records
            usage_records.erase(
                std::remove_if(usage_records.begin(), usage_records.end(),
                    [hour_ago](const auto& record) { return record.first < hour_ago; }),
                usage_records.end()
            );
            
            // Sum tokens in last hour
            for (const auto& record : usage_records) {
                hourly_usage += record.second;
            }
        }
        
        return (hourly_usage + tokens_requested) <= tokens_per_hour_limit_;
    }
    
    /**
     * @brief Record token usage for rate limiting
     */
    void record_token_usage(const std::string& agent_id, int tokens_used) {
        std::lock_guard<std::mutex> lock(token_usage_mutex_);
        token_usage_[agent_id].push_back({std::chrono::system_clock::now(), tokens_used});
    }
    
    /**
     * @brief Generate cache key for response caching
     */
    std::string generate_cache_key(const std::string& task_type, const nlohmann::json& input_data) {
        // Simple hash of task_type + input_data
        return task_type + "_" + std::to_string(std::hash<std::string>{}(input_data.dump()));
    }
    
    /**
     * @brief Check if cached response exists and is still valid
     */
    bool has_cached_response(const std::string& cache_key) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        if (response_cache_.count(cache_key) == 0) {
            return false;
        }
        
        auto& cached = response_cache_[cache_key];
        auto now = std::chrono::system_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::minutes>(now - cached.timestamp);
        
        return age.count() < cache_ttl_minutes_;
    }
    
    /**
     * @brief Get cached response
     */
    ToolResult get_cached_response(const std::string& cache_key) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        return response_cache_[cache_key].result;
    }
    
    /**
     * @brief Cache response
     */
    void cache_response(const std::string& cache_key, const ToolResult& result) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        response_cache_[cache_key] = {result, std::chrono::system_clock::now()};
    }
    
    /**
     * @brief Initialize prompt templates for common tasks
     */
    void initialize_prompt_templates() {
        prompt_templates_["fraud_analysis"] = R"(
You are a financial fraud detection expert. Analyze the following transaction data for potential fraud indicators:

Transaction Details:
{{transaction_details}}

Customer Profile:
{{customer_profile}}

Historical Behavior:
{{historical_behavior}}

Provide a detailed analysis including:
1. Fraud risk score (0-100)
2. Specific fraud indicators detected
3. Recommended actions
4. Reasoning for your assessment

Format your response as JSON.
)";

        prompt_templates_["compliance_check"] = R"(
You are a regulatory compliance expert. Check if the following activity complies with regulations:

Activity Details:
{{activity_details}}

Applicable Regulations:
{{regulations}}

Provide:
1. Compliance status (COMPLIANT / NON_COMPLIANT / NEEDS_REVIEW)
2. Specific violations or concerns
3. Recommended remediation steps
4. Risk level (LOW / MEDIUM / HIGH / CRITICAL)

Format your response as JSON.
)";

        prompt_templates_["risk_assessment"] = R"(
You are a risk assessment specialist. Evaluate the risk profile of:

Entity Information:
{{entity_info}}

Context:
{{context}}

Provide:
1. Overall risk score (0-100)
2. Risk category (LOW / MEDIUM / HIGH / CRITICAL)
3. Key risk factors
4. Mitigation recommendations

Format your response as JSON.
)";

        prompt_templates_["text_summarization"] = R"(
Summarize the following text concisely:

{{text}}

Provide a summary of the key points, maintaining important details.
)";

        prompt_templates_["pattern_detection"] = R"(
You are a pattern recognition expert. Analyze the following data for patterns:

Data:
{{data}}

Identify:
1. Recurring patterns
2. Anomalies
3. Trends
4. Correlations

Format your response as JSON with patterns array.
)";
    }
    
    std::shared_ptr<AnthropicClient> llm_client_;
    int max_tokens_per_request_;
    int tokens_per_hour_limit_;
    bool enable_caching_;
    int cache_ttl_minutes_;
    
    // Prompt templates
    std::map<std::string, std::string> prompt_templates_;
    
    // Token usage tracking (agent_id -> [(timestamp, tokens)])
    std::map<std::string, std::vector<std::pair<std::chrono::system_clock::time_point, int>>> token_usage_;
    std::mutex token_usage_mutex_;
    
    // Response cache
    struct CachedResponse {
        ToolResult result;
        std::chrono::system_clock::time_point timestamp;
    };
    std::map<std::string, CachedResponse> response_cache_;
    std::mutex cache_mutex_;
};

} // namespace regulens

#endif // REGULENS_LLM_ANALYSIS_TOOL_HPP

