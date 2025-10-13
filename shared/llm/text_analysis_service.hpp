/**
 * LLM Text Analysis Service
 * Production-grade multi-task NLP analysis pipeline
 */

#ifndef TEXT_ANALYSIS_SERVICE_HPP
#define TEXT_ANALYSIS_SERVICE_HPP

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <chrono>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "../database/postgresql_connection.hpp"
#include "../cache/redis_client.hpp"
#include "openai_client.hpp"

namespace regulens {

enum class AnalysisTask {
    SENTIMENT_ANALYSIS,
    ENTITY_EXTRACTION,
    TEXT_SUMMARIZATION,
    TOPIC_CLASSIFICATION,
    LANGUAGE_DETECTION,
    KEYWORD_EXTRACTION,
    EMOTION_ANALYSIS,
    INTENT_RECOGNITION
};

struct SentimentResult {
    std::string label;  // "positive", "negative", "neutral"
    double confidence = 0.0;
    std::unordered_map<std::string, double> scores;  // Detailed scores
};

struct Entity {
    std::string text;
    std::string type;   // "PERSON", "ORG", "GPE", "MONEY", etc.
    std::string category;  // "person", "organization", "location", etc.
    double confidence = 0.0;
    size_t start_pos = 0;
    size_t end_pos = 0;
    std::optional<nlohmann::json> metadata;
};

struct SummarizationResult {
    std::string summary;
    double compression_ratio = 0.0;
    int original_length = 0;
    int summary_length = 0;
    std::string method_used;  // "extractive", "abstractive"
};

struct ClassificationResult {
    std::string primary_topic;
    std::vector<std::pair<std::string, double>> topic_scores;
    std::vector<std::string> keywords;
    std::vector<std::string> categories;
};

struct LanguageDetectionResult {
    std::string language_code;  // "en", "es", "fr", etc.
    std::string language_name;  // "English", "Spanish", etc.
    double confidence = 0.0;
    std::vector<std::pair<std::string, double>> alternatives;
};

struct TextAnalysisRequest {
    std::string text;
    std::vector<AnalysisTask> tasks;
    std::string source = "api";  // "api", "document", "email", etc.
    std::optional<std::string> language_hint;
    std::optional<std::string> domain_context;  // "regulatory", "financial", etc.
    bool enable_caching = true;
    int max_keywords = 10;
    double confidence_threshold = 0.5;
};

struct TextAnalysisResult {
    std::string request_id;
    std::string text_hash;
    std::chrono::system_clock::time_point analyzed_at;
    std::chrono::milliseconds processing_time;

    std::optional<SentimentResult> sentiment;
    std::vector<Entity> entities;
    std::optional<SummarizationResult> summary;
    std::optional<ClassificationResult> classification;
    std::optional<LanguageDetectionResult> language;
    std::vector<std::string> keywords;
    std::optional<nlohmann::json> emotions;
    std::optional<std::string> intent;

    bool success = true;
    std::optional<std::string> error_message;
    std::unordered_map<std::string, double> task_confidences;

    // Token and cost tracking
    int total_tokens = 0;
    double total_cost = 0.0;
};

struct AnalysisCacheEntry {
    std::string text_hash;
    std::string analysis_result;
    std::chrono::system_clock::time_point created_at;
    std::chrono::hours ttl = std::chrono::hours(24);
    std::string tasks_hash;  // Hash of requested tasks
};

class TextAnalysisService {
public:
    TextAnalysisService(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<OpenAIClient> openai_client,
        std::shared_ptr<RedisClient> redis_client = nullptr
    );

    ~TextAnalysisService();

    // Core analysis methods
    TextAnalysisResult analyze_text(const TextAnalysisRequest& request);

    // Individual task methods
    SentimentResult analyze_sentiment(const std::string& text);
    std::vector<Entity> extract_entities(const std::string& text);
    SummarizationResult summarize_text(const std::string& text, int max_length = 150);
    ClassificationResult classify_topics(const std::string& text);
    LanguageDetectionResult detect_language(const std::string& text);
    std::vector<std::string> extract_keywords(const std::string& text, int max_keywords = 10);

    // Batch processing
    std::vector<TextAnalysisResult> analyze_batch(
        const std::vector<TextAnalysisRequest>& requests,
        int max_concurrent = 5
    );

    // Cache management
    bool clear_cache(const std::string& text_hash = "");
    std::unordered_map<std::string, int> get_cache_stats();

    // Configuration
    void set_default_model(const std::string& model);
    void set_cache_enabled(bool enabled);
    void set_cache_ttl_hours(int hours);
    void set_batch_size(int size);
    void set_confidence_threshold(double threshold);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<OpenAIClient> openai_client_;
    std::shared_ptr<RedisClient> redis_client_;

    // Configuration
    std::string default_model_ = "gpt-4-turbo-preview";
    bool cache_enabled_ = true;
    int cache_ttl_hours_ = 24;
    int batch_size_ = 5;
    double confidence_threshold_ = 0.5;

    // Internal methods
    std::string generate_text_hash(const std::string& text);
    std::string generate_tasks_hash(const std::vector<AnalysisTask>& tasks);

    // Cache operations
    std::optional<TextAnalysisResult> get_cached_result(const std::string& text_hash, const std::string& tasks_hash);
    void cache_result(const std::string& text_hash, const std::string& tasks_hash, const TextAnalysisResult& result);

    // LLM prompt generation
    std::string build_sentiment_prompt(const std::string& text);
    std::string build_entity_extraction_prompt(const std::string& text);
    std::string build_summarization_prompt(const std::string& text, int max_length);
    std::string build_classification_prompt(const std::string& text);
    std::string build_keyword_extraction_prompt(const std::string& text, int max_keywords);

    // Response parsing
    SentimentResult parse_sentiment_response(const std::string& response);
    std::vector<Entity> parse_entity_response(const std::string& response);
    SummarizationResult parse_summarization_response(const std::string& response);
    ClassificationResult parse_classification_response(const std::string& response);
    std::vector<std::string> parse_keyword_response(const std::string& response);

    // Cost and token tracking
    void update_analysis_stats(const TextAnalysisResult& result);
    double calculate_task_cost(AnalysisTask task, int tokens_used);
    double calculate_entity_confidence(const std::vector<Entity>& entities);
    int estimate_token_count(const std::string& text);

    // Database operations
    void store_analysis_result(const TextAnalysisResult& result);
    std::optional<TextAnalysisResult> load_analysis_result(const std::string& text_hash, const std::string& tasks_hash);

    // Utility methods
    std::string normalize_text(const std::string& text);
    std::string generate_uuid();
    bool is_valid_language_code(const std::string& code);
    LanguageDetectionResult detect_text_language_fallback(const std::string& text);

    // Batch processing helpers
    std::vector<std::vector<TextAnalysisRequest>> create_batches(
        const std::vector<TextAnalysisRequest>& requests,
        int batch_size
    );
};

} // namespace regulens

#endif // TEXT_ANALYSIS_SERVICE_HPP
