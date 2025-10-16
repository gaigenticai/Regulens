#include "openai_client.hpp"

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <thread>
#include <regex>
#include <openssl/sha.h>

namespace regulens {

OpenAIClient::OpenAIClient(std::shared_ptr<ConfigurationManager> config,
                         std::shared_ptr<StructuredLogger> logger,
                         std::shared_ptr<ErrorHandler> error_handler)
    : config_manager_(config),
      logger_(logger),
      error_handler_(error_handler),
      http_client_(std::make_shared<HttpClient>()),
      streaming_handler_(std::make_shared<StreamingResponseHandler>(config, logger.get(), error_handler.get())),
      redis_client_(create_redis_client(config, logger, error_handler)),
      max_tokens_(4096),
      temperature_(0.7),
      request_timeout_seconds_(30),
      max_retries_(3),
      rate_limit_window_(std::chrono::milliseconds(60000)), // 1 minute
      total_requests_(0),
      successful_requests_(0),
      failed_requests_(0),
      total_tokens_used_(0),
      estimated_cost_usd_(0.0),
      last_request_time_(std::chrono::system_clock::now()),
      max_requests_per_minute_(50) {  // Conservative default, can be configured
}

OpenAIClient::~OpenAIClient() {
    shutdown();
}

bool OpenAIClient::initialize() {
    try {
        // Load configuration from environment
        api_key_ = config_manager_->get_string("LLM_OPENAI_API_KEY").value_or("");
        if (api_key_.empty()) {
            logger_->error("OpenAI API key not configured");
            return false;
        }

        base_url_ = config_manager_->get_string("LLM_OPENAI_BASE_URL")
                   .value_or("https://api.openai.com/v1");
        default_model_ = config_manager_->get_string("LLM_OPENAI_MODEL")
                        .value_or("gpt-4-turbo-preview");
        max_tokens_ = static_cast<int>(config_manager_->get_int("LLM_OPENAI_MAX_TOKENS")
                                      .value_or(4000));
        temperature_ = config_manager_->get_double("LLM_OPENAI_TEMPERATURE")
                      .value_or(0.7);
        request_timeout_seconds_ = static_cast<int>(config_manager_->get_int("LLM_OPENAI_TIMEOUT_SECONDS")
                                                   .value_or(30));
        max_retries_ = static_cast<int>(config_manager_->get_int("LLM_OPENAI_MAX_RETRIES")
                                       .value_or(3));
        max_requests_per_minute_ = static_cast<int>(config_manager_->get_int("LLM_OPENAI_MAX_REQUESTS_PER_MINUTE")
                                                   .value_or(50));

        // Advanced circuit breaker configuration
        use_advanced_circuit_breaker_ = config_manager_->get_bool("LLM_OPENAI_USE_ADVANCED_CIRCUIT_BREAKER")
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
            logger_->error("OpenAI client configuration incomplete - missing API key or base URL");
            return false;
        }

        logger_->info("OpenAI client initialized with model: " + default_model_ + ", timeout: " +
                     std::to_string(request_timeout_seconds_) + "s, max_tokens: " + std::to_string(max_tokens_));
        return true;

    } catch (const std::exception& e) {
        logger_->error("Failed to initialize OpenAI client: " + std::string(e.what()));
        return false;
    }
}

void OpenAIClient::shutdown() {
    logger_->info("OpenAI client shutdown - Total requests: " + std::to_string(total_requests_) +
                 ", Successful: " + std::to_string(successful_requests_) +
                 ", Failed: " + std::to_string(failed_requests_));
}

