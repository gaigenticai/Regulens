#include "enterprise_connectors.hpp"
#include "tool_categories.hpp"
#include "../../logging/structured_logger.hpp"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <regex>
#include <utility>
#include <cstdlib>
#include <unordered_set>
#include <vector>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <openssl/evp.h>

namespace regulens {

namespace {

std::string normalize_base_url(std::string base_url) {
    if (base_url.empty()) {
        return base_url;
    }
    // Remove trailing slash to avoid double slashes when joining paths
    while (base_url.size() > 1 && base_url.back() == '/') {
        base_url.pop_back();
    }
    return base_url;
}

std::unordered_map<std::string, std::string> json_to_string_map(const nlohmann::json& input) {
    std::unordered_map<std::string, std::string> result;
    if (!input.is_object()) {
        return result;
    }
    for (const auto& [key, value] : input.items()) {
        if (value.is_string()) {
            result.emplace(key, value.get<std::string>());
        } else {
            result.emplace(key, value.dump());
        }
    }
    return result;
}

std::string url_encode(const std::string& value) {
    static constexpr char hex_chars[] = "0123456789ABCDEF";
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << hex_chars[(c >> 4) & 0xF] << hex_chars[c & 0xF];
        }
    }
    return escaped.str();
}

std::string base64_encode(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    std::string buffer;
    buffer.resize(((value.size() + 2) / 3) * 4);
    int encoded_length = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(&buffer[0]),
                                         reinterpret_cast<const unsigned char*>(value.data()),
                                         static_cast<int>(value.size()));
    if (encoded_length < 0) {
        throw std::runtime_error("Base64 encoding failed");
    }
    buffer.resize(static_cast<std::size_t>(encoded_length));
    return buffer;
}

std::vector<unsigned char> base64_decode(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    std::vector<unsigned char> buffer;
    buffer.resize((value.size() * 3) / 4 + 3);
    int decoded_length = EVP_DecodeBlock(buffer.data(),
                                         reinterpret_cast<const unsigned char*>(value.data()),
                                         static_cast<int>(value.size()));
    if (decoded_length < 0) {
        throw std::runtime_error("Base64 decoding failed");
    }
    int padding = 0;
    if (!value.empty() && value[value.size() - 1] == '=') {
        padding++;
    }
    if (value.size() > 1 && value[value.size() - 2] == '=') {
        padding++;
    }
    decoded_length -= padding;
    if (decoded_length < 0) {
        decoded_length = 0;
    }
    buffer.resize(static_cast<std::size_t>(decoded_length));
    return buffer;
}

std::string sha256_hex(const std::vector<unsigned char>& data) {
    if (data.empty()) {
        return "";
    }
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int length = 0;
    if (EVP_Digest(data.data(), data.size(), hash, &length, EVP_sha256(), nullptr) != 1) {
        throw std::runtime_error("SHA-256 computation failed");
    }
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < length; ++i) {
        oss << std::setw(2) << static_cast<int>(hash[i]);
    }
    return oss.str();
}

} // namespace

