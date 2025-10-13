/**
 * Message Translator
 * Protocol conversion between agents (JSON-RPC, gRPC, REST)
 */

#ifndef MESSAGE_TRANSLATOR_HPP
#define MESSAGE_TRANSLATOR_HPP

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>
#include <nlohmann/json.hpp>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"

namespace regulens {

enum class MessageProtocol {
    JSON_RPC,      // JSON-RPC 2.0
    REST_HTTP,     // REST over HTTP
    GRAPHQL,       // GraphQL
    WEBSOCKET,     // WebSocket messages
    GRPC,          // gRPC protocol buffers
    SOAP,          // SOAP XML
    MQTT,          // MQTT protocol
    AMQP,          // AMQP message queue
    CUSTOM         // Custom protocol
};

enum class MessageType {
    REQUEST,       // Request message
    RESPONSE,      // Response message
    NOTIFICATION,  // Notification/broadcast
    ERROR,         // Error message
    HEARTBEAT,     // Health check
    ACKNOWLEDGMENT // Acknowledgment
};

enum class TranslationResult {
    SUCCESS,
    PARTIAL_SUCCESS,  // Some fields translated, others defaulted
    ADAPTATION_NEEDED, // Message adapted but may lose some information
    FAILURE,          // Translation failed
    UNSUPPORTED       // Protocol combination not supported
};

struct MessageHeader {
    std::string message_id;
    std::string correlation_id;
    MessageType message_type;
    MessageProtocol source_protocol;
    MessageProtocol target_protocol;
    std::chrono::system_clock::time_point timestamp;
    std::string sender_id;
    std::string recipient_id;
    int priority = 1; // 1=low, 5=high
    std::unordered_map<std::string, std::string> custom_headers;
};

struct TranslationRule {
    std::string rule_id;
    std::string name;
    MessageProtocol from_protocol;
    MessageProtocol to_protocol;
    nlohmann::json transformation_rules;
    bool bidirectional = false;
    int priority = 1; // Higher priority rules are applied first
    bool active = true;
    std::chrono::system_clock::time_point created_at;
};

struct TranslationResultData {
    TranslationResult result;
    std::string translated_message;
    MessageHeader translated_header;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    nlohmann::json metadata;
    std::chrono::milliseconds processing_time;
};

struct ProtocolMapping {
    MessageProtocol protocol;
    std::string protocol_name;
    std::string content_type;
    std::unordered_map<std::string, std::string> default_headers;
    nlohmann::json protocol_schema;
    bool supports_binary = false;
};

class MessageTranslator {
public:
    MessageTranslator(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<StructuredLogger> logger
    );

    ~MessageTranslator();

    // Message translation
    TranslationResultData translate_message(
        const std::string& message,
        const MessageHeader& header,
        MessageProtocol target_protocol
    );

    // Protocol detection
    std::optional<MessageProtocol> detect_protocol(const std::string& message);
    std::optional<MessageType> detect_message_type(const std::string& message, MessageProtocol protocol);

    // Translation rules management
    bool add_translation_rule(const TranslationRule& rule);
    bool update_translation_rule(const std::string& rule_id, const TranslationRule& updated_rule);
    bool remove_translation_rule(const std::string& rule_id);
    std::vector<TranslationRule> get_translation_rules(MessageProtocol from_protocol = MessageProtocol::CUSTOM,
                                                       MessageProtocol to_protocol = MessageProtocol::CUSTOM);

    // Protocol schema management
    bool register_protocol_schema(MessageProtocol protocol, const nlohmann::json& schema);
    std::optional<nlohmann::json> get_protocol_schema(MessageProtocol protocol);
    bool validate_message_against_schema(const std::string& message, MessageProtocol protocol);

    // Batch translation
    std::vector<TranslationResultData> translate_batch(
        const std::vector<std::pair<std::string, MessageHeader>>& messages,
        MessageProtocol target_protocol
    );

    // Protocol-specific translators
    std::string json_rpc_to_rest(const std::string& json_rpc_message);
    std::string rest_to_json_rpc(const std::string& rest_message);
    std::string json_rpc_to_grpc(const std::string& json_rpc_message);
    std::string grpc_to_json_rpc(const std::string& grpc_message);
    std::string rest_to_soap(const std::string& rest_message);
    std::string soap_to_rest(const std::string& soap_message);
    std::string websocket_to_rest(const std::string& ws_message);
    std::string rest_to_websocket(const std::string& rest_message);

    // Message parsing and building
    std::optional<nlohmann::json> parse_message(const std::string& message, MessageProtocol protocol);
    std::string build_message(const nlohmann::json& message_data, MessageProtocol protocol);