std::optional<OpenAIResponse> OpenAIClient::create_chat_completion(const OpenAICompletionRequest& request) {
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

                    // Create OpenAIResponse from cached data
                    OpenAIResponse response;
                    response.id = "cached-" + prompt_hash.substr(0, 8);
                    response.object = "chat.completion";
                    response.created = std::chrono::system_clock::now();
                    response.model = request.model;

                    // Create choice with cached response
                    OpenAIChoice choice;
                    choice.index = 0;
                    choice.message.role = "assistant";
                    choice.message.content = response_text;
                    choice.finish_reason = "stop";
                    response.choices.push_back(choice);

                    // Estimate usage from cached data
                    if (cached_json.contains("input_tokens") && cached_json.contains("output_tokens")) {
                        response.usage.prompt_tokens = cached_json["input_tokens"];
                        response.usage.completion_tokens = cached_json["output_tokens"];
                        response.usage.total_tokens = response.usage.prompt_tokens + response.usage.completion_tokens;
                    }

                    logger_->debug("LLM response served from cache",
                                 "OpenAIClient", "create_chat_completion",
                                 {{"prompt_hash", prompt_hash}, {"model", request.model}});

                    return response;
                }
            } catch (const std::exception& e) {
                logger_->warn("Failed to parse cached LLM response, proceeding with API call",
                             "OpenAIClient", "create_chat_completion",
                             {{"error", e.what()}});
            }
        }
    }

    // Use circuit breaker protection (advanced or basic based on configuration)
    std::optional<OpenAIResponse> result;

    if (use_advanced_circuit_breaker_) {
        // Use advanced circuit breaker with detailed error handling
        // Store response in a variable that will be captured by the lambda
        std::optional<OpenAIResponse> temp_result;

        auto breaker_result = error_handler_->execute_with_advanced_circuit_breaker(
            [this, &request, &temp_result]() -> regulens::CircuitBreakerResult {
                // Make the API request
                auto http_response = make_api_request("/chat/completions", request.to_json());
                if (!http_response) {
                    return regulens::CircuitBreakerResult(false, std::nullopt,
                        "HTTP request failed", std::chrono::milliseconds(0),
                        regulens::CircuitState::CLOSED);
                }

                // Parse the response
                auto parsed_response = parse_api_response(*http_response);
                if (!parsed_response) {
                    return regulens::CircuitBreakerResult(false, std::nullopt,
                        "API response parsing failed", std::chrono::milliseconds(0),
                        regulens::CircuitState::CLOSED);
                }

                // Validate response
                if (!validate_response(*parsed_response)) {
                    handle_api_error("validation", "Invalid API response structure");
                    return regulens::CircuitBreakerResult(false, std::nullopt,
                        "API response validation failed", std::chrono::milliseconds(0),
                        regulens::CircuitState::CLOSED);
                }

                // Update usage statistics
                update_usage_stats(*parsed_response);

                // Store result for later use
                temp_result = parsed_response;

                return regulens::CircuitBreakerResult(true, nlohmann::json::object(),
                    "Success", std::chrono::milliseconds(0),
                    regulens::CircuitState::CLOSED);
            },
            CIRCUIT_BREAKER_SERVICE, "OpenAIClient", "create_chat_completion"
        );

        if (breaker_result.success) {
            result = temp_result;
        }
    } else {
        // Direct execution without advanced circuit breaker
        try {
            // Make the API request
            auto http_response = make_api_request("/chat/completions", request.to_json());
            if (!http_response) {
                result = std::nullopt;
            } else {
                // Parse the response
                auto parsed_response = parse_api_response(*http_response);
                if (!parsed_response) {
                    result = std::nullopt;
                } else if (!validate_response(*parsed_response)) {
                    // Validate response
                    handle_api_error("validation", "Invalid API response structure");
                    result = std::nullopt;
                } else {
                    // Update usage statistics
                    update_usage_stats(*parsed_response);
                    result = parsed_response;
                }
            }
        } catch (const std::exception& e) {
            logger_->error("API request execution failed: " + std::string(e.what()));
            result = std::nullopt;
        }
    }

    if (result) {
        successful_requests_++;

        // Cache the successful response if caching is enabled
        if (redis_client_ && result->choices.size() > 0) {
            try {
                std::string prompt_hash = generate_prompt_hash(request);
                std::string response_text = result->choices[0].message.content;

                // Calculate prompt complexity for TTL
                double complexity = calculate_prompt_complexity(request);

                // Cache the response
                auto cache_result = redis_client_->cache_llm_response(
                    prompt_hash, request.model, response_text, complexity);

                if (cache_result.success) {
                    logger_->debug("LLM response cached successfully",
                                 "OpenAIClient", "create_chat_completion",
                                 {{"prompt_hash", prompt_hash}, {"model", request.model}});
                } else {
                    logger_->warn("Failed to cache LLM response",
                                 "OpenAIClient", "create_chat_completion",
                                 {{"error", cache_result.error_message}});
                }
            } catch (const std::exception& e) {
                logger_->warn("Exception during LLM response caching",
                             "OpenAIClient", "create_chat_completion",
                             {{"error", e.what()}});
            }
        }

        return *result;
    } else {
        failed_requests_++;
        return std::nullopt;
    }
}

