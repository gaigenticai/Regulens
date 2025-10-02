#include "anthropic_client.hpp"

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <regex>

namespace regulens {

AnthropicClient::AnthropicClient(std::shared_ptr<ConfigurationManager> config,
                               std::shared_ptr<StructuredLogger> logger,
                               std::shared_ptr<ErrorHandler> error_handler)
    : config_manager_(config), logger_(logger), error_handler_(error_handler),
      http_client_(std::make_shared<HttpClient>()),
      total_requests_(0), successful_requests_(0), failed_requests_(0),
      total_input_tokens_(0), total_output_tokens_(0), estimated_cost_usd_(0.0),
      last_request_time_(std::chrono::system_clock::now()),
      max_requests_per_minute_(50) {  // Conservative default, can be configured
}

AnthropicClient::~AnthropicClient() {
    shutdown();
}

bool AnthropicClient::initialize() {
    try {
        // Load configuration from environment
        api_key_ = config_manager_->get_string("LLM_ANTHROPIC_API_KEY").value_or("");
        if (api_key_.empty()) {
            logger_->error("Anthropic API key not configured");
            return false;
        }

        base_url_ = config_manager_->get_string("LLM_ANTHROPIC_BASE_URL")
                   .value_or("https://api.anthropic.com/v1");
        default_model_ = config_manager_->get_string("LLM_ANTHROPIC_MODEL")
                        .value_or("claude-3-sonnet-20240229");
        max_tokens_ = static_cast<int>(config_manager_->get_int("LLM_ANTHROPIC_MAX_TOKENS")
                                      .value_or(4096));
        temperature_ = config_manager_->get_double("LLM_ANTHROPIC_TEMPERATURE")
                      .value_or(0.7);
        request_timeout_seconds_ = static_cast<int>(config_manager_->get_int("LLM_ANTHROPIC_TIMEOUT_SECONDS")
                                                   .value_or(30));
        max_retries_ = static_cast<int>(config_manager_->get_int("LLM_ANTHROPIC_MAX_RETRIES")
                                       .value_or(3));
        base_retry_delay_ = std::chrono::milliseconds(1000); // 1 second base delay for exponential backoff
        max_requests_per_minute_ = static_cast<int>(config_manager_->get_int("LLM_ANTHROPIC_MAX_REQUESTS_PER_MINUTE")
                                                   .value_or(50));

        // Validate configuration
        if (api_key_.empty() || base_url_.empty()) {
            logger_->error("Anthropic client configuration incomplete - missing API key or base URL");
            return false;
        }

        logger_->info("Anthropic client initialized with model: {}, timeout: {}s, max_tokens: {}",
                     default_model_, request_timeout_seconds_, max_tokens_);
        return true;

    } catch (const std::exception& e) {
        logger_->error("Failed to initialize Anthropic client: {}", e.what());
        return false;
    }
}

void AnthropicClient::shutdown() {
    logger_->info("Anthropic client shutdown - Total requests: {}, Successful: {}, Failed: {}",
                 static_cast<size_t>(total_requests_),
                 static_cast<size_t>(successful_requests_),
                 static_cast<size_t>(failed_requests_));
}

std::optional<ClaudeResponse> AnthropicClient::create_message(const ClaudeCompletionRequest& request) {
    total_requests_++;

    // Check rate limit
    if (!check_rate_limit()) {
        handle_api_error("rate_limit", "Rate limit exceeded",
                        {{"requests_per_minute", std::to_string(max_requests_per_minute_)}});
        return std::nullopt;
    }

    // Use error handler for circuit breaker protection
    auto result = error_handler_->execute_with_circuit_breaker<std::optional<ClaudeResponse>>(
        [this, &request]() -> std::optional<ClaudeResponse> {
            // Make the API request
            auto http_response = make_api_request(request.to_json());
            if (!http_response) {
                return std::nullopt;
            }

            // Parse the response
            auto parsed_response = parse_api_response(*http_response);
            if (!parsed_response) {
                return std::nullopt;
            }

            // Validate response
            if (!validate_response(*parsed_response)) {
                handle_api_error("validation", "Invalid API response structure");
                return std::nullopt;
            }

            // Update usage statistics
            update_usage_stats(*parsed_response);

            return parsed_response;
        },
        CIRCUIT_BREAKER_SERVICE, "AnthropicClient", "create_message"
    );

    if (result && *result) {
        successful_requests_++;
        return *result;
    } else {
        failed_requests_++;
        return std::nullopt;
    }
}

std::optional<std::string> AnthropicClient::advanced_reasoning_analysis(
    const std::string& prompt, const std::string& context, const std::string& analysis_type) {

    std::string system_prompt = create_reasoning_system_prompt(analysis_type);

    if (!context.empty()) {
        system_prompt += "\n\nAdditional Context: " + context;
    }

    ClaudeCompletionRequest request{
        .model = default_model_,
        .max_tokens = max_tokens_,
        .messages = {ClaudeMessage{"user", prompt}},
        .system = system_prompt,
        .temperature = 0.2  // Lower temperature for consistent reasoning
    };

    // Use production-grade retry mechanism with circuit breaker
    auto response = execute_with_retry(
        [this, &request]() { return create_message(request); },
        "advanced_reasoning_analysis"
    );

    if (!response || response->content.empty()) {
        throw std::runtime_error("Advanced reasoning analysis failed: No valid response after retries");
    }

    return response->content[0].content;
}

std::optional<std::string> AnthropicClient::constitutional_ai_analysis(
    const std::string& content, const std::vector<std::string>& compliance_requirements) {

    std::string system_prompt = create_constitutional_system_prompt("compliance");

    std::string user_prompt = "Please analyze the following content for constitutional AI compliance, ethical considerations, and regulatory adherence:\n\n" + content;

    if (!compliance_requirements.empty()) {
        user_prompt += "\n\nSpecific Compliance Requirements:\n";
        for (size_t i = 0; i < compliance_requirements.size(); ++i) {
            user_prompt += std::to_string(i + 1) + ". " + compliance_requirements[i] + "\n";
        }
    }

    ClaudeCompletionRequest request{
        .model = default_model_,
        .max_tokens = max_tokens_,
        .messages = {ClaudeMessage{"user", user_prompt}},
        .system = system_prompt,
        .temperature = 0.1  // Very low temperature for ethical compliance analysis
    };

    try {
        auto response = create_message(request);
        if (!response || response->content.empty()) {
            // API call succeeded but returned empty response
            record_api_failure("constitutional_ai", "Empty response from API");
            throw std::runtime_error("Constitutional AI analysis failed: Empty response from API");
        }
        return response->content[0].content;
    } catch (const std::exception& e) {
        // Proper error handling - record failure and propagate error
        record_api_failure("constitutional_ai", e.what());
        throw std::runtime_error("Constitutional AI analysis failed: " + std::string(e.what()));
    }
}

std::optional<std::string> AnthropicClient::ethical_decision_analysis(
    const std::string& scenario,
    const std::vector<std::string>& options,
    const std::vector<std::string>& constraints,
    const std::vector<std::string>& ethical_considerations) {

    std::string system_prompt = create_constitutional_system_prompt("ethical_decision");

    std::string user_prompt = "Please analyze the following decision scenario with ethical reasoning and constitutional AI principles:\n\n";
    user_prompt += "Scenario: " + scenario + "\n\n";

    if (!options.empty()) {
        user_prompt += "Available Options:\n";
        for (size_t i = 0; i < options.size(); ++i) {
            user_prompt += std::to_string(i + 1) + ". " + options[i] + "\n";
        }
        user_prompt += "\n";
    }

    if (!constraints.empty()) {
        user_prompt += "Constraints:\n";
        for (size_t i = 0; i < constraints.size(); ++i) {
            user_prompt += std::to_string(i + 1) + ". " + constraints[i] + "\n";
        }
        user_prompt += "\n";
    }

    if (!ethical_considerations.empty()) {
        user_prompt += "Ethical Considerations:\n";
        for (size_t i = 0; i < ethical_considerations.size(); ++i) {
            user_prompt += std::to_string(i + 1) + ". " + ethical_considerations[i] + "\n";
        }
        user_prompt += "\n";
    }

    user_prompt += "Please provide a comprehensive ethical decision analysis following constitutional AI principles.";

    ClaudeCompletionRequest request{
        .model = default_model_,
        .max_tokens = max_tokens_,
        .messages = {ClaudeMessage{"user", user_prompt}},
        .system = system_prompt,
        .temperature = 0.1  // Low temperature for ethical consistency
    };

    try {
        auto response = create_message(request);
        if (!response || response->content.empty()) {
            // API call succeeded but returned empty response
            record_api_failure("ethical_decision", "Empty response from API");
            throw std::runtime_error("Ethical decision analysis failed: Empty response from API");
        }
        return response->content[0].content;
    } catch (const std::exception& e) {
        // Proper error handling - record failure and propagate error
        record_api_failure("ethical_decision", e.what());
        throw std::runtime_error("Ethical decision analysis failed: " + std::string(e.what()));
    }
}

std::optional<std::string> AnthropicClient::complex_reasoning_task(
    const std::string& task_description, const nlohmann::json& data, int reasoning_steps) {

    std::string system_prompt = create_reasoning_system_prompt("complex_reasoning");

    std::string user_prompt = "Task: " + task_description + "\n\n";
    user_prompt += "Input Data:\n" + data.dump(2) + "\n\n";
    user_prompt += "Please perform this complex reasoning task with " + std::to_string(reasoning_steps) + " distinct reasoning steps.";
    user_prompt += "\n\nStructure your response with clear step-by-step reasoning and a final conclusion.";

    ClaudeCompletionRequest request{
        .model = default_model_,
        .max_tokens = max_tokens_,
        .messages = {ClaudeMessage{"user", user_prompt}},
        .system = system_prompt,
        .temperature = 0.3  // Moderate temperature for creative reasoning
    };

    try {
        auto response = create_message(request);
        if (!response || response->content.empty()) {
            // API call succeeded but returned empty response
            record_api_failure("complex_reasoning", "Empty response from API");
            throw std::runtime_error("Complex reasoning task failed: Empty response from API");
        }
        return response->content[0].content;
    } catch (const std::exception& e) {
        // Proper error handling - record failure and propagate error
        record_api_failure("complex_reasoning", e.what());
        throw std::runtime_error("Complex reasoning task failed: " + std::string(e.what()));
    }
}

std::optional<std::string> AnthropicClient::regulatory_compliance_reasoning(
    const std::string& regulation_text,
    const std::string& business_context,
    const std::vector<std::string>& risk_factors) {

    std::string system_prompt = create_constitutional_system_prompt("regulatory_compliance");

    std::string user_prompt = "Please analyze the following regulatory text in the context of the business scenario:\n\n";
    user_prompt += "Regulatory Text:\n" + regulation_text + "\n\n";
    user_prompt += "Business Context:\n" + business_context + "\n\n";

    if (!risk_factors.empty()) {
        user_prompt += "Risk Factors to Consider:\n";
        for (size_t i = 0; i < risk_factors.size(); ++i) {
            user_prompt += std::to_string(i + 1) + ". " + risk_factors[i] + "\n";
        }
        user_prompt += "\n";
    }

    user_prompt += "Please provide comprehensive regulatory compliance reasoning following constitutional AI principles.";

    ClaudeCompletionRequest request{
        .model = default_model_,
        .max_tokens = max_tokens_,
        .messages = {ClaudeMessage{"user", user_prompt}},
        .system = system_prompt,
        .temperature = 0.1  // Low temperature for regulatory consistency
    };

    try {
        auto response = create_message(request);
        if (!response || response->content.empty()) {
            // API call succeeded but returned empty response
            record_api_failure("regulatory_compliance", "Empty response from API");
            throw std::runtime_error("Regulatory compliance reasoning failed: Empty response from API");
        }
        return response->content[0].content;
    } catch (const std::exception& e) {
        // Proper error handling - record failure and propagate error
        record_api_failure("regulatory_compliance", e.what());
        throw std::runtime_error("Regulatory compliance reasoning failed: " + std::string(e.what()));
    }
}

nlohmann::json AnthropicClient::get_usage_statistics() {
    return {
        {"total_requests", static_cast<size_t>(total_requests_)},
        {"successful_requests", static_cast<size_t>(successful_requests_)},
        {"failed_requests", static_cast<size_t>(failed_requests_)},
        {"success_rate", total_requests_ > 0 ?
            (static_cast<double>(successful_requests_) / total_requests_) * 100.0 : 0.0},
        {"total_input_tokens", static_cast<size_t>(total_input_tokens_)},
        {"total_output_tokens", static_cast<size_t>(total_output_tokens_)},
        {"total_tokens", static_cast<size_t>(total_input_tokens_ + total_output_tokens_)},
        {"estimated_cost_usd", estimated_cost_usd_.load()},
        {"last_request_time", std::chrono::duration_cast<std::chrono::milliseconds>(
            last_request_time_.time_since_epoch()).count()},
        {"configuration", {
            {"model", default_model_},
            {"max_tokens", max_tokens_},
            {"temperature", temperature_},
            {"max_requests_per_minute", max_requests_per_minute_}
        }}
    };
}

nlohmann::json AnthropicClient::get_health_status() {
    auto circuit_breaker = error_handler_->get_circuit_breaker(CIRCUIT_BREAKER_SERVICE);

    return {
        {"service", "anthropic_api"},
        {"status", "operational"}, // Could be enhanced with actual health checks
        {"last_request", std::chrono::duration_cast<std::chrono::milliseconds>(
            last_request_time_.time_since_epoch()).count()},
        {"circuit_breaker", circuit_breaker ? circuit_breaker->to_json() : nullptr},
        {"usage_stats", get_usage_statistics()}
    };
}

void AnthropicClient::reset_usage_counters() {
    total_requests_ = 0;
    successful_requests_ = 0;
    failed_requests_ = 0;
    total_input_tokens_ = 0;
    total_output_tokens_ = 0;
    estimated_cost_usd_ = 0.0;

    logger_->info("Anthropic client usage counters reset");
}

// Private implementation methods

std::optional<HttpResponse> AnthropicClient::make_api_request(const nlohmann::json& payload) {
    try {
        std::string url = base_url_ + "/messages";

        std::unordered_map<std::string, std::string> headers = {
            {"x-api-key", api_key_},
            {"anthropic-version", "2023-06-01"},
            {"Content-Type", "application/json"}
        };

        std::string payload_str = payload.dump();

        logger_->debug("Making Anthropic API request to: {}", url);

        auto response = http_client_->post(url, payload_str, headers,
                                         std::chrono::seconds(request_timeout_seconds_));

        last_request_time_ = std::chrono::system_clock::now();

        if (!response) {
            handle_api_error("network", "No response from Anthropic API");
            return std::nullopt;
        }

        if (response->status_code < 200 || response->status_code >= 300) {
            handle_api_error("http_error", "HTTP " + std::to_string(response->status_code),
                           {{"status_code", std::to_string(response->status_code)},
                            {"response_body", response->body ? response->body->substr(0, 500) : "empty"}});
            return std::nullopt;
        }

        return response;

    } catch (const std::exception& e) {
        handle_api_error("exception", std::string("API request exception: ") + e.what());
        return std::nullopt;
    }
}

std::optional<ClaudeResponse> AnthropicClient::parse_api_response(const HttpResponse& response) {
    try {
        if (!response.body) {
            handle_api_error("parsing", "Empty response body");
            return std::nullopt;
        }

        nlohmann::json json_response = nlohmann::json::parse(*response.body);

        // Check for API errors
        if (json_response.contains("error")) {
            auto error = json_response["error"];
            std::string error_type = error.contains("type") ? error["type"] : "unknown";
            std::string error_message = error.contains("message") ? error["message"] : "Unknown API error";

            handle_api_error("api_error", error_message, {{"error_type", error_type}});
            return std::nullopt;
        }

        // Parse successful response
        ClaudeResponse parsed_response;

        parsed_response.id = json_response.value("id", "");
        parsed_response.type = json_response.value("type", "");
        parsed_response.role = json_response.value("role", "");
        parsed_response.model = json_response.value("model", "");
        parsed_response.stop_reason = json_response.value("stop_reason", "");

        if (json_response.contains("stop_sequence")) {
            parsed_response.stop_sequence = json_response["stop_sequence"];
        }

        // Parse content
        if (json_response.contains("content")) {
            for (const auto& content_item : json_response["content"]) {
                if (content_item.contains("text")) {
                    ClaudeMessage msg;
                    msg.role = parsed_response.role;
                    msg.content = content_item["text"];
                    parsed_response.content.push_back(msg);
                }
            }
        }

        // Parse usage
        if (json_response.contains("usage")) {
            auto usage_json = json_response["usage"];
            parsed_response.usage.input_tokens = usage_json.value("input_tokens", 0);
            parsed_response.usage.output_tokens = usage_json.value("output_tokens", 0);
        }

        // Set timestamp
        parsed_response.created_at = std::chrono::system_clock::now();

        return parsed_response;

    } catch (const std::exception& e) {
        handle_api_error("parsing", std::string("Failed to parse API response: ") + e.what(),
                        {{"response_body", response.body ? response.body->substr(0, 200) : "empty"}});
        return std::nullopt;
    }
}

void AnthropicClient::handle_api_error(const std::string& error_type,
                                     const std::string& message,
                                     const std::unordered_map<std::string, std::string>& context) {
    // Report error through error handler
    ErrorInfo error_info(ErrorCategory::EXTERNAL_API, ErrorSeverity::HIGH,
                        "AnthropicClient", "api_request", message);
    error_info.context = context;
    error_info.context["error_type"] = error_type;
    error_info.context["service"] = CIRCUIT_BREAKER_SERVICE;

    error_handler_->report_error(error_info);

    // Log the error
    logger_->error("Anthropic API error - Type: {}, Message: {}", error_type, message);
}

bool AnthropicClient::check_rate_limit() {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);

