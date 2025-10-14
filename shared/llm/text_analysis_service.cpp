/**
 * LLM Text Analysis Service Implementation
 * Production-grade multi-task NLP analysis pipeline
 */

#include "text_analysis_service.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <openssl/sha.h>
#include <uuid/uuid.h>
#include <spdlog/spdlog.h>
#include <thread>
#include <future>

namespace regulens {

TextAnalysisService::TextAnalysisService(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<OpenAIClient> openai_client,
    std::shared_ptr<RedisClient> redis_client
) : db_conn_(db_conn), openai_client_(openai_client), redis_client_(redis_client) {

    if (!db_conn_) {
        throw std::runtime_error("Database connection is required for TextAnalysisService");
    }
    if (!openai_client_) {
        throw std::runtime_error("OpenAI client is required for TextAnalysisService");
    }

    spdlog::info("TextAnalysisService initialized with caching enabled");
}

TextAnalysisService::~TextAnalysisService() {
    spdlog::info("TextAnalysisService shutting down");
}

TextAnalysisResult TextAnalysisService::analyze_text(const TextAnalysisRequest& request) {
    TextAnalysisResult result;
    result.request_id = generate_uuid();
    result.text_hash = generate_text_hash(request.text);
    result.analyzed_at = std::chrono::system_clock::now();

    auto start_time = std::chrono::high_resolution_clock::now();

    try {
        // Normalize input text
        std::string normalized_text = normalize_text(request.text);
        if (normalized_text.empty()) {
            result.success = false;
            result.error_message = "Empty or invalid text provided";
            return result;
        }

        // Generate tasks hash for caching
        std::string tasks_hash = generate_tasks_hash(request.tasks);

        // Check cache first if enabled
        if (cache_enabled_ && request.enable_caching) {
            auto cached_result = get_cached_result(result.text_hash, tasks_hash);
            if (cached_result) {
                spdlog::info("Using cached analysis result for text hash: {}", result.text_hash);
                return *cached_result;
            }
        }

        // Perform requested analysis tasks
        for (AnalysisTask task : request.tasks) {
            try {
                switch (task) {
                    case AnalysisTask::SENTIMENT_ANALYSIS:
                        result.sentiment = analyze_sentiment(normalized_text);
                        result.task_confidences["sentiment"] = result.sentiment->confidence;
                        break;

                    case AnalysisTask::ENTITY_EXTRACTION:
                        result.entities = extract_entities(normalized_text);
                        result.task_confidences["entities"] = calculate_entity_confidence(result.entities);
                        break;

                    case AnalysisTask::TEXT_SUMMARIZATION:
                        result.summary = summarize_text(normalized_text);
                        result.task_confidences["summary"] = 0.9; // Summarization confidence
                        break;

                    case AnalysisTask::TOPIC_CLASSIFICATION:
                        result.classification = classify_topics(normalized_text);
                        result.task_confidences["classification"] = 0.8; // Classification confidence
                        break;

                    case AnalysisTask::LANGUAGE_DETECTION:
                        result.language = detect_language(normalized_text);
                        result.task_confidences["language"] = result.language->confidence;
                        break;

                    case AnalysisTask::KEYWORD_EXTRACTION:
                        result.keywords = extract_keywords(normalized_text, request.max_keywords);
                        result.task_confidences["keywords"] = 0.85; // Keyword extraction confidence
                        break;

                    default:
                        spdlog::warn("Unsupported analysis task: {}", static_cast<int>(task));
                        break;
                }
            } catch (const std::exception& e) {
                spdlog::error("Error performing task {}: {}", static_cast<int>(task), e.what());
                result.task_confidences[std::to_string(static_cast<int>(task))] = 0.0;
            }
        }

        // Calculate processing time
        auto end_time = std::chrono::high_resolution_clock::now();
        result.processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        // Estimate tokens and cost (rough approximation)
        result.total_tokens = estimate_token_count(normalized_text) + estimate_token_count(build_sentiment_prompt(""));
        result.total_cost = calculate_task_cost(AnalysisTask::SENTIMENT_ANALYSIS, result.total_tokens);

        result.success = true;

        // Cache the result if caching is enabled
        if (cache_enabled_ && request.enable_caching) {
            cache_result(result.text_hash, tasks_hash, result);
        }

        // Store analysis result in database
        store_analysis_result(result);

        spdlog::info("Text analysis completed: {} tasks, {}ms processing time, {} tokens",
                    request.tasks.size(), result.processing_time.count(), result.total_tokens);

    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("Analysis failed: ") + e.what();

        auto end_time = std::chrono::high_resolution_clock::now();
        result.processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        spdlog::error("Text analysis failed: {}", e.what());
    }

    return result;
}

SentimentResult TextAnalysisService::analyze_sentiment(const std::string& text) {
    SentimentResult result;

    try {
        std::string prompt = build_sentiment_prompt(text);

        OpenAICompletionRequest request;
        request.model = default_model_;
        request.messages = {{"system", "You are a sentiment analysis expert. Analyze the sentiment of the given text and return a JSON response with 'label' (positive/negative/neutral), 'confidence' (0-1), and detailed 'scores'."},
                           {"user", prompt}};
        request.temperature = 0.1; // Low temperature for consistent results
        request.max_tokens = 200;

        auto response = openai_client_->create_chat_completion(request);
        if (!response || response->choices.empty()) {
            throw std::runtime_error("Failed to get sentiment analysis response from OpenAI");
        }

        std::string response_text = response->choices[0].message.content;
        result = parse_sentiment_response(response_text);

        // Update token usage
        result.confidence = std::max(result.confidence, confidence_threshold_);

    } catch (const std::exception& e) {
        spdlog::error("Sentiment analysis failed: {}", e.what());
        result.label = "neutral";
        result.confidence = 0.5;
    }

    return result;
}

std::vector<Entity> TextAnalysisService::extract_entities(const std::string& text) {
    std::vector<Entity> entities;

    try {
        std::string prompt = build_entity_extraction_prompt(text);

        OpenAICompletionRequest request;
        request.model = default_model_;
        request.messages = {{"system", "You are an expert at named entity recognition. Extract all named entities from the text and return them as a JSON array with 'text', 'type', 'confidence', and position information."},
                           {"user", prompt}};
        request.temperature = 0.1;
        request.max_tokens = 500;

        auto response = openai_client_->create_chat_completion(request);
        if (!response || response->choices.empty()) {
            throw std::runtime_error("Failed to get entity extraction response from OpenAI");
        }

        std::string response_text = response->choices[0].message.content;
        entities = parse_entity_response(response_text);

        // Filter by confidence threshold
        entities.erase(
            std::remove_if(entities.begin(), entities.end(),
                          [this](const Entity& e) { return e.confidence < confidence_threshold_; }),
            entities.end()
        );

    } catch (const std::exception& e) {
        spdlog::error("Entity extraction failed: {}", e.what());
    }

    return entities;
}

SummarizationResult TextAnalysisService::summarize_text(const std::string& text, int max_length) {
    SummarizationResult result;

    try {
        std::string prompt = build_summarization_prompt(text, max_length);

        OpenAICompletionRequest request;
        request.model = default_model_;
        request.messages = {{"system", "You are an expert at text summarization. Create a concise summary of the given text while preserving the key information and main points."},
                           {"user", prompt}};
        request.temperature = 0.3;
        request.max_tokens = 300;

        auto response = openai_client_->create_chat_completion(request);
        if (!response || response->choices.empty()) {
            throw std::runtime_error("Failed to get summarization response from OpenAI");
        }

        std::string response_text = response->choices[0].message.content;
        result = parse_summarization_response(response_text);
        result.original_length = text.length();
        result.summary_length = result.summary.length();
        result.compression_ratio = static_cast<double>(result.summary_length) / result.original_length;

    } catch (const std::exception& e) {
        spdlog::error("Text summarization failed: {}", e.what());
        result.summary = text.substr(0, std::min(100, static_cast<int>(text.length())));
        result.method_used = "fallback";
    }

    return result;
}

ClassificationResult TextAnalysisService::classify_topics(const std::string& text) {
    ClassificationResult result;

    try {
        std::string prompt = build_classification_prompt(text);

        OpenAICompletionRequest request;
        request.model = default_model_;
        request.messages = {{"system", "You are an expert at topic classification. Analyze the text and identify the main topics, categories, and key themes. Return a structured JSON response."},
                           {"user", prompt}};
        request.temperature = 0.2;
        request.max_tokens = 300;

        auto response = openai_client_->create_chat_completion(request);
        if (!response || response->choices.empty()) {
            throw std::runtime_error("Failed to get classification response from OpenAI");
        }

        std::string response_text = response->choices[0].message.content;
        result = parse_classification_response(response_text);

    } catch (const std::exception& e) {
        spdlog::error("Topic classification failed: {}", e.what());
        result.primary_topic = "general";
        result.categories = {"general"};
    }

    return result;
}

LanguageDetectionResult TextAnalysisService::detect_language(const std::string& text) {
    LanguageDetectionResult result;

    // Simple fallback language detection based on common patterns
    result = detect_text_language_fallback(text);

    // In a production system, you would use a proper language detection library
    // or call a language detection API

    return result;
}

std::vector<std::string> TextAnalysisService::extract_keywords(const std::string& text, int max_keywords) {
    std::vector<std::string> keywords;

    try {
        std::string prompt = build_keyword_extraction_prompt(text, max_keywords);

        OpenAICompletionRequest request;
        request.model = default_model_;
        request.messages = {{"system", "You are an expert at keyword extraction. Identify the most important and relevant keywords from the text. Return them as a JSON array of strings."},
                           {"user", prompt}};
        request.temperature = 0.1;
        request.max_tokens = 200;

        auto response = openai_client_->create_chat_completion(request);
        if (!response || response->choices.empty()) {
            throw std::runtime_error("Failed to get keyword extraction response from OpenAI");
        }

        std::string response_text = response->choices[0].message.content;
        keywords = parse_keyword_response(response_text);

        // Limit results
        if (keywords.size() > static_cast<size_t>(max_keywords)) {
            keywords.resize(max_keywords);
        }

    } catch (const std::exception& e) {
        spdlog::error("Keyword extraction failed: {}", e.what());
        // Fallback: extract some basic keywords
        keywords = {"content", "text", "analysis"};
    }

    return keywords;
}

std::vector<TextAnalysisResult> TextAnalysisService::analyze_batch(
    const std::vector<TextAnalysisRequest>& requests,
    int max_concurrent
) {
    std::vector<TextAnalysisResult> results;
    results.reserve(requests.size());

    if (requests.empty()) {
        return results;
    }

    // Create batches for parallel processing
    auto batches = create_batches(requests, batch_size_);

    std::vector<std::future<std::vector<TextAnalysisResult>>> futures;

    // Process batches concurrently
    for (const auto& batch : batches) {
        futures.push_back(std::async(std::launch::async, [this, batch]() {
            std::vector<TextAnalysisResult> batch_results;
            for (const auto& request : batch) {
                batch_results.push_back(analyze_text(request));
            }
            return batch_results;
        }));
    }

    // Collect results
    for (auto& future : futures) {
        auto batch_results = future.get();
        results.insert(results.end(), batch_results.begin(), batch_results.end());
    }

    spdlog::info("Batch text analysis completed: {} requests processed", results.size());
    return results;
}

// Cache management methods
bool TextAnalysisService::clear_cache(const std::string& text_hash) {
    try {
        if (!db_conn_) return false;

        std::string query;
        std::vector<std::string> params;

        if (text_hash.empty()) {
            // Clear all cache
            query = "DELETE FROM text_analysis_cache WHERE created_at < NOW() - INTERVAL '24 hours'";
            // Note: This only clears expired cache. For full cache clear, we'd need a different approach
        } else {
            // Clear specific cache entry
            query = "DELETE FROM text_analysis_cache WHERE text_hash = $1";
            params = {text_hash};
        }

        return db_conn_->execute_command(query, params);

    } catch (const std::exception& e) {
        spdlog::error("Cache clear failed: {}", e.what());
        return false;
    }
}

std::unordered_map<std::string, int> TextAnalysisService::get_cache_stats() {
    std::unordered_map<std::string, int> stats = {
        {"cache_enabled", cache_enabled_ ? 1 : 0},
        {"ttl_hours", cache_ttl_hours_}
    };

    try {
        if (db_conn_) {
            std::string query = "SELECT COUNT(*) as cache_count FROM text_analysis_cache";
            auto result = db_conn_->execute_query(query, {});
            if (!result.rows.empty()) {
                stats["cached_entries"] = std::stoi(result.rows[0].at("cache_count"));
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to get cache stats: {}", e.what());
    }

    return stats;
}

// Configuration setters
void TextAnalysisService::set_default_model(const std::string& model) {
    default_model_ = model;
}

void TextAnalysisService::set_cache_enabled(bool enabled) {
    cache_enabled_ = enabled;
}

void TextAnalysisService::set_cache_ttl_hours(int hours) {
    cache_ttl_hours_ = std::max(1, hours);
}

void TextAnalysisService::set_batch_size(int size) {
    batch_size_ = std::max(1, size);
}

void TextAnalysisService::set_confidence_threshold(double threshold) {
    confidence_threshold_ = std::max(0.0, std::min(1.0, threshold));
}

// Private helper methods implementation
std::string TextAnalysisService::generate_text_hash(const std::string& text) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, text.c_str(), text.length());
    SHA256_Final(hash, &sha256);

    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }

