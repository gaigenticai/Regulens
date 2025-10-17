/**
 * OpenAPI/Swagger Documentation Generator - Implementation
 *
 * Generates OpenAPI 3.0 specification for REST API documentation.
 * Supports Swagger UI integration and YAML/JSON output formats.
 */

#include "openapi_generator.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <algorithm>
#include <sstream>

// OpenAPIGenerator implementation

regulens::OpenAPIGenerator::OpenAPIGenerator(
    const std::string& title,
    const std::string& version,
    const std::string& description
) : title_(title), version_(version), description_(description) {
}

void regulens::OpenAPIGenerator::add_endpoint(const APIEndpoint& endpoint) {
    endpoints_.push_back(endpoint);
}

void regulens::OpenAPIGenerator::add_schema(const SchemaDefinition& schema) {
    schemas_[schema.name] = schema;
}

void regulens::OpenAPIGenerator::add_security_scheme(
    const std::string& scheme_name,
    const std::string& scheme_type,
    const std::string& description
) {
    nlohmann::json scheme;
    scheme["type"] = scheme_type;
    scheme["description"] = description;

    // Set scheme-specific properties
    if (scheme_type == "http") {
        scheme["scheme"] = "bearer";
        scheme["bearerFormat"] = "JWT";
    } else if (scheme_type == "apiKey") {
        scheme["in"] = "header";
        scheme["name"] = "X-API-Key";
    }

    security_schemes_[scheme_name] = scheme;
}

void regulens::OpenAPIGenerator::add_server(const std::string& url, const std::string& description) {
    servers_.push_back(std::make_pair(url, description));
}

/**
 * Set API version
 */
void regulens::OpenAPIGenerator::set_info_version(const std::string& version) {
    spec_["info"]["version"] = version;
}

/**
 * Set API description
 */
void regulens::OpenAPIGenerator::set_info_description(const std::string& description) {
    spec_["info"]["description"] = description;
}

/**
 * Set API title
 */
void regulens::OpenAPIGenerator::set_info_title(const std::string& title) {
    spec_["info"]["title"] = title;
}

/**
 * Set contact information
 */
void regulens::OpenAPIGenerator::set_info_contact(const nlohmann::json& contact) {
    spec_["info"]["contact"] = contact;
}

/**
 * Set license information
 */
void regulens::OpenAPIGenerator::set_info_license(const nlohmann::json& license) {
    spec_["info"]["license"] = license;
}

std::string regulens::OpenAPIGenerator::generate_json() const {
    nlohmann::json openapi = build_openapi_json();
    return openapi.dump(2); // Pretty print with 2-space indentation
}

std::string regulens::OpenAPIGenerator::generate_yaml() const {
    // For now, return JSON as YAML conversion would require additional dependencies
    // In production, you might want to use a YAML library like yaml-cpp
    std::string json_output = generate_json();
    std::string yaml_output = "# OpenAPI 3.0 Specification\n";
    yaml_output += "# Note: This is JSON converted to YAML format\n";
    yaml_output += "# For proper YAML, consider using a YAML library\n\n";
    yaml_output += json_output;
    return yaml_output;
}

bool regulens::OpenAPIGenerator::write_to_file(const std::string& file_path, const std::string& format) {
    try {
        std::string content;
        if (format == "json") {
            content = generate_json();
        } else if (format == "yaml") {
            content = generate_yaml();
        } else {
            std::cerr << "Error: Unsupported format '" << format << "'. Use 'json' or 'yaml'." << std::endl;
            return false;
        }

        std::ofstream file(file_path);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file '" << file_path << "' for writing." << std::endl;
            return false;
        }

        file << content;
        file.close();

        std::cout << "✅ OpenAPI specification written to '" << file_path << "'" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error writing OpenAPI specification: " << e.what() << std::endl;
        return false;
    }
}

std::string regulens::OpenAPIGenerator::generate_swagger_ui_html(const std::string& spec_url) {
    std::stringstream html;
    html << R"html(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <meta name="description" content="Regulens API Documentation - Swagger UI" />
    <title>Regulens API Documentation</title>
    <link rel="stylesheet" href="https://unpkg.com/swagger-ui-dist@5.10.5/swagger-ui.css" />
    <link rel="icon" type="image/png" href="https://unpkg.com/swagger-ui-dist@5.10.5/favicon-32x32.png" sizes="32x32" />
    <link rel="icon" type="image/png" href="https://unpkg.com/swagger-ui-dist@5.10.5/favicon-16x16.png" sizes="16x16" />
    <style>
        html { box-sizing: border-box; overflow: -moz-scrollbars-vertical; overflow-y: scroll; }
        *, *:before, *:after { box-sizing: inherit; }
        body { margin: 0; background: #fafafa; }
    </style>
</head>
<body>
    <div id="swagger-ui"></div>
    <script src="https://unpkg.com/swagger-ui-dist@5.10.5/swagger-ui-bundle.js" crossorigin></script>
    <script src="https://unpkg.com/swagger-ui-dist@5.10.5/swagger-ui-standalone-preset.js" crossorigin></script>
    <script>
        window.onload = () => {
            window.ui = SwaggerUIBundle({
                url: ')html" << spec_url << R"html(',
                dom_id: '#swagger-ui',
                deepLinking: true,
                presets: [
                    SwaggerUIBundle.presets.apis,
                    SwaggerUIStandalonePreset
                ],
                plugins: [
                    SwaggerUIBundle.plugins.DownloadUrl
                ],
                layout: "StandaloneLayout"
            });
        };
    </script>
</body>
</html>
)html";
    return html.str();
}

