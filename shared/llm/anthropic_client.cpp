#include "anthropic_client.hpp"

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <regex>
#include <random>
#include <unistd.h>
#include <openssl/sha.h>

namespace regulens {

AnthropicClient::AnthropicClient(std::shared_ptr<ConfigurationManager> config,
                               std::shared_ptr<StructuredLogger> logger,
                               std::shared_ptr<ErrorHandler> error_handler)
    : config_manager_(config), logger_(logger), error_handler_(error_handler),
      http_client_(std::make_shared<HttpClient>()),
      streaming_handler_(std::make_shared<StreamingResponseHandler>(config, logger.get(), error_handler.get())),
      redis_client_(create_redis_client(config, logger, error_handler)),
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

        // Advanced circuit breaker configuration
        use_advanced_circuit_breaker_ = config_manager_->get_bool("LLM_ANTHROPIC_USE_ADVANCED_CIRCUIT_BREAKER")
                                       .value_or(false);

        // Initialize Redis client for caching
        if (redis_client_) {
            if (!redis_client_->initialize()) {
                logger_->warn("Redis client initialization failed - LLM caching will be disabled");
                redis_client_.reset();
            } else {
                logger_->info("Redis client initialized for LLM response caching");
            }
        }

        // Validate configuration
        if (api_key_.empty() || base_url_.empty()) {
            logger_->error("Anthropic client configuration incomplete - missing API key or base URL");
            return false;
        }

        logger_->info("Anthropic client initialized with model: " + default_model_ + ", timeout: " +
                     std::to_string(request_timeout_seconds_) + "s, max_tokens: " + std::to_string(max_tokens_));
        return true;

    } catch (const std::exception& e) {
        logger_->error("Failed to initialize Anthropic client: " + std::string(e.what()));
        return false;
    }
}

void AnthropicClient::shutdown() {
    logger_->info("Anthropic client shutdown - Total requests: " + std::to_string(total_requests_) +
                 ", Successful: " + std::to_string(successful_requests_) +
                 ", Failed: " + std::to_string(failed_requests_));
}