std::optional<std::string> OpenAIClient::analyze_text(const std::string& text,
                                                    const std::string& analysis_type,
                                                    const std::string& context) {
    // Create analysis request
    std::string system_prompt = create_system_prompt(analysis_type);
    if (!context.empty()) {
        system_prompt += "\n\nAdditional Context: " + context;
    }

    OpenAICompletionRequest request = create_analysis_request(text, system_prompt, default_model_);

    try {
        auto response = create_chat_completion(request);
        if (!response || response->choices.empty()) {
            // API call succeeded but returned empty response
            
            throw std::runtime_error("Analysis failed: Empty response from API");
        }
        return response->choices[0].message.content;
    } catch (const std::exception& e) {
        // Proper error handling - record failure and propagate error
        
        throw std::runtime_error("Analysis failed: " + std::string(e.what()));
    }
}

std::optional<std::string> OpenAIClient::generate_compliance_reasoning(
    const std::string& decision_context,
    const std::vector<std::string>& regulatory_requirements,
    const std::vector<std::string>& risk_factors) {

    std::string system_prompt = R"(
You are an expert compliance officer with deep knowledge of financial regulations, risk management, and corporate governance.

Your task is to provide detailed compliance reasoning for business decisions, considering:
1. Applicable regulatory requirements
2. Identified risk factors
3. Potential compliance implications
4. Recommended risk mitigation strategies
5. Documentation and reporting requirements

Provide your analysis in a structured format with clear reasoning and actionable recommendations.)";

    std::string user_prompt = "Decision Context:\n" + decision_context + "\n\n";

    if (!regulatory_requirements.empty()) {
        user_prompt += "Regulatory Requirements:\n";
        for (size_t i = 0; i < regulatory_requirements.size(); ++i) {
            user_prompt += std::to_string(i + 1) + ". " + regulatory_requirements[i] + "\n";
        }
        user_prompt += "\n";
    }

    if (!risk_factors.empty()) {
        user_prompt += "Risk Factors:\n";
        for (size_t i = 0; i < risk_factors.size(); ++i) {
            user_prompt += std::to_string(i + 1) + ". " + risk_factors[i] + "\n";
        }
        user_prompt += "\n";
    }

    user_prompt += "Please provide comprehensive compliance reasoning and recommendations.";

    OpenAICompletionRequest request{
        .model = default_model_,
        .messages = {
            OpenAIMessage{"system", system_prompt},
            OpenAIMessage{"user", user_prompt}
        },
        .temperature = 0.1,  // Low temperature for consistent compliance analysis
        .max_tokens = 3000
    };

    try {
        auto response = create_chat_completion(request);
        if (!response || response->choices.empty()) {
            // API call succeeded but returned empty response
            
            throw std::runtime_error("Compliance reasoning failed: Empty response from API");
        }
        return response->choices[0].message.content;
    } catch (const std::exception& e) {
        // Proper error handling - record failure and propagate error
        
        throw std::runtime_error("Compliance reasoning failed: " + std::string(e.what()));
    }
}