ExternalApiTool::ExternalApiTool(const ToolConfig& config,
                                 StructuredLogger* logger,
                                 const std::string& tool_family)
    : Tool(config, logger),
      http_client_(std::make_shared<HttpClient>()),
      authenticated_(false),
      tool_family_(tool_family) {

    connection_profile_ = config.connection_config;
    auth_profile_ = config.auth_config;

    if (!connection_profile_.contains("base_url") || !connection_profile_["base_url"].is_string()) {
        throw std::invalid_argument("ExternalApiTool requires connection_config.base_url to be defined");
    }

    base_url_ = normalize_base_url(connection_profile_.value("base_url", std::string{}));

    http_client_->set_timeout(connection_profile_.value("timeout_seconds", 30));
    if (connection_profile_.contains("user_agent") && connection_profile_["user_agent"].is_string()) {
        http_client_->set_user_agent(connection_profile_["user_agent"].get<std::string>());
    } else {
        http_client_->set_user_agent("Regulens-Enterprise-Connector/1.0");
    }

    bool verify_tls = connection_profile_.value("verify_tls", true);
    http_client_->set_ssl_verify(verify_tls);

    if (connection_profile_.contains("proxy") && connection_profile_["proxy"].is_string()) {
        http_client_->set_proxy(connection_profile_["proxy"].get<std::string>());
    }

    api_key_header_ = auth_profile_.value("api_key_header", std::string{"Authorization"});
    if (auth_profile_.contains("api_key") && auth_profile_["api_key"].is_string()) {
        api_key_value_ = auth_profile_["api_key"].get<std::string>();
    } else if (auth_profile_.contains("api_key_env") && auth_profile_["api_key_env"].is_string()) {
        const char* env_value = std::getenv(auth_profile_["api_key_env"].get<std::string>().c_str());
        if (env_value) {
            api_key_value_ = env_value;
        }
    }

    if (config_.auth_type == AuthType::API_KEY && api_key_value_.empty()) {
        throw std::invalid_argument("API key authentication selected without providing api_key or api_key_env");
    }

    last_auth_check_ = std::chrono::steady_clock::time_point{};
    token_expiry_ = std::chrono::steady_clock::time_point{};
}

bool ExternalApiTool::authenticate() {
    std::unique_lock<std::shared_mutex> lock(state_mutex_);
    auto now = std::chrono::steady_clock::now();

    if (authenticated_ && now - last_auth_check_ < std::chrono::minutes(5)) {
        if (config_.auth_type == AuthType::OAUTH2 && now >= token_expiry_) {
            refresh_oauth_token_locked();
        }
        return true;
    }

    if (config_.auth_type == AuthType::OAUTH2) {
        refresh_oauth_token_locked();
    }

    if (config_.auth_type == AuthType::BASIC) {
        if (!auth_profile_.contains("username") || !auth_profile_.contains("password")) {
            throw std::invalid_argument("BASIC authentication requires username and password in auth_config");
        }
    }

    if (has_healthcheck()) {
        auto health = invoke_custom_healthcheck();
        if (!health.success) {
            authenticated_ = false;
            logger_->log(LogLevel::ERROR, "Healthcheck failed for " + tool_family_ + " tool: " + health.error_message);
            return false;
        }
    }

    authenticated_ = true;
    last_auth_check_ = now;
    logger_->log(LogLevel::INFO, tool_family_ + " connector authenticated successfully");
    return true;
}

bool ExternalApiTool::is_authenticated() const {
    std::shared_lock<std::shared_mutex> lock(state_mutex_);
    return authenticated_;
}

bool ExternalApiTool::disconnect() {
    std::unique_lock<std::shared_mutex> lock(state_mutex_);
    authenticated_ = false;
    bearer_token_.clear();
    token_expiry_ = std::chrono::steady_clock::time_point{};
    last_auth_check_ = std::chrono::steady_clock::time_point{};
    return true;
}

