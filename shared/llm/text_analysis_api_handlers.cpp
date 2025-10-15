/**
 * Text Analysis API Handlers Implementation
 * REST API endpoints for LLM-based text analysis
 */

#include "text_analysis_api_handlers.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace regulens {

TextAnalysisAPIHandlers::TextAnalysisAPIHandlers(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<TextAnalysisService> text_analysis_service
) : db_conn_(db_conn), text_analysis_service_(text_analysis_service), access_control_(db_conn) {

    if (!db_conn_) {
        throw std::runtime_error("Database connection is required for TextAnalysisAPIHandlers");
    }
    if (!text_analysis_service_) {
        throw std::runtime_error("TextAnalysisService is required for TextAnalysisAPIHandlers");
    }

    spdlog::info("TextAnalysisAPIHandlers initialized");
}

TextAnalysisAPIHandlers::~TextAnalysisAPIHandlers() {
    spdlog::info("TextAnalysisAPIHandlers shutting down");
}

std::string TextAnalysisAPIHandlers::handle_analyze_text(const std::string& request_body, const std::string& user_id) {
    try {
        // Parse and validate request
        nlohmann::json request = nlohmann::json::parse(request_body);
        std::string validation_error;
        if (!validate_analysis_request(request, validation_error)) {
            return create_error_response(validation_error).dump();
        }

        if (!validate_user_access(user_id, "analyze_text")) {
            return create_error_response("Access denied", 403).dump();
        }

        // Parse the analysis request
        TextAnalysisRequest analysis_request = parse_analysis_request(request);

        // Perform analysis
        auto start_time = std::chrono::high_resolution_clock::now();
        TextAnalysisResult result = text_analysis_service_->analyze_text(analysis_request);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        // Format response
        nlohmann::json response_data = format_analysis_result(result);
        response_data["total_processing_time_ms"] = processing_time.count();

        spdlog::info("Text analysis API request completed for user {}: {} tasks in {}ms",
                    user_id, analysis_request.tasks.size(), processing_time.count());

        return create_success_response(response_data, "Text analysis completed successfully").dump();

    } catch (const nlohmann::json::exception& e) {
        spdlog::error("JSON parsing error in handle_analyze_text: {}", e.what());
        return create_error_response("Invalid JSON format", 400).dump();
    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_analyze_text: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string TextAnalysisAPIHandlers::handle_batch_analyze_text(const std::string& request_body, const std::string& user_id) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);

        if (!request.contains("requests") || !request["requests"].is_array()) {
            return create_error_response("Missing or invalid 'requests' array").dump();
        }

        if (!validate_user_access(user_id, "batch_analyze_text")) {
            return create_error_response("Access denied", 403).dump();
        }

        // Parse batch requests
        std::vector<TextAnalysisRequest> batch_requests;
        for (const auto& req_json : request["requests"]) {
            std::string validation_error;
            if (!validate_analysis_request(req_json, validation_error)) {
                return create_error_response("Invalid request in batch: " + validation_error).dump();
            }
            batch_requests.push_back(parse_analysis_request(req_json));
        }

        if (batch_requests.empty()) {
            return create_error_response("No valid requests in batch").dump();
        }

        if (batch_requests.size() > 50) {
            return create_error_response("Batch size too large (maximum 50 requests)").dump();
        }

        // Perform batch analysis
        auto start_time = std::chrono::high_resolution_clock::now();
        std::vector<TextAnalysisResult> results = text_analysis_service_->analyze_batch(batch_requests);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        // Format batch response
        std::vector<nlohmann::json> formatted_results;
        for (const auto& result : results) {
            formatted_results.push_back(format_analysis_result(result));
        }

        nlohmann::json response_data = create_batch_response(
            formatted_results,
            batch_requests.size(),
            results.size()
        );
        response_data["total_processing_time_ms"] = processing_time.count();

        spdlog::info("Batch text analysis API request completed for user {}: {} requests in {}ms",
                    user_id, results.size(), processing_time.count());

        return create_success_response(response_data, "Batch text analysis completed successfully").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_batch_analyze_text: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string TextAnalysisAPIHandlers::handle_analyze_sentiment(const std::string& request_body, const std::string& user_id) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);

        if (!request.contains("text") || !request["text"].is_string()) {
            return create_error_response("Missing or invalid 'text' field").dump();
        }

        std::string validation_error;
        if (!validate_text_input(request["text"], validation_error)) {
            return create_error_response(validation_error).dump();
        }

        if (!validate_user_access(user_id, "analyze_sentiment")) {
            return create_error_response("Access denied", 403).dump();
        }

        std::string text = request["text"];
        SentimentResult result = text_analysis_service_->analyze_sentiment(text);

        nlohmann::json response_data = format_sentiment_result(result);
        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_analyze_sentiment: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string TextAnalysisAPIHandlers::handle_extract_entities(const std::string& request_body, const std::string& user_id) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);

        if (!request.contains("text") || !request["text"].is_string()) {
            return create_error_response("Missing or invalid 'text' field").dump();
        }

        std::string validation_error;
        if (!validate_text_input(request["text"], validation_error)) {
            return create_error_response(validation_error).dump();
        }

        if (!validate_user_access(user_id, "extract_entities")) {
            return create_error_response("Access denied", 403).dump();
        }

        std::string text = request["text"];
        std::vector<Entity> entities = text_analysis_service_->extract_entities(text);

        nlohmann::json response_data = format_entity_results(entities);
        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_extract_entities: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string TextAnalysisAPIHandlers::handle_summarize_text(const std::string& request_body, const std::string& user_id) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);

        if (!request.contains("text") || !request["text"].is_string()) {
            return create_error_response("Missing or invalid 'text' field").dump();
        }

        std::string validation_error;
        if (!validate_text_input(request["text"], validation_error)) {
            return create_error_response(validation_error).dump();
        }

        if (!validate_user_access(user_id, "summarize_text")) {
            return create_error_response("Access denied", 403).dump();
        }

        std::string text = request["text"];
        int max_length = request.value("max_length", 150);
        SummarizationResult result = text_analysis_service_->summarize_text(text, max_length);

        nlohmann::json response_data = format_summarization_result(result);
        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_summarize_text: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string TextAnalysisAPIHandlers::handle_classify_topics(const std::string& request_body, const std::string& user_id) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);

        if (!request.contains("text") || !request["text"].is_string()) {
            return create_error_response("Missing or invalid 'text' field").dump();
        }

        std::string validation_error;
        if (!validate_text_input(request["text"], validation_error)) {
            return create_error_response(validation_error).dump();
        }

        if (!validate_user_access(user_id, "classify_topics")) {
            return create_error_response("Access denied", 403).dump();
        }

        std::string text = request["text"];
        ClassificationResult result = text_analysis_service_->classify_topics(text);

        nlohmann::json response_data = format_classification_result(result);
        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_classify_topics: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string TextAnalysisAPIHandlers::handle_detect_language(const std::string& request_body, const std::string& user_id) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);

        if (!request.contains("text") || !request["text"].is_string()) {
            return create_error_response("Missing or invalid 'text' field").dump();
        }

        std::string text = request["text"];
        if (text.empty()) {
            return create_error_response("Text cannot be empty").dump();
        }

        if (!validate_user_access(user_id, "detect_language")) {
            return create_error_response("Access denied", 403).dump();
        }

        LanguageDetectionResult result = text_analysis_service_->detect_language(text);

        nlohmann::json response_data = format_language_result(result);
        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_detect_language: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string TextAnalysisAPIHandlers::handle_extract_keywords(const std::string& request_body, const std::string& user_id) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);

        if (!request.contains("text") || !request["text"].is_string()) {
            return create_error_response("Missing or invalid 'text' field").dump();
        }

        std::string validation_error;
        if (!validate_text_input(request["text"], validation_error)) {
            return create_error_response(validation_error).dump();
        }

        if (!validate_user_access(user_id, "extract_keywords")) {
            return create_error_response("Access denied", 403).dump();
        }

        std::string text = request["text"];
        int max_keywords = request.value("max_keywords", 10);
        std::vector<std::string> keywords = text_analysis_service_->extract_keywords(text, max_keywords);

        nlohmann::json response_data = format_keyword_results(keywords);
        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_extract_keywords: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string TextAnalysisAPIHandlers::handle_get_analysis_stats() {
    try {
        auto stats = text_analysis_service_->get_cache_stats();

        nlohmann::json response_data = {
            {"cache_stats", stats},
            {"supported_tasks", get_supported_tasks_list()},
            {"supported_languages", get_supported_languages_list()},
            {"service_status", "operational"}
        };

        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_analysis_stats: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string TextAnalysisAPIHandlers::handle_clear_analysis_cache(const std::string& text_hash) {
    try {
        bool success = text_analysis_service_->clear_cache(text_hash);

        nlohmann::json response_data = {
            {"cache_cleared", success},
            {"text_hash", text_hash.empty() ? "all" : text_hash}
        };

        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_clear_analysis_cache: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string TextAnalysisAPIHandlers::handle_get_analysis_config() {
    nlohmann::json config = {
        {"supported_tasks", {
            "sentiment_analysis",
            "entity_extraction",
            "text_summarization",
            "topic_classification",
            "language_detection",
            "keyword_extraction"
        }},
        {"max_text_length", 10000},
        {"max_batch_size", 50},
        {"default_model", "gpt-4-turbo-preview"},
        {"cache_enabled", true},
        {"confidence_threshold", 0.5}
    };

    return create_success_response(config).dump();
}

std::string TextAnalysisAPIHandlers::handle_update_analysis_config(const std::string& request_body) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);
        
        // Validate required fields
        if (!request.contains("config_key")) {
            return create_error_response("Missing required field: config_key", 400).dump();
        }
        
        std::string config_key = request["config_key"];
        
        // Update configuration based on key
        if (config_key == "default_model" && request.contains("value")) {
            text_analysis_service_->set_default_model(request["value"]);
        } else if (config_key == "cache_enabled" && request.contains("value")) {
            text_analysis_service_->set_cache_enabled(request["value"].get<bool>());
        } else if (config_key == "cache_ttl_hours" && request.contains("value")) {
            text_analysis_service_->set_cache_ttl_hours(request["value"].get<int>());
        } else if (config_key == "batch_size" && request.contains("value")) {
            text_analysis_service_->set_batch_size(request["value"].get<int>());
        } else if (config_key == "confidence_threshold" && request.contains("value")) {
            text_analysis_service_->set_confidence_threshold(request["value"].get<double>());
        } else {
            return create_error_response("Unknown configuration key: " + config_key, 400).dump();
        }
        
        // Store configuration in database for persistence
        std::string store_query = R"(
            INSERT INTO text_analysis_config (config_key, config_value, updated_at)
            VALUES ($1, $2, NOW())
            ON CONFLICT (config_key) DO UPDATE
            SET config_value = $2, updated_at = NOW()
        )";
        
        bool persisted = db_conn_->execute_command(store_query, {
            config_key,
            request["value"].dump()
        });

        if (!persisted) {
            return create_error_response("Failed to persist configuration change", 500).dump();
        }
        
        nlohmann::json response = {
            {"success", true},
            {"message", "Configuration updated successfully"},
            {"config_key", config_key},
            {"new_value", request["value"]}
        };
        
        return create_success_response(response).dump();
        
    } catch (const nlohmann::json::parse_error& e) {
        return create_error_response("Invalid JSON: " + std::string(e.what()), 400).dump();
    } catch (const std::exception& e) {
        return create_error_response("Configuration update failed: " + std::string(e.what()), 500).dump();
    }
}

