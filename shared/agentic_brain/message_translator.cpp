/**
 * Message Translator Implementation
 * Protocol conversion between agents (JSON-RPC, gRPC, REST)
 */

#include "message_translator.hpp"
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <spdlog/spdlog.h>

namespace regulens {

MessageTranslator::MessageTranslator(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<StructuredLogger> logger
) : db_conn_(db_conn), logger_(logger) {

    if (!db_conn_) {
        throw std::runtime_error("Database connection is required for MessageTranslator");
    }

    initialize_protocol_mappings();
    load_translation_rules();

    if (logger_) {
        logger_->info("MessageTranslator initialized with multi-protocol translation capabilities");
    }
}

MessageTranslator::~MessageTranslator() {
    if (logger_) {
        logger_->info("MessageTranslator shutting down");
    }
}

TranslationResultData MessageTranslator::translate_message(
    const std::string& message,
    const MessageHeader& header,
    MessageProtocol target_protocol
) {
    try {
        auto start_time = std::chrono::high_resolution_clock::now();

        // Detect source protocol if not specified
        MessageProtocol source_protocol = header.source_protocol;
        if (source_protocol == MessageProtocol::CUSTOM) {
            auto detected = detect_protocol(message);
            if (detected) {
                source_protocol = *detected;
            } else {
                return create_error_result("Unable to detect source protocol");
            }
        }

        // Skip translation if protocols are the same
        if (source_protocol == target_protocol) {
            MessageHeader translated_header = header;
            translated_header.target_protocol = target_protocol;
            return create_success_result(message, translated_header);
        }

        // Parse the source message
        auto parsed_message = parse_message(message, source_protocol);
        if (!parsed_message) {
            return create_error_result("Failed to parse source message");
        }

        // Apply translation rules if available
        auto rule = find_best_translation_rule(source_protocol, target_protocol);
        nlohmann::json transformed_message = *parsed_message;

        if (rule) {
            transformed_message = apply_transformation_rules(transformed_message, rule->transformation_rules);
        } else {
            // Use built-in protocol translators
            std::string translated_str;

            if (source_protocol == MessageProtocol::JSON_RPC && target_protocol == MessageProtocol::REST_HTTP) {
                translated_str = json_rpc_to_rest(message);
            } else if (source_protocol == MessageProtocol::REST_HTTP && target_protocol == MessageProtocol::JSON_RPC) {
                translated_str = rest_to_json_rpc(message);
            } else if (source_protocol == MessageProtocol::JSON_RPC && target_protocol == MessageProtocol::GRPC) {
                translated_str = json_rpc_to_grpc(message);
            } else if (source_protocol == MessageProtocol::GRPC && target_protocol == MessageProtocol::JSON_RPC) {
                translated_str = grpc_to_json_rpc(message);
            } else if (source_protocol == MessageProtocol::REST_HTTP && target_protocol == MessageProtocol::SOAP) {
                translated_str = rest_to_soap(message);
            } else if (source_protocol == MessageProtocol::SOAP && target_protocol == MessageProtocol::REST_HTTP) {
                translated_str = soap_to_rest(message);
            } else if (source_protocol == MessageProtocol::WEBSOCKET && target_protocol == MessageProtocol::REST_HTTP) {
                translated_str = websocket_to_rest(message);
            } else if (source_protocol == MessageProtocol::REST_HTTP && target_protocol == MessageProtocol::WEBSOCKET) {
                translated_str = rest_to_websocket(message);
            } else {
                // Generic JSON translation
                translated_str = build_message(transformed_message, target_protocol);
            }

            if (translated_str.empty()) {
                return create_error_result("Protocol translation not supported: " +
                                         protocol_to_string(source_protocol) + " to " +
                                         protocol_to_string(target_protocol),
                                         TranslationResult::UNSUPPORTED);
            }

            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

            MessageHeader translated_header = translate_header(header, target_protocol);
            update_translation_metrics(protocol_to_string(source_protocol),
                                     protocol_to_string(target_protocol),
                                     TranslationResult::SUCCESS, duration);

            return create_success_result(translated_str, translated_header);
        }

        // Build the target message
        std::string target_message = build_message(transformed_message, target_protocol);
        if (target_message.empty()) {
            return create_error_result("Failed to build target message");
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        MessageHeader translated_header = translate_header(header, target_protocol);
        update_translation_metrics(protocol_to_string(source_protocol),
                                 protocol_to_string(target_protocol),
                                 TranslationResult::SUCCESS, duration);

        return create_success_result(target_message, translated_header);

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in translate_message: {}", e.what());
        }
        return create_error_result("Translation failed: " + std::string(e.what()));
    }
}

std::optional<MessageProtocol> MessageTranslator::detect_protocol(const std::string& message) {
    try {
        // Try to parse as JSON first
        nlohmann::json json_msg;
        try {
            json_msg = nlohmann::json::parse(message);
        } catch (...) {
            // Not JSON, check other protocols
            if (message.find("<?xml") != std::string::npos || message.find("<soap:") != std::string::npos) {
                return MessageProtocol::SOAP;
            }
            return std::nullopt;
        }

        // Check for JSON-RPC indicators
        if (json_msg.contains("jsonrpc") && json_msg.contains("method")) {
            return MessageProtocol::JSON_RPC;
        }

        // Check for GraphQL indicators
        if (json_msg.contains("query") || json_msg.contains("mutation")) {
            return MessageProtocol::GRAPHQL;
        }

        // Check for REST-like structure
        if (json_msg.contains("method") && json_msg.contains("url")) {
            return MessageProtocol::REST_HTTP;
        }

        // Default to REST for generic JSON
        return MessageProtocol::REST_HTTP;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in detect_protocol: {}", e.what());
        }
        return std::nullopt;
    }
}