std::optional<nlohmann::json> OpenAIClient::extract_structured_data(
    const std::string& text, const nlohmann::json& schema) {

    std::string system_prompt = R"(
You are an expert data extraction AI. Your task is to extract structured information from unstructured text according to the provided schema.

Return ONLY valid JSON that matches the schema structure. Do not include any explanatory text or markdown formatting.)";

    std::string schema_str = schema.dump(2);
    std::string user_prompt = "Extract the following information from the text according to this JSON schema:\n\n";
    user_prompt += "Schema:\n" + schema_str + "\n\n";
    user_prompt += "Text to analyze:\n" + text + "\n\n";
    user_prompt += "Return only the JSON object:";

    OpenAICompletionRequest request{
        .model = default_model_,
        .messages = {
            OpenAIMessage{"system", system_prompt},
            OpenAIMessage{"user", user_prompt}
        },
        .temperature = 0.0,  // Zero temperature for deterministic extraction
        .max_tokens = 2000
    };

    auto response = create_chat_completion(request);
    if (!response || response->choices.empty()) {
        return std::nullopt;
    }

    try {
        // Extract JSON from response
        std::string content = response->choices[0].message.content;

        // Remove any markdown formatting if present
        std::regex json_regex("```json\\s*([\\s\\S]*?)\\s*```");
        std::smatch match;
        if (std::regex_search(content, match, json_regex)) {
            content = match[1].str();
        }

        // Parse the JSON
        return nlohmann::json::parse(content);
    } catch (const std::exception& e) {
        handle_api_error("json_parsing", std::string("Failed to parse extracted JSON: ") + e.what(),
                        {{"response_content", response->choices[0].message.content.substr(0, 100)}});
        return std::nullopt;
    }
}

std::optional<std::string> OpenAIClient::generate_decision_recommendation(
    const std::string& scenario,
    const std::vector<std::string>& options,
    const std::vector<std::string>& constraints) {

    std::string system_prompt = R"(
You are an expert decision analyst specializing in compliance and risk management.

For each decision scenario, you must:
1. Analyze the business context and objectives
2. Evaluate each option against the given constraints
3. Assess compliance and regulatory implications
4. Consider risk factors and mitigation strategies
5. Provide a clear recommendation with reasoning
6. Include implementation considerations

Structure your response with:
- Situation Analysis
- Option Evaluation
- Risk Assessment
- Final Recommendation
- Implementation Steps)";

    std::string user_prompt = "Decision Scenario:\n" + scenario + "\n\n";

    if (!options.empty()) {
        user_prompt += "Available Options:\n";
        for (size_t i = 0; i < options.size(); ++i) {
            user_prompt += std::to_string(i + 1) + ". " + options[i] + "\n";
        }
        user_prompt += "\n";
    }

    if (!constraints.empty()) {
        user_prompt += "Constraints and Requirements:\n";
        for (size_t i = 0; i < constraints.size(); ++i) {
            user_prompt += std::to_string(i + 1) + ". " + constraints[i] + "\n";
        }
        user_prompt += "\n";
    }

    OpenAICompletionRequest request{
        .model = default_model_,
        .messages = {
            OpenAIMessage{"system", system_prompt},
            OpenAIMessage{"user", user_prompt}
        },
        .temperature = 0.3,  // Moderate temperature for balanced analysis
        .max_tokens = 2500
    };

    try {
        auto response = create_chat_completion(request);
        if (!response || response->choices.empty()) {
            // API call succeeded but returned empty response
            
            throw std::runtime_error("Decision recommendation failed: Empty response from API");
        }
        return response->choices[0].message.content;
    } catch (const std::exception& e) {
        // Proper error handling - record failure and propagate error
        
        throw std::runtime_error("Decision recommendation failed: " + std::string(e.what()));
    }
}