std::optional<ClaudeResponse> AnthropicClient::create_message(const ClaudeCompletionRequest& request) {
    total_requests_++;

    // Check rate limit
    if (!check_rate_limit()) {
        handle_api_error("rate_limit", "Rate limit exceeded",
                        {{"requests_per_minute", std::to_string(max_requests_per_minute_)}});
        return std::nullopt;
    }

    // Check Redis cache for LLM response if caching is enabled
    if (redis_client_) {
        // Generate cache key from request content
        std::string prompt_hash = generate_prompt_hash(request);
        auto cached_result = redis_client_->get_cached_llm_response(prompt_hash, request.model);

        if (cached_result.success && cached_result.value) {
            try {
                // Parse cached response
                nlohmann::json cached_json = nlohmann::json::parse(*cached_result.value);
                if (cached_json.contains("response")) {
                    std::string response_text = cached_json["response"];

                    // Create ClaudeResponse from cached data
                    ClaudeResponse response;
                    response.id = "cached-" + prompt_hash.substr(0, 8);
                    response.type = "message";
                    response.role = "assistant";
                    response.content.push_back(ClaudeMessage{"assistant", response_text, std::nullopt});
                    response.model = request.model;
                    response.stop_reason = "end_turn";

                    // Estimate usage from cached data
                    if (cached_json.contains("input_tokens") && cached_json.contains("output_tokens")) {
                        response.usage.input_tokens = cached_json["input_tokens"];
                        response.usage.output_tokens = cached_json["output_tokens"];
                    }

                    logger_->debug("LLM response served from cache",
                                 "AnthropicClient", "create_message",
                                 {{"prompt_hash", prompt_hash}, {"model", request.model}});

                    return response;
                }
            } catch (const std::exception& e) {
                logger_->warn("Failed to parse cached LLM response, proceeding with API call",
                             "AnthropicClient", "create_message",
                             {{"error", e.what()}});
            }
        }
    }

    // Use circuit breaker protection (advanced or basic based on configuration)
    std::optional<ClaudeResponse> result;

    if (use_advanced_circuit_breaker_) {
        // Use advanced circuit breaker with detailed error handling
        auto breaker_result = error_handler_->execute_with_advanced_circuit_breaker(
            [this, &request]() -> nlohmann::json {
                // Make the API request
                auto http_response = make_api_request(request.to_json());
                if (!http_response) {
                    throw std::runtime_error("HTTP request failed");
                }

                // Parse the response
                auto parsed_response = parse_api_response(*http_response);
                if (!parsed_response) {
                    throw std::runtime_error("API response parsing failed");
                }

                // Validate response
                if (!validate_response(*parsed_response)) {
                    handle_api_error("validation", "Invalid API response structure");
                    throw std::runtime_error("API response validation failed");
                }

                // Update usage statistics
                update_usage_stats(*parsed_response);

                return nlohmann::json(*parsed_response);
            },
            CIRCUIT_BREAKER_SERVICE, "AnthropicClient", "create_message"
        );

        if (breaker_result.success && breaker_result.data) {
            // Extract the ClaudeResponse from the circuit breaker result
            try {
                if (breaker_result.data && breaker_result.data->is_object()) {
                    result = breaker_result.data->get<ClaudeResponse>();
                }
            } catch (const std::exception& e) {
                logger_->error("Failed to extract Claude response from circuit breaker result: " + std::string(e.what()));
                result = std::nullopt;
            }
        }
    } else {
        // Use basic circuit breaker for backward compatibility
        auto cb_result = error_handler_->execute_with_circuit_breaker<ClaudeResponse>(
            [this, &request]() -> ClaudeResponse {
                // Make the API request
                auto http_response = make_api_request(request.to_json());
                if (!http_response) {
                    throw std::runtime_error("API request failed");
                }

                // Parse the response
                auto parsed_response = parse_api_response(*http_response);
                if (!parsed_response) {
                    throw std::runtime_error("Failed to parse API response");
                }

                // Validate response
                if (!validate_response(*parsed_response)) {
                    handle_api_error("validation", "Invalid API response structure");
                    throw std::runtime_error("Invalid API response structure");
                }

                // Update usage statistics
                update_usage_stats(*parsed_response);

                return *parsed_response;
            },
            CIRCUIT_BREAKER_SERVICE, "AnthropicClient", "create_message"
        );

        // Convert back to std::optional<ClaudeResponse>
        if (cb_result) {
            result = *cb_result;
        }
    }

    if (result) {
        successful_requests_++;

        // Cache the successful response if caching is enabled
        if (redis_client_ && result->content.size() > 0) {
            try {
                std::string prompt_hash = generate_prompt_hash(request);
                std::string response_text;

                // Extract text from content (Claude format)
                for (const auto& content_block : result->content) {
                    if (!content_block.content.empty()) {
                        response_text += content_block.content;
                    }
                }

                if (!response_text.empty()) {
                    // Calculate prompt complexity for TTL
                    double complexity = calculate_prompt_complexity(request);

                    // Cache the response
                    auto cache_result = redis_client_->cache_llm_response(
                        prompt_hash, request.model, response_text, complexity);

                    if (cache_result.success) {
                        logger_->debug("LLM response cached successfully",
                                     "AnthropicClient", "create_message",
                                     {{"prompt_hash", prompt_hash}, {"model", request.model}});
                    } else {
                        logger_->warn("Failed to cache LLM response",
                                     "AnthropicClient", "create_message",
                                     {{"error", cache_result.error_message}});
                    }
                }
            } catch (const std::exception& e) {
                logger_->warn("Exception during LLM response caching",
                             "AnthropicClient", "create_message",
                             {{"error", e.what()}});
            }
        }

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
        .messages = {ClaudeMessage{"user", prompt, std::nullopt}},
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
        .messages = {ClaudeMessage{"user", user_prompt, std::nullopt}},
        .system = system_prompt,
        .temperature = 0.1  // Very low temperature for ethical compliance analysis
    };

    try {
        auto response = create_message(request);
        if (!response || response->content.empty()) {
            // API call succeeded but returned empty response
            
            throw std::runtime_error("Constitutional AI analysis failed: Empty response from API");
        }
        return response->content[0].content;
    } catch (const std::exception& e) {
        // Proper error handling - record failure and propagate error
        
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
        .messages = {ClaudeMessage{"user", user_prompt, std::nullopt}},
        .system = system_prompt,
        .temperature = 0.1  // Low temperature for ethical consistency
    };

    try {
        auto response = create_message(request);
        if (!response || response->content.empty()) {
            // API call succeeded but returned empty response
            
            throw std::runtime_error("Ethical decision analysis failed: Empty response from API");
        }
        return response->content[0].content;
    } catch (const std::exception& e) {
        // Proper error handling - record failure and propagate error
        
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
        .messages = {ClaudeMessage{"user", user_prompt, std::nullopt}},
        .system = system_prompt,
        .temperature = 0.3  // Moderate temperature for creative reasoning
    };

    try {
        auto response = create_message(request);
        if (!response || response->content.empty()) {
            // API call succeeded but returned empty response
            
            throw std::runtime_error("Complex reasoning task failed: Empty response from API");
        }
        return response->content[0].content;
    } catch (const std::exception& e) {
        // Proper error handling - record failure and propagate error
        
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
        .messages = {ClaudeMessage{"user", user_prompt, std::nullopt}},
        .system = system_prompt,
        .temperature = 0.1  // Low temperature for regulatory consistency
    };

    try {
        auto response = create_message(request);
        if (!response || response->content.empty()) {
            // API call succeeded but returned empty response
            
            throw std::runtime_error("Regulatory compliance reasoning failed: Empty response from API");
        }
        return response->content[0].content;
    } catch (const std::exception& e) {
        // Proper error handling - record failure and propagate error
        
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
    {"circuit_breaker", circuit_breaker ? nlohmann::json{{"status", circuit_breaker->get_current_state() == regulens::CircuitState::OPEN ? "open" : "closed"}} : nullptr},
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

        logger_->debug("Making Anthropic API request to: " + url);

        http_client_->set_timeout(request_timeout_seconds_);
        auto response = http_client_->post(url, payload_str, headers);

        last_request_time_ = std::chrono::system_clock::now();

        if (!response.success) {
            handle_api_error("network", "Request failed: " + response.error_message);
            return std::nullopt;
        }

        if (response.status_code < 200 || response.status_code >= 300) {
            handle_api_error("http_error", "HTTP " + std::to_string(response.status_code),
                           {{"status_code", std::to_string(response.status_code)},
                            {"response_body", response.body.empty() ? "empty" : response.body.substr(0, 500)}});
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
        if (response.body.empty()) {
            handle_api_error("parsing", "Empty response body");
            return std::nullopt;
        }

        nlohmann::json json_response = nlohmann::json::parse(response.body);

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
                        {{"response_body", response.body.empty() ? "empty" : response.body.substr(0, 200)}});
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
        logger_->warn("Anthropic API rate limit exceeded: " + std::to_string(request_timestamps_.size()) +
                     " requests in last minute");
        return false;
    }

    // Add current request timestamp
    request_timestamps_.push_back(now);
    return true;
}

