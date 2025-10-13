/**
 * Message Translator API Handlers
 * REST API endpoints for protocol translation between agents
 */

#ifndef MESSAGE_TRANSLATOR_API_HANDLERS_HPP
#define MESSAGE_TRANSLATOR_API_HANDLERS_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "message_translator.hpp"
#include "../../database/postgresql_connection.hpp"

namespace regulens {

class MessageTranslatorAPIHandlers {
public:
    MessageTranslatorAPIHandlers(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<MessageTranslator> translator
    );

    ~MessageTranslatorAPIHandlers();

    // Message translation endpoints
    std::string handle_translate_message(const std::string& request_body, const std::string& user_id);
    std::string handle_batch_translate(const std::string& request_body, const std::string& user_id);
    std::string handle_detect_protocol(const std::string& request_body, const std::string& user_id);

    // Translation rules management endpoints
    std::string handle_add_translation_rule(const std::string& request_body, const std::string& user_id);
    std::string handle_update_translation_rule(const std::string& rule_id, const std::string& request_body, const std::string& user_id);
    std::string handle_remove_translation_rule(const std::string& rule_id, const std::string& user_id);
    std::string handle_get_translation_rules(const std::string& query_params, const std::string& user_id);

    // Protocol schema management endpoints
    std::string handle_register_protocol_schema(const std::string& protocol, const std::string& request_body, const std::string& user_id);
    std::string handle_get_protocol_schema(const std::string& protocol, const std::string& user_id);
    std::string handle_validate_message(const std::string& protocol, const std::string& request_body, const std::string& user_id);

    // Protocol conversion endpoints
    std::string handle_json_rpc_to_rest(const std::string& request_body, const std::string& user_id);
    std::string handle_rest_to_json_rpc(const std::string& request_body, const std::string& user_id);
    std::string handle_json_rpc_to_grpc(const std::string& request_body, const std::string& user_id);
    std::string handle_grpc_to_json_rpc(const std::string& request_body, const std::string& user_id);
    std::string handle_rest_to_soap(const std::string& request_body, const std::string& user_id);
    std::string handle_soap_to_rest(const std::string& request_body, const std::string& user_id);

    // Analytics and monitoring endpoints
    std::string handle_get_translation_stats(const std::string& user_id);
    std::string handle_get_protocol_usage_stats(const std::string& user_id);

    // Configuration endpoints
    std::string handle_get_translator_config(const std::string& user_id);
    std::string handle_update_translator_config(const std::string& request_body, const std::string& user_id);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<MessageTranslator> translator_;

    // Helper methods
    MessageHeader parse_message_header(const nlohmann::json& request);
    TranslationRule parse_translation_rule(const nlohmann::json& request);
    MessageProtocol parse_protocol_param(const std::string& protocol_str);

    nlohmann::json format_translation_result(const TranslationResultData& result);
    nlohmann::json format_translation_rule(const TranslationRule& rule);
    nlohmann::json format_batch_results(const std::vector<TranslationResultData>& results);

    // Request validation
    bool validate_translation_request(const nlohmann::json& request, std::string& error_message);
    bool validate_rule_request(const nlohmann::json& request, std::string& error_message);
    bool validate_batch_request(const nlohmann::json& request, std::string& error_message);
    bool validate_user_access(const std::string& user_id, const std::string& operation);
    bool is_admin_user(const std::string& user_id);

    // Response formatting
    nlohmann::json create_success_response(const nlohmann::json& data = nullptr,
                                         const std::string& message = "");
    nlohmann::json create_error_response(const std::string& message,
                                       int status_code = 400);

    // Query parameter parsing
    std::unordered_map<std::string, std::string> parse_query_params(const std::string& query_string);
    MessageProtocol parse_protocol_from_query(const std::string& query_string);
    int parse_int_param(const std::string& value, int default_value = 0);
    bool parse_bool_param(const std::string& value, bool default_value = false);

    // Batch processing
    std::vector<std::pair<std::string, MessageHeader>> prepare_batch_messages(const nlohmann::json& batch_request);
    nlohmann::json process_batch_results(const std::vector<TranslationResultData>& results);

    // Utility methods
    std::string protocol_to_string(MessageProtocol protocol);
    std::string result_to_string(TranslationResult result);
    MessageProtocol string_to_protocol(const std::string& protocol_str);
    TranslationResult string_to_result(const std::string& result_str);

    // Configuration helpers
    nlohmann::json get_translator_configuration();
    bool update_translator_configuration(const nlohmann::json& config);

    // Rule management helpers
    std::vector<nlohmann::json> filter_rules_by_protocols(const std::vector<TranslationRule>& rules,
                                                        MessageProtocol from_protocol,
                                                        MessageProtocol to_protocol);
    nlohmann::json get_rule_summary(const TranslationRule& rule);

    // Performance helpers
    nlohmann::json format_translation_stats(const std::unordered_map<std::string, double>& stats);
    nlohmann::json format_protocol_usage_stats(const std::vector<std::pair<std::string, int>>& usage_stats);

    // Protocol validation helpers
    bool is_supported_protocol(const std::string& protocol);
    bool is_valid_protocol_combination(MessageProtocol from, MessageProtocol to);

    // Error handling
    std::string create_validation_error(const std::string& field, const std::string& reason);
    std::string create_protocol_error(const std::string& protocol, const std::string& reason);
    std::string create_authentication_error(const std::string& operation);

    // Logging helpers
    void log_translation_request(const std::string& user_id, MessageProtocol from, MessageProtocol to);
    void log_rule_operation(const std::string& user_id, const std::string& operation, const std::string& rule_id);
    void log_batch_operation(const std::string& user_id, int batch_size);
};

} // namespace regulens

#endif // MESSAGE_TRANSLATOR_API_HANDLERS_HPP