void ExternalApiTool::refresh_oauth_token_locked() {
    if (config_.auth_type != AuthType::OAUTH2) {
        return;
    }

    if (!auth_profile_.contains("token_url") || !auth_profile_["token_url"].is_string()) {
        throw std::invalid_argument("OAUTH2 authentication requires token_url in auth_config");
    }
    if (!auth_profile_.contains("client_id") || !auth_profile_.contains("client_secret")) {
        throw std::invalid_argument("OAUTH2 authentication requires client_id and client_secret");
    }

    std::unordered_map<std::string, std::string> headers = {
        {"Content-Type", "application/x-www-form-urlencoded"}
    };

    std::ostringstream body;
    body << "grant_type=client_credentials"
         << "&client_id=" << url_encode(auth_profile_["client_id"].get<std::string>())
         << "&client_secret=" << url_encode(auth_profile_["client_secret"].get<std::string>());

    if (auth_profile_.contains("scope")) {
        body << "&scope=" << url_encode(auth_profile_["scope"].get<std::string>());
    }

    HttpResponse token_response = http_client_->post(auth_profile_["token_url"].get<std::string>(), body.str(), headers);
    if (!token_response.success || token_response.status_code >= 400) {
        std::string message = "Failed to obtain OAuth token: " + token_response.error_message;
        if (!token_response.body.empty()) {
            message += " body=" + token_response.body;
        }
        throw std::runtime_error(message);
    }

    auto json = nlohmann::json::parse(token_response.body);
    if (!json.contains("access_token")) {
        throw std::runtime_error("OAuth token response missing access_token");
    }

    bearer_token_ = json["access_token"].get<std::string>();
    int expires_in = json.value("expires_in", 3600);
    token_expiry_ = std::chrono::steady_clock::now() + std::chrono::seconds(expires_in - 60);
}

ExternalApiTool::EndpointDefinition ExternalApiTool::resolve_endpoint(const std::string& operation) const {
    EndpointDefinition endpoint;
    endpoint.method = "GET";
    endpoint.path = base_url_;
    endpoint.allow_query_passthrough = true;

    if (connection_profile_.contains("endpoints")) {
        const auto& endpoints = connection_profile_["endpoints"];
        if (endpoints.contains(operation)) {
            const auto& def = endpoints[operation];
            endpoint.method = def.value("method", endpoint.method);
            endpoint.path = def.value("path", std::string{});
            if (def.contains("body_template")) {
                endpoint.body_template = def["body_template"];
            }
            if (def.contains("headers")) {
                endpoint.headers = def["headers"];
            }
            endpoint.allow_query_passthrough = def.value("allow_query_passthrough", true);
            return endpoint;
        }
    }

    // Allow callers to specify endpoint configuration dynamically via parameters
    endpoint.path.clear();
    return endpoint;
}

std::string ExternalApiTool::resolve_url(const std::string& path) const {
    if (path.empty()) {
        return base_url_;
    }
    if (path.rfind("http", 0) == 0) {
        return path;
    }
    if (path.front() == '/') {
        return base_url_ + path;
    }
    return base_url_ + "/" + path;
}

std::string ExternalApiTool::apply_path_parameters(const std::string& path,
                                                    const nlohmann::json& parameters) const {
    if (path.empty()) {
        return path;
    }

    std::string resolved = path;
    auto replace_token = [&](const std::string& token, const std::string& value) {
        std::regex placeholder("\\{" + token + "\\}|:" + token);
        resolved = std::regex_replace(resolved, placeholder, value);
    };

    if (parameters.contains("path_params") && parameters["path_params"].is_object()) {
        for (const auto& [key, value] : parameters["path_params"].items()) {
            replace_token(key, value.is_string() ? value.get<std::string>() : value.dump());
        }
    }

    if (parameters.contains("path") && parameters["path"].is_object()) {
        for (const auto& [key, value] : parameters["path"].items()) {
            replace_token(key, value.is_string() ? value.get<std::string>() : value.dump());
        }
    }

    return resolved;
}

std::unordered_map<std::string, std::string> ExternalApiTool::build_headers(
    const EndpointDefinition& endpoint,
    const nlohmann::json& parameters) const {

    std::unordered_map<std::string, std::string> headers = json_to_string_map(
        connection_profile_.value("default_headers", nlohmann::json::object()));

    if (!endpoint.headers.is_null()) {
        auto endpoint_headers = json_to_string_map(endpoint.headers);
        headers.insert(endpoint_headers.begin(), endpoint_headers.end());
    }

    if (parameters.contains("headers") && parameters["headers"].is_object()) {
        auto request_headers = json_to_string_map(parameters["headers"]);
        headers.insert(request_headers.begin(), request_headers.end());
    }

    if (config_.auth_type == AuthType::API_KEY && !api_key_value_.empty()) {
        headers[api_key_header_] = api_key_value_;
    }

    if (config_.auth_type == AuthType::OAUTH2 && !bearer_token_.empty()) {
        headers["Authorization"] = "Bearer " + bearer_token_;
    }

    if (config_.auth_type == AuthType::BASIC && auth_profile_.contains("username") && auth_profile_.contains("password")) {
        std::string credential = auth_profile_["username"].get<std::string>() + ":" + auth_profile_["password"].get<std::string>();
        headers["Authorization"] = "Basic " + base64_encode(credential);
    }

    return headers;
}