// Helper method implementations

TextAnalysisRequest TextAnalysisAPIHandlers::parse_analysis_request(const nlohmann::json& request) {
    TextAnalysisRequest analysis_request;

    analysis_request.text = request.value("text", "");
    analysis_request.source = request.value("source", "api");
    analysis_request.language_hint = request.contains("language_hint") ?
                                   std::make_optional(request["language_hint"].get<std::string>()) :
                                   std::nullopt;
    analysis_request.domain_context = request.contains("domain_context") ?
                                    std::make_optional(request["domain_context"].get<std::string>()) :
                                    std::nullopt;
    analysis_request.enable_caching = request.value("enable_caching", true);
    analysis_request.max_keywords = request.value("max_keywords", 10);
    analysis_request.confidence_threshold = request.value("confidence_threshold", 0.5);

    // Parse tasks
    if (request.contains("tasks")) {
        if (request["tasks"].is_array()) {
            analysis_request.tasks = parse_task_list(request["tasks"]);
        } else if (request["tasks"].is_string()) {
            // Single task
            analysis_request.tasks = {parse_single_task(request["tasks"])};
        }
    } else {
        // Default to all tasks
        analysis_request.tasks = {
            AnalysisTask::SENTIMENT_ANALYSIS,
            AnalysisTask::ENTITY_EXTRACTION,
            AnalysisTask::KEYWORD_EXTRACTION
        };
    }

    return analysis_request;
}