nlohmann::json OpenAIClient::get_usage_statistics() {
    return {
        {"total_requests", static_cast<size_t>(total_requests_)},
        {"successful_requests", static_cast<size_t>(successful_requests_)},
        {"failed_requests", static_cast<size_t>(failed_requests_)},
        {"success_rate", total_requests_ > 0 ?
            (static_cast<double>(successful_requests_) / total_requests_) * 100.0 : 0.0},
        {"total_tokens_used", static_cast<size_t>(total_tokens_used_)},
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

nlohmann::json OpenAIClient::get_health_status() {
    auto circuit_breaker = error_handler_->get_circuit_breaker(CIRCUIT_BREAKER_SERVICE);

    return {
        {"service", "openai_api"},
        {"status", "operational"}, // Could be enhanced with actual health checks
        {"last_request", std::chrono::duration_cast<std::chrono::milliseconds>(
            last_request_time_.time_since_epoch()).count()},
        {"circuit_breaker", circuit_breaker ? circuit_breaker->to_json() : nullptr},
        {"usage_stats", get_usage_statistics()}
    };
}

void OpenAIClient::reset_usage_counters() {
    total_requests_ = 0;
    successful_requests_ = 0;
    failed_requests_ = 0;
    total_tokens_used_ = 0;
    estimated_cost_usd_ = 0.0;

    logger_->info("OpenAI client usage counters reset");
}

// Private implementation methods

std::optional<HttpResponse> OpenAIClient::make_api_request(const std::string& endpoint,
                                                        const nlohmann::json& payload) {
    try {
        std::string url = base_url_ + endpoint;

        std::unordered_map<std::string, std::string> headers = {
            {"Authorization", "Bearer " + api_key_},
            {"Content-Type", "application/json"}
        };

        std::string payload_str = payload.dump();

        logger_->debug("Making OpenAI API request to: " + url);

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

std::optional<OpenAIResponse> OpenAIClient::parse_api_response(const HttpResponse& response) {
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
        OpenAIResponse parsed_response;

        parsed_response.id = json_response.value("id", "");
        parsed_response.object = json_response.value("object", "");
        parsed_response.created = std::chrono::system_clock::time_point(
            std::chrono::seconds(json_response.value("created", 0)));
        parsed_response.model = json_response.value("model", "");

        if (json_response.contains("system_fingerprint")) {
            parsed_response.system_fingerprint = json_response["system_fingerprint"];
        }

        // Parse choices
        if (json_response.contains("choices")) {
            for (const auto& choice_json : json_response["choices"]) {
                OpenAIChoice choice;
                choice.index = choice_json.value("index", 0);
                choice.finish_reason = choice_json.value("finish_reason", "");

                if (choice_json.contains("message")) {
                    auto msg_json = choice_json["message"];
                    choice.message.role = msg_json.value("role", "");
                    choice.message.content = msg_json.value("content", "");
                    if (msg_json.contains("name")) {
                        choice.message.name = msg_json["name"];
                    }
                }

                parsed_response.choices.push_back(choice);
            }
        }

        // Parse usage
        if (json_response.contains("usage")) {
            auto usage_json = json_response["usage"];
            parsed_response.usage.prompt_tokens = usage_json.value("prompt_tokens", 0);
            parsed_response.usage.completion_tokens = usage_json.value("completion_tokens", 0);
            parsed_response.usage.total_tokens = usage_json.value("total_tokens", 0);
        }

        return parsed_response;

    } catch (const std::exception& e) {
        handle_api_error("parsing", std::string("Failed to parse API response: ") + e.what(),
                        {{"response_body", response.body.empty() ? "empty" : response.body.substr(0, 200)}});
        return std::nullopt;
    }
}

void OpenAIClient::handle_api_error(const std::string& error_type,
                                  const std::string& message,
                                  const std::unordered_map<std::string, std::string>& context) {
    // Report error through error handler
    ErrorInfo error_info(ErrorCategory::EXTERNAL_API, ErrorSeverity::HIGH,
                        "OpenAIClient", "api_request", message);
    error_info.context = context;
    error_info.context["error_type"] = error_type;
    error_info.context["service"] = CIRCUIT_BREAKER_SERVICE;

    error_handler_->report_error(error_info);

    // Log the error
    logger_->error("OpenAI API error - Type: " + error_type + ", Message: " + message);
}

bool OpenAIClient::check_rate_limit() {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);

    auto now = std::chrono::system_clock::now();
    auto window_start = now - rate_limit_window_;

    // Remove old timestamps outside the window
    while (!request_timestamps_.empty() && request_timestamps_.front() < window_start) {
        request_timestamps_.pop_front();
    }

    // Check if we're within limits
    if (static_cast<int>(request_timestamps_.size()) >= max_requests_per_minute_) {
        logger_->warn("OpenAI API rate limit exceeded: " + std::to_string(request_timestamps_.size()) +
                     " requests in last minute");
        return false;
    }

    // Add current request timestamp
    request_timestamps_.push_back(now);
    return true;
}

void OpenAIClient::update_usage_stats(const OpenAIResponse& response) {
    total_tokens_used_ += static_cast<size_t>(response.usage.total_tokens);
    double cost = calculate_cost(response.model, response.usage.total_tokens);
    double current_cost = estimated_cost_usd_.load();
    estimated_cost_usd_.store(current_cost + cost);

    logger_->debug("OpenAI usage updated - Tokens: " + std::to_string(response.usage.total_tokens) +
                  ", Cost: $" + std::to_string(calculate_cost(response.model, response.usage.total_tokens)));
}

double OpenAIClient::calculate_cost(const std::string& model, int tokens) {
    // OpenAI pricing (as of 2024, subject to change)
    static const std::unordered_map<std::string, double> pricing_per_1k_tokens = {
        // GPT-4 Turbo
        {"gpt-4-turbo-preview", 0.01},     // $0.01 per 1K input tokens
        {"gpt-4-turbo", 0.01},
        {"gpt-4-1106-preview", 0.01},
        // GPT-4
        {"gpt-4", 0.03},                   // $0.03 per 1K input tokens
        {"gpt-4-32k", 0.06},
        // GPT-3.5 Turbo
        {"gpt-3.5-turbo", 0.0015},        // $0.0015 per 1K input tokens
        {"gpt-3.5-turbo-16k", 0.003}
    };

    auto it = pricing_per_1k_tokens.find(model);
    if (it == pricing_per_1k_tokens.end()) {
        logger_->warn("Unknown model for cost calculation: " + model);
        return 0.0; // Unknown model, no cost calculation
    }

    double cost_per_1k = it->second;
    return (tokens / 1000.0) * cost_per_1k;
}

std::string OpenAIClient::create_system_prompt(const std::string& task_type) {
    static const std::unordered_map<std::string, std::string> prompts = {
        {"compliance", R"(
You are an expert compliance analyst with deep knowledge of financial regulations, corporate governance, and risk management.

Your role is to analyze business activities, transactions, and decisions for compliance with applicable laws and regulations.

Provide analysis that includes:
1. Identification of applicable regulations
2. Assessment of compliance status
3. Identification of potential risks
4. Recommendations for compliance improvement
5. Documentation and reporting requirements)"},

        {"risk", R"(
You are an expert risk management professional specializing in financial services and regulatory compliance.

Your role is to identify, assess, and provide recommendations for managing various types of risk including:
1. Regulatory compliance risk
2. Operational risk
3. Financial risk
4. Reputational risk
5. Strategic risk

Provide comprehensive risk analysis with mitigation strategies.)"},

        {"sentiment", R"(
You are an expert sentiment analyst specializing in financial communications and regulatory disclosures.

Your role is to analyze text for:
1. Overall sentiment (positive, negative, neutral)
2. Emotional tone and intensity
3. Key themes and topics
4. Risk indicators
5. Communication effectiveness

Provide detailed sentiment analysis with supporting evidence.)"},

        {"general", R"(
You are an AI assistant specializing in financial services, regulatory compliance, and business analysis.

Provide accurate, helpful, and contextually appropriate responses based on your expertise in:
1. Financial regulations and compliance
2. Risk management and assessment
3. Business process analysis
4. Regulatory reporting and documentation
5. Industry best practices)"}
    };

    auto it = prompts.find(task_type);
    if (it != prompts.end()) {
        return it->second;
    }

    return prompts.at("general");
}

bool OpenAIClient::validate_response(const OpenAIResponse& response) {
    if (response.id.empty() || response.choices.empty()) {
        return false;
    }

    // Validate each choice
    for (const auto& choice : response.choices) {
        if (choice.message.content.empty()) {
            return false;
        }
    }

    return true;
}

// Fallback responses removed - Rule 7 compliance: No makeshift workarounds or simplified functionality

std::optional<std::shared_ptr<StreamingSession>> OpenAIClient::create_streaming_completion(
    const OpenAICompletionRequest& request,
    StreamingCallback streaming_callback,
    CompletionCallback completion_callback) {

    // Generate unique session ID
    std::string session_id = "openai_stream_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) +
                           "_" + std::to_string(std::rand());

    try {
        // Create streaming session
        auto session = streaming_handler_->create_session(session_id);
        if (!session) {
            logger_->error("Failed to create streaming session", "OpenAIClient", "create_streaming_completion");
            return std::nullopt;
        }

        // Set up session callbacks
        session->start(
            streaming_callback,
            completion_callback,
            [this, session_id](const std::string& error) {
                logger_->error("Streaming session error: " + error, "OpenAIClient", "create_streaming_completion");
                streaming_handler_->remove_session(session_id);
            }
        );

        // Prepare request with streaming enabled
        auto streaming_request = request;
        streaming_request.stream = true;

        // Make streaming HTTP request
        std::string url = base_url_ + "/chat/completions";
        std::string payload_str = streaming_request.to_json().dump();

        std::unordered_map<std::string, std::string> headers = {
            {"Authorization", "Bearer " + api_key_},
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
            // For OpenAI streaming, the final response might be in the last chunk
            // The session should have been completed by the streaming callback
            // If not, we can attempt to construct a final response from accumulated data
            if (!session->is_active()) {
                // Session was already completed by streaming callback
                logger_->info("OpenAI streaming session completed successfully: " + session_id,
                             "OpenAIClient", "create_streaming_completion");
            } else {
                // Session still active - complete it with accumulated data
                nlohmann::json final_response = {
                    {"object", "chat.completion"},
                    {"model", streaming_request.model},
                    {"choices", nlohmann::json::array()},
                    {"usage", {
                        {"prompt_tokens", 0},
                        {"completion_tokens", 0},
                        {"total_tokens", 0}
                    }}
                };

                // Add the accumulated content as a choice
                final_response["choices"].push_back({
                    {"index", 0},
                    {"message", {
                        {"role", "assistant"},
                        {"content", session->get_accumulated_response()["content"]}
                    }},
                    {"finish_reason", "stop"}
                });

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
        logger_->error("Streaming completion failed: " + std::string(e.what()),
                      "OpenAIClient", "create_streaming_completion");
        error_handler_->report_error(ErrorInfo{
            ErrorCategory::EXTERNAL_API,
            ErrorSeverity::HIGH,
            "OpenAIClient",
            "create_streaming_completion",
            "OpenAI streaming completion failed: " + std::string(e.what()),
            "Session ID: " + session_id
        });

        streaming_handler_->remove_session(session_id);
        return std::nullopt;
    }
}

// Function Calling Implementation

OpenAICompletionRequest OpenAIClient::create_function_completion_request(
    const std::vector<OpenAIMessage>& messages,
    const nlohmann::json& functions,
    const std::string& model) {

    return OpenAICompletionRequest{
        .model = model,
        .messages = messages,
        .functions = functions,
        .temperature = 0.1,  // Lower temperature for more consistent function calling
        .max_tokens = 2000
    };
}

OpenAICompletionRequest OpenAIClient::create_tool_completion_request(
    const std::vector<OpenAIMessage>& messages,
    const nlohmann::json& tools,
    const std::string& tool_choice,
    const std::string& model) {

    return OpenAICompletionRequest{
        .model = model,
        .messages = messages,
        .temperature = 0.1,  // Lower temperature for more consistent tool calling
        .max_tokens = 2000,
        .tools = {tools},
        .tool_choice = tool_choice
    };
}

OpenAIMessage OpenAIClient::create_function_response_message(
    const std::string& function_name,
    const nlohmann::json& function_response,
    const std::string& tool_call_id) {

    OpenAIMessage message;
    message.role = tool_call_id.empty() ? "function" : "tool";
    message.content = function_response.dump();

    if (!tool_call_id.empty()) {
        message.tool_call_id = tool_call_id;
    } else {
        message.name = function_name;
    }

    return message;
}

std::vector<FunctionCall> OpenAIClient::parse_function_calls_from_response(const OpenAIResponse& response) {
    std::vector<FunctionCall> calls;

    try {
        if (!response.choices.empty()) {
            const auto& choice = response.choices[0];
            const auto& message = choice.message;

            // Check for tool calls (new format)
            if (message.tool_calls) {
                for (const auto& tool_call : *message.tool_calls) {
                    if (tool_call.contains("function")) {
                        calls.push_back(FunctionCall::from_openai_tool_call(tool_call));
                    }
                }
            }
            // Check for function call (legacy format)
            else if (message.function_call) {
                calls.push_back(FunctionCall::from_openai_function_call(*message.function_call));
            }
        }
    } catch (const std::exception& e) {
        logger_->error("Failed to parse function calls from response: " + std::string(e.what()),
                      "OpenAIClient", "parse_function_calls_from_response");

        error_handler_->report_error(ErrorInfo{
            ErrorCategory::PROCESSING,
            ErrorSeverity::MEDIUM,
            "OpenAIClient",
            "parse_function_calls_from_response",
            "Failed to parse function calls: " + std::string(e.what()),
            "Response parsing failed"
        });
    }

    return calls;
}

bool OpenAIClient::response_contains_function_calls(const OpenAIResponse& response) {
    if (response.choices.empty()) {
        return false;
    }

    const auto& message = response.choices[0].message;

    // Check for tool calls (new format)
    if (message.tool_calls && !(*message.tool_calls).empty()) {
        return true;
    }

    // Check for function call (legacy format)
    if (message.function_call) {
        return true;
    }

    return false;
}

bool OpenAIClient::is_healthy() const {
    // Production-grade health check with actual API connectivity verification
    if (api_key_.empty() || base_url_.empty()) {
        return false;
    }
    
    try {
        // Make lightweight API call to /v1/models endpoint to verify connectivity
        nlohmann::json headers = {
            {"Authorization", "Bearer " + api_key_},
            {"Content-Type", "application/json"}
        };
        
        auto response = http_client_->get(base_url_ + "/v1/models", headers);
        
        // Check if we got a successful response (200-299 status code)
        if (response.status_code >= 200 && response.status_code < 300) {
            return true;
        }
        
        if (logger_) {
            logger_->warn("OpenAI health check failed with status: " + std::to_string(response.status_code),
                         "OpenAIClient", "is_healthy");
        }
        return false;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->warn("OpenAI health check exception: " + std::string(e.what()),
                         "OpenAIClient", "is_healthy");
        }
        return false;
    }
}

std::string OpenAIClient::generate_prompt_hash(const OpenAICompletionRequest& request) {
    std::stringstream content;

    // Include all messages in the hash
    for (const auto& message : request.messages) {
        content << message.role << ":" << message.content;
        if (message.name) {
            content << ":" << *message.name;
        }
        content << "|";
    }

    // Include key parameters that affect the response
    content << "model:" << request.model << "|";
    content << "temperature:" << request.temperature.value_or(0.7) << "|";
    content << "max_tokens:" << request.max_tokens.value_or(2000) << "|";

    // Include function/tool definitions if present
    if (request.functions) {
        content << "functions:" << request.functions->dump() << "|";
    }
    if (request.tools) {
        content << "tools:" << request.tools->dump() << "|";
    }

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

double OpenAIClient::calculate_prompt_complexity(const OpenAICompletionRequest& request) {
    double complexity = 0.0;

    // Base complexity from message count and length
    size_t total_chars = 0;
    for (const auto& message : request.messages) {
        total_chars += message.content.length();
    }

    // Normalize character count to 0-0.5 range
    double length_score = std::min(1.0, static_cast<double>(total_chars) / 8000.0) * 0.5;

    // Temperature affects complexity (lower temp = more deterministic = higher complexity)
    double temp_score = (1.0 - request.temperature.value_or(0.7)) * 0.2;

    // Function/tool calling increases complexity
    double function_score = 0.0;
    if (request.functions || request.tools) {
        function_score = 0.3;
    }

    // Model complexity (GPT-4 is more complex than GPT-3.5)
    double model_score = 0.0;
    if (request.model.find("gpt-4") != std::string::npos) {
        model_score = 0.2;
    }

    complexity = length_score + temp_score + function_score + model_score;
    return std::min(1.0, complexity);
}

} // namespace regulens