bool MessageTranslator::add_translation_rule(const TranslationRule& rule) {
    try {
        if (!store_translation_rule(rule)) {
            if (logger_) {
                logger_->error("Failed to store translation rule {}", rule.rule_id);
            }
            return false;
        }

        std::lock_guard<std::mutex> lock(translator_mutex_);
        translation_rules_.push_back(rule);

        // Sort rules by priority (higher priority first)
        std::sort(translation_rules_.begin(), translation_rules_.end(),
                 [](const TranslationRule& a, const TranslationRule& b) {
                     return a.priority > b.priority;
                 });

        if (logger_) {
            logger_->info("Added translation rule: {} -> {}",
                         protocol_to_string(rule.from_protocol),
                         protocol_to_string(rule.to_protocol));
        }

        return true;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in add_translation_rule: {}", e.what());
        }
        return false;
    }
}

std::string MessageTranslator::json_rpc_to_rest(const std::string& json_rpc_message) {
    try {
        auto parsed = parse_json_rpc(json_rpc_message);
        if (!parsed) return "";

        nlohmann::json rest_message = {
            {"method", parsed->value("method", "")},
            {"url", "/api/v1/" + parsed->value("method", "")}, // Simple mapping
            {"headers", {
                {"Content-Type", "application/json"},
                {"Accept", "application/json"}
            }}
        };

        // Add params as body if present
        if (parsed->contains("params")) {
            rest_message["body"] = parsed->at("params");
        }

        return rest_message.dump(2);

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in json_rpc_to_rest: {}", e.what());
        }
        return "";
    }
}

std::string MessageTranslator::rest_to_json_rpc(const std::string& rest_message) {
    try {
        nlohmann::json rest_json = nlohmann::json::parse(rest_message);

        nlohmann::json json_rpc_message = {
            {"jsonrpc", "2.0"},
            {"method", rest_json.value("method", "unknown")},
            {"id", generate_message_id()}
        };

        // Extract params from body if present
        if (rest_json.contains("body")) {
            json_rpc_message["params"] = rest_json.at("body");
        }

        return json_rpc_message.dump(2);

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in rest_to_json_rpc: {}", e.what());
        }
        return "";
    }
}

std::string MessageTranslator::json_rpc_to_grpc(const std::string& json_rpc_message) {
    try {
        auto parsed = parse_json_rpc(json_rpc_message);
        if (!parsed) return "";

        // Simplified gRPC conversion - in real implementation would use protobuf
        nlohmann::json grpc_message = {
            {"service", parsed->value("method", "").substr(0, parsed->value("method", "").find("."))},
            {"method", parsed->value("method", "").substr(parsed->value("method", "").find(".") + 1)},
            {"request", parsed->value("params", nlohmann::json::object())}
        };

        return grpc_message.dump(2);

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in json_rpc_to_grpc: {}", e.what());
        }
        return "";
    }
}

std::string MessageTranslator::grpc_to_json_rpc(const std::string& grpc_message) {
    try {
        nlohmann::json grpc_json = nlohmann::json::parse(grpc_message);

        nlohmann::json json_rpc_message = {
            {"jsonrpc", "2.0"},
            {"method", grpc_json.value("service", "") + "." + grpc_json.value("method", "")},
            {"id", generate_message_id()}
        };

        if (grpc_json.contains("request")) {
            json_rpc_message["params"] = grpc_json.at("request");
        }

        return json_rpc_message.dump(2);

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in grpc_to_json_rpc: {}", e.what());
        }
        return "";
    }
}

