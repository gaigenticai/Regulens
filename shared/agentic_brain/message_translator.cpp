/**
 * Message Translator Implementation
 * Protocol conversion between agents (JSON-RPC, gRPC, REST)
 */

#include "message_translator.hpp"
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <spdlog/spdlog.h>
#include <regex>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

namespace regulens {

namespace {

int safe_json_to_int(const nlohmann::json& value, int default_value = 0) {
    try {
        if (value.is_number_integer()) {
            return value.get<int>();
        }
        if (value.is_string()) {
            return std::stoi(value.get<std::string>());
        }
        if (value.is_number()) {
            return static_cast<int>(value.get<double>());
        }
    } catch (const std::exception&) {
        // Ignore and fall back to default
    }
    return default_value;
}

bool safe_json_to_bool(const nlohmann::json& value, bool default_value = false) {
    try {
        if (value.is_boolean()) {
            return value.get<bool>();
        }
        if (value.is_string()) {
            const auto text = value.get<std::string>();
            return text == "true" || text == "1" || text == "TRUE";
        }
        if (value.is_number_integer()) {
            return value.get<int>() != 0;
        }
    } catch (const std::exception&) {
        // Ignore and use default
    }
    return default_value;
}

void append_json_to_xml(xmlNodePtr parent, const std::string& key, const nlohmann::json& value) {
    const xmlChar* node_name = BAD_CAST key.c_str();

    if (value.is_object()) {
        xmlNodePtr object_node = xmlNewChild(parent, nullptr, node_name, nullptr);
        for (const auto& [child_key, child_value] : value.items()) {
            append_json_to_xml(object_node, child_key, child_value);
        }
    } else if (value.is_array()) {
        xmlNodePtr array_node = xmlNewChild(parent, nullptr, node_name, nullptr);
        size_t index = 0;
        for (const auto& item : value) {
            xmlNodePtr item_node = xmlNewChild(array_node, nullptr, BAD_CAST "item", nullptr);
            xmlNewProp(item_node, BAD_CAST "index", BAD_CAST std::to_string(index++).c_str());
            if (item.is_object()) {
                for (const auto& [child_key, child_value] : item.items()) {
                    append_json_to_xml(item_node, child_key, child_value);
                }
            } else if (item.is_array()) {
                append_json_to_xml(item_node, "item", item);
            } else {
                std::string text = item.is_string() ? item.get<std::string>() : item.dump();
                xmlNodeSetContent(item_node, BAD_CAST text.c_str());
            }
        }
    } else {
        std::string text = value.is_string() ? value.get<std::string>() : value.dump();
        xmlNewChild(parent, nullptr, node_name, BAD_CAST text.c_str());
    }
}

nlohmann::json xml_node_to_json(xmlNodePtr node) {
    nlohmann::json result = nlohmann::json::object();

    for (xmlNodePtr child = node->children; child != nullptr; child = child->next) {
        if (child->type != XML_ELEMENT_NODE) {
            continue;
        }

        std::string name = reinterpret_cast<const char*>(child->name);
        nlohmann::json child_value;

        bool has_element_children = false;
        for (xmlNodePtr grand = child->children; grand != nullptr; grand = grand->next) {
            if (grand->type == XML_ELEMENT_NODE) {
                has_element_children = true;
                break;
            }
        }

        if (!has_element_children && child->children != nullptr) {
            xmlChar* content = xmlNodeGetContent(child);
            std::string value_text = content ? reinterpret_cast<const char*>(content) : "";
            if (content) {
                xmlFree(content);
            }
            child_value = value_text;
        } else {
            child_value = xml_node_to_json(child);
        }

        if (child->properties) {
            nlohmann::json attributes = nlohmann::json::object();
            for (xmlAttrPtr attr = child->properties; attr != nullptr; attr = attr->next) {
                xmlChar* value = xmlNodeListGetString(child->doc, attr->children, 1);
                if (value) {
                    attributes[reinterpret_cast<const char*>(attr->name)] = std::string(reinterpret_cast<const char*>(value));
                    xmlFree(value);
                }
            }

            if (!attributes.empty()) {
                if (!child_value.is_object()) {
                    nlohmann::json wrapped;
                    wrapped["value"] = child_value;
                    wrapped["_attributes"] = attributes;
                    child_value = wrapped;
                } else {
                    child_value["_attributes"] = attributes;
                }
            }
        }

        if (result.contains(name)) {
            if (!result[name].is_array()) {
                result[name] = nlohmann::json::array({result[name]});
            }
            result[name].push_back(child_value);
        } else {
            result[name] = child_value;
        }
    }

    return result;
}

std::string extract_graphql_operation_name(const std::string& query_text) {
    static const std::regex operation_regex(R"((query|mutation|subscription)\s+([_A-Za-z][_0-9A-Za-z]*)?)", std::regex::icase);
    std::smatch match;
    if (std::regex_search(query_text, match, operation_regex) && match.size() >= 3) {
        std::string op_name = match[2].str();
        if (!op_name.empty()) {
            return op_name;
        }
    }
    return "";
}

nlohmann::json build_rest_envelope_from_websocket(const nlohmann::json& websocket_message) {
    nlohmann::json envelope;
    envelope["protocol"] = "REST_HTTP";
    envelope["headers"] = websocket_message.value("headers", nlohmann::json::object());
    envelope["metadata"] = {
        {"websocket_type", websocket_message.value("type", std::string("message"))},
        {"channel", websocket_message.value("channel", std::string())},
        {"message_id", websocket_message.value("id", std::string())}
    };

    if (websocket_message.contains("payload")) {
        envelope["body"] = websocket_message["payload"];
    } else if (websocket_message.contains("data")) {
        envelope["body"] = websocket_message["data"];
    } else {
        envelope["body"] = websocket_message;
    }

    if (websocket_message.contains("method")) {
        envelope["method"] = websocket_message["method"];
    }

    return envelope;
}

} // namespace

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