    auto now = std::chrono::system_clock::now();
    auto window_start = now - rate_limit_window_;

    // Remove old timestamps outside the window
    while (!request_timestamps_.empty() && request_timestamps_.front() < window_start) {
        request_timestamps_.pop_front();
    }

    // Check if we're within limits
    if (static_cast<int>(request_timestamps_.size()) >= max_requests_per_minute_) {
        logger_->warn("Anthropic API rate limit exceeded: {} requests in last minute",
                     request_timestamps_.size());
        return false;
    }

    // Add current request timestamp
    request_timestamps_.push_back(now);
    return true;
}

void AnthropicClient::update_usage_stats(const ClaudeResponse& response) {
    total_input_tokens_ += response.usage.input_tokens;
    total_output_tokens_ += response.usage.output_tokens;
    estimated_cost_usd_ += calculate_cost(response.model, response.usage.input_tokens, response.usage.output_tokens);

    logger_->debug("Anthropic usage updated - Input: {}, Output: {}, Cost: ${:.4f}",
                  response.usage.input_tokens, response.usage.output_tokens,
                  calculate_cost(response.model, response.usage.input_tokens, response.usage.output_tokens));
}

double AnthropicClient::calculate_cost(const std::string& model, int input_tokens, int output_tokens) {
    // Anthropic pricing (as of 2024, subject to change)
    static const std::unordered_map<std::string, std::pair<double, double>> pricing_per_1k_tokens = {
        // Input price, Output price per 1K tokens
        {"claude-3-opus-20240229", {15.0, 75.0}},      // $15/$75 per 1K
        {"claude-3-sonnet-20240229", {3.0, 15.0}},     // $3/$15 per 1K
        {"claude-3-haiku-20240307", {0.25, 1.25}},     // $0.25/$1.25 per 1K
        {"claude-3-5-sonnet-20240620", {3.0, 15.0}},   // $3/$15 per 1K
        {"claude-2.1", {8.0, 24.0}},                   // $8/$24 per 1K
        {"claude-2", {8.0, 24.0}},                     // $8/$24 per 1K
        {"claude-instant-1.2", {0.8, 2.4}}             // $0.8/$2.4 per 1K
    };

    auto it = pricing_per_1k_tokens.find(model);
    if (it == pricing_per_1k_tokens.end()) {
        logger_->warn("Unknown model for cost calculation: {}", model);
        return 0.0; // Unknown model, no cost calculation
    }

    auto [input_price, output_price] = it->second;
    double input_cost = (input_tokens / 1000.0) * input_price;
    double output_cost = (output_tokens / 1000.0) * output_price;

    return input_cost + output_cost;
}