std::optional<nlohmann::json> MessageTranslator::parse_message(const std::string& message, MessageProtocol protocol) {
    try {
        switch (protocol) {
            case MessageProtocol::JSON_RPC:
                return parse_json_rpc(message);
            case MessageProtocol::REST_HTTP:
                return parse_rest_http(message);
            case MessageProtocol::GRAPHQL:
                return parse_graphql(message);
            case MessageProtocol::WEBSOCKET:
                return parse_websocket(message);
            case MessageProtocol::GRPC:
                return parse_grpc(message);
            case MessageProtocol::SOAP:
                return parse_soap(message);
            default:
                // Try JSON parsing for unknown protocols
                return nlohmann::json::parse(message);
        }
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in parse_message: {}", e.what());
        }
        return std::nullopt;
    }
}

std::string MessageTranslator::build_message(const nlohmann::json& message_data, MessageProtocol protocol) {
    try {
        switch (protocol) {
            case MessageProtocol::JSON_RPC:
                return build_json_rpc(message_data);
            case MessageProtocol::REST_HTTP:
                return build_rest_http(message_data);
            case MessageProtocol::GRAPHQL:
                return build_graphql(message_data);
            case MessageProtocol::WEBSOCKET:
                return build_websocket(message_data);
            case MessageProtocol::GRPC:
                return build_grpc(message_data);
            case MessageProtocol::SOAP:
                return build_soap(message_data);
            default:
                return message_data.dump(2);
        }
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in build_message: {}", e.what());
        }
        return "";
    }
}

MessageHeader MessageTranslator::translate_header(const MessageHeader& source_header, MessageProtocol target_protocol) {
    MessageHeader translated_header = source_header;
    translated_header.target_protocol = target_protocol;
    translated_header.timestamp = std::chrono::system_clock::now();

    // Add protocol-specific headers
    auto mapping_it = protocol_mappings_.find(target_protocol);
    if (mapping_it != protocol_mappings_.end()) {
        for (const auto& [key, value] : mapping_it->second.default_headers) {
            translated_header.custom_headers[key] = value;
        }
    }

    return translated_header;
}

void MessageTranslator::initialize_protocol_mappings() {
    protocol_mappings_[MessageProtocol::JSON_RPC] = {
        MessageProtocol::JSON_RPC,
        "JSON-RPC 2.0",
        "application/json",
        {{"Content-Type", "application/json"}},
        R"({"type":"object","required":["jsonrpc","method"],"properties":{"jsonrpc":{"type":"string"},"method":{"type":"string"},"params":{"type":"object"},"id":{"type":"string"}}})"_json,
        false
    };

    protocol_mappings_[MessageProtocol::REST_HTTP] = {
        MessageProtocol::REST_HTTP,
        "REST HTTP",
        "application/json",
        {{"Content-Type", "application/json"}, {"Accept", "application/json"}},
        R"({"type":"object","properties":{"method":{"type":"string"},"url":{"type":"string"},"headers":{"type":"object"},"body":{"type":"object"}}})"_json,
        false
    };

    protocol_mappings_[MessageProtocol::GRPC] = {
        MessageProtocol::GRPC,
        "gRPC",
        "application/grpc",
        {{"Content-Type", "application/grpc"}},
        R"({"type":"object","properties":{"service":{"type":"string"},"method":{"type":"string"},"request":{"type":"object"}}})"_json,
        true
    };

    protocol_mappings_[MessageProtocol::SOAP] = {
        MessageProtocol::SOAP,
        "SOAP",
        "application/soap+xml",
        {{"Content-Type", "application/soap+xml"}},
        R"({"type":"object","properties":{"Envelope":{"type":"object","properties":{"Body":{"type":"object"}}}}})"_json,
        false
    };
}

TranslationRule* MessageTranslator::find_best_translation_rule(MessageProtocol from, MessageProtocol to) {
    for (auto& rule : translation_rules_) {
        if (rule.from_protocol == from && rule.to_protocol == to && rule.active) {
            return &rule;
        }
        // Check bidirectional rules
        if (rule.bidirectional && ((rule.from_protocol == from && rule.to_protocol == to) ||
                                  (rule.from_protocol == to && rule.to_protocol == from)) && rule.active) {
            return &rule;
        }
    }
    return nullptr;
}