    // Header translation
    MessageHeader translate_header(const MessageHeader& source_header, MessageProtocol target_protocol);

    // Performance monitoring
    std::unordered_map<std::string, double> get_translation_stats();
    std::vector<std::pair<std::string, int>> get_protocol_usage_stats();

    // Configuration
    void set_max_batch_size(int size);
    void set_translation_timeout(std::chrono::milliseconds timeout);
    void enable_protocol_validation(bool enable);
    void set_default_protocol(MessageProtocol protocol);

    // Log translation to database for audit trail
    bool log_translation(const std::string& message_id,
                        const std::string& source_protocol,
                        const std::string& target_protocol,
                        const nlohmann::json& source_content,
                        const nlohmann::json& translated_content,
                        const std::string& rule_id = "",
                        double quality_score = 1.0,
                        int translation_time_ms = 0,
                        const std::string& translator_agent = "",
                        const std::string& error_message = "");

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<StructuredLogger> logger_;

    // Configuration
    int max_batch_size_ = 100;
    std::chrono::milliseconds translation_timeout_ = std::chrono::milliseconds(5000);
    bool protocol_validation_enabled_ = true;
    MessageProtocol default_protocol_ = MessageProtocol::JSON_RPC;

    // Protocol mappings
    std::unordered_map<MessageProtocol, ProtocolMapping> protocol_mappings_;
    std::vector<TranslationRule> translation_rules_;

    // In-memory caches
    std::unordered_map<std::string, nlohmann::json> schema_cache_;
    std::unordered_map<std::string, TranslationResultData> translation_cache_;
    std::mutex translator_mutex_;

    // Helper methods
    void initialize_protocol_mappings();
    TranslationRule* find_best_translation_rule(MessageProtocol from, MessageProtocol to);
    nlohmann::json apply_transformation_rules(const nlohmann::json& message_data,
                                             const nlohmann::json& rules);

    // Protocol-specific parsing
    std::optional<nlohmann::json> parse_json_rpc(const std::string& message);
    std::optional<nlohmann::json> parse_rest_http(const std::string& message);
    std::optional<nlohmann::json> parse_graphql(const std::string& message);
    std::optional<nlohmann::json> parse_websocket(const std::string& message);
    std::optional<nlohmann::json> parse_grpc(const std::string& message);
    std::optional<nlohmann::json> parse_soap(const std::string& message);

    // Protocol-specific building
    std::string build_json_rpc(const nlohmann::json& message_data);
    std::string build_rest_http(const nlohmann::json& message_data);
    std::string build_graphql(const nlohmann::json& message_data);
    std::string build_websocket(const nlohmann::json& message_data);
    std::string build_grpc(const nlohmann::json& message_data);
    std::string build_soap(const nlohmann::json& message_data);

    // Field mapping
    nlohmann::json map_fields(const nlohmann::json& source_data,
                             const std::unordered_map<std::string, std::string>& field_mapping);
    std::string generate_correlation_id();
    std::string generate_message_id();

    // Validation
    bool validate_json_rpc_message(const nlohmann::json& message);
    bool validate_rest_message(const nlohmann::json& message);
    bool validate_grpc_message(const nlohmann::json& message);
    bool validate_soap_message(const nlohmann::json& message);

    // Error handling
    TranslationResultData create_error_result(const std::string& error_message,
                                             TranslationResult result_type = TranslationResult::FAILURE);
    TranslationResultData create_success_result(const std::string& translated_message,
                                               const MessageHeader& header);

    // Database operations
    bool store_translation_rule(const TranslationRule& rule);
    bool update_translation_rule_db(const std::string& rule_id, const TranslationRule& rule);
    bool delete_translation_rule_db(const std::string& rule_id);
    std::vector<TranslationRule> load_translation_rules();
    bool store_protocol_schema(MessageProtocol protocol, const nlohmann::json& schema);
    std::optional<nlohmann::json> load_protocol_schema(MessageProtocol protocol);
    bool store_translation_stats(const std::string& from_protocol, const std::string& to_protocol,
                                TranslationResult result, std::chrono::milliseconds duration);

    // Performance tracking
    void update_translation_metrics(const std::string& from_protocol, const std::string& to_protocol,
                                   TranslationResult result, std::chrono::milliseconds duration);
    std::unordered_map<std::string, int> protocol_usage_counts_;
    std::unordered_map<std::string, std::chrono::milliseconds> average_translation_times_;

    // Utility methods
    std::string protocol_to_string(MessageProtocol protocol);
    std::string message_type_to_string(MessageType type);
    MessageProtocol string_to_protocol(const std::string& protocol_str);
    MessageType string_to_message_type(const std::string& type_str);
};

} // namespace regulens

#endif // MESSAGE_TRANSLATOR_HPP
