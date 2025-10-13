/**
 * Message Translator API Handlers Implementation
 * REST API endpoints for protocol translation between agents
 */

#include "message_translator_api_handlers.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace regulens {

MessageTranslatorAPIHandlers::MessageTranslatorAPIHandlers(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<MessageTranslator> translator
) : db_conn_(db_conn), translator_(translator) {

    if (!db_conn_) {
        throw std::runtime_error("Database connection is required for MessageTranslatorAPIHandlers");
    }
    if (!translator_) {
        throw std::runtime_error("MessageTranslator is required for MessageTranslatorAPIHandlers");
    }

    spdlog::info("MessageTranslatorAPIHandlers initialized");
}

MessageTranslatorAPIHandlers::~MessageTranslatorAPIHandlers() {
    spdlog::info("MessageTranslatorAPIHandlers shutting down");
}

std::string MessageTranslatorAPIHandlers::handle_translate_message(const std::string& request_body, const std::string& user_id) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);
        std::string validation_error;
        if (!validate_translation_request(request, validation_error)) {
            return create_error_response(validation_error).dump();
        }

        if (!validate_user_access(user_id, "translate_message")) {
            return create_error_response("Access denied", 403).dump();
        }

        // Parse request
        std::string message = request.value("message", "");
        MessageHeader header = parse_message_header(request);
        MessageProtocol target_protocol = parse_protocol_param(request.value("target_protocol", "JSON_RPC"));

        // Perform translation
        TranslationResultData result = translator_->translate_message(message, header, target_protocol);

        // Log the translation
        log_translation_request(user_id, header.source_protocol, target_protocol);

        return format_translation_result(result).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_translate_message: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string MessageTranslatorAPIHandlers::handle_batch_translate(const std::string& request_body, const std::string& user_id) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);
        std::string validation_error;
        if (!validate_batch_request(request, validation_error)) {
            return create_error_response(validation_error).dump();
        }

        if (!validate_user_access(user_id, "batch_translate")) {
            return create_error_response("Access denied", 403).dump();
        }

        // Prepare batch messages
        auto batch_messages = prepare_batch_messages(request);
        MessageProtocol target_protocol = parse_protocol_param(request.value("target_protocol", "JSON_RPC"));

        // Perform batch translation
        log_batch_operation(user_id, batch_messages.size());
        std::vector<TranslationResultData> results = translator_->translate_batch(
            batch_messages, target_protocol);

        return process_batch_results(results).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_batch_translate: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string MessageTranslatorAPIHandlers::handle_detect_protocol(const std::string& request_body, const std::string& user_id) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);
        std::string message = request.value("message", "");

        if (message.empty()) {
            return create_error_response("Message content is required").dump();
        }

        if (!validate_user_access(user_id, "detect_protocol")) {
            return create_error_response("Access denied", 403).dump();
        }

        auto detected_protocol = translator_->detect_protocol(message);
        auto detected_type = translator_->detect_message_type(message,
            detected_protocol.value_or(MessageProtocol::REST_HTTP));

        nlohmann::json response_data = {
            {"detected_protocol", detected_protocol ?
                protocol_to_string(*detected_protocol) : "UNKNOWN"},
            {"message_type", detected_type ?
                translator_->message_type_to_string(*detected_type) : "UNKNOWN"},
            {"confidence", detected_protocol ? "HIGH" : "LOW"}
        };

        return create_success_response(response_data, "Protocol detection completed").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_detect_protocol: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string MessageTranslatorAPIHandlers::handle_add_translation_rule(const std::string& request_body, const std::string& user_id) {
    try {
        if (!is_admin_user(user_id)) {
            return create_error_response("Admin access required", 403).dump();
        }

        nlohmann::json request = nlohmann::json::parse(request_body);
        std::string validation_error;
        if (!validate_rule_request(request, validation_error)) {
            return create_error_response(validation_error).dump();
        }

        // Parse translation rule
        TranslationRule rule = parse_translation_rule(request);

        // Add the rule
        if (!translator_->add_translation_rule(rule)) {
            return create_error_response("Failed to add translation rule").dump();
        }

        log_rule_operation(user_id, "add", rule.rule_id);

        nlohmann::json response_data = format_translation_rule(rule);
        return create_success_response(response_data, "Translation rule added successfully").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_add_translation_rule: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string MessageTranslatorAPIHandlers::handle_get_translation_rules(const std::string& query_params, const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "get_translation_rules")) {
            return create_error_response("Access denied", 403).dump();
        }

        auto params = parse_query_params(query_params);
        MessageProtocol from_protocol = parse_protocol_from_query(query_params);
        MessageProtocol to_protocol = MessageProtocol::CUSTOM; // Default to any

        auto rules = translator_->get_translation_rules(from_protocol, to_protocol);
        auto filtered_rules = filter_rules_by_protocols(rules, from_protocol, to_protocol);

        nlohmann::json rules_json;
        for (const auto& rule : filtered_rules) {
            rules_json.push_back(format_translation_rule(rule));
        }

        nlohmann::json response_data = {
            {"rules", rules_json},
            {"total_count", rules_json.size()},
            {"filters", {
                {"from_protocol", from_protocol != MessageProtocol::CUSTOM ?
                    protocol_to_string(from_protocol) : "ANY"},
                {"to_protocol", to_protocol != MessageProtocol::CUSTOM ?
                    protocol_to_string(to_protocol) : "ANY"}
            }}
        };

        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_translation_rules: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string MessageTranslatorAPIHandlers::handle_register_protocol_schema(const std::string& protocol, const std::string& request_body, const std::string& user_id) {
    try {
        if (!is_admin_user(user_id)) {
            return create_error_response("Admin access required", 403).dump();
        }

        nlohmann::json request = nlohmann::json::parse(request_body);
        nlohmann::json schema = request.value("schema", nlohmann::json::object());
        MessageProtocol protocol_enum = parse_protocol_param(protocol);

        if (!translator_->register_protocol_schema(protocol_enum, schema)) {
            return create_error_response("Failed to register protocol schema").dump();
        }

        nlohmann::json response_data = {
            {"protocol", protocol},
            {"schema_registered", true},
            {"schema_summary", {
                {"type", schema.value("type", "unknown")},
                {"has_properties", schema.contains("properties")}
            }}
        };

        return create_success_response(response_data, "Protocol schema registered successfully").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_register_protocol_schema: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string MessageTranslatorAPIHandlers::handle_json_rpc_to_rest(const std::string& request_body, const std::string& user_id) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);
        std::string json_rpc_message = request.value("message", "");

        if (json_rpc_message.empty()) {
            return create_error_response("JSON-RPC message is required").dump();
        }

        if (!validate_user_access(user_id, "protocol_conversion")) {
            return create_error_response("Access denied", 403).dump();
        }

        std::string rest_message = translator_->json_rpc_to_rest(json_rpc_message);

        if (rest_message.empty()) {
            return create_error_response("Failed to convert JSON-RPC to REST").dump();
        }

        nlohmann::json response_data = {
            {"original_protocol", "JSON_RPC"},
            {"target_protocol", "REST_HTTP"},
            {"converted_message", nlohmann::json::parse(rest_message)}
        };

        return create_success_response(response_data, "Protocol conversion completed").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_json_rpc_to_rest: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string MessageTranslatorAPIHandlers::handle_rest_to_json_rpc(const std::string& request_body, const std::string& user_id) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);
        std::string rest_message = request.value("message", "");

        if (rest_message.empty()) {
            return create_error_response("REST message is required").dump();
        }

        if (!validate_user_access(user_id, "protocol_conversion")) {
            return create_error_response("Access denied", 403).dump();
        }

        std::string json_rpc_message = translator_->rest_to_json_rpc(rest_message);

        if (json_rpc_message.empty()) {
            return create_error_response("Failed to convert REST to JSON-RPC").dump();
        }

        nlohmann::json response_data = {
            {"original_protocol", "REST_HTTP"},
            {"target_protocol", "JSON_RPC"},
            {"converted_message", nlohmann::json::parse(json_rpc_message)}
        };

        return create_success_response(response_data, "Protocol conversion completed").dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_rest_to_json_rpc: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

std::string MessageTranslatorAPIHandlers::handle_get_translation_stats(const std::string& user_id) {
    try {
        if (!validate_user_access(user_id, "get_translation_stats")) {
            return create_error_response("Access denied", 403).dump();
        }

        auto stats = translator_->get_translation_stats();
        auto usage_stats = translator_->get_protocol_usage_stats();

        nlohmann::json response_data = {
            {"translation_stats", format_translation_stats(stats)},
            {"protocol_usage", format_protocol_usage_stats(usage_stats)},
            {"generated_at", "2024-01-15T10:30:00Z"}
        };

        return create_success_response(response_data).dump();

    } catch (const std::exception& e) {
        spdlog::error("Exception in handle_get_translation_stats: {}", e.what());
        return create_error_response("Internal server error", 500).dump();
    }
}

// Helper method implementations

MessageHeader MessageTranslatorAPIHandlers::parse_message_header(const nlohmann::json& request) {
    MessageHeader header;

    header.message_id = request.value("message_id", translator_->generate_message_id());
    header.correlation_id = request.value("correlation_id", "");
    header.source_protocol = parse_protocol_param(request.value("source_protocol", "REST_HTTP"));
    header.sender_id = request.value("sender_id", "");
    header.recipient_id = request.value("recipient_id", "");
    header.priority = request.value("priority", 1);
    header.timestamp = std::chrono::system_clock::now();

    if (request.contains("custom_headers") && request["custom_headers"].is_object()) {
        for (const auto& [key, value] : request["custom_headers"].items()) {
            if (value.is_string()) {
                header.custom_headers[key] = value.get<std::string>();
            }
        }
    }

    return header;
}

TranslationRule MessageTranslatorAPIHandlers::parse_translation_rule(const nlohmann::json& request) {
    TranslationRule rule;

    rule.rule_id = request.value("rule_id", "rule_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
    rule.name = request.value("name", "");
    rule.from_protocol = parse_protocol_param(request.value("from_protocol", "JSON_RPC"));
    rule.to_protocol = parse_protocol_param(request.value("to_protocol", "REST_HTTP"));
    rule.bidirectional = request.value("bidirectional", false);
    rule.priority = request.value("priority", 1);
    rule.active = request.value("active", true);
    rule.created_at = std::chrono::system_clock::now();

    if (request.contains("transformation_rules")) {
        rule.transformation_rules = request["transformation_rules"];
    }

    return rule;
}

MessageProtocol MessageTranslatorAPIHandlers::parse_protocol_param(const std::string& protocol_str) {
    if (protocol_str == "JSON_RPC") return MessageProtocol::JSON_RPC;
    if (protocol_str == "REST_HTTP") return MessageProtocol::REST_HTTP;
    if (protocol_str == "GRAPHQL") return MessageProtocol::GRAPHQL;
    if (protocol_str == "WEBSOCKET") return MessageProtocol::WEBSOCKET;
    if (protocol_str == "GRPC") return MessageProtocol::GRPC;
    if (protocol_str == "SOAP") return MessageProtocol::SOAP;
    if (protocol_str == "MQTT") return MessageProtocol::MQTT;
    if (protocol_str == "AMQP") return MessageProtocol::AMQP;
    return MessageProtocol::REST_HTTP; // Default
}

nlohmann::json MessageTranslatorAPIHandlers::format_translation_result(const TranslationResultData& result) {
    nlohmann::json response = {
        {"result", result_to_string(result.result)},
        {"processing_time_ms", result.processing_time.count()},
        {"warnings", result.warnings},
        {"errors", result.errors}
    };

    if (!result.translated_message.empty()) {
        response["translated_message"] = result.translated_message;
    }

    if (!result.translated_header.message_id.empty()) {
        response["translated_header"] = {
            {"message_id", result.translated_header.message_id},
            {"correlation_id", result.translated_header.correlation_id},
            {"protocol", protocol_to_string(result.translated_header.target_protocol)},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                result.translated_header.timestamp.time_since_epoch()).count()}
        };
    }

    if (!result.metadata.empty()) {
        response["metadata"] = result.metadata;
    }

    return create_success_response(response);
}