std::string ExternalApiTool::build_query_string(const nlohmann::json& parameters) const {
    if (!parameters.contains("query")) {
        return {};
    }

    const auto& query = parameters["query"];
    if (!query.is_object()) {
        return {};
    }

    std::vector<std::string> pairs;
    for (const auto& [key, value] : query.items()) {
        std::string encoded_key = url_encode(key);
        std::string encoded_value = url_encode(value.is_string() ? value.get<std::string>() : value.dump());
        pairs.emplace_back(encoded_key + "=" + encoded_value);
    }

    if (pairs.empty()) {
        return {};
    }

    std::ostringstream oss;
    oss << '?';
    for (size_t i = 0; i < pairs.size(); ++i) {
        if (i > 0) {
            oss << '&';
        }
        oss << pairs[i];
    }
    return oss.str();
}

nlohmann::json ExternalApiTool::materialize_body(const EndpointDefinition& endpoint,
                                                 const nlohmann::json& parameters) const {
    nlohmann::json body_json = endpoint.body_template;
    if (parameters.contains("body")) {
        if (!body_json.is_object()) {
            body_json = parameters["body"];
        } else if (parameters["body"].is_object()) {
            body_json.merge_patch(parameters["body"]);
        } else {
            body_json["payload"] = parameters["body"];
        }
    }
    return body_json;
}

ToolResult ExternalApiTool::transform_response(const HttpResponse& response) {
    if (!response.success || response.status_code >= 400) {
        std::string error = response.error_message;
        if (error.empty() && !response.body.empty()) {
            error = response.body;
        }
        return create_error_result("HTTP " + std::to_string(response.status_code) + ": " + error,
                                   std::chrono::milliseconds(0));
    }

    nlohmann::json payload;
    if (!response.body.empty()) {
        try {
            payload = nlohmann::json::parse(response.body);
        } catch (const std::exception&) {
            payload = {
                {"raw", response.body}
            };
        }
    }

    payload["status_code"] = response.status_code;
    payload["headers"] = response.headers;

    return create_success_result(payload);
}

bool ExternalApiTool::has_healthcheck() const {
    return connection_profile_.contains("healthcheck") && connection_profile_["healthcheck"].is_object();
}

ToolResult ExternalApiTool::invoke_custom_healthcheck() {
    const auto& health_cfg = connection_profile_["healthcheck"];
    EndpointDefinition endpoint;
    endpoint.method = health_cfg.value("method", std::string{"GET"});
    endpoint.path = health_cfg.value("path", std::string{"/health"});
    if (health_cfg.contains("headers")) {
        endpoint.headers = health_cfg["headers"];
    }
    endpoint.allow_query_passthrough = false;

    auto result = execute_endpoint_request(endpoint, nlohmann::json::object());
    if (!result.success) {
        return result;
    }

    return result;
}