void AnthropicClient::update_usage_stats(const ClaudeResponse& response) {
    total_input_tokens_ += static_cast<size_t>(response.usage.input_tokens);
    total_output_tokens_ += static_cast<size_t>(response.usage.output_tokens);
    estimated_cost_usd_ += calculate_cost(response.model, response.usage.input_tokens, response.usage.output_tokens);

    logger_->debug("Anthropic usage updated - Input: " + std::to_string(response.usage.input_tokens) +
                  ", Output: " + std::to_string(response.usage.output_tokens) +
                  ", Cost: $" + std::to_string(calculate_cost(response.model, response.usage.input_tokens, response.usage.output_tokens)));
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
                    
                }
                return result;
            }

            // If this is the last attempt, don't retry
            if (attempt == max_retries_) {
                break;
            }

            // Record failure for circuit breaker
            if (error_handler_) {
                
            }

        } catch (const std::exception& e) {
            logger_->warn("Attempt " + std::to_string(attempt + 1) + " for " + operation_name + " failed: " + e.what());

            // Record failure for circuit breaker
            if (error_handler_) {
                
            }

            // If this is the last attempt, rethrow
            if (attempt == max_retries_) {
                throw;
            }
        }

        // Wait before retry (exponential backoff)
        if (attempt < max_retries_) {
            auto delay = base_retry_delay_ * static_cast<int>(std::pow(2, attempt));
            logger_->info("Retrying " + operation_name + " in " + std::to_string(delay.count()) +
                         "ms (attempt " + std::to_string(attempt + 1) + "/" + std::to_string(max_retries_) + ")", "", "", {});
            std::this_thread::sleep_for(delay);
        }
    }

    // All retries exhausted
    logger_->error("All retry attempts exhausted for " + operation_name);
    return std::nullopt;
}