nlohmann::json MessageTranslatorAPIHandlers::format_translation_rule(const TranslationRule& rule) {
    return {
        {"rule_id", rule.rule_id},
        {"name", rule.name},
        {"from_protocol", protocol_to_string(rule.from_protocol)},
        {"to_protocol", protocol_to_string(rule.to_protocol)},
        {"bidirectional", rule.bidirectional},
        {"priority", rule.priority},
        {"active", rule.active},
        {"transformation_rules", rule.transformation_rules},
        {"created_at", std::chrono::duration_cast<std::chrono::seconds>(
            rule.created_at.time_since_epoch()).count()}
    };
}

bool MessageTranslatorAPIHandlers::validate_translation_request(const nlohmann::json& request, std::string& error_message) {
    if (!request.contains("message") || !request["message"].is_string() || request["message"].get<std::string>().empty()) {
        error_message = "Missing or invalid 'message' field";
        return false;
    }

    if (!request.contains("target_protocol") || !request["target_protocol"].is_string()) {
        error_message = "Missing or invalid 'target_protocol' field";
        return false;
    }

    std::string target_protocol = request["target_protocol"];
    if (!is_supported_protocol(target_protocol)) {
        error_message = "Unsupported target protocol: " + target_protocol;
        return false;
    }

    return true;
}