ToolResult ExternalApiTool::execute_endpoint_request(const EndpointDefinition& endpoint,
                                                      const nlohmann::json& parameters) {
    EndpointDefinition resolved = endpoint;

    if (resolved.path.empty()) {
        if (!parameters.contains("path") && !connection_profile_.contains("default_path")) {
            return create_error_result("No endpoint path specified for operation",
                                       std::chrono::milliseconds(0));
        }
        resolved.path = parameters.value("path", connection_profile_.value("default_path", std::string{}));
    }

    std::string concrete_path = apply_path_parameters(resolved.path, parameters);
    std::string url = resolve_url(concrete_path);
    if (resolved.allow_query_passthrough) {
        url += build_query_string(parameters);
    }

    auto headers = build_headers(resolved, parameters);
    auto body_json = materialize_body(resolved, parameters);
    std::string body_payload = body_json.is_null() ? std::string{} : body_json.dump();

    HttpResponse response;
    std::string method_upper;
    for (char c : resolved.method) {
        method_upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }

    if (method_upper == "GET") {
        response = http_client_->get(url, headers);
    } else if (method_upper == "POST") {
        response = http_client_->post(url, body_payload, headers);
    } else if (method_upper == "PUT") {
        response = http_client_->put(url, body_payload, headers);
    } else if (method_upper == "PATCH") {
        response = http_client_->patch(url, body_payload, headers);
    } else if (method_upper == "DELETE") {
        response = http_client_->del(url, headers);
    } else {
        return create_error_result("Unsupported HTTP method: " + resolved.method,
                                   std::chrono::milliseconds(0));
    }

    if (!response.success && response.error_message.empty()) {
        response.error_message = "Request failed";
    }

    return transform_response(response);
}

ToolResult ExternalApiTool::handle_domain_operation(const std::string& operation,
                                                    const nlohmann::json& parameters,
                                                    const EndpointDefinition& endpoint) {
    return execute_endpoint_request(endpoint, parameters);
}

ToolResult ExternalApiTool::execute_operation(const std::string& operation,
                                              const nlohmann::json& parameters) {
    if (!is_authenticated()) {
        if (!authenticate()) {
            return create_error_result("Authentication failure", std::chrono::milliseconds(0));
        }
    }

    try {
        EndpointDefinition endpoint = resolve_endpoint(operation);
        return handle_domain_operation(operation, parameters, endpoint);
    } catch (const std::exception& ex) {
        spdlog::error("{} connector operation {} failed: {}", tool_family_, operation, ex.what());
        return create_error_result(ex.what(), std::chrono::milliseconds(0));
    }
}

// ======================= ERP Integration =======================

ERPIntegrationTool::ERPIntegrationTool(const ToolConfig& config, StructuredLogger* logger)
    : ExternalApiTool(config, logger, "ERP") {}

ToolResult ERPIntegrationTool::handle_domain_operation(const std::string& operation,
                                                       const nlohmann::json& parameters,
                                                       const EndpointDefinition& endpoint) {
    if (operation == "sync_master_data" && parameters.contains("body")) {
        const auto& body = parameters["body"];
        if (body.contains("records") && body["records"].is_array()) {
            const auto& records = body["records"];
            const std::size_t batch_size = connection_profile_.value("sync_batch_size", 250);
            nlohmann::json aggregated = {
                {"successful_batches", 0},
                {"failed_batches", 0},
                {"errors", nlohmann::json::array()}
            };

            for (std::size_t offset = 0; offset < records.size(); offset += batch_size) {
                std::size_t end = std::min(records.size(), offset + batch_size);
                nlohmann::json batch_body = parameters;
                batch_body["body"] = body;
                using difference_type = nlohmann::json::difference_type;
                batch_body["body"]["records"] = nlohmann::json(
                    records.begin() + static_cast<difference_type>(offset),
                    records.begin() + static_cast<difference_type>(end));

                auto result = execute_endpoint_request(endpoint, batch_body);
                if (result.success) {
                    aggregated["successful_batches"] = aggregated["successful_batches"].get<int>() + 1;
                } else {
                    aggregated["failed_batches"] = aggregated["failed_batches"].get<int>() + 1;
                    aggregated["errors"].push_back({
                        {"offset", offset},
                        {"message", result.error_message}
                    });
                }
            }

            if (aggregated["failed_batches"].get<int>() > 0) {
                return ToolResult{
                    false,
                    aggregated,
                    "One or more batches failed during ERP sync",
                    std::chrono::milliseconds(0),
                    0
                };
            }

            return create_success_result(aggregated);
        }
    }

    if (operation == "post_journal_entry" && parameters.contains("body")) {
        // Ensure balanced ledger before dispatch
        double debits = 0.0;
        double credits = 0.0;
        const auto& lines = parameters["body"].value("lines", nlohmann::json::array());
        for (const auto& line : lines) {
            debits += line.value("debit", 0.0);
            credits += line.value("credit", 0.0);
        }
        if (std::fabs(debits - credits) > 0.01) {
            return create_error_result("Ledger not balanced for journal entry", std::chrono::milliseconds(0));
        }
    }

    return ExternalApiTool::handle_domain_operation(operation, parameters, endpoint);
}