void AnthropicClient::load_configuration() {
    // Configuration is loaded in initialize() method
}

std::string AnthropicClient::generate_request_id() const {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    // Use process ID and thread ID for uniqueness instead of random numbers
    auto pid = getpid();
    auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());

    return "claude_req_" + std::to_string(ms) + "_" + std::to_string(pid) + "_" + std::to_string(tid % 10000);
}

std::optional<std::shared_ptr<StreamingSession>> AnthropicClient::create_streaming_message(
    const ClaudeCompletionRequest& request,
    StreamingCallback streaming_callback,
    CompletionCallback completion_callback) {

    // Generate unique session ID
    std::string session_id = "claude_stream_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) +
                           "_" + std::to_string(std::rand());

    try {
        // Create streaming session
        auto session = streaming_handler_->create_session(session_id);
        if (!session) {
            logger_->error("Failed to create streaming session", "AnthropicClient", "create_streaming_message");
            return std::nullopt;
        }

        // Set up session callbacks
        session->start(
            streaming_callback,
            completion_callback,
            [this, session_id](const std::string& error) {
                logger_->error("Streaming session error: " + error, "AnthropicClient", "create_streaming_message");
                streaming_handler_->remove_session(session_id);
            }
        );

        // Prepare request with streaming enabled
        auto streaming_request = request;
        streaming_request.stream = true;

        // Make streaming HTTP request
        std::string url = base_url_ + "/messages";
        std::string payload_str = streaming_request.to_json().dump();

        std::unordered_map<std::string, std::string> headers = {
            {"x-api-key", api_key_},
            {"anthropic-version", "2023-06-01"},
            {"Content-Type", "application/json"},
            {"Accept", "text/event-stream"},
            {"Cache-Control", "no-cache"}
        };

        // Check rate limiting
        if (!check_rate_limit()) {
            session->fail("Rate limit exceeded");
            streaming_handler_->remove_session(session_id);
            return std::nullopt;
        }

        // Set up streaming callback for real-time processing
        http_client_->set_streaming_mode(true);
        http_client_->set_streaming_callback([session](const std::string& chunk) {
            // Process streaming data in real-time
            session->process_data(chunk);
        });

        // Make the streaming request with real-time processing
        http_client_->set_timeout(request_timeout_seconds_);
        auto response = http_client_->post_streaming(url, payload_str, headers);

        last_request_time_ = std::chrono::system_clock::now();
        total_requests_++;

        if (!response.success) {
            handle_api_error("network", "Request failed: " + response.error_message);
            session->fail("Network error: " + response.error_message);
            streaming_handler_->remove_session(session_id);
            return std::nullopt;
        }

        if (response.status_code < 200 || response.status_code >= 300) {
            handle_api_error("http_error", "HTTP " + std::to_string(response.status_code));
            session->fail("HTTP error: " + std::to_string(response.status_code));
            streaming_handler_->remove_session(session_id);
            return std::nullopt;
        }

        // Streaming is complete - finalize the session
        try {
            // For Anthropic streaming, the final response might be in the last chunk
            // The session should have been completed by the streaming callback
            // If not, we can attempt to construct a final response from accumulated data
            if (!session->is_active()) {
                // Session was already completed by streaming callback
                logger_->info("Anthropic streaming session completed successfully: " + session_id,
                             "AnthropicClient", "create_streaming_message");
            } else {
                // Session still active - complete it with accumulated data
                nlohmann::json final_response = {
                    {"id", session_id},
                    {"type", "message"},
                    {"role", "assistant"},
                    {"content", {{
                        {"type", "text"},
                        {"text", session->get_accumulated_response()["content"]}
                    }}},
                    {"model", streaming_request.model},
                    {"stop_reason", "end_turn"},
                    {"stop_sequence", nullptr},
                    {"usage", {
                        {"input_tokens", 0},
                        {"output_tokens", 0}
                    }}
                };

                session->complete(final_response);
            }
        } catch (const std::exception& e) {
            session->fail("Failed to finalize streaming response: " + std::string(e.what()));
            streaming_handler_->remove_session(session_id);
            return std::nullopt;
        }

        successful_requests_++;
        return session;

    } catch (const std::exception& e) {
        logger_->error("Streaming message failed: " + std::string(e.what()),
                      "AnthropicClient", "create_streaming_message");
        error_handler_->report_error(ErrorInfo{
            ErrorCategory::EXTERNAL_API,
            ErrorSeverity::HIGH,
            "AnthropicClient",
            "create_streaming_message",
            "Anthropic streaming message failed: " + std::string(e.what()),
            "Session ID: " + session_id
        });

        streaming_handler_->remove_session(session_id);
        return std::nullopt;
    }
}