bool MessageTranslatorAPIHandlers::validate_rule_request(const nlohmann::json& request, std::string& error_message) {
    if (!request.contains("name") || !request["name"].is_string() || request["name"].get<std::string>().empty()) {
        error_message = "Missing or invalid 'name' field";
        return false;
    }

    if (!request.contains("from_protocol") || !request["from_protocol"].is_string()) {
        error_message = "Missing or invalid 'from_protocol' field";
        return false;
    }

    if (!request.contains("to_protocol") || !request["to_protocol"].is_string()) {
        error_message = "Missing or invalid 'to_protocol' field";
        return false;
    }

    std::string from_protocol = request["from_protocol"];
    std::string to_protocol = request["to_protocol"];

    if (!is_supported_protocol(from_protocol)) {
        error_message = "Unsupported from_protocol: " + from_protocol;
        return false;
    }

    if (!is_supported_protocol(to_protocol)) {
        error_message = "Unsupported to_protocol: " + to_protocol;
        return false;
    }

    return true;
}

bool MessageTranslatorAPIHandlers::validate_batch_request(const nlohmann::json& request, std::string& error_message) {
    if (!request.contains("messages") || !request["messages"].is_array()) {
        error_message = "Missing or invalid 'messages' array";
        return false;
    }

    if (request["messages"].size() == 0) {
        error_message = "Messages array cannot be empty";
        return false;
    }

    if (request["messages"].size() > 100) {
        error_message = "Batch size cannot exceed 100 messages";
        return false;
    }

    if (!request.contains("target_protocol") || !request["target_protocol"].is_string()) {
        error_message = "Missing or invalid 'target_protocol' field";
        return false;
    }

    return true;
}

