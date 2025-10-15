#pragma once

#include "../tool_interface.hpp"
#include "../../network/http_client.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <shared_mutex>

namespace regulens {

/**
 * @brief Base class for enterprise system connectors (ERP, CRM, DMS, Storage, Integration Hubs).
 *        Provides resilient HTTP connectivity, dynamic endpoint resolution, and rich telemetry.
 */
class ExternalApiTool : public Tool {
public:
    ExternalApiTool(const ToolConfig& config,
                    StructuredLogger* logger,
                    const std::string& tool_family);

    ToolResult execute_operation(const std::string& operation,
                                 const nlohmann::json& parameters) override;

    bool authenticate() override;
    bool is_authenticated() const override;
    bool disconnect() override;

protected:
    struct EndpointDefinition {
        std::string method;
        std::string path;
        nlohmann::json body_template;
        nlohmann::json headers;
        bool allow_query_passthrough{true};
    };

    virtual ToolResult handle_domain_operation(const std::string& operation,
                                               const nlohmann::json& parameters,
                                               const EndpointDefinition& endpoint);

    ToolResult execute_endpoint_request(const EndpointDefinition& endpoint,
                                         const nlohmann::json& parameters);

    std::string resolve_url(const std::string& path) const;
    EndpointDefinition resolve_endpoint(const std::string& operation) const;
    std::unordered_map<std::string, std::string> build_headers(const EndpointDefinition& endpoint,
                                                               const nlohmann::json& parameters) const;
    std::string apply_path_parameters(const std::string& path,
                                      const nlohmann::json& parameters) const;
    std::string build_query_string(const nlohmann::json& parameters) const;
    nlohmann::json materialize_body(const EndpointDefinition& endpoint,
                                    const nlohmann::json& parameters) const;
    ToolResult transform_response(const HttpResponse& response);

    bool has_healthcheck() const;
    ToolResult invoke_custom_healthcheck();
    void refresh_oauth_token_locked();

protected:
    std::shared_ptr<HttpClient> http_client_;
    std::string base_url_;
    nlohmann::json connection_profile_;
    nlohmann::json auth_profile_;
    std::string api_key_header_;
    std::string api_key_value_;
    std::string bearer_token_;
    std::chrono::steady_clock::time_point token_expiry_;
    mutable std::shared_mutex state_mutex_;
    bool authenticated_;
    std::chrono::steady_clock::time_point last_auth_check_;
    std::string tool_family_;
};

class ERPIntegrationTool : public ExternalApiTool {
public:
    ERPIntegrationTool(const ToolConfig& config, StructuredLogger* logger);

protected:
    ToolResult handle_domain_operation(const std::string& operation,
                                       const nlohmann::json& parameters,
                                       const EndpointDefinition& endpoint) override;
};

class CRMIntegrationTool : public ExternalApiTool {
public:
    CRMIntegrationTool(const ToolConfig& config, StructuredLogger* logger);

protected:
    ToolResult handle_domain_operation(const std::string& operation,
                                       const nlohmann::json& parameters,
                                       const EndpointDefinition& endpoint) override;
};

class DocumentManagementTool : public ExternalApiTool {
public:
    DocumentManagementTool(const ToolConfig& config, StructuredLogger* logger);

protected:
    ToolResult handle_domain_operation(const std::string& operation,
                                       const nlohmann::json& parameters,
                                       const EndpointDefinition& endpoint) override;
};

class StorageGatewayTool : public ExternalApiTool {
public:
    StorageGatewayTool(const ToolConfig& config, StructuredLogger* logger);

protected:
    ToolResult handle_domain_operation(const std::string& operation,
                                       const nlohmann::json& parameters,
                                       const EndpointDefinition& endpoint) override;
};

class IntegrationHubTool : public ExternalApiTool {
public:
    IntegrationHubTool(const ToolConfig& config, StructuredLogger* logger);

protected:
    ToolResult handle_domain_operation(const std::string& operation,
                                       const nlohmann::json& parameters,
                                       const EndpointDefinition& endpoint) override;
};

class ModelContextBridgeTool : public ExternalApiTool {
public:
    ModelContextBridgeTool(const ToolConfig& config, StructuredLogger* logger);

protected:
    ToolResult handle_domain_operation(const std::string& operation,
                                       const nlohmann::json& parameters,
                                       const EndpointDefinition& endpoint) override;
};

} // namespace regulens