std::string AnthropicClient::create_constitutional_system_prompt(const std::string& task_type) {
    static const std::unordered_map<std::string, std::string> prompts = {
        {"compliance", R"(
You are Claude, an AI assistant created by Anthropic. You are designed to be helpful, honest, and harmless.

As a constitutional AI, you must ensure all analysis and recommendations comply with:
1. **Legal and Regulatory Requirements**: All actions must comply with applicable laws and regulations
2. **Ethical Standards**: Consider fairness, transparency, and non-discrimination
3. **Safety and Security**: Protect user data and prevent harm
4. **Accountability**: Provide clear reasoning and acknowledge limitations
5. **Beneficence**: Act in ways that benefit users and society

When analyzing content or making recommendations, explicitly consider these constitutional principles and provide guidance that promotes safe, ethical, and compliant outcomes.

Structure your analysis to include:
- Identification of relevant constitutional principles
- Assessment of compliance with each principle
- Recommendations for improvement or mitigation
- Clear reasoning for all conclusions)"},

        {"ethical_decision", R"(
You are Claude, an AI assistant created by Anthropic. You are designed to be helpful, honest, and harmless.

For ethical decision analysis, you must apply constitutional AI principles:
1. **Autonomy**: Respect user agency and decision-making rights
2. **Beneficence**: Promote positive outcomes and prevent harm
3. **Non-maleficence**: Avoid causing harm through actions or recommendations
4. **Justice**: Ensure fairness and equitable treatment
5. **Transparency**: Provide clear reasoning and acknowledge uncertainties

Structure your ethical decision analysis:
- Identify the decision context and stakeholders
- Apply constitutional principles to each option
- Consider short-term and long-term consequences
- Provide balanced analysis of risks and benefits
- Recommend the most ethically sound course of action)"},

        {"regulatory_compliance", R"(
You are Claude, an AI assistant created by Anthropic. You are designed to be helpful, honest, and harmless.

For regulatory compliance analysis, you must ensure all interpretations and recommendations align with:
1. **Legal Accuracy**: Correct interpretation of laws and regulations
2. **Practical Application**: Consider real-world implementation challenges
3. **Risk Assessment**: Identify compliance risks and mitigation strategies
4. **Documentation**: Ensure proper record-keeping and audit trails
5. **Continuous Compliance**: Consider ongoing monitoring and adaptation

Provide comprehensive regulatory analysis that includes:
- Clear interpretation of regulatory requirements
- Practical implementation guidance
- Risk mitigation recommendations
- Compliance monitoring suggestions
- Documentation and reporting requirements)"}
    };

    auto it = prompts.find(task_type);
    if (it != prompts.end()) {
        return it->second;
    }

    return prompts.at("compliance");
}