std::string regulens::OpenAPIGenerator::generate_redoc_html(const std::string& spec_url) {
    std::stringstream html;
    html << R"html(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <meta name="description" content="Regulens API Documentation - ReDoc" />
    <title>Regulens API Documentation</title>
    <link rel="icon" type="image/png" href="https://unpkg.com/swagger-ui-dist@5.10.5/favicon-32x32.png" sizes="32x32" />
    <style>
        body { margin: 0; padding: 0; }
        redoc { display: block; }
    </style>
</head>
<body>
    <redoc spec-url=")html" << spec_url << R"html("></redoc>
    <script src="https://unpkg.com/redoc@next/bundles/redoc.standalone.js"></script>
</body>
</html>
)html";
    return html.str();
}

// Private helper methods

std::string regulens::OpenAPIGenerator::method_to_string(HTTPMethod method) const {
    switch (method) {
        case HTTPMethod::GET: return "get";
        case HTTPMethod::POST: return "post";
        case HTTPMethod::PUT: return "put";
        case HTTPMethod::DELETE: return "delete";
        case HTTPMethod::PATCH: return "patch";
        case HTTPMethod::OPTIONS: return "options";
        default: return "get";
    }
}

std::string regulens::OpenAPIGenerator::location_to_string(ParameterLocation location) const {
    switch (location) {
        case ParameterLocation::PATH: return "path";
        case ParameterLocation::QUERY: return "query";
        case ParameterLocation::HEADER: return "header";
        case ParameterLocation::COOKIE: return "cookie";
        case ParameterLocation::BODY: return "body";
        default: return "query";
    }
}

std::string regulens::OpenAPIGenerator::type_to_string(ParameterType type) const {
    switch (type) {
        case ParameterType::STRING: return "string";
        case ParameterType::INTEGER: return "integer";
        case ParameterType::NUMBER: return "number";
        case ParameterType::BOOLEAN: return "boolean";
        case ParameterType::ARRAY: return "array";
        case ParameterType::OBJECT: return "object";
        default: return "string";
    }
}

nlohmann::json regulens::OpenAPIGenerator::build_openapi_json() const {
    nlohmann::json openapi;

    // OpenAPI version
    openapi["openapi"] = "3.0.3";

    // Info section
    openapi["info"]["title"] = title_;
    openapi["info"]["version"] = version_;
    openapi["info"]["description"] = description_;

    // Contact info
    openapi["info"]["contact"]["name"] = "Regulens Development Team";
    openapi["info"]["contact"]["email"] = "api@regulens.com";

    // License
    openapi["info"]["license"]["name"] = "Proprietary";
    openapi["info"]["license"]["url"] = "https://regulens.com/license";

    // Servers
    if (!servers_.empty()) {
        for (const auto& server : servers_) {
            nlohmann::json server_json;
            server_json["url"] = server.first;
            if (!server.second.empty()) {
                server_json["description"] = server.second;
            }
            openapi["servers"].push_back(server_json);
        }
    } else {
        // Default server
        openapi["servers"] = nlohmann::json::array();
        nlohmann::json default_server;
        // Use environment variable for server URL - PRODUCTION REQUIREMENT: No hardcoded localhost
        const char* server_url_env = std::getenv("API_SERVER_URL");
        default_server["url"] = server_url_env ? server_url_env : "https://api.regulens.com";
        default_server["description"] = "Development server";
        openapi["servers"].push_back(default_server);
    }

    // Security schemes
    if (!security_schemes_.empty()) {
        openapi["components"]["securitySchemes"] = security_schemes_;
    }

    // Schemas
    if (!schemas_.empty()) {
        for (const auto& schema_pair : schemas_) {
            openapi["components"]["schemas"][schema_pair.first] = build_schema(schema_pair.second);
        }
    }

    // Paths
    openapi["paths"] = build_paths();

    return openapi;
}

nlohmann::json regulens::OpenAPIGenerator::build_paths() const {
    nlohmann::json paths = nlohmann::json::object();

    for (const auto& endpoint : endpoints_) {
        if (paths.find(endpoint.path) == paths.end()) {
            paths[endpoint.path] = nlohmann::json::object();
        }

        std::string method = endpoint.method;
        // Convert to lowercase for OpenAPI
        std::transform(method.begin(), method.end(), method.begin(), ::tolower);
        paths[endpoint.path][method] = build_operation(endpoint);
    }

    return paths;
}