// ======================= CRM Integration =======================

CRMIntegrationTool::CRMIntegrationTool(const ToolConfig& config, StructuredLogger* logger)
    : ExternalApiTool(config, logger, "CRM") {}

ToolResult CRMIntegrationTool::handle_domain_operation(const std::string& operation,
                                                       const nlohmann::json& parameters,
                                                       const EndpointDefinition& endpoint) {
    if (operation == "upsert_contacts" && parameters.contains("body")) {
        nlohmann::json request = parameters;
        if (parameters["body"].contains("contacts")) {
            std::unordered_set<std::string> dedupe;
            nlohmann::json unique_contacts = nlohmann::json::array();
            for (const auto& contact : parameters["body"]["contacts"]) {
                std::string key = contact.value("email", "");
                if (!key.empty() && dedupe.insert(key).second) {
                    unique_contacts.push_back(contact);
                }
            }
            request["body"]["contacts"] = unique_contacts;
        }
        return ExternalApiTool::handle_domain_operation(operation, request, endpoint);
    }

    return ExternalApiTool::handle_domain_operation(operation, parameters, endpoint);
}

// ======================= Document Management =======================

DocumentManagementTool::DocumentManagementTool(const ToolConfig& config, StructuredLogger* logger)
    : ExternalApiTool(config, logger, "DMS") {}

ToolResult DocumentManagementTool::handle_domain_operation(const std::string& operation,
                                                           const nlohmann::json& parameters,
                                                           const EndpointDefinition& endpoint) {
    if (operation == "upload_document" && parameters.contains("body")) {
        if (!parameters["body"].contains("content_base64")) {
            return create_error_result("content_base64 is required for upload_document",
                                       std::chrono::milliseconds(0));
        }
        nlohmann::json augmented = parameters;
        try {
            auto binary = base64_decode(parameters["body"]["content_base64"].get<std::string>());
            if (binary.empty()) {
                return create_error_result("Decoded document content is empty", std::chrono::milliseconds(0));
            }
            augmented["body"]["content_sha256"] = sha256_hex(binary);
            augmented["body"]["content_size"] = binary.size();
        } catch (const std::exception& ex) {
            return create_error_result(std::string{"Invalid base64 payload: "} + ex.what(),
                                       std::chrono::milliseconds(0));
        }
        return ExternalApiTool::handle_domain_operation(operation, augmented, endpoint);
    }

    return ExternalApiTool::handle_domain_operation(operation, parameters, endpoint);
}

// ======================= Storage Gateway =======================

StorageGatewayTool::StorageGatewayTool(const ToolConfig& config, StructuredLogger* logger)
    : ExternalApiTool(config, logger, "Storage") {}