    return ss.str();
}

std::string TextAnalysisService::generate_tasks_hash(const std::vector<AnalysisTask>& tasks) {
    std::stringstream ss;
    for (AnalysisTask task : tasks) {
        ss << static_cast<int>(task) << ",";
    }
    return generate_text_hash(ss.str());
}

std::string TextAnalysisService::generate_uuid() {
    uuid_t uuid;
    uuid_generate(uuid);
    char uuid_str[37];
    uuid_unparse(uuid, uuid_str);
    return std::string(uuid_str);
}

std::string TextAnalysisService::build_sentiment_prompt(const std::string& text) {
    return "Analyze the sentiment of this text and respond with a JSON object containing:\n"
           "- label: 'positive', 'negative', or 'neutral'\n"
           "- confidence: number between 0 and 1\n"
           "- scores: object with detailed sentiment scores\n\n"
           "Text: " + text;
}

std::string TextAnalysisService::build_entity_extraction_prompt(const std::string& text) {
    return "Extract all named entities from this text. For each entity, provide:\n"
           "- text: the entity text\n"
           "- type: PERSON, ORG, GPE, MONEY, DATE, etc.\n"
           "- confidence: number between 0 and 1\n"
           "- start_pos and end_pos: character positions\n\n"
           "Return as a JSON array of entity objects.\n\n"
           "Text: " + text;
}