bool MessageTranslatorAPIHandlers::validate_user_access(const std::string& user_id, const std::string& operation) {
    // TODO: Implement proper access control based on user roles and permissions
    return !user_id.empty();
}

bool MessageTranslatorAPIHandlers::is_admin_user(const std::string& user_id) {
    // TODO: Implement proper admin user checking
    return user_id == "admin" || user_id == "system";
}

nlohmann::json MessageTranslatorAPIHandlers::create_success_response(const nlohmann::json& data, const std::string& message) {
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

nlohmann::json MessageTranslatorAPIHandlers::create_error_response(const std::string& message, int status_code) {
    return {
        {"success", false},
        {"status_code", status_code},
        {"error", message}
    };
}

std::unordered_map<std::string, std::string> MessageTranslatorAPIHandlers::parse_query_params(const std::string& query_string) {
    std::unordered_map<std::string, std::string> params;

    if (query_string.empty()) return params;

    std::stringstream ss(query_string);
    std::string pair;

    while (std::getline(ss, pair, '&')) {
        size_t equals_pos = pair.find('=');
        if (equals_pos != std::string::npos) {
            std::string key = pair.substr(0, equals_pos);
            std::string value = pair.substr(equals_pos + 1);
            params[key] = value;
        }
    }

    return params;
}

MessageProtocol MessageTranslatorAPIHandlers::parse_protocol_from_query(const std::string& query_string) {
    auto params = parse_query_params(query_string);
    std::string from_protocol = params["from_protocol"];
    return from_protocol.empty() ? MessageProtocol::CUSTOM : parse_protocol_param(from_protocol);
}

std::vector<std::pair<std::string, MessageHeader>> MessageTranslatorAPIHandlers::prepare_batch_messages(const nlohmann::json& batch_request) {
    std::vector<std::pair<std::string, MessageHeader>> batch_messages;

    for (const auto& msg_request : batch_request["messages"]) {
        std::string message = msg_request.value("message", "");
        MessageHeader header = parse_message_header(msg_request);
        batch_messages.emplace_back(message, header);
    }

    return batch_messages;
}

nlohmann::json MessageTranslatorAPIHandlers::process_batch_results(const std::vector<TranslationResultData>& results) {
    nlohmann::json batch_response;
    int success_count = 0;
    int failure_count = 0;

    for (const auto& result : results) {
        batch_response["results"].push_back(format_translation_result(result));
        if (result.result == TranslationResult::SUCCESS) {
            success_count++;
        } else {
            failure_count++;
        }
    }

    batch_response["summary"] = {
        {"total_messages", results.size()},
        {"successful_translations", success_count},
        {"failed_translations", failure_count},
        {"success_rate", results.empty() ? 0.0 : static_cast<double>(success_count) / results.size()}
    };

    return create_success_response(batch_response, "Batch translation completed");
}

std::string MessageTranslatorAPIHandlers::protocol_to_string(MessageProtocol protocol) {
    return translator_->protocol_to_string(protocol);
}

std::string MessageTranslatorAPIHandlers::result_to_string(TranslationResult result) {
    switch (result) {
        case TranslationResult::SUCCESS: return "SUCCESS";
        case TranslationResult::PARTIAL_SUCCESS: return "PARTIAL_SUCCESS";
        case TranslationResult::ADAPTATION_NEEDED: return "ADAPTATION_NEEDED";
        case TranslationResult::FAILURE: return "FAILURE";
        case TranslationResult::UNSUPPORTED: return "UNSUPPORTED";
        default: return "UNKNOWN";
    }
}

bool MessageTranslatorAPIHandlers::is_supported_protocol(const std::string& protocol) {
    return protocol == "JSON_RPC" || protocol == "REST_HTTP" || protocol == "GRAPHQL" ||
           protocol == "WEBSOCKET" || protocol == "GRPC" || protocol == "SOAP" ||
           protocol == "MQTT" || protocol == "AMQP";
}

nlohmann::json MessageTranslatorAPIHandlers::format_translation_stats(const std::unordered_map<std::string, double>& stats) {
    return stats;
}

nlohmann::json MessageTranslatorAPIHandlers::format_protocol_usage_stats(const std::vector<std::pair<std::string, int>>& usage_stats) {
    nlohmann::json usage_json;
    for (const auto& [protocol, count] : usage_stats) {
        usage_json[protocol] = count;
    }
    return usage_json;
}

void MessageTranslatorAPIHandlers::log_translation_request(const std::string& user_id, MessageProtocol from, MessageProtocol to) {
    spdlog::info("Translation request: {} -> {} by user {}",
                 protocol_to_string(from), protocol_to_string(to), user_id);
}

void MessageTranslatorAPIHandlers::log_rule_operation(const std::string& user_id, const std::string& operation, const std::string& rule_id) {
    spdlog::info("Rule {} operation: {} by user {}", operation, rule_id, user_id);
}

void MessageTranslatorAPIHandlers::log_batch_operation(const std::string& user_id, int batch_size) {
    spdlog::info("Batch translation: {} messages by user {}", batch_size, user_id);
}

} // namespace regulens