        // Log successful translation to database
        try {
            nlohmann::json source_json = nlohmann::json::parse(message);
            nlohmann::json translated_json = nlohmann::json::parse(translated_str);
            log_translation(header.message_id,
                           protocol_to_string(source_protocol),
                           protocol_to_string(target_protocol),
                           source_json,
                           translated_json,
                           "", // no rule used for built-in translations
                           1.0, // quality_score
                           static_cast<int>(duration.count()),
                           "MessageTranslator",
                           "");
        } catch (const std::exception& e) {
            if (logger_) {
                logger_->warn("Failed to log translation: {}", e.what());
            }
        }

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

        // Log successful translation to database
        try {
            nlohmann::json source_json = nlohmann::json::parse(message);
            nlohmann::json translated_json = nlohmann::json::parse(target_message);
            std::string rule_id = rule ? rule->rule_id : "";
            log_translation(header.message_id,
                           protocol_to_string(source_protocol),
                           protocol_to_string(target_protocol),
                           source_json,
                           translated_json,
                           rule_id,
                           1.0, // quality_score
                           static_cast<int>(duration.count()),
                           "MessageTranslator",
                           "");
        } catch (const std::exception& e) {
            if (logger_) {
                logger_->warn("Failed to log translation: {}", e.what());
            }
        }

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
std::optional<nlohmann::json> MessageTranslator::parse_graphql(const std::string& message) {
    try {
        nlohmann::json payload = nlohmann::json::parse(message);
        nlohmann::json result = nlohmann::json::object();

        if (payload.is_string()) {
            result["query"] = payload.get<std::string>();
        } else {
            if (payload.contains("query")) {
                result["query"] = payload["query"].is_string() ? payload["query"].get<std::string>() : payload["query"].dump();
            } else if (payload.contains("document")) {
                result["query"] = payload["document"].is_string() ? payload["document"].get<std::string>() : payload["document"].dump();
            }

            if (payload.contains("variables")) {
                result["variables"] = payload["variables"];
            }
            if (payload.contains("operationName")) {
                result["operationName"] = payload["operationName"].get<std::string>();
            }
        }

        if (!result.contains("query")) {
            return std::nullopt;
        }

        if (!result.contains("operationName")) {
            result["operationName"] = extract_graphql_operation_name(result["query"].get<std::string>());
        }

        if (!result.contains("variables")) {
            result["variables"] = nlohmann::json::object();
        }

        return result;
    } catch (...) {
        if (message.empty()) {
            return std::nullopt;
        }
        nlohmann::json fallback = {
            {"query", message},
            {"variables", nlohmann::json::object()},
            {"operationName", extract_graphql_operation_name(message)}
        };
        return fallback;
    }
}

std::optional<nlohmann::json> MessageTranslator::parse_websocket(const std::string& message) {
    try {
        nlohmann::json parsed = nlohmann::json::parse(message);
        nlohmann::json result = nlohmann::json::object();

        result["type"] = parsed.value("type", std::string("message"));
        result["channel"] = parsed.value("channel", std::string());
        result["id"] = parsed.value("id", std::string());
        result["headers"] = parsed.value("headers", nlohmann::json::object());

        if (parsed.contains("payload")) {
            result["payload"] = parsed["payload"];
        } else if (parsed.contains("data")) {
            result["payload"] = parsed["data"];
        } else if (parsed.contains("body")) {
            result["payload"] = parsed["body"];
        } else {
            result["payload"] = parsed;
        }

        result["timestamp"] = parsed.value("timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

        return result;
    } catch (...) {
        return std::nullopt;
    }
}

std::string MessageTranslator::rest_to_soap(const std::string& rest_message) {
    try {
        nlohmann::json rest_payload = nlohmann::json::parse(rest_message);

        xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
        xmlNodePtr envelope = xmlNewNode(nullptr, BAD_CAST "soap:Envelope");
        xmlNewProp(envelope, BAD_CAST "xmlns:soap", BAD_CAST "http://www.w3.org/2003/05/soap-envelope");
        xmlDocSetRootElement(doc, envelope);

        xmlNodePtr body = xmlNewChild(envelope, nullptr, BAD_CAST "soap:Body", nullptr);
        xmlNodePtr payload_node = xmlNewChild(body, nullptr, BAD_CAST "RestPayload", nullptr);

        if (rest_payload.contains("method")) {
            std::string method_str = rest_payload["method"].is_string()
                ? rest_payload["method"].get<std::string>()
                : rest_payload["method"].dump();
            xmlNewChild(payload_node, nullptr, BAD_CAST "Method", BAD_CAST method_str.c_str());
        }
        if (rest_payload.contains("headers")) {
            append_json_to_xml(payload_node, "Headers", rest_payload["headers"]);
        }
        if (rest_payload.contains("body")) {
            append_json_to_xml(payload_node, "Body", rest_payload["body"]);
        }

        xmlChar* buffer = nullptr;
        int size = 0;
        xmlDocDumpFormatMemoryEnc(doc, &buffer, &size, "UTF-8", 1);

        std::string soap_message;
        if (buffer && size > 0) {
            soap_message.assign(reinterpret_cast<const char*>(buffer), static_cast<size_t>(size));
        }

        if (buffer) {
            xmlFree(buffer);
        }
        xmlFreeDoc(doc);
        return soap_message;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->warn("Failed to convert REST to SOAP: {}", e.what());
        }
        return rest_message;
    }
}

std::string MessageTranslator::soap_to_rest(const std::string& soap_message) {
    xmlDocPtr doc = xmlReadMemory(soap_message.c_str(), static_cast<int>(soap_message.size()), nullptr, nullptr, XML_PARSE_NOBLANKS);
    if (!doc) {
        return nlohmann::json{{"protocol", "REST_HTTP"}, {"raw_content", soap_message}}.dump();
    }

    xmlXPathContextPtr xpath_context = xmlXPathNewContext(doc);
    nlohmann::json rest_payload = nlohmann::json::object();

    if (xpath_context) {
        xmlXPathRegisterNs(xpath_context, BAD_CAST "soap", BAD_CAST "http://www.w3.org/2003/05/soap-envelope");
        xmlXPathObjectPtr body_result = xmlXPathEvalExpression(BAD_CAST "//soap:Body", xpath_context);
        if (body_result && body_result->nodesetval && body_result->nodesetval->nodeNr > 0) {
            xmlNodePtr body_node = body_result->nodesetval->nodeTab[0];
            rest_payload = xml_node_to_json(body_node);
        }

        if (body_result) {
            xmlXPathFreeObject(body_result);
        }
        xmlXPathFreeContext(xpath_context);
    }

    xmlFreeDoc(doc);

    if (!rest_payload.is_object() || rest_payload.empty()) {
        rest_payload["raw_content"] = soap_message;
    }
    rest_payload["protocol"] = "REST_HTTP";
    return rest_payload.dump();
}

std::string MessageTranslator::websocket_to_rest(const std::string& ws_message) {
    try {
        auto parsed = parse_websocket(ws_message);
        if (!parsed) {
            return nlohmann::json{{"protocol", "REST_HTTP"}, {"raw_content", ws_message}}.dump();
        }
        auto envelope = build_rest_envelope_from_websocket(*parsed);
        return envelope.dump();
    } catch (...) {
        return nlohmann::json{{"protocol", "REST_HTTP"}, {"raw_content", ws_message}}.dump();
    }
}

std::string MessageTranslator::rest_to_websocket(const std::string& rest_message) {
    try {
        nlohmann::json rest_payload = nlohmann::json::parse(rest_message);
        nlohmann::json websocket_message;

        websocket_message["type"] = rest_payload.value("websocket_type", std::string("message"));
        websocket_message["channel"] = rest_payload.value("channel", rest_payload.value("path", std::string()));
        websocket_message["headers"] = rest_payload.value("headers", nlohmann::json::object());
        websocket_message["id"] = rest_payload.value("message_id", generate_message_id());
        websocket_message["payload"] = rest_payload.value("body", rest_payload);
        websocket_message["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        return websocket_message.dump();
    } catch (...) {
        return nlohmann::json{{"type", "message"}, {"payload", rest_message}}.dump();
    }
}

std::string MessageTranslator::build_graphql(const nlohmann::json& message_data) {
    nlohmann::json payload = nlohmann::json::object();
    payload["query"] = message_data.value("query", std::string());
    payload["variables"] = message_data.value("variables", nlohmann::json::object());
    if (payload["query"].get<std::string>().empty() && message_data.contains("document")) {
        payload["query"] = message_data["document"].get<std::string>();
    }
    if (message_data.contains("operationName")) {
        payload["operationName"] = message_data["operationName"].get<std::string>();
    } else {
        payload["operationName"] = extract_graphql_operation_name(payload["query"].get<std::string>());
    }
    payload["extensions"] = message_data.value("extensions", nlohmann::json::object());
    return payload.dump();
}

std::string MessageTranslator::build_websocket(const nlohmann::json& message_data) {
    nlohmann::json websocket_message;
    websocket_message["type"] = message_data.value("type", std::string("message"));
    websocket_message["channel"] = message_data.value("channel", std::string());
    websocket_message["id"] = message_data.value("id", generate_message_id());
    websocket_message["headers"] = message_data.value("headers", nlohmann::json::object());
    websocket_message["payload"] = message_data.value("payload", message_data);
    websocket_message["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return websocket_message.dump();
}

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

// Log translation to database for audit trail
bool MessageTranslator::log_translation(const std::string& message_id,
                                       const std::string& source_protocol,
                                       const std::string& target_protocol,
                                       const nlohmann::json& source_content,
                                       const nlohmann::json& translated_content,
                                       const std::string& rule_id,
                                       double quality_score,
                                       int translation_time_ms,
                                       const std::string& translator_agent,
                                       const std::string& error_message) {
    try {
        if (!db_conn_) {
            if (logger_) {
                logger_->warn("Database connection not available for logging translation");
            }
            return false;
        }

        std::string query = R"(
            INSERT INTO message_translations (
                message_id, source_protocol, target_protocol, source_content, translated_content,
                translation_rule_id, translation_quality, translation_time_ms, translator_agent, error_message
            ) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10)
        )";

        std::vector<std::string> params = {
            message_id,
            source_protocol,
            target_protocol,
            source_content.dump(),
            translated_content.dump(),
            rule_id.empty() ? nullptr : rule_id,
            std::to_string(quality_score),
            std::to_string(translation_time_ms),
            translator_agent,
            error_message
        };

        return db_conn_->execute_command(query, params);

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in log_translation: {}", e.what());
        }
        return false;
    }
}