nlohmann::json TextAnalysisAPIHandlers::format_analysis_result(const TextAnalysisResult& result) {
    nlohmann::json response = {
        {"request_id", result.request_id},
        {"text_hash", result.text_hash},
        {"analyzed_at", std::chrono::duration_cast<std::chrono::seconds>(
            result.analyzed_at.time_since_epoch()).count()},
        {"processing_time_ms", result.processing_time.count()},
        {"success", result.success},
        {"total_tokens", result.total_tokens},
        {"total_cost", result.total_cost},
        {"task_confidences", result.task_confidences}
    };

    if (!result.success && result.error_message) {
        response["error"] = *result.error_message;
    }

    if (result.sentiment) {
        response["sentiment"] = format_sentiment_result(*result.sentiment);
    }

    if (!result.entities.empty()) {
        response["entities"] = format_entity_results(result.entities);
    }

    if (result.summary) {
        response["summary"] = format_summarization_result(*result.summary);
    }

    if (result.classification) {
        response["classification"] = format_classification_result(*result.classification);
    }

    if (result.language) {
        response["language"] = format_language_result(*result.language);
    }

    if (!result.keywords.empty()) {
        response["keywords"] = format_keyword_results(result.keywords);
    }

    return response;
}

nlohmann::json TextAnalysisAPIHandlers::format_sentiment_result(const SentimentResult& result) {
    return {
        {"label", result.label},
        {"confidence", result.confidence},
        {"scores", result.scores}
    };
}