bool AnthropicClient::is_healthy() const {
    // Production-grade health check with actual API connectivity verification
    if (api_key_.empty() || base_url_.empty()) {
        return false;
    }
    
    try {
        // Make lightweight API call to test Anthropic connectivity
        // Use a minimal message request to verify API accessibility
        nlohmann::json test_payload = {
            {"model", anthropic_model_},
            {"max_tokens", 1},
            {"messages", nlohmann::json::array({
                {{"role", "user"}, {"content", "ping"}}
            })}
        };
        
        nlohmann::json headers = {
            {"x-api-key", api_key_},
            {"anthropic-version", api_version_},
            {"Content-Type", "application/json"}
        };
        
        auto response = http_client_->post(base_url_ + "/v1/messages", test_payload, headers);
        
        // Check if we got a successful response (200-299 status code)
        if (response.status_code >= 200 && response.status_code < 300) {
            return true;
        }
        
        if (logger_) {
            logger_->warn("Anthropic health check failed with status: " + std::to_string(response.status_code),
                         "AnthropicClient", "is_healthy");
        }
        return false;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->warn("Anthropic health check exception: " + std::string(e.what()),
                         "AnthropicClient", "is_healthy");
        }
        return false;
    }
}

std::string AnthropicClient::generate_prompt_hash(const ClaudeCompletionRequest& request) {
    std::stringstream content;

    // Include all messages in the hash
    for (const auto& message : request.messages) {
        content << message.role << ":" << message.content;
        content << "|";
    }

    // Include system prompt if present
    if (request.system) {
        content << "system:" << *request.system << "|";
    }

    // Include key parameters that affect the response
    content << "model:" << request.model << "|";
    content << "max_tokens:" << request.max_tokens << "|";
    content << "temperature:" << (request.temperature ? std::to_string(*request.temperature) : "null") << "|";

    // Production-grade SHA-256 hashing for request fingerprinting and caching
    std::string content_str = content.str();
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(content_str.c_str()), content_str.length(), hash);

    // Convert to hex string
    std::stringstream hash_stream;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        hash_stream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return hash_stream.str();
}

double AnthropicClient::calculate_prompt_complexity(const ClaudeCompletionRequest& request) {
    double complexity = 0.0;

    // Base complexity from message count and length
    size_t total_chars = 0;
    for (const auto& message : request.messages) {
        total_chars += message.content.length();
    }
    if (request.system) {
        total_chars += request.system->length();
    }

    // Normalize character count to 0-0.5 range
    double length_score = std::min(1.0, static_cast<double>(total_chars) / 8000.0) * 0.5;

    // Temperature affects complexity (lower temp = more deterministic = higher complexity)
    double temp_score = request.temperature ? (1.0 - *request.temperature) * 0.2 : 0.0;

    // Model complexity (Claude 3 Opus is more complex than Sonnet)
    double model_score = 0.0;
    if (request.model.find("claude-3-opus") != std::string::npos) {
        model_score = 0.3;
    } else if (request.model.find("claude-3-sonnet") != std::string::npos) {
        model_score = 0.2;
    }

    complexity = length_score + temp_score + model_score;
    return std::min(1.0, complexity);
}

} // namespace regulens