// Production database operations for translation rules
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
            rule.rule_id = row.value("rule_id", "");
            rule.name = row.value("name", "");
            rule.from_protocol = static_cast<MessageProtocol>(
                safe_json_to_int(row.value("from_protocol", nlohmann::json{}), 0));
            rule.to_protocol = static_cast<MessageProtocol>(
                safe_json_to_int(row.value("to_protocol", nlohmann::json{}), 0));

            try {
                const auto transformation_json = row.value("transformation_rules", nlohmann::json::object());
                if (transformation_json.is_string()) {
                    rule.transformation_rules = nlohmann::json::parse(transformation_json.get<std::string>());
                } else {
                    rule.transformation_rules = transformation_json;
                }
            } catch (const std::exception&) {
                rule.transformation_rules = nlohmann::json::object();
            }

            rule.bidirectional = safe_json_to_bool(row.value("bidirectional", nlohmann::json{}), false);
            rule.priority = safe_json_to_int(row.value("priority", nlohmann::json{}), 0);
            rule.active = safe_json_to_bool(row.value("active", nlohmann::json{}), true);

            rules.push_back(rule);
        }

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in load_translation_rules: {}", e.what());
        }
    }

    return rules;
}

// Translation validation
bool MessageTranslator::validate_translation(const std::string& original_message, const TranslationResultData& translated_result) {
    try {
        // Basic validation checks
        if (translated_result.result == TranslationResult::FAILURE ||
            translated_result.result == TranslationResult::UNSUPPORTED) {
            return false;
        }

        // Check if translated message is not empty
        if (translated_result.translated_message.empty()) {
            return false;
        }

        // Check if there are any critical errors
        for (const auto& error : translated_result.errors) {
            if (error.find("critical") != std::string::npos ||
                error.find("fatal") != std::string::npos) {
                return false;
            }
        }

        // Validate that the translation preserved essential message structure
        // This is a basic implementation - could be enhanced with more sophisticated validation
        try {
            nlohmann::json original_json = nlohmann::json::parse(original_message);
            nlohmann::json translated_json = nlohmann::json::parse(translated_result.translated_message);

            // Check if essential fields are preserved (basic check)
            if (original_json.is_object() && translated_json.is_object()) {
                // For now, just ensure both are valid JSON objects
                return true;
            }
        } catch (const nlohmann::json::parse_error&) {
            // If parsing fails, consider it invalid
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in validate_translation: {}", e.what());
        }
        return false;
    }
}

} // namespace regulens