nlohmann::json TextAnalysisAPIHandlers::format_entity_results(const std::vector<Entity>& entities) {
    nlohmann::json entity_array = nlohmann::json::array();

    for (const auto& entity : entities) {
        nlohmann::json entity_json = {
            {"text", entity.text},
            {"type", entity.type},
            {"confidence", entity.confidence},
            {"start_pos", entity.start_pos},
            {"end_pos", entity.end_pos}
        };

        if (entity.category != entity.type) {
            entity_json["category"] = entity.category;
        }

        entity_array.push_back(entity_json);
    }

    return entity_array;
}

nlohmann::json TextAnalysisAPIHandlers::format_summarization_result(const SummarizationResult& result) {
    return {
        {"summary", result.summary},
        {"compression_ratio", result.compression_ratio},
        {"original_length", result.original_length},
        {"summary_length", result.summary_length},
        {"method_used", result.method_used}
    };
}

nlohmann::json TextAnalysisAPIHandlers::format_classification_result(const ClassificationResult& result) {
    return {
        {"primary_topic", result.primary_topic},
        {"topic_scores", result.topic_scores},
        {"keywords", result.keywords},
        {"categories", result.categories}
    };
}

nlohmann::json TextAnalysisAPIHandlers::format_language_result(const LanguageDetectionResult& result) {
    return {
        {"language_code", result.language_code},
        {"language_name", result.language_name},
        {"confidence", result.confidence},
        {"alternatives", result.alternatives}
    };
}

nlohmann::json TextAnalysisAPIHandlers::format_keyword_results(const std::vector<std::string>& keywords) {
    return keywords;
}

bool TextAnalysisAPIHandlers::validate_analysis_request(const nlohmann::json& request, std::string& error_message) {
    if (!request.contains("text") || !request["text"].is_string()) {
        error_message = "Missing or invalid 'text' field";
        return false;
    }

    return validate_text_input(request["text"], error_message);
}

bool TextAnalysisAPIHandlers::validate_text_input(const std::string& text, std::string& error_message) {
    if (text.empty()) {
        error_message = "Text cannot be empty";
        return false;
    }

    if (text.length() > 10000) {
        error_message = "Text too long (maximum 10000 characters)";
        return false;
    }

    return true;
}