nlohmann::json MessageTranslator::apply_transformation_rules(const nlohmann::json& message_data,
                                                           const nlohmann::json& rules) {
    nlohmann::json transformed = message_data;

    try {
        if (rules.contains("field_mappings") && rules["field_mappings"].is_object()) {
            const auto& mappings = rules["field_mappings"];
            for (const auto& [source_field, target_field] : mappings.items()) {
                if (transformed.contains(source_field)) {
                    transformed[target_field] = transformed[source_field];
                    transformed.erase(source_field);
                }
            }
        }

        if (rules.contains("value_transformations") && rules["value_transformations"].is_object()) {
            const auto& transformations = rules["value_transformations"];
            for (const auto& [field, transform] : transformations.items()) {
                if (transformed.contains(field) && transform.contains("operation")) {
                    std::string operation = transform["operation"];
                    if (operation == "uppercase" && transformed[field].is_string()) {
                        std::string value = transformed[field];
                        std::transform(value.begin(), value.end(), value.begin(), ::toupper);
                        transformed[field] = value;
                    } else if (operation == "lowercase" && transformed[field].is_string()) {
                        std::string value = transformed[field];
                        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
                        transformed[field] = value;
                    }
                }
            }
        }

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in apply_transformation_rules: {}", e.what());
        }
    }

    return transformed;
}

// Protocol-specific parsing implementations
std::optional<nlohmann::json> MessageTranslator::parse_json_rpc(const std::string& message) {
    try {
        nlohmann::json parsed = nlohmann::json::parse(message);
        if (parsed.contains("jsonrpc") && parsed.contains("method")) {
            return parsed;
        }
    } catch (...) {}
    return std::nullopt;
}