ToolResult StorageGatewayTool::handle_domain_operation(const std::string& operation,
                                                       const nlohmann::json& parameters,
                                                       const EndpointDefinition& endpoint) {
    if (operation == "stream_upload" && parameters.contains("body") && parameters["body"].contains("chunks")) {
        const auto& chunks = parameters["body"]["chunks"];
        nlohmann::json results = nlohmann::json::array();
        std::size_t max_chunk_bytes = connection_profile_.value("max_chunk_bytes", static_cast<std::size_t>(2 * 1024 * 1024));
        for (const auto& chunk : chunks) {
            if (!chunk.contains("content_base64") || !chunk["content_base64"].is_string()) {
                return create_error_result("Each chunk must include content_base64",
                                           std::chrono::milliseconds(0));
            }
            auto binary = base64_decode(chunk["content_base64"].get<std::string>());
            if (binary.size() > max_chunk_bytes) {
                return create_error_result("Chunk exceeds configured max_chunk_bytes",
                                           std::chrono::milliseconds(0));
            }
            nlohmann::json chunk_params = parameters;
            chunk_params["body"]["chunk"] = chunk;
            chunk_params["body"]["chunk"]["byte_size"] = binary.size();
            auto chunk_result = ExternalApiTool::handle_domain_operation(operation, chunk_params, endpoint);
            results.push_back({
                {"success", chunk_result.success},
                {"error", chunk_result.error_message}
            });
            if (!chunk_result.success) {
                return ToolResult{
                    false,
                    {{"chunks", results}},
                    "Chunk upload failed",
                    std::chrono::milliseconds(0),
                    0
                };
            }
        }
        return create_success_result({{"chunks", results}});
    }

    return ExternalApiTool::handle_domain_operation(operation, parameters, endpoint);
}

// ======================= Integration Hub =======================

IntegrationHubTool::IntegrationHubTool(const ToolConfig& config, StructuredLogger* logger)
    : ExternalApiTool(config, logger, "IntegrationHub") {}

ToolResult IntegrationHubTool::handle_domain_operation(const std::string& operation,
                                                       const nlohmann::json& parameters,
                                                       const EndpointDefinition& endpoint) {
    if (operation == "orchestrate" && parameters.contains("workflow")) {
        const auto& workflow = parameters["workflow"];
        if (!workflow.is_array()) {
            return create_error_result("workflow must be an array of steps", std::chrono::milliseconds(0));
        }

        nlohmann::json result = nlohmann::json::array();
        for (const auto& step : workflow) {
            std::string step_operation = step.value("operation", "");
            if (step_operation.empty()) {
                continue;
            }
            nlohmann::json step_params = parameters;
            if (step.contains("parameters")) {
                step_params.merge_patch(step["parameters"]);
            }
            auto step_endpoint = resolve_endpoint(step_operation);
            auto step_result = ExternalApiTool::handle_domain_operation(step_operation, step_params, step_endpoint);
            result.push_back({
                {"operation", step_operation},
                {"success", step_result.success},
                {"error", step_result.error_message},
                {"data", step_result.data}
            });
            if (!step_result.success) {
                return ToolResult{
                    false,
                    {{"steps", result}},
                    "Workflow step failed",
                    std::chrono::milliseconds(0),
                    0
                };
            }
        }
        return create_success_result({{"steps", result}});
    }

    return ExternalApiTool::handle_domain_operation(operation, parameters, endpoint);
}

// ======================= Model Context Bridge =======================

ModelContextBridgeTool::ModelContextBridgeTool(const ToolConfig& config, StructuredLogger* logger)
    : ExternalApiTool(config, logger, "ModelContextBridge") {}

ToolResult ModelContextBridgeTool::handle_domain_operation(const std::string& operation,
                                                           const nlohmann::json& parameters,
                                                           const EndpointDefinition& endpoint) {
    if (operation == "invoke_tool" && parameters.contains("body")) {
        nlohmann::json body = parameters["body"];
        body["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        nlohmann::json invoke_params = parameters;
        invoke_params["body"] = body;
        return ExternalApiTool::handle_domain_operation(operation, invoke_params, endpoint);
    }

    return ExternalApiTool::handle_domain_operation(operation, parameters, endpoint);
}

} // namespace regulens
