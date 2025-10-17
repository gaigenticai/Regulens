/**
 * OpenAPI/Swagger Documentation Generator - Header
 * 
 * Generates OpenAPI 3.0 specification for REST API documentation.
 */

#ifndef REGULENS_OPENAPI_GENERATOR_HPP
#define REGULENS_OPENAPI_GENERATOR_HPP

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

// Forward declaration for APIEndpoint from api_registry
#include "../api_registry/api_registry.hpp"

namespace regulens {

// HTTP method enum
enum class HTTPMethod {
    GET,
    POST,
    PUT,
    DELETE,
    PATCH,
    OPTIONS
};

// Parameter location
enum class ParameterLocation {
    PATH,
    QUERY,
    HEADER,
    COOKIE,
    BODY
};

// Parameter data type
enum class ParameterType {
    STRING,
    INTEGER,
    NUMBER,
    BOOLEAN,
    ARRAY,
    OBJECT
};

// APIParameter and APIResponse are defined in api_registry.hpp

// API endpoint - Forward declaration, defined in api_registry.hpp
struct APIEndpoint;

// Schema definition
struct SchemaDefinition {
    std::string name;
    std::string type;  // "object", "array", etc.
    std::string description;
    std::map<std::string, std::string> properties;  // property_name -> type
    std::vector<std::string> required_properties;
    std::string example;
};

/**
 * OpenAPI Generator
 * 
 * Generates OpenAPI 3.0 specification for API documentation.
 * Supports Swagger UI integration.
 */
class OpenAPIGenerator {
public:
    /**
     * Constructor
     * 
     * @param title API title
     * @param version API version
     * @param description API description
     */
    OpenAPIGenerator(
        const std::string& title = "Regulens API",
        const std::string& version = "1.0.0",
        const std::string& description = "Agentic AI Compliance System API"
    );

    /**
     * Add an API endpoint
     * 
     * @param endpoint Endpoint specification
     */
    void add_endpoint(const APIEndpoint& endpoint);

    /**
     * Add a schema definition
     * 
     * @param schema Schema specification
     */
    void add_schema(const SchemaDefinition& schema);

    /**
     * Add authentication scheme
     * 
     * @param scheme_name Name of the scheme
     * @param scheme_type Type (e.g., "bearer", "apiKey")
     * @param description Description
     */
    void add_security_scheme(
        const std::string& scheme_name,
        const std::string& scheme_type,
        const std::string& description
    );

    /**
     * Set server URL
     *
     * @param url Server URL
     * @param description Server description
     */
    void add_server(const std::string& url, const std::string& description = "");

    /**
     * Set API version
     *
     * @param version API version string
     */
    void set_info_version(const std::string& version);

    /**
     * Set API description
     *
     * @param description API description
     */
    void set_info_description(const std::string& description);

    /**
     * Set API title
     *
     * @param title API title
     */
    void set_info_title(const std::string& title);

    /**
     * Set contact information
     *
     * @param contact JSON object with contact info
     */
    void set_info_contact(const nlohmann::json& contact);

    /**
     * Set license information
     *
     * @param license JSON object with license info
     */
    void set_info_license(const nlohmann::json& license);

    /**
     * Generate OpenAPI JSON specification
     * 
     * @return JSON string
     */
    std::string generate_json() const;

    /**
     * Generate OpenAPI YAML specification
     * 
     * @return YAML string
     */
    std::string generate_yaml() const;

    /**
     * Write specification to file
     * 
     * @param file_path Output file path
     * @param format Format ("json" or "yaml")
     * @return true if successful
     */
    bool write_to_file(const std::string& file_path, const std::string& format = "json");

    /**
     * Generate Swagger UI HTML
     * 
     * @param spec_url URL to OpenAPI specification
     * @return HTML string
     */
    static std::string generate_swagger_ui_html(const std::string& spec_url);

    /**
     * Generate ReDoc HTML
     * 
     * @param spec_url URL to OpenAPI specification
     * @return HTML string
     */
    static std::string generate_redoc_html(const std::string& spec_url);

private:
    std::string title_;
    std::string version_;
    std::string description_;
    std::vector<std::pair<std::string, std::string>> servers_;  // url, description
    std::vector<APIEndpoint> endpoints_;
    std::map<std::string, SchemaDefinition> schemas_;
    std::map<std::string, nlohmann::json> security_schemes_;
    nlohmann::json spec_;  // OpenAPI specification JSON

    /**
     * Convert HTTPMethod to string
     */
    std::string method_to_string(HTTPMethod method) const;

    /**
     * Convert ParameterLocation to string
     */
    std::string location_to_string(ParameterLocation location) const;

    /**
     * Convert ParameterType to string
     */
    std::string type_to_string(ParameterType type) const;

    /**
     * Build OpenAPI JSON structure
     */
    nlohmann::json build_openapi_json() const;

    /**
     * Build paths section
     */
    nlohmann::json build_paths() const;

    /**
     * Build components section
     */
    nlohmann::json build_components() const;

    /**
     * Build endpoint operation
     */
    nlohmann::json build_operation(const APIEndpoint& endpoint) const;

    /**
     * Build parameter object
     */
    nlohmann::json build_parameter(const APIParameter& parameter) const;

    /**
     * Build response object
     */
    nlohmann::json build_response(const APIResponse& response) const;

    /**
     * Build schema object
     */
    nlohmann::json build_schema(const SchemaDefinition& schema) const;
};

/**
 * Helper function to register all Regulens API endpoints
 * 
 * @param generator OpenAPI generator instance
 */
void register_regulens_api_endpoints(OpenAPIGenerator& generator);

} // namespace regulens

#endif // REGULENS_OPENAPI_GENERATOR_HPP