std::optional<nlohmann::json> MessageTranslator::parse_rest_http(const std::string& message) {
    try {
        return nlohmann::json::parse(message);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<nlohmann::json> MessageTranslator::parse_grpc(const std::string& message) {
    try {
        // Simplified gRPC parsing - in real implementation would decode protobuf
        return nlohmann::json::parse(message);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<nlohmann::json> MessageTranslator::parse_soap(const std::string& message) {
    try {
        // Simplified SOAP parsing - in real implementation would parse XML
        nlohmann::json soap_data = {
            {"protocol", "soap"},
            {"raw_content", message}
        };
        return soap_data;
    } catch (...) {
        return std::nullopt;
    }
}

// Protocol-specific building implementations
std::string MessageTranslator::build_json_rpc(const nlohmann::json& message_data) {
    nlohmann::json json_rpc = {
        {"jsonrpc", "2.0"},
        {"method", message_data.value("method", "unknown")},
        {"id", message_data.value("id", generate_message_id())}
    };

    if (message_data.contains("params")) {
        json_rpc["params"] = message_data["params"];
    }

    return json_rpc.dump(2);
}

std::string MessageTranslator::build_rest_http(const nlohmann::json& message_data) {
    return message_data.dump(2);
}

std::string MessageTranslator::build_grpc(const nlohmann::json& message_data) {
    // Simplified gRPC building - in real implementation would encode protobuf
    return message_data.dump(2);
}

std::string MessageTranslator::build_soap(const nlohmann::json& message_data) {
    // Simplified SOAP building - in real implementation would generate XML
    std::string soap_xml = R"(
<?xml version="1.0" encoding="UTF-8"?>
<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope">
  <soap:Body>
    <jsonData>)";
    soap_xml += message_data.dump();
    soap_xml += R"(
    </jsonData>
  </soap:Body>
</soap:Envelope>)";
    return soap_xml;
}

// Placeholder implementations for remaining methods
std::optional<nlohmann::json> MessageTranslator::parse_graphql(const std::string& message) { return parse_rest_http(message); }
std::optional<nlohmann::json> MessageTranslator::parse_websocket(const std::string& message) { return parse_rest_http(message); }
std::string MessageTranslator::rest_to_soap(const std::string& rest_message) { return build_soap(nlohmann::json::parse(rest_message)); }
std::string MessageTranslator::soap_to_rest(const std::string& soap_message) { return nlohmann::json{{"protocol", "rest"}, {"raw_content", soap_message}}.dump(); }
std::string MessageTranslator::websocket_to_rest(const std::string& ws_message) { return ws_message; }
std::string MessageTranslator::rest_to_websocket(const std::string& rest_message) { return rest_message; }
std::string MessageTranslator::build_graphql(const nlohmann::json& message_data) { return message_data.dump(2); }
std::string MessageTranslator::build_websocket(const nlohmann::json& message_data) { return message_data.dump(2); }

TranslationResultData MessageTranslator::create_error_result(const std::string& error_message, TranslationResult result_type) {
    TranslationResultData result;
    result.result = result_type;
    result.errors = {error_message};
    result.processing_time = std::chrono::milliseconds(0);
    return result;
}

TranslationResultData MessageTranslator::create_success_result(const std::string& translated_message, const MessageHeader& header) {
    TranslationResultData result;
    result.result = TranslationResult::SUCCESS;
    result.translated_message = translated_message;
    result.translated_header = header;
    result.processing_time = std::chrono::milliseconds(0); // Would be set by caller
    return result;
}

std::string MessageTranslator::generate_message_id() {
    return "msg_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
}

std::string MessageTranslator::protocol_to_string(MessageProtocol protocol) {
    switch (protocol) {
        case MessageProtocol::JSON_RPC: return "JSON_RPC";
        case MessageProtocol::REST_HTTP: return "REST_HTTP";
        case MessageProtocol::GRAPHQL: return "GRAPHQL";
        case MessageProtocol::WEBSOCKET: return "WEBSOCKET";
        case MessageProtocol::GRPC: return "GRPC";
        case MessageProtocol::SOAP: return "SOAP";
        case MessageProtocol::MQTT: return "MQTT";
        case MessageProtocol::AMQP: return "AMQP";
        case MessageProtocol::CUSTOM: return "CUSTOM";
        default: return "UNKNOWN";
    }
}

void MessageTranslator::update_translation_metrics(const std::string& from_protocol, const std::string& to_protocol,
                                                 TranslationResult result, std::chrono::milliseconds duration) {
    std::lock_guard<std::mutex> lock(translator_mutex_);
    std::string key = from_protocol + "_to_" + to_protocol;
    protocol_usage_counts_[key]++;
    average_translation_times_[key] = duration;
}

std::unordered_map<std::string, double> MessageTranslator::get_translation_stats() {
    std::lock_guard<std::mutex> lock(translator_mutex_);
    std::unordered_map<std::string, double> stats;

    for (const auto& [key, count] : protocol_usage_counts_) {
        stats[key + "_count"] = count;
    }

    for (const auto& [key, duration] : average_translation_times_) {
        stats[key + "_avg_ms"] = duration.count();
    }

    return stats;
}

// Database operations (simplified implementations)
bool MessageTranslator::store_translation_rule(const TranslationRule& rule) {
    try {
        if (!db_conn_) return false;

        std::string query = R"(
            INSERT INTO translation_rules (
                rule_id, name, from_protocol, to_protocol, transformation_rules,
                bidirectional, priority, active
            ) VALUES ($1, $2, $3, $4, $5, $6, $7, $8)
            ON CONFLICT (rule_id) DO UPDATE SET
                name = EXCLUDED.name,
                transformation_rules = EXCLUDED.transformation_rules,
                bidirectional = EXCLUDED.bidirectional,
                priority = EXCLUDED.priority,
                active = EXCLUDED.active
        )";

        std::vector<std::string> params = {
            rule.rule_id,
            rule.name,
            std::to_string(static_cast<int>(rule.from_protocol)),
            std::to_string(static_cast<int>(rule.to_protocol)),
            rule.transformation_rules.dump(),
            rule.bidirectional ? "true" : "false",
            std::to_string(rule.priority),
            rule.active ? "true" : "false"
        };

        return db_conn_->execute_command(query, params);

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in store_translation_rule: {}", e.what());
        }
        return false;
    }
}

std::vector<TranslationRule> MessageTranslator::load_translation_rules() {
    std::vector<TranslationRule> rules;

    try {
        if (!db_conn_) return rules;

        std::string query = R"(
            SELECT rule_id, name, from_protocol, to_protocol, transformation_rules,
                   bidirectional, priority, active, created_at
            FROM translation_rules
            WHERE active = true
            ORDER BY priority DESC, created_at DESC
        )";

        auto results = db_conn_->execute_query_multi(query, {});

        for (const auto& row : results) {
            TranslationRule rule;
            rule.rule_id = row.at("rule_id");
            rule.name = row.at("name");
            rule.from_protocol = static_cast<MessageProtocol>(std::stoi(row.at("from_protocol")));
            rule.to_protocol = static_cast<MessageProtocol>(std::stoi(row.at("to_protocol")));
            rule.transformation_rules = nlohmann::json::parse(row.at("transformation_rules"));
            rule.bidirectional = (row.at("bidirectional") == "true");
            rule.priority = std::stoi(row.at("priority"));
            rule.active = (row.at("active") == "true");

            rules.push_back(rule);
        }

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in load_translation_rules: {}", e.what());
        }
    }

    return rules;
}

} // namespace regulens