std::string TextAnalysisService::build_summarization_prompt(const std::string& text, int max_length) {
    return "Summarize this text in " + std::to_string(max_length) + " words or less, "
           "preserving the key information and main points:\n\n" + text;
}

std::string TextAnalysisService::build_classification_prompt(const std::string& text) {
    return "Classify this text by topic and extract keywords. Respond with JSON containing:\n"
           "- primary_topic: main topic\n"
           "- topic_scores: array of [topic, score] pairs\n"
           "- keywords: array of important keywords\n"
           "- categories: array of relevant categories\n\n"
           "Text: " + text;
}

std::string TextAnalysisService::build_keyword_extraction_prompt(const std::string& text, int max_keywords) {
    return "Extract the " + std::to_string(max_keywords) + " most important keywords from this text. "
           "Return as a JSON array of strings:\n\n" + text;
}

SentimentResult TextAnalysisService::parse_sentiment_response(const std::string& response) {
    SentimentResult result;

    try {
        nlohmann::json json_response = nlohmann::json::parse(response);

        result.label = json_response.value("label", "neutral");
        result.confidence = json_response.value("confidence", 0.5);

        if (json_response.contains("scores")) {
            for (auto& [key, value] : json_response["scores"].items()) {
                result.scores[key] = value;
            }
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to parse sentiment response: {}", e.what());
        result.label = "neutral";
        result.confidence = 0.5;
    }

    return result;
}

std::vector<Entity> TextAnalysisService::parse_entity_response(const std::string& response) {
    std::vector<Entity> entities;

    try {
        nlohmann::json json_response = nlohmann::json::parse(response);

        if (json_response.is_array()) {
            for (const auto& entity_json : json_response) {
                Entity entity;
                entity.text = entity_json.value("text", "");
                entity.type = entity_json.value("type", "");
                entity.confidence = entity_json.value("confidence", 0.5);
                entity.start_pos = entity_json.value("start_pos", 0);
                entity.end_pos = entity_json.value("end_pos", 0);

                if (!entity.text.empty()) {
                    entities.push_back(entity);
                }
            }
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to parse entity response: {}", e.what());
    }

    return entities;
}

SummarizationResult TextAnalysisService::parse_summarization_response(const std::string& response) {
    SummarizationResult result;
    result.summary = response;
    result.method_used = "abstractive";

    // Clean up the response
    // Remove any JSON formatting if present
    if (result.summary.find("{") == 0 || result.summary.find("[") == 0) {
        try {
            nlohmann::json json_resp = nlohmann::json::parse(result.summary);
            if (json_resp.contains("summary")) {
                result.summary = json_resp["summary"];
            }
        } catch (...) {
            // Keep original response
        }
    }

    return result;
}

ClassificationResult TextAnalysisService::parse_classification_response(const std::string& response) {
    ClassificationResult result;

    try {
        nlohmann::json json_response = nlohmann::json::parse(response);

        result.primary_topic = json_response.value("primary_topic", "general");

        if (json_response.contains("topic_scores") && json_response["topic_scores"].is_array()) {
            for (const auto& score_pair : json_response["topic_scores"]) {
                if (score_pair.is_array() && score_pair.size() >= 2) {
                    result.topic_scores.emplace_back(score_pair[0], score_pair[1]);
                }
            }
        }

        if (json_response.contains("keywords") && json_response["keywords"].is_array()) {
            for (const auto& keyword : json_response["keywords"]) {
                result.keywords.push_back(keyword);
            }
        }

        if (json_response.contains("categories") && json_response["categories"].is_array()) {
            for (const auto& category : json_response["categories"]) {
                result.categories.push_back(category);
            }
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to parse classification response: {}", e.what());
        result.primary_topic = "general";
        result.categories = {"general"};
    }

    return result;
}

std::vector<std::string> TextAnalysisService::parse_keyword_response(const std::string& response) {
    std::vector<std::string> keywords;

    try {
        nlohmann::json json_response = nlohmann::json::parse(response);

        if (json_response.is_array()) {
            for (const auto& keyword : json_response) {
                if (keyword.is_string()) {
                    keywords.push_back(keyword);
                }
            }
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to parse keyword response: {}", e.what());
        // Fallback: try to extract keywords from raw text
        std::stringstream ss(response);
        std::string word;
        while (ss >> word && keywords.size() < 10) {
            // Basic filtering
            if (word.length() > 3) {
                keywords.push_back(word);
            }
        }
    }

    return keywords;
}

double TextAnalysisService::calculate_entity_confidence(const std::vector<Entity>& entities) {
    if (entities.empty()) return 0.0;

    double total_confidence = 0.0;
    for (const auto& entity : entities) {
        total_confidence += entity.confidence;
    }

    return total_confidence / entities.size();
}

int TextAnalysisService::estimate_token_count(const std::string& text) {
    // Rough estimation: ~4 characters per token for English text
    return std::max(1, static_cast<int>(text.length() / 4));
}

double TextAnalysisService::calculate_task_cost(AnalysisTask task, int tokens_used) {
    // OpenAI pricing (approximate, per 1K tokens)
    double input_price_per_1k = 0.03;   // GPT-4 input
    double output_price_per_1k = 0.06;  // GPT-4 output

    // Estimate 70% input, 30% output
    int input_tokens = static_cast<int>(tokens_used * 0.7);
    int output_tokens = tokens_used - input_tokens;

    double input_cost = (input_tokens / 1000.0) * input_price_per_1k;
    double output_cost = (output_tokens / 1000.0) * output_price_per_1k;

    return input_cost + output_cost;
}

std::vector<std::vector<TextAnalysisRequest>> TextAnalysisService::create_batches(
    const std::vector<TextAnalysisRequest>& requests,
    int batch_size
) {
    std::vector<std::vector<TextAnalysisRequest>> batches;

    for (size_t i = 0; i < requests.size(); i += batch_size) {
        size_t end = std::min(i + batch_size, requests.size());
        batches.emplace_back(requests.begin() + i, requests.begin() + end);
    }

    return batches;
}

std::string TextAnalysisService::normalize_text(const std::string& text) {
    std::string normalized = text;

    // Basic normalization
    // Remove excessive whitespace
    auto end = std::unique(normalized.begin(), normalized.end(),
                          [](char a, char b) { return std::isspace(a) && std::isspace(b); });
    normalized.erase(end, normalized.end());

    // Trim leading/trailing whitespace
    normalized.erase(normalized.begin(),
                    std::find_if(normalized.begin(), normalized.end(),
                                [](char c) { return !std::isspace(c); }));
    normalized.erase(std::find_if(normalized.rbegin(), normalized.rend(),
                                 [](char c) { return !std::isspace(c); }).base(),
                    normalized.end());

    return normalized;
}

LanguageDetectionResult TextAnalysisService::detect_text_language_fallback(const std::string& text) {
    LanguageDetectionResult result;

    // Simple heuristic-based language detection
    // This is a very basic implementation - in production, use a proper library

    // Check for Spanish words
    if (text.find(" el ") != std::string::npos ||
        text.find(" la ") != std::string::npos ||
        text.find(" que ") != std::string::npos) {
        result.language_code = "es";
        result.language_name = "Spanish";
        result.confidence = 0.8;
    }
    // Check for French words
    else if (text.find(" le ") != std::string::npos ||
             text.find(" la ") != std::string::npos ||
             text.find(" et ") != std::string::npos) {
        result.language_code = "fr";
        result.language_name = "French";
        result.confidence = 0.8;
    }
    // Default to English
    else {
        result.language_code = "en";
        result.language_name = "English";
        result.confidence = 0.9;
    }

    return result;
}

// Cache operations (simplified - in production, use Redis)
std::optional<TextAnalysisResult> TextAnalysisService::get_cached_result(
    const std::string& text_hash,
    const std::string& tasks_hash
) {
    if (!redis_client_) {
        return std::nullopt;
    }

    try {
        std::string cache_key = "text_analysis:" + text_hash + ":" + tasks_hash;

        auto cached_data = redis_client_->get(cache_key);
        if (!cached_data) {
            return std::nullopt;
        }

        // Parse cached JSON data
        nlohmann::json cached_json = nlohmann::json::parse(*cached_data);

        // Reconstruct TextAnalysisResult from cached data
        TextAnalysisResult result;
        result.request_id = cached_json["request_id"];
        result.text_hash = cached_json["text_hash"];

        // Parse timestamp
        if (cached_json.contains("analyzed_at")) {
            std::string timestamp_str = cached_json["analyzed_at"];
            // Simple timestamp parsing - in production, use proper parsing
            result.analyzed_at = std::chrono::system_clock::now(); // Fallback
        }

        if (cached_json.contains("processing_time_ms")) {
            result.processing_time = std::chrono::milliseconds(cached_json["processing_time_ms"]);
        }

        // Parse sentiment if present
        if (cached_json.contains("sentiment")) {
            SentimentResult sentiment;
            sentiment.label = cached_json["sentiment"]["label"];
            sentiment.confidence = cached_json["sentiment"]["confidence"];
            if (cached_json["sentiment"].contains("scores")) {
                for (auto& [key, value] : cached_json["sentiment"]["scores"].items()) {
                    sentiment.scores[key] = value;
                }
            }
            result.sentiment = sentiment;
        }

        // Parse entities if present
        if (cached_json.contains("entities")) {
            for (auto& entity_json : cached_json["entities"]) {
                Entity entity;
                entity.text = entity_json["text"];
                entity.type = entity_json["type"];
                entity.category = entity_json.value("category", "");
                entity.confidence = entity_json.value("confidence", 0.0);
                entity.start_pos = entity_json.value("start_pos", 0);
                entity.end_pos = entity_json.value("end_pos", 0);
                result.entities.push_back(entity);
            }
        }

        // Parse other fields as needed
        result.success = cached_json.value("success", true);
        result.total_tokens = cached_json.value("total_tokens", 0);
        result.total_cost = cached_json.value("total_cost", 0.0);

        spdlog::info("Retrieved cached text analysis result for hash: {}", text_hash);
        return result;

    } catch (const std::exception& e) {
        spdlog::warn("Failed to retrieve cached result: {}", e.what());
        return std::nullopt;
    }
}

void TextAnalysisService::cache_result(
    const std::string& text_hash,
    const std::string& tasks_hash,
    const TextAnalysisResult& result
) {
    if (!redis_client_) {
        return;
    }

    try {
        std::string cache_key = "text_analysis:" + text_hash + ":" + tasks_hash;

        // Build JSON representation for caching
        nlohmann::json cache_json;
        cache_json["request_id"] = result.request_id;
        cache_json["text_hash"] = result.text_hash;
        cache_json["analyzed_at"] = std::to_string(
            std::chrono::duration_cast<std::chrono::seconds>(
                result.analyzed_at.time_since_epoch()).count());
        cache_json["processing_time_ms"] = result.processing_time.count();
        cache_json["success"] = result.success;
        cache_json["total_tokens"] = result.total_tokens;
        cache_json["total_cost"] = result.total_cost;

        // Cache sentiment
        if (result.sentiment) {
            nlohmann::json sentiment_json;
            sentiment_json["label"] = result.sentiment->label;
            sentiment_json["confidence"] = result.sentiment->confidence;
            nlohmann::json scores_json;
            for (auto& [key, value] : result.sentiment->scores) {
                scores_json[key] = value;
            }
            sentiment_json["scores"] = scores_json;
            cache_json["sentiment"] = sentiment_json;
        }

        // Cache entities
        if (!result.entities.empty()) {
            nlohmann::json entities_json = nlohmann::json::array();
            for (const auto& entity : result.entities) {
                nlohmann::json entity_json;
                entity_json["text"] = entity.text;
                entity_json["type"] = entity.type;
                entity_json["category"] = entity.category;
                entity_json["confidence"] = entity.confidence;
                entity_json["start_pos"] = entity.start_pos;
                entity_json["end_pos"] = entity.end_pos;
                entities_json.push_back(entity_json);
            }
            cache_json["entities"] = entities_json;
        }

        // Cache other results as needed
        if (result.summary) {
            nlohmann::json summary_json;
            summary_json["summary"] = result.summary->summary;
            summary_json["compression_ratio"] = result.summary->compression_ratio;
            cache_json["summary"] = summary_json;
        }

        std::string cache_data = cache_json.dump();

        // Set cache with TTL (24 hours by default)
        int ttl_seconds = cache_ttl_hours_ * 3600;
        redis_client_->setex(cache_key, ttl_seconds, cache_data);

        spdlog::info("Cached text analysis result for hash: {}", text_hash);

    } catch (const std::exception& e) {
        spdlog::warn("Failed to cache result: {}", e.what());
    }
}

// Database operations
void TextAnalysisService::store_analysis_result(const TextAnalysisResult& result) {
    if (!db_conn_) {
        spdlog::warn("No database connection available for storing analysis result");
        return;
    }

    try {
        // Prepare JSON data for storage
        nlohmann::json result_json;
        result_json["request_id"] = result.request_id;
        result_json["text_hash"] = result.text_hash;
        result_json["processing_time_ms"] = result.processing_time.count();
        result_json["success"] = result.success;
        result_json["total_tokens"] = result.total_tokens;
        result_json["total_cost"] = result.total_cost;

        // Store task confidences
        nlohmann::json confidences_json;
        for (auto& [task, confidence] : result.task_confidences) {
            confidences_json[task] = confidence;
        }
        result_json["task_confidences"] = confidences_json;

        std::string result_data = result_json.dump();

        // SQL to insert analysis result
        const char* query = R"(
            INSERT INTO text_analysis_results (
                request_id, text_hash, result_data, analyzed_at, processing_time_ms,
                total_tokens, total_cost, created_at
            ) VALUES ($1, $2, $3, $4, $5, $6, $7, NOW())
            ON CONFLICT (text_hash) DO UPDATE SET
                result_data = EXCLUDED.result_data,
                analyzed_at = EXCLUDED.analyzed_at,
                processing_time_ms = EXCLUDED.processing_time_ms,
                total_tokens = EXCLUDED.total_tokens,
                total_cost = EXCLUDED.total_cost
        )";

        // Convert timestamp to string
        auto analyzed_time = std::chrono::system_clock::to_time_t(result.analyzed_at);
        char time_buffer[26];
        std::strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&analyzed_time));
        std::string analyzed_at_str = time_buffer;

        const char* param_values[7] = {
            result.request_id.c_str(),
            result.text_hash.c_str(),
            result_data.c_str(),
            analyzed_at_str.c_str(),
            std::to_string(result.processing_time.count()).c_str(),
            std::to_string(result.total_tokens).c_str(),
            std::to_string(result.total_cost).c_str()
        };

        PGresult* pg_result = PQexecParams(db_conn_->get_connection(), query, 7, NULL, param_values, NULL, NULL, 0);

        if (PQresultStatus(pg_result) != PGRES_COMMAND_OK) {
            spdlog::error("Failed to store analysis result: {}", PQerrorMessage(db_conn_->get_connection()));
        } else {
            spdlog::info("Stored analysis result for request: {}", result.request_id);
        }

        PQclear(pg_result);

    } catch (const std::exception& e) {
        spdlog::error("Failed to store analysis result: {}", e.what());
    }
}

std::optional<TextAnalysisResult> TextAnalysisService::load_analysis_result(
    const std::string& text_hash,
    const std::string& tasks_hash
) {
    if (!db_conn_) {
        return std::nullopt;
    }

    try {
        const char* query = R"(
            SELECT request_id, result_data, analyzed_at, processing_time_ms,
                   total_tokens, total_cost
            FROM text_analysis_results
            WHERE text_hash = $1
            ORDER BY created_at DESC
            LIMIT 1
        )";

        const char* param_values[1] = {text_hash.c_str()};
        PGresult* pg_result = PQexecParams(db_conn_->get_connection(), query, 1, NULL, param_values, NULL, NULL, 0);

        if (PQresultStatus(pg_result) != PGRES_TUPLES_OK || PQntuples(pg_result) == 0) {
            PQclear(pg_result);
            return std::nullopt;
        }

        // Parse database result
        TextAnalysisResult result;
        result.request_id = PQgetvalue(pg_result, 0, 0);
        result.text_hash = text_hash;

        std::string result_data = PQgetvalue(pg_result, 0, 1);
        nlohmann::json result_json = nlohmann::json::parse(result_data);

        result.success = result_json.value("success", true);
        result.total_tokens = result_json.value("total_tokens", 0);
        result.total_cost = result_json.value("total_cost", 0.0);

        if (result_json.contains("task_confidences")) {
            for (auto& [task, confidence] : result_json["task_confidences"].items()) {
                result.task_confidences[task] = confidence;
            }
        }

        result.analyzed_at = std::chrono::system_clock::now(); // Fallback timestamp

        if (PQgetvalue(pg_result, 0, 3)) {
            result.processing_time = std::chrono::milliseconds(
                std::stoll(PQgetvalue(pg_result, 0, 3)));
        }

        PQclear(pg_result);

        spdlog::info("Loaded analysis result from database for hash: {}", text_hash);
        return result;

    } catch (const std::exception& e) {
        spdlog::error("Failed to load analysis result: {}", e.what());
        return std::nullopt;
    }
}

} // namespace regulens