std::string AnthropicClient::create_reasoning_system_prompt(const std::string& reasoning_type) {
    static const std::unordered_map<std::string, std::string> prompts = {
        {"general", R"(
You are Claude, an AI assistant created by Anthropic. You are designed to be helpful, honest, and harmless.

For reasoning tasks, you should:
1. **Break down complex problems** into manageable components
2. **Consider multiple perspectives** and potential outcomes
3. **Apply logical reasoning** with clear step-by-step analysis
4. **Acknowledge uncertainties** and limitations of the analysis
5. **Provide actionable insights** when possible

Structure your reasoning process clearly and provide well-supported conclusions.)"},

        {"complex_reasoning", R"(
You are Claude, an AI assistant created by Anthropic. You are designed to be helpful, honest, and harmless.

For complex reasoning tasks, you must:
1. **Decompose the problem** into fundamental components
2. **Establish clear reasoning steps** with logical progression
3. **Consider alternative hypotheses** and potential counterarguments
4. **Integrate multiple data sources** and perspectives
5. **Provide probabilistic assessments** when certainty is limited
6. **Draw well-supported conclusions** based on the evidence

Structure your response with numbered reasoning steps, clear transitions between steps, and a final synthesis of findings.)"},

        {"analysis", R"(
You are Claude, an AI assistant created by Anthropic. You are designed to be helpful, honest, and harmless.

For analytical reasoning, you should:
1. **Systematically examine** all available data and information
2. **Identify patterns and relationships** within the data
3. **Apply appropriate analytical frameworks** to the problem
4. **Consider contextual factors** that may influence the analysis
5. **Provide evidence-based conclusions** with confidence levels
6. **Identify areas requiring further investigation**

Present your analysis in a structured format with clear sections for methodology, findings, and implications.)"}
    };

    auto it = prompts.find(reasoning_type);
    if (it != prompts.end()) {
        return it->second;
    }

    return prompts.at("general");
}

