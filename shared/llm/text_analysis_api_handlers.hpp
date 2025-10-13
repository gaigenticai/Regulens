/**
 * Text Analysis API Handlers
 * REST API endpoints for LLM-based text analysis
 */

#ifndef TEXT_ANALYSIS_API_HANDLERS_HPP
#define TEXT_ANALYSIS_API_HANDLERS_HPP

#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "text_analysis_service.hpp"
#include "../database/postgresql_connection.hpp"

namespace regulens {

class TextAnalysisAPIHandlers {
public:
    TextAnalysisAPIHandlers(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<TextAnalysisService> text_analysis_service
    );

    ~TextAnalysisAPIHandlers();

    // Main analysis endpoints
    std::string handle_analyze_text(const std::string& request_body, const std::string& user_id);
    std::string handle_batch_analyze_text(const std::string& request_body, const std::string& user_id);

    // Individual task endpoints
    std::string handle_analyze_sentiment(const std::string& request_body, const std::string& user_id);
    std::string handle_extract_entities(const std::string& request_body, const std::string& user_id);
    std::string handle_summarize_text(const std::string& request_body, const std::string& user_id);
    std::string handle_classify_topics(const std::string& request_body, const std::string& user_id);
    std::string handle_detect_language(const std::string& request_body, const std::string& user_id);
    std::string handle_extract_keywords(const std::string& request_body, const std::string& user_id);

    // Management endpoints
    std::string handle_get_analysis_stats();
    std::string handle_clear_analysis_cache(const std::string& text_hash = "");

    // Configuration endpoints
    std::string handle_get_analysis_config();
    std::string handle_update_analysis_config(const std::string& request_body);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<TextAnalysisService> text_analysis_service_;

    // Helper methods
    TextAnalysisRequest parse_analysis_request(const nlohmann::json& request);
    nlohmann::json format_analysis_result(const TextAnalysisResult& result);
    nlohmann::json format_sentiment_result(const SentimentResult& result);
    nlohmann::json format_entity_results(const std::vector<Entity>& entities);
    nlohmann::json format_summarization_result(const SummarizationResult& result);
    nlohmann::json format_classification_result(const ClassificationResult& result);
    nlohmann::json format_language_result(const LanguageDetectionResult& result);
    nlohmann::json format_keyword_results(const std::vector<std::string>& keywords);

    // Validation methods
    bool validate_analysis_request(const nlohmann::json& request, std::string& error_message);
    bool validate_text_input(const std::string& text, std::string& error_message);
    bool validate_user_access(const std::string& user_id, const std::string& operation);

    // Task parsing
    std::vector<AnalysisTask> parse_task_list(const nlohmann::json& tasks_json);
    AnalysisTask parse_single_task(const std::string& task_str);

    // Response formatting
    nlohmann::json create_success_response(const nlohmann::json& data = nullptr,
                                         const std::string& message = "");
    nlohmann::json create_error_response(const std::string& message,
                                       int status_code = 400);
    nlohmann::json create_batch_response(const std::vector<nlohmann::json>& results,
                                       int total_count,
                                       int processed_count);

    // Utility methods
    std::string get_supported_tasks_list();
    std::string get_supported_languages_list();
    bool is_task_supported(AnalysisTask task);
};

} // namespace regulens

#endif // TEXT_ANALYSIS_API_HANDLERS_HPP