bool TextAnalysisAPIHandlers::validate_user_access(const std::string& user_id, const std::string& operation) {
    if (user_id.empty() || operation.empty()) {
        return false;
    }

    if (access_control_.is_admin(user_id)) {
        return true;
    }

    std::vector<AccessControlService::PermissionQuery> queries = {
        {operation, "text_analysis", "", 0},
        {operation, "llm_analysis", "", 0},
        {"use_text_analysis", "", "", 0},
        {operation, "", "", 0}
    };

    if (operation.find("batch") != std::string::npos) {
        queries.push_back({"batch_text_analysis", "text_analysis", "", 0});
    }
    if (operation.find("stats") != std::string::npos) {
        queries.push_back({"view_text_analysis_metrics", "", "", 0});
    }

    return access_control_.has_any_permission(user_id, queries);
}

std::vector<AnalysisTask> TextAnalysisAPIHandlers::parse_task_list(const nlohmann::json& tasks_json) {
    std::vector<AnalysisTask> tasks;

    for (const auto& task_json : tasks_json) {
        if (task_json.is_string()) {
            AnalysisTask task = parse_single_task(task_json);
            if (is_task_supported(task)) {
                tasks.push_back(task);
            }
        }
    }

    // Ensure at least one valid task
    if (tasks.empty()) {
        tasks = {AnalysisTask::SENTIMENT_ANALYSIS};
    }

    return tasks;
}

AnalysisTask TextAnalysisAPIHandlers::parse_single_task(const std::string& task_str) {
    if (task_str == "sentiment_analysis" || task_str == "sentiment") {
        return AnalysisTask::SENTIMENT_ANALYSIS;
    } else if (task_str == "entity_extraction" || task_str == "entities") {
        return AnalysisTask::ENTITY_EXTRACTION;
    } else if (task_str == "text_summarization" || task_str == "summarization") {
        return AnalysisTask::TEXT_SUMMARIZATION;
    } else if (task_str == "topic_classification" || task_str == "classification") {
        return AnalysisTask::TOPIC_CLASSIFICATION;
    } else if (task_str == "language_detection" || task_str == "language") {
        return AnalysisTask::LANGUAGE_DETECTION;
    } else if (task_str == "keyword_extraction" || task_str == "keywords") {
        return AnalysisTask::KEYWORD_EXTRACTION;
    }

    return AnalysisTask::SENTIMENT_ANALYSIS; // Default fallback
}

nlohmann::json TextAnalysisAPIHandlers::create_success_response(const nlohmann::json& data, const std::string& message) {
    nlohmann::json response = {
        {"success", true},
        {"status_code", 200}
    };

    if (!message.empty()) {
        response["message"] = message;
    }

    if (data.is_object() || data.is_array()) {
        response["data"] = data;
    }

    return response;
}

nlohmann::json TextAnalysisAPIHandlers::create_error_response(const std::string& message, int status_code) {
    return {
        {"success", false},
        {"status_code", status_code},
        {"error", message}
    };
}

nlohmann::json TextAnalysisAPIHandlers::create_batch_response(
    const std::vector<nlohmann::json>& results,
    int total_count,
    int processed_count
) {
    return {
        {"results", results},
        {"total_requested", total_count},
        {"total_processed", processed_count},
        {"success_count", std::count_if(results.begin(), results.end(),
                                       [](const nlohmann::json& r) {
                                           return r.value("success", false);
                                       })}
    };
}

std::string TextAnalysisAPIHandlers::get_supported_tasks_list() {
    return "sentiment_analysis, entity_extraction, text_summarization, topic_classification, language_detection, keyword_extraction";
}

std::string TextAnalysisAPIHandlers::get_supported_languages_list() {
    return "en (English), es (Spanish), fr (French), de (German), it (Italian), pt (Portuguese), zh (Chinese), ja (Japanese), ko (Korean), ar (Arabic), ru (Russian)";
}

bool TextAnalysisAPIHandlers::is_task_supported(AnalysisTask task) {
    switch (task) {
        case AnalysisTask::SENTIMENT_ANALYSIS:
        case AnalysisTask::ENTITY_EXTRACTION:
        case AnalysisTask::TEXT_SUMMARIZATION:
        case AnalysisTask::TOPIC_CLASSIFICATION:
        case AnalysisTask::LANGUAGE_DETECTION:
        case AnalysisTask::KEYWORD_EXTRACTION:
            return true;
        default:
            return false;
    }
}

} // namespace regulens