bool AnthropicClient::validate_response(const ClaudeResponse& response) {
    if (response.id.empty() || response.content.empty()) {
        return false;
    }

    // Check that we have at least one content item with text
    for (const auto& content : response.content) {
        if (!content.content.empty()) {
            return true;
        }
    }

    return false;
}

// Production-grade retry mechanism implementation

std::optional<ClaudeResponse> AnthropicClient::execute_with_retry(
    std::function<std::optional<ClaudeResponse>()> operation,
    const std::string& operation_name) {

    for (int attempt = 0; attempt <= max_retries_; ++attempt) {
        try {
            auto result = operation();
            if (result) {
                // Success - record for circuit breaker
                if (error_handler_) {
                    error_handler_->record_success("anthropic_api");
                }
                return result;
            }

            // If this is the last attempt, don't retry
            if (attempt == max_retries_) {
                break;
            }

            // Record failure for circuit breaker
            if (error_handler_) {
                error_handler_->record_failure("anthropic_api");
            }

        } catch (const std::exception& e) {
            logger_->warn("Attempt {} for {} failed: {}", attempt + 1, operation_name, e.what());

            // Record failure for circuit breaker
            if (error_handler_) {
                error_handler_->record_failure("anthropic_api");
            }

            // If this is the last attempt, rethrow
            if (attempt == max_retries_) {
                throw;
            }
        }

        // Wait before retry (exponential backoff)
        if (attempt < max_retries_) {
            auto delay = base_retry_delay_ * static_cast<int>(std::pow(2, attempt));
            logger_->info("Retrying {} in {}ms (attempt {}/{})",
                         operation_name, delay.count(), attempt + 1, max_retries_);
            std::this_thread::sleep_for(delay);
        }
    }

    // All retries exhausted
    logger_->error("All retry attempts exhausted for {}", operation_name);
    return std::nullopt;
}

void AnthropicClient::load_configuration() {
    // Configuration is loaded in initialize() method
}

std::string AnthropicClient::generate_request_id() const {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);

    return "claude_req_" + std::to_string(ms) + "_" + std::to_string(dis(gen));
}

} // namespace regulens