nlohmann::json regulens::OpenAPIGenerator::build_operation(const APIEndpoint& endpoint) const {
    nlohmann::json operation;

    operation["summary"] = endpoint.summary;
    operation["description"] = endpoint.description;

    if (!endpoint.operation_id.empty()) {
        operation["operationId"] = endpoint.operation_id;
    }

    if (!endpoint.tags.empty()) {
        operation["tags"] = endpoint.tags;
    }

    // Security requirements
    if (endpoint.requires_auth && !security_schemes_.empty()) {
        nlohmann::json security_req;
        for (const auto& scheme : endpoint.security_schemes) {
            if (security_schemes_.find(scheme) != security_schemes_.end()) {
                security_req[scheme] = nlohmann::json::array();
            }
        }
        if (!security_req.empty()) {
            operation["security"] = nlohmann::json::array({security_req});
        }
    }

    // Parameters
    if (!endpoint.parameters.empty()) {
        nlohmann::json params = nlohmann::json::array();
        for (const auto& param : endpoint.parameters) {
            params.push_back(build_parameter(param));
        }
        operation["parameters"] = params;
    }

    // Request body (for POST, PUT, PATCH)
    if ((endpoint.method == "POST" || endpoint.method == "PUT" ||
         endpoint.method == "PATCH") && !endpoint.parameters.empty()) {
        // Check if we have body parameters
        bool has_body_param = false;
        for (const auto& param : endpoint.parameters) {
            if (param.in == "body") {
                has_body_param = true;
                break;
            }
        }

        if (has_body_param) {
            nlohmann::json request_body;
            request_body["required"] = true;
            request_body["content"]["application/json"]["schema"]["type"] = "object";
            operation["requestBody"] = request_body;
        }
    }

    // Responses
    nlohmann::json responses = nlohmann::json::object();
    for (const auto& response_pair : endpoint.responses) {
        responses[std::to_string(response_pair.first)] = build_response(response_pair.second);
    }

    // Add default error responses if not present
    if (responses.find("400") == responses.end()) {
        nlohmann::json error_response;
        error_response["description"] = "Bad Request";
        error_response["content"]["application/json"]["schema"]["$ref"] = "#/components/schemas/Error";
        responses["400"] = error_response;
    }

    if (responses.find("401") == responses.end()) {
        nlohmann::json error_response;
        error_response["description"] = "Unauthorized";
        error_response["content"]["application/json"]["schema"]["$ref"] = "#/components/schemas/Error";
        responses["401"] = error_response;
    }

    if (responses.find("500") == responses.end()) {
        nlohmann::json error_response;
        error_response["description"] = "Internal Server Error";
        error_response["content"]["application/json"]["schema"]["$ref"] = "#/components/schemas/Error";
        responses["500"] = error_response;
    }

    operation["responses"] = responses;

    return operation;
}

nlohmann::json regulens::OpenAPIGenerator::build_parameter(const APIParameter& parameter) const {
    nlohmann::json param;

    param["name"] = parameter.name;
    param["in"] = parameter.in;  // Use 'in' field directly from APIParameter struct
    param["description"] = parameter.description;
    param["required"] = parameter.required;

    nlohmann::json schema;
    schema["type"] = parameter.type;  // Use 'type' field directly from APIParameter struct

    param["schema"] = schema;

    return param;
}

nlohmann::json regulens::OpenAPIGenerator::build_response(const APIResponse& response) const {
    nlohmann::json resp;

    resp["description"] = response.description;

    // Use the schema field from APIResponse struct
    if (!response.schema.empty()) {
        nlohmann::json content;
        content["application/json"]["schema"] = response.schema;
        resp["content"] = content;
    }

    return resp;
}

nlohmann::json regulens::OpenAPIGenerator::build_schema(const SchemaDefinition& schema) const {
    nlohmann::json schema_json;

    if (schema.type == "object") {
        schema_json["type"] = "object";

        if (!schema.properties.empty()) {
            nlohmann::json properties;
            for (const auto& prop : schema.properties) {
                // Simple type mapping - in production, you'd want more sophisticated schema building
                properties[prop.first]["type"] = prop.second;
            }
            schema_json["properties"] = properties;
        }

        if (!schema.required_properties.empty()) {
            schema_json["required"] = schema.required_properties;
        }

        if (!schema.example.empty()) {
            schema_json["example"] = nlohmann::json::parse(schema.example);
        }
    } else {
        schema_json["type"] = schema.type;
    }

    return schema_json;
}

// Helper function implementation
void regulens::register_regulens_api_endpoints(OpenAPIGenerator& generator) {
    // TEMPORARILY DISABLED: OpenAPI generator has structural issues that need resolution
    // This function contains incompatible struct definitions and enum usage
    // TODO: Fix struct field mismatches and type conversions for production-grade API documentation
    (void)generator; // Suppress unused parameter warning
}
